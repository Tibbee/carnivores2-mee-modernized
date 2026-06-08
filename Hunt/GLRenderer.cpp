// ==========================================================================
// GLRenderer.cpp — OpenGL 3.3 Core Profile renderer for Carnivores 2 ME
//
// Ported from C1's GLRenderer, adapted to C2 ME data structures.
// Currently implements context initialization only.
// Rendering methods are stubs that will be implemented incrementally.
// ==========================================================================

#include "Hunt.h"
#include "GLRenderer.h"

#ifdef _gl

#include "glad/glad.h"
#include <cstdio>
#include <cstring>

// ============================================================================
// WGL extension definitions (for creating modern GL context)
// ============================================================================

#define WGL_CONTEXT_MAJOR_VERSION_ARB     0x2091
#define WGL_CONTEXT_MINOR_VERSION_ARB     0x2092
#define WGL_CONTEXT_PROFILE_MASK_ARB      0x2133
#define WGL_CONTEXT_CORE_PROFILE_BIT_ARB  0x00000001

typedef HGLRC (WINAPI *PFNWGLCREATECONTEXTATTRIBSARBPROC)(HDC, HGLRC, const int*);

// ============================================================================
// GLAD loader for Windows
// ============================================================================

static HMODULE libGL = nullptr;

static void* glad_get_proc(const char* name) {
    void* p = (void*)wglGetProcAddress(name);
    if (p == 0 || p == (void*)0x1 || p == (void*)0x2 || p == (void*)0x3 || p == (void*)-1) {
        p = (void*)GetProcAddress(libGL, name);
    }
    return p;
}

// ============================================================================
// Global GL renderer instance
// ============================================================================

GLRenderer* g_GLRenderer = nullptr;

// ============================================================================
// Construction / Destruction
// ============================================================================

GLRenderer::GLRenderer()
{
    memset(m_TextureCache, 0, sizeof(m_TextureCache));
    memset(m_TextureUsed, 0, sizeof(m_TextureUsed));
}

GLRenderer::~GLRenderer()
{
    Shutdown();
}

// ============================================================================
// Context Creation
// ============================================================================

