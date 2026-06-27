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



















// ============================================================================
// HUD Pipeline
// ============================================================================








// ======================================================================
// Night desaturation + darkness overlay
// ======================================================================









// Copy picture pixels directly to lpVideoBuf (matching C1 and D3D/3DFX approach).
// lpVideoBuf is a 16-bit DIB section, pictures are already in 565 format after conv_pic.


// Phase 2.20: upload lpVideoBuf directly as GL_RGB5 texture.
// lpVideoBuf is 16-bit X1R5G5B5, top-down, stride = VideoPitch.
// The fragment shader converts 555→RGBA8 and handles transparency.





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
