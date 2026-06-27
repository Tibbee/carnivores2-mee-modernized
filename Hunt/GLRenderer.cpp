// ==========================================================================
// GLRenderer.cpp — OpenGL 3.3 Core Profile renderer for Carnivores 2 ME
// ==========================================================================

#include "Hunt.h"
#include "GLRenderer.h"

#ifdef _gl

#include "glad/glad.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "Renderer/GLUtils.h"

#define WGL_CONTEXT_MAJOR_VERSION_ARB     0x2091
#define WGL_CONTEXT_MINOR_VERSION_ARB     0x2092
#define WGL_CONTEXT_PROFILE_MASK_ARB      0x9126
#define WGL_CONTEXT_CORE_PROFILE_BIT_ARB  0x00000001

using PFNWGLCREATECONTEXTATTRIBSARBPROC = HGLRC (WINAPI *)(HDC, HGLRC, const int*);



static float DistanceToNearClipPlane(const Vector3d& position)
{
    return position.z - kModelNearClip;
}

static void ClipTriangleAgainstNearPlane(const ModelClipVertex& a,
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

GLRenderer* g_GLRenderer = nullptr;

GLRenderer::GLRenderer() = default;

GLRenderer::~GLRenderer()
{
    Shutdown();
}

bool GLRenderer::CreateContext()
{
    PrintLog("\n");
    PrintLog("==Init OpenGL==\n");

    m_hwnd = hwndMain;
    if (!m_hwnd) {
        PrintLog("GL: ERROR - hwndMain is nullptr!\n");
        return false;
    }

    m_hdc = GetDC(m_hwnd);
    if (!m_hdc) {
        PrintLog("GL: ERROR - GetDC failed!\n");
        return false;
    }

    PIXELFORMATDESCRIPTOR pfd = {
        sizeof(PIXELFORMATDESCRIPTOR),
        1,
        PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
        PFD_TYPE_RGBA,
        32,
        0, 0, 0, 0, 0, 0,
        0,
        0,
        0,
        0, 0, 0, 0,
        24,
        8,
        0,
        PFD_MAIN_PLANE,
        0,
        0, 0, 0
    };

    int pixelFormat = ChoosePixelFormat(m_hdc, &pfd);
    if (!pixelFormat) {
        PrintLog("GL: ERROR - Failed to choose pixel format.\n");
        return false;
    }

    if (!SetPixelFormat(m_hdc, pixelFormat, &pfd)) {
        PrintLog("GL: ERROR - Failed to set pixel format.\n");
        return false;
    }

    HGLRC tempContext = wglCreateContext(m_hdc);
    if (!tempContext) {
        PrintLog("GL: ERROR - Failed to create temporary context.\n");
        return false;
    }

    if (!wglMakeCurrent(m_hdc, tempContext)) {
        PrintLog("GL: ERROR - Failed to make temporary context current.\n");
        wglDeleteContext(tempContext);
        return false;
    }

    PFNWGLCREATECONTEXTATTRIBSARBPROC wglCreateContextAttribsARB =
        (PFNWGLCREATECONTEXTATTRIBSARBPROC)wglGetProcAddress("wglCreateContextAttribsARB");

    if (wglCreateContextAttribsARB) {
        int attribs[] = {
            WGL_CONTEXT_MAJOR_VERSION_ARB, 3,
            WGL_CONTEXT_MINOR_VERSION_ARB, 3,
            WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
            0
        };

        m_hrc = wglCreateContextAttribsARB(m_hdc, 0, attribs);
        if (m_hrc) {
            wglMakeCurrent(nullptr, nullptr);
            wglDeleteContext(tempContext);
            wglMakeCurrent(m_hdc, m_hrc);
            PrintLog("GL: OpenGL 3.3 Core Profile context created.\n");
        } else {
            PrintLog("GL: WARNING - Failed to create 3.3 context, falling back to legacy.\n");
            m_hrc = tempContext;
        }
    } else {
        PrintLog("GL: WARNING - wglCreateContextAttribsARB not found, using legacy context.\n");
        m_hrc = tempContext;
    }

    if (!m_hrc) {
        PrintLog("GL: ERROR - Failed to create any context.\n");
        return false;
    }

    libGL = LoadLibraryA("opengl32.dll");
    if (!libGL) {
        PrintLog("GL: ERROR - Failed to load opengl32.dll!\n");
        return false;
    }

    if (!gladLoadGLLoader((GLADloadproc)glad_get_proc)) {
        PrintLog("GL: ERROR - Failed to initialize GLAD.\n");
        return false;
    }

    char logMsg[512];
    const char* version = (const char*)glGetString(GL_VERSION);
    if (version) {
        sprintf(logMsg, "GL: Version: %s\n", version);
        PrintLog(logMsg);
    }

    const char* vendor = (const char*)glGetString(GL_VENDOR);
    if (vendor) {
        sprintf(logMsg, "GL: Vendor: %s\n", vendor);
        PrintLog(logMsg);
    }

    const char* rendererStr = (const char*)glGetString(GL_RENDERER);
    if (rendererStr) {
        sprintf(logMsg, "GL: Renderer: %s\n", rendererStr);
        PrintLog(logMsg);
    }

    const char* glslVersion = (const char*)glGetString(GL_SHADING_LANGUAGE_VERSION);
    if (glslVersion) {
        sprintf(logMsg, "GL: GLSL Version: %s\n", glslVersion);
        PrintLog(logMsg);
    }

    GLint numExtensions = 0;
    glGetIntegerv(GL_NUM_EXTENSIONS, &numExtensions);
    sprintf(logMsg, "GL: Extensions: %d\n", numExtensions);
    PrintLog(logMsg);

    PrintLog("==OpenGL Initialized==\n");
    PrintLog("\n");

    return true;
}

bool GLRenderer::InitGLState()
{
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glViewport(0, 0, WinW, WinH);
    return true;
}

void GLRenderer::LoadGLExtensions()
{
#ifdef GL_PERF_HOOKS
    glperf_init();
#endif
}

bool GLRenderer::Initialize()
{
    PrintLog("GL: Initialize() called.\n");

    if (!CreateContext()) {
        PrintLog("GL: Initialize() failed at CreateContext().\n");
        return false;
    }

    if (!InitGLState()) {
        PrintLog("GL: Initialize() failed at InitGLState().\n");
        return false;
    }

    LoadGLExtensions();

    const char* terrainVertexSource =
        "#version 330 core\n"
        "layout (location = 0) in vec3 aPos;\n"
        "layout (location = 1) in vec2 aTexCoord;\n"
        "layout (location = 2) in float aLayer;\n"
        "layout (location = 3) in vec4 aLightFogAlpha;\n"
        "layout (location = 4) in vec3 aFogColor;\n"
        "uniform PerFrame {\n"
        "   mat4 uProjection;\n"
        "   vec2 uFogRange;          // (fadeStart, distance)\n"
        "   vec3 uDistanceFogColor;\n"
        "   float uForceFog;\n"
        "   vec3 uFogColor;\n"
        "   mat4 uView;\n"
        "   vec4 uWaterAlphaFade;    // x=start, y=end, z=enabled, w=fade step\n"
        "};\n"
        "out vec2 vTexCoord;\n"
        "flat out int vLayer;\n"
        "out float vLight;\n"
        "out float vFog;\n"
        "out vec3 vFogColor;\n"
        "out float vAlpha;\n"
        "out float vViewZ;\n"
        "out float vViewDistance;\n"
        "out float vWaterAlphaFade;\n"
        "void main() {\n"
        "   gl_Position = uProjection * vec4(aPos, 1.0);\n"
        "   vTexCoord = aTexCoord;\n"
        "   vLayer = int(aLayer + 0.5);\n"
        "   // uint8 attributes are normalized to [0,1] by the driver.\n"
        "   vLight = aLightFogAlpha.x;\n"
        "   vFog   = aLightFogAlpha.y;\n"
        "   vAlpha = aLightFogAlpha.z;\n"
        "   vFogColor = aFogColor;\n"
        "   vViewZ = max(-aPos.z, 0.0);\n"
        "   vViewDistance = length(aPos);\n"
        "   vWaterAlphaFade = aLightFogAlpha.w;\n"
        "}\n";

    const char* terrainFragmentSource =
        "#version 330 core\n"
        "out vec4 FragColor;\n"
        "in vec2 vTexCoord;\n"
        "flat in int vLayer;\n"
        "in float vLight;\n"
        "in float vFog;\n"
        "in vec3 vFogColor;\n"
        "in float vAlpha;\n"
        "in float vViewZ;\n"
        "in float vViewDistance;\n"
        "in float vWaterAlphaFade;\n"
        "uniform PerFrame {\n"
        "   mat4 uProjection;\n"
        "   vec2 uFogRange;          // (fadeStart, distance)\n"
        "   vec3 uDistanceFogColor;\n"
        "   float uForceFog;\n"
        "   vec3 uFogColor;\n"
        "   mat4 uView;\n"
        "   vec4 uWaterAlphaFade;    // x=start, y=end, z=enabled, w=fade step\n"
        "};\n"
        "uniform sampler2DArray uTerrainArray;\n"
        "void main() {\n"
        "   vec4 texColor = texture(uTerrainArray, vec3(vTexCoord, float(vLayer)));\n"
        "   if (texColor.a < 0.05) discard;\n"
        "   vec3 litColor = texColor.rgb * vLight;\n"
        "   // Per-vertex volumetric fog (volume-specific color and amount).\n"
        "   vec3 volumetricFogColor = mix(litColor, vFogColor, vFog);\n"
        "   // Per-pixel distance fog: smooth ramp from uFogRange.x to\n"
        "   // uFogRange.y. Uses the global horizon color instead of the\n"
        "   // per-vertex vFogColor, which prevents local fog volumes from\n"
        "   // bleeding into the horizon fade.\n"
        "   float distanceFog = clamp((vViewZ - uFogRange.x) / max(uFogRange.y - uFogRange.x, 1.0), 0.0, 1.0);\n"
        "   vec3 finalColor = mix(volumetricFogColor, uDistanceFogColor, distanceFog);\n"
        "   float waterAlphaFade = 1.0;\n"
        "   if (vWaterAlphaFade > 0.5 && uWaterAlphaFade.z > 0.5) {\n"
        "      float zz = vViewDistance - uWaterAlphaFade.y;\n"
        "      if (zz > 0.0) {\n"
        "         waterAlphaFade = clamp((255.0 - zz / max(uWaterAlphaFade.w, 1.0)) / 255.0, 0.0, 1.0);\n"
        "      }\n"
        "   }\n"
        "   FragColor = vec4(finalColor, texColor.a * vAlpha * waterAlphaFade);\n"
        "}\n";

    GLuint vertexShader = CompileShader(GL_VERTEX_SHADER, terrainVertexSource);
    if (!vertexShader) {
        return false;
    }

    GLuint fragmentShader = CompileShader(GL_FRAGMENT_SHADER, terrainFragmentSource);
    if (!fragmentShader) {
        glDeleteShader(vertexShader);
        return false;
    }

    m_terrainShader = LinkProgram(vertexShader, fragmentShader);
    if (!m_terrainShader) {
        return false;
    }

    const char* modelVertexSource =
        "#version 330 core\n"
        "layout (location = 0) in vec3 aPos;\n"
        "layout (location = 1) in vec2 aTexCoord;\n"
        "layout (location = 2) in vec4 aLightFogAlphaCutout;\n"
        "layout (location = 3) in vec3 aFogColor;\n"
        "uniform PerFrame {\n"
        "   mat4 uProjection;\n"
        "   vec2 uFogRange;\n"
        "   vec3 uDistanceFogColor;\n"
        "   float uForceFog;\n"
        "   vec3 uFogColor;\n"
        "};\n"
        "out vec2 vTexCoord;\n"
        "out float vLight;\n"
        "out float vFog;\n"
        "out vec3 vFogColor;\n"
        "out float vAlpha;\n"
        "out float vCutout;\n"
        "out float vViewZ;\n"
        "void main() {\n"
        "   gl_Position = uProjection * vec4(aPos, 1.0);\n"
        "   vTexCoord = aTexCoord;\n"
        "   // uint8 attributes are normalized to [0,1] by the driver.\n"
        "   vLight  = aLightFogAlphaCutout.x;\n"
        "   vFog    = aLightFogAlphaCutout.y;\n"
        "   vAlpha  = aLightFogAlphaCutout.z;\n"
        "   vCutout = aLightFogAlphaCutout.w;\n"
        "   vFogColor = aFogColor;\n"
        "   vViewZ = max(-aPos.z, 0.0);\n"
        "}\n";

    const char* modelFragmentSource =
        "#version 330 core\n"
        "out vec4 FragColor;\n"
        "in vec2 vTexCoord;\n"
        "in float vLight;\n"
        "in float vFog;\n"
        "in vec3 vFogColor;\n"
        "in float vAlpha;\n"
        "in float vCutout;\n"
        "in float vViewZ;\n"
        "uniform PerFrame {\n"
        "   mat4 uProjection;\n"
        "   vec2 uFogRange;\n"
        "   vec3 uDistanceFogColor;\n"
        "   float uForceFog;\n"
        "   vec3 uFogColor;\n"
        "};\n"
        "uniform sampler2D uModelTexture;\n"
        "uniform float uTintByFogColor;\n"
        "void main() {\n"
        "   vec4 texColor = texture(uModelTexture, vTexCoord);\n"
        "   if (vCutout > 0.5 && texColor.a <= 0.5) discard;\n"
        "   vec3 litColor = texColor.rgb * vLight;\n"
        "   // Phase 2.7: branch-less tint via mix (was if > 0.5).\n"
        "   vec3 tinted = litColor * vFogColor;\n"
        "   litColor = mix(litColor, tinted, uTintByFogColor);\n"
        "   vec3 finalColor = mix(litColor, vFogColor, vFog);\n"
        "   // Match the terrain/instanced-model horizon fade for legacy\n"
        "   // model-path objects (BMP billboards and water-clipped meshes).\n"
        "   float distanceFog = clamp((vViewZ - uFogRange.x) / max(uFogRange.y - uFogRange.x, 1.0), 0.0, 1.0);\n"
        "   finalColor = mix(finalColor, uDistanceFogColor, distanceFog);\n"
        "   FragColor = vec4(finalColor, texColor.a * vAlpha);\n"
        "}\n";

    GLuint modelVertexShader = CompileShader(GL_VERTEX_SHADER, modelVertexSource);
    if (!modelVertexShader) {
        return false;
    }

    GLuint modelFragmentShader = CompileShader(GL_FRAGMENT_SHADER, modelFragmentSource);
    if (!modelFragmentShader) {
        glDeleteShader(modelVertexShader);
        return false;
    }

    m_modelShader = LinkProgram(modelVertexShader, modelFragmentShader);
    if (!m_modelShader) {
        return false;
    }

    // Phase 2.3+2.4: instanced model shader.
    // Per-vertex (from static mesh VBO, binding 0):
    //   attribute 0: vec3 aPos        — model-space position
    //   attribute 1: vec3 aNormal      — face normal (unused in 2.3, reserved for 2.5)
    //   attribute 2: vec2 aTexCoord    — pre-baked UV
    // Per-instance (from instance VBO, binding 1, divisor=1):
    //   attribute 4: vec4 aWorldCol0   — matrix column 0 (model→view for now)
    //   attribute 5: vec4 aWorldCol1   — matrix column 1
    //   attribute 6: vec4 aWorldCol2   — matrix column 2
    //   attribute 7: vec4 aWorldCol3   — matrix column 3
    //   attribute 8: vec4 aInstanceLight — .x = base light [0,1]
    //   attribute 9: vec4 aInstanceFlags — .x = cutout, .y = tintByFog, .z = fogAmount, .w = alpha
    // Phase 2.4: uView added to PerFrame UBO (currently identity).
    //   gl_Position = uProjection * uView * iWorld * vec4(aPos, 1.0)
    // Future phases will split iWorld into model→world and uView into world→view.
    const char* instancedModelVertexSource =
        "#version 330 core\n"
        "// Per-vertex from static mesh VBO (binding 0)\n"
        "layout (location = 0) in vec3 aPos;\n"
        "layout (location = 1) in vec3 aNormal;\n"
        "layout (location = 2) in vec2 aTexCoord;\n"
        "// Per-instance from instance VBO (binding 1, divisor=1)\n"
        "layout (location = 4) in vec4 aWorldCol0;\n"
        "layout (location = 5) in vec4 aWorldCol1;\n"
        "layout (location = 6) in vec4 aWorldCol2;\n"
        "layout (location = 7) in vec4 aWorldCol3;\n"
        "layout (location = 8) in vec4 aInstanceLight;\n"
        "layout (location = 9) in vec4 aInstanceFlags;\n"
        "uniform PerFrame {\n"
        "   mat4 uProjection;\n"
        "   vec2 uFogRange;\n"
        "   vec3 uDistanceFogColor;\n"
        "   float uForceFog;\n"
        "   vec3 uFogColor;\n"
        "   mat4 uView;              // Phase 2.4: view matrix (identity for now)\n"
        "};\n"
        "out vec2 vTexCoord;\n"
        "out float vLight;\n"
        "out float vViewZ;           // Phase 2.5: view-space Z for per-pixel fog\n"
        "out vec3 vWorldNormal;      // Phase 2.5: face normal for directional light\n"
        "out float vAlpha;\n"
        "out float vCutout;\n"
        "out float vTintByFog;\n"
        "out vec3 vVolumetricFogColor; // Phase 2.6: per-instance pocket fog colour\n"
        "out float vVolumetricFog;     // Phase 2.6: per-vertex pocket fog amount\n"
        "void main() {\n"
        "   mat4 iWorld = mat4(aWorldCol0, aWorldCol1, aWorldCol2, aWorldCol3);\n"
        "   vec4 viewPos = uView * iWorld * vec4(aPos, 1.0);\n"
        "   gl_Position = uProjection * viewPos;\n"
        "   vTexCoord = aTexCoord;\n"
        "   vLight = aInstanceLight.x;\n"
        "   vCutout = aInstanceFlags.x;\n"
        "   // Phase 2.x: tintByFog not used for instanced; slot .y is now fogGrad.\n"
        "   vTintByFog = 0.0;\n"
        "   vAlpha = aInstanceFlags.w;\n"
        "   // Phase 2.5: vViewZ = view-space depth (positive in front of camera).\n"
        "   // Matches the terrain shader's vViewZ = max(-aPos.z, 0.0).\n"
        "   vViewZ = max(-viewPos.z, 0.0);\n"
        "   // Phase 2.5: transform face normal for directional light\n"
        "   vWorldNormal = mat3(iWorld) * aNormal;\n"
        "   // Phase 2.x: 3DFX-style height-graded pocket fog.\n"
        "   // fogGrad is the Y-gradient (dFog/dY), aInstanceFlags.z is fogBase.\n"
        "   // Per-vertex fog = fogBase + modelSpaceY * fogGrad, clamped.\n"
        "   float fogGrad = aInstanceFlags.y;\n"
        "   float perVertexFog = aInstanceFlags.z + aPos.y * fogGrad;\n"
        "   vVolumetricFog = clamp(perVertexFog, 0.0, 1.0);\n"
        "   vVolumetricFogColor = aInstanceLight.yzw;\n"
        "}\n";

    const char* instancedModelFragmentSource =
        "#version 330 core\n"
        "out vec4 FragColor;\n"
        "in vec2 vTexCoord;\n"
        "in float vLight;\n"
        "in float vViewZ;\n"
        "in vec3 vWorldNormal;\n"
        "in float vAlpha;\n"
        "in float vCutout;\n"
        "in float vTintByFog;\n"
        "in vec3 vVolumetricFogColor;\n"
        "in float vVolumetricFog;\n"
        "uniform PerFrame {\n"
        "   mat4 uProjection;\n"
        "   vec2 uFogRange;\n"
        "   vec3 uDistanceFogColor;\n"
        "   float uForceFog;\n"
        "   vec3 uFogColor;\n"
        "   mat4 uView;\n"
        "};\n"
        "uniform sampler2D uModelTexture;\n"
        "void main() {\n"
        "   vec4 texColor = texture(uModelTexture, vTexCoord);\n"
        "   if (vCutout > 0.5 && texColor.a <= 0.5) discard;\n"
        "   vec3 litColor = texColor.rgb * vLight;\n"
        "   // Phase 2.7: branch-less tint via mix (was if > 0.5).\n"
        "   vec3 tinted = litColor * uDistanceFogColor;\n"
        "   litColor = mix(litColor, tinted, vTintByFog);\n"
        "   // Phase 2.5: per-pixel distance fog matching the terrain shader.\n"
        "   // Ramp from uFogRange.x to uFogRange.y, uses view-space Z\n"
        "   // (not Euclidean distance) for parity with the terrain.\n"
        "   float distanceFog = clamp((vViewZ - uFogRange.x) / max(uFogRange.y - uFogRange.x, 1.0), 0.0, 1.0);\n"
        "   // Phase 2.6: volumetric (pocket) fog placeholder — zero for now.\n"
        "   vec3 afterVolumetric = mix(litColor, vVolumetricFogColor, vVolumetricFog);\n"
        "   // Final: fade to distance fog colour over the ramp.\n"
        "   vec3 finalColor = mix(afterVolumetric, uDistanceFogColor, distanceFog);\n"
        "   FragColor = vec4(finalColor, texColor.a * vAlpha);\n"
        "}\n";

    GLuint instancedVertexShader = CompileShader(GL_VERTEX_SHADER, instancedModelVertexSource);
    if (!instancedVertexShader) {
        return false;
    }
    GLuint instancedFragmentShader = CompileShader(GL_FRAGMENT_SHADER, instancedModelFragmentSource);
    if (!instancedFragmentShader) {
        glDeleteShader(instancedVertexShader);
        return false;
    }
    m_instancedModelShader = LinkProgram(instancedVertexShader, instancedFragmentShader);
    if (!m_instancedModelShader) {
        return false;
    }

    if (!InitializeTerrainPipeline()) {
        return false;
    }

    if (!InitializeModelPipeline()) {
        return false;
    }

    // Phase 2.3 fix: StaticMeshPipeline must initialize before
    // InstancingPipeline so m_instanceVAO can reference m_staticMeshVBO
    // (which doesn't exist yet if the order is reversed).
    if (!InitializeStaticMeshPipeline()) {
        return false;
    }

    if (!InitializeInstancingPipeline()) {
        return false;
    }

    InitializeSkyPipeline();
    InitializeHudPipeline();
    InitializeNightDesaturation();

    glUseProgram(m_modelShader);
    glUniform1i(glGetUniformLocation(m_modelShader, "uModelTexture"), 0);
    glUniform1f(glGetUniformLocation(m_modelShader, "uTintByFogColor"), 0.0f);

    // Phase 2.3: set instanced model shader's texture uniform.
    glUseProgram(m_instancedModelShader);
    glUniform1i(glGetUniformLocation(m_instancedModelShader, "uModelTexture"), 0);

    glUseProgram(m_terrainShader);
    glUniform1i(glGetUniformLocation(m_terrainShader, "uTerrainArray"), 0);

    Vector3d fogColor = GetDistanceFogColor();
    glClearColor(fogColor.x, fogColor.y, fogColor.z, 1.0f);

    m_uploadedTerrainTextures.fill(nullptr);

    // ---- Phase 1.1: PerFrame UBO (binding 0) shared by terrain, model, and sky shaders.
    // Create the UBO once and bind all three shaders' PerFrame blocks to binding 0.
    EnsurePerFrameUBO();
    if (m_perFrameUBO) {
        const GLuint perFrameBlock_terrain = glGetUniformBlockIndex(m_terrainShader, "PerFrame");
        if (perFrameBlock_terrain != GL_INVALID_INDEX) {
            glUniformBlockBinding(m_terrainShader, perFrameBlock_terrain, 0);
        }
        const GLuint perFrameBlock_model = glGetUniformBlockIndex(m_modelShader, "PerFrame");
        if (perFrameBlock_model != GL_INVALID_INDEX) {
            glUniformBlockBinding(m_modelShader, perFrameBlock_model, 0);
        }
        // Phase 2.3: bind instanced model shader's PerFrame UBO.
        const GLuint perFrameBlock_instanced = glGetUniformBlockIndex(m_instancedModelShader, "PerFrame");
        if (perFrameBlock_instanced != GL_INVALID_INDEX) {
            glUniformBlockBinding(m_instancedModelShader, perFrameBlock_instanced, 0);
        }
        const GLuint perFrameBlock_sky = glGetUniformBlockIndex(m_skyShader, "PerFrame");
        if (perFrameBlock_sky != GL_INVALID_INDEX) {
            glUniformBlockBinding(m_skyShader, perFrameBlock_sky, 0);
        }
        // Bind the UBO to binding 0 once. The binding persists for the
        // program's lifetime; we update the data with glBufferSubData.
        glBindBufferBase(GL_UNIFORM_BUFFER, 0, m_perFrameUBO);
    }

    // ---- Phase 1.6, 1.9: cache uniform locations to eliminate per-draw
    // glGetUniformLocation calls. Sampler uniforms (uModelTexture, uTerrainArray,
    // uSkyTexture) are set once at init; tint/light uniforms are set per draw
    // using the cached location.
    m_locModelTexture    = glGetUniformLocation(m_modelShader, "uModelTexture");
    m_locModelTint       = glGetUniformLocation(m_modelShader, "uTintByFogColor");
    m_locSkyTexture      = glGetUniformLocation(m_skyShader, "uSkyTexture");
    m_locSkyViewport     = glGetUniformLocation(m_skyShader, "uViewport");
    m_locSkyVideoCenter  = glGetUniformLocation(m_skyShader, "uVideoCenter");
    m_locSkyQ            = glGetUniformLocation(m_skyShader, "uQ");
    m_locSkyP            = glGetUniformLocation(m_skyShader, "uP");
    m_locSkyR            = glGetUniformLocation(m_skyShader, "uR");
    m_locSkyTime         = glGetUniformLocation(m_skyShader, "uSkyTime");
    m_locSkyFogBase      = glGetUniformLocation(m_skyShader, "uFogBase");

    m_Initialized = true;
    PrintLog("GL: Initialize() completed successfully.\n");
    return true;
}

void GLRenderer::Shutdown()
{
#ifdef GL_PERF_HOOKS
    glperf_shutdown();
#endif

    if (!m_Initialized && !m_hrc) return;

    ShutdownTerrainPipeline();
    ShutdownModelPipeline();
    ShutdownInstancingPipeline();
    ShutdownStaticMeshPipeline();
    ShutdownSkyPipeline();
    ShutdownHudPipeline();
    ShutdownNightDesaturation();

    if (m_hrc) {
        if (wglGetCurrentContext() == m_hrc) {
            wglMakeCurrent(nullptr, nullptr);
        }
        wglDeleteContext(m_hrc);
        m_hrc = nullptr;
    }

    if (m_hdc && m_hwnd) {
        ReleaseDC(m_hwnd, m_hdc);
        m_hdc = nullptr;
    }

    if (libGL) {
        FreeLibrary(libGL);
        libGL = nullptr;
    }

    m_Initialized = false;
}

void GLRenderer::DestroyContext()
{
    if (m_hrc) {
        if (wglGetCurrentContext() == m_hrc) {
            wglMakeCurrent(nullptr, nullptr);
        }
        wglDeleteContext(m_hrc);
        m_hrc = nullptr;
    }

    if (m_hdc && m_hwnd) {
        ReleaseDC(m_hwnd, m_hdc);
        m_hdc = nullptr;
    }

    if (libGL) {
        FreeLibrary(libGL);
        libGL = nullptr;
    }
}

bool GLRenderer::InitializeTerrainPipeline()
{
    glGenVertexArrays(1, &m_terrainVAO);
    glGenBuffers(1, &m_terrainVBO);

    glBindVertexArray(m_terrainVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_terrainVBO);
    glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW);

    // Phase 1.5: packed TerrainVertex layout (32 bytes).
    //   attribute 0: vec3  aPos                (12 bytes, float)
    //   attribute 1: vec2  aTexCoord            ( 8 bytes, float)
    //   attribute 2: float aLayer               ( 4 bytes, float)
    //   attribute 3: vec4  light/fog/alpha/pad  ( 4 bytes, uint8 normalized)
    //   attribute 4: vec3  fogR/fogG/fogB       ( 3 bytes, uint8 normalized)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT,         GL_FALSE, sizeof(TerrainVertex), reinterpret_cast<void*>(offsetof(TerrainVertex, x)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT,         GL_FALSE, sizeof(TerrainVertex), reinterpret_cast<void*>(offsetof(TerrainVertex, u)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 1, GL_FLOAT,         GL_FALSE, sizeof(TerrainVertex), reinterpret_cast<void*>(offsetof(TerrainVertex, layer)));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 4, GL_UNSIGNED_BYTE, GL_TRUE,  sizeof(TerrainVertex), reinterpret_cast<void*>(offsetof(TerrainVertex, light)));
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 3, GL_UNSIGNED_BYTE, GL_TRUE,  sizeof(TerrainVertex), reinterpret_cast<void*>(offsetof(TerrainVertex, fogR)));

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    return true;
}

