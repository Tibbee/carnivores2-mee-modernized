// ==========================================================================
// GLFlashlight.cpp -- world-only flashlight beam (GL renderer)
// ==========================================================================
//
// The beam is forward lighting inside the existing world shaders, not a
// post-process over a copied framebuffer. See Core/Flashlight.h for the
// rationale and for the curve this pushes: every value here is consumed as-is
// by the identical block in shaders/terrain.frag, model.frag and
// instanced_model.frag.

#include "Hunt.h"
#include "GLRenderer.h"

#ifdef _gl

#include "glad/glad.h"
#include "Core/Flashlight.h"

bool GLRenderer::FlashlightEnabled() const
{
    // Purchased for this hunt (-flashlight from the menu) and toggled on.
    // A zero intensity is the config-level off switch, so checking it here
    // makes the toggle honest: the HUD message and the beam agree. Death
    // also disables flashlight-driven HUD restoration through this policy.
    return FlashlightShouldIlluminate(FlashlightMode, FlashlightOn,
        OptFlashlightIntensityPct, MyHealth);
}

void GLRenderer::ApplyFlashlightUniforms(unsigned int program, bool worldLit)
{
    if (program == 0) return;

    // Location cache keyed by program. A function-local static cannot be used
    // here the way the neighbouring single-program sites do it: this helper
    // runs for every world program, and a static would hand the second and
    // later programs the locations belonging to the first.
    auto cached = m_flashlightLocations.find(program);
    if (cached == m_flashlightLocations.end()) {
        const FlashlightUniformLocations fresh = {
            glGetUniformLocation(program, "uFlashlight"),
            glGetUniformLocation(program, "uFlashlightColor"),
        };
        cached = m_flashlightLocations.emplace(program, fresh).first;
    }
    if (cached->second.beam < 0) {
        // Program does not declare the effect (ui.frag, sky.frag): nothing to
        // keep in sync, and nothing stale can accumulate.
        return;
    }

    // A transition to disabled must still upload; only identical values can
    // be skipped. Desired beam state is evaluated even for non-world draws.
    std::array<float, 4> beam = { 0.0f, kFlashlightRangeDefault, 1.0f, 0.0f };
    const bool enabled = worldLit && FlashlightEnabled();
    if (enabled) {
        float innerDeg = static_cast<float>(OptFlashlightInnerAngleDeg);
        float outerDeg = static_cast<float>(OptFlashlightOuterAngleDeg);
        // Repair rather than trust: an unordered pair reaches smoothstep().
        NormalizeFlashlightConeDeg(innerDeg, outerDeg);
        beam[0] = static_cast<float>(OptFlashlightIntensityPct) / 100.0f;
        beam[1] = static_cast<float>(ClampFlashlightRange(OptFlashlightRange));
        beam[2] = FlashlightCosFromAngleDeg(innerDeg);
        beam[3] = FlashlightCosFromAngleDeg(outerDeg);
    }

    if (cached->second.beamValue.shouldUpload(beam))
        glUniform4fv(cached->second.beam, 1, beam.data());
    if (cached->second.color >= 0) {
        const auto& tint = FlashlightTint();
        if (cached->second.colorValue.shouldUpload(tint))
            glUniform3fv(cached->second.color, 1, tint.data());
    }
}

#endif // _gl
