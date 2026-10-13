#include <gtest/gtest.h>
#include "Workbench/InputBoundary.h"
using namespace workbench;
namespace {
KeySnapshot Down(std::initializer_list<unsigned> keys) {
	KeySnapshot state{};
	for (auto key : keys) state[key] = 128;
	return state;
}
InputBoundary Open() {
	InputBoundary boundary;
	boundary.SetAvailable(true, {});
	boundary.SetOpen(true, {});
	return boundary;
}
}
TEST(WorkbenchInput, ClosedAvailableOrUnavailableIsTransparent) {
	InputBoundary boundary;
	KeySnapshot keys{}; keys.fill(129);
	const auto reference = keys;
	boundary.FilterKeys(keys.data()); EXPECT_EQ(keys, reference);
	EXPECT_FALSE(boundary.OwnsInput()); EXPECT_FALSE(boundary.BlocksLook());
	EXPECT_FALSE(boundary.ConsumeKey(65, true));
	boundary.SetAvailable(true, {});
	boundary.FilterKeys(keys.data()); EXPECT_EQ(keys, reference);
}
TEST(WorkbenchInput, CannotOpenBeforeBackendReady) {
	InputBoundary boundary;
	EXPECT_FALSE(boundary.SetOpen(true, {}));
	EXPECT_FALSE(boundary.IsOpen());
	boundary.SetAvailable(true, {});
	EXPECT_TRUE(boundary.SetOpen(true, {}));
	EXPECT_FALSE(boundary.SetOpen(true, {}));
}
TEST(WorkbenchInput, UIBlocksEveryBindingAndRetainsToggleBits) {
	auto boundary = Open();
	KeySnapshot keys{}; keys.fill(129);
	boundary.FilterKeys(keys.data());
	for (auto key : keys) EXPECT_EQ(key, 1);
	for (unsigned key = 0; key < 256; ++key) EXPECT_TRUE(boundary.ConsumeKey(key, true));
	EXPECT_TRUE(boundary.OwnsInput()); EXPECT_TRUE(boundary.BlocksLook());
}
TEST(WorkbenchInput, ClosingCannotFireOrMoveWithAHeldInput) {
	auto boundary = Open();
	boundary.SetOpen(false, Down({1, 2, 87, 13})); // Mouse buttons, movement, trophy Enter.
	auto keys = Down({1, 2, 87, 13}); boundary.FilterKeys(keys.data());
	EXPECT_EQ(keys, KeySnapshot{});
	EXPECT_TRUE(boundary.ConsumeKey(87, true)); // Windows repeat is also fenced.
	keys = {}; boundary.FilterKeys(keys.data());
	keys = Down({1, 87}); boundary.FilterKeys(keys.data());
	EXPECT_EQ(keys[1], 128); EXPECT_EQ(keys[87], 128);
}
TEST(WorkbenchInput, UIKeyPressesAreQuarantinedEvenWithoutATransitionSnapshot) {
	auto boundary = Open();
	boundary.ConsumeKey(67, true);
	boundary.SetOpen(false, {});
	EXPECT_TRUE(boundary.ConsumeKey(67, true));
	EXPECT_TRUE(boundary.ConsumeKey(67, false));
	EXPECT_FALSE(boundary.ConsumeKey(67, true));
}
TEST(WorkbenchInput, PollOnlyPressesAreAlsoQuarantined) {
	auto boundary = Open(); auto keys = Down({88});
	boundary.FilterKeys(keys.data()); EXPECT_EQ(keys[88], 0);
	boundary.SetOpen(false, {});
	keys = Down({88}); boundary.FilterKeys(keys.data()); EXPECT_EQ(keys[88], 0);
}
TEST(WorkbenchInput, UIToTestIsNeutralUntilReleaseButFreshBindingsWork) {
	auto boundary = Open(); boundary.SetMode(InputMode::Test, Down({1, 87, 119}));
	EXPECT_FALSE(boundary.OwnsInput());
	auto keys = Down({1, 87, 119, 83}); boundary.FilterKeys(keys.data());
	EXPECT_EQ(keys[1], 0); EXPECT_EQ(keys[87], 0); EXPECT_EQ(keys[119], 0); EXPECT_EQ(keys[83], 128);
	EXPECT_TRUE(boundary.BlocksLook()); boundary.FinishInputFrame(); EXPECT_FALSE(boundary.BlocksLook());
}
TEST(WorkbenchInput, UIToTestToUIToClosedDoesNotRearmHeldKeys) {
	auto boundary = Open();
	boundary.SetMode(InputMode::Test, Down({1}));
	boundary.SetMode(InputMode::UI, Down({1}));
	boundary.SetOpen(false, Down({1}));
	EXPECT_TRUE(boundary.ConsumeKey(1, true));
}
TEST(WorkbenchInput, FirstGameplayLookFrameIsFencedAfterEveryTransition) {
	auto boundary = Open(); boundary.FinishInputFrame(); EXPECT_TRUE(boundary.BlocksLook());
	boundary.SetOpen(false, {}); EXPECT_TRUE(boundary.BlocksLook());
	boundary.FinishInputFrame(); EXPECT_FALSE(boundary.BlocksLook());
}
TEST(WorkbenchInput, FocusLossSuspendsTestAndReturnsToUIWithoutChangingVisibility) {
	auto boundary = Open(); boundary.SetMode(InputMode::Test, {});
	boundary.SetFocused(false, Down({1, 87}));
	EXPECT_TRUE(boundary.IsOpen()); EXPECT_EQ(boundary.Mode(), InputMode::UI);
	EXPECT_TRUE(boundary.OwnsInput());
	EXPECT_FALSE(boundary.SetMode(InputMode::Test, {}));
	boundary.SetFocused(true, Down({1, 87})); EXPECT_TRUE(boundary.OwnsInput());
	boundary.SetMode(InputMode::Test, Down({1, 87}));
	auto keys = Down({1, 87}); boundary.FilterKeys(keys.data()); EXPECT_EQ(keys, KeySnapshot{});
}
TEST(WorkbenchInput, FocusLossWhileClosedStillQuarantinesHeldInputs) {
	InputBoundary boundary; boundary.SetFocused(false, Down({87}));
	EXPECT_TRUE(boundary.OwnsInput()); boundary.SetFocused(true, Down({87}));
	EXPECT_FALSE(boundary.OwnsInput()); EXPECT_TRUE(boundary.ConsumeKey(87, true));
}
TEST(WorkbenchInput, RendererRecreationCannotResumeTestSilently) {
	auto boundary = Open(); boundary.SetMode(InputMode::Test, {});
	boundary.SetAvailable(false, Down({1}));
	EXPECT_TRUE(boundary.IsOpen()); EXPECT_TRUE(boundary.OwnsInput()); EXPECT_EQ(boundary.Mode(), InputMode::UI);
	boundary.SetAvailable(true, Down({1})); EXPECT_TRUE(boundary.OwnsInput());
}
TEST(WorkbenchInput, FailedBackendCanAlwaysCloseToRestoreGameplay) {
	auto boundary = Open(); boundary.SetAvailable(false, {});
	EXPECT_TRUE(boundary.SetOpen(false, {})); EXPECT_FALSE(boundary.OwnsInput());
}
TEST(WorkbenchInput, ClosedModeSwitchesAndInvalidKeysAreSafe) {
	InputBoundary boundary;
	EXPECT_FALSE(boundary.SetMode(InputMode::Test, {}));
	EXPECT_FALSE(boundary.ConsumeKey(256, true));
	EXPECT_FALSE(boundary.ConsumeKey(~0u, false));
	boundary = Open(); EXPECT_TRUE(boundary.ConsumeKey(~0u, true));
}