bool GLRenderer::InitializeModelPipeline()
{
    glGenVertexArrays(1, &m_modelVAO);
    glGenBuffers(1, &m_modelVBO);

    glBindVertexArray(m_modelVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_modelVBO);
    glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW);

    // Phase 1.4: packed ModelVertex layout (32 bytes).
    //   attribute 0: vec3  aPos                 (12 bytes, float)
    //   attribute 1: vec2  aTexCoord             ( 8 bytes, float)
    //   attribute 2: vec4  light/fog/alpha/cutout ( 4 bytes, uint8 normalized)
    //   attribute 3: vec3  fogR/fogG/fogB        ( 3 bytes, uint8 normalized)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT,         GL_FALSE, sizeof(ModelVertex), reinterpret_cast<void*>(offsetof(ModelVertex, x)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT,         GL_FALSE, sizeof(ModelVertex), reinterpret_cast<void*>(offsetof(ModelVertex, u)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE,  sizeof(ModelVertex), reinterpret_cast<void*>(offsetof(ModelVertex, light)));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 3, GL_UNSIGNED_BYTE, GL_TRUE,  sizeof(ModelVertex), reinterpret_cast<void*>(offsetof(ModelVertex, fogR)));

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    // Create 1x1 white texture for flat-color rendering (circles, overlays)
    glGenTextures(1, &m_whiteTexture);
    glBindTexture(GL_TEXTURE_2D, m_whiteTexture);
    const uint32_t white = 0xFFFFFFFF;
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, &white);
    glBindTexture(GL_TEXTURE_2D, 0);

    return true;
}

void GLRenderer::ShutdownModelPipeline()
{
    for (const auto& item : m_modelTextureCache) {
        if (item.second) {
            glDeleteTextures(1, &item.second);
        }
    }
    m_modelTextureCache.clear();

    for (const auto& item : m_bmpTextureCache) {
        if (item.second) {
            glDeleteTextures(1, &item.second);
        }
    }
    m_bmpTextureCache.clear();

    if (m_modelVBO) {
        glDeleteBuffers(1, &m_modelVBO);
        m_modelVBO = 0;
    }
    if (m_modelVAO) {
        glDeleteVertexArrays(1, &m_modelVAO);
        m_modelVAO = 0;
    }
    if (m_modelShader) {
        glDeleteProgram(m_modelShader);
        m_modelShader = 0;
    }
    // Phase 2.3: clean up instanced model shader.
    if (m_instancedModelShader) {
        glDeleteProgram(m_instancedModelShader);
        m_instancedModelShader = 0;
    }
    if (m_whiteTexture) {
        glDeleteTextures(1, &m_whiteTexture);
        m_whiteTexture = 0;
    }
    if (m_phongTexture) {
        glDeleteTextures(1, &m_phongTexture);
        m_phongTexture = 0;
    }
    if (m_envTexture) {
        glDeleteTextures(1, &m_envTexture);
        m_envTexture = 0;
    }

    m_worldModelItems.clear();
    m_transparentModelItems.clear();
    m_objectList.clear();
}

bool GLRenderer::InitializeInstancingPipeline()
{
    // Phase 2.1 + 2.3: allocate and configure the instance VBO and VAO.
    //
    // The instance VAO combines:
    //   - Static mesh VBO (per-vertex: position, normal, UV)
    //   - Instance VBO (per-instance: world matrix, light, flags)
    //
    // Per-vertex attributes (from static mesh VBO):
    //   attribute 0: vec3 aPos       (offset  0, 12 bytes)
    //   attribute 1: vec3 aNormal    (offset 12, 12 bytes)
    //   attribute 2: vec2 aTexCoord  (offset 24,  8 bytes)
    //
    // Per-instance attributes (from instance VBO, divisor=1):
    //   attribute 4: vec4 aWorldRow0      (offset  0)
    //   attribute 5: vec4 aWorldRow1      (offset 16)
    //   attribute 6: vec4 aWorldRow2      (offset 32)
    //   attribute 7: vec4 aWorldRow3      (offset 48)
    //   attribute 8: vec4 aInstanceLight  (offset 64)
    //   attribute 9: vec4 aInstanceFlags  (offset 80)

    glGenBuffers(1, &m_instanceVBO);
    glGenVertexArrays(1, &m_instanceVAO);

    glBindVertexArray(m_instanceVAO);

    // Per-vertex attributes from static mesh VBO.
    // Bind the static mesh VBO and set up per-vertex attributes.
    glBindBuffer(GL_ARRAY_BUFFER, m_staticMeshVBO);

    glEnableVertexAttribArray(0); // aPos
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(StaticMeshVertex),
                          reinterpret_cast<void*>(offsetof(StaticMeshVertex, x)));

    glEnableVertexAttribArray(1); // aNormal
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(StaticMeshVertex),
                          reinterpret_cast<void*>(offsetof(StaticMeshVertex, nx)));

    glEnableVertexAttribArray(2); // aTexCoord
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(StaticMeshVertex),
                          reinterpret_cast<void*>(offsetof(StaticMeshVertex, u)));

    // Per-instance attributes from instance VBO.
    // Bind the instance VBO and set up per-instance attributes with divisor=1.
    glBindBuffer(GL_ARRAY_BUFFER, m_instanceVBO);
    glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_STREAM_DRAW);

    // Each instance attribute is a vec4 (4 floats).
    // The attributes are at locations 4-9.
    for (GLuint loc = 4; loc <= 9; ++loc) {
        glEnableVertexAttribArray(loc);
        glVertexAttribPointer(loc, 4, GL_FLOAT, GL_FALSE, sizeof(ModelInstance),
                              reinterpret_cast<void*>(
                                  static_cast<uintptr_t>((loc - 4) * 4 * sizeof(float))));
        glVertexAttribDivisor(loc, 1); // advance once per instance
    }

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    m_instanceData.clear();
    m_instanceData.reserve(kInitialInstanceCapacity);
    m_instanceInfo.clear();

    return true;
}

void GLRenderer::ShutdownInstancingPipeline()
{
    if (m_instanceVBO) {
        glDeleteBuffers(1, &m_instanceVBO);
        m_instanceVBO = 0;
    }
    if (m_instanceVAO) {
        glDeleteVertexArrays(1, &m_instanceVAO);
        m_instanceVAO = 0;
    }
    m_instanceData.clear();
    m_instanceData.shrink_to_fit();
    m_instanceInfo.clear();
    m_instanceInfo.shrink_to_fit();
}

bool GLRenderer::InitializeStaticMeshPipeline()
{
    // Phase 2.2: allocate the static VBO/IBO pair that holds every
    // unique TModel*'s geometry. Both buffers are GL_STATIC_DRAW
    // (data doesn't change after upload). Initial capacities cover a
    // typical custom map; growth is handled in EnsureStaticMeshCapacity.

    glGenBuffers(1, &m_staticMeshVBO);
    glGenBuffers(1, &m_staticMeshIBO);

    m_staticMeshVBOCapacity = kInitialStaticMeshVBOCapacity;
    m_staticMeshIBOCapacity = kInitialStaticMeshIBOCapacity;

    glBindBuffer(GL_ARRAY_BUFFER, m_staticMeshVBO);
    glBufferData(GL_ARRAY_BUFFER, m_staticMeshVBOCapacity, nullptr, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_staticMeshIBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, m_staticMeshIBOCapacity, nullptr, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    m_staticMeshNextVertexOffset = 0;
    m_staticMeshNextIndexOffset = 0;
    m_staticMeshCache.clear();

    return true;
}

void GLRenderer::ShutdownStaticMeshPipeline()
{
    if (m_staticMeshVBO) {
        glDeleteBuffers(1, &m_staticMeshVBO);
        m_staticMeshVBO = 0;
    }
    if (m_staticMeshIBO) {
        glDeleteBuffers(1, &m_staticMeshIBO);
        m_staticMeshIBO = 0;
    }
    m_staticMeshVBOCapacity = 0;
    m_staticMeshIBOCapacity = 0;
    m_staticMeshNextVertexOffset = 0;
    m_staticMeshNextIndexOffset = 0;
    m_staticMeshCache.clear();
}

void GLRenderer::EnsureStaticMeshCapacity(size_t vertexBytes, size_t indexBytes)
{
    const bool needGrowVBO = vertexBytes > m_staticMeshVBOCapacity;
    const bool needGrowIBO = indexBytes > m_staticMeshIBOCapacity;

    if (!needGrowVBO && !needGrowIBO) {
        return;
    }

    if (needGrowVBO) {
        size_t newCapacity = m_staticMeshVBOCapacity;
        while (newCapacity < vertexBytes) newCapacity *= 2;
        m_staticMeshVBOCapacity = newCapacity;
        glBindBuffer(GL_ARRAY_BUFFER, m_staticMeshVBO);
        glBufferData(GL_ARRAY_BUFFER, m_staticMeshVBOCapacity, nullptr, GL_STATIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    if (needGrowIBO) {
        size_t newCapacity = m_staticMeshIBOCapacity;
        while (newCapacity < indexBytes) newCapacity *= 2;
        m_staticMeshIBOCapacity = newCapacity;
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_staticMeshIBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, m_staticMeshIBOCapacity, nullptr, GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    }

    // Both VBO and IBO data was orphaned; clear the cache so models
    // get re-uploaded on next access. The next-offset cursors also
    // reset to 0 so the re-upload starts from the new buffer start.
    m_staticMeshCache.clear();
    m_staticMeshNextVertexOffset = 0;
    m_staticMeshNextIndexOffset = 0;
}

GLRenderer::StaticMeshEntry GLRenderer::UploadStaticMesh(TModel* mptr)
{
    if (!mptr || !mptr->gVertex || !mptr->gFace) {
        return {};
    }

    // Cache hit: return existing entry.
    auto it = m_staticMeshCache.find(mptr);
    if (it != m_staticMeshCache.end()) {
        return it->second;
    }

    // Compute upload size: triangle list, 3 vertices + 3 indices per face.
    const size_t faceCount = static_cast<size_t>(mptr->FCount);
    const size_t vertexCount = faceCount * 3;
    const size_t indexCount = faceCount * 3;
    const size_t vertexBytes = vertexCount * sizeof(StaticMeshVertex);
    const size_t indexBytes = indexCount * sizeof(uint32_t);

    // Grow buffers if needed. This may clear the cache if growth happens.
    EnsureStaticMeshCapacity(vertexBytes, indexBytes);

    // Re-check cache after a possible growth-induced clear.
    it = m_staticMeshCache.find(mptr);
    if (it != m_staticMeshCache.end()) {
        return it->second;
    }

    // Build the upload data on the CPU.
    std::vector<StaticMeshVertex> vertices;
    vertices.reserve(vertexCount);
    std::vector<uint32_t> indices;
    indices.reserve(indexCount);

    const int texHeight = (mptr->TextureHeight > 1) ? mptr->TextureHeight : 1;

    // Phase 2.3: sfOpacity faces are alpha-tested cutouts. sfTransparent
    // faces are real blended/non-solid faces and must not force cutout handling.
    // These are per-model booleans cached in the entry so the instanced draw
    // path can set the correct shader flags.
    bool hasCutout = false;
    bool hasTransparent = false;

    for (int f = 0; f < mptr->FCount; ++f) {
        const TFace& face = mptr->gFace[f];

        if (!hasCutout && (face.Flags & sfOpacity)) {
            hasCutout = true;
        }
        if (!hasTransparent && (face.Flags & sfTransparent)) {
            hasTransparent = true;
        }

        const TPoint3d& p0Raw = mptr->gVertex[face.v1];
        const TPoint3d& p1Raw = mptr->gVertex[face.v2];
        const TPoint3d& p2Raw = mptr->gVertex[face.v3];

        const Vector2df uv0 = DecodeLegacyFaceUV(face.tax, face.tay, texHeight);
        const Vector2df uv1 = DecodeLegacyFaceUV(face.tbx, face.tby, texHeight);
        const Vector2df uv2 = DecodeLegacyFaceUV(face.tcx, face.tcy, texHeight);

        // Face normal: e1 × e2 where e1 = p1-p0, e2 = p2-p0.
        // (Not normalized — the vertex shader normalizes when needed.
        // Saves a sqrt per face for a one-time upload cost.)
        const float e1x = p1Raw.x - p0Raw.x;
        const float e1y = p1Raw.y - p0Raw.y;
        const float e1z = p1Raw.z - p0Raw.z;
        const float e2x = p2Raw.x - p0Raw.x;
        const float e2y = p2Raw.y - p0Raw.y;
        const float e2z = p2Raw.z - p0Raw.z;
        const float nx = e1y * e2z - e1z * e2y;
        const float ny = e1z * e2x - e1x * e2z;
        const float nz = e1x * e2y - e1y * e2x;

        const uint32_t baseIdx = m_staticMeshNextVertexOffset + static_cast<uint32_t>(vertices.size());

        vertices.push_back({p0Raw.x, p0Raw.y, p0Raw.z, nx, ny, nz, uv0.x, uv0.y});
        vertices.push_back({p1Raw.x, p1Raw.y, p1Raw.z, nx, ny, nz, uv1.x, uv1.y});
        vertices.push_back({p2Raw.x, p2Raw.y, p2Raw.z, nx, ny, nz, uv2.x, uv2.y});

        indices.push_back(baseIdx);
        indices.push_back(baseIdx + 1);
        indices.push_back(baseIdx + 2);
    }

    // Upload to the static VBO.
    const uint32_t vboOffset = m_staticMeshNextVertexOffset;
    glBindBuffer(GL_ARRAY_BUFFER, m_staticMeshVBO);
    glBufferSubData(GL_ARRAY_BUFFER, vboOffset * sizeof(StaticMeshVertex), vertexBytes, vertices.data());
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    // Upload to the static IBO.
    const uint32_t iboOffset = m_staticMeshNextIndexOffset;
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_staticMeshIBO);
    glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, iboOffset * sizeof(uint32_t), indexBytes, indices.data());
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    // Cache the entry.
    StaticMeshEntry entry;
    entry.baseVertex = vboOffset;
    entry.baseIndex = iboOffset;
    entry.vertexCount = static_cast<uint32_t>(vertexCount);
    entry.indexCount = static_cast<uint32_t>(indexCount);
    entry.hasCutout = hasCutout;
    entry.hasTransparent = hasTransparent;
    m_staticMeshCache[mptr] = entry;

    // Advance the next-offset cursors.
    m_staticMeshNextVertexOffset += static_cast<uint32_t>(vertexCount);
    m_staticMeshNextIndexOffset += static_cast<uint32_t>(indexCount);

    return entry;
}

const GLRenderer::StaticMeshEntry* GLRenderer::GetStaticMeshEntry(const TModel* mptr) const
{
    if (!mptr) {
        return nullptr;
    }
    auto it = m_staticMeshCache.find(mptr);
    if (it == m_staticMeshCache.end()) {
        return nullptr;
    }
    return &it->second;
}

void GLRenderer::ShutdownTerrainPipeline()
{
    if (m_terrainTextureArray) {
        glDeleteTextures(1, &m_terrainTextureArray);
        m_terrainTextureArray = 0;
    }
    if (m_terrainVBO) {
        glDeleteBuffers(1, &m_terrainVBO);
        m_terrainVBO = 0;
    }
    if (m_terrainVAO) {
        glDeleteVertexArrays(1, &m_terrainVAO);
        m_terrainVAO = 0;
    }
    if (m_terrainShader) {
        glDeleteProgram(m_terrainShader);
        m_terrainShader = 0;
    }

    m_terrainVertices.clear();
    m_waterVertices.clear();
    m_uploadedTerrainTextures.fill(nullptr);
}

// ==========================================================================
// PerFrame UBO (Phase 1.1 + Phase 2.4)
// Shared by terrain and model shaders. std140 layout:
//   offset 0   : mat4  uProjection           (64 bytes)
//   offset 64  : vec2  uFogRange             ( 8 bytes)  (fadeStart, distance)
//   offset 72  :        (pad to vec3 align)  ( 8 bytes)
//   offset 80  : vec3  uDistanceFogColor     (12 bytes)
//   offset 92  : float uForceFog             ( 4 bytes)
//   offset 96  : vec3  uFogColor             (12 bytes)
//   offset 108 :        (pad to mat4 align)  ( 4 bytes)  -- Phase 2.4
//   offset 112 : mat4  uView                 (64 bytes)  -- Phase 2.4
//   total 192 bytes.
// Phase 2.4: uView is identity for legacy shaders (they receive view-
// space vertices).  The instanced shader uses it as a placeholder for
// the world→view split; currently identity, to be refined in 2.5+.
// ==========================================================================

void GLRenderer::EnsurePerFrameUBO()
{
    if (m_perFrameUBOInitialized) {
        return;
    }
    constexpr GLsizeiptr kUBOBytes = 192;  // Phase 2.4: +64 bytes for uView, +16 bytes for uWaterAlphaFade
    glGenBuffers(1, &m_perFrameUBO);
    glBindBuffer(GL_UNIFORM_BUFFER, m_perFrameUBO);
    glBufferData(GL_UNIFORM_BUFFER, kUBOBytes, nullptr, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
    m_perFrameUBOInitialized = true;
}

void GLRenderer::UpdatePerFrameUBO()
{
    UpdatePerFrameUBO(BuildLegacyProjection());
}

void GLRenderer::UpdatePerFrameUBO(const std::array<float, 16>& projection,
                                   float waterAlphaEnabled,
                                   float waterAlphaFadeStart,
                                   float waterAlphaFadeEnd,
                                   float waterAlphaFadeStep)
{
    if (!m_perFrameUBO || !m_perFrameUBOInitialized) {
        return;
    }

    // Caller-provided projection. World passes use BuildLegacyProjection();
    // near-model passes (weapon viewmodel + its Phong/env effects) use the
    // cached near-model projection from RenderNearModel; NDC-space passes
    // (full-screen glare, HUD circles) use the identity matrix. Previously
    // the no-arg overload was the only entry point and the caller-supplied
    // projection in DrawModelVertices was ignored, which broke the env-map
    // alignment on the weapon and the sun glare overlay (regression from
    // 6bda7780).
    // near-model path (wind/compass/weapon viewmodels) changes between
    // draws. BuildLegacyProjection is cheap enough that recomputing per
    // draw costs microseconds, vs. the visual regression of a stale
    // matrix.
    m_cachedProjection = projection;

    // Fog range only depends on ctViewR, but ctViewR can change at runtime
    // (widescreen FOV), so we recompute unconditionally too.
    m_cachedFogStart    = static_cast<float>(ctViewR) * 192.0f;
    m_cachedFogDistance = static_cast<float>(ctViewR) * 256.0f;

    // Distance fog color is read fresh from GetDistanceFogColor() (it can
    // change every frame as the smoothed sky color updates).
    const Vector3d distFogColor = GetDistanceFogColor();
    m_cachedDistanceFogColor[0] = distFogColor.x;
    m_cachedDistanceFogColor[1] = distFogColor.y;
    m_cachedDistanceFogColor[2] = distFogColor.z;

    // Sky fog color tracks the smoothed value in m_smoothedSkyFogColor.
    m_cachedFogColor[0] = m_smoothedSkyFogColor.x;
    m_cachedFogColor[1] = m_smoothedSkyFogColor.y;
    m_cachedFogColor[2] = m_smoothedSkyFogColor.z;

    m_cachedForceFog = IsUnderwater() ? 1.0f : 0.0f;

    // Phase 2.4: pack into a 48-float (192-byte) buffer.
    //   offset 0   : mat4 uProjection           (16 floats)
    //   offset 64  : vec2 uFogRange             ( 2 floats)
    //   offset 72  :        (pad to vec3 align) ( 2 floats)
    //   offset 80  : vec3 uDistanceFogColor     ( 3 floats)
    //   offset 92  : float uForceFog            ( 1 float)
    //   offset 96  : vec3 uFogColor             ( 3 floats)
    //   offset 108 :        (pad to mat4 align) ( 1 float)   -- Phase 2.4
    //   offset 112 : mat4 uView                 (16 floats)   -- Phase 2.4
    //   offset 176 : vec4 uWaterAlphaFade       ( 4 floats)   -- x=start, y=end, z=enabled, w=fade step
    std::array<float, 48> data{};
    std::memcpy(&data[0],  m_cachedProjection.data(), 16 * sizeof(float));
    data[16] = m_cachedFogStart;     // uFogRange.x
    data[17] = m_cachedFogDistance;  // uFogRange.y
    data[18] = 0.0f;                 // pad to vec3 alignment
    data[19] = 0.0f;                 // pad to vec3 alignment
    data[20] = m_cachedDistanceFogColor[0];
    data[21] = m_cachedDistanceFogColor[1];
    data[22] = m_cachedDistanceFogColor[2];
    data[23] = m_cachedForceFog;
    data[24] = m_cachedFogColor[0];
    data[25] = m_cachedFogColor[1];
    data[26] = m_cachedFogColor[2];
    data[27] = 0.0f;                 // pad to mat4 alignment (Phase 2.4)
    data[28] = 1.0f; data[29] = 0.0f; data[30] = 0.0f; data[31] = 0.0f;  // col0
    data[32] = 0.0f; data[33] = 1.0f; data[34] = 0.0f; data[35] = 0.0f;  // col1
    data[36] = 0.0f; data[37] = 0.0f; data[38] = 1.0f; data[39] = 0.0f;  // col2
    data[40] = 0.0f; data[41] = 0.0f; data[42] = 0.0f; data[43] = 1.0f;  // col3
    // Phase 2.4: uWaterAlphaFade packed after uView.
    data[44] = waterAlphaFadeStart;   // uWaterAlphaFade.x
    data[45] = waterAlphaFadeEnd;     // uWaterAlphaFade.y
    data[46] = waterAlphaEnabled;     // uWaterAlphaFade.z
    data[47] = waterAlphaFadeStep;    // uWaterAlphaFade.w

    glBindBuffer(GL_UNIFORM_BUFFER, m_perFrameUBO);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, data.size() * sizeof(float), data.data());
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

void GLRenderer::SetWaterAlphaFade(float enabled, float fadeStart, float fadeEnd, float fadeStep)
{
    if (!m_perFrameUBO) {
        return;
    }

    const float data[4] = {
        fadeStart,
        fadeEnd,
        enabled,
        fadeStep
    };

    glBindBuffer(GL_UNIFORM_BUFFER, m_perFrameUBO);
    glBufferSubData(GL_UNIFORM_BUFFER, 176, sizeof(data), data);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

void GLRenderer::BeginTerrainFrame()
{
    m_terrainVertices.clear();
    // m_waterVertices is cleared in BeginWaterFrame (called by RenderWater).
    // Clearing here too was redundant — RenderGround never touches water.
}

void GLRenderer::BeginWaterFrame()
{
    m_waterVertices.clear();
    m_waterUsedLayers.fill(false);
}

void GLRenderer::RenderWaterSurface()
{
    if (m_waterVertices.empty()) {
        return;
    }

    EnsureTerrainTextureArray();

    // m_waterUsedLayers is maintained incrementally by AppendWaterTriangle
    // (called from CollectWaterTileFast / CollectWaterTile / CollectWaterTile2),
    // so we no longer need to scan m_waterVertices to discover which layers
    // are in use. The Textures[layer] check mirrors the previous behavior:
    // a vertex referencing a null texture pointer should be skipped.
    for (int layer = 0; layer < kMaxTerrainTextureLayers; ++layer) {
        if (!m_waterUsedLayers[layer] || !Textures[layer]) {
            continue;
        }
        if (m_uploadedTerrainTextures[layer] != Textures[layer].get()) {
            UploadTerrainLayer(layer, *Textures[layer]);
            m_uploadedTerrainTextures[layer] = Textures[layer].get();
        }
    }

    const auto projection = BuildLegacyProjection();
    // Bake water alpha fade into the UBO update — saves a separate
    // glBufferSubData call vs. UpdatePerFrameUBO() + SetWaterAlphaFade().
    UpdatePerFrameUBO(projection, 1.0f,
                      static_cast<float>((ctViewR - 8) << 8),
                      256.0f * static_cast<float>(ctViewR - 4),
                      765.0f);
    glUseProgram(m_terrainShader);
#ifdef GL_PERF_HOOKS
    GL_PERF_STATE_CHANGE();
#endif
    // Per-pixel distance fog: uFogRange is the (fadeStart, distance) pair,
    // now sourced from the PerFrame UBO. The shader interpolates between
    // the per-vertex volumetric fog color and the global horizon color
    // (uDistanceFogColor, also in the UBO), keeping local volumes from
    // tinting the far horizon.

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D_ARRAY, m_terrainTextureArray);
    glBindVertexArray(m_terrainVAO);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    DrawVertexBatch(m_waterVertices);
    SetWaterAlphaFade(0.0f, static_cast<float>((ctViewR - 8) << 8), 256.0f * static_cast<float>(ctViewR - 4), 765.0f);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);

    glBindVertexArray(0);
}

GLuint GLRenderer::UploadModelTexture(TModel* mptr)
{
    if (!mptr || !mptr->lpTexture) {
        return 0;
    }

    const auto cached = m_modelTextureCache.find(mptr);
    if (cached != m_modelTextureCache.end()) {
        return cached->second;
    }

    GLuint texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);

    const int height = mptr->TextureHeight > 1 ? mptr->TextureHeight : 256;
    const int width = 256;
    const size_t availableTexels = static_cast<size_t>(mptr->TextureSize) / sizeof(WORD);
    const size_t texelCount = static_cast<size_t>(width) * height;
    if (availableTexels < texelCount) {
        return 0;
    }

    std::vector<uint32_t> expanded(texelCount);
    for (size_t i = 0; i < texelCount; ++i) {
        expanded[i] = Expand1555to8888(mptr->lpTexture[i]);
    }

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, expanded.data());
    m_modelTextureCache[mptr] = texture;
    return texture;
}

GLuint GLRenderer::UploadBMPModelTexture(TBMPModel* mptr)
{
    if (!mptr || !mptr->lpTexture) {
        return 0;
    }

    const auto cached = m_bmpTextureCache.find(mptr);
    if (cached != m_bmpTextureCache.end()) {
        return cached->second;
    }

    GLuint texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);

    std::vector<uint32_t> expanded(128 * 128);
    for (size_t i = 0; i < expanded.size(); ++i) {
        expanded[i] = Expand1555to8888(mptr->lpTexture[i]);
    }

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 128, 128, 0, GL_RGBA, GL_UNSIGNED_BYTE, expanded.data());
    m_bmpTextureCache[mptr] = texture;
    return texture;
}

GLuint GLRenderer::UploadPictureTexture(const TPicture& pic)
{
    if (!pic.lpImage || pic.W <= 0 || pic.H <= 0) {
        return 0;
    }

    const size_t texelCount = static_cast<size_t>(pic.W) * static_cast<size_t>(pic.H);
    std::vector<uint8_t> rgba(texelCount * 4);
    for (size_t i = 0; i < texelCount; ++i) {
        const unsigned short c = pic.lpImage[i];
        rgba[i * 4 + 0] = static_cast<uint8_t>(((c >> 10) & 0x1F) * 255 / 31);
        rgba[i * 4 + 1] = static_cast<uint8_t>(((c >> 5) & 0x1F) * 255 / 31);
        rgba[i * 4 + 2] = static_cast<uint8_t>((c & 0x1F) * 255 / 31);
        rgba[i * 4 + 3] = 255;
    }

    GLuint texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, pic.W, pic.H, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

    return texture;
}

