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

#define WGL_CONTEXT_MAJOR_VERSION_ARB     0x2091
#define WGL_CONTEXT_MINOR_VERSION_ARB     0x2092
#define WGL_CONTEXT_PROFILE_MASK_ARB      0x2133
#define WGL_CONTEXT_CORE_PROFILE_BIT_ARB  0x00000001

typedef HGLRC (WINAPI *PFNWGLCREATECONTEXTATTRIBSARBPROC)(HDC, HGLRC, const int*);

static HMODULE libGL = nullptr;

static void* glad_get_proc(const char* name)
{
    void* p = (void*)wglGetProcAddress(name);
    if (p == 0 || p == (void*)0x1 || p == (void*)0x2 || p == (void*)0x3 || p == (void*)-1) {
        p = (void*)GetProcAddress(libGL, name);
    }
    return p;
}

static GLuint CompileShader(GLenum type, const char* source)
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

static GLuint LinkProgram(GLuint vertexShader, GLuint fragmentShader)
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

constexpr float kModelNearClip = -16.0f;

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

Vector3d DecodeFogColor(int rgb)
{
    return {
        static_cast<float>(rgb & 0xFF) / 255.0f,
        static_cast<float>((rgb >> 8) & 0xFF) / 255.0f,
        static_cast<float>((rgb >> 16) & 0xFF) / 255.0f
    };
}

Vector3d GetFogColor()
{
    if (UNDERWATER && FogsList[127].fogRGB) {
        int rgb = FogsList[127].fogRGB;
        return {
            static_cast<float>((rgb >> 16) & 0xFF) / 255.0f,
            static_cast<float>((rgb >> 8) & 0xFF) / 255.0f,
            static_cast<float>(rgb & 0xFF) / 255.0f
        };
    }

    if (CAMERAINFOG && CameraFogI > 0) {
        return DecodeFogColor(FogsList[CameraFogI].fogRGB);
    }

    if (FOGON && CurFogColor) {
        return DecodeFogColor(CurFogColor);
    }

    return {
        static_cast<float>(SkyR) / 255.0f,
        static_cast<float>(SkyG) / 255.0f,
        static_cast<float>(SkyB) / 255.0f
    };
}

Vector2df DecodeLegacyFaceUV(float tx, float ty, int texHeight)
{
    // fp_conv() already converted int pixel coords to float for non-soft builds.
    // For GL (non-d3d), fp_conv does: f = (float)i — raw pixel coords.
    // Normalize to [0,1] by dividing by texture dimensions.
    const float h = static_cast<float>((texHeight > 1) ? texHeight : 1);
    return {
        tx / 256.0f,
        ty / h
    };
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
        {
            a.position.x + (b.position.x - a.position.x) * t,
            a.position.y + (b.position.y - a.position.y) * t,
            a.position.z + (b.position.z - a.position.z) * t
        },
        {
            a.uv.x + (b.uv.x - a.uv.x) * t,
            a.uv.y + (b.uv.y - a.uv.y) * t
        },
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
    for (size_t i = 0; i < input.size(); ++i) {
        const ModelClipVertex& current = input[i];
        const ModelClipVertex& previous = input[(i + input.size() - 1) % input.size()];
        const float currentDistance = DistanceToWaterPlane(current.position);
        const float previousDistance = DistanceToWaterPlane(previous.position);
        const bool currentInside = currentDistance >= 0.0f;
        const bool previousInside = previousDistance >= 0.0f;

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
        return {0.0f, GetFogColor()};
    }

    // Camera-state gate (C2 transformation of the C1 model-fog
    // technique). C1 samples CalcFogLevel for every model vertex
    // and lets a vertex inside a volumetric pocket saturate to 1.0
    // with the pocket's color, which makes a tree in a green
    // pocket look solid green when the player walks by in clear
    // air. C2 has the same per-vertex CalcFogLevel path, but the
    // C2 fog pockets are world-authored: they only "exist" for
    // the player when the camera is in one. So we gate the per-
    // vertex volumetric fog on CAMERAINFOG. Underwater is the
    // exception -- the underwater volume (FogsList[127]) is the
    // camera's own volume, so we always apply the underwater
    // fog there even if the camera cell happens to sit above the
    // pocket's YBegin (the closest seabed rocks are below the
    // YBegin in many maps and would otherwise get no fog at all).
    if (!CAMERAINFOG && !UNDERWATER) {
        return {0.0f, GetFogColor()};
    }

    const float amount = std::clamp(CalcFogLevel(point) / 255.0f, 0.0f, 1.0f);
    return {amount, GetFogColor()};
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
        PrintLog("GL: ERROR - hwndMain is NULL!\n");
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
        "layout (location = 3) in float aLight;\n"
        "layout (location = 4) in float aFog;\n"
        "layout (location = 5) in vec3 aFogColor;\n"
        "layout (location = 6) in float aAlpha;\n"
        "uniform mat4 uProjection;\n"
        "out vec2 vTexCoord;\n"
        "flat out int vLayer;\n"
        "out float vLight;\n"
        "out float vFog;\n"
        "out vec3 vFogColor;\n"
        "out float vAlpha;\n"
        "out float vViewZ;\n"
        "void main() {\n"
        "   gl_Position = uProjection * vec4(aPos, 1.0);\n"
        "   vTexCoord = aTexCoord;\n"
        "   vLayer = int(aLayer + 0.5);\n"
        "   vLight = clamp(aLight / 255.0, 0.0, 1.0);\n"
        "   vFog = clamp(aFog / 255.0, 0.0, 1.0);\n"
        "   vFogColor = aFogColor;\n"
        "   vAlpha = clamp(aAlpha, 0.0, 1.0);\n"
        "   vViewZ = max(-aPos.z, 0.0);\n"
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
        "uniform sampler2DArray uTerrainArray;\n"
        "uniform float uFogDistance;\n"
        "uniform float uFogFadeStart;\n"
        "void main() {\n"
        "   vec4 texColor = texture(uTerrainArray, vec3(vTexCoord, float(vLayer)));\n"
        "   if (texColor.a < 0.05) discard;\n"
        "   vec3 litColor = texColor.rgb * vLight;\n"
        "   // Per-vertex volumetric fog (volume-specific color and amount).\n"
        "   vec3 volumetricFogColor = mix(litColor, vFogColor, vFog);\n"
        "   // Per-pixel distance fog: smooth ramp from uFogFadeStart to\n"
        "   // uFogDistance. Gated by vFog so the per-pixel ramp only fills\n"
        "   // the FLimit cap on vertices already inside a fog volume;\n"
        "   // clear-air vertices (vFog=0) keep their full texture color,\n"
        "   // matching the legacy D3D/3DFX behavior.\n"
        "   float distanceFog = clamp((vViewZ - uFogFadeStart) / max(uFogDistance - uFogFadeStart, 1.0), 0.0, 1.0) * vFog;\n"
        "   vec3 finalColor = mix(volumetricFogColor, vFogColor, distanceFog);\n"
        "   FragColor = vec4(finalColor, texColor.a * vAlpha);\n"
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
        "layout (location = 2) in float aLight;\n"
        "layout (location = 3) in float aFog;\n"
        "layout (location = 4) in vec3 aFogColor;\n"
        "layout (location = 5) in float aAlpha;\n"
        "layout (location = 6) in float aCutout;\n"
        "uniform mat4 uProjection;\n"
        "out vec2 vTexCoord;\n"
        "out float vLight;\n"
        "out float vFog;\n"
        "out vec3 vFogColor;\n"
        "out float vAlpha;\n"
        "out float vCutout;\n"
        "void main() {\n"
        "   gl_Position = uProjection * vec4(aPos, 1.0);\n"
        "   vTexCoord = aTexCoord;\n"
        "   vLight = clamp(aLight / 255.0, 0.0, 1.0);\n"
        "   vFog = clamp(aFog, 0.0, 1.0);\n"
        "   vFogColor = aFogColor;\n"
        "   vAlpha = clamp(aAlpha, 0.0, 1.0);\n"
        "   vCutout = aCutout;\n"
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
        "uniform sampler2D uModelTexture;\n"
        "void main() {\n"
        "   vec4 texColor = texture(uModelTexture, vTexCoord);\n"
        "   if (vCutout > 0.5 && dot(texColor.rgb, vec3(1.0)) < 0.01) discard;\n"
        "   vec3 litColor = texColor.rgb * vLight;\n"
        "   // Per-vertex volumetric fog only. The terrain shader has a\n"
        "   // per-pixel distance ramp for the terrain-to-sky transition,\n"
        "   // but we intentionally do NOT use one for models: the per-pixel\n"
        "   // ramp uses camera-space depth (vViewZ), which changes as the\n"
        "   // camera rotates, causing angle-dependent over-fogging of\n"
        "   // discrete 3D objects like trees. The per-vertex volumetric\n"
        "   // fog uses world-relative position and is rotation-invariant,\n"
        "   // matching the legacy D3D/3DFX model fog behavior.\n"
        "   vec3 finalColor = mix(litColor, vFogColor, vFog);\n"
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

    if (!InitializeTerrainPipeline()) {
        return false;
    }

    if (!InitializeModelPipeline()) {
        return false;
    }

    InitializeSkyPipeline();
    InitializeHudPipeline();

    glUseProgram(m_modelShader);
    glUniform1i(glGetUniformLocation(m_modelShader, "uModelTexture"), 0);

    glUseProgram(m_terrainShader);
    glUniform1i(glGetUniformLocation(m_terrainShader, "uTerrainArray"), 0);

    Vector3d fogColor = GetCurrentFogColor();
    glClearColor(fogColor.x, fogColor.y, fogColor.z, 1.0f);

    m_uploadedTerrainTextures.fill(nullptr);
    m_Initialized = true;
    PrintLog("GL: Initialize() completed successfully.\n");
    return true;
}

