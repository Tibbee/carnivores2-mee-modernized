#pragma once
#include <cstddef>
#include <string>
#include <vector>
#include "TuningSession.h"

// W3 write path: persist the session's changed values for the fields that have a
// recognized config.cfg key.
//
// This is the only place in the workbench allowed to touch the filesystem, and
// it never runs on the render path: the panel calls Preview on a button and
// Apply on a confirmation. The merge itself lives in Core/ConfigMerge.h and is
// pure; everything here is the protocol around it -- validate, preview, detect a
// concurrent edit, back up, then replace atomically.
//
// Only a field the operator actually changed is ever written, and only when the
// descriptor's policy says a loader key exists. The thirteen legacy scalars and
// the beam tint have no key, so editing the file could not reproduce them and
// they are reported as unpublishable instead of being written as a lie.

namespace workbench {

enum class PersistRefusal {
	None,
	NoSession,           // No captured baseline, or a hunt restart invalidated it.
	Comparing,           // The panel is previewing loaded values.
	NothingToWrite,      // No publishable field differs from the file.
	ReadFailed,          // config.cfg is missing or unreadable.
	TooLarge,            // Over the loader's 1 MiB ceiling.
	UnsupportedEncoding, // UTF-16 is not this format.
	NulBytes,            // The loader recovers and would resave padded bytes.
	SessionMoved,        // Edits or the baseline changed since the preview.
	FileMoved,           // Another writer changed config.cfg since the preview.
	BackupFailed,
	TempExists,          // A stale temp file is in the way; it is left alone.
	WriteFailed,
	ReplaceFailed,       // The original file is intact; the temp was removed.
};

struct PersistField {
	Parameter id = Parameter::Count;
	std::string key;
	std::string oldValue; // Empty when the key is absent from the file.
	std::string newValue;
	std::size_t line = 0; // 0-based line in the source file; unused when appended.
	bool appended = false;
};

struct PersistPreview {
	PersistRefusal refusal = PersistRefusal::NoSession;
	std::string message;
	std::string path;                      // Absolute destination.
	std::size_t bytesBefore = 0, bytesAfter = 0;
	std::vector<PersistField> fields;      // Publishable and different.
	std::vector<std::string> duplicates;   // Owned keys with an inert earlier line.
	std::vector<Parameter> unpublishable;  // Changed, but no loader key exists.
	std::string source;                    // Bytes read, for the race check.
	std::string merged;                    // Bytes that Apply would write.

	bool ok() const { return refusal == PersistRefusal::None; }
};

struct PersistWrite {
	PersistRefusal refusal = PersistRefusal::NoSession;
	std::string message;
	std::string path;
	std::string backup; // File name only, empty when no backup was needed.
	std::size_t keysWritten = 0;
	std::size_t bytesWritten = 0;

	bool ok() const { return refusal == PersistRefusal::None; }
};

// Resolve a destination and derive everything the panel needs to show a review.
// Reads the file; writes nothing.
PersistPreview PreviewConfigWrite(const std::string& path, const Session& session);

// Run the protocol. Re-reads the file and re-derives the merge, refusing if
// either the file or the session moved since the preview, so the bytes written
// are exactly the bytes that were reviewed.
PersistWrite ApplyConfigWrite(const std::string& path, const Session& session,
                              const PersistPreview& preview);

// The token this writer emits for a value. Exposed so a test can prove the round
// trip through the loader's strict parser for every publishable field, using the
// production formatter rather than re-deriving the rule and testing that.
std::string FormatConfigValue(const Descriptor& descriptor, float value);

const char* RefusalMessage(PersistRefusal refusal);

} // namespace workbench
