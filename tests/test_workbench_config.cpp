#include <gtest/gtest.h>
#include "Core/ConfigMerge.h"
#include "Workbench/ConfigPersistence.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

// The merge is the risky part of the W3 write path: it decides which bytes of a
// hand-edited config.cfg survive a save. These cases pin the rules that follow
// from the engine loader (Game/EngineInit.cpp LoadConfig) rather than from
// preference, so a regression has to argue with the reader, not with this file.
//
// Line endings are always written with explicit escapes. Raw string literals
// would inherit whatever terminator the checkout uses and silently test
// something else on a differently-configured working tree.
namespace {
using namespace configmerge;
using namespace workbench;

// A file with every shape the merge must leave alone, used by several cases.
const std::string kAwkward =
	"\xEF\xBB\xBF" "# comment\r\n"
	"   \t \r\n"
	"env0_decay\t1.000\r\n"
	"  night_sky_brightness   85   # hand tuned\r\n"
	"#env0_decay 9.9\r\n"
	"env0_decay=2.0\r\n"
	"env0_decay\r\n"
	"MY_MOD_KEY x\r\n";
} // namespace

TEST(ConfigMerge, NoChangesLeaveEveryByteIdentical) {
	const std::string text = kAwkward;
	const auto result = Merge(text, {{"env0_decay", "1.000"}, {"night_sky_brightness", "85"}});
	ASSERT_TRUE(result.ok) << result.error;
	EXPECT_EQ(result.text, text);
	EXPECT_TRUE(result.report.changes.empty());
	EXPECT_TRUE(result.report.duplicates.empty());
}

TEST(ConfigMerge, AnUnrelatedKeyNeverMoves) {
	const std::string text =
		"renderer 1\r\n"
		"night_world_brightness 15\r\n"
		"fov 62\r\n";
	const auto result = Merge(text, {{"night_world_brightness", "16"}});
	ASSERT_TRUE(result.ok) << result.error;
	EXPECT_EQ(result.text, "renderer 1\r\nnight_world_brightness 16\r\nfov 62\r\n");
	ASSERT_EQ(result.report.changes.size(), 1u);
	EXPECT_EQ(result.report.changes[0].action, Action::Updated);
	EXPECT_EQ(result.report.changes[0].key, "night_world_brightness");
	EXPECT_EQ(result.report.changes[0].oldValue, "15");
	EXPECT_EQ(result.report.changes[0].newValue, "16");
	EXPECT_EQ(result.report.changes[0].line, 1u);
}

TEST(ConfigMerge, CROnlyFilesKeepCROnEveryLine) {
	// Classic Mac terminators: the loader splits on \r just as on \n.
	const std::string text = "renderer 1\renv0_decay 1.0\rb 2\r";
	const auto result = Merge(text, {{"env0_decay", "1.5"}});
	ASSERT_TRUE(result.ok) << result.error;
	EXPECT_EQ(result.text, "renderer 1\renv0_decay 1.5\rb 2\r");

	const std::string append = "renderer 1\r";
	const auto appended = Merge(append, {{"env0_decay", "1.5"}});
	ASSERT_TRUE(appended.ok) << appended.error;
	EXPECT_EQ(appended.text, "renderer 1\renv0_decay 1.5\r");
}

TEST(ConfigMerge, TrailingWhitespaceAndTrailingTokensSurviveAValueRewrite) {
	const std::string text =
		"env0_decay 1.000   \r\n"
		"env0_decay 1.000 2.0\r\n";
	const auto result = Merge(text, {{"env0_decay", "1.5"}});
	ASSERT_TRUE(result.ok) << result.error;
	// Only the value token of the reader's last active occurrence moves: the
	// trailing spaces after the first line and the trailing token on the second
	// are not this writer's to clean up.
	EXPECT_EQ(result.text, "env0_decay 1.000   \r\nenv0_decay 1.5 2.0\r\n");
	ASSERT_EQ(result.report.changes.size(), 1u);
	EXPECT_EQ(result.report.changes[0].line, 1u);
	ASSERT_EQ(result.report.duplicates.size(), 1u);
}

TEST(ConfigMerge, AValueContainingHashIsReplacedByTheNewToken) {
	// LoadConfig rejects "1.5#x" (strtof does not consume the whole token), so
	// the old line was inert; replacing the whole token makes the new value the
	// one the loader reads instead of appending a second live key.
	const std::string text = "env0_decay 1.5#x\r\n";
	const auto result = Merge(text, {{"env0_decay", "1.6"}});
	ASSERT_TRUE(result.ok) << result.error;
	EXPECT_EQ(result.text, "env0_decay 1.6\r\n");
	ASSERT_EQ(result.report.changes.size(), 1u);
	EXPECT_EQ(result.report.changes[0].action, Action::Updated);
}

