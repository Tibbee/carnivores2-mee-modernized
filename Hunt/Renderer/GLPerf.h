// ==========================================================================
// GLPerf.h — OpenGL renderer performance harness (Phase 0.1)
//
// Lightweight in-engine profiling hooks for the OpenGL renderer only. All
// hooks compile to no-ops when GL_PERF_HOOKS is not defined, so release
// builds pay zero cost.
//
// Usage:
//   #define GL_PERF_HOOKS at the top of one TU (or via /D in the build) to
//   enable the harness. Recommended: define in a dedicated CMake
//   "ogl-perf-debug" preset so the release presets stay clean.
//
//   GL_PERF_FRAME_BEGIN()  -- call once at the start of a frame
//   GL_PERF_FRAME_END()    -- call once at the end of a frame
//   GL_PERF_SCOPE("name")  -- RAII timer scope (CPU + GPU)
//   GL_PERF_DRAW(n)        -- n = triangle count from glDrawArrays
//   GL_PERF_TEXTURE_BIND(h)-- log a texture bind (deduped on handle)
//   GL_PERF_STATE_CHANGE() -- log a state change (program/blend/depth)
//
// All output is written to glperf.log in the working directory. F11 starts
// a 1-second per-frame CSV capture to glperf-frame.csv.
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

// F11 capture trigger. Starts a ~1s per-frame dump to glperf-frame.csv.
void glperf_trigger_capture();

// Init / shutdown. Called from the GL renderer (LoadGLExtensions / Shutdown).
// Init probes GPU timer query support. Shutdown releases GL query objects
// and closes the log file.
void glperf_init();
void glperf_shutdown();

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

// Frame boundary. Begin at the start of a frame, end at the end.
void glperf_frame_begin();
void glperf_frame_end();


// CPU+GPU scope enter/exit. The name must be a string literal with static
// lifetime (typically a string literal at the call site).
void glperf_scope_enter(const char* name);
void glperf_scope_exit(const char* name);

// Counter hooks.
void glperf_add_draw(uint32_t triangles);
void glperf_note_texture_bind(uint32_t handle);
void glperf_note_state_change();

#ifdef __cplusplus
}
#endif

class GLPerfScope {
public:
    explicit GLPerfScope(const char* name) : m_name(name) { glperf_scope_enter(name); }
    ~GLPerfScope()                                          { glperf_scope_exit(m_name); }
    GLPerfScope(const GLPerfScope&) = delete;
    GLPerfScope& operator=(const GLPerfScope&) = delete;
private:
    const char* m_name;
};

#define GL_PERF_FRAME_BEGIN()       ::glperf_frame_begin()
#define GL_PERF_FRAME_END()         ::glperf_frame_end()
#define GL_PERF_SCOPE(name)         ::GLPerfScope glperf_scope_obj_(name)
#define GL_PERF_DRAW(n)             ::glperf_add_draw(static_cast<uint32_t>(n))
#define GL_PERF_TEXTURE_BIND(h)     ::glperf_note_texture_bind(static_cast<uint32_t>(h))
#define GL_PERF_STATE_CHANGE()      ::glperf_note_state_change()

#else  // GL_PERF_HOOKS not defined -- release builds

#define GL_PERF_FRAME_BEGIN()       ((void)0)
#define GL_PERF_FRAME_END()         ((void)0)
#define GL_PERF_SCOPE(name)         ((void)0)
#define GL_PERF_DRAW(n)             ((void)0)
#define GL_PERF_TEXTURE_BIND(h)     ((void)0)
#define GL_PERF_STATE_CHANGE()      ((void)0)

#endif // GL_PERF_HOOKS

#endif // GLPERF_H
