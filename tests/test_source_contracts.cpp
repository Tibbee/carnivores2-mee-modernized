// ==========================================================================
// test_source_contracts.cpp -- Source-text contracts for wiring that has no
// runtime seam: GLSL block copies, renderer call sites and ordering rules.
// ==========================================================================
// Grouped by subsystem, so a rename or reorder fails one case with one owner.
// These read production sources under C2_SOURCE_ROOT; behavior is covered by
// the runtime suites next to them.

#include <gtest/gtest.h>

#include "Core/NightPresentation.h"

#include <algorithm>
#include <cstddef>
#include <fstream>
#include <iterator>
#include <string>

namespace {
std::string Source(const char* relative) {
    std::ifstream file(std::string(C2_SOURCE_ROOT) + "/" + relative, std::ios::binary);
    EXPECT_TRUE(file.is_open()) << relative;
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

// Text between two markers, with CRs stripped so a CRLF checkout compares
// equal to the LF blob.
std::string Block(const std::string& text, const char* begin, const char* end) {
    const auto start = text.find(begin);
    if (start == std::string::npos) return {};
    const auto finish = text.find(end, start);
    if (finish == std::string::npos) return {};
    auto block = text.substr(start, finish - start);
    block.erase(std::remove(block.begin(), block.end(), '\r'), block.end());
    return block;
}
} // namespace

TEST(SourceContracts, SkyAndMoonWiring) {
    const auto sky = Source("Hunt/Renderer/GLSky.cpp");

    // RenderSun projects the current celestial position before the halo
    // uniform and draw, keeps the async readback, and no longer caches the
    // legacy screen coordinates.
    const auto prep = sky.find("ProjectCelestialBody(sunDir, VideoCX, VideoCY, CameraW, CameraH)");
    ASSERT_NE(prep, std::string::npos);
    EXPECT_LT(prep, sky.find("glUniform2f(m_locSkySunScreenPos"));
    EXPECT_LT(prep, sky.find("glDrawArrays(GL_TRIANGLES, 0, 3)"));
    EXPECT_NE(sky.find("CelestialHaloVisibility(m_celestialProjectionValid, m_skyTraceK)"), std::string::npos);
    EXPECT_NE(sky.find("shouldDrawSky && m_celestialProjectionValid"), std::string::npos);
    EXPECT_NE(sky.find("if (m_celestialTimeOfDay != OptDayNight) ResetCelestialState(OptDayNight);"), std::string::npos);
    const auto renderSun = sky.find("void GLRenderer::RenderSun(");
    const auto getSkyK = sky.find("float GLRenderer::GetSkyK", renderSun);
    ASSERT_NE(renderSun, std::string::npos);
    ASSERT_NE(getSkyK, std::string::npos);
    EXPECT_EQ(sky.substr(renderSun, getSkyK - renderSun).find("m_sunScrX ="), std::string::npos);
    EXPECT_EQ(sky.substr(renderSun, getSkyK - renderSun).find("m_sunScrY ="), std::string::npos);
    EXPECT_NE(sky.find("glReadPixels(bx, WinH - ey, bw, bh, GL_RGBA, GL_UNSIGNED_BYTE, 0)"), std::string::npos);
    EXPECT_NE(sky.find("CelestialVisibilityFromSkySample(OptDayNight, k)"), std::string::npos);

    // The lifecycle reset discards pending samples, the cloud latch and the
    // projection, and runs at sky-pipeline init.
    const auto resetStart = sky.find("void GLRenderer::ResetCelestialState(");
    ASSERT_NE(resetStart, std::string::npos);
    const auto reset = sky.substr(resetStart, sky.find("\n}", resetStart) - resetStart);
    for (const char* part : {"m_skyTraceK = CelestialInitialVisibility(timeOfDay)", "m_skyReadPBOReady = false", "m_cloudLatched = false", "m_celestialProjectionValid = false", "m_sunScrX = m_sunScrY = -1"})
        EXPECT_NE(reset.find(part), std::string::npos);
    const auto init = sky.find("void GLRenderer::InitializeSkyPipeline()");
    ASSERT_NE(init, std::string::npos);
    EXPECT_NE(sky.substr(init).find("ResetCelestialState(OptDayNight);"), std::string::npos);

    // Sun-depth occlusion consumes the shared factor from cached screen
    // coordinates and performs no end-of-frame readback or write-back.
    const auto occlusionStart = sky.find("void GLRenderer::ApplySunDepthOcclusion()");
    ASSERT_NE(occlusionStart, std::string::npos);
    const auto occlusionEnd = sky.find("\n}", occlusionStart);
    ASSERT_NE(occlusionEnd, std::string::npos);
    const auto occlusion = sky.substr(occlusionStart, occlusionEnd - occlusionStart);
    EXPECT_NE(occlusion.find("ApplySunDepthFactor(m_sunLight, m_traceKApplied,"), std::string::npos);
    EXPECT_NE(occlusion.find("m_sunScrX, m_sunScrY, WinW, WinH"), std::string::npos);
    EXPECT_EQ(occlusion.find("glReadPixels"), std::string::npos);
    EXPECT_EQ(occlusion.find("m_traceKApplied ="), std::string::npos);
    EXPECT_NE(sky.find("if (!SunTracePositionOnScreen(x, y, WinW, WinH))"), std::string::npos);

    // Moon glow uses the live policy, the sky shader keeps a dedicated moon
    // visibility fade, and the glow keeps cloud and fog attenuation.
    EXPECT_NE(sky.find("glUniform1f(m_locSkySunGlow, NightMoonGlowStrength(OptDayNight, NightLightingOptions()));"), std::string::npos);
    EXPECT_NE(sky.find("NightCelestialDiscAlpha(OptDayNight) * std::pow(m_skyTraceK, 0.6f) * depthAtten * sunEnvDim * SunGlare_Disc"), std::string::npos);
    const auto skyShader = Source("shaders/sky.frag");
    EXPECT_NE(skyShader.find("isMoon ? uSunVisibility : uSunVisibility * uSunVisibility"), std::string::npos);
    EXPECT_NE(skyShader.find("uSunGlow * occ * (1.0 - fogFactor * 0.5) * (1.0f - envSky)"), std::string::npos);
    EXPECT_NE(skyShader.find("float glowRadius = isMoon ? 110.0"), std::string::npos);

    // The moon-brightness uniform is located per program, cached, and fed by
    // the same policy; the model shader applies it and the loader reads it.
    const auto night = Source("Hunt/Renderer/GLNight.cpp");
    EXPECT_NE(night.find("glGetUniformLocation(program, \"uMoonBrightness\")"), std::string::npos);
    EXPECT_NE(night.find("NightMoonBrightnessGain(OptDayNight, moonBody, NightLightingOptions())"), std::string::npos);
    EXPECT_NE(night.find("moonBrightnessValue.shouldUpload(moonGain)"), std::string::npos);
    EXPECT_NE(night.find("glUniform1f(cached->second.moonBrightness, moonGain)"), std::string::npos);
    EXPECT_NE(Source("Hunt/Renderer/GLRenderer.h").find("MoonBrightnessUniformCache moonBrightnessValue;"), std::string::npos);
    EXPECT_NE(Source("Hunt/Renderer/GLRenderer.h").find("bool moonBody = false"), std::string::npos);
    const auto modelShader = Source("shaders/model.frag");
    EXPECT_NE(modelShader.find("uniform float uMoonBrightness = 1.0;"), std::string::npos);
    EXPECT_NE(modelShader.find("vec4(finalColor * uMoonBrightness, texColor.a * vAlpha)"), std::string::npos);
    const auto engine = Source("Hunt/Game/ConfigFile.cpp");
    EXPECT_NE(engine.find("NightLightingOption::MoonBrightness"), std::string::npos);
    EXPECT_NE(engine.find("NightLightingOption::MoonGlowStrength"), std::string::npos);

    // The obsolete night_lighting key only logs; no mode enum or parser is
    // allowed to come back.
    const auto obsoleteStart = engine.find("else if (_stricmp(key, \"night_lighting\") == 0) {");
    const auto obsoleteEnd = engine.find("else if (_stricmp(key, \"night_world_brightness\")", obsoleteStart);
    ASSERT_NE(obsoleteStart, std::string::npos);
    ASSERT_NE(obsoleteEnd, std::string::npos);
    const auto ignored = engine.substr(obsoleteStart, obsoleteEnd - obsoleteStart);
    EXPECT_NE(ignored.find("night_lighting is obsolete"), std::string::npos);
    EXPECT_EQ(ignored.find("ParseNightLightingOption"), std::string::npos);
    EXPECT_EQ(ignored.find("NightLightingOptions()"), std::string::npos);
    EXPECT_EQ(Source("Hunt/Core/NightLighting.h").find("int mode"), std::string::npos);
    EXPECT_EQ(engine.find("NightLightingOption::Mode"), std::string::npos);
}

TEST(SourceContracts, DrawPassAndShaderWiring) {
    // The night-lighting and flashlight blocks live in one included file that
    // every world shader pulls in; restoration still runs before re-illumination.
    const auto sharedLighting = Source("shaders/flashlight.glsl");
    const auto terrain = Source("shaders/terrain.frag");
    const auto model = Source("shaders/model.frag");
    const auto instanced = Source("shaders/instanced_model.frag");
    EXPECT_NE(sharedLighting.find("uNightSettings.y * uNightStrength"), std::string::npos);
    EXPECT_NE(sharedLighting.find("mix(1.0, uNightSettings.x, uNightStrength)"), std::string::npos);
    EXPECT_NE(sharedLighting.find("clamp(uFlashlight.x * cone * atten, 0.0, 1.0)"), std::string::npos);
    EXPECT_NE(sharedLighting.find("mix(color, originalColor, restoration)"), std::string::npos);
    EXPECT_NE(sharedLighting.find("void ApplyFlashlight("), std::string::npos);
    EXPECT_NE(sharedLighting.find("void ApplyNightLighting("), std::string::npos);
    EXPECT_NE(sharedLighting.find("uNightSettings = vec4(0.22, 0.85, 0.60, 1.0)"), std::string::npos);
    for (const auto* shader : {&terrain, &model, &instanced}) {
        EXPECT_NE(shader->find("#include \"flashlight.glsl\""), std::string::npos);
        EXPECT_EQ(shader->find("void ApplyFlashlight("), std::string::npos);
        EXPECT_EQ(shader->find("void ApplyNightLighting("), std::string::npos);
        const auto restore = shader->find("ApplyNightLighting(finalColor, beamDistance, beamCos);");
        const auto illuminate = shader->find("ApplyFlashlight(finalColor, beamDistance, beamCos);");
        ASSERT_NE(restore, std::string::npos);
        ASSERT_NE(illuminate, std::string::npos);
        EXPECT_LT(restore, illuminate);
    }

    // The NIGHT PRESENTATION block is shared by ui.frag and model.frag; alpha
    // is computed before grading and the readable-rect constants are pinned.
    const auto ui = Source("shaders/ui.frag");
    ASSERT_FALSE(Block(ui, "// BEGIN NIGHT PRESENTATION", "// END NIGHT PRESENTATION").empty());
    EXPECT_EQ(Block(ui, "// BEGIN NIGHT PRESENTATION", "// END NIGHT PRESENTATION"),
              Block(model, "// BEGIN NIGHT PRESENTATION", "// END NIGHT PRESENTATION"));
    EXPECT_LT(ui.find("if (dot(rgb555"), ui.find("ApplyNightPresentation(rgb555"));
    EXPECT_EQ(kNightReadableRectLimit, 16);
    EXPECT_FLOAT_EQ(kNightReadabilityFloor, 0.55f);
    EXPECT_NE(ui.find("uUIReadableRects[16]"), std::string::npos);
    EXPECT_NE(ui.find("brightness = max(brightness, 0.55)"), std::string::npos);
    EXPECT_NE(model.find("if (uNightPresentation.z > 0.5)"), std::string::npos);
    EXPECT_NE(model.find("ApplyNightPresentation(finalColor, uNightPresentation.x);"), std::string::npos);

    // Every active draw path applies the night uniforms; the sky pass stays
    // Unaffected/moon-targeted, never writes the night uniform directly, and
    // never enables the world beam.
    EXPECT_NE(Source("Hunt/Renderer/GLTerrain.cpp").find("ApplyNightLightingUniforms(m_terrainShader.GetProgramID());"), std::string::npos);
    EXPECT_NE(Source("Hunt/Renderer/GLWater.cpp").find("ApplyNightLightingUniforms(m_terrainShader.GetProgramID());"), std::string::npos);
    const auto gpuModel = Source("Hunt/Renderer/GLModel.cpp");
    EXPECT_NE(gpuModel.find("ApplyNightLightingUniforms(m_instancedModelShader.GetProgramID());"), std::string::npos);
    EXPECT_NE(gpuModel.find("ApplyNightLightingUniforms(m_modelShader.GetProgramID(), nightKind);"), std::string::npos);
    const auto sky = Source("Hunt/Renderer/GLSky.cpp");
    EXPECT_NE(sky.find("ApplyNightLightingUniforms(m_skyShader.GetProgramID());"), std::string::npos);
    EXPECT_NE(sky.find("NightDrawKind::Unaffected, OptDayNight == 2);"), std::string::npos);
    EXPECT_EQ(sky.find("glUniform1f(uNight"), std::string::npos);
    EXPECT_NE(sky.find("ApplyFlashlightUniforms(m_modelShader.GetProgramID(), false);"), std::string::npos);

    // Held/viewmodel passes use explicit per-pass policy at five sites; HUD,
    // pause and exit regions stay readable; fullscreen overlay and moon draws
    // stay ungraded.
    std::size_t viewmodelCount = 0, pos = 0;
    while ((pos = gpuModel.find("NightDrawKind::Viewmodel", pos)) != std::string::npos) { ++viewmodelCount; ++pos; }
    EXPECT_EQ(viewmodelCount, 5u); // diffuse, cutout, transparent, env and specular
    const auto night = Source("Hunt/Renderer/GLNight.cpp");
    EXPECT_NE(night.find("FlashlightEnabled(), NightLightingOptions(), kind"), std::string::npos);
    EXPECT_NE(night.find("!presentationEnabled"), std::string::npos);
    EXPECT_NE(night.find("cached->second.presentation >= 0 && presentation[2] > 0.5f"), std::string::npos);
    EXPECT_NE(night.find("glUniform3fv(cached->second.presentation"), std::string::npos);
    EXPECT_NE(night.find("presentationValue.shouldUpload(presentation)"), std::string::npos);
    EXPECT_NE(night.find("strengthValue.shouldUpload({strength})"), std::string::npos);
    EXPECT_NE(night.find("settingsValue.shouldUpload(parameters)"), std::string::npos);
    const auto hud = Source("Hunt/Renderer/GLHUD.cpp");
    EXPECT_NE(hud.find("NightDrawKind::EssentialHud"), std::string::npos);
    EXPECT_NE(hud.find("NightDrawKind::Hud"), std::string::npos);
    EXPECT_NE(hud.find("m_nightReadableRegions.Clear();"), std::string::npos);
    EXPECT_NE(hud.find("&pic == &ExitPic || &pic == &PausePic"), std::string::npos);
    EXPECT_NE(Source("Hunt/Renderer/GLUI.cpp").find("ProtectHUDRect(x0 - 1"), std::string::npos);
    const auto overlayStart = sky.find("void GLRenderer::RenderFSRect");
    const auto moonStart = sky.find("void GLRenderer::RenderModelSun", overlayStart);
    ASSERT_NE(overlayStart, std::string::npos);
    ASSERT_NE(moonStart, std::string::npos);
    EXPECT_NE(sky.substr(overlayStart, moonStart - overlayStart).find("NightDrawKind::Unaffected"), std::string::npos);
    EXPECT_NE(sky.substr(moonStart).find("NightDrawKind::Unaffected"), std::string::npos);

    // The renderer wiring owns the per-draw uploads and clears both program
    // maps; the flashlight cache uses the shared tint and beam policy.
    const auto flashlight = Source("Hunt/Renderer/GLFlashlight.cpp");
    EXPECT_NE(flashlight.find("return FlashlightShouldIlluminate(FlashlightMode, FlashlightOn,"), std::string::npos);
    EXPECT_NE(flashlight.find("OptFlashlightIntensityPct, MyHealth);"), std::string::npos);
    EXPECT_NE(flashlight.find("const bool enabled = worldLit && FlashlightEnabled();"), std::string::npos);
    EXPECT_NE(flashlight.find("std::array<float, 4> beam = { 0.0f,"), std::string::npos);
    EXPECT_NE(flashlight.find("glUniform4fv(cached->second.beam, 1, beam.data());"), std::string::npos);
    EXPECT_NE(flashlight.find("tint = FlashlightTint()"), std::string::npos);
    EXPECT_NE(flashlight.find("beamValue.shouldUpload(beam)"), std::string::npos);
    EXPECT_NE(flashlight.find("colorValue.shouldUpload(tint)"), std::string::npos);
    const auto renderer = Source("Hunt/Renderer/GLRenderer.cpp");
    EXPECT_NE(renderer.find("m_nightLocations.clear();"), std::string::npos);
    EXPECT_NE(renderer.find("m_flashlightLocations.clear();"), std::string::npos);
}

TEST(SourceContracts, CallSitesAndOrdering) {
    // Death clears the health gate before the flashlight toggle, and the
    // toggle before the kill camera; the key handler refuses to relight.
    const auto characters = Source("Hunt/Game/Characters.cpp");
    const auto deathStart = characters.find("void AddDeadBody(");
    const auto deathEnd = characters.find("float AngleDifference(", deathStart);
    ASSERT_NE(deathStart, std::string::npos);
    ASSERT_NE(deathEnd, std::string::npos);
    const auto death = characters.substr(deathStart, deathEnd - deathStart);
    const auto health = death.find("MyHealth = 0;");
    const auto off = death.find("FlashlightOn = false;");
    const auto camera = death.find("DemoPoint.DemoTime = 1;");
    ASSERT_NE(health, std::string::npos);
    ASSERT_NE(off, std::string::npos);
    ASSERT_NE(camera, std::string::npos);
    EXPECT_LT(health, off);
    EXPECT_LT(off, camera);
    EXPECT_NE(Source("Hunt/Game/Hunt.cpp").find("if (pressed(FlashlightKey) && FlashlightMode && MyHealth > 0) {"), std::string::npos);

    // The per-frame UBO guard follows fresh packing, is invalidated by every
    // renderer lifecycle change, and precedes the water partial subdata.
    const auto renderer = Source("Hunt/Renderer/GLRenderer.cpp");
    const auto guard = renderer.find("if (!m_perFrameUpload.shouldUpload(data)) return;");
    ASSERT_NE(guard, std::string::npos);
    EXPECT_LT(renderer.find("data[49] ="), guard);
    EXPECT_LT(guard, renderer.find("glBufferSubData(GL_UNIFORM_BUFFER, 0, data.size()"));
    for (const char* method : {"Initialize()", "Shutdown()", "DestroyContext()", "EnsurePerFrameUBO()"}) {
        const auto start = renderer.find(std::string("GLRenderer::") + method);
        ASSERT_NE(start, std::string::npos);
        const auto end = renderer.find("\n}", start);
        EXPECT_NE(renderer.substr(start, end - start).find("m_perFrameUpload.invalidate();"), std::string::npos);
    }
    const auto water = Source("Hunt/Renderer/GLWater.cpp");
    const auto invalidate = water.find("m_perFrameUpload.invalidate();");
    ASSERT_NE(invalidate, std::string::npos);
    EXPECT_LT(invalidate, water.find("glBufferSubData(GL_UNIFORM_BUFFER, 176,"));
    EXPECT_NE(Source("Hunt/Renderer/GLRenderer.h").find("UniformValueCache<60> m_perFrameUpload;"), std::string::npos);

    // Terrain chunk smoothing calls the non-mutating production helper.
    const auto terrain = Source("Hunt/Renderer/GLTerrain.cpp");
    const auto chunkStart = terrain.find("void GLRenderer::CollectTerrainChunk2x2(");
    const auto chunkEnd = terrain.find("void GLRenderer::RebuildOversizedObjectPlacements", chunkStart);
    ASSERT_NE(chunkStart, std::string::npos);
    ASSERT_NE(chunkEnd, std::string::npos);
    const auto chunk = terrain.substr(chunkStart, chunkEnd - chunkStart);
    EXPECT_NE(chunk.find("SmoothedTileFogColors("), std::string::npos);
    EXPECT_EQ(chunk.find("SmoothFogColors(fogCol"), std::string::npos);

    // The water renderer uses the production cache's absolute-time update and
    // masked lighting/surface lookups.
    const auto effects = Source("Hunt/Game/Effects.cpp");
    EXPECT_NE(effects.find("static water_wave::Cache waves;"), std::string::npos);
    EXPECT_NE(effects.find("waves.update(RealTime, RandomMap, {WWave1Amp, WWave2Amp, WWave3Amp, WWaveSpeed});"), std::string::npos);
    EXPECT_NE(effects.find("waves.lighting[yy & 31][xx & 31]"), std::string::npos);
    EXPECT_NE(effects.find("waves.surface[yy & 31][xx & 31]"), std::string::npos);

    // The workbench message gate runs before every legacy key action and
    // forwards focus; gameplay polling, look and UI draw stay gated without
    // pausing simulation.
    const auto hunt = Source("Hunt/Game/Hunt.cpp");
    const auto route = hunt.find("workbench::HandleUIMessage(hWnd");
    ASSERT_NE(route, std::string::npos);
    EXPECT_LT(route, hunt.find("if (pressed(KeyMap.fkBinoc)"));
    EXPECT_LT(route, hunt.find("switch (message)", route));
    EXPECT_NE(hunt.find("workbench::NotifyUIFocus(blActive != 0)"), std::string::npos);
    const auto controls = Source("Hunt/Game/Controls.cpp");
    EXPECT_LT(controls.find("workbench::FilterGameplayKeys(KeyboardState)"), controls.find("KeyboardState[KeyMap.fkReload]"));
    EXPECT_NE(controls.find("if (workbench::UIOwnsInput()) capture = false;"), std::string::npos);
    EXPECT_NE(controls.find("if (workbench::UIOwnsInput()) return;"), std::string::npos);
    EXPECT_NE(controls.find("workbench::FinishGameplayInputFrame()"), std::string::npos);
    const auto movement = Source("Hunt/Game/PlayerMovement.cpp");
    const auto movementStart = movement.find("void ProcessPlayerMovement()");
    ASSERT_NE(movementStart, std::string::npos);
    EXPECT_LT(movement.find("!workbench::UIBlocksLook()", movementStart), movement.find("GetCursorPos(&ms)", movementStart));
    const auto draw = Source("Hunt/Renderer/GLUI.cpp");
    EXPECT_LT(draw.find("NightVisionOn"), draw.find("workbench::RenderUI()"));
    EXPECT_LT(draw.find("workbench::RenderUI()"), draw.find("SwapBuffers(hdc)"));
}