void GLRenderer::Shutdown()
{
    if (!m_Initialized && !m_hrc) return;

    ShutdownTerrainPipeline();
    ShutdownModelPipeline();
    ShutdownSkyPipeline();
    ShutdownHudPipeline();

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

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(TerrainVertex), reinterpret_cast<void*>(offsetof(TerrainVertex, x)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(TerrainVertex), reinterpret_cast<void*>(offsetof(TerrainVertex, u)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, sizeof(TerrainVertex), reinterpret_cast<void*>(offsetof(TerrainVertex, layer)));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(TerrainVertex), reinterpret_cast<void*>(offsetof(TerrainVertex, light)));
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, sizeof(TerrainVertex), reinterpret_cast<void*>(offsetof(TerrainVertex, fog)));
    glEnableVertexAttribArray(5);
    glVertexAttribPointer(5, 3, GL_FLOAT, GL_FALSE, sizeof(TerrainVertex), reinterpret_cast<void*>(offsetof(TerrainVertex, fogR)));
    glEnableVertexAttribArray(6);
    glVertexAttribPointer(6, 1, GL_FLOAT, GL_FALSE, sizeof(TerrainVertex), reinterpret_cast<void*>(offsetof(TerrainVertex, alpha)));

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

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(ModelVertex), reinterpret_cast<void*>(offsetof(ModelVertex, x)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(ModelVertex), reinterpret_cast<void*>(offsetof(ModelVertex, u)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, sizeof(ModelVertex), reinterpret_cast<void*>(offsetof(ModelVertex, light)));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(ModelVertex), reinterpret_cast<void*>(offsetof(ModelVertex, fog)));
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, sizeof(ModelVertex), reinterpret_cast<void*>(offsetof(ModelVertex, fogR)));
    glEnableVertexAttribArray(5);
    glVertexAttribPointer(5, 1, GL_FLOAT, GL_FALSE, sizeof(ModelVertex), reinterpret_cast<void*>(offsetof(ModelVertex, alpha)));
    glEnableVertexAttribArray(6);
    glVertexAttribPointer(6, 1, GL_FLOAT, GL_FALSE, sizeof(ModelVertex), reinterpret_cast<void*>(offsetof(ModelVertex, cutout)));

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

    m_modelTextureFilterState.clear();

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
    if (m_whiteTexture) {
        glDeleteTextures(1, &m_whiteTexture);
        m_whiteTexture = 0;
    }

    m_worldModelItems.clear();
    m_transparentModelItems.clear();
    m_objectList.clear();
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

void GLRenderer::BeginTerrainFrame()
{
    m_terrainVertices.clear();
    m_waterVertices.clear();
}

void GLRenderer::BeginWaterFrame()
{
    m_waterVertices.clear();
}

void GLRenderer::RenderWaterSurface()
{
    if (m_waterVertices.empty()) {
        return;
    }

    EnsureTerrainTextureArray();

    std::array<bool, kMaxTerrainTextureLayers> usedLayers{};
    for (const TerrainVertex& vertex : m_waterVertices) {
        const int layer = static_cast<int>(vertex.layer + 0.5f);
        if (layer >= 0 && layer < kMaxTerrainTextureLayers && Textures[layer]) {
            usedLayers[layer] = true;
        }
    }

    for (int layer = 0; layer < kMaxTerrainTextureLayers; ++layer) {
        if (!usedLayers[layer] || !Textures[layer]) {
            continue;
        }
        if (m_uploadedTerrainTextures[layer] != Textures[layer]) {
            UploadTerrainLayer(layer, *Textures[layer]);
            m_uploadedTerrainTextures[layer] = Textures[layer];
        }
    }

    const auto projection = BuildLegacyProjection();
    glUseProgram(m_terrainShader);
    glUniformMatrix4fv(glGetUniformLocation(m_terrainShader, "uProjection"), 1, GL_FALSE, projection.data());
    // Per-pixel distance fog: uFogDistance is the view distance (ctViewR*256
    // in world units); uFogFadeStart is where the per-pixel ramp begins,
    // 1024 units (4 cells) before that. Between fade-start and distance the
    // terrain blends from the per-vertex volumetric fog color toward the
    // volume's full fog color, smoothing the terrain-to-sky transition.
    glUniform1f(glGetUniformLocation(m_terrainShader, "uFogDistance"), static_cast<float>(ctViewR) * 256.0f);
    glUniform1f(glGetUniformLocation(m_terrainShader, "uFogFadeStart"), static_cast<float>(ctViewR) * 256.0f - 1024.0f);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D_ARRAY, m_terrainTextureArray);
    glBindVertexArray(m_terrainVAO);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    DrawVertexBatch(m_waterVertices);
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
    m_modelTextureFilterState[texture] = false;
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
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);

    std::vector<uint32_t> expanded(128 * 128);
    for (size_t i = 0; i < expanded.size(); ++i) {
        expanded[i] = Expand1555to8888(mptr->lpTexture[i]);
    }

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 128, 128, 0, GL_RGBA, GL_UNSIGNED_BYTE, expanded.data());
    m_bmpTextureCache[mptr] = texture;
    m_modelTextureFilterState[texture] = true;
    return texture;
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
    static thread_local std::vector<Vector3d> transformed;
    static thread_local std::vector<Vector3d> unrotated;
    transformed.reserve(mptr->VCount);
    unrotated.reserve(mptr->VCount);
    transformed.clear();
    unrotated.clear();

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
    outItem.distance = std::sqrt(x0 * x0 + y0 * y0 + z0 * z0);
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
        target.push_back({a.position.x, a.position.y, a.position.z, a.uv.x, a.uv.y, a.light, fogA.amount, fogA.color.x, fogA.color.y, fogA.color.z, alpha, cutoutValue});
        target.push_back({b.position.x, b.position.y, b.position.z, b.uv.x, b.uv.y, b.light, fogB.amount, fogB.color.x, fogB.color.y, fogB.color.z, alpha, cutoutValue});
        target.push_back({c.position.x, c.position.y, c.position.z, c.uv.x, c.uv.y, c.light, fogC.amount, fogC.color.x, fogC.color.y, fogC.color.z, alpha, cutoutValue});
    };    static thread_local std::vector<ModelClipVertex> polygon;
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
        const bool cutout = (face.Flags & (sfOpacity | sfTransparent)) != 0;
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
                                   bool additive)
{
    if (!m_modelShader || texture == 0 || vertices.empty()) {
        return;
    }

    glUseProgram(m_modelShader);
    glUniformMatrix4fv(glGetUniformLocation(m_modelShader, "uProjection"), 1, GL_FALSE, projection.data());

    if (depthTest) {
        glEnable(GL_DEPTH_TEST);
        if (!enableBlend) {
            glDepthMask(GL_TRUE);
        }
    } else {
        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);
    }

    if (enableBlend) {
        glEnable(GL_BLEND);
        if (additive) {
            glBlendFunc(GL_SRC_ALPHA, GL_ONE);  // additive — used by water circles
        } else {
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        }
        glDepthMask(GL_FALSE);
    } else {
        glDisable(GL_BLEND);
    }

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    glBindVertexArray(m_modelVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_modelVBO);
    const GLsizeiptr vertexSize = static_cast<GLsizeiptr>(vertices.size() * sizeof(ModelVertex));
    glBufferData(GL_ARRAY_BUFFER, vertexSize, nullptr, GL_STREAM_DRAW);
    glBufferSubData(GL_ARRAY_BUFFER, 0, vertexSize, vertices.data());
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertices.size()));
    glBindVertexArray(0);

    glDepthMask(GL_TRUE);
    if (!depthTest) {
        glEnable(GL_DEPTH_TEST);
    }
    if (enableBlend) {
        glDisable(GL_BLEND);
    }
}

