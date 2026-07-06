// ==========================================================================
// GLPerf.cpp — Phase 0.1 in-engine GL perf harness
//
// All counters, CPU scope timers, GPU timer queries, rolling 1s log dump,
// and F11-triggered per-frame CSV capture live here. The public surface is
// the C-linkage functions and macros in GLPerf.h.
//
// State is single-instance; the renderer is single-threaded so no atomics
// are required.
// ==========================================================================

#include "GLPerf.h"

#ifndef GL_PERF_HOOKS
// ---------------------------------------------------------------------------
// Release stubs. All hooks are no-ops when GL_PERF_HOOKS is not defined.
// We still need symbol definitions for the C-linkage surface so that any
// guard-less call site (e.g. glperf_is_active() in shared code) links.
// ---------------------------------------------------------------------------

extern "C" bool glperf_is_active()     { return false; }
extern "C" void glperf_trigger_capture() {}
extern "C" void glperf_init()          {}
extern "C" void glperf_shutdown()      {}

#else  // GL_PERF_HOOKS

#include "glad/glad.h"

#include <windows.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

namespace {

constexpr int   kMaxPasses          = 16;
constexpr int   kMaxCaptureFrames   = 120;   // ~2 seconds at 60 fps
constexpr int   kMaxTextureHistory  = 256;   // ring of last-N distinct handles
constexpr float kLogIntervalSeconds = 1.0f;  // rolling dump cadence

// File-naming helpers. One log per game session (timestamp at glperf_init),
// one CSV per F11 press (timestamp at the trigger). This avoids the old
// "glperf-frame.csv overwritten on every capture" anti-pattern where you
// could not keep two captures side by side.

// Formats a time_t into "YYYY-MM-DD-HHMMSS" in out. Returns out.
char* MakeTimestamp(char* out, size_t outSize, std::time_t t) {
    struct tm tm_buf {};
#ifdef _WIN32
    localtime_s(&tm_buf, &t);
#else
    localtime_r(&t, &tm_buf);
#endif
    std::strftime(out, outSize, "%Y-%m-%d-%H%M%S", &tm_buf);
    return out;
}

constexpr size_t kTimestampLen = 32;
constexpr size_t kFilenameLen  = 96;

void MakeLogFilename(char* out, size_t outSize, const char* timestamp) {
    std::snprintf(out, outSize, "glperf-%s.log", timestamp);
}

void MakeCaptureFilename(char* out, size_t outSize, const char* timestamp) {
    std::snprintf(out, outSize, "glperf-capture-%s.csv", timestamp);
}

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------

struct PassTimings {
    static constexpr int kQueryRingSize = 4;
    const char* name       = nullptr;
    GLuint      queries[kQueryRingSize] = {};
    int         currentQuerySlot = -1;   // most recently begun slot; -1 = none
    bool        slotInFlight[kQueryRingSize] = {};
    double      cpuMsSum   = 0.0;
    double      gpuMsSum   = 0.0;
    uint32_t    cpuSamples = 0;
    uint32_t    gpuSamples = 0;
    bool        queryAvailable = false;
};

struct GLPerfState {
    // Lifecycle
    bool initialized = false;
    bool gpuTimersOk = false;

    // Per-frame counters
    uint32_t frameDrawCalls        = 0;
    uint32_t frameTriangles        = 0;
    uint32_t frameStateChanges     = 0;
    uint32_t frameTextureBinds     = 0;
    uint32_t frameDistinctTextures = 0;
    uint32_t frameLastTexture      = 0;
    bool     frameHasLastTexture   = false;

    // Rolling sums (for 1s average)
    uint32_t rollingFrames         = 0;
    double   rollingCpuMsSum       = 0.0;
    double   rollingGpuMsSum       = 0.0;
    uint64_t rollingDrawCalls      = 0;
    uint64_t rollingTriangles      = 0;
    uint64_t rollingStateChanges   = 0;
    uint64_t rollingTextureBinds   = 0;
    uint64_t rollingDistinctTex    = 0;

    // Scope stack (for proper nesting instead of the old "close prev" hack)
    static constexpr int kMaxNestDepth = 8;
    struct ScopeFrame {
        int passIndex = -1;
        std::chrono::steady_clock::time_point cpuStart;
    };
    ScopeFrame scopeStack[kMaxNestDepth] = {};
    int scopeStackDepth = 0;