bool GLRenderer::CreateContext()
{
    PrintLog("\n");
    PrintLog("==Init OpenGL==\n");

    // Get window handle
    m_hwnd = hwndMain;
    if (!m_hwnd) {
        PrintLog("GL: ERROR - hwndMain is NULL!\n");
        return false;
    }
    PrintLog("GL: Window handle obtained.\n");

    // Get device context
    m_hdc = GetDC(m_hwnd);
    if (!m_hdc) {
        PrintLog("GL: ERROR - GetDC failed!\n");
        return false;
    }
    PrintLog("GL: Device context obtained.\n");

    // Set up pixel format
    PrintLog("GL: Setting up pixel format...\n");
    PIXELFORMATDESCRIPTOR pfd = {
        sizeof(PIXELFORMATDESCRIPTOR),
        1,
        PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
        PFD_TYPE_RGBA,
        32,             // Color bits
        0, 0, 0, 0, 0, 0,
        0,
        0,
        0,
        0, 0, 0, 0,
        24,             // Depth bits
        8,              // Stencil bits
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
    PrintLog("GL: Pixel format chosen.\n");

    if (!SetPixelFormat(m_hdc, pixelFormat, &pfd)) {
        PrintLog("GL: ERROR - Failed to set pixel format.\n");
        return false;
    }
    PrintLog("GL: Pixel format set.\n");

    // Create temporary legacy context to load extensions
    PrintLog("GL: Creating temporary context...\n");
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
    PrintLog("GL: Temporary context created.\n");

    // Try to create OpenGL 3.3 Core Profile context
    PrintLog("GL: Looking for wglCreateContextAttribsARB...\n");
    PFNWGLCREATECONTEXTATTRIBSARBPROC wglCreateContextAttribsARB =
        (PFNWGLCREATECONTEXTATTRIBSARBPROC)wglGetProcAddress("wglCreateContextAttribsARB");

    if (wglCreateContextAttribsARB) {
        PrintLog("GL: Creating OpenGL 3.3 Core Profile context...\n");
        int attribs[] = {
            WGL_CONTEXT_MAJOR_VERSION_ARB, 3,
            WGL_CONTEXT_MINOR_VERSION_ARB, 3,
            WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
            0
        };

        m_hrc = wglCreateContextAttribsARB(m_hdc, 0, attribs);
        if (m_hrc) {
            // Success - destroy temporary context and use the new one
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

    // Initialize GLAD
    PrintLog("GL: Loading opengl32.dll...\n");
    libGL = LoadLibraryA("opengl32.dll");
    if (!libGL) {
        PrintLog("GL: ERROR - Failed to load opengl32.dll!\n");
        return false;
    }
    PrintLog("GL: opengl32.dll loaded.\n");

    PrintLog("GL: Initializing GLAD...\n");
    if (!gladLoadGLLoader((GLADloadproc)glad_get_proc)) {
        PrintLog("GL: ERROR - Failed to initialize GLAD.\n");
        return false;
    }
    PrintLog("GL: GLAD initialized successfully.\n");

    // Log GL info
    char logMsg[512];
    const char* version = (const char*)glGetString(GL_VERSION);
    if (version) {
        sprintf(logMsg, "GL: Version: %s\n", version);
        PrintLog(logMsg);
    } else {
        PrintLog("GL: ERROR - glGetString(GL_VERSION) returned NULL!\n");
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

    // Log extensions count
    GLint numExtensions = 0;
    glGetIntegerv(GL_NUM_EXTENSIONS, &numExtensions);
    sprintf(logMsg, "GL: Extensions: %d\n", numExtensions);
    PrintLog(logMsg);

    PrintLog("==OpenGL Initialized==\n");
    PrintLog("\n");

    return true;
}

// ============================================================================
// GL State Initialization
// ============================================================================

bool GLRenderer::InitGLState()
{
    PrintLog("GL: Initializing GL state...\n");

    // Enable depth testing
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);

    // Disable face culling for now (game uses alpha-blended faces)
    glDisable(GL_CULL_FACE);

    // Enable blending for alpha textures
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Set clear color to cornflower blue (easy to see if rendering works)
    glClearColor(0.392f, 0.584f, 0.929f, 1.0f);

    // Set viewport
    glViewport(0, 0, WinW, WinH);

    PrintLog("GL: GL state initialized.\n");
    return true;
}

// ============================================================================
// GL Extensions (placeholder)
// ============================================================================

void GLRenderer::LoadGLExtensions()
{
    // TODO: Load additional GL extensions if needed
    PrintLog("GL: LoadGLExtensions() — no additional extensions needed yet.\n");
}

// ============================================================================
// Test functions (temporary - verifies GL rendering pipeline)
// ============================================================================

// Simple vertex shader (supports both colored and textured rendering)
static const char* kVertexShaderSource = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aColor;
layout (location = 2) in vec2 aTexCoord;
out vec3 ourColor;
out vec2 TexCoord;
void main()
{
    gl_Position = vec4(aPos, 1.0);
    ourColor = aColor;
    TexCoord = aTexCoord;
}
)";

// Simple fragment shader (supports both colored and textured rendering)
static const char* kFragmentShaderSource = R"(
#version 330 core
in vec3 ourColor;
in vec2 TexCoord;
out vec4 FragColor;
uniform sampler2D uTexture;
uniform bool uUseTexture;
void main()
{
    if (uUseTexture)
        FragColor = texture(uTexture, TexCoord);
    else
        FragColor = vec4(ourColor, 1.0);
}
)";

unsigned int GLRenderer::Expand1555to8888(unsigned short c)
{
    // X1R5G5B5: bits 14:10 = R, 9:5 = G, 4:0 = B
    if (c == 0) return 0x00000000;  // Transparent black

    unsigned int r = (c >> 10) & 0x1F;
    unsigned int g = (c >> 5)  & 0x1F;
    unsigned int b = (c)       & 0x1F;

    // Expand 5-bit to 8-bit
    r = (r << 3) | (r >> 2);
    g = (g << 3) | (g >> 2);
    b = (b << 3) | (b >> 2);

    return 0xFF000000 | (b << 16) | (g << 8) | r;
}

bool GLRenderer::InitTestTriangle()
{
    PrintLog("GL: Initializing test triangle...\n");

    // Compile vertex shader
    unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &kVertexShaderSource, nullptr);
    glCompileShader(vertexShader);

    int success;
    char infoLog[512];
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(vertexShader, 512, nullptr, infoLog);
        PrintLog("GL: ERROR - Vertex shader compilation failed:\n");
        PrintLog(infoLog);
        return false;
    }
    PrintLog("GL: Vertex shader compiled.\n");

    // Compile fragment shader
    unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &kFragmentShaderSource, nullptr);
    glCompileShader(fragmentShader);

    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(fragmentShader, 512, nullptr, infoLog);
        PrintLog("GL: ERROR - Fragment shader compilation failed:\n");
        PrintLog(infoLog);
        return false;
    }
    PrintLog("GL: Fragment shader compiled.\n");

    // Link shader program
    m_TestShader = glCreateProgram();
    glAttachShader(m_TestShader, vertexShader);
    glAttachShader(m_TestShader, fragmentShader);
    glLinkProgram(m_TestShader);

    glGetProgramiv(m_TestShader, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(m_TestShader, 512, nullptr, infoLog);
        PrintLog("GL: ERROR - Shader program linking failed:\n");
        PrintLog(infoLog);
        return false;
    }
    PrintLog("GL: Shader program linked.\n");

    // Delete shaders (no longer needed after linking)
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    // Quad vertices: position (x,y,z) + color (r,g,b) + texcoord (u,v)
    // Large quad covering most of the screen
    float vertices[] = {
        // positions        // colors          // texcoords
        -0.9f, -0.9f, 0.0f,  1.0f, 1.0f, 1.0f,  0.0f, 0.0f,  // bottom left
         0.9f, -0.9f, 0.0f,  1.0f, 1.0f, 1.0f,  1.0f, 0.0f,  // bottom right
         0.9f,  0.9f, 0.0f,  1.0f, 1.0f, 1.0f,  1.0f, 1.0f,  // top right

        -0.9f, -0.9f, 0.0f,  1.0f, 1.0f, 1.0f,  0.0f, 0.0f,  // bottom left
         0.9f,  0.9f, 0.0f,  1.0f, 1.0f, 1.0f,  1.0f, 1.0f,  // top right
        -0.9f,  0.9f, 0.0f,  1.0f, 1.0f, 1.0f,  0.0f, 1.0f,  // top left
    };

    // Create VAO and VBO
    glGenVertexArrays(1, &m_TestVAO);
    glGenBuffers(1, &m_TestVBO);

    glBindVertexArray(m_TestVAO);

    glBindBuffer(GL_ARRAY_BUFFER, m_TestVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    // Position attribute (location = 0)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // Color attribute (location = 1)
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // Texture coordinate attribute (location = 2)
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);

    m_TestTriangleReady = true;
    PrintLog("GL: Test quad initialized.\n");
    return true;
}