TEST(ConfigMerge, OnlyValueTokensChangeInAFileOfAwkwardShapes) {
	const auto result = Merge(kAwkward, {{"env0_decay", "1.500"}, {"night_sky_brightness", "90"}});
	ASSERT_TRUE(result.ok) << result.error;
	// Indent, key case, the tab separator and the trailing hand comment survive;
	// the commented example, the "key=value" line and the bare "key" line do not
	// become occurrences, and the mid-file unknown key is untouched.
	const std::string expected =
		"\xEF\xBB\xBF" "# comment\r\n"
		"   \t \r\n"
		"env0_decay\t1.500\r\n"
		"  night_sky_brightness   90   # hand tuned\r\n"
		"#env0_decay 9.9\r\n"
		"env0_decay=2.0\r\n"
		"env0_decay\r\n"
		"MY_MOD_KEY x\r\n";
	EXPECT_EQ(result.text, expected);
	ASSERT_EQ(result.report.changes.size(), 2u);
	EXPECT_EQ(result.report.changes[0].key, "env0_decay");
	EXPECT_EQ(result.report.changes[0].line, 2u);
	EXPECT_EQ(result.report.changes[1].key, "night_sky_brightness");
	EXPECT_EQ(result.report.changes[1].line, 3u);
	EXPECT_TRUE(result.report.duplicates.empty());
}

TEST(ConfigMerge, AUtf8BomIsPreservedAndDoesNotHideTheFirstKey) {
	const std::string text = "\xEF\xBB\xBF" "env0_decay 1.0\r\n";
	const auto result = Merge(text, {{"env0_decay", "1.5"}});
	ASSERT_TRUE(result.ok) << result.error;
	EXPECT_EQ(result.text, "\xEF\xBB\xBF" "env0_decay 1.5\r\n");
	ASSERT_EQ(result.report.changes.size(), 1u);
	EXPECT_EQ(result.report.changes[0].action, Action::Updated);
}

TEST(ConfigMerge, ABomAfterTheFirstLineIsPartOfTheKey) {
	// ReadConfigText strips only a file-leading BOM, so a stray one later is data.
	const std::string text = "renderer 1\r\n" "\xEF\xBB\xBF" "env0_decay 1.0\r\n";
	const auto result = Merge(text, {{"env0_decay", "1.5"}});
	ASSERT_TRUE(result.ok) << result.error;
	EXPECT_EQ(result.text, "renderer 1\r\n" "\xEF\xBB\xBF" "env0_decay 1.0\r\nenv0_decay 1.5\r\n");
	ASSERT_EQ(result.report.changes.size(), 1u);
	EXPECT_EQ(result.report.changes[0].action, Action::Appended);
}

TEST(ConfigMerge, TheLastActiveOccurrenceWinsAndEarlierDuplicatesAreReported) {
	const std::string text =
		"env0_decay 1.0\r\n"
		"env0_decay 1.2\r\n"
		"env0_decay 1.4\r\n";
	const auto result = Merge(text, {{"env0_decay", "1.6"}});
	ASSERT_TRUE(result.ok) << result.error;
	// The reader assigns as it walks, so line 3 is the one that has to move.
	EXPECT_EQ(result.text, "env0_decay 1.0\r\nenv0_decay 1.2\r\nenv0_decay 1.6\r\n");
	ASSERT_EQ(result.report.changes.size(), 1u);
	EXPECT_EQ(result.report.changes[0].line, 2u);
	ASSERT_EQ(result.report.duplicates.size(), 1u);
	EXPECT_EQ(result.report.duplicates[0], "env0_decay");
}

TEST(ConfigMerge, DuplicatesAreReportedOnlyForOwnedKeys) {
	const std::string text =
		"my_mod_key 1\r\n"
		"my_mod_key 2\r\n"
		"env0_decay 1.0\r\n";
	const auto result = Merge(text, {{"env0_decay", "1.5"}});
	ASSERT_TRUE(result.ok) << result.error;
	EXPECT_TRUE(result.report.duplicates.empty());
}

TEST(ConfigMerge, ACommentedKeyIsAbsentAndIsAppendedWithoutUncommenting) {
	const std::string text =
		"#env0_decay 1.49\r\n"
		"renderer 1\r\n";
	const auto result = Merge(text, {{"env0_decay", "1.60"}});
	ASSERT_TRUE(result.ok) << result.error;
	// The example line is a doc comment; the real setting is appended.
	EXPECT_EQ(result.text, "#env0_decay 1.49\r\nrenderer 1\r\nenv0_decay 1.60\r\n");
	ASSERT_EQ(result.report.changes.size(), 1u);
	EXPECT_EQ(result.report.changes[0].action, Action::Appended);
}

TEST(ConfigMerge, AnIndentedCommentIsNotAnOccurrence) {
	const std::string text = "   #env0_decay 1.49\r\n";
	const auto result = Merge(text, {{"env0_decay", "1.60"}});
	ASSERT_TRUE(result.ok) << result.error;
	EXPECT_EQ(result.text, "   #env0_decay 1.49\r\nenv0_decay 1.60\r\n");
	ASSERT_EQ(result.report.changes.size(), 1u);
	EXPECT_EQ(result.report.changes[0].action, Action::Appended);
}

TEST(ConfigMerge, VerticalTabAndFormFeedSeparateTokensLikeTheLoader) {
	// sscanf_s treats '\v' and '\f' as whitespace, so the loader reads both
	// spellings as an active env0_decay. If the merge did not, it would rewrite
	// an earlier line and leave a later \v/\f line to override the saved value
	// on the next launch, telling the operator the write had taken effect.
	const std::string tabbed = "env0_decay\v1.2\r\n";
	const auto inner = Merge(tabbed, {{"env0_decay", "1.6"}});
	ASSERT_TRUE(inner.ok) << inner.error;
	EXPECT_EQ(inner.text, "env0_decay\v1.6\r\n");
	ASSERT_EQ(inner.report.changes.size(), 1u);
	EXPECT_EQ(inner.report.changes[0].action, Action::Updated);
	EXPECT_EQ(inner.report.changes[0].oldValue, "1.2");

	const std::string fed =
		"env0_decay 1.0\r\n"
		"env0_decay\f1.4\r\n";
	const auto result = Merge(fed, {{"env0_decay", "1.6"}});
	ASSERT_TRUE(result.ok) << result.error;
	// The loader's last active occurrence is the form-feed line, so that is the
	// line that must move; the earlier value survives byte-identical.
	EXPECT_EQ(result.text, "env0_decay 1.0\r\nenv0_decay\f1.6\r\n");
	ASSERT_EQ(result.report.changes.size(), 1u);
	EXPECT_EQ(result.report.changes[0].line, 1u);
	ASSERT_EQ(result.report.duplicates.size(), 1u);
}

