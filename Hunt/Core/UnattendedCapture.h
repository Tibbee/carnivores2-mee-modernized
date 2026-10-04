#pragma once

#include "CommandLineParse.h"
#include <cstdint>

namespace unattended_capture {
inline constexpr int kCaptureFrames = 120;
// Parsed BEFORE window creation, not just during InitEngine: no loading
// window, display-mode switch, focus grab or cursor capture may precede it.
struct Options {
    bool enabled = false;
    int warmupMs = 3000;
    char output[260] = {};
};
inline Options options{};

inline bool parse(int argc, char* const* argv, Options& out) {
    Options parsed;
    bool hasCaptureOption = false;
    for (int i = 1; i < argc; ++i) {
        const char* arg = argv[i];
        const char* value = nullptr;
        if (_stricmp(arg, "-capture-background") == 0) {
            parsed.enabled = true;
        } else if (CommandLineOptionValue(arg, "-capture-warmup-ms=", &value)) {
            hasCaptureOption = true;
            if (!ParseCommandLineInt(value, parsed.warmupMs) ||
                parsed.warmupMs < 0 || parsed.warmupMs > 120000) return false;
        } else if (CommandLineOptionValue(arg, "-capture-output=", &value)) {
            hasCaptureOption = true;
            if (CopyCommandLineOption(parsed.output, sizeof(parsed.output), arg,
                                      "-capture-output=") != CommandLineCopyResult::Copied) return false;
        }
    }
    if ((hasCaptureOption && !parsed.enabled) ||
        (parsed.enabled && parsed.output[0] == '\0')) return false;
    out = parsed;
    return true;
}

// Pure one-shot scheduling, shared with tests. Warm-up starts at the first
// rendered frame, not process launch/loading. A clock discontinuity cannot
// trigger a capture early through unsigned underflow.
struct Schedule {
    bool started = false;
    bool triggered = false;
    std::uint64_t firstFrameMs = 0;
    bool shouldTrigger(std::uint64_t nowMs, int warmupMs) {
        if (!started) { started = true; firstFrameMs = nowMs; }
        if (!triggered && nowMs >= firstFrameMs &&
            nowMs - firstFrameMs >= static_cast<std::uint64_t>(warmupMs)) {
            triggered = true;
            return true;
        }
        return false;
    }
};
} // namespace unattended_capture
