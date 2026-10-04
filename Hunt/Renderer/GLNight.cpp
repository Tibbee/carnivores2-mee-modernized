// ==========================================================================
// GLNight.cpp -- Night vision and desaturation rendering
// ==========================================================================

#include "Hunt.h"
#include "GLRenderer.h"
#include "Renderer/GLUtils.h"
#include "Core/NightLighting.h"

#ifdef _gl

#include "glad/glad.h"
#include <cmath>

#ifdef GL_PERF_HOOKS
#include "Renderer/GLPerf.h"
#endif

void GLRenderer::RenderNightDarkness()
{
    // Just the dark overlay -- applied AFTER the HUD is composited
    RenderFSRect(0x80000000, false);
}

void GLRenderer::RenderSceneDesaturated()
{
    if (!m_nightDesatProgram.IsValid()) return;

    EnsureNightSceneTex(m_nightSceneTex, m_nightTexWidth, m_nightTexHeight, WinW, WinH);

    // 1. Copy the current framebuffer (3D scene) to the texture
    glBindTexture(GL_TEXTURE_2D, m_nightSceneTex);
#ifdef GL_PERF_HOOKS
    GL_PERF_TEXTURE_BIND(m_nightSceneTex);
#endif
    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, WinW, WinH);

    // 2. Render opaque fullscreen quad with desaturation shader
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    GLint blendSaved;
    glGetIntegerv(GL_BLEND, &blendSaved);
    glDisable(GL_BLEND);

    m_nightDesatProgram.Use();
    glUniform1i(m_locNightDesatTexture, 0);
    glBindTexture(GL_TEXTURE_2D, m_nightSceneTex);
#ifdef GL_PERF_HOOKS
    GL_PERF_TEXTURE_BIND(m_nightSceneTex);
#endif
    glBindVertexArray(m_uiVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
#ifdef GL_PERF_HOOKS
    GL_PERF_DRAW(2);
#endif
    glBindVertexArray(0);

    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
    if (blendSaved) glEnable(GL_BLEND);
}

void GLRenderer::ShutdownNightDesaturation()
{
    if (m_nightSceneTex && m_hrc) { glDeleteTextures(1, &m_nightSceneTex); m_nightSceneTex = 0; }
    // m_nightDesatProgram destroyed by GLShader destructor
    m_nightTexWidth = 0;
    m_nightTexHeight = 0;
    m_locNightDesatTexture = -1;
    m_locNightDesatStrength = -1;
}

void GLRenderer::InitializeNightDesaturation()
{
    if (!m_nightDesatProgram.LoadFromFile("shaders/night_desat.vert", "shaders/night_desat.frag")) {
        LOG_ERROR("Night desaturation shader compilation failed");
        return;
    }
    LOG_INFO("Night desaturation shader compilation succeeded");

    m_nightDesatProgram.Use();
    m_locNightDesatTexture = glGetUniformLocation(m_nightDesatProgram.GetProgramID(), "uSceneTexture");
    m_locNightDesatStrength = glGetUniformLocation(m_nightDesatProgram.GetProgramID(), "uDesaturateStrength");
    glUniform1i(m_locNightDesatTexture, 0);
    glUniform1f(m_locNightDesatStrength, 0.6f);  // 60% desaturation
}

void GLRenderer::ApplyNightLightingUniforms(unsigned int program, NightDrawKind kind, bool moonBody)
{
    if (program == 0) return;
    auto cached = m_nightLocations.find(program);
    if (cached == m_nightLocations.end()) {
        const NightUniformLocations fresh = {
            glGetUniformLocation(program, "uNightStrength"),
            glGetUniformLocation(program, "uNightSettings"),
            glGetUniformLocation(program, "uNightPresentation"),
            glGetUniformLocation(program, "uMoonBrightness"),
        };
        cached = m_nightLocations.emplace(program, fresh).first;
    }
    // Evaluate every draw, but upload only on first use or a value change.
    // The first non-moon draw still resets a moon gain to 1; subsequent world,
    // held-model and overlay draws reuse it without another driver call.
    if (cached->second.moonBrightness >= 0) {
        const float moonGain = NightMoonBrightnessGain(OptDayNight, moonBody, NightLightingOptions());
        if (cached->second.moonBrightnessValue.shouldUpload(moonGain))
            glUniform1f(cached->second.moonBrightness, moonGain);
    }
    // Compute desired state every draw; changed values (including resets) upload.
    const auto presentation = NightPresentationParameters(OptDayNight, NightVisionOn != 0,
        FlashlightEnabled(), NightLightingOptions(), kind);
    if (cached->second.presentation >= 0 &&
        cached->second.presentationValue.shouldUpload(presentation))
        glUniform3fv(cached->second.presentation, 1, presentation.data());
    // Modded/older shaders without the new uniform keep their world grade.
    const bool presentationEnabled = cached->second.presentation >= 0 && presentation[2] > 0.5f;
    const float strength = kind != NightDrawKind::Unaffected &&
        !presentationEnabled && OptDayNight == 2 && !NightVisionOn ? 1.0f : 0.0f;
    if (cached->second.strength >= 0 && cached->second.strengthValue.shouldUpload({strength}))
        glUniform1f(cached->second.strength, strength);
    if (cached->second.settings >= 0) {
        const auto parameters = NightLightingParameters(OptDayNight, NightVisionOn != 0,
            NightLightingOptions());
        if (cached->second.settingsValue.shouldUpload(parameters))
            glUniform4fv(cached->second.settings, 1, parameters.data());
    }
}

#endif // _gl
