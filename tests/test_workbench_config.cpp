#include <gtest/gtest.h>
#include "Core/ConfigMerge.h"

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
