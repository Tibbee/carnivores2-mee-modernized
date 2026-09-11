// ==========================================================================
// GLPerf.h -- OpenGL renderer performance harness
//
// Lightweight in-engine profiling hooks for the OpenGL renderer only. All
// hooks compile to no-ops when GL_PERF_HOOKS is not defined, so non-GL
// renderer builds and normal builds pay zero cost.
//
// Frame contract:
//   GL_PERF_FRAME_BEGIN()  -- once at the start of DrawScene, before
//                             PreCashGroundModel
//   GL_PERF_FRAME_END()    -- once in ShowVideo, after scene/post-processing
//                             and immediately before SwapBuffers
//
// Scope/counter contract:
//   GL_PERF_SCOPE("name")      -- CPU scope plus an asynchronous GPU pair
//   GL_PERF_CPU_SCOPE("name")  -- CPU-only scope (use for collection work)
//   GL_PERF_DRAW(n)             -- n = triangles submitted by one draw call
//   GL_PERF_TERRAIN_WORKLOAD(...) -- aggregate terrain cull/geometry counters
//   GL_PERF_TEXTURE_BIND(h)     -- an instrumented render-time texture bind
//   GL_PERF_STATE_CHANGE()      -- an instrumented render-state change
//
// GPU queries are resolved only with GL_QUERY_RESULT_AVAILABLE polling. The
// rolling log reports resolved GPU samples; the F11 CSV deliberately writes
// -1 for GPU fields because a result is not attributed to an arbitrary CPU
// frame when it becomes available. The CSV CPU/counter fields are per frame.
// ==========================================================================

#ifndef GLPERF_H
#define GLPERF_H

#pragma once

#include <cstdint>

// ---------------------------------------------------------------------------
// Public C-linkage surface (callable from non-GL TUs like Hunt.cpp)
// ---------------------------------------------------------------------------

#ifdef __cplusplus
extern "C" {
#endif

// True if the harness is compiled in and runtime-initialized.
bool glperf_is_active();

// F11 capture trigger. Starts a fixed per-frame CPU/counter CSV capture.
void glperf_trigger_capture();

// Init / shutdown. Called from the GL renderer after a current context exists.
void glperf_init();
void glperf_shutdown();

// Runtime control of log output. When disabled, the harness still collects
// data internally but does not write to log files or CSV captures.
// Can be called before or after glperf_init(); if called before init,
// the value is stored and applied during initialization.
void glperf_set_logging(bool enabled);

#ifdef __cplusplus
}
#endif

// ---------------------------------------------------------------------------
// C++-only scope timer (RAII). When GL_PERF_HOOKS is off, all macros below
// resolve to ((void)0) and GLPerfScope is not declared.
// ---------------------------------------------------------------------------

#ifdef GL_PERF_HOOKS

#ifdef __cplusplus
extern "C" {
#endif

// Frame boundary. Duplicate begins/ends are ignored defensively so one bad
// call site cannot double-count a rendered frame.
void glperf_frame_begin();
void glperf_frame_end();
// Recorded separately; CSV attributes these to the previous rendered frame.
void glperf_swap_begin();
void glperf_swap_end();

// Return true when a scope was pushed. The RAII wrapper uses this to avoid a
// destructor closing an unrelated scope when the profiling stack is full.
bool glperf_scope_enter(const char* name);
bool glperf_scope_enter_cpu(const char* name);
void glperf_scope_exit(const char* name);

// Counter hooks.
void glperf_add_draw(uint32_t triangles);
void glperf_note_terrain_workload(uint32_t chunkCandidates,
                                 uint32_t tileCandidates,
                                 uint32_t coarseCulled,
                                 uint32_t backCulled,
                                 uint32_t frustumCulled,
                                 uint32_t distanceCulled,
                                 uint32_t alphaCulled,
                                 uint32_t emittedTiles,
                                 uint32_t vertices);
void glperf_note_texture_bind(uint32_t handle);
void glperf_note_state_change();

#ifdef __cplusplus
}
#endif

class GLPerfScope {
public:
    explicit GLPerfScope(const char* name, bool timeGpu = true)
        : m_name(name),
          m_active(timeGpu ? glperf_scope_enter(name)
                           : glperf_scope_enter_cpu(name)) {}

    ~GLPerfScope() {
        if (m_active) glperf_scope_exit(m_name);
    }

    GLPerfScope(const GLPerfScope&) = delete;
    GLPerfScope& operator=(const GLPerfScope&) = delete;

private:
    const char* m_name;
    bool m_active;
};

#define GL_PERF_FRAME_BEGIN()       ::glperf_frame_begin()
#define GL_PERF_FRAME_END()         ::glperf_frame_end()
#define GL_PERF_SCOPE(name)         ::GLPerfScope glperf_scope_obj_(name)
#define GL_PERF_CPU_SCOPE(name)     ::GLPerfScope glperf_scope_obj_(name, false)
#define GL_PERF_DRAW(n)             ::glperf_add_draw(static_cast<uint32_t>(n))
#define GL_PERF_TERRAIN_WORKLOAD(chunks, candidates, coarse, back, frustum, distance, alpha, emitted, vertices) \
    ::glperf_note_terrain_workload(static_cast<uint32_t>(chunks), static_cast<uint32_t>(candidates), \
        static_cast<uint32_t>(coarse), static_cast<uint32_t>(back), static_cast<uint32_t>(frustum), \
        static_cast<uint32_t>(distance), static_cast<uint32_t>(alpha), static_cast<uint32_t>(emitted), \
        static_cast<uint32_t>(vertices))
#define GL_PERF_TEXTURE_BIND(h)     ::glperf_note_texture_bind(static_cast<uint32_t>(h))
#define GL_PERF_STATE_CHANGE()      ::glperf_note_state_change()

#else  // GL_PERF_HOOKS not defined -- release builds

#define GL_PERF_FRAME_BEGIN()       ((void)0)
#define GL_PERF_FRAME_END()         ((void)0)
#define GL_PERF_SCOPE(name)         ((void)0)
#define GL_PERF_CPU_SCOPE(name)     ((void)0)
#define GL_PERF_DRAW(n)             ((void)0)
#define GL_PERF_TERRAIN_WORKLOAD(chunks, candidates, coarse, back, frustum, distance, alpha, emitted, vertices) ((void)0)
#define GL_PERF_TEXTURE_BIND(h)     ((void)0)
#define GL_PERF_STATE_CHANGE()      ((void)0)

#endif // GL_PERF_HOOKS

#endif // GLPERF_H
