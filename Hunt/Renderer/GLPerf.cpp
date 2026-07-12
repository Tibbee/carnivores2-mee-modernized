// ==========================================================================
// GLPerf.cpp — OpenGL renderer performance harness
//
// The harness is intentionally single-threaded: all hooks execute on the
// render thread while its GL context is current. CPU scope samples are
// exclusive under nesting. GPU scope samples use independent asynchronous
// timestamp pairs, so nested scopes do not attempt to nest GL_TIME_ELAPSED
// queries. A separate timestamp pair measures the whole render interval.
// ============================================================================

#include "GLPerf.h"

#ifndef GL_PERF_HOOKS

// Release stubs. All hooks are no-ops when GL_PERF_HOOKS is not defined.
// These symbols are still provided for the unguarded lifecycle calls shared
// by the renderer setup code.
extern "C" bool glperf_is_active()     { return false; }
extern "C" void glperf_trigger_capture() {}
extern "C" void glperf_init()          {}
extern "C" void glperf_shutdown()      {}

#else  // GL_PERF_HOOKS

#include "glad/glad.h"

#include <windows.h>

#include <array>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <string>
#include <unordered_set>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;

constexpr int   kMaxPasses          = 32;
constexpr int   kMaxCaptureFrames   = 120;
constexpr int   kScopeQueryRingSize = 8;
constexpr int   kFrameQueryRingSize = 8;
constexpr int   kMaxNestDepth       = 8;
constexpr float kLogIntervalSeconds = 1.0f;

constexpr size_t kTimestampLen = 32;
constexpr size_t kFilenameLen  = 96;

char* MakeTimestamp(char* out, size_t outSize, std::time_t t) {
    struct tm tmBuf {};
#ifdef _WIN32
    localtime_s(&tmBuf, &t);
#else
    localtime_r(&t, &tmBuf);
#endif
    std::strftime(out, outSize, "%Y-%m-%d-%H%M%S", &tmBuf);
    return out;
}

void MakeLogFilename(char* out, size_t outSize, const char* timestamp) {
    std::snprintf(out, outSize, "glperf-%s.log", timestamp);
}

void MakeCaptureFilename(char* out, size_t outSize, const char* timestamp) {
    std::snprintf(out, outSize, "glperf-capture-%s.csv", timestamp);
}

struct TimestampPair {
    GLuint begin = 0;
    GLuint end   = 0;
    bool active  = false; // begin was issued; end has not been issued
    bool ended   = false; // both commands issued; result may be in flight
};

struct PassTimings {
    const char* name = nullptr;
    TimestampPair queries[kScopeQueryRingSize] = {};
    int nextQuerySlot = -1;

    // Rolling stage aggregates. GPU sums are updated when query pairs become
    // available, not when the CPU scope exits.
    double   cpuMsSum = 0.0;
    double   gpuMsSum = 0.0;
    uint32_t cpuSamples = 0;
    uint32_t gpuSamples = 0;
    uint32_t gpuSamplesDropped = 0;
    bool     gpuQueryAvailable = false;
    bool     gpuScope = false;

    // Per-render-frame CPU aggregate used by the F11 CSV. If a scope is
    // entered more than once in one frame this is the exclusive total.
    double   frameCpuMs = 0.0;
    uint32_t frameCpuSamples = 0;
};

struct GLPerfState {
    bool initialized = false;
    bool gpuTimersOk = false;
    bool frameActive = false;

    // Counters for the currently active rendered frame.
    uint32_t frameDrawCalls = 0;
    uint32_t frameTriangles = 0;
    uint32_t frameStateChanges = 0;
    uint32_t frameTextureBinds = 0;
    uint32_t frameTextureSwitches = 0;
    uint32_t frameUniqueTextures = 0;
    uint32_t frameLastTexture = 0;
    bool frameHasLastTexture = false;
    std::unordered_set<uint32_t> frameUniqueTextureHandles;

