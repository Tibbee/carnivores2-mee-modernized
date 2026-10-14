#version 330 core
out vec4 FragColor;
flat in float vFaceVisible;
in vec2 vTexCoord;
in float vLight;
in float vViewZ;
in float vRadialDist;
in vec3 vWorldNormal;
in float vAlpha;
in float vCutout;
in vec3 vViewPos;        // view-space position (camera at origin)
in float vTintByFog;
in vec3 vVolumetricFogColor;
in float vVolumetricFog;
uniform PerFrame {
   mat4 uProjection;
   vec2 uFogRange;
   vec3 uDistanceFogColor;
   float uForceFog;
   vec3 uFogColor;
   mat4 uView;
};
uniform sampler2D uModelTexture;
uniform float uNightStrength;    // world-only night lighting (0=day, 1=night)
uniform vec3 uCamFogColor;       // camera-in-fog envelope colour
uniform float uCamFogAmount;     // camera-in-fog envelope strength (0 = off)

// Flashlight beam and night-lighting integration; the shared block
// lives in flashlight.glsl and is expanded by the loader.
#include "flashlight.glsl"

void main() {
   if (vFaceVisible < 0.5) discard;
   vec4 texColor = texture(uModelTexture, vTexCoord);
   if (vCutout > 0.5 && texColor.a <= 0.5) discard;
   vec3 litColor = texColor.rgb * vLight;
   // Phase 2.7: branch-less tint via mix (was if > 0.5).
   vec3 tinted = litColor * uDistanceFogColor;
   litColor = mix(litColor, tinted, vTintByFog);
   // Phase 2.5: per-pixel distance fog matching the terrain shader.
   // Ramp from uFogRange.x to uFogRange.y, uses radial camera distance
   // (not forward-only view-space Z) for parity with the terrain and
   // with the radial CPU alpha fade -- see terrain.frag.
   float distanceFog = clamp((vRadialDist - uFogRange.x) / max(uFogRange.y - uFogRange.x, 1.0), 0.0, 1.0);
   // Phase 2.6: volumetric (pocket) fog placeholder -- zero for now.
   vec3 afterVolumetric = mix(litColor, vVolumetricFogColor, vVolumetricFog);
   // Final: fade to distance fog colour over the ramp.
   vec3 finalColor = mix(afterVolumetric, uDistanceFogColor, distanceFog);

   // camera-in-fog global envelope -- see terrain.frag.
   if (uCamFogAmount > 0.001f) {
       // Near baseline so close objects are also hazed (sells "inside fog");
       // far objects still reach the full envelope amount.
       const float kNearFog = 0.25f;
       float camEnvDist = kNearFog + (1.0f - kNearFog) * (1.0f - exp(-2.5f * vViewZ / max(uFogRange.y, 1.0f)));
       finalColor = mix(finalColor, uCamFogColor, uCamFogAmount * camEnvDist);
   }

   // Grade night, restore local color/brightness, then apply the unchanged
   // warm beam contribution. Sky and HUD never reach this world-only path.
   {
      // The beam must be evaluated from the INTERPOLATED POSITION, not from
      // interpolated scalars. GPU varyings are perspective-correct, so
      // vViewPos is exact per fragment, whereas vViewZ/vRadialDist were
      // computed per vertex: along a near/far tile the true depth varies
      // hyperbolically, and interpolating a scalar distance linearly
      // overshoots it badly mid-triangle, which both overstated the falloff
      // distance and dragged the cone's cosine toward 1. See
      // Core/Flashlight.h and tests/test_flashlight.cpp.
      float beamDistance = length(vViewPos);
      float beamCos = max(-vViewPos.z, 0.0) / max(beamDistance, 1.0);
      ApplyNightLighting(finalColor, beamDistance, beamCos);
      ApplyFlashlight(finalColor, beamDistance, beamCos);
   }

   FragColor = vec4(finalColor, texColor.a * vAlpha);
}