    // CPU frame timing
    std::chrono::steady_clock::time_point frameStart;
    double   frameCpuMs                 = 0.0;
    double   frameGpuMs                 = 0.0;

    // Pass table (populated lazily as scopes are entered)
    std::array<PassTimings, kMaxPasses> passes{};
    int passCount = 0;

    // Log
    std::chrono::steady_clock::time_point lastFlush;
    bool logOpened = false;
    FILE* logFile = nullptr;
    char logTimestamp[kTimestampLen] = {};     // set at glperf_init, used for filename
    char logFilename[kFilenameLen]  = {};      // cached "glperf-<ts>.log"

    // F11 capture
    bool captureActive = false;
    int  captureFramesRemaining = 0;
    int  captureFramesWritten   = 0;
    bool captureHeaderWritten   = false;
    FILE* captureFile = nullptr;
    char captureTimestamp[kTimestampLen] = {}; // refreshed per F11 press
    std::vector<std::string> captureScopeOrder;
};

GLPerfState g_state;

PassTimings* findOrCreatePass(const char* name) {
    for (int i = 0; i < g_state.passCount; ++i) {
        if (std::strcmp(g_state.passes[i].name, name) == 0) {
            return &g_state.passes[i];
        }
    }
    if (g_state.passCount >= kMaxPasses) {
        return nullptr;  // table full; drop this scope's timings
    }
    PassTimings* p = &g_state.passes[g_state.passCount++];
    p->name = name;
    if (g_state.gpuTimersOk) {
        glGenQueries(PassTimings::kQueryRingSize, p->queries);
    }
    return p;
}

void appendCaptureHeader() {
    if (!g_state.captureFile) return;
    std::fprintf(g_state.captureFile,
        "frame,wallclock_ms,cpu_ms,gpu_ms,draw_calls,triangles,state_changes,texture_binds,distinct_textures");
    for (const std::string& name : g_state.captureScopeOrder) {
        std::fprintf(g_state.captureFile, ",%s_cpu_ms,%s_gpu_ms", name.c_str(), name.c_str());
    }
    std::fputc('\n', g_state.captureFile);
    g_state.captureHeaderWritten = true;
}

void startCapture() {
    if (g_state.captureActive) return;
    g_state.captureActive = true;
    g_state.captureFramesRemaining = kMaxCaptureFrames;
    g_state.captureFramesWritten = 0;
    g_state.captureHeaderWritten = false;
    if (g_state.captureFile) {
        std::fclose(g_state.captureFile);
        g_state.captureFile = nullptr;
    }

    // Refresh the capture timestamp so multiple F11 presses in the same
    // session each get their own file. If two captures happen within the
    // same second, the second overwrites the first -- rare in practice
    // (the user has to press F11 twice in <1s).
    char captureFilename[kFilenameLen] = {};
    MakeTimestamp(g_state.captureTimestamp, sizeof(g_state.captureTimestamp), std::time(nullptr));
    MakeCaptureFilename(captureFilename, sizeof(captureFilename), g_state.captureTimestamp);

    g_state.captureFile = std::fopen(captureFilename, "w");
    if (g_state.captureFile) {
        // Seed header with the scope names we know about right now.
        g_state.captureScopeOrder.clear();
        for (int i = 0; i < g_state.passCount; ++i) {
            g_state.captureScopeOrder.emplace_back(g_state.passes[i].name);
        }
        appendCaptureHeader();
    }
}

void stopCapture() {
    if (!g_state.captureActive) return;
    g_state.captureActive = false;
    g_state.captureFramesRemaining = 0;
    if (g_state.captureFile) {
        std::fclose(g_state.captureFile);
        g_state.captureFile = nullptr;
    }
}

void writeCaptureFrame() {
    if (!g_state.captureActive || !g_state.captureFile) return;
    if (!g_state.captureHeaderWritten) {
        appendCaptureHeader();
    }
    std::fprintf(g_state.captureFile, "%d,%.3f,%.3f,%.3f,%u,%u,%u,%u,%u",
                 g_state.captureFramesWritten,
                 g_state.frameCpuMs + g_state.frameGpuMs,  // wallclock = cpu + gpu overlapped time approximation
                 g_state.frameCpuMs,
                 g_state.frameGpuMs,
                 g_state.frameDrawCalls,
                 g_state.frameTriangles,
                 g_state.frameStateChanges,
                 g_state.frameTextureBinds,
                 g_state.frameDistinctTextures);
    // Append per-scope rows in the order they were first seen
    for (int i = 0; i < g_state.passCount; ++i) {
        const PassTimings& p = g_state.passes[i];
        const double cpuAvg = p.cpuSamples > 0 ? p.cpuMsSum / p.cpuSamples : 0.0;
        const double gpuAvg = (p.gpuSamples > 0 && p.queryAvailable) ? p.gpuMsSum / p.gpuSamples : -1.0;
        std::fprintf(g_state.captureFile, ",%.3f,%.3f", cpuAvg, gpuAvg);
    }
    std::fputc('\n', g_state.captureFile);
    std::fflush(g_state.captureFile);
    ++g_state.captureFramesWritten;
}

void flushRollingLog() {
    if (!g_state.initialized) return;
    if (!g_state.logFile) {
        g_state.logFile = std::fopen(g_state.logFilename, "a");
        g_state.logOpened = (g_state.logFile != nullptr);
        if (!g_state.logFile) return;
    }

    const double avgCpu = g_state.rollingFrames > 0 ? g_state.rollingCpuMsSum / g_state.rollingFrames : 0.0;
    const double avgGpu = g_state.rollingFrames > 0 ? g_state.rollingGpuMsSum / g_state.rollingFrames : 0.0;

    // Timestamp
    std::time_t t = std::time(nullptr);
    struct tm tm_buf {};
#ifdef _WIN32
    localtime_s(&tm_buf, &t);
#else
    localtime_r(&t, &tm_buf);
#endif
    char ts[32];
    std::strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &tm_buf);