TEST(ConfigMerge, AnEqualsSeparatedLineIsNotAnOccurrence) {
	// The loader reads one token, "env0_decay=1.0", which matches no key, so the
	// line is inert: append a real setting rather than rewriting dead text.
	const std::string text = "env0_decay=1.0\r\n";
	const auto result = Merge(text, {{"env0_decay", "1.5"}});
	ASSERT_TRUE(result.ok) << result.error;
	EXPECT_EQ(result.text, "env0_decay=1.0\r\nenv0_decay 1.5\r\n");
	ASSERT_EQ(result.report.changes.size(), 1u);
	EXPECT_EQ(result.report.changes[0].action, Action::Appended);
}

TEST(ConfigMerge, AKeyWithoutAValueIsNotAnOccurrence) {
	// LoadConfig logs "missing value, ignoring" for a one-token line.
	const std::string text = "env0_decay\r\n";
	const auto result = Merge(text, {{"env0_decay", "1.5"}});
	ASSERT_TRUE(result.ok) << result.error;
	EXPECT_EQ(result.text, "env0_decay\r\nenv0_decay 1.5\r\n");
	ASSERT_EQ(result.report.changes.size(), 1u);
	EXPECT_EQ(result.report.changes[0].action, Action::Appended);
}

TEST(ConfigMerge, CRLFSurvivesOnUpdatedLines) {
	// Menu/Resources.cpp SaveConfig rewrites owned keys with a bare LF while
	// passing other lines through with their CR. This writer must not: a save
	// may not silently mix terminators on the lines it touches.
	const std::string text = "a 1\r\nenv0_decay 1.0\r\nb 2\r\n";
	const auto result = Merge(text, {{"env0_decay", "1.5"}});
	ASSERT_TRUE(result.ok) << result.error;
	EXPECT_EQ(result.text, "a 1\r\nenv0_decay 1.5\r\nb 2\r\n");
	EXPECT_NE(result.text.find("1.5\r\n"), std::string::npos);
	// A bare LF after the value would be the menu's mixed-ending bug: "1.5\r\n"
	// does not contain "1.5\n", so npos here means the terminator really is CRLF.
	EXPECT_EQ(result.text.find("1.5\n"), std::string::npos);
}

TEST(ConfigMerge, LFFilesStayLF) {
	const std::string text = "env0_decay 1.0\nrenderer 1\n";
	const auto result = Merge(text, {{"env0_decay", "1.5"}});
	ASSERT_TRUE(result.ok) << result.error;
	EXPECT_EQ(result.text, "env0_decay 1.5\nrenderer 1\n");
	EXPECT_EQ(result.text.find('\r'), std::string::npos);
}

TEST(ConfigMerge, AMissingFinalTerminatorIsPreservedWhenNothingChanges) {
	const std::string text = "renderer 1\r\nenv0_decay 1.400";
	const auto result = Merge(text, {{"env0_decay", "1.400"}});
	ASSERT_TRUE(result.ok) << result.error;
	EXPECT_EQ(result.text, text);
}

TEST(ConfigMerge, AMissingFinalTerminatorSurvivesAnUpdate) {
	const std::string text = "renderer 1\r\nenv0_decay 1.400";
	const auto result = Merge(text, {{"env0_decay", "1.5"}});
	ASSERT_TRUE(result.ok) << result.error;
	EXPECT_EQ(result.text, "renderer 1\r\nenv0_decay 1.5");
}

TEST(ConfigMerge, AMissingFinalTerminatorIsCompletedBeforeAnAppend) {
	const std::string text = "renderer 1";
	const auto result = Merge(text, {{"env0_decay", "1.4"}});
	ASSERT_TRUE(result.ok) << result.error;
	EXPECT_EQ(result.text, "renderer 1\r\nenv0_decay 1.4\r\n");
}

TEST(ConfigMerge, AnAppendAdoptsTheFilesOwnTerminator) {
	const std::string text = "renderer 1\nfov 62\n";
	const auto result = Merge(text, {{"env0_decay", "1.4"}});
	ASSERT_TRUE(result.ok) << result.error;
	EXPECT_EQ(result.text, "renderer 1\nfov 62\nenv0_decay 1.4\n");
}

TEST(ConfigMerge, AnEmptyFileAndACommentsOnlyFileBothAppend) {
	const auto empty = Merge("", {{"env0_decay", "1.4"}});
	ASSERT_TRUE(empty.ok) << empty.error;
	EXPECT_EQ(empty.text, "env0_decay 1.4\r\n");
	ASSERT_EQ(empty.report.changes.size(), 1u);
	EXPECT_EQ(empty.report.changes[0].action, Action::Appended);

	const auto comments = Merge("# nothing here\r\n", {{"env0_decay", "1.4"}});
	ASSERT_TRUE(comments.ok) << comments.error;
	EXPECT_EQ(comments.text, "# nothing here\r\nenv0_decay 1.4\r\n");
}