bool GLRenderer::NeedsNearestModelFiltering(const std::vector<ModelVertex>& vertices) const
{
    return std::any_of(vertices.begin(), vertices.end(), [](const ModelVertex& vertex) {
        return vertex.cutout > 0.5f;
    });
}

void GLRenderer::SetModelTextureFiltering(GLuint texture, bool nearest)
{
    if (!texture) {
        return;
    }

    const auto it = m_modelTextureFilterState.find(texture);
    if (it != m_modelTextureFilterState.end() && it->second == nearest) {
        return;
    }

    glBindTexture(GL_TEXTURE_2D, texture);
    const GLint filter = nearest ? GL_NEAREST : GL_LINEAR;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
    m_modelTextureFilterState[texture] = nearest;
}

void GLRenderer::RenderWorldModels()
{
    if (m_worldModelItems.empty()) {
        return;
    }

    const auto projection = BuildLegacyProjection();
    for (const ModelDrawItem& item : m_worldModelItems) {
        DrawModelVertices(item.texture, item.opaqueVertices, projection, true, false, false);

        if (!item.cutoutVertices.empty()) {
            SetModelTextureFiltering(item.texture, true);
            DrawModelVertices(item.texture, item.cutoutVertices, projection, true, false, false);
            SetModelTextureFiltering(item.texture, false);
        }
    }

    m_transparentModelItems.clear();
    m_transparentModelItems.reserve(m_worldModelItems.size());
    for (const ModelDrawItem& item : m_worldModelItems) {
        if (!item.transparentVertices.empty()) {
            m_transparentModelItems.push_back(&item);
        }
    }

    std::sort(m_transparentModelItems.begin(), m_transparentModelItems.end(),
              [](const ModelDrawItem* a, const ModelDrawItem* b) {
                  return a->distance > b->distance;
              });

    for (const ModelDrawItem* item : m_transparentModelItems) {
        const bool useNearestFiltering = NeedsNearestModelFiltering(item->transparentVertices);
        if (useNearestFiltering) {
            SetModelTextureFiltering(item->texture, true);
        }
        DrawModelVertices(item->texture, item->transparentVertices, projection, true, true, item->additive);
        if (useNearestFiltering) {
            SetModelTextureFiltering(item->texture, false);
        }
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
    if (m_objectList.size() >= 2048) {
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
        CalcModelGroundLight(MObjects[ob].model, x * 256 + 128, y * 256 + 128, FI);
    } else {
        mlight = -(RandomMap[y & 31][x & 31] >> 5) + (LMap[y][x] >> 1) + 96;
    }

    mlight = std::clamp(mlight, 64, 192);

    Vector3d pos;
    pos.x = x * 256 + 128 - CameraX;
    pos.z = y * 256 + 128 - CameraZ;
    pos.y = static_cast<float>(HMapO[y][x]) * ctHScale - CameraY;

    const float zs = VectorLength(pos);
    if (pos.y + MObjects[ob].info.YHi < (HMap[y][x] + HMap[y + 1][x + 1]) / 2 * ctHScale - CameraY) {
        return;
    }

    waterclip = FALSE;
    if (!UNDERWATER && (FMap[y][x] & fmWaterA) && HMapO[y][x] < WaterList[WMap[y][x]].wlevel) {
        if (WaterList[WMap[y][x]].wlevel * ctHScale > HMapO[y][x] * ctHScale + MObjects[ob].info.YHi) {
            return;
        }

        waterclipbase = pos;
        waterclipbase.y = WaterList[WMap[y][x]].wlevel * ctHScale - CameraY;
        waterclipbase = RotateVector(waterclipbase);
        waterclip = TRUE;
    }

    pos = RotateVector(pos);
    GlassL = 0;
    if (zs > 256 * (ctViewR - 4))
        GlassL = min(255, (int)(zs / 4 - 64 * (ctViewR - 4)));
    if (GlassL == 255) {
        return;
    }

    if ((MObjects[ob].info.flags & ofANIMATED) && MObjects[ob].info.LastAniTime != RealTime) {
        MObjects[ob].info.LastAniTime = RealTime;
        CreateMorphedObject(MObjects[ob].model, MObjects[ob].vtl, RealTime % MObjects[ob].vtl.AniTime);
    }

    float renderDistance = zs;
    if (MObjects[ob].info.flags & ofNOBMP) {
        renderDistance = 0.0f;
    }

    if (renderDistance > ctViewRM * 256) {
        RenderBMPModel(&MObjects[ob].bmpmodel, pos.x, pos.y, pos.z, mlight - 16);
    } else if (waterclip) {
        RenderModelClipWater(MObjects[ob].model, pos.x, pos.y, pos.z, mlight, FI, fi, CameraBeta);
    } else if (pos.z < -256 * 8) {
        RenderModel(MObjects[ob].model, pos.x, pos.y, pos.z, mlight, FI, fi, CameraBeta);
    } else {
        RenderModelClip(MObjects[ob].model, pos.x, pos.y, pos.z, mlight, FI, fi, CameraBeta);
    }
}

void GLRenderer::RenderModelsList()
{
    for (const Vector2di& object : m_objectList) {
        RenderMappedObject(object.x, object.y);
    }
    m_objectList.clear();
    RenderWorldModels();
}

void GLRenderer::Render3DHardwarePosts()
{
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

        waterclip = FALSE;

        if (cptr->rpos.z > -256.0f * 10.0f)
            RenderModelClip(cptr->pinfo->mptr,
                            cptr->rpos.x, cptr->rpos.y, cptr->rpos.z, 210, 0,
                            -cptr->alpha + pi / 2.0f + CameraAlpha,
                            CameraBeta);
        else
            RenderModel(cptr->pinfo->mptr,
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

            waterclip = FALSE;

            if (cptr->rpos.z > -256.0f * 10.0f)
                RenderModelClip(cptr->pinfo->mptr,
                                cptr->rpos.x, cptr->rpos.y, cptr->rpos.z, 210, 0,
                                -cptr->alpha + pi / 2.0f + CameraAlpha,
                                CameraBeta);
            else
                RenderModel(cptr->pinfo->mptr,
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
                    GlassL = 0;
                    float zs = VectorLength(Ship.rpos);
                    if (zs > 256.0f * (ctViewR - 4))
                        GlassL = (std::min)(255, static_cast<int>((zs - 256.0f * (ctViewR - 4)) / 4.0f));

                    CreateMorphedModel(ShipModel.mptr, &ShipModel.Animation[0], Ship.FTime, 1.0);

                    if (fabs(Ship.rpos.z) < 4000.0f)
                        RenderModelClip(ShipModel.mptr,
                                        Ship.rpos.x, Ship.rpos.y, Ship.rpos.z, 210, 0,
                                        -Ship.alpha - pi / 2.0f + CameraAlpha, CameraBeta);
                    else
                        RenderModel(ShipModel.mptr,
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
                    GlassL = 0;
                    float zs = VectorLength(SShip.rpos);
                    if (zs > 256.0f * (ctViewR - 4))
                        GlassL = (std::min)(255, static_cast<int>((zs - 256.0f * (ctViewR - 4)) / 4.0f));

                    CreateMorphedModelBetaGamma(SShipModel.mptr, &SShipModel.Animation[0],
                                                SShip.FTime, 1.0, SShip.beta, SShip.gamma);

                    if (fabs(SShip.rpos.z) < 4000.0f)
                        RenderModelClip(SShipModel.mptr,
                                        SShip.rpos.x, SShip.rpos.y, SShip.rpos.z, 210, 0,
                                        -SShip.alpha - pi / 2.0f + CameraAlpha, CameraBeta);
                    else
                        RenderModel(SShipModel.mptr,
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
                    GlassL = 0;
                    float zs = VectorLength(AmmoBag.rpos);
                    if (zs > 256.0f * (ctViewR - 4))
                        GlassL = (std::min)(255, static_cast<int>((zs - 256.0f * (ctViewR - 4)) / 4.0f));

                    CreateMorphedModel(BagModel.mptr, &BagModel.Animation[0], AmmoBag.FTime, 1.0);

                    if (fabs(AmmoBag.rpos.z) < 4000.0f)
                        RenderModelClip(BagModel.mptr,
                                        AmmoBag.rpos.x, AmmoBag.rpos.y, AmmoBag.rpos.z, 210, 0,
                                        -pi / 2.0f + CameraAlpha, CameraBeta);
                    else
                        RenderModel(BagModel.mptr,
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
                GlassL = 0;
                float zs = VectorLength(bullet[b].rpos);
                if (zs > 256.0f * (ctViewR - 4))
                    GlassL = (std::min)(255, static_cast<int>((zs - 256.0f * (ctViewR - 4)) / 4.0f));

                CreateMorphedModelBetaGamma(Weapon.Bullet[bullet[b].parent].mptr,
                                            &Weapon.Bullet[bullet[b].parent].Animation[0],
                                            bullet[b].FTime, 1.0, bullet[b].beta, 0.0f);

                if (fabs(bullet[b].rpos.z) < 4000.0f)
                    RenderModelClip(Weapon.Bullet[bullet[b].parent].mptr,
                                    bullet[b].rpos.x, bullet[b].rpos.y, bullet[b].rpos.z, 210, 0,
                                    -bullet[b].alpha - pi / 2.0f + CameraAlpha,
                                    -bullet[b].beta - pi / 2.0f + CameraBeta);
                else
                    RenderModel(Weapon.Bullet[bullet[b].parent].mptr,
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
    auto unpackABGR = [](uint32_t c, float& r, float& g, float& b, float& a) {
        a = static_cast<float>((c >> 24) & 0xFF) / 255.0f;
        b = static_cast<float>((c >> 16) & 0xFF) / 255.0f;
        g = static_cast<float>((c >> 8) & 0xFF) / 255.0f;
        r = static_cast<float>(c & 0xFF) / 255.0f;
    };

    float cr, cg, cb, ca;
    float er, eg, eb, ea;
    unpackABGR(RGBA, cr, cg, cb, ca);
    unpackABGR(RGBA2, er, eg, eb, ea);

    // Clamp radius to sub-pixel precision (matching D3D)
    float r  = floorf(R * 16.0f) / 16.0f;
    float r2 = floorf(0.65f * R * 16.0f) / 16.0f;

    // Convert screen-space position to NDC (-1 to 1)
    float ndcX = (cx - VideoCX) / VideoCX;
    float ndcY = (VideoCY - cy) / VideoCY;

    // Use the model shader with fog=1.0 to output flat color
    struct CircleVertex {
        float x, y, z, u, v, light, fog, fogR, fogG, fogB, alpha, cutout;
    };

    // 8 triangles forming an octagon with alternating outer/inner radius
    // Matches D3D: angle 0=R, 45=R2, 90=R, 135=R2, ...
    std::vector<CircleVertex> vertices;
    vertices.reserve(24);

    for (int i = 0; i < 8; i++) {
        int next = (i + 1) % 8;

        // Radius alternates: even indices use R (outer), odd indices use R2 (inner)
        float rad_i = (i % 2 == 0) ? r : r2;
        float rad_next = (next % 2 == 0) ? r : r2;

        float angle_i = i * pi / 4.0f;
        float angle_next = next * pi / 4.0f;

        // Center vertex (color RGBA)
        vertices.push_back({ndcX, ndcY, 0.0001f, 0, 0, 255, 1.0f, cr, cg, cb, ca, 0});

        // Edge vertex i (color RGBA2)
        float ex1 = ndcX + cosf(angle_i) * rad_i / VideoCX;
        float ey1 = ndcY + sinf(angle_i) * rad_i / VideoCY;
        vertices.push_back({ex1, ey1, 0.0001f, 0, 0, 255, 1.0f, er, eg, eb, ea, 0});

        // Edge vertex i+1 (color RGBA2)
        float ex2 = ndcX + cosf(angle_next) * rad_next / VideoCX;
        float ey2 = ndcY + sinf(angle_next) * rad_next / VideoCY;
        vertices.push_back({ex2, ey2, 0.0001f, 0, 0, 255, 1.0f, er, eg, eb, ea, 0});
    }

    // Identity projection for NDC-space rendering
    const float identity[16] = {
        1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1
    };

    glUseProgram(m_modelShader);
    glUniformMatrix4fv(glGetUniformLocation(m_modelShader, "uProjection"), 1, GL_FALSE, identity);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_whiteTexture);
    glBindVertexArray(m_modelVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_modelVBO);
    const GLsizeiptr vertexSize = static_cast<GLsizeiptr>(vertices.size() * sizeof(CircleVertex));
    glBufferData(GL_ARRAY_BUFFER, vertexSize, nullptr, GL_STREAM_DRAW);
    glBufferSubData(GL_ARRAY_BUFFER, 0, vertexSize, vertices.data());
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertices.size()));
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

            float sx = VideoCX - (int)(CameraW * rpos.x / rpos.z * 16) / 16.0f;
            float sy = VideoCY + (int)(CameraH * rpos.y / rpos.z * 16) / 16.0f;
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

        float sx = VideoCX - (int)(CameraW * rpos.x / rpos.z * 16) / 16.0f;
        float sy = VideoCY + (int)(CameraH * rpos.y / rpos.z * 16) / 16.0f;

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

            float sx = VideoCX - (int)(CameraW * rpos.x / rpos.z * 16) / 16.0f;
            float sy = VideoCY + (int)(CameraH * rpos.y / rpos.z * 16) / 16.0f;

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

    std::vector<ModelVertex> vertices;
    vertices.reserve(6);

    const float baseLight = std::clamp(static_cast<float>(light), 0.0f, 255.0f);
    const float alpha = std::clamp((255.0f - static_cast<float>(GlassL)) / 255.0f, 0.0f, 1.0f);
    const FogSample fog = SampleFogAtPoint({x0, y0, z0}, false);

    auto appendVertex = [&](int index, float u, float v) {
        Vector3d pos;
        pos.x = mptr->gVertex[index].x + x0;
        pos.y = mptr->gVertex[index].y + y0;
        pos.z = z0;
        if (pos.z >= -256.0f) {
            return;
        }
        vertices.push_back({pos.x, pos.y, pos.z, u, v, baseLight, fog.amount, fog.color.x, fog.color.y, fog.color.z, alpha, 0.0f});
    };

    appendVertex(0, 0.0f, 0.0f);
    appendVertex(1, 1.0f, 0.0f);
    appendVertex(2, 1.0f, 1.0f);
    appendVertex(0, 0.0f, 0.0f);
    appendVertex(2, 1.0f, 1.0f);
    appendVertex(3, 0.0f, 1.0f);

    if (vertices.size() < 6 || (vertices.size() % 6) != 0) {
        return;
    }

    const auto projection = BuildLegacyProjection();
    DrawModelVertices(texture, vertices, projection, true, true, false);  // standard alpha blend
}

void GLRenderer::RenderModel(TModel* mptr, float x0, float y0, float z0,
                             int light, int vt, float al, float bt)
{
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

    glClear(GL_DEPTH_BUFFER_BIT);
    const auto projection = BuildLegacyProjection();
    DrawModelVertices(item.texture, item.opaqueVertices, projection, true, false, false);
    if (!item.cutoutVertices.empty()) {
        SetModelTextureFiltering(item.texture, true);
        DrawModelVertices(item.texture, item.cutoutVertices, projection, true, false, false);
        SetModelTextureFiltering(item.texture, false);
    }
    if (!item.transparentVertices.empty()) {
        const bool useNearestFiltering = NeedsNearestModelFiltering(item.transparentVertices);
        if (useNearestFiltering) {
            SetModelTextureFiltering(item.texture, true);
        }
        DrawModelVertices(item.texture, item.transparentVertices, projection, true, true, false);
        if (useNearestFiltering) {
            SetModelTextureFiltering(item.texture, false);
        }
    }
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
    if (UNDERWATER && FogsList[127].fogRGB) {
        int rgb = FogsList[127].fogRGB;
        return {
            static_cast<float>((rgb >> 16) & 0xFF) / 255.0f,
            static_cast<float>((rgb >> 8) & 0xFF) / 255.0f,
            static_cast<float>(rgb & 0xFF) / 255.0f
        };
    }

    if (CAMERAINFOG && CameraFogI > 0) {
        return DecodeFogColor(FogsList[CameraFogI].fogRGB);
    }

    if (FOGON && CurFogColor) {
        return DecodeFogColor(CurFogColor);
    }

    return {
        static_cast<float>(SkyR) / 255.0f,
        static_cast<float>(SkyG) / 255.0f,
        static_cast<float>(SkyB) / 255.0f
    };
}

Vector3d GLRenderer::GetFogColorForMapPoint(int mapX, int mapY)
{
    if (UNDERWATER || !FOGON) {
        return GetCurrentFogColor();
    }

    const int fogX = (mapX & (ctMapSize - 1)) >> 1;
    const int fogY = (mapY & (ctMapSize - 1)) >> 1;
    int fogIndex = FogsMap[fogY][fogX];
    if (!fogIndex && CAMERAINFOG) {
        fogIndex = CameraFogI;
    }

    if (fogIndex > 0) {
        return DecodeFogColor(FogsList[fogIndex].fogRGB);
    }

    return GetCurrentFogColor();
}

float GLRenderer::Clamp01(float value)
{
    if (value < 0.0f) return 0.0f;
    if (value > 1.0f) return 1.0f;
    return value;
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

float GLRenderer::CalcWaterAlpha(const EPoint& vertex, float zs)
{
    float alpha = Clamp01(vertex.ALPHA / 255.0f);

    if (!UNDERWATER && zs > (ctViewR - 8) * 256.0f) {
        const float zz = VectorLength(vertex.v) - 256.0f * (ctViewR - 4);
        if (zz > 0.0f) {
            alpha = Clamp01((255.0f - zz / 3.0f) / 255.0f);
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

    vertices.push_back({v0.v.x, v0.v.y, v0.v.z, uv[0].x, uv[0].y, layer, static_cast<float>(v0.Light), v0.Fog, fogColor0.x, fogColor0.y, fogColor0.z, alpha0});
    vertices.push_back({v1.v.x, v1.v.y, v1.v.z, uv[1].x, uv[1].y, layer, static_cast<float>(v1.Light), v1.Fog, fogColor1.x, fogColor1.y, fogColor1.z, alpha1});
    vertices.push_back({v2.v.x, v2.v.y, v2.v.z, uv[2].x, uv[2].y, layer, static_cast<float>(v2.Light), v2.Fog, fogColor2.x, fogColor2.y, fogColor2.z, alpha2});
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
                                     float alpha2)
{
    const auto uv = GetTerrainUVs(reverse, second, direction);
    const float layer = static_cast<float>(textureLayer);

    vertices.push_back({v0.v.x, v0.v.y, v0.v.z, uv[0].x, uv[0].y, layer, static_cast<float>(v0.Light), v0.Fog, fogColor0.x, fogColor0.y, fogColor0.z, alpha0});
    vertices.push_back({v1.v.x, v1.v.y, v1.v.z, uv[1].x, uv[1].y, layer, static_cast<float>(v1.Light), v1.Fog, fogColor1.x, fogColor1.y, fogColor1.z, alpha1});
    vertices.push_back({v2.v.x, v2.v.y, v2.v.z, uv[2].x, uv[2].y, layer, static_cast<float>(v2.Light), v2.Fog, fogColor2.x, fogColor2.y, fogColor2.z, alpha2});
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

    const int localX = x - CCX + 128;
    const int localY = y - CCY + 128;
    if (localX < 0 || localY < 0 || localX + 1 >= 256 || localY + 1 >= 256) {
        return;
    }

    EPoint v00 = VMap[localY][localX];
    if (v00.v.z > backR) {
        return;
    }

    EPoint v10 = VMap[localY][localX + 1];
    EPoint v01 = VMap[localY + 1][localX];
    EPoint v11 = VMap[localY + 1][localX + 1];

    const float xx = (v00.v.x + v11.v.x) * 0.5f;
    const float yy = (v00.v.y + v11.v.y) * 0.5f;
    const float zz = (v00.v.z + v11.v.z) * 0.5f;

    if (std::fabs(xx * FOVK) > -zz + backR) {
        return;
    }

    const float zs = std::sqrt(xx * xx + yy * yy + zz * zz);
    if (zs > ctViewR * 256.0f) {
        return;
    }

    const bool reverse = (FMap[y][x] & fmReverse) != 0;
    const int direction = FMap[y][x] & 3;
    const Vector3d fog00 = GetFogColorForMapPoint(x, y);
    const Vector3d fog10 = GetFogColorForMapPoint(x + 1, y);
    const Vector3d fog01 = GetFogColorForMapPoint(x, y + 1);
    const Vector3d fog11 = GetFogColorForMapPoint(x + 1, y + 1);

    const int textureLayer = TMap1[y][x];
    if (textureLayer >= 0 && textureLayer < kMaxTerrainTextureLayers && Textures[textureLayer]) {
        if (reverse) {
            AppendTerrainTriangle(m_terrainVertices, v00, v10, v01, fog00, fog10, fog01, textureLayer, reverse, false, direction);
            AppendTerrainTriangle(m_terrainVertices, v01, v10, v11, fog01, fog10, fog11, textureLayer, reverse, true, direction);
        } else {
            AppendTerrainTriangle(m_terrainVertices, v00, v10, v11, fog00, fog10, fog11, textureLayer, reverse, false, direction);
            AppendTerrainTriangle(m_terrainVertices, v00, v11, v01, fog00, fog11, fog01, textureLayer, reverse, true, direction);
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

    const int localX = x - CCX + 128;
    const int localY = y - CCY + 128;
    if (localX < 0 || localY < 0 || localX + 2 >= 256 || localY + 2 >= 256) {
        return;
    }

    EPoint v00 = VMap[localY][localX];
    if (v00.v.z > BackViewR) {
        return;
    }

    const int textureLayer = TMap2[y][x];

    EPoint v20 = VMap[localY][localX + 2];
    EPoint v02 = VMap[localY + 2][localX];
    EPoint v22 = VMap[localY + 2][localX + 2];

    const float xx = (v00.v.x + v22.v.x) * 0.5f;
    const float yy = (v00.v.y + v22.v.y) * 0.5f;
    const float zz = (v00.v.z + v22.v.z) * 0.5f;

    if (std::fabs(xx * FOVK) > -zz + BackViewR) {
        return;
    }

    const float zs = std::sqrt(xx * xx + yy * yy + zz * zz);
    if (zs > ctViewR * 256.0f) {
        return;
    }

    const int direction = (FMap[y][x] >> 8) & 3;
    const Vector3d fog00 = GetFogColorForMapPoint(x, y);
    const Vector3d fog20 = GetFogColorForMapPoint(x + 2, y);
    const Vector3d fog02 = GetFogColorForMapPoint(x, y + 2);
    const Vector3d fog22 = GetFogColorForMapPoint(x + 2, y + 2);

    if (textureLayer >= 0 && textureLayer < kMaxTerrainTextureLayers && Textures[textureLayer]) {
        AppendTerrainTriangle(m_terrainVertices, v00, v20, v22, fog00, fog20, fog22, textureLayer, false, false, direction);
        AppendTerrainTriangle(m_terrainVertices, v00, v22, v02, fog00, fog22, fog02, textureLayer, false, true, direction);
    }

    RenderObject(x, y);
    RenderObject(x + 1, y);
    RenderObject(x, y + 1);
    RenderObject(x + 1, y + 1);
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

    const int localX = x - CCX + 128;
    const int localY = y - CCY + 128;
    if (localX < 0 || localY < 0 || localX + 1 >= 256 || localY + 1 >= 256) {
        return;
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

    const float zs = std::sqrt(xx * xx + yy * yy + zz * zz);
    if (zs > ctViewR * 256.0f) {
        return;
    }

    const float a00 = CalcWaterAlpha(v00, zs);
    const float a10 = CalcWaterAlpha(v10, zs);
    const float a01 = CalcWaterAlpha(v01, zs);
    const float a11 = CalcWaterAlpha(v11, zs);

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
            AppendWaterTriangle(m_waterVertices, v00, v10, v11, fog00, fog10, fog11, textureLayer, false, false, 0, a00, a10, a11);
        }
    }

    if (a00 > 0.0f || a11 > 0.0f || a01 > 0.0f) {
        if (IsWaterTriangleValid(v00, v11, v01, BackViewR)) {
            AppendWaterTriangle(m_waterVertices, v00, v11, v01, fog00, fog11, fog01, textureLayer, false, true, 0, a00, a11, a01);
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

    const int localX = x - CCX + 128;
    const int localY = y - CCY + 128;
    if (localX < 0 || localY < 0 || localX + 2 >= 256 || localY + 2 >= 256) {
        return;
    }

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

    const float zs = std::sqrt(xx * xx + yy * yy + zz * zz);
    if (zs > ctViewR * 256.0f) {
        return;
    }

    const float a00 = CalcWaterAlpha(v00, zs);
    const float a20 = CalcWaterAlpha(v20, zs);
    const float a02 = CalcWaterAlpha(v02, zs);
    const float a22 = CalcWaterAlpha(v22, zs);

    // Per-corner map-based fog color (far-detail water path; mirrors
    // the near-detail CollectWaterTile and the terrain path in
    // CollectTerrainTile2).
    const Vector3d fog00 = GetFogColorForMapPoint(x, y);
    const Vector3d fog20 = GetFogColorForMapPoint(x + 2, y);
    const Vector3d fog02 = GetFogColorForMapPoint(x, y + 2);
    const Vector3d fog22 = GetFogColorForMapPoint(x + 2, y + 2);

    if (a00 > 0.0f || a20 > 0.0f || a22 > 0.0f) {
        if (IsWaterTriangleValid(v00, v20, v22, BackViewR)) {
            AppendWaterTriangle(m_waterVertices, v00, v20, v22, fog00, fog20, fog22, textureLayer, false, false, 0, a00, a20, a22);
        }
    }

    if (a00 > 0.0f || a22 > 0.0f || a02 > 0.0f) {
        if (IsWaterTriangleValid(v00, v22, v02, BackViewR)) {
            AppendWaterTriangle(m_waterVertices, v00, v22, v02, fog00, fog22, fog02, textureLayer, false, true, 0, a00, a22, a02);
        }
    }
}

void GLRenderer::RenderGround()
{
    BeginTerrainFrame();
    m_worldModelItems.clear();
    m_transparentModelItems.clear();
    m_objectList.clear();

    for (int rr = ctViewR; rr >= ctViewR1; rr -= 2) {
        for (int x = rr; x > 0; x -= 2) {
            CollectTerrainTile2(CCX - x, CCY + rr, rr);
            CollectTerrainTile2(CCX + x, CCY + rr, rr);
            CollectTerrainTile2(CCX - x, CCY - rr, rr);
            CollectTerrainTile2(CCX + x, CCY - rr, rr);
        }

        CollectTerrainTile2(CCX, CCY - rr, rr);
        CollectTerrainTile2(CCX, CCY + rr, rr);

        for (int y = rr - 2; y > 0; y -= 2) {
            CollectTerrainTile2(CCX + rr, CCY - y, rr);
            CollectTerrainTile2(CCX + rr, CCY + y, rr);
            CollectTerrainTile2(CCX - rr, CCY + y, rr);
            CollectTerrainTile2(CCX - rr, CCY - y, rr);
        }

        CollectTerrainTile2(CCX - rr, CCY, rr);
        CollectTerrainTile2(CCX + rr, CCY, rr);
    }

    int rr = ctViewR1 - 1;
    for (int x = rr; x > -rr; --x) {
        CollectTerrainTile(CCX + rr, CCY + x, rr);
        CollectTerrainTile(CCX + x, CCY + rr, rr);
    }

    for (rr = ctViewR1 - 2; rr > 0; --rr) {
        for (int x = rr; x > 0; --x) {
            CollectTerrainTile(CCX - x, CCY + rr, rr);
            CollectTerrainTile(CCX + x, CCY + rr, rr);
            CollectTerrainTile(CCX - x, CCY - rr, rr);
            CollectTerrainTile(CCX + x, CCY - rr, rr);
        }

        CollectTerrainTile(CCX, CCY - rr, rr);
        CollectTerrainTile(CCX, CCY + rr, rr);

        for (int y = rr - 1; y > 0; --y) {
            CollectTerrainTile(CCX + rr, CCY - y, rr);
            CollectTerrainTile(CCX + rr, CCY + y, rr);
            CollectTerrainTile(CCX - rr, CCY + y, rr);
            CollectTerrainTile(CCX - rr, CCY - y, rr);
        }

        CollectTerrainTile(CCX - rr, CCY, rr);
        CollectTerrainTile(CCX + rr, CCY, rr);
    }

    CollectTerrainTile(CCX, CCY, 0);

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
        if (m_uploadedTerrainTextures[layer] != Textures[layer]) {
            UploadTerrainLayer(layer, *Textures[layer]);
            m_uploadedTerrainTextures[layer] = Textures[layer];
        }
    }

    const auto projection = BuildLegacyProjection();
    glUseProgram(m_terrainShader);
    glUniformMatrix4fv(glGetUniformLocation(m_terrainShader, "uProjection"), 1, GL_FALSE, projection.data());
    glUniform1f(glGetUniformLocation(m_terrainShader, "uFogDistance"), static_cast<float>(ctViewR) * 256.0f);
    glUniform1f(glGetUniformLocation(m_terrainShader, "uFogFadeStart"), static_cast<float>(ctViewR) * 256.0f - 1024.0f);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D_ARRAY, m_terrainTextureArray);
    glBindVertexArray(m_terrainVAO);

    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
    DrawVertexBatch(m_terrainVertices);

    glBindVertexArray(0);
}

void GLRenderer::RenderWater()
{
    if (!NeedWater) {
        return;
    }

    BeginWaterFrame();

    for (int r = ctViewR; r >= ctViewR1; r -= 2) {
        for (int x = r; x > 0; x -= 2) {
            CollectWaterTile2(CCX - x, CCY + r, r);
            CollectWaterTile2(CCX + x, CCY + r, r);
            CollectWaterTile2(CCX - x, CCY - r, r);
            CollectWaterTile2(CCX + x, CCY - r, r);
        }

        CollectWaterTile2(CCX, CCY - r, r);
        CollectWaterTile2(CCX, CCY + r, r);

        for (int y = r - 2; y > 0; y -= 2) {
            CollectWaterTile2(CCX + r, CCY - y, r);
            CollectWaterTile2(CCX + r, CCY + y, r);
            CollectWaterTile2(CCX - r, CCY + y, r);
            CollectWaterTile2(CCX - r, CCY - y, r);
        }

        CollectWaterTile2(CCX - r, CCY, r);
        CollectWaterTile2(CCX + r, CCY, r);
    }

    int rr = ctViewR1 - 1;
    for (int x = rr; x > -rr; --x) {
        CollectWaterTile(CCX + rr, CCY + x, rr);
        CollectWaterTile(CCX + x, CCY + rr, rr);
    }

    for (rr = ctViewR1 - 2; rr > 0; --rr) {
        for (int x = rr; x > 0; --x) {
            CollectWaterTile(CCX - x, CCY + rr, rr);
            CollectWaterTile(CCX + x, CCY + rr, rr);
            CollectWaterTile(CCX - x, CCY - rr, rr);
            CollectWaterTile(CCX + x, CCY - rr, rr);
        }

        CollectWaterTile(CCX, CCY - rr, rr);
        CollectWaterTile(CCX, CCY + rr, rr);

        for (int y = rr - 1; y > 0; --y) {
            CollectWaterTile(CCX + rr, CCY - y, rr);
            CollectWaterTile(CCX + rr, CCY + y, rr);
            CollectWaterTile(CCX - rr, CCY + y, rr);
            CollectWaterTile(CCX - rr, CCY - y, rr);
        }

        CollectWaterTile(CCX - rr, CCY, rr);
        CollectWaterTile(CCX + rr, CCY, rr);
    }

    CollectWaterTile(CCX, CCY, 0);

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

        CreateMorphedModel(WCircleModel.mptr, &WCircleModel.Animation[0],
                           static_cast<int>(wptr->FTime), wptr->scale);

        // Build the draw item directly with additive=true. We can't go through
        // RenderModelClip / RenderModelClipWater because the public IRenderer
        // overrides don't expose the additive flag (other renderers don't need it).
        ModelDrawItem item;
        const bool closeEnough = fabs(rpos.z) + fabs(rpos.x) < 1000.0f;
        if (!BuildModelDrawItem(item, WCircleModel.mptr,
                                rpos.x, rpos.y, rpos.z, 250, 0, 0, CameraBeta,
                                false, false, closeEnough, /*additive=*/true)) {
            continue;
        }
        item.texture = UploadModelTexture(WCircleModel.mptr);
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
    m_modelTextureFilterState.erase(texture);

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
    m_uploadedTerrainTextures.fill(nullptr);
    m_skyTextureDirty = true;
}

void GLRenderer::ClearVideoBuf()
{
    const Vector3d fogColor = GetCurrentFogColor();
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
        "uniform sampler2D uSkyTexture;\n"
        "uniform vec2 uViewport;\n"
        "uniform vec2 uVideoCenter;\n"
        "uniform vec3 uFogColor;\n"
        "uniform vec3 uQ;\n"
        "uniform vec3 uP;\n"
        "uniform vec3 uR;\n"
        "uniform float uSkyTime;\n"
        "uniform float uForceFog;\n"
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
    if (OptDayNight == 2) k = 0.3f + k / 2.75f;

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

    RenderModelSun(SunModel, x * d, y * d, z * d, static_cast<int>(200.0f * m_skyTraceK));
}

void GLRenderer::RenderModelSun(TModel* mptr, float x0, float y0, float z0, int alpha)
{
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
                255.0f,  // full brightness
                0.0f,    // no fog
                0.0f, 0.0f, 0.0f,  // fog color (unused)
                alphaVal,
                0.0f     // no cutout
            };
        };

        m_sunModelVertices.push_back(makeVertex(face.v1, face.tax, face.tay));
        m_sunModelVertices.push_back(makeVertex(face.v2, face.tbx, face.tby));
        m_sunModelVertices.push_back(makeVertex(face.v3, face.tcx, face.tcy));
    }

    if (m_sunModelVertices.empty()) return;

    const auto projection = BuildLegacyProjection();
    glUseProgram(m_modelShader);
    glUniformMatrix4fv(glGetUniformLocation(m_modelShader, "uProjection"), 1, GL_FALSE, projection.data());

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);  // additive blending for sun
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_FALSE);  // don't write depth for sun

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    glBindVertexArray(m_modelVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_modelVBO);
    const GLsizeiptr vertexSize = static_cast<GLsizeiptr>(m_sunModelVertices.size() * sizeof(ModelVertex));
    glBufferData(GL_ARRAY_BUFFER, vertexSize, nullptr, GL_STREAM_DRAW);
    glBufferSubData(GL_ARRAY_BUFFER, 0, vertexSize, m_sunModelVertices.data());
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(m_sunModelVertices.size()));

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
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
    m_sunLight *= GetTraceK(m_sunScrX, m_sunScrY);
}

void GLRenderer::RenderFSRect(uint32_t color)
{
    float a = static_cast<float>((color >> 24) & 0xFF) / 255.0f;
    float r = static_cast<float>((color >> 16) & 0xFF) / 255.0f;
    float g = static_cast<float>((color >> 8) & 0xFF) / 255.0f;
    float b = static_cast<float>(color & 0xFF) / 255.0f;

    // Create a 1x1 white texture for flat-color rendering
    GLuint whiteTex = 0;
    glGenTextures(1, &whiteTex);
    glBindTexture(GL_TEXTURE_2D, whiteTex);
    const uint32_t white = 0xFFFFFFFF;
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, &white);

    // Build a fullscreen quad — use fog=1.0 so the model shader
    // outputs the fog color (our desired glare color) instead of the texture
    struct FSVertex { float x, y, z, u, v, light, fog, fogR, fogG, fogB, alpha, cutout; };
    const FSVertex quad[6] = {
        {-1.0f, -1.0f, 0.0001f, 0, 0, 255, 1.0f, r, g, b, a, 0},
        { 1.0f, -1.0f, 0.0001f, 0, 0, 255, 1.0f, r, g, b, a, 0},
        { 1.0f,  1.0f, 0.0001f, 0, 0, 255, 1.0f, r, g, b, a, 0},
        {-1.0f, -1.0f, 0.0001f, 0, 0, 255, 1.0f, r, g, b, a, 0},
        { 1.0f,  1.0f, 0.0001f, 0, 0, 255, 1.0f, r, g, b, a, 0},
        {-1.0f,  1.0f, 0.0001f, 0, 0, 255, 1.0f, r, g, b, a, 0},
    };

    // Identity projection — vertices are already in NDC
    const float identity[16] = {
        1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1
    };

    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);  // additive blending for glare

    glUseProgram(m_modelShader);
    glUniformMatrix4fv(glGetUniformLocation(m_modelShader, "uProjection"), 1, GL_FALSE, identity);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, whiteTex);
    glBindVertexArray(m_modelVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_modelVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), nullptr, GL_STREAM_DRAW);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(quad), quad);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);

    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);

    glDeleteTextures(1, &whiteTex);
}

