#pragma once
#include <cstddef>
#include <string_view>
#include <vector>

// Shared field scanner for config.cfg: the tokenizer that the engine reader
// (Game/EngineInit.cpp LoadConfig), the menu reader (Menu/Resources.cpp
// ParseConfigField/LoadConfig) and the load-policy pre-reader
// (Loaders/LoadDiagnostics.h InitLoadPolicyFromConfigText) each carry a copy
// of today. It yields only what those readers agree on structurally; value
// interpretation (ranges, clamps, strict parsing) stays with the consumer.
//
//   - the first whitespace-delimited token is the key, the second is the
//     value; any further tokens are ignored;
//   - a line with no second token yields hasValue == false, which every
//     reader ignores (the engine logs it);
//   - fields come out in file order; the readers assign as they walk, so the
//     last occurrence of a key wins;
//   - '\v' and '\f' count as whitespace, matching sscanf_s's %s and the
//     byte-preserving merge in Core/ConfigMerge.h (a missed occurrence would
//     let a later line silently override a value a writer just reported);
//   - callers pass the output of ReadConfigText (Core/ConfigText.h): the BOM
//     is already stripped and NUL padding already turned into newlines. The
//     engine's sscanf_s caps tokens at 63 bytes; the scanner returns whole
//     tokens, so a consumer that needs the old cap applies it when copying.
//
// Separator::WhitespaceOrEquals is the load-policy pre-reader's grammar: it
// also accepts `load_mode=strict` and `load_mode = strict`, which the two
// readers do not. Both grammars are intentional (owner call, 2026-10-14) and
// pinned by tests/test_config_scan.cpp, not bugs to converge silently.
// Comment handling follows each grammar: whitespace mode treats only column 0
// as a comment (the engine reader; the menu re-checks a '#'-prefixed token),
// while equals mode also skips a '#' after leading blanks (the pre-reader).

namespace configscan {

struct Field {
	std::size_t line = 0; // 0-based line index, for diagnostics.
	std::string_view key;
	std::string_view value; // Empty when hasValue is false.
	bool hasValue = false;
};

enum class Separator {
	Whitespace,         // Engine and menu readers.
	WhitespaceOrEquals, // Load-policy pre-reader.
};

// ASCII case-insensitive key compare: the rule the engine reader (_stricmp)
// and the load-policy pre-reader (_strnicmp) already apply. Config keys are
// ASCII and every shipped file spells them lowercase, so the locale's
// _stricmp edge cases cannot appear; a hand-written mixed-case key still
// matches, which is what the engine does.
inline bool KeyEquals(std::string_view key, std::string_view name)
{
	if (key.size() != name.size()) return false;
	for (std::size_t i = 0; i < key.size(); ++i) {
		char a = key[i], b = name[i];
		if (a >= 'A' && a <= 'Z') a = static_cast<char>(a - 'A' + 'a');
		if (b >= 'A' && b <= 'Z') b = static_cast<char>(b - 'A' + 'a');
		if (a != b) return false;
	}
	return true;
}

inline std::vector<Field> Scan(std::string_view text,
                               Separator separator = Separator::Whitespace)
{
	const bool equals = separator == Separator::WhitespaceOrEquals;
	const auto blank = [equals](char c) {
		if (c == ' ' || c == '\t') return true;
		return !equals && (c == '\v' || c == '\f');
	};

	std::vector<Field> fields;
	std::size_t i = 0;
	std::size_t line = 0;
	while (i < text.size()) {
		const std::size_t begin = i;
		while (i < text.size() && text[i] != '\n' && text[i] != '\r') ++i;
		const std::string_view body = text.substr(begin, i - begin);
		if (i + 1 < text.size() && text[i] == '\r' && text[i + 1] == '\n') i += 2;
		else if (i < text.size()) ++i;

		std::size_t p = 0;
		while (p < body.size() && blank(body[p])) ++p;
		if (p < body.size() && !(body[p] == '#' && (equals || p == 0))) {
			const std::size_t keyBegin = p;
			while (p < body.size() && !blank(body[p]) && !(equals && body[p] == '=')) ++p;
			const std::size_t keyEnd = p;
			while (p < body.size() && (blank(body[p]) || (equals && body[p] == '='))) ++p;
			if (keyEnd > keyBegin) {
				Field field;
				field.line = line;
				field.key = body.substr(keyBegin, keyEnd - keyBegin);
				if (p < body.size()) {
					const std::size_t valueBegin = p;
					while (p < body.size() && !blank(body[p])) ++p;
					field.value = body.substr(valueBegin, p - valueBegin);
					field.hasValue = true;
				}
				fields.push_back(field);
			}
		}
		++line;
	}
	return fields;
}

} // namespace configscan
