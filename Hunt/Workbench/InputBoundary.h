#pragma once
#include <array>
#include <bitset>
#include <cstdint>

namespace workbench {
using KeySnapshot = std::array<std::uint8_t, 256>;
enum class InputMode { UI, Test };

// Independent of ImGui capture flags, Win32, game bindings and GameMode.
// Every ownership/focus transition quarantines held keys (including mouse
// buttons). A release must be observed before gameplay can use them again.
class InputBoundary {
public:
	bool IsOpen() const { return m_open; }
	bool IsFocused() const { return m_focused; }
	bool IsAvailable() const { return m_available; }
	InputMode Mode() const { return m_mode; }
	bool OwnsInput() const { return !m_focused || (m_open && (m_mode == InputMode::UI || !m_available)); }
	bool BlocksLook() const { return OwnsInput() || m_lookFence; }
	bool SetOpen(bool open, const KeySnapshot& keys);
	bool SetMode(InputMode mode, const KeySnapshot& keys);
	bool SetFocused(bool focused, const KeySnapshot& keys);
	bool SetAvailable(bool available, const KeySnapshot& keys);
	bool ConsumeKey(unsigned int key, bool down);
	void FilterKeys(std::uint8_t* keys); // Exactly 256 Win32-style bytes.
	void FinishInputFrame() { m_lookFence = false; }
private:
	void Fence(const KeySnapshot& keys);
	bool m_open = false, m_available = false, m_focused = true, m_lookFence = false;
	InputMode m_mode = InputMode::UI;
	std::bitset<256> m_blocked;
};
} // namespace workbench