    // Rolling counters. These are totals over `rollingFrames`; divide by
    // that field to obtain a per-render-frame average.
    uint32_t rollingFrames = 0;
    double rollingCpuMsSum = 0.0;
    uint64_t rollingDrawCalls = 0;
    uint64_t rollingTriangles = 0;
    uint64_t rollingStateChanges = 0;
    uint64_t rollingTextureBinds = 0;
    uint64_t rollingTextureSwitches = 0;
    uint64_t rollingUniqueTextures = 0;

    // Frame GPU queries have their own ring. The result is deliberately
    // accumulated by resolved sample count, rather than divided by all frame
    // end calls while some queries are still in flight.
    TimestampPair frameQueries[kFrameQueryRingSize] = {};
    int frameGpuQuerySlot = -1;
    uint32_t rollingGpuSamples = 0;
    uint32_t rollingGpuSamplesDropped = 0;
    double rollingGpuMsSum = 0.0;

    struct ScopeFrame {
        const char* name = nullptr;
        int passIndex = -1;
        Clock::time_point cpuStart;
        bool gpuRequested = false;
        bool gpuActive = false;
        int gpuQuerySlot = -1;
    };
    ScopeFrame scopeStack[kMaxNestDepth] = {};
    int scopeStackDepth = 0;

    Clock::time_point frameStart;
    double frameCpuMs = 0.0;

    std::array<PassTimings, kMaxPasses> passes {};
    int passCount = 0;

    Clock::time_point lastFlush;
    FILE* logFile = nullptr;
    char logTimestamp[kTimestampLen] = {};
    char logFilename[kFilenameLen] = {};

    bool captureActive = false;
    int captureFramesRemaining = 0;
    int captureFramesWritten = 0;
    bool captureHeaderWritten = false;
    FILE* captureFile = nullptr;
    char captureTimestamp[kTimestampLen] = {};
    std::vector<std::string> captureScopeOrder;
};

GLPerfState g_state;

bool QueryReady(GLuint query) {
    if (!query) return false;
    GLuint ready = GL_FALSE;
    glGetQueryObjectuiv(query, GL_QUERY_RESULT_AVAILABLE, &ready);
    return ready != GL_FALSE;
}

bool ReadTimestampPair(TimestampPair& pair, double& milliseconds) {
    if (!pair.ended || !QueryReady(pair.begin) || !QueryReady(pair.end)) {
        return false;
    }

    GLuint64 begin = 0;
    GLuint64 end = 0;
    glGetQueryObjectui64v(pair.begin, GL_QUERY_RESULT, &begin);
    glGetQueryObjectui64v(pair.end, GL_QUERY_RESULT, &end);
    pair.ended = false;

    if (end < begin) {
        return false;
    }

    milliseconds = static_cast<double>(end - begin) / 1.0e6;
    return true;
}

PassTimings* FindOrCreatePass(const char* name) {
    for (int i = 0; i < g_state.passCount; ++i) {
        if (std::strcmp(g_state.passes[i].name, name) == 0) {
            return &g_state.passes[i];
        }
    }

    if (g_state.passCount >= kMaxPasses) {
        return nullptr;
    }

    PassTimings* pass = &g_state.passes[g_state.passCount++];
    pass->name = name;
    if (g_state.gpuTimersOk) {
        std::array<GLuint, kScopeQueryRingSize * 2> ids {};
        glGenQueries(static_cast<GLsizei>(ids.size()), ids.data());
        for (int i = 0; i < kScopeQueryRingSize; ++i) {
            pass->queries[i].begin = ids[static_cast<size_t>(i) * 2];
            pass->queries[i].end = ids[static_cast<size_t>(i) * 2 + 1];
        }
    }
    return pass;
}

PassTimings* FindPassByName(const std::string& name) {
    for (int i = 0; i < g_state.passCount; ++i) {
        if (g_state.passes[i].name && name == g_state.passes[i].name) {
            return &g_state.passes[i];
        }
    }
    return nullptr;
}