    std::fprintf(g_state.logFile, "[%s] frames=%u avg_cpu=%.2fms avg_gpu=%.2fms draws=%llu tris=%llu states=%llu binds=%llu distinct=%llu\n",
                 ts,
                 g_state.rollingFrames,
                 avgCpu, avgGpu,
                 static_cast<unsigned long long>(g_state.rollingDrawCalls),
                 static_cast<unsigned long long>(g_state.rollingTriangles),
                 static_cast<unsigned long long>(g_state.rollingStateChanges),
                 static_cast<unsigned long long>(g_state.rollingTextureBinds),
                 static_cast<unsigned long long>(g_state.rollingDistinctTex));

    for (int i = 0; i < g_state.passCount; ++i) {
        const PassTimings& p = g_state.passes[i];
        const double cpuAvg = p.cpuSamples > 0 ? p.cpuMsSum / p.cpuSamples : 0.0;
        if (p.queryAvailable) {
            const double gpuAvg = p.gpuSamples > 0 ? p.gpuMsSum / p.gpuSamples : 0.0;
            std::fprintf(g_state.logFile, "  %-22s cpu=%.2fms gpu=%.2fms (n=%u)\n",
                         p.name, cpuAvg, gpuAvg, p.cpuSamples);
        } else {
            std::fprintf(g_state.logFile, "  %-22s cpu=%.2fms gpu=n/a        (n=%u)\n",
                         p.name, cpuAvg, p.cpuSamples);
        }
    }
    std::fputc('\n', g_state.logFile);
    std::fflush(g_state.logFile);

    // Reset rolling sums and per-pass accumulators
    g_state.rollingFrames       = 0;
    g_state.rollingCpuMsSum     = 0.0;
    g_state.rollingGpuMsSum     = 0.0;
    g_state.rollingDrawCalls    = 0;
    g_state.rollingTriangles    = 0;
    g_state.rollingStateChanges = 0;
    g_state.rollingTextureBinds = 0;
    g_state.rollingDistinctTex  = 0;
    for (int i = 0; i < g_state.passCount; ++i) {
        g_state.passes[i].cpuMsSum   = 0.0;
        g_state.passes[i].gpuMsSum   = 0.0;
        g_state.passes[i].cpuSamples = 0;
        g_state.passes[i].gpuSamples = 0;
    }
}

void resolveGpuQueries() {
    if (!g_state.gpuTimersOk) return;
    for (int i = 0; i < g_state.passCount; ++i) {
        PassTimings& p = g_state.passes[i];
        for (int slot = 0; slot < PassTimings::kQueryRingSize; ++slot) {
            if (!p.slotInFlight[slot]) continue;
            GLuint ready = 0;
            glGetQueryObjectuiv(p.queries[slot], GL_QUERY_RESULT_AVAILABLE, &ready);
            if (!ready) continue;
            GLuint64 nanos = 0;
            glGetQueryObjectui64v(p.queries[slot], GL_QUERY_RESULT, &nanos);
            const double ms = static_cast<double>(nanos) / 1.0e6;
            p.gpuMsSum   += ms;
            p.gpuSamples += 1;
            p.slotInFlight[slot] = false;
            p.queryAvailable = true;
            g_state.rollingGpuMsSum += ms;
        }
    }
}

}  // namespace