bool GLRenderer::BuildModelEffectVertices(std::vector<ModelVertex>& outVertices,
                                          TModel* mptr,
                                          float x0,
                                          float y0,
                                          float z0,
                                          float al,
                                          float bt,
                                          int flagMask,
                                          const Vector3d& fogColor) const
{
    if (!mptr || !mptr->gVertex || !mptr->gFace || !PhongMapping) {
        return false;
    }

    const float ca = std::cos(al);
    const float sa = std::sin(al);
    const float cb = std::cos(bt);
    const float sb = std::sin(bt);
    // Phase 1.13: local vector (was static thread_local). The renderer
    // is single-threaded so the static thread_local was misleading; the
    // heap traffic at ~100 calls/frame is trivial.
    std::vector<Vector3d> transformed;
    transformed.reserve(mptr->VCount);

    bool anyVisible = false;
    for (int i = 0; i < mptr->VCount; ++i) {
        const Vector3d position = TransformModelVertex(mptr->gVertex[i], x0, y0, z0, ca, sa, cb, sb);
        transformed.push_back(position);
        if (position.z < kModelNearClip) {
            anyVisible = true;
        }
    }

    if (!anyVisible) {
        return false;
    }

    outVertices.clear();
    outVertices.reserve(static_cast<size_t>(mptr->FCount) * 3);

    for (int i = 0; i < mptr->FCount; ++i) {
        const TFace& face = mptr->gFace[i];
        if (!(face.Flags & flagMask)) {
            continue;
        }

        if (ShouldCullModelFace(face.Flags, transformed[face.v1], transformed[face.v2], transformed[face.v3])) {
            continue;
        }

        const ModelClipVertex v0 = {
            transformed[face.v1],
            { PhongMapping[face.v1].x / 256.0f, PhongMapping[face.v1].y / 256.0f },
            255
        };
        const ModelClipVertex v1 = {
            transformed[face.v2],
            { PhongMapping[face.v2].x / 256.0f, PhongMapping[face.v2].y / 256.0f },
            255
        };
        const ModelClipVertex v2 = {
            transformed[face.v3],
            { PhongMapping[face.v3].x / 256.0f, PhongMapping[face.v3].y / 256.0f },
            255
        };

        // Phase 1.13: local vector (was static thread_local).
        std::vector<ModelClipVertex> clipped;
        ClipTriangleAgainstNearPlane(v0, v1, v2, clipped);
        for (size_t j = 1; j + 1 < clipped.size(); ++j) {
            // Phase 1.4: pack the float fields (light, fog, alpha, cutout)
            // and the fog color vec3 to uint8. The water surface always
            // uses light=255, fog.amount=0, alpha=1, cutout=0, so the
            // packed bytes are constant and can be computed once.
            const uint8_t lightByte  = 255;
            const uint8_t fog0Byte   = 0;
            const uint8_t alphaByte  = 255;
            const uint8_t cutoutByte = 0;
            const uint8_t fogRByte   = Float01ToByte(fogColor.x);
            const uint8_t fogGByte   = Float01ToByte(fogColor.y);
            const uint8_t fogBByte   = Float01ToByte(fogColor.z);
            const uint8_t pad[5] = {0, 0, 0, 0, 0};
            const ModelVertex out0 = {
                clipped[0].position.x,
                clipped[0].position.y,
                clipped[0].position.z,
                clipped[0].uv.x,
                clipped[0].uv.y,
                lightByte, fog0Byte, alphaByte, cutoutByte,
                fogRByte, fogGByte, fogBByte,
                {0, 0, 0, 0, 0}
            };
            const ModelVertex out1 = {
                clipped[j].position.x,
                clipped[j].position.y,
                clipped[j].position.z,
                clipped[j].uv.x,
                clipped[j].uv.y,
                lightByte, fog0Byte, alphaByte, cutoutByte,
                fogRByte, fogGByte, fogBByte,
                {0, 0, 0, 0, 0}
            };
            const ModelVertex out2 = {
                clipped[j + 1].position.x,
                clipped[j + 1].position.y,
                clipped[j + 1].position.z,
                clipped[j + 1].uv.x,
                clipped[j + 1].uv.y,
                lightByte, fog0Byte, alphaByte, cutoutByte,
                fogRByte, fogGByte, fogBByte,
                {0, 0, 0, 0, 0}
            };
            outVertices.push_back(out0);
            outVertices.push_back(out1);
            outVertices.push_back(out2);
        }
    }

    return !outVertices.empty();
}

bool GLRenderer::BuildModelDrawItem(ModelDrawItem& outItem,
                                    TModel* mptr,
                                    float x0,
                                    float y0,
                                    float z0,
                                    int light,
                                    int vt,
                                    float al,
                                    float bt,
                                    bool waterClipped,
                                    bool disableFog,
                                    bool clippedVariant,
                                    bool additive) const
{
    if (!mptr || !mptr->lpTexture || !mptr->gVertex || !mptr->gFace) {
        return false;
    }

    const float ca = std::cos(al);
    const float sa = std::sin(al);
    const float cb = std::cos(bt);
    const float sb = std::sin(bt);
    // Phase 1.13: local vectors (were static thread_local). The
    // renderer is single-threaded; heap traffic at ~100 calls/frame
    // is trivial.
    std::vector<Vector3d> transformed;
    std::vector<Vector3d> unrotated;
    transformed.reserve(mptr->VCount);
    unrotated.reserve(mptr->VCount);

    // C1 technique for model fog. The view rotation in
    // RenderMappedObject is RotateVector = R_x(CameraBeta) * R_y(CameraAlpha)
    // (Math.cpp:52, applied at GLRenderer.cpp:1227). The model center
    // (x0, y0, z0) is in view space, so its world-relative position is
    //     unrotatedCenter = R_view^-1 * viewCenter = R_y^-1 * R_x^-1 * viewCenter
    // Then for each model vertex, the "unrotated" position used for the
    // fog calculation is unrotatedCenter + gVertex[i] -- the world-
    // relative position of the model center plus the model vertex's raw
    // offset in model space. The model's own rotation (al, bt) is
    // intentionally NOT applied here, matching Carnivores 1 exactly
    // (see Carnivores1/Hunt/GLRenderer.cpp:1138, 1148). For small
    // objects this is a good approximation, and CalcFogLevel is smooth
    // enough that the small error is invisible in practice.
    const float ucY = ::cb * y0 + ::sb * z0;
    const float ucZ = ::cb * z0 - ::sb * y0;
    const Vector3d unrotatedCenter = {
        ::ca * x0 - ::sa * ucZ,
        ucY,
        ::sa * x0 + ::ca * ucZ
    };

    bool anyVisible = false;
    for (int i = 0; i < mptr->VCount; ++i) {
        // View-space position of the vertex (with model rotation applied).
        transformed.push_back(TransformModelVertex(mptr->gVertex[i], x0, y0, z0, ca, sa, cb, sb));
        // Unrotated position: unrotatedCenter + gVertex[i] (identity
        // model rotation). The fog for this vertex is computed from
        // this position in the triangle loop via SampleFogAtPoint.
        unrotated.push_back(TransformModelVertex(mptr->gVertex[i], unrotatedCenter.x, unrotatedCenter.y, unrotatedCenter.z, 1.0f, 0.0f, 1.0f, 0.0f));

        if (transformed.back().z < kModelNearClip) {
            anyVisible = true;
        }
    }

    if (!anyVisible) {
        return false;
    }

    outItem = ModelDrawItem();
    outItem.texture = 0;
    outItem.additive = additive;
    const Vector3d center = {x0, y0, z0};
    outItem.distance = VectorLengthSq(center);
    const size_t reserveCount = static_cast<size_t>(mptr->FCount) * 3;
    outItem.opaqueVertices.reserve(reserveCount);
    outItem.cutoutVertices.reserve(reserveCount);
    outItem.transparentVertices.reserve(reserveCount);

    const int lightIndex = std::clamp(vt, 0, 3);
    const float baseLight = static_cast<float>(light);
    const float baseAlpha = std::clamp((255.0f - static_cast<float>(GlassL)) / 255.0f, 0.0f, 1.0f);
    const float transparentScale = clippedVariant ? (0x70 / 255.0f) : (0x80 / 255.0f);
    const bool forceDistanceBlend = baseAlpha < 0.999f;

    auto appendTriangle = [&](const ModelClipVertex& a,
                              const ModelClipVertex& b,
                              const ModelClipVertex& c,
                              const Vector3d& uA,
                              const Vector3d& uB,
                              const Vector3d& uC,
                              bool transparent,
                              bool cutout) {
        // Per-triangle-vertex fog, exactly like Carnivores 1. The fog
        // is computed from the *original* face unrotated positions
        // (not interpolated during clipping -- matches C1 and is fine
        // because fog is a smooth function of position).
        const FogSample fogA = SampleFogAtPoint(uA, disableFog);
        const FogSample fogB = SampleFogAtPoint(uB, disableFog);
        const FogSample fogC = SampleFogAtPoint(uC, disableFog);
        const float alpha = transparent ? baseAlpha * transparentScale : baseAlpha;
        const float cutoutValue = cutout ? 1.0f : 0.0f;
        const bool blended = transparent || forceDistanceBlend;
        std::vector<ModelVertex>& target = blended ? outItem.transparentVertices : (cutout ? outItem.cutoutVertices : outItem.opaqueVertices);
        // Phase 1.4: pack light/fog/alpha/cutout as uint8 (driver normalizes
        // them back to [0,1] in the vertex shader) and the per-vertex fog
        // color as a vec3 of uint8. The float->uint8 conversion is the only
        // CPU cost of the new layout; it's a single clamp+multiply+cast.
        // Each of a, b, c gets its own light byte -- using a single
        // lightByte for all three causes flat shading (every triangle
        // would inherit vertex a's lighting).
        const uint8_t lightAByte = Light255ToByte(a.light);
        const uint8_t lightBByte = Light255ToByte(b.light);
        const uint8_t lightCByte = Light255ToByte(c.light);
        const uint8_t fogAByte   = Float01ToByte(fogA.amount);
        const uint8_t fogAR      = Float01ToByte(fogA.color.x);
        const uint8_t fogAG      = Float01ToByte(fogA.color.y);
        const uint8_t fogAB      = Float01ToByte(fogA.color.z);
        const uint8_t alphaByte  = Float01ToByte(alpha);
        const uint8_t cutoutByte = CutoutToByte(cutout);
        const uint8_t fogBByte   = Float01ToByte(fogB.amount);
        const uint8_t fogBR      = Float01ToByte(fogB.color.x);
        const uint8_t fogBG      = Float01ToByte(fogB.color.y);
        const uint8_t fogBB      = Float01ToByte(fogB.color.z);
        const uint8_t fogCByte   = Float01ToByte(fogC.amount);
        const uint8_t fogCR      = Float01ToByte(fogC.color.x);
        const uint8_t fogCG      = Float01ToByte(fogC.color.y);
        const uint8_t fogCB      = Float01ToByte(fogC.color.z);
        target.push_back({a.position.x, a.position.y, a.position.z, a.uv.x, a.uv.y, lightAByte, fogAByte, alphaByte, cutoutByte, fogAR, fogAG, fogAB, {0,0,0,0,0}});
        target.push_back({b.position.x, b.position.y, b.position.z, b.uv.x, b.uv.y, lightBByte, fogBByte, alphaByte, cutoutByte, fogBR, fogBG, fogBB, {0,0,0,0,0}});
        target.push_back({c.position.x, c.position.y, c.position.z, c.uv.x, c.uv.y, lightCByte, fogCByte, alphaByte, cutoutByte, fogCR, fogCG, fogCB, {0,0,0,0,0}});
    };
    // Phase 1.13: local vector (was static thread_local).
    std::vector<ModelClipVertex> polygon;
    polygon.reserve(4);
    polygon.clear();

    for (int f = 0; f < mptr->FCount; ++f) {
        const TFace& face = mptr->gFace[f];
        const Vector3d& p0 = transformed[face.v1];
        const Vector3d& p1 = transformed[face.v2];
        const Vector3d& p2 = transformed[face.v3];

        if (ShouldCullModelFace(face.Flags, p0, p1, p2)) {
            continue;
        }

        const float l0 = std::clamp(baseLight + mptr->VLight[lightIndex][face.v1], 0.0f, 255.0f);
        const float l1 = std::clamp(baseLight + mptr->VLight[lightIndex][face.v2], 0.0f, 255.0f);
        const float l2 = std::clamp(baseLight + mptr->VLight[lightIndex][face.v3], 0.0f, 255.0f);

        const int texHeight = (mptr->TextureHeight > 1) ? mptr->TextureHeight : 1;
        // fp_conv() in CorrectModel already converted int UVs to float pixel coords.
        // C1's ModelClipVertex carries only position/uv/light: the per-vertex
        // fog is recomputed in appendTriangle from the original face's
        // unrotated positions (unrotated[face.v1/2/3]) rather than
        // interpolated through the water clipper. Match that here so the
        // build compiles and the model fog behavior is identical to C1.
        ModelClipVertex v0{p0, DecodeLegacyFaceUV(face.tax, face.tay, texHeight), l0};
        ModelClipVertex v1{p1, DecodeLegacyFaceUV(face.tbx, face.tby, texHeight), l1};
        ModelClipVertex v2{p2, DecodeLegacyFaceUV(face.tcx, face.tcy, texHeight), l2};

        if (waterClipped) {
            ClipTriangleAgainstWater(v0, v1, v2, polygon);
            if (polygon.size() < 3) {
                continue;
            }
        } else {
            polygon.clear();
            polygon.reserve(3);
            polygon.push_back(v0);
            polygon.push_back(v1);
            polygon.push_back(v2);
        }

        const bool isFaceTransparent = (face.Flags & sfTransparent) != 0;
        const bool isFaceAlphaTest = (face.Flags & sfOpacity) != 0;
        const bool cutout = isFaceAlphaTest;
        for (size_t i = 1; i + 1 < polygon.size(); ++i) {
            // Pass the original face unrotated positions to
            // appendTriangle so the fog is computed from the
            // un-clipped vertices (matches C1 behavior).
            appendTriangle(polygon[0], polygon[i], polygon[i + 1],
                           unrotated[face.v1], unrotated[face.v2], unrotated[face.v3],
                           isFaceTransparent, cutout);
        }
    }

    return !outItem.opaqueVertices.empty() || !outItem.cutoutVertices.empty() || !outItem.transparentVertices.empty();
}

void GLRenderer::DrawModelVertices(GLuint texture,
                                   const std::vector<ModelVertex>& vertices,
                                   const std::array<float, 16>& projection,
                                   bool depthTest,
                                   bool enableBlend,
                                   bool additive,
                                   bool tintByFogColor)
{
    if (!m_modelShader || texture == 0 || vertices.empty()) {
        return;
    }

    UpdatePerFrameUBO(projection);
    glUseProgram(m_modelShader);
#ifdef GL_PERF_HOOKS
    GL_PERF_STATE_CHANGE();
#endif
    // uProjection is now in the PerFrame UBO (Phase 1.1). uTintByFogColor
    // stays a per-draw uniform; location cached at Initialize() (1.6).
    glUniform1f(m_locModelTint, tintByFogColor ? 1.0f : 0.0f);

    if (depthTest) {
        glEnable(GL_DEPTH_TEST);
#ifdef GL_PERF_HOOKS
        GL_PERF_STATE_CHANGE();
#endif
        if (!enableBlend) {
            glDepthMask(GL_TRUE);
#ifdef GL_PERF_HOOKS
            GL_PERF_STATE_CHANGE();
#endif
        }
    } else {
        glDisable(GL_DEPTH_TEST);
#ifdef GL_PERF_HOOKS
        GL_PERF_STATE_CHANGE();
#endif
        glDepthMask(GL_FALSE);
#ifdef GL_PERF_HOOKS
        GL_PERF_STATE_CHANGE();
#endif
    }

    if (enableBlend) {
        glEnable(GL_BLEND);
#ifdef GL_PERF_HOOKS
        GL_PERF_STATE_CHANGE();
#endif
        if (additive) {
            glBlendFunc(GL_SRC_ALPHA, GL_ONE);  // additive — used by water circles
        } else {
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        }
#ifdef GL_PERF_HOOKS
        GL_PERF_STATE_CHANGE();
#endif
        glDepthMask(GL_FALSE);
#ifdef GL_PERF_HOOKS
        GL_PERF_STATE_CHANGE();
#endif
    } else {
        glDisable(GL_BLEND);
#ifdef GL_PERF_HOOKS
        GL_PERF_STATE_CHANGE();
#endif
    }

    glActiveTexture(GL_TEXTURE0);
    if (texture != m_lastBoundModelTexture) {
        glBindTexture(GL_TEXTURE_2D, texture);
        m_lastBoundModelTexture = texture;
#ifdef GL_PERF_HOOKS
        GL_PERF_TEXTURE_BIND(texture);
#endif
    }
    glBindVertexArray(m_modelVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_modelVBO);
    const GLsizeiptr vertexSize = static_cast<GLsizeiptr>(vertices.size() * sizeof(ModelVertex));
    glBufferData(GL_ARRAY_BUFFER, vertexSize, nullptr, GL_STREAM_DRAW);
    glBufferSubData(GL_ARRAY_BUFFER, 0, vertexSize, vertices.data());
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertices.size()));
#ifdef GL_PERF_HOOKS
    GL_PERF_DRAW(static_cast<uint32_t>(vertices.size()) / 3);
#endif
    glBindVertexArray(0);

    glDepthMask(GL_TRUE);
#ifdef GL_PERF_HOOKS
    GL_PERF_STATE_CHANGE();
#endif
    if (!depthTest) {
        glEnable(GL_DEPTH_TEST);
#ifdef GL_PERF_HOOKS
        GL_PERF_STATE_CHANGE();
#endif
    }
    if (enableBlend) {
        glDisable(GL_BLEND);
#ifdef GL_PERF_HOOKS
        GL_PERF_STATE_CHANGE();
#endif
    }
}

void GLRenderer::RenderWorldModels()
{
    if (m_worldModelItems.empty()) {
        return;
    }

    const auto projection = BuildLegacyProjection();

    // Phase 1.7: bucket items by (pass, texture, additive) and merge their
    // vertex lists into one draw call per bucket. One draw call per (texture,
    // pass) group instead of one per item, which collapses the per-item
    // glBindTexture + glBufferData(orphan) + glBufferSubData round-trips
    // into a single round-trip per group.
    //
    //   pass:     0 = opaque, 1 = cutout, 2 = transparent
    //   texture:  GL texture handle
    //   additive: true for transparent items with GL_BLEND_FUNC(SRC_ALPHA, ONE)
    //
    // Within the transparent pass, items are appended in their original
    // dispatch order (which is back-to-front for transparent), so the
    // within-texture order is still back-to-front. Across textures the order
    // becomes texture-handle order, which can put a far item of texture A
    // before a near item of texture B -- the same tie-breaking the original
    // dispatch order had for items of equal distance, just made explicit.
    struct BucketKey {
        int pass;
        GLuint texture;
        bool additive;
        bool operator<(const BucketKey& o) const {
            if (pass != o.pass) return pass < o.pass;
            if (texture != o.texture) return texture < o.texture;
            // Within transparent, additive draws after alpha-blend so the
            // additive pass can layer on top.
            if (pass == 2) return !additive && o.additive;
            return false;
        }
    };

    std::map<BucketKey, std::vector<ModelVertex>> buckets;

    for (const ModelDrawItem& item : m_worldModelItems) {
        if (!item.opaqueVertices.empty()) {
            auto& v = buckets[{0, item.texture, false}];
            v.insert(v.end(), item.opaqueVertices.begin(), item.opaqueVertices.end());
        }
        if (!item.cutoutVertices.empty()) {
            auto& v = buckets[{1, item.texture, false}];
            v.insert(v.end(), item.cutoutVertices.begin(), item.cutoutVertices.end());
        }
        if (!item.transparentVertices.empty()) {
            auto& v = buckets[{2, item.texture, item.additive}];
            v.insert(v.end(), item.transparentVertices.begin(), item.transparentVertices.end());
        }
    }

    for (const auto& [key, verts] : buckets) {
        if (verts.empty()) {
            continue;
        }

        const bool enableBlend = (key.pass == 2);
        const bool additive     = (key.pass == 2) && key.additive;
        DrawModelVertices(key.texture, verts, projection, true, enableBlend, additive);
    }

    m_worldModelItems.clear();
    m_transparentModelItems.clear();
}



void GLRenderer::RenderObject(int x, int y)
{
    if (x < 0 || y < 0 || x >= ctMapSize || y >= ctMapSize) {
        return;
    }
    if (OMap[y][x] == 255 || !MODELS) {
        return;
    }
    // Safety cap.  With the dedup fix in CollectTerrainTile2 (only the
    // primary cell calls RenderObject), each cell is visited at most once
    // per frame.  Dense custom maps at max view distance may still push
    // beyond 8K unique objects — 32K is a generous upper bound (~256 KB
    // in m_objectList, ~3 MB in m_instanceData).
    if (m_objectList.size() >= 32768) {
        static int hitCount = 0;
        ++hitCount;
        if (hitCount <= 20 || hitCount % 100 == 0) {
            char buf[128];
            sprintf(buf, "WARNING: m_objectList hit 32768 cap (%zu entries); objects dropped (hit #%d)\n",
                    m_objectList.size(), hitCount);
            PrintLogVerbose(buf);
        }
        return;
    }

    m_objectList.push_back({x, y});
}

void GLRenderer::RenderMappedObject(int x, int y)
{
    const int ob = OMap[y][x];
    if (!MObjects[ob].model) {
        return;
    }

    const int FI = (FMap[y][x] >> 2) & 3;
    const float fi = CameraAlpha + static_cast<float>(FI) * 2.0f * pi / 4.0f;

    int mlight;
    if (MObjects[ob].info.flags & ofDEFLIGHT) {
        mlight = MObjects[ob].info.DefLight;
    } else if (MObjects[ob].info.flags & ofGRNDLIGHT) {
        mlight = 128;
        CalcModelGroundLight(MObjects[ob].model.get(), x * 256 + 128, y * 256 + 128, FI);
    } else {
        mlight = -(RandomMap[y & 31][x & 31] >> 5) + (LMap[y][x] >> 1) + 96;
    }

    mlight = std::clamp(mlight, 64, 192);

    Vector3d pos;
    pos.x = x * 256 + 128 - CameraX;
    pos.z = y * 256 + 128 - CameraZ;
    pos.y = static_cast<float>(HMapO[y][x]) * ctHScale - CameraY;

    const float distanceSq = VectorLengthSq(pos);
    if (pos.y + MObjects[ob].info.YHi < (HMap[y][x] + HMap[y + 1][x + 1]) / 2 * ctHScale - CameraY) {
        return;
    }

    waterclip = false;
    if (!IsUnderwater() && (FMap[y][x] & fmWaterA) && HMapO[y][x] < WaterList[WMap[y][x]].wlevel) {
        if (WaterList[WMap[y][x]].wlevel * ctHScale > HMapO[y][x] * ctHScale + MObjects[ob].info.YHi) {
            return;
        }

        waterclipbase = pos;
        waterclipbase.y = WaterList[WMap[y][x]].wlevel * ctHScale - CameraY;
        waterclipbase = RotateVector(waterclipbase);
        waterclip = true;
    }

    // Phase 2.x: 3DFX-style height-graded pocket fog for instanced models.
    // CalcFogLevel at the object centre gives the base fog level and the
    // pocket colour (via global CurFogColor).  Sampling 800 units higher
    // gives the Y-gradient.  The vertex shader computes per-vertex fog as:
    //   fog = fogBase + modelSpaceY * fogGrad
    // This matches the D3D / 3DFX look: tall objects fade more at the top.
    const Vector3d unrotatedFogPos = pos;  // before RotateVector
    const float fogBase = CalcFogLevel(unrotatedFogPos);
    const Vector3d fogPocketColor =
        IsUnderwater() ? DecodeFogColorBGR(FogsList[127].fogRGB) : DecodeFogColor(CurFogColor);

    float fogGrad = 0.0f;
    if (fogBase > 0.0f) {
        Vector3d highPoint = unrotatedFogPos;
        highPoint.y += 800.0f;
        const float fogHigh = CalcFogLevel(highPoint);
        fogGrad = (fogHigh - fogBase) / 800.0f;
    }

    pos = RotateVector(pos);
    float zs = 0.0f;
    const float fadeStart = 256.0f * (ctViewR - 4);
    const float fadeStartSq = fadeStart * fadeStart;
    if (distanceSq > fadeStartSq) {
        zs = static_cast<float>(std::sqrt(distanceSq));
        GlassL = (std::min)(255, static_cast<int>((zs - fadeStart) / 4.0f));
    } else {
        GlassL = 0;
    }
    if (GlassL == 255) {
        return;
    }

    if ((MObjects[ob].info.flags & ofANIMATED) && MObjects[ob].info.LastAniTime != RealTime) {
        MObjects[ob].info.LastAniTime = RealTime;
        CreateMorphedObject(MObjects[ob].model.get(), MObjects[ob].vtl, RealTime % MObjects[ob].vtl.AniTime);
    }

    bool renderAsBMP = false;
    if (!(MObjects[ob].info.flags & ofNOBMP)) {
        const float bmpDistanceLimit = ctViewRM * 256.0f;
        const float bmpDistanceLimitSq = bmpDistanceLimit * bmpDistanceLimit;
        if (distanceSq > bmpDistanceLimitSq) {
            if (GlassL > 0) {
                renderAsBMP = zs > bmpDistanceLimit;
            } else {
                const float distance = static_cast<float>(std::sqrt(distanceSq));
                renderAsBMP = distance > bmpDistanceLimit;
            }
        }
    }

    if (renderAsBMP) {
        // Phase 2.3: BMP fallback path unchanged.
        RenderBMPModel(&MObjects[ob].bmpmodel, pos.x, pos.y, pos.z, mlight - 16);
    } else if (waterclip) {
        // Phase 2.3: water-clipped objects use legacy path (Phase 2.10
        // will move water clip to fragment shader).
        UploadStaticMesh(MObjects[ob].model.get());
        RenderModelClipWater(MObjects[ob].model.get(), pos.x, pos.y, pos.z, mlight, FI, fi, CameraBeta);
    } else {
        // Phase 2.3: instanced path for non-BMP, non-water-clip objects.
        // Compute world matrix from position and rotation.
        const StaticMeshEntry meshEntry = UploadStaticMesh(MObjects[ob].model.get());

        // Phase 2.3: route models with sfTransparent faces through the
        // legacy path (they need blend which the instanced opaque pass
        // does not set up).  The proper instanced transparent pass is
        // deferred to a follow-up task.
        if (meshEntry.hasTransparent) {
            RenderModelClip(MObjects[ob].model.get(), pos.x, pos.y, pos.z, mlight, FI, fi, CameraBeta);
            return;
        }

        const float ca = std::cos(fi);
        const float sa = std::sin(fi);
        const float cb = std::cos(CameraBeta);
        const float sb = std::sin(CameraBeta);

        ModelInstance instance;
        // Phase 2.3: populate the view-from-model matrix COLUMNS.
        // GLSL mat4(col0,col1,col2,col3) takes column vectors, so
        // we fill worldCol0-3 as the four columns of:
        //   | ca       0        sa       pos.x |
        //   | sa*sb    cb       -ca*sb   pos.y |
        //   | -sa*cb   sb       ca*cb    pos.z |
        //   | 0        0        0        1     |
        instance.worldCol0[0] = ca;
        instance.worldCol0[1] = sa * sb;
        instance.worldCol0[2] = -sa * cb;
        instance.worldCol0[3] = 0.0f;
        instance.worldCol1[0] = 0.0f;
        instance.worldCol1[1] = cb;
        instance.worldCol1[2] = sb;
        instance.worldCol1[3] = 0.0f;
        instance.worldCol2[0] = sa;
        instance.worldCol2[1] = -ca * sb;
        instance.worldCol2[2] = ca * cb;
        instance.worldCol2[3] = 0.0f;
        instance.worldCol3[0] = pos.x;
        instance.worldCol3[1] = pos.y;
        instance.worldCol3[2] = pos.z;
        instance.worldCol3[3] = 1.0f;

        // Instance light: normalize to [0,1] range (current mlight is 64-192).
        // Phase 2.x: .yzw carry the per-object pocket-fog colour (3DFX-style).
        instance.instanceLight[0] = static_cast<float>(mlight) / 255.0f;
        instance.instanceLight[1] = fogPocketColor.x;
        instance.instanceLight[2] = fogPocketColor.y;
        instance.instanceLight[3] = fogPocketColor.z;

        // Phase 2.3: sfOpacity flag from the static mesh cache. The fragment
        // shader alpha-tests cutout faces with linear filtering, matching the
        // Ice Age 3DFX-style handling instead of switching whole textures to
        // nearest filtering.
        // Phase 2.x: .y = fogGrad (Y-gradient, was tintByFog=0).
        //            .z = fogBase (pocket-fog amount at object centre).
        const float alpha = std::clamp((255.0f - static_cast<float>(GlassL)) / 255.0f, 0.0f, 1.0f);
        instance.instanceFlags[0] = meshEntry.hasCutout ? 1.0f : 0.0f; // cutout
        instance.instanceFlags[1] = (fogGrad / 255.0f) * kFogDensity; // Phase 2.x: fog Y-gradient
        instance.instanceFlags[2] = (fogBase / 255.0f) * kFogDensity; // Phase 2.x: fog base amount
        instance.instanceFlags[3] = alpha; // alpha

        // Ensure capacity and add instance.
        m_instanceData.push_back(instance);

        // Track model and texture for instanced draw grouping.
        InstanceInfo info;
        info.model = MObjects[ob].model.get();
        info.texture = UploadModelTexture(MObjects[ob].model.get());
        m_instanceInfo.push_back(info);
    }
}