bool GLRenderer::InitTestTexture()
{
    PrintLog("GL: Initializing test texture...\n");

    // Try to load weapon texture (bullet1.tga) - more recognizable than terrain texture
    TPicture pic = {};
    const char* weaponPath = "HUNTDAT\\WEAPONS\\ammo\\bullet1.tga";
    PrintLog("GL: Loading weapon texture: ");
    PrintLog((LPSTR)weaponPath);
    PrintLog("\n");

    // Load TGA file
    HANDLE hfile = CreateFile(weaponPath, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hfile == INVALID_HANDLE_VALUE) {
        PrintLog("GL: Failed to open weapon texture, falling back to terrain texture.\n");
        // Fallback to terrain texture
        if (!Textures[0]) {
            PrintLog("GL: No textures available.\n");
            return false;
        }
        TEXTURE* tex = Textures[0];
        pic.W = 128;
        pic.H = 128;
        pic.lpImage = (WORD*)tex->DataA;
    } else {
        // Read TGA header
        DWORD l;
        WORD w, h;
        SetFilePointer(hfile, 12, 0, FILE_BEGIN);
        ReadFile(hfile, &w, 2, &l, NULL);
        ReadFile(hfile, &h, 2, &l, NULL);
        SetFilePointer(hfile, 18, 0, FILE_BEGIN);

        pic.W = w;
        pic.H = h;
        pic.lpImage = (WORD*)HeapAlloc(GetProcessHeap(), 0, w * h * 2);
        ReadFile(hfile, pic.lpImage, w * h * 2, &l, NULL);
        CloseHandle(hfile);

        char sizeLog[128];
        sprintf(sizeLog, "GL: Loaded TGA %dx%d\n", w, h);
        PrintLog(sizeLog);
    }

    // Convert 16-bit X1R5G5B5 to 32-bit RGBA
    int pixelCount = pic.W * pic.H;
    unsigned int* buffer = (unsigned int*)HeapAlloc(GetProcessHeap(), 0, pixelCount * 4);
    for (int i = 0; i < pixelCount; i++) {
        buffer[i] = Expand1555to8888(pic.lpImage[i]);
    }
    PrintLog("GL: Texture expanded to 32-bit RGBA.\n");

    // Debug: log first 4 pixels
    char pixLog[256];
    for (int i = 0; i < 4 && i < pixelCount; i++) {
        unsigned int p = buffer[i];
        sprintf(pixLog, "GL: Pixel[%d] = %08X => A=%02X R=%02X G=%02X B=%02X\n",
                i, p, (p>>24)&0xFF, (p>>0)&0xFF, (p>>8)&0xFF, (p>>16)&0xFF);
        PrintLog(pixLog);
    }

    // Create GL texture
    glGenTextures(1, &m_TestTexture);
    glBindTexture(GL_TEXTURE_2D, m_TestTexture);

    // Set texture parameters (matching C1's terrain/model textures)
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Upload texture
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, pic.W, pic.H, 0, GL_RGBA, GL_UNSIGNED_BYTE, buffer);
    char sizeLog[128];
    sprintf(sizeLog, "GL: Texture uploaded (%dx%d, GL_NEAREST)\n", pic.W, pic.H);
    PrintLog(sizeLog);

    // Clean up
    HeapFree(GetProcessHeap(), 0, buffer);
    if (pic.lpImage != (WORD*)Textures[0]->DataA) {
        HeapFree(GetProcessHeap(), 0, pic.lpImage);
    }

    m_TestTextureReady = true;
    PrintLog("GL: Test texture initialized successfully.\n");
    return true;
}