TEST(ConfigMerge, AnAbsentKeyAppendsExactlyOnce) {
	const std::string text = "renderer 1\r\n";
	const auto result = Merge(text, {{"env0_decay", "1.4"}});
	ASSERT_TRUE(result.ok) << result.error;
	EXPECT_EQ(result.text, "renderer 1\r\nenv0_decay 1.4\r\n");
	ASSERT_EQ(result.report.changes.size(), 1u);
	EXPECT_EQ(result.report.changes[0].line, 1u);
}

TEST(ConfigMerge, AppendedKeysFollowCallerOrderNotAlphabeticalOrder) {
	const auto result = Merge("", {{"env8_reverb", "0.4"}, {"env0_decay", "1.4"}});
	ASSERT_TRUE(result.ok) << result.error;
	EXPECT_EQ(result.text, "env8_reverb 0.4\r\nenv0_decay 1.4\r\n");
	ASSERT_EQ(result.report.changes.size(), 2u);
	EXPECT_EQ(result.report.changes[0].key, "env8_reverb");
	EXPECT_EQ(result.report.changes[1].key, "env0_decay");
}

TEST(ConfigMerge, KeysMatchCaseInsensitivelyLikeTheLoader) {
	const std::string text = "ENV0_DECAY 1.0\r\n";
	const auto result = Merge(text, {{"env0_decay", "1.5"}});
	ASSERT_TRUE(result.ok) << result.error;
	// The operator's spelling is preserved; only the value moves.
	EXPECT_EQ(result.text, "ENV0_DECAY 1.5\r\n");
	ASSERT_EQ(result.report.changes.size(), 1u);
	EXPECT_EQ(result.report.changes[0].action, Action::Updated);
}

TEST(ConfigMerge, AnUnparseableValueIsStillUpdatedInPlace) {
	// LoadConfig rejects "abc" and keeps the compiled default, so the field can
	// only become dirty if the operator changed it; the dead token is replaced.
	const std::string text = "env0_decay abc\r\n";
	const auto result = Merge(text, {{"env0_decay", "1.5"}});
	ASSERT_TRUE(result.ok) << result.error;
	EXPECT_EQ(result.text, "env0_decay 1.5\r\n");
	ASSERT_EQ(result.report.changes.size(), 1u);
	EXPECT_EQ(result.report.changes[0].oldValue, "abc");
}

TEST(ConfigMerge, EntriesThatCannotBeTokenisedAreRejected) {
	const Entry rejected[] = {
		{"env0_decay", ""},
		{"", "1.0"},
		{"env0_decay", "1.0 "},
		{"env0_decay", " 1.0"},
		{"env0_decay", "1 0"},
		{"env0_decay", "1\r\n0"},
		{"a b", "1.0"},
		{"a\tb", "1.0"},
		{"a\r\nb", "1.0"},
		// A '#' key could never be read back: the loader treats it as a comment.
		{"#env0_decay", "1.0"},
		{"#", "1.0"},
	};
	for (const auto& entry : rejected) {
		const auto result = Merge("renderer 1\r\n", {entry});
		EXPECT_FALSE(result.ok) << "accepted key=[" << entry.key << "] value=[" << entry.value << "]";
		EXPECT_FALSE(result.error.empty());
		EXPECT_TRUE(result.text.empty());
	}
}

TEST(ConfigMerge, NoEntriesLeavesTheFileUntouched) {
	const std::string text = kAwkward;
	const auto result = Merge(text, {});
	ASSERT_TRUE(result.ok) << result.error;
	EXPECT_EQ(result.text, text);
	EXPECT_TRUE(result.report.changes.empty());
}

TEST(ConfigMerge, SeveralKeysUpdateAndAppendInOnePass) {
	const std::string text =
		"renderer 1\r\n"
		"env3_reverb 0.275\r\n"
		"# comment\r\n"
		"night_moon_brightness 200\r\n";
	const auto result = Merge(text, {
		{"env3_reverb", "0.400"},
		{"env0_decay", "1.490"},
		{"night_moon_brightness", "220"},
	});
	ASSERT_TRUE(result.ok) << result.error;
	EXPECT_EQ(result.text,
		"renderer 1\r\n"
		"env3_reverb 0.400\r\n"
		"# comment\r\n"
		"night_moon_brightness 220\r\n"
		"env0_decay 1.490\r\n");
	ASSERT_EQ(result.report.changes.size(), 3u);
	// File order for updates, caller order for appends.
	EXPECT_EQ(result.report.changes[0].key, "env3_reverb");
	EXPECT_EQ(result.report.changes[1].key, "night_moon_brightness");
	EXPECT_EQ(result.report.changes[2].action, Action::Appended);
	EXPECT_EQ(result.report.changes[2].key, "env0_decay");
}

// ---------------------------------------------------------------------------
// The write protocol: what the panel does when the operator confirms a review.
// These run against real files in the system temp directory. Each case gets its
// own directory so a failure cannot be blamed on a neighbour.
// ---------------------------------------------------------------------------