void ResolvePassQueries(PassTimings& pass) {
    if (!g_state.gpuTimersOk) return;

    for (int i = 0; i < kScopeQueryRingSize; ++i) {
        TimestampPair& pair = pass.queries[i];
        if (!pair.ended) continue;

        double milliseconds = 0.0;
        if (!ReadTimestampPair(pair, milliseconds)) continue;

        pass.gpuMsSum += milliseconds;
        ++pass.gpuSamples;
        pass.gpuQueryAvailable = true;
    }
}

void ResolveFrameQueries() {
    if (!g_state.gpuTimersOk) return;

    for (int i = 0; i < kFrameQueryRingSize; ++i) {
        TimestampPair& pair = g_state.frameQueries[i];
        if (!pair.ended) continue;

        double milliseconds = 0.0;
        if (!ReadTimestampPair(pair, milliseconds)) continue;

        g_state.rollingGpuMsSum += milliseconds;
        ++g_state.rollingGpuSamples;
    }
}

void ResolveAllQueries() {
    ResolveFrameQueries();
    for (int i = 0; i < g_state.passCount; ++i) {
        ResolvePassQueries(g_state.passes[i]);
    }
}

int FindFreeFrameQuerySlot() {
    ResolveFrameQueries();

    for (int offset = 1; offset <= kFrameQueryRingSize; ++offset) {
        const int slot = (g_state.frameGpuQuerySlot + offset + kFrameQueryRingSize) % kFrameQueryRingSize;
        TimestampPair& pair = g_state.frameQueries[slot];
        if (!pair.active && !pair.ended) {
            g_state.frameGpuQuerySlot = slot;
            return slot;
        }
    }
    return -1;
}

int FindFreePassQuerySlot(PassTimings& pass) {
    ResolvePassQueries(pass);

    for (int offset = 1; offset <= kScopeQueryRingSize; ++offset) {
        const int slot = (pass.nextQuerySlot + offset + kScopeQueryRingSize) % kScopeQueryRingSize;
        TimestampPair& pair = pass.queries[slot];
        if (!pair.active && !pair.ended) {
            pass.nextQuerySlot = slot;
            return slot;
        }
    }
    return -1;
}

void AddCpuSegment(PassTimings& pass, double milliseconds) {
    if (milliseconds < 0.0) return;
    pass.cpuMsSum += milliseconds;
    pass.frameCpuMs += milliseconds;
}

void EndScopeGpu(GLPerfState::ScopeFrame& frame, PassTimings& pass) {
    if (!frame.gpuActive || frame.gpuQuerySlot < 0) return;

    TimestampPair& pair = pass.queries[frame.gpuQuerySlot];
    glQueryCounter(pair.end, GL_TIMESTAMP);
    pair.active = false;
    pair.ended = true;
    frame.gpuActive = false;
}

void StartScopeGpu(GLPerfState::ScopeFrame& frame, PassTimings& pass) {
    const int slot = FindFreePassQuerySlot(pass);
    if (slot < 0) {
        ++pass.gpuSamplesDropped;
        return;
    }

    TimestampPair& pair = pass.queries[slot];
    glQueryCounter(pair.begin, GL_TIMESTAMP);
    pair.active = true;
    pair.ended = false;
    frame.gpuQuerySlot = slot;
    frame.gpuActive = true;
}

void FinishTopScope(const Clock::time_point& now) {
    if (g_state.scopeStackDepth <= 0) return;

    GLPerfState::ScopeFrame frame =
        g_state.scopeStack[g_state.scopeStackDepth - 1];
    PassTimings& pass = g_state.passes[frame.passIndex];

    const double milliseconds = std::chrono::duration<double, std::milli>(
        now - frame.cpuStart).count();
    AddCpuSegment(pass, milliseconds);
    ++pass.cpuSamples;
    ++pass.frameCpuSamples;

    if (frame.gpuRequested) {
        EndScopeGpu(frame, pass);
    }

    --g_state.scopeStackDepth;
    if (g_state.scopeStackDepth > 0) {
        // The parent was paused while this child ran. Its next exclusive
        // segment starts at the child's exit time.
        g_state.scopeStack[g_state.scopeStackDepth - 1].cpuStart = now;
    }
}

