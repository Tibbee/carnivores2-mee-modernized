// ==========================================================================
// GLUtils.cpp — Shared helper implementations for OpenGL renderer
// ==========================================================================

#include "Hunt.h"
#include "GLRenderer.h"
#include "Renderer/GLUtils.h"

#ifdef _gl

#include "glad/glad.h"
#include <array>
#include <cmath>
#include <cstdint>

HMODULE libGL = nullptr;

void* glad_get_proc(const char* name)
{
    void* p = (void*)wglGetProcAddress(name);
    if (p == 0 || p == (void*)0x1 || p == (void*)0x2 || p == (void*)0x3 || p == (void*)-1) {
        p = (void*)GetProcAddress(libGL, name);
    }
    return p;
}

GLuint CompileShader(GLenum type, const char* source)
{
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    GLint success = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[1024];
        glGetShaderInfoLog(shader, sizeof(infoLog), nullptr, infoLog);
        PrintLog("GL: Shader compilation failed:\n");
        PrintLog(infoLog);
        glDeleteShader(shader);
        return 0;
    }

    return shader;
}

GLuint LinkProgram(GLuint vertexShader, GLuint fragmentShader)
{
    GLuint program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);

    GLint success = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[1024];
        glGetProgramInfoLog(program, sizeof(infoLog), nullptr, infoLog);
        PrintLog("GL: Program linking failed:\n");
        PrintLog(infoLog);
        glDeleteProgram(program);
        return 0;
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    return program;
}

Vector3d DecodeFogColor(int rgb)
{
    return {
        static_cast<float>(rgb & 0xFF) / 255.0f,
        static_cast<float>((rgb >> 8) & 0xFF) / 255.0f,
        static_cast<float>((rgb >> 16) & 0xFF) / 255.0f
    };
}

Vector3d DecodeFogColorBGR(int rgb)
{
    return {
        static_cast<float>((rgb >> 16) & 0xFF) / 255.0f,
        static_cast<float>((rgb >> 8) & 0xFF) / 255.0f,
        static_cast<float>(rgb & 0xFF) / 255.0f
    };
}

Vector3d GetFogColor()
{
    if (IsUnderwater() && FogsList[127].fogRGB) {
        return DecodeFogColorBGR(FogsList[127].fogRGB);
    }

    if (CAMERAINFOG && CameraFogI > 0) {
        return DecodeFogColor(FogsList[CameraFogI].fogRGB);
    }

    return {
        static_cast<float>(SkyR) / 255.0f,
        static_cast<float>(SkyG) / 255.0f,
        static_cast<float>(SkyB) / 255.0f
    };
}

Vector3d GetDistanceFogColor()
{
    if (IsUnderwater() && FogsList[127].fogRGB) {
        return DecodeFogColorBGR(FogsList[127].fogRGB);
    }

    return {
        static_cast<float>(SkyR) / 255.0f,
        static_cast<float>(SkyG) / 255.0f,
        static_cast<float>(SkyB) / 255.0f
    };
}

int GetFogIndexForMapPoint(int mapX, int mapY)
{
    const int fogX = (mapX & (ctMapSize - 1)) >> 1;
    const int fogY = (mapY & (ctMapSize - 1)) >> 1;
    return FogsMap[fogY][fogX];
}

Vector2df DecodeLegacyFaceUV(float tx, float ty, int texHeight)
{
    const float h = static_cast<float>((texHeight > 1) ? texHeight : 1);
    return { tx / 256.0f, ty / h };
}

Vector3d TransformModelVertex(const TPoint3d& source, float x0, float y0, float z0, float ca, float sa, float cb, float sb)
{
    Vector3d result;
    result.x = source.x * ca + source.z * sa + x0;
    const float vz = source.z * ca - source.x * sa;
    result.y = source.y * cb - vz * sb + y0;
    result.z = vz * cb + source.y * sb + z0;
    return result;
}

bool ShouldCullModelFace(WORD flags, const Vector3d& p0, const Vector3d& p1, const Vector3d& p2)
{
    if ((flags & (sfDarkBack | sfNeedVC)) == 0) {
        return false;
    }

    const Vector3d edge1 = {p1.x - p0.x, p1.y - p0.y, p1.z - p0.z};
    const Vector3d edge2 = {p2.x - p0.x, p2.y - p0.y, p2.z - p0.z};
    const Vector3d normal = {
        edge1.y * edge2.z - edge1.z * edge2.y,
        edge1.z * edge2.x - edge1.x * edge2.z,
        edge1.x * edge2.y - edge1.y * edge2.x
    };

    const float facing = normal.x * p0.x + normal.y * p0.y + normal.z * p0.z;
    return facing < 0.0f;
}

float DistanceToWaterPlane(const Vector3d& position)
{
    const float dx = position.x - waterclipbase.x;
    const float dy = position.y - waterclipbase.y;
    const float dz = position.z - waterclipbase.z;
    return dx * ClipW.nv.x + dy * ClipW.nv.y + dz * ClipW.nv.z;
}

