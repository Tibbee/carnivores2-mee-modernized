#pragma once
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "Core/ConfigScan.h"

// Byte-preserving "key value" merge for config.cfg, used by both writers: the
// workbench's ConfigPersistence save path and the menu's SaveConfig(). The two
// writers are kept from colliding by a disjoint key set (ConfigRegistry test).
// This header stays pure -- no I/O, no platform headers -- so the rules remain
// unit-testable without a filesystem.
//
// The engine reader (Game/EngineInit.cpp LoadConfig) splits on \r and \n, skips
// a line whose first character is '#', and reads the first two whitespace-
// delimited tokens; a line with only one token is logged as "missing value" and
// ignored. It assigns as it walks the file, so the LAST active occurrence of a
// key wins. Every rule below follows from those facts rather than from taste.

namespace configmerge {

// One setting the caller owns. The value is emitted verbatim: the caller is
// responsible for formatting it so the engine's strict parser accepts the whole
// token (see Core/ConfigParse.h).
struct Entry {
	std::string key;
	std::string value;
};

enum class Action { Updated, Appended };

struct Change {
	Action action = Action::Updated;
	std::string key;
	std::string oldValue; // Empty when action is Appended.
	std::string newValue;
	std::size_t line = 0;  // 0-based index of the updated line; unused when Appended.
};

struct Report {
	// Updated lines in file order, then appended keys in caller order.
	std::vector<Change> changes;
	// Owned keys that appeared more than once as an active line. The earlier
	// occurrences are left byte-identical on purpose -- they are inert because
	// the last one wins -- but a preview must say they exist so the operator is
	// not surprised by a stale-looking duplicate. Keys the caller does not own
	// are never reported: their duplicates are none of this writer's business.
	std::vector<std::string> duplicates;
};

struct Result {
	bool ok = false;
	std::string error; // Set when ok is false; a caller/format mistake, not I/O.
	std::string text;  // Merged bytes. Equal to the input when nothing changed.
	Report report;
};

inline bool IsBlank(char c)
{
	// sscanf_s's %s treats the C-locale isspace set as separators. Lines cannot
	// contain \r or \n (they terminate the line), so '\v' and '\f' are the only
	// extra whitespace the loader sees but a merge that checked only ' '/'\t'
	// would miss -- and a missed occurrence can let a later line silently
	// override a value this writer just reported as updated.
	return c == ' ' || c == '\t' || c == '\v' || c == '\f';
}

// A line retains its own terminator so an untouched line can be copied out
// byte-exact and an updated line changes only its value token.
struct Line {
	std::size_t begin = 0;
	std::size_t bodyEnd = 0;
	std::size_t end = 0;
};

inline std::vector<Line> SplitLines(std::string_view text)
{
	std::vector<Line> lines;
	std::size_t i = 0;
	while (i < text.size()) {
		const std::size_t begin = i;
		while (i < text.size() && text[i] != '\n' && text[i] != '\r') ++i;
		const std::size_t bodyEnd = i;
		if (i + 1 < text.size() && text[i] == '\r' && text[i + 1] == '\n') i += 2;
		else if (i < text.size()) ++i;
		lines.push_back({begin, bodyEnd, i});
	}
	return lines;
}

struct Fields {
	// A '#' line is a comment, never a setting. This matters because the loader
	// tests only column 0 for '#' -- an indented comment reaches its key table as
	// "#..." -- so the two agree only because a '#'-prefixed token can never
	// equal a real key. Merge rejects '#'-prefixed entry keys to keep that
	// guarantee explicit rather than incidental.
	bool comment = false;
	bool key = false;
	bool value = false; // A second token, which is what the loader requires.
	std::size_t keyBegin = 0, keyEnd = 0, valueBegin = 0, valueEnd = 0;
};

inline Fields ParseFields(std::string_view body, bool leadingBom = false)
{
	Fields found;
	std::size_t i = 0;
	// ReadConfigText strips a UTF-8 BOM at the start of the file and then parses
	// the remainder, so the key on line 0 must not be hidden behind it. A BOM in
	// the middle of the file is left alone as the loader would leave it.
	if (leadingBom && body.size() >= 3 && body.compare(0, 3, "\xEF\xBB\xBF") == 0) i = 3;
	while (i < body.size() && IsBlank(body[i])) ++i;
	if (i >= body.size()) return found;
	if (body[i] == '#') { found.comment = true; return found; }
	found.keyBegin = i;
	while (i < body.size() && !IsBlank(body[i])) ++i;
	found.keyEnd = i;
	found.key = true;
	while (i < body.size() && IsBlank(body[i])) ++i;
	if (i >= body.size()) return found; // "key" alone: the loader ignores it.
	found.valueBegin = i;
	while (i < body.size() && !IsBlank(body[i])) ++i;
	found.valueEnd = i;
	found.value = true;
	return found;
}

// Follow the file's own convention. A brand-new file falls back to CRLF because
// the engine's first-run template (CreateDefaultConfig) writes CRLF.
inline std::string_view DetectTerminator(std::string_view text)
{
	if (text.find("\r\n") != std::string_view::npos) return "\r\n";
	if (text.find('\n') != std::string_view::npos) return "\n";
	if (text.find('\r') != std::string_view::npos) return "\r";
	return "\r\n";
}

inline bool EndsWithTerminator(std::string_view text)
{
	return !text.empty() && (text.back() == '\n' || text.back() == '\r');
}

// Merge the owned entries into text.
//
// Only an owned key's own value token is ever rewritten. Indentation, key
// spelling and case, separator whitespace, trailing text on the line, comments,
// blank lines, unknown keys and every terminator are preserved byte for byte.
// A key the loader would ignore -- "key=value", a bare "key", a commented line,
// or a key with no second token -- is not an occurrence, so it neither receives
// the new value nor blocks an append.
inline Result Merge(std::string_view text, const std::vector<Entry>& owned)
{
	Result result;
	for (const auto& entry : owned) {
		if (entry.key.empty() || entry.value.empty()) {
			result.error = "an entry has an empty key or value";
			return result;
		}
		if (entry.key.find_first_of(" \t\r\n") != std::string::npos ||
			entry.value.find_first_of(" \t\r\n") != std::string::npos) {
			result.error = "keys and values must be single tokens";
			return result;
		}
		if (entry.key.front() == '#') {
			// A '#'-prefixed key is unreadable by the loader: at column 0 the line
			// is a comment, and indented the token itself begins with '#'. Writing
			// one would produce a setting that only looks applied.
			result.error = "a key may not begin with '#'";
			return result;
		}
	}

	const auto lines = SplitLines(text);

	// Last active occurrence of each owned key, and how many there were.
	std::vector<std::size_t> target(owned.size(), static_cast<std::size_t>(-1));
	std::vector<std::size_t> seen(owned.size(), 0);
	for (std::size_t index = 0; index < lines.size(); ++index) {
		const auto body = text.substr(lines[index].begin, lines[index].bodyEnd - lines[index].begin);
		const auto found = ParseFields(body, index == 0);
		if (found.comment || !found.key || !found.value) continue;
		const auto token = body.substr(found.keyBegin, found.keyEnd - found.keyBegin);
		for (std::size_t slot = 0; slot < owned.size(); ++slot) {
			if (!configscan::KeyEquals(token, owned[slot].key)) continue;
			++seen[slot];
			target[slot] = index;
			break;
		}
	}

	std::string merged;
	merged.reserve(text.size() + 64);
	for (std::size_t index = 0; index < lines.size(); ++index) {
		const auto& line = lines[index];
		std::size_t slot = static_cast<std::size_t>(-1);
		for (std::size_t candidate = 0; candidate < owned.size(); ++candidate) {
			if (target[candidate] == index) { slot = candidate; break; }
		}
		if (slot == static_cast<std::size_t>(-1)) {
			merged.append(text.substr(line.begin, line.end - line.begin));
			continue;
		}
		const auto body = text.substr(line.begin, line.bodyEnd - line.begin);
		const auto found = ParseFields(body, index == 0);
		const auto oldValue = body.substr(found.valueBegin, found.valueEnd - found.valueBegin);
		// Already what the caller wants: leave the bytes alone rather than
		// normalizing a hand-written spelling of the same value.
		if (oldValue == owned[slot].value) {
			merged.append(text.substr(line.begin, line.end - line.begin));
			continue;
		}
		merged.append(body.substr(0, found.valueBegin));
		merged.append(owned[slot].value);
		merged.append(body.substr(found.valueEnd));
		merged.append(text.substr(line.bodyEnd, line.end - line.bodyEnd));
		Change change;
		change.action = Action::Updated;
		change.key = owned[slot].key;
		change.oldValue.assign(oldValue);
		change.newValue = owned[slot].value;
		change.line = index;
		result.report.changes.push_back(change);
	}

	// Absent keys append once, in caller order. No-key and comment-only lines are
	// absent by design, so a commented example can coexist with a real setting.
	bool appended = false;
	for (std::size_t slot = 0; slot < owned.size(); ++slot) {
		if (target[slot] != static_cast<std::size_t>(-1)) continue;
		if (!appended) {
			if (!merged.empty() && !EndsWithTerminator(merged)) merged.append(DetectTerminator(text));
			appended = true;
		}
		merged.append(owned[slot].key);
		merged.push_back(' ');
		merged.append(owned[slot].value);
		merged.append(DetectTerminator(text));
		Change change;
		change.action = Action::Appended;
		change.key = owned[slot].key;
		change.newValue = owned[slot].value;
		change.line = lines.size();
		result.report.changes.push_back(change);
	}

	for (std::size_t slot = 0; slot < owned.size(); ++slot)
		if (seen[slot] > 1) result.report.duplicates.push_back(owned[slot].key);

	result.ok = true;
	result.text = std::move(merged);
	return result;
}

} // namespace configmerge