void GLRenderer::RenderSkyPlane()
{
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

    // Compute a stable sky fog color that does NOT depend on the
    // CurFogColor side effect of CalcFogLevel (which can come from any
    // cell's lookup, including our probe or a far vertex). The sky
    // should be tinted by the volume the camera is actually in (or by
    // the sky color when not in a volume), not by an arbitrary cell
    // the probe happened to be in.
    Vector3d targetSkyFogColor;
    if (UNDERWATER && FogsList[127].fogRGB) {
        int rgb = FogsList[127].fogRGB;
        targetSkyFogColor = {
            static_cast<float>((rgb >> 16) & 0xFF) / 255.0f,
            static_cast<float>((rgb >> 8) & 0xFF) / 255.0f,
            static_cast<float>(rgb & 0xFF) / 255.0f
        };
    } else if (CAMERAINFOG && CameraFogI > 0) {
        targetSkyFogColor = DecodeFogColor(FogsList[CameraFogI].fogRGB);
    } else {
        targetSkyFogColor = {
            static_cast<float>(SkyR) / 255.0f,
            static_cast<float>(SkyG) / 255.0f,
            static_cast<float>(SkyB) / 255.0f
        };
    }

    // Temporal low-pass filter on the sky color so that crossing a
    // fog volume boundary produces a smooth color blend across a few
    // frames instead of an instant pop. k=0.15 means ~7-frame settle
    // (~0.12s at 60fps), fast enough to feel responsive but smooth
    // enough to hide the volume-boundary jump.
    if (!m_smoothedSkyFogColorInit) {
        m_smoothedSkyFogColor = targetSkyFogColor;
        m_smoothedSkyFogColorInit = true;
    } else {
        constexpr float k = 0.15f;
        m_smoothedSkyFogColor.x += (targetSkyFogColor.x - m_smoothedSkyFogColor.x) * k;
        m_smoothedSkyFogColor.y += (targetSkyFogColor.y - m_smoothedSkyFogColor.y) * k;
        m_smoothedSkyFogColor.z += (targetSkyFogColor.z - m_smoothedSkyFogColor.z) * k;
    }

    glUseProgram(m_skyShader);
    glUniform1i(glGetUniformLocation(m_skyShader, "uSkyTexture"), 0);
    glUniform2f(glGetUniformLocation(m_skyShader, "uViewport"), static_cast<float>(WinW), static_cast<float>(WinH));
    glUniform2f(glGetUniformLocation(m_skyShader, "uVideoCenter"), static_cast<float>(VideoCX), static_cast<float>(VideoCY));
    glUniform3f(glGetUniformLocation(m_skyShader, "uFogColor"),
                m_smoothedSkyFogColor.x, m_smoothedSkyFogColor.y, m_smoothedSkyFogColor.z);
    glUniform3f(glGetUniformLocation(m_skyShader, "uQ"), qx, qy, qz);
    glUniform3f(glGetUniformLocation(m_skyShader, "uP"), px, py, pz);
    glUniform3f(glGetUniformLocation(m_skyShader, "uR"), rx, ry, rz);
    glUniform1f(glGetUniformLocation(m_skyShader, "uSkyTime"), static_cast<float>(SKYDTime) / 256.0f);
    glUniform1f(glGetUniformLocation(m_skyShader, "uForceFog"), UNDERWATER ? 1.0f : 0.0f);

    // Sample CalcFogLevel directly above the camera (X=0, Z=0 in
    // camera-relative space) at sky height to get the base fog amount
    // for the per-pixel sky gradient. Using (0, ...) instead of
    // (512, ...) keeps the probe in the same map cell as the camera,
    // so the resulting fog amount matches the volume the camera is in
    // (when CAMERAINFOG) and doesn't jump as the camera crosses cell
    // boundaries along the X axis.
    const Vector3d fogProbe = {0.0f, 4.0f * 512.0f * 16.0f, 0.0f};
    const float fogBase = CalcFogLevel(fogProbe);
    glUniform1f(glGetUniformLocation(m_skyShader, "uFogBase"), fogBase);

    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_skyTexture);

    glBindVertexArray(m_skyVAO);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);

    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);

    // Render sun on top of sky (matching D3D/3DFX: sky plane renders sun)
    if (SunModel && !UNDERWATER) {
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
    const char* vsSource =
        "#version 330 core\n"
        "layout (location = 0) in vec2 aPos;\n"
        "layout (location = 1) in vec2 aTexCoord;\n"
        "out vec2 vTexCoord;\n"
        "void main() {\n"
        "   gl_Position = vec4(aPos, 0.0, 1.0);\n"
        "   vTexCoord = aTexCoord;\n"
        "}\n";

    const char* fsSource =
        "#version 330 core\n"
        "in vec2 vTexCoord;\n"
        "out vec4 FragColor;\n"
        "uniform sampler2D uTexture;\n"
        "void main() {\n"
        "   FragColor = texture(uTexture, vTexCoord);\n"
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
    m_uiPixels.clear();
}

