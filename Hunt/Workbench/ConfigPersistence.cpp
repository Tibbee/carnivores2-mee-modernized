#include "ConfigPersistence.h"

// The workbench's only filesystem writer. Kept out of the headers (which stay
// platform-free) and out of TuningSession (which stays I/O-free by design).

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "Core/ConfigMerge.h"

namespace workbench {

std::string FormatConfigValue(const Descriptor& descriptor, float value)
{
	char buffer[64];
	std::snprintf(buffer, sizeof(buffer), "%.*f", descriptor.dumpDecimals, value);
	return buffer;
}

namespace {

// The loader refuses these, so a writer that produced them would create a file
// the engine silently ignores. Mirrors Core/ConfigText.h and the offline
// c2_config_defaults.py guard.
constexpr std::size_t kMaxConfigBytes = 1024 * 1024;

bool ReadWholeFile(const std::string& path, std::string& bytes)
{
	std::ifstream input(path, std::ios::binary);
	if (!input.is_open()) return false;
	bytes.assign(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
	return !input.bad();
}

PersistRefusal Classify(const std::string& bytes)
{
	if (bytes.size() > kMaxConfigBytes) return PersistRefusal::TooLarge;
	if (bytes.size() >= 2) {
		const unsigned char a = static_cast<unsigned char>(bytes[0]);
		const unsigned char b = static_cast<unsigned char>(bytes[1]);
		if ((a == 0xFF && b == 0xFE) || (a == 0xFE && b == 0xFF)) return PersistRefusal::UnsupportedEncoding;
	}
	if (bytes.find('\0') != std::string::npos) return PersistRefusal::NulBytes;
	return PersistRefusal::None;
}

// The engine resolves config.cfg against the executable directory and falls back
// to the working directory, so a relative path here is legitimate. Make it
// absolute so the temp and backup are created in the destination's own
// directory: the replacement is then same-volume and atomic.
std::string ResolveAbsolute(const std::string& path)
{
	char buffer[MAX_PATH * 2];
	const DWORD written = GetFullPathNameA(path.c_str(), static_cast<DWORD>(sizeof(buffer)), buffer, nullptr);
	if (written == 0 || written >= sizeof(buffer)) return path;
	return std::string(buffer, written);
}

std::string LastErrorMessage(const char* action)
{
	return std::string(action) + " failed (error " + std::to_string(GetLastError()) + ")";
}

// Create with CREATE_NEW so an existing file is never overwritten: an earlier
// owner snapshot, or a temp left by a crash, is the operator's to remove.
bool CreateExclusive(const std::string& path, const std::string& bytes, std::string& error)
{
	HANDLE file = CreateFileA(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
	                          FILE_ATTRIBUTE_NORMAL, nullptr);
	if (file == INVALID_HANDLE_VALUE) {
		error = LastErrorMessage("create");
		return false;
	}
	const char* cursor = bytes.data();
	std::size_t remaining = bytes.size();
	while (remaining > 0) {
		const DWORD chunk = static_cast<DWORD>(remaining > 0x10000000u ? 0x10000000u : remaining);
		DWORD written = 0;
		if (!WriteFile(file, cursor, chunk, &written, nullptr) || written == 0) {
			error = LastErrorMessage("write");
			CloseHandle(file);
			// CREATE_NEW proves this call created the file, so a partial write
			// must not be mistaken for an earlier snapshot or a stale temp.
			DeleteFileA(path.c_str());
			return false;
		}
		cursor += written;
		remaining -= written;
	}
	if (!FlushFileBuffers(file)) {
		error = LastErrorMessage("flush");
		CloseHandle(file);
		DeleteFileA(path.c_str());
		return false;
	}
	CloseHandle(file);
	return true;
}

// Never reuse a backup name: an earlier snapshot is the operator's.
std::string PickBackupName(const std::string& path, std::string& error)
{
	std::string candidate = path + ".before-workbench.bak";
	for (int index = 1; index < 10000; ++index) {
		if (GetFileAttributesA(candidate.c_str()) == INVALID_FILE_ATTRIBUTES) return candidate;
		candidate = path + ".before-workbench." + std::to_string(index) + ".bak";
	}
	error = "too many existing backups";
	return {};
}

// The owned entries, in registry order, for the fields the operator changed.
// Changed fields with no loader key are collected separately: editing the file
// could not reproduce them, so they are reported rather than written.
void CollectEntries(const Session& session, std::vector<configmerge::Entry>& entries,
                    std::vector<Parameter>& unpublishable)
{
	for (const auto& descriptor : Descriptors()) {
		if (!session.Changed(descriptor.id)) continue;
		if (descriptor.persist != PersistPolicy::RecognizedConfigKey) {
			unpublishable.push_back(descriptor.id);
			continue;
		}
		const auto index = static_cast<std::size_t>(descriptor.id);
		entries.push_back({descriptor.key, FormatConfigValue(descriptor, session.Edited()[index])});
	}
}

} // namespace

const char* RefusalMessage(PersistRefusal refusal)
{
	switch (refusal) {
	case PersistRefusal::None: return "Ready.";
	case PersistRefusal::NoSession: return "No baseline captured. A hunt restart discards the session; capture again before saving.";
	case PersistRefusal::Comparing: return "Previewing loaded values. Return to edited values before saving.";
	case PersistRefusal::NothingToWrite: return "No changed field has a config.cfg key, or the file already holds these values.";
	case PersistRefusal::ReadFailed: return "config.cfg could not be read.";
	case PersistRefusal::TooLarge: return "config.cfg is larger than the engine's 1 MiB limit.";
	case PersistRefusal::UnsupportedEncoding: return "config.cfg is UTF-16; resave it as UTF-8 or ANSI first.";
	case PersistRefusal::NulBytes: return "config.cfg contains NUL padding; resave it before saving from here.";
	case PersistRefusal::SessionMoved: return "Edits changed since the review. Review again before saving.";
	case PersistRefusal::FileMoved: return "config.cfg changed since the review. Review again before saving.";
	case PersistRefusal::BackupFailed: return "The backup could not be written; config.cfg was not touched.";
	case PersistRefusal::TempExists: return "A stale config.cfg.workbench.tmp exists; remove it and retry.";
	case PersistRefusal::WriteFailed: return "The new config.cfg could not be written; the original is untouched.";
	case PersistRefusal::ReplaceFailed: return "config.cfg could not be replaced; the original is untouched.";
	}
	return "Unknown state.";
}

PersistPreview PreviewConfigWrite(const std::string& path, const Session& session)
{
	PersistPreview preview;
	preview.path = ResolveAbsolute(path);

	if (!session.IsLoaded()) {
		preview.refusal = PersistRefusal::NoSession;
		preview.message = RefusalMessage(preview.refusal);
		return preview;
	}
	if (session.IsComparing()) {
		preview.refusal = PersistRefusal::Comparing;
		preview.message = RefusalMessage(preview.refusal);
		return preview;
	}

	std::vector<configmerge::Entry> entries;
	CollectEntries(session, entries, preview.unpublishable);
	if (entries.empty()) {
		preview.refusal = PersistRefusal::NothingToWrite;
		preview.message = RefusalMessage(preview.refusal);
		return preview;
	}

	if (!ReadWholeFile(preview.path, preview.source)) {
		preview.refusal = PersistRefusal::ReadFailed;
		preview.message = RefusalMessage(preview.refusal);
		return preview;
	}
	preview.bytesBefore = preview.source.size();
	if (const PersistRefusal refused = Classify(preview.source); refused != PersistRefusal::None) {
		preview.refusal = refused;
		preview.message = RefusalMessage(refused);
		return preview;
	}

	const auto merge = configmerge::Merge(preview.source, entries);
	if (!merge.ok) {
		preview.refusal = PersistRefusal::NothingToWrite;
		preview.message = merge.error;
		return preview;
	}
	if (merge.report.changes.empty()) {
		// Every changed value is already spelled the same way on disk, which is
		// what a successful save leaves behind. Say so rather than reusing the
		// "nothing has a key" wording, so the operator can tell the two apart.
		preview.refusal = PersistRefusal::NothingToWrite;
		preview.message = "config.cfg already holds every changed value; there is nothing to write.";
		return preview;
	}

	preview.merged = merge.text;
	preview.bytesAfter = merge.text.size();
	preview.duplicates = merge.report.duplicates;
	for (const auto& change : merge.report.changes) {
		PersistField field;
		field.id = FindParameter(change.key);
		field.key = change.key;
		field.oldValue = change.oldValue;
		field.newValue = change.newValue;
		field.line = change.line;
		field.appended = change.action == configmerge::Action::Appended;
		preview.fields.push_back(field);
	}

	preview.refusal = PersistRefusal::None;
	preview.message = "Ready to write.";
	return preview;
}

PersistWrite ApplyConfigWrite(const std::string& path, const Session& session,
                              const PersistPreview& preview)
{
	PersistWrite result;
	result.path = ResolveAbsolute(path);

	if (!preview.ok()) {
		result.refusal = PersistRefusal::NothingToWrite;
		result.message = "Nothing was reviewed to write.";
		return result;
	}
	if (!session.IsLoaded() || session.IsComparing()) {
		result.refusal = PersistRefusal::SessionMoved;
		result.message = RefusalMessage(result.refusal);
		return result;
	}

	// Refuse before writing anything at all, so a refusal leaves no litter.
	const std::string temp = result.path + ".workbench.tmp";
	if (GetFileAttributesA(temp.c_str()) != INVALID_FILE_ATTRIBUTES) {
		result.refusal = PersistRefusal::TempExists;
		result.message = RefusalMessage(result.refusal);
		return result;
	}

	// The review the operator confirmed must still describe the same two things:
	// the bytes on disk, and the values in the session.
	std::string source;
	if (!ReadWholeFile(result.path, source)) {
		result.refusal = PersistRefusal::ReadFailed;
		result.message = RefusalMessage(result.refusal);
		return result;
	}
	if (source != preview.source) {
		result.refusal = PersistRefusal::FileMoved;
		result.message = RefusalMessage(result.refusal);
		return result;
	}

	std::vector<configmerge::Entry> entries;
	std::vector<Parameter> unpublishable;
	CollectEntries(session, entries, unpublishable);
	const auto merge = configmerge::Merge(source, entries);
	if (!merge.ok || merge.text != preview.merged) {
		result.refusal = PersistRefusal::SessionMoved;
		result.message = RefusalMessage(result.refusal);
		return result;
	}

	std::string error;
	const std::string backup = PickBackupName(result.path, error);
	if (backup.empty()) {
		result.refusal = PersistRefusal::BackupFailed;
		result.message = error.empty() ? RefusalMessage(result.refusal) : error;
		return result;
	}
	if (!CreateExclusive(backup, source, error)) {
		result.refusal = PersistRefusal::BackupFailed;
		result.message = std::string(RefusalMessage(result.refusal)) + " " + error;
		return result;
	}
	result.backup = backup.substr(result.path.size());

	if (!CreateExclusive(temp, merge.text, error)) {
		// The backup was this call's own snapshot of an original the temp
		// failure never touched; remove it so a failed save leaves no litter.
		DeleteFileA(backup.c_str());
		result.refusal = PersistRefusal::WriteFailed;
		result.message = std::string(RefusalMessage(result.refusal)) + " " + error;
		return result;
	}
	if (!MoveFileExA(temp.c_str(), result.path.c_str(), MOVEFILE_REPLACE_EXISTING)) {
		const std::string why = LastErrorMessage("replace");
		(void)DeleteFileA(temp.c_str());
		DeleteFileA(backup.c_str());
		result.refusal = PersistRefusal::ReplaceFailed;
		result.message = std::string(RefusalMessage(result.refusal)) + " " + why;
		return result;
	}

	result.refusal = PersistRefusal::None;
	result.message = "Saved. The values apply on the next launch.";
	result.keysWritten = preview.fields.size();
	result.bytesWritten = merge.text.size();
	return result;
}

} // namespace workbench