void AppendCaptureHeader() {
    if (!g_state.captureFile) return;

    std::fprintf(g_state.captureFile,
        "frame,cpu_ms,gpu_ms,draw_calls,triangles,state_changes,"
        "texture_binds,texture_switches,unique_textures");
    for (const std::string& name : g_state.captureScopeOrder) {
        std::fprintf(g_state.captureFile, ",%s_cpu_ms,%s_gpu_ms",
                     name.c_str(), name.c_str());
    }
    std::fputc('\n', g_state.captureFile);
    g_state.captureHeaderWritten = true;
}

void StartCapture() {
    if (g_state.captureActive) return;

    g_state.captureActive = true;
    g_state.captureFramesRemaining = kMaxCaptureFrames;
    g_state.captureFramesWritten = 0;
    g_state.captureHeaderWritten = false;
    g_state.captureScopeOrder.clear();

    if (g_state.captureFile) {
        std::fclose(g_state.captureFile);
        g_state.captureFile = nullptr;
    }

    char filename[kFilenameLen] = {};
    MakeTimestamp(g_state.captureTimestamp, sizeof(g_state.captureTimestamp),
                  std::time(nullptr));
    MakeCaptureFilename(filename, sizeof(filename), g_state.captureTimestamp);
    g_state.captureFile = std::fopen(filename, "w");
}

void StopCapture() {
    if (!g_state.captureActive) return;

    g_state.captureActive = false;
    g_state.captureFramesRemaining = 0;
    if (g_state.captureFile) {
        std::fclose(g_state.captureFile);
        g_state.captureFile = nullptr;
    }
}

void WriteCaptureFrame() {
    if (!g_state.captureActive || !g_state.captureFile) return;

    // Delay the header until the first captured frame so a capture triggered
    // before the first scene has still seen all lazily-created scope names.
    if (!g_state.captureHeaderWritten) {
        for (int i = 0; i < g_state.passCount; ++i) {
            g_state.captureScopeOrder.emplace_back(g_state.passes[i].name);
        }
        AppendCaptureHeader();
    }

    // GPU query results are asynchronous. Keep the sentinel explicit instead
    // of writing a result from another frame or a rolling average here.
    std::fprintf(g_state.captureFile,
        "%d,%.3f,%.3f,%u,%u,%u,%u,%u,%u",
        g_state.captureFramesWritten,
        g_state.frameCpuMs,
        -1.0,
        g_state.frameDrawCalls,
        g_state.frameTriangles,
        g_state.frameStateChanges,
        g_state.frameTextureBinds,
        g_state.frameTextureSwitches,
        g_state.frameUniqueTextures);

    for (const std::string& name : g_state.captureScopeOrder) {
        const PassTimings* pass = FindPassByName(name);
        const double cpu = (pass && pass->frameCpuSamples > 0)
                         ? pass->frameCpuMs : 0.0;
        std::fprintf(g_state.captureFile, ",%.3f,%.3f", cpu, -1.0);
    }

    std::fputc('\n', g_state.captureFile);
    std::fflush(g_state.captureFile);
    ++g_state.captureFramesWritten;
}