void GLRenderer::RenderInstancedModels()
{
    // Phase 2.3: render instanced models with one glDrawElementsInstanced
    // per (model, texture) group. The instance data was populated by
    // RenderMappedObject calls above.

    if (m_instanceData.empty() || m_instanceInfo.empty() ||
        !m_instancedModelShader || !m_instanceVAO) {
        return;
    }

    // Phase 2.9: removed wasteful full-array upload.  The per-group
    // loop below uploads only the current group's slice to VBO offset 0
    // (GL 3.3 workaround for missing glDrawElementsInstancedBaseInstance).
    // The full-array upload was always overwritten by the first group.

    // Group instances by (model, texture) for instanced draws.
    struct InstanceGroup {
        const TModel* model;
        GLuint texture;
        uint32_t instanceStart;
        uint32_t instanceCount;
    };
    std::vector<InstanceGroup> groups;

    // Build groups by detecting transitions in the instance list.
    // Instances are added in object-list order, so objects with the
    // same model/texture are often adjacent.
    {
        const TModel* curModel = m_instanceInfo[0].model;
        GLuint curTexture = m_instanceInfo[0].texture;
        uint32_t groupStart = 0;

        for (size_t i = 1; i < m_instanceInfo.size(); ++i) {
            if (m_instanceInfo[i].model != curModel ||
                m_instanceInfo[i].texture != curTexture) {
                // End of current group.
                groups.push_back({curModel, curTexture, groupStart,
                                  static_cast<uint32_t>(i - groupStart)});
                curModel = m_instanceInfo[i].model;
                curTexture = m_instanceInfo[i].texture;
                groupStart = static_cast<uint32_t>(i);
            }
        }
        // Add the last group.
        groups.push_back({curModel, curTexture, groupStart,
                          static_cast<uint32_t>(m_instanceInfo.size() - groupStart)});
    }

    // Set up rendering state.
    const auto projection = BuildLegacyProjection();
    UpdatePerFrameUBO(projection);

    glUseProgram(m_instancedModelShader);
    glBindVertexArray(m_instanceVAO);

    // Phase 2.9: orphan the instance VBO once (glBufferData with
    // nullptr) to avoid per-group stalls.  Size to the largest group
    // in bytes.  The per-group loop then uses glBufferSubData without
    // further orphans — the buffer was just orphaned so the driver
    // knows all content is being replaced.
    GLsizeiptr maxGroupBytes = 0;
    for (const auto& g : groups) {
        const GLsizeiptr gb = static_cast<GLsizeiptr>(g.instanceCount) * sizeof(ModelInstance);
        if (gb > maxGroupBytes) maxGroupBytes = gb;
    }
    if (maxGroupBytes < static_cast<GLsizeiptr>(sizeof(ModelInstance)))
        maxGroupBytes = sizeof(ModelInstance);

    glBindBuffer(GL_ARRAY_BUFFER, m_instanceVBO);
    glBufferData(GL_ARRAY_BUFFER, maxGroupBytes, nullptr, GL_STREAM_DRAW);
    // Keep m_instanceVBO bound — the VAO references it for attributes
    // 4-9.  The per-group loop glBufferSubData's into this same buffer.

    // Bind the static IBO for indexed drawing.
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_staticMeshIBO);

    // Enable depth test, disable blend (opaque pass).
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);

    // Draw each group with instanced rendering.
    uint32_t totalDrawCalls = 0;
    uint32_t totalInstances = 0;

    for (const auto& group : groups) {
        const StaticMeshEntry* meshEntry = GetStaticMeshEntry(group.model);
        if (!meshEntry || meshEntry->indexCount == 0) {
            continue;
        }

        // Phase 2.9: m_instanceVBO was already bound + orphaned above.
        // glBufferSubData writes the group's slice to offset 0 without
        // an extra bind/unbind round-trip.
        const GLsizeiptr groupSliceBytes =
            static_cast<GLsizeiptr>(group.instanceCount) * sizeof(ModelInstance);
        glBufferSubData(GL_ARRAY_BUFFER, 0, groupSliceBytes,
                        &m_instanceData[group.instanceStart]);

        // Bind the texture for this group.
        if (group.texture != m_lastBoundModelTexture) {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, group.texture);
            m_lastBoundModelTexture = group.texture;
#ifdef GL_PERF_HOOKS
            GL_PERF_TEXTURE_BIND(group.texture);
#endif
        }

        // Issue the instanced draw call.
        // The indices in the static IBO reference the static VBO directly.
        glDrawElementsInstanced(
            GL_TRIANGLES,
            static_cast<GLsizei>(meshEntry->indexCount),
            GL_UNSIGNED_INT,
            reinterpret_cast<void*>(
                static_cast<uintptr_t>(meshEntry->baseIndex * sizeof(uint32_t))),
            static_cast<GLsizei>(group.instanceCount));

#ifdef GL_PERF_HOOKS
        GL_PERF_DRAW(static_cast<uint32_t>(group.instanceCount));
#endif
        totalDrawCalls++;
        totalInstances += group.instanceCount;
    }

    // Phase 2.9: unbind the instance VBO (was left bound for the
    // per-group glBufferSubData loop).
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    // Phase 2.3: unbind the VAO to avoid leaking instance-attribute
    // state into subsequent draws (e.g., the legacy model path in
    // RenderWorldModels).  Do NOT reset glVertexAttribDivisor on
    // m_instanceVAO — VAO state is persistent and we need divisor=1
    // for the next frame's instanced draws.
    glBindVertexArray(0);
    glUseProgram(0);

    (void)totalDrawCalls;
    (void)totalInstances;
}

void GLRenderer::RenderModelsList()
{
#ifdef GL_PERF_HOOKS
    GL_PERF_SCOPE("RenderModelsList");
#endif
    // Phase 1.11: reset the last-bound model texture tracker at the
    // start of each frame's model-draw session.
    m_lastBoundModelTexture = 0;

    // Phase 2.3: populate instance data for instanced objects.
    // RenderMappedObject now adds to m_instanceData for non-BMP,
    // non-water-clip objects.
    m_instanceData.clear();
    m_instanceInfo.clear();
    for (const Vector2di& object : m_objectList) {
        RenderMappedObject(object.x, object.y);
    }
    m_objectList.clear();

    // Phase 2.3: render instanced models (non-BMP, non-water-clip).
    if (!m_instanceData.empty()) {
        RenderInstancedModels();
    }

    // Phase 2.3: render legacy path models (water-clip, BMP).
    RenderWorldModels();
}

static void ApplyGLModelDistanceFade(const Vector3d& rpos)
{
    const float distanceSq = VectorLengthSq(rpos);
    const float fadeStart = 256.0f * (ctViewR - 4);
    const float fadeStartSq = fadeStart * fadeStart;

    GlassL = 0;
    if (distanceSq > fadeStartSq) {
        const float distance = std::sqrt(distanceSq);
        GlassL = (std::min)(255, static_cast<int>((distance - fadeStart) / 4.0f));
    }
}


void GLRenderer::Render3DHardwarePosts()
{
#ifdef GL_PERF_HOOKS
    GL_PERF_SCOPE("Render3DHardwarePosts");
#endif

    // ── Characters (dinosaurs, hunters) ──────────────────────────────
    for (int c = 0; c < ChCount; c++) {
        TCharacter* cptr = &Characters[c];
        cptr->rpos.x = cptr->pos.x - CameraX;
        cptr->rpos.y = cptr->pos.y - CameraY;
        cptr->rpos.z = cptr->pos.z - CameraZ;

        float r = static_cast<float>((std::max)(fabs(cptr->rpos.x), fabs(cptr->rpos.z)));
        int ri = -1 + static_cast<int>(r / 256.0f + 0.5f);
        if (ri < 0) ri = 0;
        if (ri > ctViewR) continue;

        cptr->rpos = RotateVector(cptr->rpos);

        float br = BackViewR + DinoInfo[cptr->CType].Radius;
        if (cptr->rpos.z > br) continue;
        if (fabs(cptr->rpos.x) > -cptr->rpos.z + br) continue;
        if (fabs(cptr->rpos.y) > -cptr->rpos.z + br) continue;

        // Morph the character model
        CreateChMorphedModel(cptr);

        float zs = sqrtf(cptr->rpos.x * cptr->rpos.x +
                         cptr->rpos.y * cptr->rpos.y +
                         cptr->rpos.z * cptr->rpos.z);
        if (zs > ctViewR * 256.0f) continue;

        GlassL = 0;
        if (zs > 256.0f * (ctViewR - 4))
            GlassL = (std::min)(255, static_cast<int>(zs / 4.0f - 64.0f * (ctViewR - 4)));

        waterclip = false;

        if (cptr->rpos.z > -256.0f * 10.0f)
            RenderModelClip(cptr->pinfo->mptr.get(),
                            cptr->rpos.x, cptr->rpos.y, cptr->rpos.z, 210, 0,
                            -cptr->alpha + pi / 2.0f + CameraAlpha,
                            CameraBeta);
        else
            RenderModel(cptr->pinfo->mptr.get(),
                        cptr->rpos.x, cptr->rpos.y, cptr->rpos.z, 210, 0,
                        -cptr->alpha + pi / 2.0f + CameraAlpha,
                        CameraBeta);
    }

    // ── Multiplayer players ──────────────────────────────────────────
    if (Multiplayer) {
        for (int c = 0; c < 1; c++) {
            TCharacter* cptr = &MPlayers[c];
            cptr->rpos.x = cptr->pos.x - CameraX;
            cptr->rpos.y = cptr->pos.y - CameraY;
            cptr->rpos.z = cptr->pos.z - CameraZ;

            float r = static_cast<float>((std::max)(fabs(cptr->rpos.x), fabs(cptr->rpos.z)));
            int ri = -1 + static_cast<int>(r / 256.0f + 0.5f);
            if (ri < 0) ri = 0;
            if (ri > ctViewR) continue;

            cptr->rpos = RotateVector(cptr->rpos);

            float br = BackViewR + DinoInfo[cptr->CType].Radius;
            if (cptr->rpos.z > br) continue;
            if (fabs(cptr->rpos.x) > -cptr->rpos.z + br) continue;
            if (fabs(cptr->rpos.y) > -cptr->rpos.z + br) continue;

            CreateChMorphedModel(cptr);

            float zs = sqrtf(cptr->rpos.x * cptr->rpos.x +
                             cptr->rpos.y * cptr->rpos.y +
                             cptr->rpos.z * cptr->rpos.z);
            if (zs > ctViewR * 256.0f) continue;

            GlassL = 0;
            if (zs > 256.0f * (ctViewR - 4))
                GlassL = (std::min)(255, static_cast<int>(zs / 4.0f - 64.0f * (ctViewR - 4)));

            waterclip = false;

            if (cptr->rpos.z > -256.0f * 10.0f)
                RenderModelClip(cptr->pinfo->mptr.get(),
                                cptr->rpos.x, cptr->rpos.y, cptr->rpos.z, 210, 0,
                                -cptr->alpha + pi / 2.0f + CameraAlpha,
                                CameraBeta);
            else
                RenderModel(cptr->pinfo->mptr.get(),
                            cptr->rpos.x, cptr->rpos.y, cptr->rpos.z, 210, 0,
                            -cptr->alpha + pi / 2.0f + CameraAlpha,
                            CameraBeta);
        }
    }

    // ── Ship ─────────────────────────────────────────────────────────
    Ship.rpos.x = Ship.pos.x - CameraX;
    Ship.rpos.y = Ship.pos.y - CameraY;
    Ship.rpos.z = Ship.pos.z - CameraZ;
    {
        float r = static_cast<float>((std::max)(fabs(Ship.rpos.x), fabs(Ship.rpos.z)));
        int ri = -1 + static_cast<int>(r / 256.0f + 0.2f);
        if (ri < 0) ri = 0;
        if (ri < ctViewR) {
            Ship.rpos = RotateVector(Ship.rpos);
            if (Ship.rpos.z <= BackViewR &&
                fabs(Ship.rpos.x) <= -Ship.rpos.z + BackViewR) {
                if (Ship.State != -1) {
                    ApplyGLModelDistanceFade(Ship.rpos);

                    CreateMorphedModel(ShipModel.mptr.get(), &ShipModel.Animation[0], Ship.FTime, 1.0);

                    if (fabs(Ship.rpos.z) < 4000.0f)
                        RenderModelClip(ShipModel.mptr.get(),
                                        Ship.rpos.x, Ship.rpos.y, Ship.rpos.z, 210, 0,
                                        -Ship.alpha - pi / 2.0f + CameraAlpha, CameraBeta);
                    else
                        RenderModel(ShipModel.mptr.get(),
                                    Ship.rpos.x, Ship.rpos.y, Ship.rpos.z, 210, 0,
                                    -Ship.alpha - pi / 2.0f + CameraAlpha, CameraBeta);
                }
            }
        }
    }

    // ── Super Ship ───────────────────────────────────────────────────
    SShip.rpos.x = SShip.pos.x - CameraX;
    SShip.rpos.y = SShip.pos.y - CameraY;
    SShip.rpos.z = SShip.pos.z - CameraZ;
    {
        float r = static_cast<float>((std::max)(fabs(SShip.rpos.x), fabs(SShip.rpos.z)));
        int ri = -1 + static_cast<int>(r / 256.0f + 0.2f);
        if (ri < 0) ri = 0;
        if (ri < ctViewR) {
            SShip.rpos = RotateVector(SShip.rpos);
            if (SShip.rpos.z <= BackViewR &&
                fabs(SShip.rpos.x) <= -SShip.rpos.z + BackViewR) {
                if (SShip.State >= 1) {
                    ApplyGLModelDistanceFade(SShip.rpos);

                    CreateMorphedModelBetaGamma(SShipModel.mptr.get(), &SShipModel.Animation[0],
                                                SShip.FTime, 1.0, SShip.beta, SShip.gamma);

                    if (fabs(SShip.rpos.z) < 4000.0f)
                        RenderModelClip(SShipModel.mptr.get(),
                                        SShip.rpos.x, SShip.rpos.y, SShip.rpos.z, 210, 0,
                                        -SShip.alpha - pi / 2.0f + CameraAlpha, CameraBeta);
                    else
                        RenderModel(SShipModel.mptr.get(),
                                    SShip.rpos.x, SShip.rpos.y, SShip.rpos.z, 210, 0,
                                    -SShip.alpha - pi / 2.0f + CameraAlpha, CameraBeta);
                }
            }
        }
    }

    // ── Ammo Bag ─────────────────────────────────────────────────────
    AmmoBag.rpos.x = AmmoBag.pos.x - CameraX;
    AmmoBag.rpos.y = AmmoBag.pos.y - CameraY;
    AmmoBag.rpos.z = AmmoBag.pos.z - CameraZ;
    {
        float r = static_cast<float>((std::max)(fabs(AmmoBag.rpos.x), fabs(AmmoBag.rpos.z)));
        int ri = -1 + static_cast<int>(r / 256.0f + 0.2f);
        if (ri < 0) ri = 0;
        if (ri < ctViewR) {
            AmmoBag.rpos = RotateVector(AmmoBag.rpos);
            if (AmmoBag.rpos.z <= BackViewR &&
                fabs(AmmoBag.rpos.x) <= -AmmoBag.rpos.z + BackViewR) {
                if (AmmoBag.State >= 1) {
                    ApplyGLModelDistanceFade(AmmoBag.rpos);

                    CreateMorphedModel(BagModel.mptr.get(), &BagModel.Animation[0], AmmoBag.FTime, 1.0);

                    if (fabs(AmmoBag.rpos.z) < 4000.0f)
                        RenderModelClip(BagModel.mptr.get(),
                                        AmmoBag.rpos.x, AmmoBag.rpos.y, AmmoBag.rpos.z, 210, 0,
                                        -pi / 2.0f + CameraAlpha, CameraBeta);
                    else
                        RenderModel(BagModel.mptr.get(),
                                    AmmoBag.rpos.x, AmmoBag.rpos.y, AmmoBag.rpos.z, 210, 0,
                                    -pi / 2.0f + CameraAlpha, CameraBeta);
                }
            }
        }
    }

    // ── Bullets ──────────────────────────────────────────────────────
    for (int b = 0; b < bulletCh; b++) {
        if (!WeapInfo[bullet[b].parent].bullet) continue;

        bullet[b].rpos.x = bullet[b].a.x - CameraX;
        bullet[b].rpos.y = bullet[b].a.y - CameraY;
        bullet[b].rpos.z = bullet[b].a.z - CameraZ;
        float r = static_cast<float>((std::max)(fabs(bullet[b].rpos.x), fabs(bullet[b].rpos.z)));
        int ri = -1 + static_cast<int>(r / 256.0f + 0.2f);
        if (ri < 0) ri = 0;
        if (ri < ctViewR) {
            bullet[b].rpos = RotateVector(bullet[b].rpos);
            if (bullet[b].rpos.z <= BackViewR &&
                fabs(bullet[b].rpos.x) <= -bullet[b].rpos.z + BackViewR) {
                ApplyGLModelDistanceFade(bullet[b].rpos);

                CreateMorphedModelBetaGamma(Weapon.Bullet[bullet[b].parent].mptr.get(),
                                            &Weapon.Bullet[bullet[b].parent].Animation[0],
                                            bullet[b].FTime, 1.0, bullet[b].beta, 0.0f);

                if (fabs(bullet[b].rpos.z) < 4000.0f)
                    RenderModelClip(Weapon.Bullet[bullet[b].parent].mptr.get(),
                                    bullet[b].rpos.x, bullet[b].rpos.y, bullet[b].rpos.z, 210, 0,
                                    -bullet[b].alpha - pi / 2.0f + CameraAlpha,
                                    -bullet[b].beta - pi / 2.0f + CameraBeta);
                else
                    RenderModel(Weapon.Bullet[bullet[b].parent].mptr.get(),
                                bullet[b].rpos.x, bullet[b].rpos.y, bullet[b].rpos.z, 210, 0,
                                -bullet[b].alpha - pi / 2.0f + CameraAlpha,
                                -bullet[b].beta - pi / 2.0f + CameraBeta);
            }
        }
    }

    // Flush all queued models (characters, ships, bullets) to GPU
    RenderWorldModels();
}

void GLRenderer::RenderCircle(float cx, float cy, float z, float R, uint32_t RGBA, uint32_t RGBA2)
{
    // The game stores colors in ABGR format (R in bits 0-7, B in bits 16-23).
    // D3D vertex colors are ARGB, so D3D inherently swaps R↔B when reading.
    // We must do the same: extract ABGR and pass as-is to the shader.
    auto unpackABGR = [](uint32_t c, uint8_t& r, uint8_t& g, uint8_t& b, uint8_t& a) {
        a = static_cast<uint8_t>((c >> 24) & 0xFF);
        b = static_cast<uint8_t>((c >> 16) & 0xFF);
        g = static_cast<uint8_t>((c >> 8) & 0xFF);
        r = static_cast<uint8_t>(c & 0xFF);
    };

    uint8_t cr, cg, cb, ca;
    uint8_t er, eg, eb, ea;
    unpackABGR(RGBA, cr, cg, cb, ca);
    unpackABGR(RGBA2, er, eg, eb, ea);

    // Clamp radius to sub-pixel precision (matching D3D)
    float r  = floorf(R * 16.0f) / 16.0f;
    float r2 = floorf(0.65f * R * 16.0f) / 16.0f;

    // Convert screen-space position to NDC (-1 to 1)
    float ndcX = (cx - VideoCX) / VideoCX;
    float ndcY = (VideoCY - cy) / VideoCY;

    // Use the packed ModelVertex layout (Phase 1.4: 32 bytes, color
    // attributes are uint8 normalized). The model VAO's attribute
    // pointers expect this layout; the old CircleVertex used floats
    // for color fields, which the driver read as raw bytes — producing
    // garbage colors (same regression as RenderFSRect, fixed in d3c7d25).
    //
    // fog=255 (normalized to 1.0) makes the model shader output the
    // fog color (our desired particle color) instead of the texture.
    const uint8_t lightByte  = 255;
    const uint8_t fogByte    = 255;
    const uint8_t cutoutByte = 0;

    // 8 triangles forming an octagon with alternating outer/inner radius
    // Matches D3D: angle 0=R, 45=R2, 90=R, 135=R2, ...
    std::vector<ModelVertex> vertices;
    vertices.reserve(24);

    auto makeCircleVertex = [&](float x, float y, uint8_t vr, uint8_t vg,
                               uint8_t vb, uint8_t va) -> ModelVertex {
        return {x, y, 0.0001f, 0.0f, 0.0f,
                lightByte, fogByte, va, cutoutByte,
                vr, vg, vb, {0,0,0,0,0}};
    };

    for (int i = 0; i < 8; i++) {
        int next = (i + 1) % 8;

        // Radius alternates: even indices use R (outer), odd indices use R2 (inner)
        float rad_i = (i % 2 == 0) ? r : r2;
        float rad_next = (next % 2 == 0) ? r : r2;

        float angle_i = i * pi / 4.0f;
        float angle_next = next * pi / 4.0f;

        // Center vertex (color RGBA)
        vertices.push_back(makeCircleVertex(ndcX, ndcY, cr, cg, cb, ca));

        // Edge vertex i (color RGBA2)
        float ex1 = ndcX + cosf(angle_i) * rad_i / VideoCX;
        float ey1 = ndcY + sinf(angle_i) * rad_i / VideoCY;
        vertices.push_back(makeCircleVertex(ex1, ey1, er, eg, eb, ea));

        // Edge vertex i+1 (color RGBA2)
        float ex2 = ndcX + cosf(angle_next) * rad_next / VideoCX;
        float ey2 = ndcY + sinf(angle_next) * rad_next / VideoCY;
        vertices.push_back(makeCircleVertex(ex2, ey2, er, eg, eb, ea));
    }

    // 2D HUD circles: vertices are in NDC space. Upload the identity
    // projection to the UBO so gl_Position = projection * vec4(pos, 1)
    // passes the NDC coordinates through unchanged.
    const std::array<float, 16> identity = {
        1.0f,0.0f,0.0f,0.0f, 0.0f,1.0f,0.0f,0.0f, 0.0f,0.0f,1.0f,0.0f, 0.0f,0.0f,0.0f,1.0f
    };

    UpdatePerFrameUBO(identity);
    glUseProgram(m_modelShader);
#ifdef GL_PERF_HOOKS
    GL_PERF_STATE_CHANGE();
#endif
    // uProjection in PerFrame UBO (Phase 1.1)

    glEnable(GL_DEPTH_TEST);
#ifdef GL_PERF_HOOKS
    GL_PERF_STATE_CHANGE();
#endif
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_FALSE);
#ifdef GL_PERF_HOOKS
    GL_PERF_STATE_CHANGE();
#endif
    glEnable(GL_BLEND);
#ifdef GL_PERF_HOOKS
    GL_PERF_STATE_CHANGE();
#endif
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
#ifdef GL_PERF_HOOKS
    GL_PERF_STATE_CHANGE();
#endif

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_whiteTexture);
#ifdef GL_PERF_HOOKS
    GL_PERF_TEXTURE_BIND(m_whiteTexture);
#endif
    glBindVertexArray(m_modelVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_modelVBO);
    const GLsizeiptr vertexSize = static_cast<GLsizeiptr>(vertices.size() * sizeof(ModelVertex));
    glBufferData(GL_ARRAY_BUFFER, vertexSize, nullptr, GL_STREAM_DRAW);
    glBufferSubData(GL_ARRAY_BUFFER, 0, vertexSize, vertices.data());
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertices.size()));
#ifdef GL_PERF_HOOKS
    GL_PERF_DRAW(static_cast<uint32_t>(vertices.size()) / 3);
#endif
    glBindVertexArray(0);

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}

// Blend an ABGR color (R in bits 0-7, B in 16-23, A in 24-31) toward the
// current fog color at the given world position, preserving the source
// alpha. Used to fog particles, blood trails, and snow — the legacy D3D/3DFX
// renderers pre-baked fog into the element RGBA in Game.cpp, but the GL
// pipeline doesn't reuse that, so we apply the fog here at draw time using
// the same CalcFogLevel() the rest of the scene uses.
static uint32_t ApplyFogToABGR(uint32_t abgr, const Vector3d& worldPos)
{
    const FogSample fog = SampleFogAtPoint(worldPos, false);
    if (fog.amount <= 0.0f) {
        return abgr;
    }

    const float a = static_cast<float>((abgr >> 24) & 0xFF) / 255.0f;
    const float b = static_cast<float>((abgr >> 16) & 0xFF) / 255.0f;
    const float g = static_cast<float>((abgr >> 8) & 0xFF) / 255.0f;
    const float r = static_cast<float>(abgr & 0xFF) / 255.0f;

    const float k = fog.amount;
    const float outR = r * (1.0f - k) + fog.color.x * k;
    const float outG = g * (1.0f - k) + fog.color.y * k;
    const float outB = b * (1.0f - k) + fog.color.z * k;

    const uint32_t A = static_cast<uint32_t>(a * 255.0f);
    const uint32_t R = static_cast<uint32_t>(outR * 255.0f);
    const uint32_t G = static_cast<uint32_t>(outG * 255.0f);
    const uint32_t B = static_cast<uint32_t>(outB * 255.0f);

    return (A << 24) | (B << 16) | (G << 8) | R;
}

