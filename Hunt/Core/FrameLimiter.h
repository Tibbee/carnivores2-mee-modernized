#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <timeapi.h>

// Shared by the engine's frame loop and the menu's message loop: both pace
// their frames by sleeping whole milliseconds and spinning out the
// sub-millisecond remainder. The target comes from the shared FPS-limit
// option table (Core/Constants.h, kFpsLimitValues); each caller keeps one
// instance so the frame phase survives across calls.
//
// The first call also raises the process timer resolution (timeBeginPeriod),
// exactly as both inline loops did before.

namespace framelimiter {

class FrameLimiter
{
public:
    // Waits until the frame opened by the previous call has run for
    // 1/targetFps seconds. targetFps <= 0 (unlimited) returns immediately;
    // the frame phase still advances so a later limit change is paced
    // from the current frame.
    void WaitUntilFrameEnd(int targetFps)
    {
        if (!m_initialized) {
            QueryPerformanceFrequency(&m_frequency);
            QueryPerformanceCounter(&m_frameStart);
            timeBeginPeriod(1);
            m_initialized = true;
        }

        if (targetFps > 0) {
            const INT64 targetUs = 1000000 / targetFps;

            LARGE_INTEGER now;
            QueryPerformanceCounter(&now);
            INT64 elapsedUs = MicrosecondsSince(now);

            while (elapsedUs < targetUs) {
                if (targetUs - elapsedUs > 2000)
                    Sleep(1);
                QueryPerformanceCounter(&now);
                elapsedUs = MicrosecondsSince(now);
            }
        }

        QueryPerformanceCounter(&m_frameStart);
    }

private:
    INT64 MicrosecondsSince(const LARGE_INTEGER& now) const
    {
        return (now.QuadPart - m_frameStart.QuadPart) * 1000000 / m_frequency.QuadPart;
    }

    LARGE_INTEGER m_frequency{};
    LARGE_INTEGER m_frameStart{};
    bool m_initialized = false;
};

} // namespace framelimiter