void FlushRollingLog() {
    if (!g_state.initialized || g_state.rollingFrames == 0) return;

    if (!g_state.logFile) {
        g_state.logFile = std::fopen(g_state.logFilename, "a");
        if (!g_state.logFile) return;
    }

    const double avgCpu = g_state.rollingCpuMsSum /
                          static_cast<double>(g_state.rollingFrames);
    const bool haveGpu = g_state.rollingGpuSamples > 0;
    const double avgGpu = haveGpu
                        ? g_state.rollingGpuMsSum /
                          static_cast<double>(g_state.rollingGpuSamples)
                        : 0.0;

    std::time_t t = std::time(nullptr);
    struct tm tmBuf {};
#ifdef _WIN32
    localtime_s(&tmBuf, &t);
#else
    localtime_r(&t, &tmBuf);
#endif
    char timestamp[32] = {};
    std::strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &tmBuf);

    if (haveGpu) {
        std::fprintf(g_state.logFile,
            "[%s] frames=%u avg_cpu=%.2fms avg_gpu=%.2fms "
            "gpu_n=%u gpu_drop=%u draws=%llu tris=%llu states=%llu "
            "texture_binds=%llu texture_switches=%llu unique_textures=%llu\n",
            timestamp,
            g_state.rollingFrames,
            avgCpu,
            avgGpu,
            g_state.rollingGpuSamples,
            g_state.rollingGpuSamplesDropped,
            static_cast<unsigned long long>(g_state.rollingDrawCalls),
            static_cast<unsigned long long>(g_state.rollingTriangles),
            static_cast<unsigned long long>(g_state.rollingStateChanges),
            static_cast<unsigned long long>(g_state.rollingTextureBinds),
            static_cast<unsigned long long>(g_state.rollingTextureSwitches),
            static_cast<unsigned long long>(g_state.rollingUniqueTextures));
    } else {
        std::fprintf(g_state.logFile,
            "[%s] frames=%u avg_cpu=%.2fms avg_gpu=n/a gpu_n=0 "
            "gpu_drop=%u draws=%llu tris=%llu states=%llu "
            "texture_binds=%llu texture_switches=%llu unique_textures=%llu\n",
            timestamp,
            g_state.rollingFrames,
            avgCpu,
            g_state.rollingGpuSamplesDropped,
            static_cast<unsigned long long>(g_state.rollingDrawCalls),
            static_cast<unsigned long long>(g_state.rollingTriangles),
            static_cast<unsigned long long>(g_state.rollingStateChanges),
            static_cast<unsigned long long>(g_state.rollingTextureBinds),
            static_cast<unsigned long long>(g_state.rollingTextureSwitches),
            static_cast<unsigned long long>(g_state.rollingUniqueTextures));
    }

    for (int i = 0; i < g_state.passCount; ++i) {
        const PassTimings& pass = g_state.passes[i];
        const double cpuAvg = pass.cpuSamples > 0
                            ? pass.cpuMsSum / pass.cpuSamples : 0.0;
        if (pass.gpuScope && g_state.gpuTimersOk) {
            const double gpuAvg = pass.gpuSamples > 0
                                ? pass.gpuMsSum / pass.gpuSamples : 0.0;
            std::fprintf(g_state.logFile,
                "  %-22s cpu=%.2fms gpu=%.2fms "
                "(cpu_n=%u gpu_n=%u gpu_drop=%u)\n",
                pass.name,
                cpuAvg,
                gpuAvg,
                pass.cpuSamples,
                pass.gpuSamples,
                pass.gpuSamplesDropped);
        } else {
            const char* reason = pass.gpuScope ? "gpu=n/a" : "gpu=n/a(cpu-only)";
            std::fprintf(g_state.logFile,
                "  %-22s cpu=%.2fms %s "
                "(cpu_n=%u gpu_n=0 gpu_drop=%u)\n",
                pass.name,
                cpuAvg,
                reason,
                pass.cpuSamples,
                pass.gpuSamplesDropped);
        }
    }
    std::fputc('\n', g_state.logFile);
    std::fflush(g_state.logFile);

    g_state.rollingFrames = 0;
    g_state.rollingCpuMsSum = 0.0;
    g_state.rollingDrawCalls = 0;
    g_state.rollingTriangles = 0;
    g_state.rollingStateChanges = 0;
    g_state.rollingTextureBinds = 0;
    g_state.rollingTextureSwitches = 0;
    g_state.rollingUniqueTextures = 0;
    g_state.rollingGpuSamples = 0;
    g_state.rollingGpuSamplesDropped = 0;
    g_state.rollingGpuMsSum = 0.0;

    for (int i = 0; i < g_state.passCount; ++i) {
        PassTimings& pass = g_state.passes[i];
        pass.cpuMsSum = 0.0;
        pass.gpuMsSum = 0.0;
        pass.cpuSamples = 0;
        pass.gpuSamples = 0;
        pass.gpuSamplesDropped = 0;
        pass.gpuQueryAvailable = false;
    }
}