ModelClipVertex InterpolateClipVertex(const ModelClipVertex& a, const ModelClipVertex& b, float t)
{
    return {
        { a.position.x + (b.position.x - a.position.x) * t,
          a.position.y + (b.position.y - a.position.y) * t,
          a.position.z + (b.position.z - a.position.z) * t },
        { a.uv.x + (b.uv.x - a.uv.x) * t,
          a.uv.y + (b.uv.y - a.uv.y) * t },
        a.light + (b.light - a.light) * t
    };
}

void ClipTriangleAgainstWater(const ModelClipVertex& a,
    const ModelClipVertex& b,
    const ModelClipVertex& c,
    std::vector<ModelClipVertex>& output)
{
    output.clear();
    output.reserve(4);

    const std::array<ModelClipVertex, 3> input = {a, b, c};

    const float da = DistanceToWaterPlane(a.position);
    const float db = DistanceToWaterPlane(b.position);
    const float dc = DistanceToWaterPlane(c.position);

    const bool aIn = da >= 0.0f;
    const bool bIn = db >= 0.0f;
    const bool cIn = dc >= 0.0f;

    const int inCount = (aIn ? 1 : 0) + (bIn ? 1 : 0) + (cIn ? 1 : 0);

    auto emit = [&](const ModelClipVertex& v) {
        output.push_back(v);
    };

    auto clipEdge = [&](const ModelClipVertex& v1, const ModelClipVertex& v2, float d1, float d2) -> ModelClipVertex {
        const float t = d1 / (d1 - d2);
        return InterpolateClipVertex(v1, v2, t);
    };

    if (inCount == 3) {
        emit(a); emit(b); emit(c);
    } else if (inCount == 2) {
        const ModelClipVertex *in1, *in2, *out1;
        float d_in1, d_in2, d_out1;
        if (!aIn) { out1 = &a; d_out1 = da; in1 = &b; d_in1 = db; in2 = &c; d_in2 = dc; }
        else if (!bIn) { out1 = &b; d_out1 = db; in1 = &a; d_in1 = da; in2 = &c; d_in2 = dc; }
        else { out1 = &c; d_out1 = dc; in1 = &a; d_in1 = da; in2 = &b; d_in2 = db; }
        emit(*in1);
        emit(*in2);
        emit(clipEdge(*in1, *out1, d_in1, d_out1));
        emit(clipEdge(*in2, *out1, d_in2, d_out1));
    } else if (inCount == 1) {
        const ModelClipVertex *in1, *out1, *out2;
        float d_in1, d_out1, d_out2;
        if (aIn) { in1 = &a; d_in1 = da; out1 = &b; d_out1 = db; out2 = &c; d_out2 = dc; }
        else if (bIn) { in1 = &b; d_in1 = db; out1 = &a; d_out1 = da; out2 = &c; d_out2 = dc; }
        else { in1 = &c; d_in1 = dc; out1 = &a; d_out1 = da; out2 = &b; d_out2 = db; }
        emit(*in1);
        emit(clipEdge(*in1, *out1, d_in1, d_out1));
        emit(clipEdge(*in1, *out2, d_in1, d_out2));
    }
}

void EnsureNightSceneTex(GLuint& tex, int& texW, int& texH, int winW, int winH)
{
    if (!tex || texW != winW || texH != winH) {
        if (!tex) glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, winW, winH, 0,
                     GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        texW = winW;
        texH = winH;
    }
}

WORD Conv565to555(WORD c)
{
    int r = (c >> 11) & 0x1F;
    int g = (c >> 5) & 0x3F;
    int b = c & 0x1F;
    return (r << 10) | ((g >> 1) << 5) | b;
}

float DistanceToNearClipPlane(const Vector3d& position)
{
    return position.z - kModelNearClip;
}

void ClipTriangleAgainstNearPlane(const ModelClipVertex& a,
                                         const ModelClipVertex& b,
                                         const ModelClipVertex& c,
                                         std::vector<ModelClipVertex>& output)
{
    output.clear();
    output.reserve(4);

    const std::array<ModelClipVertex, 3> input = {a, b, c};
    for (size_t i = 0; i < input.size(); ++i) {
        const ModelClipVertex& current = input[i];
        const ModelClipVertex& previous = input[(i + input.size() - 1) % input.size()];
        const float currentDistance = DistanceToNearClipPlane(current.position);
        const float previousDistance = DistanceToNearClipPlane(previous.position);
        const bool currentInside = currentDistance <= 0.0f;
        const bool previousInside = previousDistance <= 0.0f;

        if (currentInside != previousInside) {
            const float denom = previousDistance - currentDistance;
            const float t = std::fabs(denom) < 0.0001f ? 0.0f : previousDistance / denom;
            output.push_back(InterpolateClipVertex(previous, current, t));
        }

        if (currentInside) {
            output.push_back(current);
        }
    }
}