void GLRenderer::RenderElements()
{
#ifdef GL_PERF_HOOKS
    GL_PERF_SCOPE("RenderElements");
#endif

    // ── Regular elements (muzzle flashes, impact sparks, etc.) ─────
    for (int eg = 0; eg < ElCount; eg++) {
        for (int e = 0; e < Elements[eg].ECount; e++) {
            TElement* el = &Elements[eg].EList[e];
            Vector3d rpos;
            rpos.x = el->pos.x - CameraX;
            rpos.y = el->pos.y - CameraY;
            rpos.z = el->pos.z - CameraZ;
            float r = el->R;

            // Sample fog at the world-relative position before we rotate
            // into view space (CalcFogLevel expects world coords).
            const uint32_t fogRGBA = ApplyFogToABGR(Elements[eg].RGBA, rpos);
            const uint32_t fogRGBA2 = ApplyFogToABGR(Elements[eg].RGBA2, rpos);

            rpos = RotateVector(rpos);
            if (rpos.z > -64) continue;
            if (fabs(rpos.x) > -rpos.z) continue;
            if (fabs(rpos.y) > -rpos.z) continue;

            float sx = VideoCX - static_cast<int>((CameraW * rpos.x / rpos.z * 16)) / 16.0f;
            float sy = VideoCY + static_cast<int>((CameraH * rpos.y / rpos.z * 16)) / 16.0f;
            RenderCircle(sx, sy, rpos.z, -r * CameraW * 0.64f / rpos.z,
                         fogRGBA, fogRGBA2);
        }
    }

    // ── Blood trails ──────────────────────────────────────────────
    for (int b = 0; b < BloodTrail.Count; b++) {
        Vector3d rpos = BloodTrail.Trail[b].pos;
        uint32_t A1 = (0xE0 * BloodTrail.Trail[b].LTime / 20000);
        if (A1 > 0xE0) A1 = 0xE0;
        uint32_t A2 = (0x20 * BloodTrail.Trail[b].LTime / 20000);
        if (A2 > 0x20) A2 = 0x20;

        rpos.x = rpos.x - CameraX;
        rpos.y = rpos.y - CameraY;
        rpos.z = rpos.z - CameraZ;

        // Blood colors are constructed in ARGB format (R<<16 | G<<8 | B).
        // conv_xGx processes them but returns unchanged in daytime.
        // RenderCircle expects ABGR, so convert: swap R and B channels.
        int dr = DinoInfo[BloodTrail.Trail[b].Owner].bloodRed;
        int dg = DinoInfo[BloodTrail.Trail[b].Owner].bloodGreen;
        int db = DinoInfo[BloodTrail.Trail[b].Owner].bloodBlue;
        uint32_t centerColor = (A1 << 24) | conv_xGx((db << 16) | (dg << 8) | dr);
        uint32_t edgeColor   = (A2 << 24) | conv_xGx((db/2 << 16) | (dg/2 << 8) | dr/2);

        // Apply fog at the world-relative position before rotation.
        const uint32_t fogCenter = ApplyFogToABGR(centerColor, rpos);
        const uint32_t fogEdge   = ApplyFogToABGR(edgeColor, rpos);

        rpos = RotateVector(rpos);
        if (rpos.z > -64) continue;
        if (fabs(rpos.x) > -rpos.z) continue;
        if (fabs(rpos.y) > -rpos.z) continue;

        float sx = VideoCX - static_cast<int>((CameraW * rpos.x / rpos.z * 16)) / 16.0f;
        float sy = VideoCY + static_cast<int>((CameraH * rpos.y / rpos.z * 16)) / 16.0f;

        RenderCircle(sx, sy, rpos.z, -12.0f * CameraW * 0.64f / rpos.z,
                     fogCenter, fogEdge);
    }

    // ── Snow particles ────────────────────────────────────────────
    for (int st = 0; st < SnowCh; st++) {
        for (int s = SnowInfo[st].addr; s < SnowInfo[st].addr + SnowInfo[st].SnCount; s++) {
            Vector3d rpos = Snow[s].pos;
            rpos.x = rpos.x - CameraX;
            rpos.y = rpos.y - CameraY;
            rpos.z = rpos.z - CameraZ;

            uint32_t A11 = SnowInfo[st].snow_a;
            if (Snow[s].ftime) {
                A11 = A11 * (2000 - Snow[s].ftime) / 2000;
            }

            // Snow colors use conv_xGx which expects ABGR (R in bits 0-7)
            uint32_t centerColor = (A11 << 24) |
                conv_xGx((SnowInfo[st].snow_b << 16) |
                (SnowInfo[st].snow_g << 8) |
                SnowInfo[st].snow_r);

            uint32_t edgeColor = ((A11 / 7) << 24) |
                conv_xGx((SnowInfo[st].snow_b / 2 << 16) |
                (SnowInfo[st].snow_g / 2 << 8) |
                SnowInfo[st].snow_r / 2);

            // Apply fog at the world-relative position before rotation.
            const uint32_t fogCenter = ApplyFogToABGR(centerColor, rpos);
            const uint32_t fogEdge   = ApplyFogToABGR(edgeColor, rpos);

            rpos = RotateVector(rpos);
            if (rpos.z > -64) continue;
            if (fabs(rpos.x) > -rpos.z) continue;
            if (fabs(rpos.y) > -rpos.z) continue;

            float sx = VideoCX - static_cast<int>((CameraW * rpos.x / rpos.z * 16)) / 16.0f;
            float sy = VideoCY + static_cast<int>((CameraH * rpos.y / rpos.z * 16)) / 16.0f;

            RenderCircle(sx, sy, rpos.z,
                         -8.0f * CameraW * 0.64f / rpos.z * SnowInfo[st].snow_rad,
                         fogCenter, fogEdge);
        }
    }
}

void GLRenderer::RenderBMPModel(TBMPModel* mptr, float x0, float y0, float z0, int light)
{
    if (!mptr) {
        return;
    }

    const GLuint texture = UploadBMPModelTexture(mptr);
    if (!texture) {
        return;
    }

    // Queue into m_worldModelItems instead of drawing immediately.
    // This ensures BMP models participate in the same depth-sorted
    // rendering pipeline as regular models. Previously they were
    // drawn with enableBlend=true (no depth writes), which let
    // water and other models overdraw them.
    ModelDrawItem item;
    item.texture = texture;
    const Vector3d center = {x0, y0, z0};
    item.distance = VectorLengthSq(center);
    item.additive = false;

    const float baseLight = std::clamp(static_cast<float>(light), 0.0f, 255.0f);
    const float alpha = std::clamp((255.0f - static_cast<float>(GlassL)) / 255.0f, 0.0f, 1.0f);

    // Unrotate view-space (x0,y0,z0) back to world-relative
    // coordinates for fog sampling. RotateVector was applied in
    // RenderMappedObject — we invert it here so SampleFogAtPoint
    // sees a stable world-space position independent of camera
    // angle. Regular models do this in BuildModelDrawItem; BMP
    // models must do it here since they bypass that path.
    const float ucY = ::cb * y0 + ::sb * z0;
    const float ucZ = ::cb * z0 - ::sb * y0;
    const Vector3d unrotatedCenter = {
        ::ca * x0 - ::sa * ucZ,
        ucY,
        ::sa * x0 + ::ca * ucZ
    };
    // Phase 2.x: 3DFX-style height-graded fog for billboards.
    // CalcFogLevel at the object centre gives the base fog level
    // (FogYBase) and the pocket colour (stored in global CurFogColor).
    // Sampling 800 units higher gives the Y-gradient (FogYGrad).
    // Each vertex then gets: fog = FogYBase + localY * FogYGrad.
    // This produces the same per-vertex gradient the 3DFX / D3D
    // renderers produce, with more fog at the top of tall sprites
    // (e.g. tree-tops) and less at the base.
    const float fogBase = CalcFogLevel(unrotatedCenter);
    const Vector3d fogColor3dfx =
        IsUnderwater() ? DecodeFogColorBGR(FogsList[127].fogRGB) : DecodeFogColor(CurFogColor);

    float fogGrad = 0.0f;
    if (fogBase > 0.0f) {
        Vector3d highPoint = unrotatedCenter;
        highPoint.y += 800.0f;
        const float fogHigh = CalcFogLevel(highPoint);
        fogGrad = (fogHigh - fogBase) / 800.0f;
    }

    const bool hasFade = alpha < 0.999f;

    // Visibility check: all 4 billboard corners share the same z.
    if (z0 >= -256.0f) return;

    // Build two triangles (0-1-2, 0-2-3) for the billboard quad.
    auto makeVertex = [&](int index, float u, float v) -> ModelVertex {
        const float vertexFog = fogBase + mptr->gVertex[index].y * fogGrad;
        const float fogAmount = std::clamp((vertexFog / 255.0f) * kFogDensity, 0.0f, 1.0f);
        return {
            mptr->gVertex[index].x + x0,
            mptr->gVertex[index].y + y0,
            z0,
            u, v,
            Light255ToByte(baseLight),
            Float01ToByte(fogAmount),
            Float01ToByte(alpha),
            CutoutToByte(!hasFade),  // cutout when no fade, opaque when fading
            Float01ToByte(fogColor3dfx.x),
            Float01ToByte(fogColor3dfx.y),
            Float01ToByte(fogColor3dfx.z),
            {0, 0, 0, 0, 0}
        };
    };

    const ModelVertex v0 = makeVertex(0, 0.0f, 0.0f);
    const ModelVertex v1 = makeVertex(1, 1.0f, 0.0f);
    const ModelVertex v2 = makeVertex(2, 1.0f, 1.0f);
    const ModelVertex v3 = makeVertex(3, 0.0f, 1.0f);

    // When GlassL==0 (no distance fade): use cutoutVertices so the
    // billboard writes depth (discarding black pixels). This prevents
    // water and other models from overdraw.
    // When GlassL>0 (distance fade): use transparentVertices for
    // alpha blending, sorted back-to-front with other transparent items.
    auto& target = hasFade ? item.transparentVertices : item.cutoutVertices;
    target.reserve(6);
    target.push_back(v0); target.push_back(v1); target.push_back(v2);
    target.push_back(v0); target.push_back(v2); target.push_back(v3);

    m_worldModelItems.push_back(std::move(item));
}

void GLRenderer::RenderModel(TModel* mptr, float x0, float y0, float z0,
                             int light, int vt, float al, float bt)
{
    // Phase 2.2: ensure the static mesh is uploaded (cache hit after first call).
    UploadStaticMesh(mptr);

    ModelDrawItem item;
    if (!BuildModelDrawItem(item, mptr, x0, y0, z0, light, vt, al, bt, false, false, false, false)) {
        return;
    }
    item.texture = UploadModelTexture(mptr);
    if (!item.texture) {
        return;
    }
    m_worldModelItems.push_back(std::move(item));
}

void GLRenderer::RenderModelClip(TModel* mptr, float x0, float y0, float z0,
                                 int light, int vt, float al, float bt)
{
    // Phase 2.2: ensure the static mesh is uploaded (cache hit after first call).
    UploadStaticMesh(mptr);

    ModelDrawItem item;
    if (!BuildModelDrawItem(item, mptr, x0, y0, z0, light, vt, al, bt, false, false, true, false)) {
        return;
    }
    item.texture = UploadModelTexture(mptr);
    if (!item.texture) {
        return;
    }
    m_worldModelItems.push_back(std::move(item));
}

void GLRenderer::RenderModelClipWater(TModel* mptr, float x0, float y0, float z0,
                                      int light, int vt, float al, float bt)
{
    // Phase 2.2: ensure the static mesh is uploaded (cache hit after first call).
    UploadStaticMesh(mptr);

    ModelDrawItem item;
    if (!BuildModelDrawItem(item, mptr, x0, y0, z0, light, vt, al, bt, true, false, true, false)) {
        return;
    }
    item.texture = UploadModelTexture(mptr);
    if (!item.texture) {
        return;
    }
    m_worldModelItems.push_back(std::move(item));
}

void GLRenderer::RenderNearModel(TModel* mptr, float x0, float y0, float z0,
                                 int light, int vt, float al, float bt)
{
    // Phase 2.2: ensure the static mesh is uploaded (cache hit after first call).
    UploadStaticMesh(mptr);

    ModelDrawItem item;
    if (!BuildModelDrawItem(item, mptr, x0, y0, z0, light, vt, al, bt, false, true, true, false)) {
        return;
    }
    item.texture = UploadModelTexture(mptr);
    if (!item.texture) {
        return;
    }

    const float ca = std::cos(al);
    const float sa = std::sin(al);
    const float cb = std::cos(bt);
    const float sb = std::sin(bt);
    for (int s = 0; s < mptr->VCount; ++s) {
        rVertex[s].x = (mptr->gVertex[s].x * ca + mptr->gVertex[s].z * sa) + x0;
        const float vz = mptr->gVertex[s].z * ca - mptr->gVertex[s].x * sa;
        rVertex[s].y = (mptr->gVertex[s].y * cb - vz * sb) + y0;
        rVertex[s].z = (vz * cb + mptr->gVertex[s].y * sb) + z0;
    }

    const auto projection = BuildLegacyProjection();
    m_lastNearModelProjection = projection;
    m_hasLastNearModelProjection = true;

    glClear(GL_DEPTH_BUFFER_BIT);
    DrawModelVertices(item.texture, item.opaqueVertices, projection, true, false, false);
    if (!item.cutoutVertices.empty()) {
        DrawModelVertices(item.texture, item.cutoutVertices, projection, true, false, false);
    }
    if (!item.transparentVertices.empty()) {
        DrawModelVertices(item.texture, item.transparentVertices, projection, true, true, false);
    }
}

void GLRenderer::RenderModelClipPhongMap(TModel* mptr, float x0, float y0, float z0,
                                         float al, float bt)
{
    // Phase 2.2: ensure the static mesh is uploaded (cache hit after first call).
    UploadStaticMesh(mptr);

    if (!m_modelShader || !m_modelVAO || !m_modelVBO) {
        return;
    }

    GLuint texture = m_phongTexture;
    if (!texture) {
        texture = UploadPictureTexture(TFX_SPECULAR);
        if (!texture) {
            return;
        }
        m_phongTexture = texture;
    }

    std::vector<ModelVertex> vertices;
    const auto clampSkyChannel = [](int value) -> float {
        return static_cast<float>(value > 255 ? 255 : value) / 255.0f;
    };
    const Vector3d color = {
        clampSkyChannel(SkyR + 64),
        clampSkyChannel(SkyG + 64),
        clampSkyChannel(SkyB + 64)
    };
    if (!BuildModelEffectVertices(vertices, mptr, x0, y0, z0, al, bt, sfPhong, color)) {
        return;
    }

    const auto projection = m_hasLastNearModelProjection
        ? m_lastNearModelProjection
        : BuildLegacyProjection();
    DrawModelVertices(texture, vertices, projection, true, true, true, true);
}

void GLRenderer::RenderModelClipEnvMap(TModel* mptr, float x0, float y0, float z0,
                                       float al, float bt)
{
    // Phase 2.2: ensure the static mesh is uploaded (cache hit after first call).
    UploadStaticMesh(mptr);

    if (!m_modelShader || !m_modelVAO || !m_modelVBO) {
        return;
    }

    GLuint texture = m_envTexture;
    if (!texture) {
        texture = UploadPictureTexture(TFX_ENVMAP);
        if (!texture) {
            return;
        }
        m_envTexture = texture;
    }

    std::vector<ModelVertex> vertices;
    const Vector3d white = {1.0f, 1.0f, 1.0f};
    if (!BuildModelEffectVertices(vertices, mptr, x0, y0, z0, al, bt, sfEnvMap, white)) {
        return;
    }

    const auto projection = m_hasLastNearModelProjection
        ? m_lastNearModelProjection
        : BuildLegacyProjection();
    DrawModelVertices(texture, vertices, projection, true, true, true, true);
}

void GLRenderer::EnsureTerrainTextureArray()
{
    if (m_terrainTextureArray) {
        return;
    }

    glGenTextures(1, &m_terrainTextureArray);
    glBindTexture(GL_TEXTURE_2D_ARRAY, m_terrainTextureArray);

    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAX_LEVEL, kTerrainMipLevels - 1);

    int size = 128;
    for (int level = 0; level < kTerrainMipLevels; ++level) {
        glTexImage3D(GL_TEXTURE_2D_ARRAY, level, GL_RGBA8, size, size, kMaxTerrainTextureLayers, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        const int nextSize = size / 2;
        size = nextSize < 1 ? 1 : nextSize;
    }
}

void GLRenderer::UploadTerrainLayer(int layer, const TEXTURE& texture)
{
    if (layer < 0 || layer >= kMaxTerrainTextureLayers) {
        return;
    }

    EnsureTerrainTextureArray();
    glBindTexture(GL_TEXTURE_2D_ARRAY, m_terrainTextureArray);

    const WORD* mipSources[kTerrainMipLevels] = {
        texture.DataA,
        texture.DataB,
        texture.DataC,
        texture.DataD
    };

    int mipSize = 128;
    for (int level = 0; level < kTerrainMipLevels; ++level) {
        std::vector<unsigned int> expanded(mipSize * mipSize);
        for (int i = 0; i < mipSize * mipSize; ++i) {
            expanded[i] = Expand1555to8888(mipSources[level][i]);
        }

        glTexSubImage3D(GL_TEXTURE_2D_ARRAY, level, 0, 0, layer, mipSize, mipSize, 1, GL_RGBA, GL_UNSIGNED_BYTE, expanded.data());
        const int nextMipSize = mipSize / 2;
        mipSize = nextMipSize < 1 ? 1 : nextMipSize;
    }
}

unsigned int GLRenderer::Expand1555to8888(unsigned short c)
{
    if (c == 0) return 0x00000000;

    unsigned int r = (c >> 10) & 0x1F;
    unsigned int g = (c >> 5) & 0x1F;
    unsigned int b = c & 0x1F;

    r = (r << 3) | (r >> 2);
    g = (g << 3) | (g >> 2);
    b = (b << 3) | (b >> 2);

    return 0xFF000000 | (b << 16) | (g << 8) | r;
}

std::array<Vector2df, 3> GLRenderer::GetTerrainUVs(bool reverse, bool second, int direction)
{
    const float tcMin = static_cast<float>(TCMIN) / (128.0f * 65536.0f);
    const float tcMax = static_cast<float>(TCMAX) / (128.0f * 65536.0f);

    auto uv = [](float u, float v) -> Vector2df {
        return Vector2df{u, v};
    };

    if (reverse) {
        if (second) {
            switch (direction) {
            case 0: return {uv(tcMin, tcMax), uv(tcMax, tcMin), uv(tcMax, tcMax)};
            case 1: return {uv(tcMax, tcMax), uv(tcMin, tcMin), uv(tcMax, tcMin)};
            case 2: return {uv(tcMax, tcMin), uv(tcMin, tcMax), uv(tcMin, tcMin)};
            default: return {uv(tcMin, tcMin), uv(tcMax, tcMax), uv(tcMin, tcMax)};
            }
        }

        switch (direction) {
        case 0: return {uv(tcMin, tcMin), uv(tcMax, tcMin), uv(tcMin, tcMax)};
        case 1: return {uv(tcMin, tcMax), uv(tcMin, tcMin), uv(tcMax, tcMax)};
        case 2: return {uv(tcMax, tcMax), uv(tcMin, tcMax), uv(tcMax, tcMin)};
        default: return {uv(tcMax, tcMin), uv(tcMax, tcMax), uv(tcMin, tcMin)};
        }
    }

    if (second) {
        switch (direction) {
        case 0: return {uv(tcMin, tcMin), uv(tcMax, tcMax), uv(tcMin, tcMax)};
        case 1: return {uv(tcMin, tcMax), uv(tcMax, tcMin), uv(tcMax, tcMax)};
        case 2: return {uv(tcMax, tcMax), uv(tcMin, tcMin), uv(tcMax, tcMin)};
        default: return {uv(tcMax, tcMin), uv(tcMin, tcMax), uv(tcMin, tcMin)};
        }
    }

    switch (direction) {
    case 0: return {uv(tcMin, tcMin), uv(tcMax, tcMin), uv(tcMax, tcMax)};
    case 1: return {uv(tcMin, tcMax), uv(tcMin, tcMin), uv(tcMax, tcMin)};
    case 2: return {uv(tcMax, tcMax), uv(tcMin, tcMax), uv(tcMin, tcMin)};
    default: return {uv(tcMax, tcMin), uv(tcMax, tcMax), uv(tcMin, tcMax)};
    }
}

std::array<float, 16> GLRenderer::BuildLegacyProjection()
{
    const float nearPlane = 16.0f;
    const float farPlaneCandidate = static_cast<float>(ctViewR * 256 + 4096);
    const float farPlane = nearPlane + 1.0f > farPlaneCandidate ? nearPlane + 1.0f : farPlaneCandidate;
    const float left = -nearPlane * static_cast<float>(VideoCX) / CameraW;
    const float right = nearPlane * static_cast<float>(WinW - VideoCX) / CameraW;
    const float top = nearPlane * static_cast<float>(VideoCY) / CameraH;
    const float bottom = -nearPlane * static_cast<float>(WinH - VideoCY) / CameraH;

    return {
        (2.0f * nearPlane) / (right - left), 0.0f, 0.0f, 0.0f,
        0.0f, (2.0f * nearPlane) / (top - bottom), 0.0f, 0.0f,
        (right + left) / (right - left), (top + bottom) / (top - bottom), -(farPlane + nearPlane) / (farPlane - nearPlane), -1.0f,
        0.0f, 0.0f, -(2.0f * farPlane * nearPlane) / (farPlane - nearPlane), 0.0f
    };
}

Vector3d GLRenderer::DecodeFogColor(int rgb)
{
    return {
        static_cast<float>(rgb & 0xFF) / 255.0f,
        static_cast<float>((rgb >> 8) & 0xFF) / 255.0f,
        static_cast<float>((rgb >> 16) & 0xFF) / 255.0f
    };
}

Vector3d GLRenderer::GetCurrentFogColor()
{
    if (IsUnderwater() && FogsList[127].fogRGB) {
        return DecodeFogColorBGR(FogsList[127].fogRGB);
    }

    if (CAMERAINFOG && CameraFogI > 0) {
        return DecodeFogColor(FogsList[CameraFogI].fogRGB);
    }

    // When not in a fog volume, always return the sky color.
    // The previous code fell through to CurFogColor (a CalcFogLevel
    // side-effect) which could be a stale fog pocket color from a
    // terrain vertex lookup, causing a color mismatch with the sky.
    return {
        static_cast<float>(SkyR) / 255.0f,
        static_cast<float>(SkyG) / 255.0f,
        static_cast<float>(SkyB) / 255.0f
    };
}

Vector3d GLRenderer::GetFogColorForMapPoint(int mapX, int mapY)
{
    if (IsUnderwater()) {
        return GetDistanceFogColor();
    }

    const int fogIndex = GetFogIndexForMapPoint(mapX, mapY);
    if (FOGON && fogIndex > 0) {
        return DecodeFogColor(FogsList[fogIndex].fogRGB);
    }

    return GetDistanceFogColor();
}

// Phase 2.x: fogIndex overload — caller already computed GetFogIndexForMapPoint.
// Eliminates the duplicate FogsMap lookup when both fog amount and fog color
// are needed for the same (x,y) corner.
Vector3d GLRenderer::GetFogColorForMapPoint(int fogIndex)
{
    if (IsUnderwater()) {
        return GetDistanceFogColor();
    }

    if (FOGON && fogIndex > 0) {
        return DecodeFogColor(FogsList[fogIndex].fogRGB);
    }

    return GetDistanceFogColor();
}

float GLRenderer::Clamp01(float value)
{
    if (value < 0.0f) return 0.0f;
    if (value > 1.0f) return 1.0f;
    return value;
}

static float VertexDistanceSq(const Vector3d& v)
{
    return v.x * v.x + v.y * v.y + v.z * v.z;
}

bool GLRenderer::IsWaterTriangleValid(const EPoint& v0, const EPoint& v1, const EPoint& v2, float backR)
{
    // Only cull if ALL vertices are beyond the far plane.
    // OpenGL's hardware clipper handles near-plane and screen-edge
    // clipping, so we must not CPU-cull based on DFlags (near plane
    // or screen bounds) or individual-vertex far-plane checks.
    if (v0.v.z > backR && v1.v.z > backR && v2.v.z > backR) {
        return false;
    }

    return true;
}