// ---------------------------------------------------------------------------
// Public C-linkage surface
// ---------------------------------------------------------------------------

extern "C" bool glperf_is_active() {
    return g_state.initialized;
}

extern "C" void glperf_trigger_capture() {
    if (!g_state.initialized) return;
    startCapture();
}

extern "C" void glperf_frame_begin() {
    if (!g_state.initialized) return;
    g_state.frameStart        = std::chrono::steady_clock::now();
    g_state.frameCpuMs        = 0.0;
    g_state.frameGpuMs        = 0.0;
    g_state.frameDrawCalls    = 0;
    g_state.frameTriangles    = 0;
    g_state.frameStateChanges = 0;
    g_state.frameTextureBinds = 0;
    g_state.frameDistinctTextures = 0;
    g_state.frameHasLastTexture   = false;
    g_state.frameLastTexture      = 0;
    g_state.scopeStackDepth   = 0;
}

extern "C" void glperf_frame_end() {
    if (!g_state.initialized) return;

    // Close any scopes still open (defensive — should not happen).
    while (g_state.scopeStackDepth > 0) {
        const int idx = g_state.scopeStackDepth - 1;
        const auto& frame = g_state.scopeStack[idx];
        PassTimings* p = &g_state.passes[frame.passIndex];
        const auto now = std::chrono::steady_clock::now();
        const double ms = std::chrono::duration<double, std::milli>(now - frame.cpuStart).count();
        p->cpuMsSum   += ms;
        p->cpuSamples += 1;
        g_state.rollingCpuMsSum += ms;
        --g_state.scopeStackDepth;
    }

    // Frame CPU time
    const auto now = std::chrono::steady_clock::now();
    g_state.frameCpuMs = std::chrono::duration<double, std::milli>(now - g_state.frameStart).count();

    // Resolve any GPU queries that completed (best-effort, non-blocking).
    resolveGpuQueries();

    // Frame GPU time is the sum of average per-scope GPU times since last flush.
    double gpuSum = 0.0;
    for (int i = 0; i < g_state.passCount; ++i) {
        const PassTimings& p = g_state.passes[i];
        if (p.gpuSamples > 0 && p.queryAvailable) {
            gpuSum += p.gpuMsSum / p.gpuSamples;
        }
    }
    g_state.frameGpuMs = gpuSum;

    // Accumulate rolling
    ++g_state.rollingFrames;
    g_state.rollingCpuMsSum     += g_state.frameCpuMs;
    g_state.rollingDrawCalls    += g_state.frameDrawCalls;
    g_state.rollingTriangles    += g_state.frameTriangles;
    g_state.rollingStateChanges += g_state.frameStateChanges;
    g_state.rollingTextureBinds += g_state.frameTextureBinds;
    g_state.rollingDistinctTex  += g_state.frameDistinctTextures;

    // F11 capture: write this frame's row, then advance
    if (g_state.captureActive) {
        writeCaptureFrame();
        if (g_state.captureFramesRemaining > 0) {
            --g_state.captureFramesRemaining;
        }
        if (g_state.captureFramesRemaining == 0) {
            stopCapture();
        }
    }

    // Flush rolling log once per second
    const auto sinceFlush = std::chrono::duration<float>(now - g_state.lastFlush).count();
    if (sinceFlush >= kLogIntervalSeconds) {
        flushRollingLog();
        g_state.lastFlush = now;
    }
}