FogSample SampleFogAtPoint(const Vector3d& point, bool disableFog)
{
    if (disableFog) {
        return {0.0f, GetDistanceFogColor()};
    }

    float d = VectorLength(point);

    // Helper: compute fog using the given pocket's parameters.
    // Mirrors CalcFogLevel's (fla+flb) * distance formula.
    // fla = vertex depth below pocket's YBegin
    // flb = camera depth below pocket's YBegin
    auto computeFog = [&](const TFogEntity& fog, bool& outVinFog) -> float {
        float fla = -(point.y + CameraY - fog.YBegin * ctHScale) / ctHScale;
        float flb = -(CameraY - fog.YBegin * ctHScale) / ctHScale;

        if (!outVinFog && fla > 0.0f) fla = 0.0f;

        if (fla < 0.0f && flb < 0.0f) return 0.0f;

        if (fla < 0.0f) { d *= flb / (flb - fla); fla = 0.0f; }
        if (flb < 0.0f) { d *= fla / (fla - flb); flb = 0.0f; }

        float fl = (fla + flb) * (d + fog.Transp * 0.5f) / fog.Transp;
        return std::clamp(fl, 0.0f, fog.FLimit);
    };

    if (IsUnderwater()) {
        const TFogEntity& fog = FogsList[127];
        bool vinFog = true;
        float fl = computeFog(fog, vinFog);

        if (fl <= 0.0f) {
            fl = (d + fog.Transp * 0.5f) / fog.Transp;
        }

        const float amount = std::clamp(fl / 255.0f, 0.0f, fog.FLimit / 255.0f);
        return {amount, DecodeFogColorBGR(fog.fogRGB)};
    }

    // Fixed fog volumes are local: only sample the volume that contains
    // the point being fogged. Do not use the camera's current pocket as a
    // blanket fog source, or distant objects outside the volume will inherit
    // the volume color.
    const int worldX = static_cast<int>(point.x + CameraX);
    const int worldZ = static_cast<int>(point.z + CameraZ);
    const int cf = FogsMap[(worldZ >> 9) & 511][(worldX >> 9) & 511];
    if (cf > 0) {
        const TFogEntity& fog = FogsList[cf];
        bool vinFog = true;
        const float fl = computeFog(fog, vinFog);
        const float amount = std::clamp(fl / 255.0f, 0.0f, fog.FLimit / 255.0f);
        if (amount > 0.0f) {
            return {amount, DecodeFogColor(fog.fogRGB)};
        }
    }

    // Distance-only fog: fades all objects (including BMP billboards)
    // to the global horizon color at the view horizon. Uses the same ramp
    // as the terrain shader (75% to 100% of view distance). This masks
    // distant pop-in without letting local fog volumes tint the horizon.
    {
        const float fogDistance = static_cast<float>(ctViewR) * 256.0f;
        const float fogFadeStart = static_cast<float>(ctViewR) * 192.0f;
        const float fadeRange = fogDistance - fogFadeStart;
        const float distanceFog = std::clamp(
            (d - fogFadeStart) / (fadeRange > 1.0f ? fadeRange : 1.0f),
            0.0f, 1.0f);
        if (distanceFog > 0.0f) {
            return {distanceFog, GetDistanceFogColor()};
        }
    }

    return {0.0f, GetDistanceFogColor()};
}

float VertexDistanceSq(const Vector3d& v)
{
    return v.x * v.x + v.y * v.y + v.z * v.z;
}

float CalcTerrainAlpha(float distanceSq, float fadeStart, float fadeStartSq, float fadeEnd)
{
    return CalcTerrainAlpha(distanceSq, fadeStart, fadeStartSq, fadeEnd, IsUnderwater());
}

// Phase 5: overload with cached isUnderwater — avoids the per-call
// global IsUnderwater() load (~500K calls saved per frame at max view).
float CalcTerrainAlpha(float distanceSq, float fadeStart, float fadeStartSq, float fadeEnd, bool isUnderwater)
{
    if (isUnderwater) {
        return 1.0f;
    }

    if (distanceSq <= fadeStartSq) {
        return 1.0f;
    }

    // Tiles between fadeStart and fadeEnd all return 1.0 — avoid the
    // std::sqrt for this common range by comparing squared distances.
    if (distanceSq <= fadeEnd * fadeEnd) {
        return 1.0f;
    }

    const float distance = std::sqrt(distanceSq);
    const float zz = distance - fadeEnd;
    return std::clamp((255.0f - zz / 3.0f) / 255.0f, 0.0f, 1.0f);
}

float GetTerrainFogAmountForMapPoint(int fogIndex, int legacyFog)
{
    return GetTerrainFogAmountForMapPoint(fogIndex, legacyFog, IsUnderwater());
}

// Phase 5: overload with cached isUnderwater — avoids the per-call
// global IsUnderwater() load.
float GetTerrainFogAmountForMapPoint(int fogIndex, int legacyFog, bool isUnderwater)
{
    if (isUnderwater) {
        return static_cast<float>(legacyFog);
    }

    if (!FOGON || fogIndex <= 0) {
        return 0.0f;
    }

    return static_cast<float>(std::clamp(legacyFog, 0, 255));
}
#endif // _gl