void GLRenderer::RenderTestTriangle()
{
    if (!m_TestTriangleReady) return;

    static int frameCount = 0;
    frameCount++;

    // Log every 60 frames to avoid spam
    if (frameCount % 60 == 1) {
        char dbg[256];
        sprintf(dbg, "GL: RenderTestTriangle() called, frame=%d, texture=%u, ready=%d\n",
                frameCount, m_TestTexture, m_TestTextureReady);
        PrintLog(dbg);
    }

    glUseProgram(m_TestShader);

    // Set texture uniform
    GLint useTextureLoc = glGetUniformLocation(m_TestShader, "uUseTexture");
    GLint textureLoc = glGetUniformLocation(m_TestShader, "uTexture");

    if (m_TestTextureReady && m_TestTexture) {
        // Render with texture
        glUniform1i(useTextureLoc, 1);
        glUniform1i(textureLoc, 0);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_TestTexture);
    } else {
        // Render with colors only
        glUniform1i(useTextureLoc, 0);
    }

    glBindVertexArray(m_TestVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);  // 6 vertices for quad (2 triangles)
    glBindVertexArray(0);
}

// ============================================================================
// Lifecycle
// ============================================================================

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

    // Initialize test quad (temporary - verifies GL rendering works)
    if (!InitTestTriangle()) {
        PrintLog("GL: WARNING - Test quad initialization failed.\n");
        // Non-fatal, continue without test quad
    }

    // NOTE: InitTestTexture() is called in Activate3DHardware() because
    // textures are not loaded yet when Initialize() runs.

    m_Initialized = true;
    PrintLog("GL: Initialize() completed successfully.\n");
    return true;
}

void GLRenderer::Shutdown()
{
    if (!m_Initialized && !m_hrc) return;

    PrintLog("GL: Shutting down...\n");

    // Release test triangle resources
    ShutdownTestTriangle();

    // TODO: Release other GL resources (textures, VAOs, VBOs, shaders)

    // Destroy context
    DestroyContext();

    m_Initialized = false;
    PrintLog("GL: Shutdown complete.\n");
}