static float CalcTerrainAlpha(float distanceSq, float fadeStart, float fadeStartSq, float fadeEnd)
{
    if (IsUnderwater()) {
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

float GLRenderer::CalcWaterAlpha(const EPoint& vertex, float centerDistanceSq, float fadeStart, float fadeStartSq, float fadeEnd)
{
    float alpha = Clamp01(vertex.ALPHA / 255.0f);

    if (!IsUnderwater() && centerDistanceSq > fadeStartSq) {
        const float distanceSq = VertexDistanceSq(vertex.v);
        if (distanceSq > fadeStartSq) {
            const float zz = std::sqrt(distanceSq) - fadeEnd;
            if (zz > 0.0f) {
                alpha = Clamp01((255.0f - zz / 3.0f) / 255.0f);
            }
        }
    }

    return alpha;
}

void GLRenderer::AppendTerrainTriangle(std::vector<TerrainVertex>& vertices,
                                       const EPoint& v0,
                                       const EPoint& v1,
                                       const EPoint& v2,
                                       const Vector3d& fogColor0,
                                       const Vector3d& fogColor1,
                                       const Vector3d& fogColor2,
                                       int textureLayer,
                                       bool reverse,
                                       bool second,
                                       int direction,
                                       float alpha0,
                                       float alpha1,
                                       float alpha2)
{
    const auto uv = GetTerrainUVs(reverse, second, direction);
    const float layer = static_cast<float>(textureLayer);

    // Phase 1.5: pack light/fog (0..200, treated as 0..255 in the
    // shader) as uint8, alpha as uint8, and per-vertex fog color as a
    // vec3 of uint8. The driver normalizes the uint8 back to [0,1] in
    // the vertex shader, matching the old float layout.
    vertices.push_back({v0.v.x, v0.v.y, v0.v.z, uv[0].x, uv[0].y, layer,
                        Light255ToByte(static_cast<float>(v0.Light)),
                        Light255ToByte(v0.Fog),
                        Float01ToByte(alpha0),
                        0,  // pad1
                        Float01ToByte(fogColor0.x),
                        Float01ToByte(fogColor0.y),
                        Float01ToByte(fogColor0.z),
                        0});  // pad2
    vertices.push_back({v1.v.x, v1.v.y, v1.v.z, uv[1].x, uv[1].y, layer,
                        Light255ToByte(static_cast<float>(v1.Light)),
                        Light255ToByte(v1.Fog),
                        Float01ToByte(alpha1),
                        0,  // pad1
                        Float01ToByte(fogColor1.x),
                        Float01ToByte(fogColor1.y),
                        Float01ToByte(fogColor1.z),
                        0});
    vertices.push_back({v2.v.x, v2.v.y, v2.v.z, uv[2].x, uv[2].y, layer,
                        Light255ToByte(static_cast<float>(v2.Light)),
                        Light255ToByte(v2.Fog),
                        Float01ToByte(alpha2),
                        0,  // pad1
                        Float01ToByte(fogColor2.x),
                        Float01ToByte(fogColor2.y),
                        Float01ToByte(fogColor2.z),
                        0});
}

float GetTerrainFogAmountForMapPoint(int mapX, int mapY, int legacyFog)
{
    if (IsUnderwater()) {
        return static_cast<float>(legacyFog);
    }

    if (!FOGON || GetFogIndexForMapPoint(mapX, mapY) <= 0) {
        return 0.0f;
    }

    return static_cast<float>(std::clamp(legacyFog, 0, 255));
}

// Phase 2.x: fogIndex overload — caller already computed GetFogIndexForMapPoint.
// Eliminates the duplicate FogsMap lookup when both fog amount and fog color
// are needed for the same (x,y) corner.
static float GetTerrainFogAmountForMapPoint(int fogIndex, int legacyFog)
{
    if (IsUnderwater()) {
        return static_cast<float>(legacyFog);
    }

    if (!FOGON || fogIndex <= 0) {
        return 0.0f;
    }

    return static_cast<float>(std::clamp(legacyFog, 0, 255));
}

void GLRenderer::AppendWaterTriangle(std::vector<TerrainVertex>& vertices,
                                     const EPoint& v0,
                                     const EPoint& v1,
                                     const EPoint& v2,
                                     const Vector3d& fogColor0,
                                     const Vector3d& fogColor1,
                                     const Vector3d& fogColor2,
                                     int textureLayer,
                                     bool reverse,
                                     bool second,
                                     int direction,
                                     float alpha0,
                                     float alpha1,
                                     float alpha2,
                                     float fadeEnabled)
{
    const auto uv = GetTerrainUVs(reverse, second, direction);
    const float layer = static_cast<float>(textureLayer);

    // Mark the water texture layer as used this frame so RenderWaterSurface
    // can skip its O(m_waterVertices) layer scan. All three Collect paths
    // (Fast, Tile, Tile2) go through this function, so the mark is
    // centralized here. The bounds check is defensive — callers already
    // validate, but a stray out-of-range layer would corrupt the array.
    if (textureLayer >= 0 && textureLayer < kMaxTerrainTextureLayers) {
        m_waterUsedLayers[textureLayer] = true;
    }

    // Phase 1.5: same uint8 packing as AppendTerrainTriangle. v0.Fog
    // is in 0..200 from CalcFogLevel (clamped to FLimit, typically
    // 200) and the old shader divided it by 255, so the uint8 packing
    // matches byte-for-byte.
    vertices.push_back({v0.v.x, v0.v.y, v0.v.z, uv[0].x, uv[0].y, layer,
                        Light255ToByte(static_cast<float>(v0.Light)),
                        Light255ToByte(v0.Fog),
                        Float01ToByte(alpha0),
                        Float01ToByte(fadeEnabled),
                        Float01ToByte(fogColor0.x),
                        Float01ToByte(fogColor0.y),
                        Float01ToByte(fogColor0.z),
                        0});
    vertices.push_back({v1.v.x, v1.v.y, v1.v.z, uv[1].x, uv[1].y, layer,
                        Light255ToByte(static_cast<float>(v1.Light)),
                        Light255ToByte(v1.Fog),
                        Float01ToByte(alpha1),
                        Float01ToByte(fadeEnabled),
                        Float01ToByte(fogColor1.x),
                        Float01ToByte(fogColor1.y),
                        Float01ToByte(fogColor1.z),
                        0});
    vertices.push_back({v2.v.x, v2.v.y, v2.v.z, uv[2].x, uv[2].y, layer,
                        Light255ToByte(static_cast<float>(v2.Light)),
                        Light255ToByte(v2.Fog),
                        Float01ToByte(alpha2),
                        Float01ToByte(fadeEnabled),
                        Float01ToByte(fogColor2.x),
                        Float01ToByte(fogColor2.y),
                        Float01ToByte(fogColor2.z),
                        0});
}

void GLRenderer::CollectTerrainTile(int x, int y, int r)
{
    (void)r;

    if (x >= ctMapSize - 1 || y >= ctMapSize - 1 || x < 0 || y < 0) {
        return;
    }

    float backR = BackViewR;
    if (OMap[y][x] != 255) {
        backR += MObjects[OMap[y][x]].info.BoundR;
    }

    const int localX = x - CCX + kViewGridCenter;
    const int localY = y - CCY + kViewGridCenter;
    if (localX < 0 || localY < 0 || localX + 1 >= kViewGridSize || localY + 1 >= kViewGridSize) {
        return;
    }

    // Coarse frustum pre-test using map coordinates + HMapO height
    // estimate.  Avoids reading 4 VMap EPoints (64 bytes) for tiles
    // that are clearly outside the horizontal frustum.
    // Uses a generous margin (backR*2 + 2048) so we never false-reject
    // tiles that the precise VMap-based test would accept.
    {
        const float wx = static_cast<float>(x * 256 + 128) - CameraX;
        const float wz = static_cast<float>(y * 256 + 128) - CameraZ;
        const float wy = static_cast<float>(HMapO[y][x]) * ctHScale - CameraY;
        const float cx  = wx * ca + wz * sa;
        const float cz1 = wz * ca - wx * sa;
        const float cz  = cz1 * cb + wy * sb;
        if (std::fabs(cx * FOVK) > -cz + backR * 2.0f + 2048.0f) {
            return;
        }
    }

    // Fetch vertices — needed for precise frustum + distance culling
    EPoint v00 = VMap[localY][localX];
    if (v00.v.z > backR) {
        return;
    }

    EPoint v10 = VMap[localY][localX + 1];
    EPoint v01 = VMap[localY + 1][localX];
    EPoint v11 = VMap[localY + 1][localX + 1];

    // Precise frustum cull using VMap vertices
    const float xx = (v00.v.x + v11.v.x) * 0.5f;
    const float yy = (v00.v.y + v11.v.y) * 0.5f;
    const float zz = (v00.v.z + v11.v.z) * 0.5f;

    if (std::fabs(xx * FOVK) > -zz + backR) {
        return;
    }

    // Distance cull
    const float viewDistance = static_cast<float>(ctViewR * 256);
    const float viewDistanceSq = viewDistance * viewDistance;
    const float distanceSq = xx * xx + yy * yy + zz * zz;
    if (distanceSq > viewDistanceSq) {
        return;
    }

    // Tile survived culling — now compute fog + alpha
    const float fadeStart = static_cast<float>((ctViewR - 8) << 8);
    const float fadeStartSq = fadeStart * fadeStart;
    const float fadeEnd = 256.0f * static_cast<float>(ctViewR - 4);

    const int fogIdx00 = GetFogIndexForMapPoint(x, y);
    const int fogIdx10 = GetFogIndexForMapPoint(x + 1, y);
    const int fogIdx01 = GetFogIndexForMapPoint(x, y + 1);
    const int fogIdx11 = GetFogIndexForMapPoint(x + 1, y + 1);

    v00.Fog = static_cast<int>(GetTerrainFogAmountForMapPoint(fogIdx00, v00.Fog));
    v10.Fog = static_cast<int>(GetTerrainFogAmountForMapPoint(fogIdx10, v10.Fog));
    v01.Fog = static_cast<int>(GetTerrainFogAmountForMapPoint(fogIdx01, v01.Fog));
    v11.Fog = static_cast<int>(GetTerrainFogAmountForMapPoint(fogIdx11, v11.Fog));

    const Vector3d fog00 = GetFogColorForMapPoint(fogIdx00);
    const Vector3d fog10 = GetFogColorForMapPoint(fogIdx10);
    const Vector3d fog01 = GetFogColorForMapPoint(fogIdx01);
    const Vector3d fog11 = GetFogColorForMapPoint(fogIdx11);

    const float alpha00 = CalcTerrainAlpha(VertexDistanceSq(v00.v), fadeStart, fadeStartSq, fadeEnd);
    const float alpha10 = CalcTerrainAlpha(VertexDistanceSq(v10.v), fadeStart, fadeStartSq, fadeEnd);
    const float alpha01 = CalcTerrainAlpha(VertexDistanceSq(v01.v), fadeStart, fadeStartSq, fadeEnd);
    const float alpha11 = CalcTerrainAlpha(VertexDistanceSq(v11.v), fadeStart, fadeStartSq, fadeEnd);

    const bool reverse = (FMap[y][x] & fmReverse) != 0;
    const int direction = FMap[y][x] & 3;

    const int textureLayer = TMap1[y][x];
    if (textureLayer >= 0 && textureLayer < kMaxTerrainTextureLayers && Textures[textureLayer]) {
        if (reverse) {
            AppendTerrainTriangle(m_terrainVertices, v00, v10, v01, fog00, fog10, fog01, textureLayer, reverse, false, direction, alpha00, alpha10, alpha01);
            AppendTerrainTriangle(m_terrainVertices, v01, v10, v11, fog01, fog10, fog11, textureLayer, reverse, true, direction, alpha01, alpha10, alpha11);
        } else {
            AppendTerrainTriangle(m_terrainVertices, v00, v10, v11, fog00, fog10, fog11, textureLayer, reverse, false, direction, alpha00, alpha10, alpha11);
            AppendTerrainTriangle(m_terrainVertices, v00, v11, v01, fog00, fog11, fog01, textureLayer, reverse, true, direction, alpha00, alpha11, alpha01);
        }
    }

    RenderObject(x, y);
}

void GLRenderer::CollectTerrainTile2(int x, int y, int r)
{
    (void)r;

    if (x >= ctMapSize - 2 || y >= ctMapSize - 2 || x < 0 || y < 0) {
        return;
    }

    const int localX = x - CCX + kViewGridCenter;
    const int localY = y - CCY + kViewGridCenter;
    if (localX < 0 || localY < 0 || localX + 2 >= kViewGridSize || localY + 2 >= kViewGridSize) {
        return;
    }

    // Coarse frustum pre-test (same pattern as CollectTerrainTile)
    {
        const float wx = static_cast<float>(x * 256 + 128) - CameraX;
        const float wz = static_cast<float>(y * 256 + 128) - CameraZ;
        const float wy = static_cast<float>(HMapO[y][x]) * ctHScale - CameraY;
        const float cx  = wx * ca + wz * sa;
        const float cz1 = wz * ca - wx * sa;
        const float cz  = cz1 * cb + wy * sb;
        if (std::fabs(cx * FOVK) > -cz + BackViewR * 2.0f + 2048.0f) {
            return;
        }
    }

    EPoint v00 = VMap[localY][localX];
    if (v00.v.z > BackViewR) {
        return;
    }

    const int textureLayer = TMap2[y][x];

    EPoint v20 = VMap[localY][localX + 2];
    EPoint v02 = VMap[localY + 2][localX];
    EPoint v22 = VMap[localY + 2][localX + 2];

    // Frustum + distance culls before fog computation
    const float xx = (v00.v.x + v22.v.x) * 0.5f;
    const float yy = (v00.v.y + v22.v.y) * 0.5f;
    const float zz = (v00.v.z + v22.v.z) * 0.5f;

    if (std::fabs(xx * FOVK) > -zz + BackViewR) {
        return;
    }

    const float viewDistance = static_cast<float>(ctViewR * 256);
    const float viewDistanceSq = viewDistance * viewDistance;
    const float distanceSq = xx * xx + yy * yy + zz * zz;
    if (distanceSq > viewDistanceSq) {
        return;
    }

    // Tile survived — now compute fog + alpha
    const float fadeStart = static_cast<float>((ctViewR - 8) << 8);
    const float fadeStartSq = fadeStart * fadeStart;
    const float fadeEnd = 256.0f * static_cast<float>(ctViewR - 4);

    const int fogIdx00 = GetFogIndexForMapPoint(x, y);
    const int fogIdx20 = GetFogIndexForMapPoint(x + 2, y);
    const int fogIdx02 = GetFogIndexForMapPoint(x, y + 2);
    const int fogIdx22 = GetFogIndexForMapPoint(x + 2, y + 2);

    v00.Fog = static_cast<int>(GetTerrainFogAmountForMapPoint(fogIdx00, v00.Fog));
    v20.Fog = static_cast<int>(GetTerrainFogAmountForMapPoint(fogIdx20, v20.Fog));
    v02.Fog = static_cast<int>(GetTerrainFogAmountForMapPoint(fogIdx02, v02.Fog));
    v22.Fog = static_cast<int>(GetTerrainFogAmountForMapPoint(fogIdx22, v22.Fog));

    const Vector3d fog00 = GetFogColorForMapPoint(fogIdx00);
    const Vector3d fog20 = GetFogColorForMapPoint(fogIdx20);
    const Vector3d fog02 = GetFogColorForMapPoint(fogIdx02);
    const Vector3d fog22 = GetFogColorForMapPoint(fogIdx22);

    const float alpha00 = CalcTerrainAlpha(VertexDistanceSq(v00.v), fadeStart, fadeStartSq, fadeEnd);
    const float alpha20 = CalcTerrainAlpha(VertexDistanceSq(v20.v), fadeStart, fadeStartSq, fadeEnd);
    const float alpha02 = CalcTerrainAlpha(VertexDistanceSq(v02.v), fadeStart, fadeStartSq, fadeEnd);
    const float alpha22 = CalcTerrainAlpha(VertexDistanceSq(v22.v), fadeStart, fadeStartSq, fadeEnd);

    const int direction = (FMap[y][x] >> 8) & 3;

    if (textureLayer >= 0 && textureLayer < kMaxTerrainTextureLayers && Textures[textureLayer]) {
        AppendTerrainTriangle(m_terrainVertices, v00, v20, v22, fog00, fog20, fog22, textureLayer, false, false, direction, alpha00, alpha20, alpha22);
        AppendTerrainTriangle(m_terrainVertices, v00, v22, v02, fog00, fog22, fog02, textureLayer, false, true, direction, alpha00, alpha22, alpha02);
    }

    // Primary cell only — neighbor cells are covered by adjacent
    // 2×2 tiles (same ring) or by the inner 1×1 loop (inner rings).
    // Calling all four would duplicate entries in m_objectList.
    RenderObject(x, y);
}

void GLRenderer::CollectWaterTileFast(int x, int y, int r,
                                      float viewDistanceSq,
                                      float fadeStart, float fadeStartSq,
                                      float fadeEnd, float fadeEndSq)
{
    (void)r;
    (void)fadeStart;
    (void)fadeEnd;
    (void)fadeEndSq;

    if (x >= ctMapSize - 1 || y >= ctMapSize - 1 || x < 0 || y < 0) {
        return;
    }

    if (!((FMap[y][x] & fmWaterA) && (FMap[y][x + 1] & fmWaterA) &&
          (FMap[y + 1][x] & fmWaterA) && (FMap[y + 1][x + 1] & fmWaterA))) {
        return;
    }

    const int textureLayer = WaterList[WMap[y][x]].tindex;
    if (textureLayer < 0 || textureLayer >= kMaxTerrainTextureLayers || !Textures[textureLayer]) {
        return;
    }

    const int localX = x - CCX + kViewGridCenter;
    const int localY = y - CCY + kViewGridCenter;
    if (localX < 0 || localY < 0 || localX + 1 >= kViewGridSize || localY + 1 >= kViewGridSize) {
        return;
    }

    // Coarse frustum pre-test — same as CollectTerrainTile, but using
    // HMapO as a rough height proxy for the water surface (water level
    // is typically close to terrain height).  Skips 4 VMap2 reads for
    // tiles outside the horizontal frustum.
    // Uses a generous margin to avoid false rejects.
    {
        const float wx = static_cast<float>(x * 256 + 128) - CameraX;
        const float wz = static_cast<float>(y * 256 + 128) - CameraZ;
        const float wy = static_cast<float>(HMapO[y][x]) * ctHScale - CameraY;
        const float cx  = wx * ca + wz * sa;
        const float cz1 = wz * ca - wx * sa;
        const float cz  = cz1 * cb + wy * sb;
        if (std::fabs(cx * FOVK) > -cz + BackViewR * 2.0f + 2048.0f) {
            return;
        }
    }

    EPoint v00 = VMap2[localY][localX];
    EPoint v10 = VMap2[localY][localX + 1];
    EPoint v01 = VMap2[localY + 1][localX];
    EPoint v11 = VMap2[localY + 1][localX + 1];

    const float xx = (v00.v.x + v11.v.x) * 0.5f;
    const float yy = (v00.v.y + v11.v.y) * 0.5f;
    const float zz = (v00.v.z + v11.v.z) * 0.5f;

    if (std::fabs(xx * FOVK) > -zz + BackViewR) {
        return;
    }

    const float centerDistanceSq = xx * xx + yy * yy + zz * zz;
    if (centerDistanceSq > viewDistanceSq) {
        return;
    }

    const float fadeEnabled = (!IsUnderwater() && centerDistanceSq > fadeStartSq) ? 1.0f : 0.0f;

    const float a00 = Clamp01(v00.ALPHA / 255.0f);
    const float a10 = Clamp01(v10.ALPHA / 255.0f);
    const float a01 = Clamp01(v01.ALPHA / 255.0f);
    const float a11 = Clamp01(v11.ALPHA / 255.0f);

    // Single FogsMap lookup for the tile center (water is flat; per-corner
    // fog precision is invisible — saves 3 FogsMap lookups per tile).
    const Vector3d fogTile = GetFogColorForMapPoint(x, y);

    if (a00 > 0.0f || a10 > 0.0f || a11 > 0.0f) {
        if (IsWaterTriangleValid(v00, v10, v11, BackViewR)) {
            AppendWaterTriangle(m_waterVertices, v00, v10, v11,
                               fogTile, fogTile, fogTile,
                               textureLayer, false, false, 0, a00, a10, a11, fadeEnabled);
        }
    }

    if (a00 > 0.0f || a11 > 0.0f || a01 > 0.0f) {
        if (IsWaterTriangleValid(v00, v11, v01, BackViewR)) {
            AppendWaterTriangle(m_waterVertices, v00, v11, v01,
                               fogTile, fogTile, fogTile,
                               textureLayer, false, true, 0, a00, a11, a01, fadeEnabled);
        }
    }
}

void GLRenderer::CollectWaterTile(int x, int y, int r)
{
    (void)r;

    if (x >= ctMapSize - 1 || y >= ctMapSize - 1 || x < 0 || y < 0) {
        return;
    }

    if (!((FMap[y][x] & fmWaterA) && (FMap[y][x + 1] & fmWaterA) &&
          (FMap[y + 1][x] & fmWaterA) && (FMap[y + 1][x + 1] & fmWaterA))) {
        return;
    }

    const int textureLayer = WaterList[WMap[y][x]].tindex;
    if (textureLayer < 0 || textureLayer >= kMaxTerrainTextureLayers || !Textures[textureLayer]) {
        return;
    }

    const int localX = x - CCX + kViewGridCenter;
    const int localY = y - CCY + kViewGridCenter;
    if (localX < 0 || localY < 0 || localX + 1 >= kViewGridSize || localY + 1 >= kViewGridSize) {
        return;
    }

    const float viewDistance = static_cast<float>(ctViewR * 256);
    const float viewDistanceSq = viewDistance * viewDistance;
    const float fadeStart = static_cast<float>((ctViewR - 8) << 8);
    const float fadeStartSq = fadeStart * fadeStart;
    const float fadeEnd = 256.0f * static_cast<float>(ctViewR - 4);

    EPoint v00 = VMap2[localY][localX];
    EPoint v10 = VMap2[localY][localX + 1];
    EPoint v01 = VMap2[localY + 1][localX];
    EPoint v11 = VMap2[localY + 1][localX + 1];

    const float xx = (v00.v.x + v11.v.x) * 0.5f;
    const float yy = (v00.v.y + v11.v.y) * 0.5f;
    const float zz = (v00.v.z + v11.v.z) * 0.5f;

    if (std::fabs(xx * FOVK) > -zz + BackViewR) {
        return;
    }

    const float centerDistanceSq = xx * xx + yy * yy + zz * zz;
    if (centerDistanceSq > viewDistanceSq) {
        return;
    }

    const float fadeEnabled = (!IsUnderwater() && centerDistanceSq > fadeStartSq) ? 1.0f : 0.0f;

    const float a00 = Clamp01(v00.ALPHA / 255.0f);
    const float a10 = Clamp01(v10.ALPHA / 255.0f);
    const float a01 = Clamp01(v01.ALPHA / 255.0f);
    const float a11 = Clamp01(v11.ALPHA / 255.0f);

    // Per-corner map-based fog color (mirrors the terrain path in
    // CollectTerrainTile). GetFogColorForMapPoint looks up the active
    // fog volume for each map cell, so water straddling a fog boundary
    // gets a per-vertex fog color that smoothly interpolates across the
    // surface. The previous version used GetCurrentFogColor() for all
    // three vertices, which lost the per-volume color and produced
    // a uniform tint across the whole water body.
    const Vector3d fog00 = GetFogColorForMapPoint(x, y);
    const Vector3d fog10 = GetFogColorForMapPoint(x + 1, y);
    const Vector3d fog01 = GetFogColorForMapPoint(x, y + 1);
    const Vector3d fog11 = GetFogColorForMapPoint(x + 1, y + 1);

    if (a00 > 0.0f || a10 > 0.0f || a11 > 0.0f) {
        if (IsWaterTriangleValid(v00, v10, v11, BackViewR)) {
            AppendWaterTriangle(m_waterVertices, v00, v10, v11, fog00, fog10, fog11, textureLayer, false, false, 0, a00, a10, a11, fadeEnabled);
        }
    }

    if (a00 > 0.0f || a11 > 0.0f || a01 > 0.0f) {
        if (IsWaterTriangleValid(v00, v11, v01, BackViewR)) {
            AppendWaterTriangle(m_waterVertices, v00, v11, v01, fog00, fog11, fog01, textureLayer, false, true, 0, a00, a11, a01, fadeEnabled);
        }
    }
}

void GLRenderer::CollectWaterTile2(int x, int y, int r)
{
    (void)r;

    if (x >= ctMapSize - 2 || y >= ctMapSize - 2 || x < 0 || y < 0) {
        return;
    }

    if (!((FMap[y][x] & fmWaterA) && (FMap[y][x + 2] & fmWaterA) &&
          (FMap[y + 2][x] & fmWaterA) && (FMap[y + 2][x + 2] & fmWaterA))) {
        return;
    }

    const int textureLayer = WaterList[WMap[y][x]].tindex;
    if (textureLayer < 0 || textureLayer >= kMaxTerrainTextureLayers || !Textures[textureLayer]) {
        return;
    }

    const int localX = x - CCX + kViewGridCenter;
    const int localY = y - CCY + kViewGridCenter;
    if (localX < 0 || localY < 0 || localX + 2 >= kViewGridSize || localY + 2 >= kViewGridSize) {
        return;
    }

    // Coarse frustum pre-test (same pattern as CollectTerrainTile2)
    {
        const float wx = static_cast<float>(x * 256 + 128) - CameraX;
        const float wz = static_cast<float>(y * 256 + 128) - CameraZ;
        const float wy = WaterList[WMap[y][x]].wlevel * ctHScale - CameraY;
        const float cx  = wx * ca + wz * sa;
        const float cz1 = wz * ca - wx * sa;
        const float cz  = cz1 * cb + wy * sb;
        if (std::fabs(cx * FOVK) > -cz + BackViewR * 2.0f + 2048.0f) {
            return;
        }
    }

    const float viewDistance = static_cast<float>(ctViewR * 256);
    const float viewDistanceSq = viewDistance * viewDistance;
    const float fadeStart = static_cast<float>((ctViewR - 8) << 8);
    const float fadeStartSq = fadeStart * fadeStart;
    const float fadeEnd = 256.0f * static_cast<float>(ctViewR - 4);

    EPoint v00 = VMap2[localY][localX];
    EPoint v20 = VMap2[localY][localX + 2];
    EPoint v02 = VMap2[localY + 2][localX];
    EPoint v22 = VMap2[localY + 2][localX + 2];

    const float xx = (v00.v.x + v22.v.x) * 0.5f;
    const float yy = (v00.v.y + v22.v.y) * 0.5f;
    const float zz = (v00.v.z + v22.v.z) * 0.5f;

    if (std::fabs(xx * FOVK) > -zz + BackViewR) {
        return;
    }

    const float centerDistanceSq = xx * xx + yy * yy + zz * zz;
    if (centerDistanceSq > viewDistanceSq) {
        return;
    }

    const float fadeEnabled = (!IsUnderwater() && centerDistanceSq > fadeStartSq) ? 1.0f : 0.0f;

    const float a00 = Clamp01(v00.ALPHA / 255.0f);
    const float a20 = Clamp01(v20.ALPHA / 255.0f);
    const float a02 = Clamp01(v02.ALPHA / 255.0f);
    const float a22 = Clamp01(v22.ALPHA / 255.0f);

    // Per-corner map-based fog color (far-detail water path; mirrors
    // the near-detail CollectWaterTile and the terrain path in
    // CollectTerrainTile2).
    const Vector3d fog00 = GetFogColorForMapPoint(x, y);
    const Vector3d fog20 = GetFogColorForMapPoint(x + 2, y);
    const Vector3d fog02 = GetFogColorForMapPoint(x, y + 2);
    const Vector3d fog22 = GetFogColorForMapPoint(x + 2, y + 2);

    if (a00 > 0.0f || a20 > 0.0f || a22 > 0.0f) {
        if (IsWaterTriangleValid(v00, v20, v22, BackViewR)) {
            AppendWaterTriangle(m_waterVertices, v00, v20, v22, fog00, fog20, fog22, textureLayer, false, false, 0, a00, a20, a22, fadeEnabled);
        }
    }

    if (a00 > 0.0f || a22 > 0.0f || a02 > 0.0f) {
        if (IsWaterTriangleValid(v00, v22, v02, BackViewR)) {
            AppendWaterTriangle(m_waterVertices, v00, v22, v02, fog00, fog22, fog02, textureLayer, false, true, 0, a00, a22, a02, fadeEnabled);
        }
    }
}

void GLRenderer::RenderGround()
{
#ifdef GL_PERF_HOOKS
    GL_PERF_SCOPE("RenderGround");
#endif
    BeginTerrainFrame();
    m_worldModelItems.clear();
    m_transparentModelItems.clear();
    m_objectList.clear();

    // If water is needed this frame, begin water collection here so we
    // can collect terrain + water vertices in a single ring walk instead
    // of two separate passes over the same ~1,800 tiles.
    if (NeedWater) {
        BeginWaterFrame();
    }

    // Precompute water distance/fade constants once for the unified walk.
    float wViewDistSq = 0.0f, wFadeStart = 0.0f, wFadeStartSq = 0.0f;
    float wFadeEnd = 0.0f, wFadeEndSq = 0.0f;
    if (NeedWater) {
        const float vd = static_cast<float>(ctViewR * 256);
        wViewDistSq = vd * vd;
        wFadeStart = static_cast<float>((ctViewR - 8) << 8);
        wFadeStartSq = wFadeStart * wFadeStart;
        wFadeEnd = 256.0f * static_cast<float>(ctViewR - 4);
        wFadeEndSq = wFadeEnd * wFadeEnd;
    }

    // Terrain and water share the same LOD boundary (ctViewR1). Water is
    // flat, so 2x2 tiles are geometrically identical to 1x1 — this is
    // always a pure win. Terrain 2x2 trades a slight blur for fewer
    // vertices, controlled by the TerrainLOD percentage slider.
    for (int r = ctViewR; r > ctViewR1; --r) {
        for (int x = -r; x <= r; ++x) {
            if (ctViewR1 < ctViewR) {
                CollectTerrainTile2(CCX + x, CCY + r, r);
                CollectTerrainTile2(CCX + x, CCY - r, r);
            }
            if (NeedWater) {
                CollectWaterTile2(CCX + x, CCY + r, r);
                CollectWaterTile2(CCX + x, CCY - r, r);
            }
        }
        for (int y = -r + 1; y < r; ++y) {
            if (ctViewR1 < ctViewR) {
                CollectTerrainTile2(CCX + r, CCY + y, r);
                CollectTerrainTile2(CCX - r, CCY + y, r);
            }
            if (NeedWater) {
                CollectWaterTile2(CCX + r, CCY + y, r);
                CollectWaterTile2(CCX - r, CCY + y, r);
            }
        }
    }

    for (int r = ctViewR1; r > 0; --r) {
        for (int x = -r; x <= r; ++x) {
            CollectTerrainTile(CCX + x, CCY + r, r);
            CollectTerrainTile(CCX + x, CCY - r, r);
            if (NeedWater) {
                CollectWaterTileFast(CCX + x, CCY + r, r, wViewDistSq,
                                     wFadeStart, wFadeStartSq,
                                     wFadeEnd, wFadeEndSq);
                CollectWaterTileFast(CCX + x, CCY - r, r, wViewDistSq,
                                     wFadeStart, wFadeStartSq,
                                     wFadeEnd, wFadeEndSq);
            }
        }
        for (int y = -r + 1; y < r; ++y) {
            CollectTerrainTile(CCX + r, CCY + y, r);
            CollectTerrainTile(CCX - r, CCY + y, r);
            if (NeedWater) {
                CollectWaterTileFast(CCX + r, CCY + y, r, wViewDistSq,
                                     wFadeStart, wFadeStartSq,
                                     wFadeEnd, wFadeEndSq);
                CollectWaterTileFast(CCX - r, CCY + y, r, wViewDistSq,
                                     wFadeStart, wFadeStartSq,
                                     wFadeEnd, wFadeEndSq);
            }
        }
    }

    CollectTerrainTile(CCX, CCY, 0);
    if (NeedWater) {
        CollectWaterTileFast(CCX, CCY, 0, wViewDistSq,
                             wFadeStart, wFadeStartSq,
                             wFadeEnd, wFadeEndSq);
    }

    RenderTerrain();
}

void GLRenderer::RenderTerrain()
{
    if (m_terrainVertices.empty()) {
        return;
    }

    EnsureTerrainTextureArray();

    for (int layer = 0; layer < kMaxTerrainTextureLayers; ++layer) {
        if (!Textures[layer]) {
            continue;
        }
        if (m_uploadedTerrainTextures[layer] != Textures[layer].get()) {
            UploadTerrainLayer(layer, *Textures[layer]);
            m_uploadedTerrainTextures[layer] = Textures[layer].get();
        }
    }

    UpdatePerFrameUBO();
    SetWaterAlphaFade(0.0f, 0.0f, 0.0f, 765.0f);
    glUseProgram(m_terrainShader);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D_ARRAY, m_terrainTextureArray);
#ifdef GL_PERF_HOOKS
    GL_PERF_TEXTURE_BIND(m_terrainTextureArray);
#endif
    glBindVertexArray(m_terrainVAO);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_TRUE);
    DrawVertexBatch(m_terrainVertices);

    glBindVertexArray(0);
}

