// RenderContext.h — Per-frame renderer state (replaces direct global reads)
// Phase 2.1: bundles all globals the renderer reads into a single context
// struct, eliminating scattered global lookups from DrawScene.
#pragma once

#include "Core/MathTypes.h"
#include "Core/Constants.h"

struct RenderFrameContext {
    // ── Viewport ──────────────────────────────────────────────────────
    int winW, winH;
    int videoCX, videoCY;
    int videoPitch;

    // ── Projection ────────────────────────────────────────────────────
    float cameraW, cameraH;
    float softPerspK;

    // ── Camera (world space) ──────────────────────────────────────────
    float cameraX, cameraY, cameraZ;

    // ── Fog ───────────────────────────────────────────────────────────
    Vector3d fogColor;
    Vector3d distanceFogColor;
    float fogDensity;
    bool underwater;
    bool fogEnabled;
    bool cameraInFog;
    int ctViewR;
    float forceFog;
    int curFogColor;
    int cameraFogI;

    // ── Sky ───────────────────────────────────────────────────────────
    Vector3d skyColor;
    float skyTraceK;
    float traceK;
    int sunScrX, sunScrY;

    // ── Map / Terrain data ─────────────────────────────────────────────
    const void* vMap;      // const EPoint (*)[kViewGridSize]
    const void* vMap2;     // const EPoint (*)[kViewGridSize], use EPoint (*)[kViewGridSize]
    const void* fogsList;  // const TFogEntity*
    int skyMin, skyDTime;

    // ── Lighting ───────────────────────────────────────────────────────
    float sunLight;
    bool hard3D;

    // ── UI / Video Buffer ──────────────────────────────────────────────
    float uiScale;
    void* lpVideoBuf;
    void* hbmpVideoBuf;
    void* hdcCMain;

    // ── Night Vision ───────────────────────────────────────────────────
    bool nightVisionOn;
    bool nightVisionMode;

    // ── Water ───────────────────────────────────────────────────────────
    bool waterReverse;
    bool waterClip;

    // ── Misc render state ──────────────────────────────────────────────
    bool gourard;
    bool correction;
    bool clip3D;
    bool noDarkBack;
    bool noClip;
    int glassL;
    int shadowSize;

    // ── Default constructor: zero-initialize ───────────────────────────
    RenderFrameContext() = default;

    // Populate from globals (defined in Hunt.cpp or equivalent)
    static RenderFrameContext FromGlobals();
};
