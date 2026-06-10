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
        return DecodeFogColor(FogsList[127].fogRGB);
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
        "void main() {\n"
        "   gl_Position = uProjection * vec4(aPos, 1.0);\n"
        "   vTexCoord = aTexCoord;\n"
        "   vLayer = int(aLayer + 0.5);\n"
        "   vLight = clamp(aLight / 255.0, 0.0, 1.0);\n"
        "   vFog = clamp(aFog / 255.0, 0.0, 1.0);\n"
        "   vFogColor = aFogColor;\n"
        "   vAlpha = clamp(aAlpha, 0.0, 1.0);\n"
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
        "uniform sampler2DArray uTerrainArray;\n"
        "void main() {\n"
        "   vec4 texColor = texture(uTerrainArray, vec3(vTexCoord, float(vLayer)));\n"
        "   if (texColor.a < 0.05) discard;\n"
        "   vec3 litColor = texColor.rgb * vLight;\n"
        "   vec3 finalColor = mix(litColor, vFogColor, vFog);\n"
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
                                    bool clippedVariant) const
{
    if (!mptr || !mptr->lpTexture || !mptr->gVertex || !mptr->gFace) {
        return false;
    }

    const float ca = std::cos(al);
    const float sa = std::sin(al);
    const float cb = std::cos(bt);
    const float sb = std::sin(bt);

    const Vector3d unrotatedCenter = {
        x0 * ca - z0 * sa,
        y0,
        z0 * ca + x0 * sa
    };

    static thread_local std::vector<Vector3d> transformed;
    static thread_local std::vector<Vector3d> unrotated;
    transformed.clear();
    unrotated.clear();
    transformed.reserve(mptr->VCount);
    unrotated.reserve(mptr->VCount);

    bool anyVisible = false;
    for (int i = 0; i < mptr->VCount; ++i) {
        transformed.push_back(TransformModelVertex(mptr->gVertex[i], x0, y0, z0, ca, sa, cb, sb));
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
    };

    static thread_local std::vector<ModelClipVertex> polygon;
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
                                   bool enableBlend)
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
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
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
        DrawModelVertices(item.texture, item.opaqueVertices, projection, true, false);

        if (!item.cutoutVertices.empty()) {
            SetModelTextureFiltering(item.texture, true);
            DrawModelVertices(item.texture, item.cutoutVertices, projection, true, false);
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
        DrawModelVertices(item->texture, item->transparentVertices, projection, true, true);
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
    DrawModelVertices(texture, vertices, projection, true, true);
}

void GLRenderer::RenderModel(TModel* mptr, float x0, float y0, float z0,
                             int light, int vt, float al, float bt)
{
    ModelDrawItem item;
    if (!BuildModelDrawItem(item, mptr, x0, y0, z0, light, vt, al, bt, false, false, false)) {
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
    if (!BuildModelDrawItem(item, mptr, x0, y0, z0, light, vt, al, bt, false, false, true)) {
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
    if (!BuildModelDrawItem(item, mptr, x0, y0, z0, light, vt, al, bt, true, false, true)) {
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
    if (!BuildModelDrawItem(item, mptr, x0, y0, z0, light, vt, al, bt, false, true, true)) {
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
    DrawModelVertices(item.texture, item.opaqueVertices, projection, true, false);
    if (!item.cutoutVertices.empty()) {
        SetModelTextureFiltering(item.texture, true);
        DrawModelVertices(item.texture, item.cutoutVertices, projection, true, false);
        SetModelTextureFiltering(item.texture, false);
    }
    if (!item.transparentVertices.empty()) {
        const bool useNearestFiltering = NeedsNearestModelFiltering(item.transparentVertices);
        if (useNearestFiltering) {
            SetModelTextureFiltering(item.texture, true);
        }
        DrawModelVertices(item.texture, item.transparentVertices, projection, true, true);
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
        return DecodeFogColor(FogsList[127].fogRGB);
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
    const Vector3d fogColor = GetCurrentFogColor();

    vertices.push_back({v0.v.x, v0.v.y, v0.v.z, uv[0].x, uv[0].y, layer, static_cast<float>(v0.Light), v0.Fog, fogColor.x, fogColor.y, fogColor.z, alpha0});
    vertices.push_back({v1.v.x, v1.v.y, v1.v.z, uv[1].x, uv[1].y, layer, static_cast<float>(v1.Light), v1.Fog, fogColor.x, fogColor.y, fogColor.z, alpha1});
    vertices.push_back({v2.v.x, v2.v.y, v2.v.z, uv[2].x, uv[2].y, layer, static_cast<float>(v2.Light), v2.Fog, fogColor.x, fogColor.y, fogColor.z, alpha2});
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

    if (a00 > 0.0f || a10 > 0.0f || a11 > 0.0f) {
        if (IsWaterTriangleValid(v00, v10, v11, BackViewR)) {
            AppendWaterTriangle(m_waterVertices, v00, v10, v11, textureLayer, false, false, 0, a00, a10, a11);
        }
    }

    if (a00 > 0.0f || a11 > 0.0f || a01 > 0.0f) {
        if (IsWaterTriangleValid(v00, v11, v01, BackViewR)) {
            AppendWaterTriangle(m_waterVertices, v00, v11, v01, textureLayer, false, true, 0, a00, a11, a01);
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

    if (a00 > 0.0f || a20 > 0.0f || a22 > 0.0f) {
        if (IsWaterTriangleValid(v00, v20, v22, BackViewR)) {
            AppendWaterTriangle(m_waterVertices, v00, v20, v22, textureLayer, false, false, 0, a00, a20, a22);
        }
    }

    if (a00 > 0.0f || a22 > 0.0f || a02 > 0.0f) {
        if (IsWaterTriangleValid(v00, v22, v02, BackViewR)) {
            AppendWaterTriangle(m_waterVertices, v00, v22, v02, textureLayer, false, true, 0, a00, a22, a02);
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

void GLRenderer::RegisterPicture(TPicture* pptr)
{
    (void)pptr;
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

void GLRenderer::RenderSkyPlane()
{
}

void GLRenderer::DrawPicture(int x, int y, TPicture& pic)
{
    (void)x; (void)y; (void)pic;
}

void GLRenderer::DrawScaledPicture(int x, int y, int w, int h, TPicture& pic)
{
    (void)x; (void)y; (void)w; (void)h; (void)pic;
}

void GLRenderer::DrawTrophyText(int x, int y)
{
    (void)x; (void)y;
}

void GLRenderer::RenderHealthBar()
{
}

void GLRenderer::Render_Cross(int x, int y)
{
    (void)x; (void)y;
}

void GLRenderer::Render_LifeInfo(int index)
{
    (void)index;
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