bool ProbeTimestampQueries() {
    if (!glad_glQueryCounter || !glad_glGetQueryObjectuiv ||
        !glad_glGetQueryObjectui64v) {
        return false;
    }

    while (glGetError() != GL_NO_ERROR) {
        // Clear an initialization error before the probe.
    }

    GLuint queries[2] = {};
    glGenQueries(2, queries);
    if (!queries[0] || !queries[1] || glGetError() != GL_NO_ERROR) {
        if (queries[0] || queries[1]) glDeleteQueries(2, queries);
        return false;
    }

    glQueryCounter(queries[0], GL_TIMESTAMP);
    glQueryCounter(queries[1], GL_TIMESTAMP);
    const GLenum error = glGetError();
    glDeleteQueries(2, queries);
    return error == GL_NO_ERROR;
}

bool BeginScope(const char* name, bool timeGpu) {
    if (!g_state.initialized || !g_state.frameActive || !name) {
        return false;
    }
    if (g_state.scopeStackDepth >= kMaxNestDepth) {
        return false;
    }

    PassTimings* pass = FindOrCreatePass(name);
    if (!pass) return false;
    pass->gpuScope = pass->gpuScope || timeGpu;

    const Clock::time_point now = Clock::now();
    if (g_state.scopeStackDepth > 0) {
        GLPerfState::ScopeFrame& parent =
            g_state.scopeStack[g_state.scopeStackDepth - 1];
        PassTimings& parentPass = g_state.passes[parent.passIndex];
        const double parentMilliseconds = std::chrono::duration<double, std::milli>(
            now - parent.cpuStart).count();
        AddCpuSegment(parentPass, parentMilliseconds);
    }

    GLPerfState::ScopeFrame& frame =
        g_state.scopeStack[g_state.scopeStackDepth++];
    frame.name = name;
    frame.passIndex = static_cast<int>(pass - g_state.passes.data());
    frame.cpuStart = now;
    frame.gpuRequested = timeGpu && g_state.gpuTimersOk;
    frame.gpuActive = false;
    frame.gpuQuerySlot = -1;

    if (frame.gpuRequested) {
        StartScopeGpu(frame, *pass);
    }
    return true;
}

namespace glperf_internal {
    bool g_perfInitCalled = false;
}

} // namespace

// ---------------------------------------------------------------------------
// Public C-linkage surface
// ---------------------------------------------------------------------------

extern "C" bool glperf_is_active() {
    return g_state.initialized;
}

extern "C" void glperf_trigger_capture() {
    if (!g_state.initialized) return;
    StartCapture();
}

extern "C" void glperf_frame_begin() {
    if (!g_state.initialized || g_state.frameActive) return;

    // Resolve old queries before the new frame's CPU interval starts. This
    // polling is non-blocking and is not part of scene CPU time.
    ResolveAllQueries();

    g_state.frameActive = true;
    g_state.frameStart = Clock::now();
    g_state.frameCpuMs = 0.0;
    g_state.frameDrawCalls = 0;
    g_state.frameTriangles = 0;
    g_state.frameStateChanges = 0;
    g_state.frameTextureBinds = 0;
    g_state.frameTextureSwitches = 0;
    g_state.frameUniqueTextures = 0;
    g_state.frameLastTexture = 0;
    g_state.frameHasLastTexture = false;
    g_state.frameUniqueTextureHandles.clear();
    if (g_state.frameUniqueTextureHandles.bucket_count() == 0) {
        g_state.frameUniqueTextureHandles.reserve(64);
    }
    g_state.scopeStackDepth = 0;

    for (int i = 0; i < g_state.passCount; ++i) {
        g_state.passes[i].frameCpuMs = 0.0;
        g_state.passes[i].frameCpuSamples = 0;
    }

    g_state.frameGpuQuerySlot = -1;
    if (g_state.gpuTimersOk) {
        const int slot = FindFreeFrameQuerySlot();
        if (slot >= 0) {
            TimestampPair& pair = g_state.frameQueries[slot];
            glQueryCounter(pair.begin, GL_TIMESTAMP);
            pair.active = true;
            pair.ended = false;
            g_state.frameGpuQuerySlot = slot;
        } else {
            ++g_state.rollingGpuSamplesDropped;
        }
    }
}