extern "C" void glperf_scope_enter(const char* name) {
    if (!g_state.initialized || !name) return;

    if (g_state.scopeStackDepth >= GLPerfState::kMaxNestDepth) {
        return;  // stack full; drop this scope
    }

    PassTimings* p = findOrCreatePass(name);
    if (!p) {
        return;
    }

    // If there's already a scope active, end its GPU query before starting
    // the new one (GL_TIME_ELAPSED does not support nesting).
    if (g_state.scopeStackDepth > 0) {
        const auto& prevFrame = g_state.scopeStack[g_state.scopeStackDepth - 1];
        PassTimings* prevP = &g_state.passes[prevFrame.passIndex];
        if (g_state.gpuTimersOk && prevP->currentQuerySlot >= 0 && prevP->slotInFlight[prevP->currentQuerySlot]) {
            glEndQuery(GL_TIME_ELAPSED);
            // The query is now in flight; resolveGpuQueries will pick it up.
            // prevP's slot remains in-flight — we'll resume it on scope_exit.
        }
    }

    // --- CPU: push new scope onto the stack ---
    auto& frame = g_state.scopeStack[g_state.scopeStackDepth];
    frame.passIndex = static_cast<int>(p - g_state.passes.data());
    frame.cpuStart  = std::chrono::steady_clock::now();
    ++g_state.scopeStackDepth;

    // --- GPU: begin query on the next ring slot ---
    if (g_state.gpuTimersOk) {
        // Advance to next slot in the ring.
        p->currentQuerySlot = (p->currentQuerySlot + 1) % PassTimings::kQueryRingSize;

        // If the slot still has an in-flight query from a prior cycle,
        // try to resolve it (should be ready by now; if not, discard).
        if (p->slotInFlight[p->currentQuerySlot]) {
            GLuint ready = 0;
            glGetQueryObjectuiv(p->queries[p->currentQuerySlot], GL_QUERY_RESULT_AVAILABLE, &ready);
            if (ready) {
                GLuint64 nanos = 0;
                glGetQueryObjectui64v(p->queries[p->currentQuerySlot], GL_QUERY_RESULT, &nanos);
                const double ms = static_cast<double>(nanos) / 1.0e6;
                p->gpuMsSum   += ms;
                p->gpuSamples += 1;
                p->queryAvailable = true;
                g_state.rollingGpuMsSum += ms;
            }
            p->slotInFlight[p->currentQuerySlot] = false;
        }

        glBeginQuery(GL_TIME_ELAPSED, p->queries[p->currentQuerySlot]);
        p->slotInFlight[p->currentQuerySlot] = true;
    }
}

extern "C" void glperf_scope_exit(const char* name) {
    if (!g_state.initialized || !name) return;
    (void)name;

    if (g_state.scopeStackDepth <= 0) return;

    // Pop the current scope from the stack.
    const auto& frame = g_state.scopeStack[g_state.scopeStackDepth - 1];
    PassTimings* p = &g_state.passes[frame.passIndex];

    // --- CPU: accumulate time for this scope ---
    const auto now = std::chrono::steady_clock::now();
    const double ms = std::chrono::duration<double, std::milli>(now - frame.cpuStart).count();
    p->cpuMsSum   += ms;
    p->cpuSamples += 1;
    g_state.rollingCpuMsSum += ms;

    // --- GPU: end the query ---
    if (g_state.gpuTimersOk && p->currentQuerySlot >= 0 && p->slotInFlight[p->currentQuerySlot]) {
        glEndQuery(GL_TIME_ELAPSED);
        // Slot stays in-flight; resolveGpuQueries will collect the result
        // once the GPU is done.
    }

    --g_state.scopeStackDepth;

    // Resume the parent scope if there is one.
    if (g_state.scopeStackDepth > 0) {
        auto& parentFrame = g_state.scopeStack[g_state.scopeStackDepth - 1];
        PassTimings* parentP = &g_state.passes[parentFrame.passIndex];

        // The parent's CPU timer was paused when the child started.
        // Resetting cpuStart to 'now' means the interval from child-exit
        // to parent-exit will be attributed to the parent.
        parentFrame.cpuStart = now;

        // Resume the parent's GPU query on a new ring slot.
        if (g_state.gpuTimersOk) {
            parentP->currentQuerySlot = (parentP->currentQuerySlot + 1) % PassTimings::kQueryRingSize;
            if (parentP->slotInFlight[parentP->currentQuerySlot]) {
                GLuint ready = 0;
                glGetQueryObjectuiv(parentP->queries[parentP->currentQuerySlot], GL_QUERY_RESULT_AVAILABLE, &ready);
                if (ready) {
                    GLuint64 nanos = 0;
                    glGetQueryObjectui64v(parentP->queries[parentP->currentQuerySlot], GL_QUERY_RESULT, &nanos);
                    const double pms = static_cast<double>(nanos) / 1.0e6;
                    parentP->gpuMsSum   += pms;
                    parentP->gpuSamples += 1;
                    parentP->queryAvailable = true;
                    g_state.rollingGpuMsSum += pms;
                }
                parentP->slotInFlight[parentP->currentQuerySlot] = false;
            }
            glBeginQuery(GL_TIME_ELAPSED, parentP->queries[parentP->currentQuerySlot]);
            parentP->slotInFlight[parentP->currentQuerySlot] = true;
        }
    }
}