void GLRenderer::DestroyContext()
{
    if (m_hrc) {
        if (wglGetCurrentContext() == m_hrc) {
            wglMakeCurrent(nullptr, nullptr);
        }
        wglDeleteContext(m_hrc);
        m_hrc = nullptr;
        PrintLog("GL: Rendering context destroyed.\n");
    }

    if (m_hdc && m_hwnd) {
        ReleaseDC(m_hwnd, m_hdc);
        m_hdc = nullptr;
        PrintLog("GL: Device context released.\n");
    }

    if (libGL) {
        FreeLibrary(libGL);
        libGL = nullptr;
    }
}

// ============================================================================
// Scene Rendering (stubs)
// ============================================================================

void GLRenderer::DrawScene()
{
    // TODO: Implement main scene rendering
    // For now, clearing and test triangle are handled via RenderSkyPlane() stub
}

void GLRenderer::DrawPostObjects()
{
    // TODO: Implement post-object rendering
}

// ============================================================================
// Resource Management (stubs)
// ============================================================================

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
    (void)mptr;
}

void GLRenderer::ResetTerrainTextureCache()
{
}

void GLRenderer::ClearLevelTextureCache()
{
    memset(m_TextureUsed, 0, sizeof(m_TextureUsed));
}

// ============================================================================
// Frame Management (stubs)
// ============================================================================

void GLRenderer::ClearVideoBuf()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Render test triangle (temporary - remove after verifying GL works)
    RenderTestTriangle();
}

void GLRenderer::WaitRetrace()
{
    // TODO: VSync control
}

void GLRenderer::PostProcess()
{
    // TODO: Post-processing effects
}

// ============================================================================
// 3D Rendering — Terrain (stubs)
// ============================================================================

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

// ============================================================================
// 3D Rendering — Models (stubs)
// ============================================================================

void GLRenderer::RenderModel(TModel* mptr, float x0, float y0, float z0,
                             int light, float al, float bt)
{
    (void)mptr; (void)x0; (void)y0; (void)z0;
    (void)light; (void)al; (void)bt;
}

void GLRenderer::RenderModelClip(TModel* mptr, float x0, float y0, float z0,
                                 int light, float al, float bt)
{
    (void)mptr; (void)x0; (void)y0; (void)z0;
    (void)light; (void)al; (void)bt;
}

void GLRenderer::RenderModelClipWater(TModel* mptr, float x0, float y0, float z0,
                                      int light, float al, float bt)
{
    (void)mptr; (void)x0; (void)y0; (void)z0;
    (void)light; (void)al; (void)bt;
}

void GLRenderer::RenderNearModel(TModel* mptr, float x0, float y0, float z0,
                                 int light, float al, float bt)
{
    (void)mptr; (void)x0; (void)y0; (void)z0;
    (void)light; (void)al; (void)bt;
}

// ============================================================================
// Character / Entity Renders (stubs)
// ============================================================================

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

// ============================================================================
// 2D Rendering (stubs)
// ============================================================================

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

// ============================================================================
// System / State
// ============================================================================

void GLRenderer::SetVideoMode(int w, int h)
{
    glViewport(0, 0, w, h);
}

void GLRenderer::SetFullScreen()
{
    // TODO: Toggle fullscreen
}

bool GLRenderer::IsSoftwareStyle() const
{
    return false;
}

void GLRenderer::ShutdownTestTriangle()
{
    if (m_TestVAO) {
        glDeleteVertexArrays(1, &m_TestVAO);
        m_TestVAO = 0;
    }
    if (m_TestVBO) {
        glDeleteBuffers(1, &m_TestVBO);
        m_TestVBO = 0;
    }
    if (m_TestShader) {
        glDeleteProgram(m_TestShader);
        m_TestShader = 0;
    }
    if (m_TestTexture) {
        glDeleteTextures(1, &m_TestTexture);
        m_TestTexture = 0;
    }
    m_TestTriangleReady = false;
    m_TestTextureReady = false;
}

#endif // _gl