extern "C" void glperf_frame_end() {
    if (!g_state.initialized || !g_state.frameActive) return;

    // A balanced call path leaves this empty. Close defensively without
    // touching SwapBuffers or blocking for a GPU result.
    while (g_state.scopeStackDepth > 0) {
        FinishTopScope(Clock::now());
    }

    if (g_state.gpuTimersOk && g_state.frameGpuQuerySlot >= 0) {
        TimestampPair& pair = g_state.frameQueries[g_state.frameGpuQuerySlot];
        if (pair.active) {
            glQueryCounter(pair.end, GL_TIMESTAMP);
            pair.active = false;
            pair.ended = true;
        }
    }
    g_state.frameGpuQuerySlot = -1;

    const Clock::time_point now = Clock::now();
    g_state.frameCpuMs = std::chrono::duration<double, std::milli>(
        now - g_state.frameStart).count();
    g_state.frameActive = false;

    // Best effort only; newly-ended queries will normally resolve in a later
    // frame and are then counted exactly once by their own sample counters.
    ResolveAllQueries();

    ++g_state.rollingFrames;
    g_state.rollingCpuMsSum += g_state.frameCpuMs;
    g_state.rollingDrawCalls += g_state.frameDrawCalls;
    g_state.rollingTriangles += g_state.frameTriangles;
    g_state.rollingStateChanges += g_state.frameStateChanges;
    g_state.rollingTextureBinds += g_state.frameTextureBinds;
    g_state.rollingTextureSwitches += g_state.frameTextureSwitches;
    g_state.rollingUniqueTextures += g_state.frameUniqueTextures;

    if (g_state.captureActive) {
        WriteCaptureFrame();
        if (g_state.captureFramesRemaining > 0) {
            --g_state.captureFramesRemaining;
        }
        if (g_state.captureFramesRemaining == 0) {
            StopCapture();
        }
    }

    const float secondsSinceFlush = std::chrono::duration<float>(
        now - g_state.lastFlush).count();
    if (secondsSinceFlush >= kLogIntervalSeconds) {
        FlushRollingLog();
        g_state.lastFlush = now;
    }
}

extern "C" bool glperf_scope_enter(const char* name) {
    return BeginScope(name, true);
}

extern "C" bool glperf_scope_enter_cpu(const char* name) {
    return BeginScope(name, false);
}

extern "C" void glperf_scope_exit(const char* name) {
    if (!g_state.initialized || !g_state.frameActive || !name ||
        g_state.scopeStackDepth <= 0) {
        return;
    }

    const GLPerfState::ScopeFrame& top =
        g_state.scopeStack[g_state.scopeStackDepth - 1];
    if (!top.name || std::strcmp(top.name, name) != 0) {
        return;
    }

    FinishTopScope(Clock::now());
}

extern "C" void glperf_add_draw(uint32_t triangles) {
    if (!g_state.initialized || !g_state.frameActive) return;
    ++g_state.frameDrawCalls;
    g_state.frameTriangles += triangles;
}

extern "C" void glperf_note_texture_bind(uint32_t handle) {
    if (!g_state.initialized || !g_state.frameActive) return;

    ++g_state.frameTextureBinds;
    if (!g_state.frameHasLastTexture || g_state.frameLastTexture != handle) {
        ++g_state.frameTextureSwitches;
        g_state.frameLastTexture = handle;
        g_state.frameHasLastTexture = true;
    }

    const auto inserted = g_state.frameUniqueTextureHandles.insert(handle);
    if (inserted.second) {
        g_state.frameUniqueTextures = static_cast<uint32_t>(
            g_state.frameUniqueTextureHandles.size());
    }
}

