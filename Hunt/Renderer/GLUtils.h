// ==========================================================================
// GLUtils.h — Shared helpers for OpenGL renderer files
// ==========================================================================

#pragma once

#include "Core/MathTypes.h"
#include "Core/Constants.h"
#include <cstdint>
#include <vector>

#ifdef _gl
#include "glad/glad.h"
#endif

#ifdef _gl
// Shared GL module handle
extern HMODULE libGL;
#endif

// ---------- constants ----------
constexpr float kModelNearClip = -16.0f;

// ---------- structs ----------
struct FogSample
{
    float amount;
    Vector3d color;
};

struct ModelClipVertex
{
    Vector3d position;
    Vector2df uv;
    float light;
};

// ---------- function declarations ----------
#ifdef _gl
void* glad_get_proc(const char* name);
GLuint CompileShader(GLenum type, const char* source);
GLuint LinkProgram(GLuint vertexShader, GLuint fragmentShader);
#endif

Vector3d DecodeFogColor(int rgb);
Vector3d DecodeFogColorBGR(int rgb);
// §3.6: sun-angle fog colour shift (warm/dim by sun elevation + visibility).
// Shared by the terrain fog colour (GetFogColorForMapPoint) and the
// model/water fog colour (CalcFogLevel -> CurFogColor) so the SAME
// shift reaches all three paths.  Pass sunLight <= 0.1f (or 0.0f)
// to skip the shift.  Deterministic in (fogRGB, sunLight), so calling
// it per vertex/per corner still yields a per-volume-uniform result.
int ApplySunFogColourShift(int fogRGB, float sunLight);
Vector3d GetFogColor();
Vector3d GetDistanceFogColor();
int GetFogIndexForMapPoint(int mapX, int mapY);
Vector2df DecodeLegacyFaceUV(float tx, float ty, int texHeight);
Vector3d TransformModelVertex(const TPoint3d& source, float x0, float y0, float z0, float ca, float sa, float cb, float sb);
bool ShouldCullModelFace(WORD flags, const Vector3d& p0, const Vector3d& p1, const Vector3d& p2);
float DistanceToWaterPlane(const Vector3d& position);
ModelClipVertex InterpolateClipVertex(const ModelClipVertex& a, const ModelClipVertex& b, float t);
void ClipTriangleAgainstWater(const ModelClipVertex& a,
    const ModelClipVertex& b,
    const ModelClipVertex& c,
    std::vector<ModelClipVertex>& output);

// Ensure the scene-copy texture exists and matches window size
#ifdef _gl
void EnsureNightSceneTex(GLuint& tex, int& texW, int& texH, int winW, int winH);
#endif
WORD Conv565to555(WORD c);

// Clipping / fog helpers
float DistanceToNearClipPlane(const Vector3d& position);
void ClipTriangleAgainstNearPlane(const ModelClipVertex& a,
    const ModelClipVertex& b,
    const ModelClipVertex& c,
    std::vector<ModelClipVertex>& output);
// point is camera-relative WORLD space (before camera rotation).
// Shadows skip the legacy CPU horizon fallback: model.frag already applies
// distance fog, and their fog must match the terrain receiver, not double it.
FogSample SampleFogAtPoint(const Vector3d& point, bool disableFog, bool distanceFallback = true);

// Terrain / water helpers
float VertexDistanceSq(const Vector3d& v);
float CalcTerrainAlpha(float distanceSq, float fadeStart, float fadeStartSq, float fadeEnd);
// Phase 5: overload with cached isUnderwater to avoid per-call global load
float CalcTerrainAlpha(float distanceSq, float fadeStart, float fadeStartSq, float fadeEnd, bool isUnderwater);
float GetTerrainFogAmountForMapPoint(int fogIndex, int legacyFog);
// Phase 5: overload with cached isUnderwater
float GetTerrainFogAmountForMapPoint(int fogIndex, int legacyFog, bool isUnderwater);
