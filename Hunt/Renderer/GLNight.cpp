// ==========================================================================
// GLNight.cpp � Night vision and desaturation rendering
// ==========================================================================

#include "Hunt.h"
#include "GLRenderer.h"
#include "Renderer/GLUtils.h"

#ifdef _gl

#include "glad/glad.h"
#include <cmath>

void GLRenderer::RenderNightDarkness()
{
    // Just the dark overlay — applied AFTER the HUD is composited
    RenderFSRect(0x80000000, false);
}

void GLRenderer::RenderSceneDesaturated()
{
    if (!m_nightDesatProgram) return;

    EnsureNightSceneTex(m_nightSceneTex, m_nightTexWidth, m_nightTexHeight, WinW, WinH);

    // 1. Copy the current framebuffer (3D scene) to the texture
    glBindTexture(GL_TEXTURE_2D, m_nightSceneTex);
    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, WinW, WinH);

    // 2. Render opaque fullscreen quad with desaturation shader
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    GLint blendSaved;
    glGetIntegerv(GL_BLEND, &blendSaved);
    glDisable(GL_BLEND);

    glUseProgram(m_nightDesatProgram);
    glUniform1i(m_locNightDesatTexture, 0);
    glBindTexture(GL_TEXTURE_2D, m_nightSceneTex);
    glBindVertexArray(m_uiVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);

    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
    if (blendSaved) glEnable(GL_BLEND);
}

void GLRenderer::ShutdownNightDesaturation()
{
    if (m_nightSceneTex && m_hrc) { glDeleteTextures(1, &m_nightSceneTex); m_nightSceneTex = 0; }
    if (m_nightDesatProgram && m_hrc) { glDeleteProgram(m_nightDesatProgram); m_nightDesatProgram = 0; }
    m_nightTexWidth = 0;
    m_nightTexHeight = 0;
    m_locNightDesatTexture = -1;
    m_locNightDesatStrength = -1;
}

void GLRenderer::InitializeNightDesaturation()
{
    // Vertex shader: pass-through (matches m_uiVAO layout: pos + texcoord)
    const char* vsSource =
        "#version 330 core\n"
        "layout (location = 0) in vec2 aPos;\n"
        "layout (location = 1) in vec2 aTexCoord;\n"
        "out vec2 vTexCoord;\n"
        "void main() {\n"
        "   gl_Position = vec4(aPos, 0.0, 1.0);\n"
        "   vTexCoord = aTexCoord;\n"
        "}\n";

    // Fragment shader: desaturate scene with configurable strength
    const char* fsSource =
        "#version 330 core\n"
        "in vec2 vTexCoord;\n"
        "out vec4 FragColor;\n"
        "uniform sampler2D uSceneTexture;\n"
        "uniform float uDesaturateStrength;\n"
        "void main() {\n"
        "   vec3 color = texture(uSceneTexture, vTexCoord).rgb;\n"
        "   float gray = dot(color, vec3(0.299, 0.587, 0.114));\n"
        "   vec3 desaturated = mix(color, vec3(gray), uDesaturateStrength);\n"
        "   FragColor = vec4(desaturated, 1.0);\n"
        "}\n";

    GLuint vertexShader = CompileShader(GL_VERTEX_SHADER, vsSource);
    GLuint fragmentShader = CompileShader(GL_FRAGMENT_SHADER, fsSource);
    m_nightDesatProgram = LinkProgram(vertexShader, fragmentShader);
    if (!m_nightDesatProgram) {
        PrintLog("GLRenderer: night desaturation shader compilation FAILED\n");
        return;
    }
    PrintLog("GLRenderer: night desaturation shader compilation OK\n");

    glUseProgram(m_nightDesatProgram);
    m_locNightDesatTexture = glGetUniformLocation(m_nightDesatProgram, "uSceneTexture");
    m_locNightDesatStrength = glGetUniformLocation(m_nightDesatProgram, "uDesaturateStrength");
    glUniform1i(m_locNightDesatTexture, 0);
    glUniform1f(m_locNightDesatStrength, 0.6f);  // 60% desaturation
}

#endif // _gl