extern "C" void glperf_note_state_change() {
    if (!g_state.initialized || !g_state.frameActive) return;
    ++g_state.frameStateChanges;
}

// ---------------------------------------------------------------------------
// Init / shutdown
// ---------------------------------------------------------------------------

extern "C" void glperf_init() {
    if (glperf_internal::g_perfInitCalled) return;
    glperf_internal::g_perfInitCalled = true;

    g_state = GLPerfState{};
    const bool gpuOk = ProbeTimestampQueries();
    g_state.gpuTimersOk = gpuOk;

    if (gpuOk) {
        std::array<GLuint, kFrameQueryRingSize * 2> ids {};
        glGenQueries(static_cast<GLsizei>(ids.size()), ids.data());
        for (int i = 0; i < kFrameQueryRingSize; ++i) {
            g_state.frameQueries[i].begin = ids[static_cast<size_t>(i) * 2];
            g_state.frameQueries[i].end = ids[static_cast<size_t>(i) * 2 + 1];
        }
    }

    g_state.lastFlush = Clock::now();
    g_state.initialized = true;

    MakeTimestamp(g_state.logTimestamp, sizeof(g_state.logTimestamp),
                  std::time(nullptr));
    MakeLogFilename(g_state.logFilename, sizeof(g_state.logFilename),
                    g_state.logTimestamp);

    if (FILE* file = std::fopen(g_state.logFilename, "w")) {
        std::fprintf(file, "GLPerf harness v2 -- started\n");
        std::fprintf(file, "  gpu_timers=%s\n", gpuOk ? "timestamp" : "n/a");
        char cwd[MAX_PATH] = {};
        if (GetCurrentDirectoryA(MAX_PATH, cwd) > 0) {
            std::fprintf(file, "  log_path=%s\\%s\n", cwd, g_state.logFilename);
        }
        std::fprintf(file,
            "  one matched frame begin/end per rendered frame; counters are "
            "rolling totals over frames\n");
        std::fprintf(file,
            "  frame CPU: DrawScene before PreCashGroundModel through "
            "ShowVideo before SwapBuffers\n");
        std::fprintf(file,
            "  GPU log values use resolved samples (gpu_n); CSV GPU fields "
            "are -1 by design\n\n");
        std::fclose(file);
    }
}

extern "C" void glperf_shutdown() {
    if (!g_state.initialized) return;

    if (g_state.captureActive) StopCapture();

    for (int i = 0; i < g_state.passCount; ++i) {
        PassTimings& pass = g_state.passes[i];
        std::array<GLuint, kScopeQueryRingSize * 2> ids {};
        int count = 0;
        for (int q = 0; q < kScopeQueryRingSize; ++q) {
            if (pass.queries[q].begin) {
                ids[static_cast<size_t>(count++)] = pass.queries[q].begin;
            }
            if (pass.queries[q].end) {
                ids[static_cast<size_t>(count++)] = pass.queries[q].end;
            }
        }
        if (count > 0) glDeleteQueries(count, ids.data());
    }

    std::array<GLuint, kFrameQueryRingSize * 2> frameIds {};
    int frameCount = 0;
    for (int i = 0; i < kFrameQueryRingSize; ++i) {
        if (g_state.frameQueries[i].begin) {
            frameIds[static_cast<size_t>(frameCount++)] =
                g_state.frameQueries[i].begin;
        }
        if (g_state.frameQueries[i].end) {
            frameIds[static_cast<size_t>(frameCount++)] =
                g_state.frameQueries[i].end;
        }
    }
    if (frameCount > 0) glDeleteQueries(frameCount, frameIds.data());

    if (g_state.logFile) {
        std::fclose(g_state.logFile);
        g_state.logFile = nullptr;
    }

    g_state = GLPerfState{};
    glperf_internal::g_perfInitCalled = false;
}

#endif  // GL_PERF_HOOKS