namespace {
namespace fs = std::filesystem;

std::string ReadFileText(const std::string& path)
{
	std::ifstream input(path, std::ios::binary);
	return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

// The value the engine loader would take for a key: the second token of the LAST
// line whose first token is not a comment and matches. Mirrors LoadConfig.
std::string LoadedToken(const std::string& text, const std::string& key)
{
	std::string found;
	std::size_t start = 0;
	while (start <= text.size()) {
		std::size_t end = text.find_first_of("\r\n", start);
		if (end == std::string::npos) end = text.size();
		std::string line = text.substr(start, end - start);
		start = end + 1;
		if (line.empty()) continue;
		std::size_t first = line.find_first_not_of(" \t\v\f");
		if (first == std::string::npos || line[first] == '#') continue;
		std::size_t keyEnd = line.find_first_of(" \t\v\f", first);
		if (keyEnd == std::string::npos) continue;
		if (line.substr(first, keyEnd - first) != key) continue;
		std::size_t valueBegin = line.find_first_not_of(" \t\v\f", keyEnd);
		if (valueBegin == std::string::npos) continue;
		std::size_t valueEnd = line.find_first_of(" \t\v\f", valueBegin);
		found = line.substr(valueBegin, valueEnd == std::string::npos ? valueEnd : valueEnd - valueBegin);
	}
	return found;
}

class ConfigPersistence : public ::testing::Test {
protected:
	std::string dir;

	void SetUp() override
	{
		dir = ::testing::TempDir() + "c2_w3_" + std::to_string(reinterpret_cast<std::uintptr_t>(this));
		std::error_code ignored;
		fs::remove_all(dir, ignored);
		fs::create_directories(dir);
	}
	void TearDown() override
	{
		std::error_code ignored;
		fs::remove_all(dir, ignored);
	}

	std::string Path(const std::string& name = "config.cfg") const { return dir + "/" + name; }
	void Write(const std::string& path, const std::string& bytes) const
	{
		std::ofstream output(path, std::ios::binary | std::ios::trunc);
		output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
	}
	bool Exists(const std::string& path) const { return fs::exists(path); }

	// A captured session with one reverb field changed to 1.6.
	static Session SessionWithEnv0Decay(float value)
	{
		Session session;
		EXPECT_EQ(session.Capture(CompiledDefaults()), Error::None);
		EXPECT_EQ(session.Edit(Parameter::Env0Decay, value), Error::None);
		return session;
	}
};

const std::string kOriginal = "renderer 1\r\nenv0_decay 1.490\r\n";
} // namespace

TEST_F(ConfigPersistence, PreviewReadsAndWritesNothing) {
	const std::string path = Path();
	Write(path, kOriginal);
	auto session = SessionWithEnv0Decay(1.6f);

	const auto preview = PreviewConfigWrite(path, session);
	ASSERT_TRUE(preview.ok()) << preview.message;
	EXPECT_EQ(ReadFileText(path), kOriginal) << "a preview must not touch the file";
	// GetFullPathNameA returns the native form, so compare identity rather than
	// spelling: resolution must point at the same file it was given.
	EXPECT_EQ(fs::weakly_canonical(preview.path), fs::weakly_canonical(path))
		<< "an already-absolute path must survive resolution";
	EXPECT_EQ(preview.bytesBefore, kOriginal.size());
	// Replacing a value of equal width must not resize the file.
	EXPECT_EQ(preview.bytesAfter, kOriginal.size());
	EXPECT_NE(preview.merged, preview.source);
	ASSERT_EQ(preview.fields.size(), 1u);
	EXPECT_EQ(preview.fields[0].key, "env0_decay");
	EXPECT_EQ(preview.fields[0].oldValue, "1.490");
	EXPECT_EQ(preview.fields[0].newValue, "1.600");
	EXPECT_FALSE(preview.fields[0].appended);
	EXPECT_TRUE(preview.unpublishable.empty());
}

TEST_F(ConfigPersistence, ApplyWritesTheReviewedBytesAndKeepsTheOriginalAsABackup) {
	const std::string path = Path();
	Write(path, kOriginal);
	auto session = SessionWithEnv0Decay(1.6f);

	const auto preview = PreviewConfigWrite(path, session);
	ASSERT_TRUE(preview.ok()) << preview.message;
	const auto write = ApplyConfigWrite(path, session, preview);
	ASSERT_TRUE(write.ok()) << write.message;
	EXPECT_EQ(write.keysWritten, 1u);
	EXPECT_EQ(write.backup, ".before-workbench.bak");
	EXPECT_EQ(ReadFileText(path), preview.merged);
	EXPECT_EQ(ReadFileText(path + ".before-workbench.bak"), kOriginal);
	EXPECT_FALSE(Exists(path + ".workbench.tmp")) << "the temp must not survive a clean save";
}

TEST_F(ConfigPersistence, WhatWasWrittenIsWhatTheLoaderWouldReadBack) {
	const std::string path = Path();
	Write(path, kOriginal);
	auto session = SessionWithEnv0Decay(17.25f);

	const auto preview = PreviewConfigWrite(path, session);
	ASSERT_TRUE(preview.ok()) << preview.message;
	ASSERT_TRUE(ApplyConfigWrite(path, session, preview).ok());

	// End to end: the token in the file must parse back to the edited value.
	const std::string token = LoadedToken(ReadFileText(path), "env0_decay");
	ASSERT_FALSE(token.empty());
	float parsed = 0.f;
	ASSERT_EQ(ParseValue(Parameter::Env0Decay, token, parsed), Error::None) << "token was " << token;
	EXPECT_TRUE(SameValue(parsed, 17.25f)) << "token was " << token;
}

TEST_F(ConfigPersistence, ASecondSaveKeepsTheFirstBackup) {
	const std::string path = Path();
	Write(path, kOriginal);
	auto session = SessionWithEnv0Decay(1.6f);
	const auto first = PreviewConfigWrite(path, session);
	ASSERT_TRUE(first.ok()) << first.message;
	ASSERT_TRUE(ApplyConfigWrite(path, session, first).ok());

	ASSERT_EQ(session.Edit(Parameter::Env0Decay, 1.7f), Error::None);
	const auto second = PreviewConfigWrite(path, session);
	ASSERT_TRUE(second.ok()) << second.message;
	const auto write = ApplyConfigWrite(path, session, second);
	ASSERT_TRUE(write.ok()) << write.message;
	EXPECT_EQ(write.backup, ".before-workbench.1.bak");
	// The first snapshot still holds the file as it was before any save.
	EXPECT_EQ(ReadFileText(path + ".before-workbench.bak"), kOriginal);
	EXPECT_EQ(ReadFileText(path + ".before-workbench.1.bak"), first.merged);
}

TEST_F(ConfigPersistence, AConcurrentEditBetweenReviewAndSaveRefuses) {
	const std::string path = Path();
	Write(path, kOriginal);
	auto session = SessionWithEnv0Decay(1.6f);
	const auto preview = PreviewConfigWrite(path, session);
	ASSERT_TRUE(preview.ok()) << preview.message;

	const std::string someoneElse = std::string(kOriginal) + "fov 62\r\n";
	Write(path, someoneElse);
	const auto write = ApplyConfigWrite(path, session, preview);
	EXPECT_EQ(write.refusal, PersistRefusal::FileMoved);
	EXPECT_EQ(ReadFileText(path), someoneElse);
	EXPECT_FALSE(Exists(path + ".before-workbench.bak")) << "a refusal must leave nothing behind";
	EXPECT_FALSE(Exists(path + ".workbench.tmp"));
}

TEST_F(ConfigPersistence, AnEditAfterTheReviewRefuses) {
	const std::string path = Path();
	Write(path, kOriginal);
	auto session = SessionWithEnv0Decay(1.6f);
	const auto preview = PreviewConfigWrite(path, session);
	ASSERT_TRUE(preview.ok()) << preview.message;

	ASSERT_EQ(session.Edit(Parameter::Env1Decay, 2.0f), Error::None);
	const auto write = ApplyConfigWrite(path, session, preview);
	EXPECT_EQ(write.refusal, PersistRefusal::SessionMoved);
	EXPECT_EQ(ReadFileText(path), kOriginal);
	EXPECT_FALSE(Exists(path + ".before-workbench.bak"));
}

TEST_F(ConfigPersistence, AStaleTempFileIsLeftAloneAndRefuses) {
	const std::string path = Path();
	Write(path, kOriginal);
	Write(path + ".workbench.tmp", "left by a crash");
	auto session = SessionWithEnv0Decay(1.6f);
	const auto preview = PreviewConfigWrite(path, session);
	ASSERT_TRUE(preview.ok()) << preview.message;

	const auto write = ApplyConfigWrite(path, session, preview);
	EXPECT_EQ(write.refusal, PersistRefusal::TempExists);
	// The stale file is the operator's to remove; this must never delete it.
	EXPECT_EQ(ReadFileText(path + ".workbench.tmp"), "left by a crash");
	EXPECT_EQ(ReadFileText(path), kOriginal);
	EXPECT_FALSE(Exists(path + ".before-workbench.bak"));
}

TEST_F(ConfigPersistence, ARefusedReplaceRemovesItsOwnBackupAndTemp) {
	const std::string path = Path();
	Write(path, kOriginal);
	// A read-only destination is the deterministic way to make MoveFileExA fail
	// after the backup and temp have been written.
	fs::permissions(path, fs::perms::owner_read, fs::perm_options::replace);
	auto session = SessionWithEnv0Decay(1.6f);
	const auto preview = PreviewConfigWrite(path, session);
	ASSERT_TRUE(preview.ok()) << preview.message;

	const auto write = ApplyConfigWrite(path, session, preview);
	EXPECT_EQ(write.refusal, PersistRefusal::ReplaceFailed);
	EXPECT_EQ(ReadFileText(path), kOriginal) << "the original must be untouched";
	EXPECT_FALSE(Exists(path + ".workbench.tmp")) << "the temp must not outlive the failure";
	EXPECT_FALSE(Exists(path + ".before-workbench.bak"))
		<< "a backup this call created for a write that failed must not be left behind";

	fs::permissions(path, fs::perms::owner_read | fs::perms::owner_write, fs::perm_options::replace);
}

TEST_F(ConfigPersistence, ARefusedPreviewNeverReportsReady) {
	const std::string path = Path();
	Write(path, kOriginal);
	auto clean = SessionWithEnv0Decay(1.490f); // same as the file: nothing to write
	const auto unchanged = PreviewConfigWrite(path, clean);
	EXPECT_EQ(unchanged.refusal, PersistRefusal::NothingToWrite);
	EXPECT_FALSE(unchanged.ok());
	EXPECT_FALSE(unchanged.message.empty());

	Session unloaded;
	EXPECT_EQ(PreviewConfigWrite(path, unloaded).refusal, PersistRefusal::NoSession);

	auto comparing = SessionWithEnv0Decay(1.6f);
	ASSERT_EQ(comparing.Compare(true), Error::None);
	EXPECT_EQ(PreviewConfigWrite(path, comparing).refusal, PersistRefusal::Comparing);
}

TEST_F(ConfigPersistence, UnreadableAndUnsupportedFilesAreRefused) {
	auto session = SessionWithEnv0Decay(1.6f);

	const std::string missing = Path("absent.cfg");
	EXPECT_EQ(PreviewConfigWrite(missing, session).refusal, PersistRefusal::ReadFailed);

	const std::string utf16 = Path("utf16.cfg");
	Write(utf16, std::string("\xFF\xFE") + "r\x00e\x00");
	EXPECT_EQ(PreviewConfigWrite(utf16, session).refusal, PersistRefusal::UnsupportedEncoding);

	const std::string padded = Path("pad.cfg");
	Write(padded, std::string("renderer 1\r\n\0\0\0", 16));
	EXPECT_EQ(PreviewConfigWrite(padded, session).refusal, PersistRefusal::NulBytes);

	const std::string huge = Path("huge.cfg");
	Write(huge, "renderer 1\r\n" + std::string(1024 * 1024 + 1, 'x'));
	EXPECT_EQ(PreviewConfigWrite(huge, session).refusal, PersistRefusal::TooLarge);
}

TEST_F(ConfigPersistence, ExactlyTheByteLimitIsAcceptedAndOneMoreIsRefused) {
	// ReadConfigText accepts a file of exactly 1 MiB and rejects one byte more;
	// the protocol must draw the same line, or a readable config would refuse.
	const std::string path = Path();
	std::string text = kOriginal;
	text.resize(1024 * 1024, ' '); // trailing whitespace line, no terminator
	Write(path, text);
	auto session = SessionWithEnv0Decay(1.6f);
	const auto preview = PreviewConfigWrite(path, session);
	ASSERT_TRUE(preview.ok()) << preview.message;
	EXPECT_EQ(preview.bytesBefore, 1024u * 1024u);

	Write(path, text + "x");
	EXPECT_EQ(PreviewConfigWrite(path, session).refusal, PersistRefusal::TooLarge);
}

TEST_F(ConfigPersistence, AChangedLegacyFieldIsReportedAndNotWritten) {
	const std::string path = Path();
	Write(path, kOriginal);
	Session session;
	ASSERT_EQ(session.Capture(CompiledDefaults()), Error::None);
	ASSERT_EQ(session.Edit(Parameter::FogDensity, 0.9f), Error::None);

	const auto preview = PreviewConfigWrite(path, session);
	// The only changed field has no loader key, so there is nothing to write and
	// the file must be untouched rather than carrying a key the loader ignores.
	EXPECT_EQ(preview.refusal, PersistRefusal::NothingToWrite);
	ASSERT_EQ(preview.unpublishable.size(), 1u);
	EXPECT_EQ(preview.unpublishable[0], Parameter::FogDensity);
	EXPECT_EQ(ReadFileText(path), kOriginal);
}

TEST_F(ConfigPersistence, ALegacyEditAlongsideAPublishableOneIsListedSeparately) {
	const std::string path = Path();
	Write(path, kOriginal);
	Session session;
	ASSERT_EQ(session.Capture(CompiledDefaults()), Error::None);
	ASSERT_EQ(session.Edit(Parameter::FogDensity, 0.9f), Error::None);
	ASSERT_EQ(session.Edit(Parameter::Env0Decay, 1.6f), Error::None);

	const auto preview = PreviewConfigWrite(path, session);
	ASSERT_TRUE(preview.ok()) << preview.message;
	ASSERT_EQ(preview.fields.size(), 1u);
	EXPECT_EQ(preview.fields[0].key, "env0_decay");
	ASSERT_EQ(preview.unpublishable.size(), 1u);
	EXPECT_EQ(preview.unpublishable[0], Parameter::FogDensity);
}

TEST_F(ConfigPersistence, DuplicateOwnedKeysAreReportedButTheEarlierLineIsPreserved) {
	const std::string path = Path();
	const std::string text =
		"env0_decay 1.000\r\n"
		"env0_decay 1.490\r\n";
	Write(path, text);
	auto session = SessionWithEnv0Decay(1.6f);

	const auto preview = PreviewConfigWrite(path, session);
	ASSERT_TRUE(preview.ok()) << preview.message;
	ASSERT_EQ(preview.duplicates.size(), 1u);
	EXPECT_EQ(preview.duplicates[0], "env0_decay");
	ASSERT_TRUE(ApplyConfigWrite(path, session, preview).ok());
	// The inert first line survives; only the line the loader honours moved.
	EXPECT_EQ(ReadFileText(path), "env0_decay 1.000\r\nenv0_decay 1.600\r\n");
}

TEST_F(ConfigPersistence, ASaveReportsThatTheFileAlreadyHoldsTheValues) {
	const std::string path = Path();
	Write(path, kOriginal);
	auto session = SessionWithEnv0Decay(1.6f);
	const auto preview = PreviewConfigWrite(path, session);
	ASSERT_TRUE(preview.ok()) << preview.message;
	ASSERT_TRUE(ApplyConfigWrite(path, session, preview).ok());

	// A second review must not claim there is nothing to save for the same reason
	// as "this field has no key": it must say the file already matches.
	const auto again = PreviewConfigWrite(path, session);
	EXPECT_EQ(again.refusal, PersistRefusal::NothingToWrite);
	EXPECT_NE(again.message.find("already holds"), std::string::npos) << again.message;
}

// ---------------------------------------------------------------------------
// Registry invariants. These are what make the write set trustworthy: only a
// key the loader actually reads, spelled the way the loader expects, may reach
// the file. Source checks mirror tools/c2_config_defaults.py, which validates
// the same contract for the offline tool.
// ---------------------------------------------------------------------------

namespace {
std::string ReadSource(const std::string& relative)
{
	std::ifstream input(std::string(C2_SOURCE_ROOT) + "/" + relative, std::ios::binary);
	EXPECT_TRUE(input.is_open()) << relative;
	return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

std::vector<const Descriptor*> Publishable()
{
	std::vector<const Descriptor*> found;
	for (const auto& descriptor : Descriptors())
		if (descriptor.persist == PersistPolicy::RecognizedConfigKey) found.push_back(&descriptor);
	return found;
}
} // namespace

TEST(ConfigRegistry, EveryPublishableKeyRoundTripsThroughTheLoaderParser) {
	const auto publishable = Publishable();
	ASSERT_EQ(publishable.size(), 41u) << "the write set size is part of the contract";
	for (const auto* descriptor : publishable) {
		for (const float value : {descriptor->minimum, descriptor->maximum, descriptor->compiledDefault}) {
			// The production formatter, not a copy of its rule: a precision loss
			// here is what would make a saved value drift on the next launch.
			const std::string token = FormatConfigValue(*descriptor, value);
			float parsed = 0.f;
			EXPECT_EQ(ParseValue(descriptor->id, token, parsed), Error::None)
				<< descriptor->key << " token=" << token;
			EXPECT_TRUE(SameValue(parsed, value))
				<< descriptor->key << " token=" << token << " value=" << value;
		}
	}
}

TEST(ConfigRegistry, PublishableKeysAreUniqueAndTokenSafe) {
	std::vector<std::string> keys;
	for (const auto* descriptor : Publishable()) {
		const std::string key = descriptor->key;
		ASSERT_FALSE(key.empty()) << "a recognized key needs a name";
		for (const char c : key)
			EXPECT_TRUE((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_')
				<< "unexpected character in " << key;
		EXPECT_NE(key.front(), '#') << key;
		keys.push_back(key);
	}
	const auto sorted = [&] { auto copy = keys; std::sort(copy.begin(), copy.end()); return copy; }();
	EXPECT_EQ(std::adjacent_find(sorted.begin(), sorted.end()), sorted.end())
		<< "two descriptors claim the same config key";
}

TEST(ConfigRegistry, EveryPublishableKeyIsReadByTheEngineLoader) {
	const std::string engine = ReadSource("Hunt/Game/EngineInit.cpp");
	// Env keys are read by a prefix branch that then matches the field name.
	EXPECT_NE(engine.find("_strnicmp(key, \"env\", 3)"), std::string::npos);
	for (const auto* descriptor : Publishable()) {
		const std::string key = descriptor->key;
		if (key.rfind("env", 0) == 0) {
			const std::string field = key.substr(5); // env<N>_
			EXPECT_NE(engine.find("_stricmp(field, \"" + field + "\")"), std::string::npos) << key;
			continue;
		}
		EXPECT_NE(engine.find("_stricmp(key, \"" + key + "\")"), std::string::npos)
			<< key << " is written but never read";
	}
}

TEST(ConfigRegistry, TheWorkbenchAndTheMenuNeverOwnTheSameKey) {
	// Two writers share config.cfg. If both claimed a key they would overwrite
	// each other, so the sets must stay disjoint.
	const std::string menu = ReadSource("Menu/Resources.cpp");
	const std::size_t begin = menu.find("void SaveConfig()");
	ASSERT_NE(begin, std::string::npos);
	const std::string body = menu.substr(begin);
	for (const auto* descriptor : Publishable()) {
		EXPECT_EQ(body.find("fresh.emplace_back(\"" + std::string(descriptor->key) + "\""), std::string::npos)
			<< descriptor->key << " is owned by both writers";
	}
}

TEST(ConfigRegistry, NoDescriptorWritesTheDryReverbEnvironment) {
	// Environment 7 intentionally does nothing, so the backend rejects it.
	for (const auto* descriptor : Publishable())
		EXPECT_NE(std::string(descriptor->key).rfind("env7_", 0), 0u) << descriptor->key;
}

TEST(ConfigRegistry, AChangedFieldWithoutALoaderKeyIsNeverWritten) {
	const std::string path = ::testing::TempDir() + "c2_w3_registry_probe.cfg";
	{
		std::ofstream output(path, std::ios::binary | std::ios::trunc);
		output << "renderer 1\r\n";
	}
	Session session;
	ASSERT_EQ(session.Capture(CompiledDefaults()), Error::None);
	ASSERT_EQ(session.Edit(Parameter::Env0Decay, 1.6f), Error::None);
	ASSERT_EQ(session.Edit(Parameter::FlashlightRed, 0.5f), Error::None); // RuntimeOnly

	const auto preview = PreviewConfigWrite(path, session);
	ASSERT_TRUE(preview.ok()) << preview.message;
	for (const auto& field : preview.fields) {
		const auto* descriptor = FindDescriptor(field.id);
		ASSERT_NE(descriptor, nullptr) << field.key;
		EXPECT_EQ(descriptor->persist, PersistPolicy::RecognizedConfigKey) << field.key;
	}
	std::error_code ignored;
	fs::remove(path, ignored);
}