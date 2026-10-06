#include "InputBoundary.h"

namespace workbench {
void InputBoundary::Fence(const KeySnapshot& keys) {
	for (std::size_t i = 0; i < keys.size(); ++i) if (keys[i] & 128) m_blocked.set(i);
	m_lookFence = true;
}
bool InputBoundary::SetOpen(bool open, const KeySnapshot& keys) {
	if (open == m_open || (open && !m_available)) return false;
	Fence(keys);
	m_open = open;
	m_mode = InputMode::UI;
	return true;
}
bool InputBoundary::SetMode(InputMode mode, const KeySnapshot& keys) {
	if (!m_open || !m_available || !m_focused || mode == m_mode) return false;
	Fence(keys);
	m_mode = mode;
	return true;
}
bool InputBoundary::SetFocused(bool focused, const KeySnapshot& keys) {
	if (focused == m_focused) return false;
	Fence(keys);
	m_focused = focused;
	if (!focused) m_mode = InputMode::UI; // Never resume Test silently after Alt-Tab.
	return true;
}
bool InputBoundary::SetAvailable(bool available, const KeySnapshot& keys) {
	if (available == m_available) return false;
	m_available = available;
	if (m_open) { Fence(keys); m_mode = InputMode::UI; }
	return true;
}
bool InputBoundary::ConsumeKey(unsigned int key, bool down) {
	if (key >= m_blocked.size()) return OwnsInput();
	const bool consume = OwnsInput() || m_blocked.test(key);
	if (!down) m_blocked.reset(key);
	else if (OwnsInput()) m_blocked.set(key);
	return consume;
}
void InputBoundary::FilterKeys(std::uint8_t* keys) {
	if (!OwnsInput() && m_blocked.none()) return; // Closed-state fast path.
	for (std::size_t i = 0; i < m_blocked.size(); ++i) {
		const bool down = (keys[i] & 128) != 0;
		if (OwnsInput() && down) m_blocked.set(i);
		const bool suppress = OwnsInput() || m_blocked.test(i);
		if (!down) m_blocked.reset(i);
		if (suppress) keys[i] &= 127; // Preserve unrelated toggle bits, e.g. Caps Lock.
	}
}
} // namespace workbench
