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
TEST(WorkbenchInput, ClosedOrUnavailableBoundaryIsTransparentAndSafe) {
	InputBoundary boundary;
	KeySnapshot keys{}; keys.fill(129);
	const auto reference = keys;
	boundary.FilterKeys(keys.data()); EXPECT_EQ(keys, reference);
	EXPECT_FALSE(boundary.OwnsInput()); EXPECT_FALSE(boundary.BlocksLook());
	EXPECT_FALSE(boundary.ConsumeKey(65, true));
	boundary.SetAvailable(true, {});
	boundary.FilterKeys(keys.data()); EXPECT_EQ(keys, reference);

	// A failed backend can always close to restore gameplay.
	boundary = Open();
	boundary.SetAvailable(false, {});
	EXPECT_TRUE(boundary.SetOpen(false, {})); EXPECT_FALSE(boundary.OwnsInput());

	// Closed mode switches and invalid key codes are safe.
	boundary = InputBoundary();
	EXPECT_FALSE(boundary.SetMode(InputMode::Test, {}));
	EXPECT_FALSE(boundary.ConsumeKey(256, true));
	EXPECT_FALSE(boundary.ConsumeKey(~0u, false));
	boundary = Open(); EXPECT_TRUE(boundary.ConsumeKey(~0u, true));
}
TEST(WorkbenchInput, BackendAvailabilityGatesAndSuspendsTest) {
	InputBoundary boundary;
	EXPECT_FALSE(boundary.SetOpen(true, {}));
	EXPECT_FALSE(boundary.IsOpen());
	boundary.SetAvailable(true, {});
	EXPECT_TRUE(boundary.SetOpen(true, {}));
	EXPECT_FALSE(boundary.SetOpen(true, {}));

	// An open UI blocks every binding and retains the unrelated toggle bits.
	KeySnapshot keys{}; keys.fill(129);
	boundary.FilterKeys(keys.data());
	for (auto key : keys) EXPECT_EQ(key, 1);
	for (unsigned key = 0; key < 256; ++key) EXPECT_TRUE(boundary.ConsumeKey(key, true));
	EXPECT_TRUE(boundary.OwnsInput()); EXPECT_TRUE(boundary.BlocksLook());

	// Losing the backend while in Test returns to UI and keeps ownership; the
	// mode cannot resume silently when the backend comes back.
	boundary.SetMode(InputMode::Test, {});
	boundary.SetAvailable(false, Down({1}));
	EXPECT_TRUE(boundary.IsOpen()); EXPECT_TRUE(boundary.OwnsInput()); EXPECT_EQ(boundary.Mode(), InputMode::UI);
	boundary.SetAvailable(true, Down({1})); EXPECT_TRUE(boundary.OwnsInput());
}
TEST(WorkbenchInput, ClosingFencesHeldKeysAndTheFirstGameplayFrame) {
	auto boundary = Open();
	boundary.SetOpen(false, Down({1, 2, 87, 13})); // Mouse buttons, movement, trophy Enter.
	auto keys = Down({1, 2, 87, 13}); boundary.FilterKeys(keys.data());
	EXPECT_EQ(keys, KeySnapshot{});
	EXPECT_TRUE(boundary.ConsumeKey(87, true)); // Windows repeat is also fenced.
	keys = {}; boundary.FilterKeys(keys.data());
	keys = Down({1, 87}); boundary.FilterKeys(keys.data());
	EXPECT_EQ(keys[1], 128); EXPECT_EQ(keys[87], 128);

	// The first gameplay look frame stays fenced after every transition until
	// it is finished once.
	auto transitioned = Open(); transitioned.FinishInputFrame(); EXPECT_TRUE(transitioned.BlocksLook());
	transitioned.SetOpen(false, {}); EXPECT_TRUE(transitioned.BlocksLook());
	transitioned.FinishInputFrame(); EXPECT_FALSE(transitioned.BlocksLook());
}
TEST(WorkbenchInput, TransitionPressesStayQuarantinedUntilRelease) {
	// A queued UI key press is quarantined even when the close carried no
	// transition snapshot: its repeat and its release are consumed first.
	auto boundary = Open();
	boundary.ConsumeKey(67, true);
	boundary.SetOpen(false, {});
	EXPECT_TRUE(boundary.ConsumeKey(67, true));
	EXPECT_TRUE(boundary.ConsumeKey(67, false));
	EXPECT_FALSE(boundary.ConsumeKey(67, true));

	// A poll-only press (seen only through FilterKeys) is quarantined too.
	auto polled = Open(); auto keys = Down({88});
	polled.FilterKeys(keys.data()); EXPECT_EQ(keys[88], 0);
	polled.SetOpen(false, {});
	keys = Down({88}); polled.FilterKeys(keys.data()); EXPECT_EQ(keys[88], 0);
}
TEST(WorkbenchInput, SwitchingToTestNeutralizesHeldKeysUntilRelease) {
	auto boundary = Open(); boundary.SetMode(InputMode::Test, Down({1, 87, 119}));
	EXPECT_FALSE(boundary.OwnsInput());
	auto keys = Down({1, 87, 119, 83}); boundary.FilterKeys(keys.data());
	EXPECT_EQ(keys[1], 0); EXPECT_EQ(keys[87], 0); EXPECT_EQ(keys[119], 0); EXPECT_EQ(keys[83], 128);
	EXPECT_TRUE(boundary.BlocksLook()); boundary.FinishInputFrame(); EXPECT_FALSE(boundary.BlocksLook());

	// Bouncing Test -> UI -> closed must not re-arm a key that never saw its
	// release.
	auto bounce = Open();
	bounce.SetMode(InputMode::Test, Down({1}));
	bounce.SetMode(InputMode::UI, Down({1}));
	bounce.SetOpen(false, Down({1}));
	EXPECT_TRUE(bounce.ConsumeKey(1, true));
}
TEST(WorkbenchInput, FocusLossSuspendsTestAndQuarantinesHeldInputs) {
	auto boundary = Open(); boundary.SetMode(InputMode::Test, {});
	boundary.SetFocused(false, Down({1, 87}));
	EXPECT_TRUE(boundary.IsOpen()); EXPECT_EQ(boundary.Mode(), InputMode::UI);
	EXPECT_TRUE(boundary.OwnsInput());
	EXPECT_FALSE(boundary.SetMode(InputMode::Test, {}));
	boundary.SetFocused(true, Down({1, 87})); EXPECT_TRUE(boundary.OwnsInput());
	boundary.SetMode(InputMode::Test, Down({1, 87}));
	auto keys = Down({1, 87}); boundary.FilterKeys(keys.data()); EXPECT_EQ(keys, KeySnapshot{});

	// A closed boundary that loses focus still owns input; the held key stays
	// quarantined across the refocus until its release is seen.
	InputBoundary closed; closed.SetFocused(false, Down({87}));
	EXPECT_TRUE(closed.OwnsInput()); closed.SetFocused(true, Down({87}));
	EXPECT_FALSE(closed.OwnsInput()); EXPECT_TRUE(closed.ConsumeKey(87, true));
}