void GLRenderer::EnsureUITexture()
{
    if (WinW <= 0 || WinH <= 0) return;
    if (m_uiTexture && m_uiTextureWidth == WinW && m_uiTextureHeight == WinH) return;

    if (!m_uiTexture) glGenTextures(1, &m_uiTexture);
    glBindTexture(GL_TEXTURE_2D, m_uiTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, WinW, WinH, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    m_uiTextureWidth = WinW;
    m_uiTextureHeight = WinH;
    m_uiPixels.assign((size_t)WinW * WinH, 0);
}

void GLRenderer::UpdateUIPixels()
{
    if (!lpVideoBuf || WinW <= 0 || WinH <= 0) return;

    const uint16_t* src = (const uint16_t*)lpVideoBuf;
    for (int y = 0; y < WinH; y++) {
        // GDI DIB is top-down (row 0 = top), OpenGL textures are bottom-up (row 0 = bottom).
        // Flip vertically.
        int destY = WinH - 1 - y;
        int srcOffset = y * VideoPitch;
        int dstOffset = destY * WinW;
        for (int x = 0; x < WinW; x++) {
            uint16_t c = src[srcOffset + x];
            if (c == 0) {
                // Pixel 0 = transparent (HUD background)
                m_uiPixels[dstOffset + x] = 0x00000000;
            } else {
                // lpVideoBuf is 555 (X1R5G5B5): XRRRRRGGGGGBBBBB
                uint32_t r = (c >> 10) & 0x1F;
                uint32_t g = (c >> 5) & 0x1F;
                uint32_t b = c & 0x1F;
                r = (r << 3) | (r >> 2);
                g = (g << 3) | (g >> 2);
                b = (b << 3) | (b >> 2);
                m_uiPixels[dstOffset + x] = 0xFF000000 | (b << 16) | (g << 8) | r;
            }
        }
    }
}

void GLRenderer::RegisterPicture(TPicture* pptr)
{
    // No-op: we copy to lpVideoBuf in DrawPicture instead
    (void)pptr;
}

// Copy picture pixels directly to lpVideoBuf (matching C1 and D3D/3DFX approach).
// lpVideoBuf is a 16-bit DIB section, pictures are already in 565 format after conv_pic.
void GLRenderer::DrawPicture(int x, int y, TPicture& pic)
{
    if (!pic.lpImage || pic.W <= 0 || pic.H <= 0 || !lpVideoBuf) return;

    WORD* dst = (WORD*)lpVideoBuf;
    for (int yy = 0; yy < pic.H; yy++) {
        int dstY = yy + y;
        if (dstY < 0 || dstY >= WinH) continue;
        int copyW = pic.W;
        int srcX = 0;
        int dstX = x;
        if (dstX < 0) { srcX = -dstX; copyW += dstX; dstX = 0; }
        if (dstX + copyW > WinW) copyW = WinW - dstX;
        if (copyW <= 0) continue;
        memcpy(dst + dstY * VideoPitch + dstX,
               pic.lpImage + yy * pic.W + srcX,
               copyW * sizeof(WORD));
    }
}

void GLRenderer::DrawScaledPicture(int x, int y, int w, int h, TPicture& pic)
{
    if (!pic.lpImage || pic.W <= 0 || pic.H <= 0 || !lpVideoBuf) return;

    WORD* dst = (WORD*)lpVideoBuf;
    for (int yy = 0; yy < h; yy++) {
        int dstY = yy + y;
        if (dstY < 0 || dstY >= WinH) continue;
        int sy = yy * pic.H / h;
        for (int xx = 0; xx < w; xx++) {
            int dstX = xx + x;
            if (dstX < 0 || dstX >= WinW) continue;
            int sx = xx * pic.W / w;
            WORD c = pic.lpImage[sy * pic.W + sx];
            if (c != 0) dst[dstY * VideoPitch + dstX] = c;
        }
    }
}

// Upload lpVideoBuf as a texture and draw as a fullscreen overlay.
// Called after all HUD elements have been drawn to lpVideoBuf.
void GLRenderer::DrawHUDOverlay()
{
    if (!m_uiShader || !lpVideoBuf || WinW <= 0 || WinH <= 0) return;

    EnsureUITexture();
    UpdateUIPixels();

    glBindTexture(GL_TEXTURE_2D, m_uiTexture);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, WinW, WinH, GL_RGBA, GL_UNSIGNED_BYTE, m_uiPixels.data());

    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glUseProgram(m_uiShader);
    glActiveTexture(GL_TEXTURE0);
    glBindVertexArray(m_uiVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);

    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
}