extern "C" void glperf_add_draw(uint32_t triangles) {
    if (!g_state.initialized) return;
    g_state.frameDrawCalls += 1;
    g_state.frameTriangles += triangles;
}

extern "C" void glperf_note_texture_bind(uint32_t handle) {
    if (!g_state.initialized) return;
    g_state.frameTextureBinds += 1;
    if (!g_state.frameHasLastTexture || g_state.frameLastTexture != handle) {
        g_state.frameDistinctTextures += 1;
        g_state.frameLastTexture    = handle;
        g_state.frameHasLastTexture = true;
    }
}

extern "C" void glperf_note_state_change() {
    if (!g_state.initialized) return;
    g_state.frameStateChanges += 1;
}

// ---------------------------------------------------------------------------
// Init / shutdown
// ---------------------------------------------------------------------------

namespace glperf_internal {
    bool g_perfInitCalled = false;
}

extern "C" void glperf_init() {
    if (glperf_internal::g_perfInitCalled) return;
    glperf_internal::g_perfInitCalled = true;

    // Probe GPU timer query support. In OpenGL 3.3 Core this is core, but
    // a few drivers may not actually support GL_TIME_ELAPSED with
    // glBeginQuery. Verify with a tiny dummy query.
    bool gpuOk = false;
    GLuint testQuery = 0;
    glGenQueries(1, &testQuery);
    GLenum err = glGetError();
    if (testQuery != 0 && err == GL_NO_ERROR) {
        glBeginQuery(GL_TIME_ELAPSED, testQuery);
        glEndQuery(GL_TIME_ELAPSED);
        glGetQueryObjectuiv(testQuery, GL_QUERY_RESULT_AVAILABLE, nullptr);
        err = glGetError();
        if (err == GL_NO_ERROR) {
            gpuOk = true;
        }
        glDeleteQueries(1, &testQuery);
    }
    g_state.gpuTimersOk = gpuOk;

    g_state.lastFlush = std::chrono::steady_clock::now();
    g_state.initialized = true;

    // Pick a timestamp for this session's log file. Computed once at init
    // so all rolling dumps in this session land in the same file.
    MakeTimestamp(g_state.logTimestamp, sizeof(g_state.logTimestamp), std::time(nullptr));
    MakeLogFilename(g_state.logFilename, sizeof(g_state.logFilename), g_state.logTimestamp);

    // Open the timestamped log immediately and write a startup banner. This
    // way the user can confirm the harness is alive even before the first
    // frame runs (e.g. by checking after a quick launch+quit).
    if (FILE* f = std::fopen(g_state.logFilename, "w")) {
        std::fprintf(f, "GLPerf harness v1 (Phase 0.1) -- started\n");
        std::fprintf(f, "  gpu_timers=%s\n", gpuOk ? "ok" : "n/a");
        char cwd[MAX_PATH] = {};
        if (GetCurrentDirectoryA(MAX_PATH, cwd) > 0) {
            std::fprintf(f, "  log_path=%s\\%s\n", cwd, g_state.logFilename);
        }
        std::fprintf(f, "  rolling 1s averages will append below once frames run\n\n");
        std::fclose(f);
    }
}

extern "C" void glperf_shutdown() {
    if (!g_state.initialized) return;
    if (g_state.captureActive) {
        stopCapture();
    }
    for (int i = 0; i < g_state.passCount; ++i) {
        PassTimings& p = g_state.passes[i];
        bool hasAny = false;
        for (int slot = 0; slot < PassTimings::kQueryRingSize; ++slot) {
            if (p.queries[slot]) {
                hasAny = true;
            } else {
                break;
            }
        }
        if (hasAny) {
            glDeleteQueries(PassTimings::kQueryRingSize, p.queries);
            for (int slot = 0; slot < PassTimings::kQueryRingSize; ++slot) {
                p.queries[slot] = 0;
            }
        }
    }
    if (g_state.logFile) {
        std::fclose(g_state.logFile);
        g_state.logFile = nullptr;
    }
    g_state = GLPerfState{};
    glperf_internal::g_perfInitCalled = false;
}

#endif  // GL_PERF_HOOKS