void GLRenderer::RenderWater()
{
    if (!NeedWater) {
        return;
    }

    // Water vertices were already collected during the unified ring walk
    // in RenderGround().  Only the GL draw call remains here — it must
    // stay after the model/shadow passes for correct alpha blending order.
    RenderWaterSurface();
}

void GLRenderer::RenderWCircles()
{
    // Water circles are wave ripples spawned by wading dinosaurs, the player,
    // and projectile impacts. They are full 3D morphed models (WCircleModel),
    // not 2D circles — so we use the model pipeline with CreateMorphedModel.
    // The D3D/3DFX legacy renderers call RenderWCircles() from inside their
    // own RenderWater() and use additive blending. We follow the same pattern
    // by routing through the IRenderer hook and drawing with additive=true.
    // See Hunt/RendererD3D.cpp:3288 and Hunt/Render3DFX.cpp:2184 for the
    // reference implementations.

    if (WCCount <= 0) {
        return;
    }

    Vector3d rpos;
    for (int c = 0; c < WCCount; c++) {
        TWCircle* wptr = &WCircles[c];
        rpos.x = wptr->pos.x - CameraX;
        rpos.y = wptr->pos.y - CameraY;
        rpos.z = wptr->pos.z - CameraZ;

        // Distance cull against ctViewR (matches D3D/3DFX ring-based cull).
        const float r = static_cast<float>(MAX(fabs(rpos.x), fabs(rpos.z)));
        int ri = -1 + static_cast<int>(r / 256.0f + 0.4f);
        if (ri < 0) ri = 0;
        if (ri > ctViewR) continue;

        rpos = RotateVector(rpos);

        // Frustum cull against BackViewR (matches D3D/3DFX cone test).
        if (rpos.z > BackViewR) continue;
        if (fabs(rpos.x) > -rpos.z + BackViewR) continue;
        if (fabs(rpos.y) > -rpos.z + BackViewR) continue;

        // Alpha fades from ~52 down to 0 as FTime advances from 0 to 2000.
        // D3D: GlassL = 255 - (2000 - FTime) / 38   => baseAlpha = (255 - GlassL) / 255
        // i.e. the alpha is exactly the same expression the shader reads from GlassL.
        GlassL = 255 - (2000 - wptr->FTime) / 38;

        CreateMorphedModel(WCircleModel.mptr.get(), &WCircleModel.Animation[0],
                           static_cast<int>(wptr->FTime), wptr->scale);

        // Build the draw item directly with additive=true. We can't go through
        // RenderModelClip / RenderModelClipWater because the public IRenderer
        // overrides don't expose the additive flag (other renderers don't need it).
        ModelDrawItem item;
        const bool closeEnough = fabs(rpos.z) + fabs(rpos.x) < 1000.0f;
        if (!BuildModelDrawItem(item, WCircleModel.mptr.get(),
                                rpos.x, rpos.y, rpos.z, 250, 0, 0, CameraBeta,
                                false, false, closeEnough, /*additive=*/true)) {
            continue;
        }
        item.texture = UploadModelTexture(WCircleModel.mptr.get());
        if (!item.texture) {
            continue;
        }
        m_worldModelItems.push_back(std::move(item));
    }

    GlassL = 0;  // reset for subsequent pass

    // Drain the water-circle items we just queued with additive blending.
    // m_worldModelItems should only contain water circles at this point:
    // RenderGround() clears it at the start of the frame, and RenderModelsList()
    // (which calls RenderWorldModels()) already drained it before we got here.
    RenderWorldModels();
}

void GLRenderer::DrawVertexBatch(const std::vector<TerrainVertex>& vertices) const
{
    if (vertices.empty() || !m_terrainVBO) {
        return;
    }

    const GLsizeiptr vertexSize = static_cast<GLsizeiptr>(vertices.size() * sizeof(TerrainVertex));
    glBindBuffer(GL_ARRAY_BUFFER, m_terrainVBO);
    glBufferData(GL_ARRAY_BUFFER, vertexSize, nullptr, GL_STREAM_DRAW);
    glBufferSubData(GL_ARRAY_BUFFER, 0, vertexSize, vertices.data());
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertices.size()));
#ifdef GL_PERF_HOOKS
    GL_PERF_DRAW(static_cast<uint32_t>(vertices.size()) / 3);
#endif
}

void GLRenderer::DrawFrame(const RenderFrameContext& ctx)
{
    // Phase 2.1: initial implementation — forward to DrawScene.
    // Future: replace global reads with ctx.WinW, ctx.fogColor, etc.
    (void)ctx;
    DrawScene();
}

void GLRenderer::DrawScene()
{
    RenderGround();
    RenderWater();
}

void GLRenderer::DrawPostObjects()
{
}

void GLRenderer::RegisterTexture(TEXTURE* tptr)
{
    (void)tptr;
}

void GLRenderer::ReleaseModelTextures(const TModel* mptr)
{
    if (!mptr) {
        return;
    }

    const auto it = m_modelTextureCache.find(mptr);
    if (it == m_modelTextureCache.end()) {
        return;
    }

    const GLuint texture = it->second;
    m_modelTextureCache.erase(it);

    if (texture && m_hrc) {
        glDeleteTextures(1, &texture);
    }
}

void GLRenderer::ResetTerrainTextureCache()
{
    m_uploadedTerrainTextures.fill(nullptr);
}

void GLRenderer::ClearLevelTextureCache()
{
    // Clear per-level model texture and mesh caches between levels.
    // Per-level TModels are heap-allocated (unique addresses across
    // levels), so address recycling is not a concern, but the cache
    // entries and GPU resources from the previous level are dead
    // weight. Clearing them here prevents unbounded VRAM growth.
    // Global models (ChInfo, SunModel, etc.) survive this clear;
    // their textures and meshes are re-uploaded on first use next
    // level (a one-time cost per level transition).

    for (const auto& item : m_modelTextureCache) {
        if (item.second) {
            glDeleteTextures(1, &item.second);
        }
    }
    m_modelTextureCache.clear();

    for (const auto& item : m_bmpTextureCache) {
        if (item.second) {
            glDeleteTextures(1, &item.second);
        }
    }
    m_bmpTextureCache.clear();

    // Static mesh cache entries hold VBO/IBO offsets that are no longer
    // valid for the new level's models. Clear the map so UploadStaticMesh
    // re-uploads fresh geometry. The VBO/IBO data is orphaned but will be
    // reused when EnsureStaticMeshCapacity regrows the buffers.
    m_staticMeshCache.clear();

    m_skyTextureDirty = true;
}

void GLRenderer::ClearVideoBuf()
{
    // NOTE: do NOT reset m_uploadedTerrainTextures here — ClearVideoBuf is
    // called every frame from RenderSkyPlane(). Invalidating the cache
    // per-frame forces RenderTerrain() and RenderWaterSurface() to re-upload
    // every terrain texture layer on every draw (~10-20 glTexSubImage3D
    // calls per frame), which tanks CPU and GPU performance.
    const Vector3d fogColor = GetDistanceFogColor();
    glClearColor(fogColor.x, fogColor.y, fogColor.z, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void GLRenderer::WaitRetrace()
{
}

void GLRenderer::PostProcess()
{
    if (m_hdc && m_hwnd) {
        SwapBuffers(m_hdc);
    }
}

void GLRenderer::DrawTPlane(bool clip)
{
    (void)clip;
}

void GLRenderer::DrawTPlaneClip(bool clip)
{
    (void)clip;
}

void GLRenderer::DrawHMap()
{
}

void GLRenderer::RenderCharacter(TCharacter* cptr)
{
    (void)cptr;
}

void GLRenderer::RenderExplosion(int index)
{
    (void)index;
}

void GLRenderer::RenderShip()
{
}

void GLRenderer::RenderPlayer(int index)
{
    (void)index;
}

void GLRenderer::InitializeSkyPipeline()
{
    const char* vsSource =
        "#version 330 core\n"
        "out vec2 vNdc;\n"
        "const vec2 kPositions[3] = vec2[3](\n"
        "   vec2(-1.0, -1.0),\n"
        "   vec2( 3.0, -1.0),\n"
        "   vec2(-1.0,  3.0)\n"
        ");\n"
        "void main() {\n"
        "   vec2 pos = kPositions[gl_VertexID];\n"
        "   vNdc = pos;\n"
        "   gl_Position = vec4(pos, 0.0, 1.0);\n"
        "}";

    const char* fsSource =
        "#version 330 core\n"
        "in vec2 vNdc;\n"
        "out vec4 FragColor;\n"
        "uniform PerFrame {\n"
        "   mat4 uProjection;\n"
        "   vec2 uFogRange;\n"
        "   vec3 uDistanceFogColor;\n"
        "   float uForceFog;\n"
        "   vec3 uFogColor;\n"
        "};\n"
        "uniform sampler2D uSkyTexture;\n"
        "uniform vec2 uViewport;\n"
        "uniform vec2 uVideoCenter;\n"
        "uniform vec3 uQ;\n"
        "uniform vec3 uP;\n"
        "uniform vec3 uR;\n"
        "uniform float uSkyTime;\n"
        "uniform float uFogBase;\n"
        "void main() {\n"
        "   vec2 pixel = vec2((vNdc.x * 0.5 + 0.5) * uViewport.x,\n"
        "                     (1.0 - (vNdc.y * 0.5 + 0.5)) * uViewport.y);\n"
        "   float sx = pixel.x - uVideoCenter.x;\n"
        "   float sy = uVideoCenter.y - pixel.y;\n"
        "   float sxQ = uQ.x * sx + uQ.y * sy + uQ.z;\n"
        "   float q = sign(sxQ) * max(abs(sxQ), 0.001);\n"
        "   float skyU = (uP.x * sx + uP.y * sy + uP.z) / q;\n"
        "   float skyV = (uR.x * sx + uR.y * sy + uR.z) / q;\n"
        "   float leftQ = uQ.x * (-uVideoCenter.x) + uQ.y * sy + uQ.z;\n"
        "   float rightQ = uQ.x * uVideoCenter.x + uQ.y * sy + uQ.z;\n"
        "   float leftU = (uP.x * (-uVideoCenter.x) + uP.y * sy + uP.z) / max(abs(leftQ), 0.001);\n"
        "   float leftV = (uR.x * (-uVideoCenter.x) + uR.y * sy + uR.z) / max(abs(leftQ), 0.001);\n"
        "   float rightU = (uP.x * uVideoCenter.x + uP.y * sy + uP.z) / max(abs(rightQ), 0.001);\n"
        "   float rightV = (uR.x * uVideoCenter.x + uR.y * sy + uR.z) / max(abs(rightQ), 0.001);\n"
        "   float dx = rightU - leftU;\n"
        "   float dy = rightV - leftV;\n"
        "   float dt = sqrt(dx*dx + dy*dy) / 96.0 - 6.0;\n"
        "   dt = clamp(dt, 0.0, 10.0);\n"
        "   float fogFactor = clamp(max(dt * 225.0 / 10.0, uFogBase) / 255.0, 0.0, 1.0);\n"
        "   fogFactor = mix(fogFactor, 1.0, clamp(uForceFog, 0.0, 1.0));\n"
        "   vec2 uv = vec2((skyU + uSkyTime) / 256.0, (skyV - uSkyTime) / 256.0);\n"
        "   vec3 skyColor = texture(uSkyTexture, uv).rgb;\n"
        "   FragColor = vec4(mix(skyColor, uFogColor, fogFactor), 1.0);\n"
        "}";

    GLuint vertexShader = CompileShader(GL_VERTEX_SHADER, vsSource);
    GLuint fragmentShader = CompileShader(GL_FRAGMENT_SHADER, fsSource);
    m_skyShader = LinkProgram(vertexShader, fragmentShader);
    if (!m_skyShader) {
        PrintLog("GLRenderer: Sky shader compilation... FAILED!\n");
        return;
    }
    PrintLog("GLRenderer: Sky shader compilation... OK\n");

    glGenVertexArrays(1, &m_skyVAO);
    glGenTextures(1, &m_skyTexture);

    glBindTexture(GL_TEXTURE_2D, m_skyTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 256, 256, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    m_skyTextureDirty = true;
}

void GLRenderer::ShutdownSkyPipeline()
{
    if (m_skyTexture) {
        glDeleteTextures(1, &m_skyTexture);
        m_skyTexture = 0;
    }
    if (m_skyVAO) {
        glDeleteVertexArrays(1, &m_skyVAO);
        m_skyVAO = 0;
    }
    if (m_skyShader) {
        glDeleteProgram(m_skyShader);
        m_skyShader = 0;
    }
}

void GLRenderer::UploadSkyTexture()
{
    if (!m_skyTextureDirty || !m_skyTexture) {
        return;
    }
    m_skyTextureDirty = false;

    std::vector<uint32_t> expanded(256 * 256);
    for (int i = 0; i < 256 * 256; ++i) {
        expanded[i] = Expand1555to8888(SkyPic[i]);
    }

    glBindTexture(GL_TEXTURE_2D, m_skyTexture);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 256, 256, GL_RGBA, GL_UNSIGNED_BYTE, expanded.data());
}

float GLRenderer::GetTraceK(int x, int y)
{
    if (x < 8 || y < 8 || x > WinW - 8 || y > WinH - 8) return 0.0f;

    float k = 0.0f;
    // Sample 9 points around the sun position on the depth buffer
    const int offsets[][2] = {
        {0, 0}, {10, 0}, {-10, 0}, {0, 10}, {0, -10},
        {8, 8}, {8, -8}, {-8, 8}, {-8, -8}
    };
    for (const auto& off : offsets) {
        float depth = 1.0f;
        glReadPixels(x + off[0], WinH - (y + off[1]), 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &depth);
        // Depth near 1.0 means sky (nothing occluding)
        if (depth > 0.9999f) k += 1.0f;
    }
    k /= 9.0f;

    DeltaFunc(m_traceK, k, TimeDt / 1024.0f);
    return m_traceK;
}

float GLRenderer::GetSkyK(int x, int y)
{
    if (x < 10 || y < 10 || x > WinW - 10 || y > WinH - 10) return 0.5f;

    float skySumR = 0.0f, skySumG = 0.0f, skySumB = 0.0f;

    // Sample 9 points around the sun position on the color buffer
    const int offsets[][2] = {
        {0, 0}, {6, 0}, {-6, 0}, {0, 6}, {0, -6},
        {4, 4}, {4, -4}, {-4, 4}, {-4, -4}
    };
    for (const auto& off : offsets) {
        unsigned char pixel[4];
        glReadPixels(x + off[0], WinH - (y + off[1]), 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
        // GL returns BGR in byte order for glReadPixels
        skySumR += pixel[0];
        skySumG += pixel[1];
        skySumB += pixel[2];
    }

    // Subtract the expected sky color (target)
    skySumR -= SkyTR * 9.0f;
    skySumG -= SkyTG * 9.0f;
    skySumB -= SkyTB * 9.0f;

    float k = std::sqrt(skySumR * skySumR + skySumG * skySumG + skySumB * skySumB) / 9.0f;
    if (k > 80.0f) k = 80.0f;
    if (k < 0.0f) k = 0.0f;
    k = 1.0f - k / 80.0f;
    if (k < 0.2f) k = 0.2f;
    if (OptDayNight == 2) k = 0.12f + k / 5.0f;

    DeltaFunc(m_skyTraceK, k, (0.07f + std::fabs(k - m_skyTraceK)) * (TimeDt / 512.0f));
    return m_skyTraceK;
}

void GLRenderer::RenderSun(float x, float y, float z)
{
    m_sunScrX = VideoCX + static_cast<int>(x / (-z) * CameraW);
    m_sunScrY = VideoCY - static_cast<int>(y / (-z) * CameraH);
    GetSkyK(m_sunScrX, m_sunScrY);

    float d = std::sqrt(x * x + y * y);
    if (d < 2048.0f) {
        m_sunLight = 220.0f - d * 220.0f / 2048.0f;
        if (m_sunLight > 140.0f) m_sunLight = 140.0f;
        m_sunLight *= m_skyTraceK;
    }

    if (d > 812.0f) d = 812.0f;
    d = (2048.0f + d) / 3048.0f;
    d += (1.0f - m_skyTraceK) / 2.0f;
    if (OptDayNight == 2) d = 1.5f;

    RenderModelSun(SunModel.get(), x * d, y * d, z * d, static_cast<int>(200.0f * m_skyTraceK));
}

void GLRenderer::RenderModelSun(TModel* mptr, float x0, float y0, float z0, int alpha)
{
    // Phase 2.2: ensure the static mesh is uploaded (cache hit after first call).
    UploadStaticMesh(mptr);

    if (!mptr || !mptr->lpTexture || !mptr->gVertex || !mptr->gFace) return;

    const GLuint texture = UploadModelTexture(mptr);
    if (!texture) return;

    m_sunModelVertices.clear();
    const size_t reserveCount = static_cast<size_t>(mptr->FCount) * 3;
    m_sunModelVertices.reserve(reserveCount);
    const float alphaVal = static_cast<float>(alpha) / 255.0f;

    for (int f = 0; f < mptr->FCount; ++f) {
        const TFace& face = mptr->gFace[f];
        const int texHeight = (mptr->TextureHeight > 1) ? mptr->TextureHeight : 1;

        auto makeVertex = [&](int vIdx, int tx, int ty) -> ModelVertex {
            const Vector2df uv = DecodeLegacyFaceUV(static_cast<float>(tx), static_cast<float>(ty), texHeight);
            return {
                mptr->gVertex[vIdx].x + x0,
                mptr->gVertex[vIdx].y + y0,
                mptr->gVertex[vIdx].z + z0,
                uv.x, uv.y,
                Light255ToByte(255.0f),  // full brightness
                Float01ToByte(0.0f),     // no fog
                Float01ToByte(alphaVal),  // per-frame alpha
                CutoutToByte(false),      // no cutout
                0, 0, 0,                  // fog color (unused)
                {0, 0, 0, 0, 0}
            };
        };

        m_sunModelVertices.push_back(makeVertex(face.v1, face.tax, face.tay));
        m_sunModelVertices.push_back(makeVertex(face.v2, face.tbx, face.tby));
        m_sunModelVertices.push_back(makeVertex(face.v3, face.tcx, face.tcy));
    }

    if (m_sunModelVertices.empty()) return;

    const auto projection = BuildLegacyProjection();
    UpdatePerFrameUBO();
    glUseProgram(m_modelShader);
#ifdef GL_PERF_HOOKS
    GL_PERF_STATE_CHANGE();
#endif
    // uProjection in PerFrame UBO (Phase 1.1)

    // The weapon's phong/env-map draw passes set uTintByFogColor=1.0 on this
    // shader. With vFogColor=(0,0,0) on the sun, that multiplies litColor to
    // black and the sun goes invisible under additive blending. Reset to 0
    // here so the sun renders normally.
    glUniform1f(m_locModelTint, 0.0f);

    glEnable(GL_BLEND);
#ifdef GL_PERF_HOOKS
    GL_PERF_STATE_CHANGE();
#endif
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);  // additive blending for sun
#ifdef GL_PERF_HOOKS
    GL_PERF_STATE_CHANGE();
#endif
    glEnable(GL_DEPTH_TEST);
#ifdef GL_PERF_HOOKS
    GL_PERF_STATE_CHANGE();
#endif
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_FALSE);  // don't write depth for sun
#ifdef GL_PERF_HOOKS
    GL_PERF_STATE_CHANGE();
#endif

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
#ifdef GL_PERF_HOOKS
    GL_PERF_TEXTURE_BIND(texture);
#endif
    glBindVertexArray(m_modelVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_modelVBO);
    const GLsizeiptr vertexSize = static_cast<GLsizeiptr>(m_sunModelVertices.size() * sizeof(ModelVertex));
    glBufferData(GL_ARRAY_BUFFER, vertexSize, nullptr, GL_STREAM_DRAW);
    glBufferSubData(GL_ARRAY_BUFFER, 0, vertexSize, m_sunModelVertices.data());
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(m_sunModelVertices.size()));
#ifdef GL_PERF_HOOKS
    GL_PERF_DRAW(static_cast<uint32_t>(m_sunModelVertices.size()) / 3);
#endif

    glDepthMask(GL_TRUE);
#ifdef GL_PERF_HOOKS
    GL_PERF_STATE_CHANGE();
#endif
    glDisable(GL_BLEND);
#ifdef GL_PERF_HOOKS
    GL_PERF_STATE_CHANGE();
#endif
    glBindVertexArray(0);
}

void GLRenderer::UpdateSunVisibility()
{
    if (m_sunScrX < 10 || m_sunScrY < 10 || m_sunScrX > WinW - 10 || m_sunScrY > WinH - 10) {
        m_skyTraceK = 0.5f;
        return;
    }

    // Rate-limit to ~15 Hz (66ms) to avoid GPU stalls from glReadPixels
    if (m_sunScrX == m_lastSunVisibilityScrX &&
        m_sunScrY == m_lastSunVisibilityScrY &&
        RealTime - m_lastSunVisibilityUpdate < 66) {
        return;
    }

    m_lastSunVisibilityUpdate = RealTime;
    m_lastSunVisibilityScrX = m_sunScrX;
    m_lastSunVisibilityScrY = m_sunScrY;

    // Depth-based occlusion (GetTraceK): is terrain/models blocking the sun?
    float traceK = GetTraceK(m_sunScrX, m_sunScrY);
    m_lastSunTraceK = traceK;
    m_lastSunTraceScrX = m_sunScrX;
    m_lastSunTraceScrY = m_sunScrY;
    m_lastSunTraceFrame = RealTime;

    // Color-based cloud occlusion (GetSkyK): are clouds dimming the sky?
    float skyK = GetSkyK(m_sunScrX, m_sunScrY);

    // Final visibility is the product
    float visibility = traceK * skyK;

    // Smooth transition
    float delta = (0.07f + std::fabs(visibility - m_skyTraceK)) * (static_cast<float>(TimeDt) / 512.0f);
    if (visibility > m_skyTraceK) {
        m_skyTraceK = (std::min)(visibility, m_skyTraceK + delta);
    } else {
        m_skyTraceK = (std::max)(visibility, m_skyTraceK - delta);
    }
}

void GLRenderer::ApplySunDepthOcclusion()
{
    // Called from ShowVideo() after the full scene is rendered.
    // Samples the depth buffer at the sun's screen position to check
    // if terrain/models are occluding the sun.
    // Depth-based occlusion sample is cached for the current frame so
    // ApplySunDepthOcclusion() can reuse the value computed by UpdateSunVisibility().
    float traceK = 0.0f;
    if (m_sunScrX == m_lastSunTraceScrX &&
        m_sunScrY == m_lastSunTraceScrY &&
        RealTime == m_lastSunTraceFrame) {
        traceK = m_lastSunTraceK;
    } else {
        traceK = GetTraceK(m_sunScrX, m_sunScrY);
        m_lastSunTraceK = traceK;
        m_lastSunTraceScrX = m_sunScrX;
        m_lastSunTraceScrY = m_sunScrY;
        m_lastSunTraceFrame = RealTime;
    }
    m_sunLight *= traceK;
}

void GLRenderer::RenderFSRect(uint32_t color, bool additive)
{
    float a = static_cast<float>((color >> 24) & 0xFF) / 255.0f;
    float r = static_cast<float>((color >> 16) & 0xFF) / 255.0f;
    float g = static_cast<float>((color >> 8) & 0xFF) / 255.0f;
    float b = static_cast<float>(color & 0xFF) / 255.0f;

    // Reuse the persistent 1x1 white texture created for flat-color rendering.
    if (!m_whiteTexture) return;

    // Build a fullscreen quad using the packed ModelVertex layout
    // (Phase 1.4: 32 bytes, color attributes are uint8 normalized).
    // The pre-Phase-1.4 local FSVertex struct used float fields for
    // light/fog/fogR/G/B/alpha/cutout, which the GL driver reads as
    // raw bytes -- producing garbage colors. Must use the same packed
    // layout as ModelVertex for the VBO's attribute pointers to interpret
    // the data correctly.
    const uint8_t lightByte  = 255;
    const uint8_t fogByte    = 255;                 // 1.0 normalized
    const uint8_t alphaByte  = static_cast<uint8_t>(a * 255.0f + 0.5f);
    const uint8_t cutoutByte = 0;
    const uint8_t fogRByte   = static_cast<uint8_t>(r * 255.0f + 0.5f);
    const uint8_t fogGByte   = static_cast<uint8_t>(g * 255.0f + 0.5f);
    const uint8_t fogBByte   = static_cast<uint8_t>(b * 255.0f + 0.5f);
    const ModelVertex quad[6] = {
        {-1.0f, -1.0f, 0.0001f, 0, 0, lightByte, fogByte, alphaByte, cutoutByte, fogRByte, fogGByte, fogBByte, {0,0,0,0,0}},
        { 1.0f, -1.0f, 0.0001f, 0, 0, lightByte, fogByte, alphaByte, cutoutByte, fogRByte, fogGByte, fogBByte, {0,0,0,0,0}},
        { 1.0f,  1.0f, 0.0001f, 0, 0, lightByte, fogByte, alphaByte, cutoutByte, fogRByte, fogGByte, fogBByte, {0,0,0,0,0}},
        {-1.0f, -1.0f, 0.0001f, 0, 0, lightByte, fogByte, alphaByte, cutoutByte, fogRByte, fogGByte, fogBByte, {0,0,0,0,0}},
        { 1.0f,  1.0f, 0.0001f, 0, 0, lightByte, fogByte, alphaByte, cutoutByte, fogRByte, fogGByte, fogBByte, {0,0,0,0,0}},
        {-1.0f,  1.0f, 0.0001f, 0, 0, lightByte, fogByte, alphaByte, cutoutByte, fogRByte, fogGByte, fogBByte, {0,0,0,0,0}},
    };

    // The fullscreen quad is already in NDC, so the UBO must carry the
    // identity projection. With the world projection here, the NDC
    // vertices get re-projected off-screen and the glare is invisible.
    const std::array<float, 16> identity = {
        1.0f,0.0f,0.0f,0.0f, 0.0f,1.0f,0.0f,0.0f, 0.0f,0.0f,1.0f,0.0f, 0.0f,0.0f,0.0f,1.0f
    };

    glDisable(GL_DEPTH_TEST);
#ifdef GL_PERF_HOOKS
    GL_PERF_STATE_CHANGE();
#endif
    glDepthMask(GL_FALSE);
#ifdef GL_PERF_HOOKS
    GL_PERF_STATE_CHANGE();
#endif
    glEnable(GL_BLEND);
#ifdef GL_PERF_HOOKS
    GL_PERF_STATE_CHANGE();
#endif
    // Use additive blending (glare) or standard alpha blending (dark overlay)
    if (additive)
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    else
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
#ifdef GL_PERF_HOOKS
    GL_PERF_STATE_CHANGE();
#endif

    UpdatePerFrameUBO(identity);
    glUseProgram(m_modelShader);
#ifdef GL_PERF_HOOKS
    GL_PERF_STATE_CHANGE();
#endif
    // uProjection in PerFrame UBO (Phase 1.1)

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_whiteTexture);
#ifdef GL_PERF_HOOKS
    GL_PERF_TEXTURE_BIND(m_whiteTexture);
#endif
    glBindVertexArray(m_modelVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_modelVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), nullptr, GL_STREAM_DRAW);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(quad), quad);
    glDrawArrays(GL_TRIANGLES, 0, 6);
#ifdef GL_PERF_HOOKS
    GL_PERF_DRAW(2);
#endif
    glBindVertexArray(0);

    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
}