void GLRenderer::DrawTrophyText(int x, int y)
{
    // Trophy text is rendered via GDI onto the lpVideoBuf.
    // D3D/3DFX call ddTextOut/FXTextOut which write to the backbuffer.
    // For GL, we draw text onto lpVideoBuf using GDI, then DrawHUDOverlay uploads it.
    // We need to use the hdcCMain + hbmpVideoBuf to draw onto lpVideoBuf.

    if (!hdcCMain || !hbmpVideoBuf || !lpVideoBuf) return;

    HBITMAP hbmpOld = (HBITMAP)SelectObject(hdcCMain, hbmpVideoBuf);
    SetBkMode(hdcCMain, TRANSPARENT);
    HFONT oldFont = NULL;
    if (fnt_Small) oldFont = (HFONT)SelectObject(hdcCMain, fnt_Small);

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
        TextOut(hdcCMain, px + 1, py + 1, str, (int)strlen(str));
        SetTextColor(hdcCMain, color);
        TextOut(hdcCMain, px, py, str, (int)strlen(str));
    };

    SIZE sz;
    auto drawLine = [&](const char* label, const char* value, int color) {
        textOut(tx, ty, label, color);
        GetTextExtentPoint32(hdcCMain, label, (int)strlen(label), &sz);
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

    wsprintf(t, "%s    ", WeapInfo[wep].Name);
    drawLine("Weapon: ", t, 0x00BFBFBF);

    wsprintf(t, "%d", score);
    drawLine("Score: ", t, 0x00BFBFBF);

    if (OptSys) sprintf(t, "%3.1fft", range / 0.3f);
    else        sprintf(t, "%3.1fm", range);
    drawLine("Range of kill: ", t, 0x00BFBFBF);

    if (OptSys) wsprintf(t, "%d.%d.%d   ", ((date>>10) & 255), (date & 255), date>>20);
    else        wsprintf(t, "%d.%d.%d   ", (date & 255), ((date>>10) & 255), date>>20);
    drawLine("Date: ", t, 0x00BFBFBF);

    wsprintf(t, "%d:%02d", ((time>>10) & 255), (time & 255));
    drawLine("Time: ", t, 0x00BFBFBF);

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

    HBITMAP hbmpOld = (HBITMAP)SelectObject(hdcCMain, hbmpVideoBuf);
    SetBkMode(hdcCMain, TRANSPARENT);
    HFONT oldFont = NULL;
    if (fnt_Small) oldFont = (HFONT)SelectObject(hdcCMain, fnt_Small);

    int ctype = Characters[index].CType;
    float scale = Characters[index].scale;
    char t[32];

    int x = VideoCX + WinW / 64;
    int y = VideoCY + (int)(WinH / 6.8);

    auto textOut = [&](int px, int py, const char* str, int color) {
        SetTextColor(hdcCMain, 0x00000000);
        TextOut(hdcCMain, px + 1, py + 1, str, (int)strlen(str));
        SetTextColor(hdcCMain, color);
        TextOut(hdcCMain, px, py, str, (int)strlen(str));
    };

    textOut(x, y, DinoInfo[ctype].Name, 0x0000b000);

    if (OptSys) sprintf(t, "Weight: %3.2ft ", DinoInfo[ctype].Mass * scale * scale / 0.907f);
    else        sprintf(t, "Weight: %3.2fT ", DinoInfo[ctype].Mass * scale * scale);
    textOut(x, y + 16, t, 0x0000b000);

    int R = (int)(VectorLength(SubVectors(Characters[index].pos, PlayerPos)) * 3 / 64.0f);
    if (OptSys) sprintf(t, "Distance: %dft ", R);
    else        sprintf(t, "Distance: %dm  ", R / 3);
    textOut(x, y + 32, t, 0x0000b000);

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