void GLRenderer::RenderSkyPlane()
{
#ifdef GL_PERF_HOOKS
    GL_PERF_SCOPE("RenderSkyPlane");
#endif
    if (!m_skyVAO || !m_skyTexture || !m_skyShader) {
        return;
    }

    UploadSkyTexture();

    const float localCa = std::cos(CameraAlpha);
    const float localSa = std::sin(CameraAlpha);
    const float pitchCos = std::cos(CameraBeta);
    const float pitchSin = std::sin(CameraBeta);

    SKYDTime = RealTime & ((1 << 16) - 1);

    const float skyPitchCos = std::cos(CameraBeta - 0.15f);
    const float skyPitchSin = std::sin(CameraBeta - 0.15f);

    Vector3d tx = {0.004f, 0.0f, 0.0f};
    Vector3d ty = {0.0f, 0.0f, 0.004f};
    Vector3d nv = {0.0f, -1.0f, 0.0f};

    auto rotateSky = [&](Vector3d& v) {
        // First rotate around Y axis (CameraAlpha)
        float x = v.x * localCa - v.z * localSa;
        float z = v.z * localCa + v.x * localSa;
        // Then rotate around X axis (CameraBeta - 0.15)
        float y = v.y * skyPitchCos + z * skyPitchSin;
        float zz = z * skyPitchCos - v.y * skyPitchSin;

        v.x = x;
        v.y = y;
        v.z = zz;
    };

    rotateSky(tx);
    rotateSky(ty);
    rotateSky(nv);

    Vector3d vbase = {-CameraX, 4.0f * 512.0f * 16.0f, CameraZ};
    rotateSky(vbase);

    const float p = nv.x * vbase.x + nv.y * vbase.y + nv.z * vbase.z;
    const float ddx = vbase.x * tx.x + vbase.y * tx.y + vbase.z * tx.z;
    const float ddy = vbase.x * ty.x + vbase.y * ty.y + vbase.z * ty.z;

    const float qx = CameraH * nv.x;
    const float qy = CameraW * nv.y;
    const float qz = CameraW * CameraH * nv.z;

    float px = p * CameraH * tx.x;
    float py = p * CameraW * tx.y;
    float pz = p * CameraW * CameraH * tx.z;
    float rx = p * CameraH * ty.x;
    float ry = p * CameraW * ty.y;
    float rz = p * CameraW * CameraH * ty.z;

    px -= ddx * qx;
    py -= ddx * qy;
    pz -= ddx * qz;
    rx -= ddy * qx;
    ry -= ddy * qy;
    rz -= ddy * qz;

    // The sky's distance-fog color is global, not the color of the
    // fixed fog volume the camera is currently inside. Local volumes are
    // still applied to terrain/models by their per-vertex fog color.
    const Vector3d targetSkyFogColor = GetDistanceFogColor();

    // Temporal low-pass filter on the sky color so day/night sky changes
    // settle smoothly instead of popping between frames.
    if (!m_smoothedSkyFogColorInit) {
        m_smoothedSkyFogColor = targetSkyFogColor;
        m_smoothedSkyFogColorInit = true;
    } else {
        constexpr float k = 0.15f;
        m_smoothedSkyFogColor.x += (targetSkyFogColor.x - m_smoothedSkyFogColor.x) * k;
        m_smoothedSkyFogColor.y += (targetSkyFogColor.y - m_smoothedSkyFogColor.y) * k;
        m_smoothedSkyFogColor.z += (targetSkyFogColor.z - m_smoothedSkyFogColor.z) * k;
    }

    UpdatePerFrameUBO();
    glUseProgram(m_skyShader);
    glUniform1i(m_locSkyTexture, 0);
    glUniform2f(m_locSkyViewport, static_cast<float>(WinW), static_cast<float>(WinH));
    glUniform2f(m_locSkyVideoCenter, static_cast<float>(VideoCX), static_cast<float>(VideoCY));
    // uFogColor and uForceFog now sourced from PerFrame UBO (Phase 1.1).
    glUniform3f(m_locSkyQ, qx, qy, qz);
    glUniform3f(m_locSkyP, px, py, pz);
    glUniform3f(m_locSkyR, rx, ry, rz);
    glUniform1f(m_locSkyTime, static_cast<float>(SKYDTime) / 256.0f);

    // Sample CalcFogLevel directly above the camera (X=0, Z=0 in
    // camera-relative space) at sky height to get the base fog amount
    // for the per-pixel sky gradient. Using (0, ...) instead of
    // (512, ...) keeps the probe in the same map cell as the camera,
    // so the resulting fog amount matches the volume the camera is in
    // (when CAMERAINFOG) and doesn't jump as the camera crosses cell
    // boundaries along the X axis.
    const Vector3d fogProbe = {0.0f, 4.0f * 512.0f * 16.0f, 0.0f};
    const float fogBase = CalcFogLevel(fogProbe);
    glUniform1f(m_locSkyFogBase, fogBase);

    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_skyTexture);
#ifdef GL_PERF_HOOKS
    GL_PERF_TEXTURE_BIND(m_skyTexture);
#endif

    glBindVertexArray(m_skyVAO);
    glDrawArrays(GL_TRIANGLES, 0, 3);
#ifdef GL_PERF_HOOKS
    GL_PERF_DRAW(1);
#endif
    glBindVertexArray(0);

    glDepthMask(GL_TRUE);
#ifdef GL_PERF_HOOKS
    GL_PERF_STATE_CHANGE();
#endif
    glEnable(GL_DEPTH_TEST);
#ifdef GL_PERF_HOOKS
    GL_PERF_STATE_CHANGE();
#endif

    // Render sun on top of sky (matching D3D/3DFX: sky plane renders sun)
    if (SunModel && !IsUnderwater()) {
        m_sunLight = 0.0f;
        Vector3d sunDir = {-2048.0f, 4048.0f, -2048.0f};
        sunDir = RotateVector(sunDir);
        if (sunDir.z < -2024.0f) {
            RenderSun(sunDir.x, sunDir.y, sunDir.z);
            // GetSkyK is called inside RenderSun for cloud occlusion.
            // GetTraceK (depth-based) is deferred to ShowVideo() after the
            // full scene is rendered, so the depth buffer has terrain/models.
        }
    }
}

// ============================================================================
// HUD Pipeline
// ============================================================================

void GLRenderer::InitializeHudPipeline()
{
    // Phase 2.20: skip the GDI round-trip.  lpVideoBuf is 16-bit 555
    // (X1R5G5B5), top-down.  We upload it directly as a GL_RGB5 texture
    // and let the fragment shader convert to RGBA8 with transparency.
    // This eliminates the UpdateUIPixels CPU loop (480K pixel conversions)
    // and the m_uiPixels RGBA8 buffer (1.92 MB).
    const char* vsSource =
        "#version 330 core\n"
        "layout (location = 0) in vec2 aPos;\n"
        "layout (location = 1) in vec2 aTexCoord;\n"
        "out vec2 vTexCoord;\n"
        "void main() {\n"
        "   gl_Position = vec4(aPos, 0.0, 1.0);\n"
        "   // GDI lpVideoBuf is top-down; GL textures are bottom-up.\n"
        "   // Flip by inverting the v-coordinate.\n"
        "   vTexCoord = vec2(aTexCoord.x, 1.0 - aTexCoord.y);\n"
        "}\n";

    const char* fsSource =
        "#version 330 core\n"
        "in vec2 vTexCoord;\n"
        "out vec4 FragColor;\n"
        "uniform sampler2D uTexture;\n"
        "void main() {\n"
        "   vec3 rgb555 = texture(uTexture, vTexCoord).rgb;\n"
        "   // Pixel value 0 = transparent (HUD background).\n"
        "   if (dot(rgb555, vec3(1.0)) < 0.01) discard;\n"
        "   FragColor = vec4(rgb555, 1.0);\n"
        "}\n";

    GLuint vertexShader = CompileShader(GL_VERTEX_SHADER, vsSource);
    GLuint fragmentShader = CompileShader(GL_FRAGMENT_SHADER, fsSource);
    m_uiShader = LinkProgram(vertexShader, fragmentShader);
    if (!m_uiShader) {
        PrintLog("GLRenderer: UI shader compilation... FAILED!\n");
        return;
    }
    PrintLog("GLRenderer: UI shader compilation... OK\n");

    // Static fullscreen quad in NDC: (x, y, u, v) per vertex
    // Texture is flipped vertically in UpdateUIPixels, so:
    //   tex (0,0) = bottom-left of image, tex (0,1) = top-left of image
    //   Screen top-left (-1,1) should show tex (0,1) = top of image
    constexpr float kQuadVertices[] = {
        -1.0f,  1.0f, 0.0f, 1.0f,
        -1.0f, -1.0f, 0.0f, 0.0f,
         1.0f, -1.0f, 1.0f, 0.0f,
        -1.0f,  1.0f, 0.0f, 1.0f,
         1.0f, -1.0f, 1.0f, 0.0f,
         1.0f,  1.0f, 1.0f, 1.0f,
    };

    glGenVertexArrays(1, &m_uiVAO);
    glGenBuffers(1, &m_uiVBO);

    glBindVertexArray(m_uiVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_uiVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(kQuadVertices), kQuadVertices, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glUseProgram(m_uiShader);
    glUniform1i(glGetUniformLocation(m_uiShader, "uTexture"), 0);
}

void GLRenderer::ShutdownHudPipeline()
{
    if (m_uiTexture && m_hrc) { glDeleteTextures(1, &m_uiTexture); m_uiTexture = 0; }
    if (m_uiVBO && m_hrc) { glDeleteBuffers(1, &m_uiVBO); m_uiVBO = 0; }
    if (m_uiVAO && m_hrc) { glDeleteVertexArrays(1, &m_uiVAO); m_uiVAO = 0; }
    if (m_uiShader && m_hrc) { glDeleteProgram(m_uiShader); m_uiShader = 0; }
    m_uiTextureWidth = 0;
    m_uiTextureHeight = 0;
}

void GLRenderer::EnsureUITexture()
{
    // Phase 2.20: allocate as GL_RGB5 (16-bit) to match lpVideoBuf's
    // X1R5G5B5 format.  No CPU-side RGBA8 buffer needed — we upload
    // lpVideoBuf directly via glTexSubImage2D.
    if (WinW <= 0 || WinH <= 0) return;
    if (m_uiTexture && m_uiTextureWidth == WinW && m_uiTextureHeight == WinH) return;

    if (!m_uiTexture) glGenTextures(1, &m_uiTexture);
    glBindTexture(GL_TEXTURE_2D, m_uiTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB5, WinW, WinH, 0,
                 GL_BGRA, GL_UNSIGNED_SHORT_1_5_5_5_REV, nullptr);
    m_uiTextureWidth = WinW;
    m_uiTextureHeight = WinH;
    m_hudNeedsFullUpload = true;  // texture recreated — must full-upload next frame
    m_dirtyRectCount = 0;
    m_prevDirtyRectCount = 0;
}

void GLRenderer::UpdateUIPixels()
{
    // Phase 2.20: this function is now a no-op.  lpVideoBuf is uploaded
    // directly to the GPU as a GL_RGB5 texture in DrawHUDOverlay — no
    // CPU-side 555→RGBA8 conversion needed.  Saved ~1ms CPU per frame.
    (void)0;
}

void GLRenderer::ClearStaleHUDRegions()
{
    // Clear only the regions from the previous frame (not the whole buffer).
    // This zeros out HUD elements that may have moved or disappeared,
    // while leaving the rest of lpVideoBuf untouched (it will retain its
    // previous content which is still valid on the GPU texture).
    if (!lpVideoBuf || VideoPitch <= 0) return;

    if (m_hudNeedsFullClear) {
        // A full-DIB write (CopyHARDToDIB) happened — the entire buffer
        // has non-zero scene data.  Clear it all and force full upload.
        memset(lpVideoBuf, 0, static_cast<size_t>(VideoPitch) * WinH * sizeof(WORD));
        m_hudNeedsFullClear = false;
        m_hudNeedsFullUpload = true;
        return;
    }

    for (int i = 0; i < m_prevDirtyRectCount; i++) {
        const DirtyRect& r = m_prevDirtyRects[i];
        // Clamp to be safe (rects from previous frame should already be clamped)
        int cx = r.x, cy = r.y, cw = r.w, ch = r.h;
        if (cx < 0) { cw += cx; cx = 0; }
        if (cy < 0) { ch += cy; cy = 0; }
        if (cx + cw > WinW) cw = WinW - cx;
        if (cy + ch > WinH) ch = WinH - cy;
        if (cw <= 0 || ch <= 0) continue;

        WORD* row = static_cast<WORD*>(lpVideoBuf) + static_cast<size_t>(cy) * VideoPitch + cx;
        for (int yy = 0; yy < ch; yy++) {
            memset(row, 0, static_cast<size_t>(cw) * sizeof(WORD));
            row += VideoPitch;
        }
    }
}

void GLRenderer::InvalidateHUDOverlay()
{
    // Called after CopyHARDToDIB writes the full 3D scene into lpVideoBuf.
    // The next frame must do a full-buffer clear + full upload to erase
    // the non-HUD scene pixels from the overlay texture.
    m_hudNeedsFullClear = true;
    m_dirtyRectCount = 0;
    m_prevDirtyRectCount = 0;
}

void GLRenderer::MarkDirtyRect(int x, int y, int w, int h)
{
    // Clamp to screen bounds
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > WinW) w = WinW - x;
    if (y + h > WinH) h = WinH - y;
    if (w <= 0 || h <= 0) return;

    // If we already need a full upload (overflow, texture recreated, etc.),
    // don't bother accumulating rects.
    if (m_hudNeedsFullUpload) return;

    // Check if this rect is already covered by an existing rect
    for (int i = 0; i < m_dirtyRectCount; i++) {
        const DirtyRect& r = m_dirtyRects[i];
        if (x >= r.x && y >= r.y && x + w <= r.x + r.w && y + h <= r.y + r.h)
            return;  // fully contained
    }

    // Overflow guard: fall back to full upload next frame
    if (m_dirtyRectCount >= kMaxDirtyRects) {
        m_hudNeedsFullUpload = true;
        m_dirtyRectCount = 0;
        return;
    }

    m_dirtyRects[m_dirtyRectCount++] = {x, y, w, h};
}

// ======================================================================
// Night desaturation + darkness overlay
// ======================================================================






void GLRenderer::RegisterPicture(TPicture* pptr)
{
    // No-op: we copy to lpVideoBuf in DrawPicture instead
    (void)pptr;
}

static WORD Conv565to555(WORD c)
{
    int r = (c >> 11) & 0x1F;
    int g = (c >> 5) & 0x3F;
    int b = c & 0x1F;
    return (r << 10) | ((g >> 1) << 5) | b;
}

// Copy picture pixels directly to lpVideoBuf (matching C1 and D3D/3DFX approach).
// lpVideoBuf is a 16-bit DIB section, pictures are already in 565 format after conv_pic.
void GLRenderer::DrawPicture(int x, int y, TPicture& pic)
{
    if (!pic.lpImage || pic.W <= 0 || pic.H <= 0 || !lpVideoBuf) return;

    WORD* dst = static_cast<WORD*>(lpVideoBuf);
    for (int yy = 0; yy < pic.H; yy++) {
        int dstY = yy + y;
        if (dstY < 0 || dstY >= WinH) continue;
        int copyW = pic.W;
        int srcX = 0;
        int dstX = x;
        if (dstX < 0) { srcX = -dstX; copyW += dstX; dstX = 0; }
        if (dstX + copyW > WinW) copyW = WinW - dstX;
        if (copyW <= 0) continue;
        const WORD* src = pic.lpImage.get() + yy * pic.W + srcX;
        WORD* d = dst + dstY * VideoPitch + dstX;
        for (int i = 0; i < copyW; i++) {
            d[i] = Conv565to555(src[i]);
        }
    }

    MarkDirtyRect(x, y, pic.W, pic.H);
}

void GLRenderer::DrawScaledPicture(int x, int y, int w, int h, TPicture& pic)
{
    if (!pic.lpImage || pic.W <= 0 || pic.H <= 0 || !lpVideoBuf) return;

    WORD* dst = static_cast<WORD*>(lpVideoBuf);
    for (int yy = 0; yy < h; yy++) {
        int dstY = yy + y;
        if (dstY < 0 || dstY >= WinH) continue;
        int sy = yy * pic.H / h;
        for (int xx = 0; xx < w; xx++) {
            int dstX = xx + x;
            if (dstX < 0 || dstX >= WinW) continue;
            int sx = xx * pic.W / w;
            WORD c = pic.lpImage[sy * pic.W + sx];
            if (c != 0) dst[dstY * VideoPitch + dstX] = Conv565to555(c);
        }
    }

    MarkDirtyRect(x, y, w, h);
}

// Phase 2.20: upload lpVideoBuf directly as GL_RGB5 texture.
// lpVideoBuf is 16-bit X1R5G5B5, top-down, stride = VideoPitch.
// The fragment shader converts 555→RGBA8 and handles transparency.
void GLRenderer::DrawHUDOverlay()
{
#ifdef GL_PERF_HOOKS
    GL_PERF_SCOPE("DrawHUDOverlay");
#endif

    if (!m_uiShader || !lpVideoBuf || WinW <= 0 || WinH <= 0) return;

    EnsureUITexture();

    glBindTexture(GL_TEXTURE_2D, m_uiTexture);
#ifdef GL_PERF_HOOKS
    GL_PERF_TEXTURE_BIND(m_uiTexture);
#endif

    // Upload dirty regions (or full buffer if needed).
    // The texture is top-down (EnsureUITexture allocates with nullptr data),
    // but lpVideoBuf is top-down too (CreateVideoDIB with negative height).
    // So no flip is needed at upload — the vertex shader handles the v-flip.
    //
    // We upload BOTH the previous frame's dirty rects (now cleared to zero
    // by ClearStaleHUDRegions) AND the current frame's dirty rects (just
    // drawn by HUD elements).  This ensures stale pixels that disappeared
    // get zeroed on the GPU, while new pixels appear.
    if (m_hudNeedsFullUpload) {
        glPixelStorei(GL_UNPACK_ROW_LENGTH, VideoPitch);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, WinW, WinH,
                        GL_BGRA, GL_UNSIGNED_SHORT_1_5_5_5_REV, lpVideoBuf);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        m_hudNeedsFullUpload = false;
    } else {
        int totalRects = m_prevDirtyRectCount + m_dirtyRectCount;
        if (totalRects > kMaxDirtyRects) {
            // Overflow — fall back to full upload this frame
            glPixelStorei(GL_UNPACK_ROW_LENGTH, VideoPitch);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, WinW, WinH,
                            GL_BGRA, GL_UNSIGNED_SHORT_1_5_5_5_REV, lpVideoBuf);
            glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        } else {
            // Upload previous frame's rects that are NOT fully covered by
            // a current rect (those covered rects will be uploaded anyway
            // by the current-rect pass below with fresh content).
            for (int i = 0; i < m_prevDirtyRectCount; i++) {
                const DirtyRect& r = m_prevDirtyRects[i];
                bool covered = false;
                for (int j = 0; j < m_dirtyRectCount; j++) {
                    const DirtyRect& c = m_dirtyRects[j];
                    if (r.x >= c.x && r.y >= c.y &&
                        r.x + r.w <= c.x + c.w && r.y + r.h <= c.y + c.h) {
                        covered = true;
                        break;
                    }
                }
                if (covered) continue;  // will be uploaded with current content below

                const WORD* src = static_cast<const WORD*>(lpVideoBuf)
                                  + static_cast<size_t>(r.y) * VideoPitch + r.x;
                glPixelStorei(GL_UNPACK_ROW_LENGTH, VideoPitch);
                glTexSubImage2D(GL_TEXTURE_2D, 0, r.x, r.y, r.w, r.h,
                                GL_BGRA, GL_UNSIGNED_SHORT_1_5_5_5_REV, src);
                glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
            }
            // Then upload current frame's rects (newly drawn content)
            for (int i = 0; i < m_dirtyRectCount; i++) {
                const DirtyRect& r = m_dirtyRects[i];
                const WORD* src = static_cast<const WORD*>(lpVideoBuf)
                                  + static_cast<size_t>(r.y) * VideoPitch + r.x;
                glPixelStorei(GL_UNPACK_ROW_LENGTH, VideoPitch);
                glTexSubImage2D(GL_TEXTURE_2D, 0, r.x, r.y, r.w, r.h,
                                GL_BGRA, GL_UNSIGNED_SHORT_1_5_5_5_REV, src);
                glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
            }
        }
    }

    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glUseProgram(m_uiShader);
    glActiveTexture(GL_TEXTURE0);
    glBindVertexArray(m_uiVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
#ifdef GL_PERF_HOOKS
    GL_PERF_DRAW(2);
#endif
    glBindVertexArray(0);

    glDisable(GL_BLEND);
#ifdef GL_PERF_HOOKS
    GL_PERF_STATE_CHANGE();
#endif
    glDepthMask(GL_TRUE);
#ifdef GL_PERF_HOOKS
    GL_PERF_STATE_CHANGE();
#endif
    glEnable(GL_DEPTH_TEST);
#ifdef GL_PERF_HOOKS
    GL_PERF_STATE_CHANGE();
#endif

    // Swap: current becomes previous for next frame's clear+upload
    for (int i = 0; i < m_dirtyRectCount && i < kMaxDirtyRects; i++)
        m_prevDirtyRects[i] = m_dirtyRects[i];
    m_prevDirtyRectCount = m_dirtyRectCount;
    m_dirtyRectCount = 0;
}

void GLRenderer::DrawTrophyText(int x, int y)
{
    // Trophy text is rendered via GDI onto the lpVideoBuf.
    // D3D/3DFX call ddTextOut/FXTextOut which write to the backbuffer.
    // For GL, we draw text onto lpVideoBuf using GDI, then DrawHUDOverlay uploads it.
    // We need to use the hdcCMain + hbmpVideoBuf to draw onto lpVideoBuf.

    if (!hdcCMain || !hbmpVideoBuf || !lpVideoBuf) return;

    HBITMAP hbmpOld = reinterpret_cast<HBITMAP>(SelectObject(hdcCMain, hbmpVideoBuf));
    SetBkMode(hdcCMain, TRANSPARENT);
    HFONT oldFont = nullptr;
    if (fnt_Small) oldFont = reinterpret_cast<HFONT>(SelectObject(hdcCMain, fnt_Small));

    int dtype = TrophyDisplayBody.ctype;
    int time  = TrophyDisplayBody.time;
    int date  = TrophyDisplayBody.date;
    int wep   = TrophyDisplayBody.weapon;
    int score = TrophyDisplayBody.score;
    float scale = TrophyDisplayBody.scale;
    float range = TrophyDisplayBody.range;
    char t[64];

    // D3D/3DFX draw at (x0+14, y0+18)
    int tx = x + 14;
    int ty = y + 18;
    int lineStep = 16;

    auto textOut = [&](int px, int py, const char* str, int color) {
        SetTextColor(hdcCMain, 0x00101010);
        TextOut(hdcCMain, px + 1, py + 1, str, static_cast<int>(strlen(str)));
        SetTextColor(hdcCMain, color);
        TextOut(hdcCMain, px, py, str, static_cast<int>(strlen(str)));
    };

    SIZE sz;
    auto drawLine = [&](const char* label, const char* value, int color) {
        textOut(tx, ty, label, color);
        GetTextExtentPoint32(hdcCMain, label, static_cast<int>(strlen(label)), &sz);
        int lw = sz.cx;
        textOut(tx + lw, ty, value, 0x0000BFBF);
        ty += lineStep;
    };

    drawLine("Name: ", DinoInfo[dtype].Name, 0x00BFBFBF);

    if (OptSys) sprintf(t, "%3.2ft ", DinoInfo[dtype].Mass * scale * scale / 0.907f);
    else        sprintf(t, "%3.2fT ", DinoInfo[dtype].Mass * scale * scale);
    drawLine("Weight: ", t, 0x00BFBFBF);

    if (OptSys) sprintf(t, "%3.2fft", DinoInfo[dtype].Length * scale / 0.3f);
    else        sprintf(t, "%3.2fm", DinoInfo[dtype].Length * scale);
    drawLine("Length: ", t, 0x00BFBFBF);

    sprintf_s(t, sizeof(t), "%s    ", WeapInfo[wep].Name);
    drawLine("Weapon: ", t, 0x00BFBFBF);

    sprintf_s(t, sizeof(t), "%d", score);
    drawLine("Score: ", t, 0x00BFBFBF);

    if (OptSys) sprintf(t, "%3.1fft", range / 0.3f);
    else        sprintf(t, "%3.1fm", range);
    drawLine("Range of kill: ", t, 0x00BFBFBF);

    if (OptSys) sprintf_s(t, sizeof(t), "%d.%d.%d   ", ((date>>10) & 255), (date & 255), date>>20);
    else        sprintf_s(t, sizeof(t), "%d.%d.%d   ", (date & 255), ((date>>10) & 255), date>>20);
    drawLine("Date: ", t, 0x00BFBFBF);

    sprintf_s(t, sizeof(t), "%d:%02d", ((time>>10) & 255), (time & 255));
    drawLine("Time: ", t, 0x00BFBFBF);

    // Mark dirty: 7 lines × 16px step starting at (x+14, y+18),
    // plus shadow offset (+1,+1) and font height (~14px).
    MarkDirtyRect(x + 13, y + 17, 160, 128);

    if (oldFont) SelectObject(hdcCMain, oldFont);
    SelectObject(hdcCMain, hbmpOld);
}

void GLRenderer::RenderHealthBar()
{
    // Interface compliance only. The GL path draws the health bar into
    // lpVideoBuf via the free function RenderHealthBar() in GLStubs.cpp
    // (called from ShowControlElements), and DrawHUDOverlay uploads it
    // along with the rest of the HUD. This matches how the other 2D
    // elements (DrawPicture, DrawTrophyText, etc.) are handled.
}

void GLRenderer::Render_Cross(int x, int y)
{
    (void)x; (void)y;
    // Crosshair is drawn by DrawOpticCross in Hunt.cpp via model rendering.
    // No additional GL code needed here.
}

void GLRenderer::Render_LifeInfo(int index)
{
    // Draw dino info when looking through binoculars
    if (!hdcCMain || !hbmpVideoBuf || !lpVideoBuf) return;
    if (index < 0 || index >= ChCount) return;

    HBITMAP hbmpOld = reinterpret_cast<HBITMAP>(SelectObject(hdcCMain, hbmpVideoBuf));
    SetBkMode(hdcCMain, TRANSPARENT);
    HFONT oldFont = nullptr;
    if (fnt_Small) oldFont = reinterpret_cast<HFONT>(SelectObject(hdcCMain, fnt_Small));

    int ctype = Characters[index].CType;
    float scale = Characters[index].scale;
    char t[32];

    int x = VideoCX + WinW / 64;
    int y = VideoCY + static_cast<int>((WinH / 6.8));

    auto textOut = [&](int px, int py, const char* str, int color) {
        SetTextColor(hdcCMain, 0x00000000);
        TextOut(hdcCMain, px + 1, py + 1, str, static_cast<int>(strlen(str)));
        SetTextColor(hdcCMain, color);
        TextOut(hdcCMain, px, py, str, static_cast<int>(strlen(str)));
    };

    textOut(x, y, DinoInfo[ctype].Name, 0x0000b000);

    if (OptSys) sprintf(t, "Weight: %3.2ft ", DinoInfo[ctype].Mass * scale * scale / 0.907f);
    else        sprintf(t, "Weight: %3.2fT ", DinoInfo[ctype].Mass * scale * scale);
    textOut(x, y + 16, t, 0x0000b000);

    int R = static_cast<int>((VectorLength(SubVectors(Characters[index].pos, PlayerPos)) * 3 / 64.0f));
    if (OptSys) sprintf(t, "Distance: %dft ", R);
    else        sprintf(t, "Distance: %dm  ", R / 3);
    textOut(x, y + 32, t, 0x0000b000);

    // Mark dirty: 3 lines × 16px step + shadow + font height
    MarkDirtyRect(x - 1, y - 1, 140, 50);

    if (oldFont) SelectObject(hdcCMain, oldFont);
    SelectObject(hdcCMain, hbmpOld);
}

void GLRenderer::SetVideoMode(int w, int h)
{
    glViewport(0, 0, w, h);
}

void GLRenderer::SetFullScreen()
{
}

bool GLRenderer::IsSoftwareStyle() const
{
    return false;
}

#endif // _gl
