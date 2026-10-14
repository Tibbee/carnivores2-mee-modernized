#version 330 core
out vec4 FragColor;
// Reset to 1 for every non-moon draw sharing this program. Alpha is unchanged.
uniform float uMoonBrightness = 1.0;
in vec2 vTexCoord;
in float vLight;
in float vFog;
in vec3 vFogColor;
in float vAlpha;
in float vCutout;
in vec3 vViewPos;        // view-space position (camera at origin)
in float vViewZ;
in float vRadialDist;
uniform PerFrame {
   mat4 uProjection;
   vec2 uFogRange;
   vec3 uDistanceFogColor;
   float uForceFog;
   vec3 uFogColor;
};
uniform sampler2D uModelTexture;
uniform float uTintByFogColor;
uniform float uNightStrength;    // world-only night lighting (0=day, 1=night)
uniform vec3 uCamFogColor;       // camera-in-fog envelope colour
uniform float uCamFogAmount;     // camera-in-fog envelope strength (0 = off)

// Flashlight beam and night-lighting integration; the shared block
// lives in flashlight.glsl and is expanded by the loader.
#include "flashlight.glsl"

uniform vec3 uNightPresentation = vec3(1.0, 0.0, 0.0);

// BEGIN NIGHT PRESENTATION -- shared by model.frag and ui.frag.
void ApplyNightPresentation(inout vec3 color, float brightness)
{
   if (uNightPresentation.z < 0.5) return;
   float gray = dot(color, vec3(0.299, 0.587, 0.114));
   color = mix(color, vec3(gray), uNightPresentation.y) * brightness;
}
// END NIGHT PRESENTATION

void main() {
   vec4 texColor = texture(uModelTexture, vTexCoord);
   if (vCutout > 0.5 && texColor.a <= 0.5) discard;
   vec3 litColor = texColor.rgb * vLight;
   // Phase 2.7: branch-less tint via mix (was if > 0.5).
   vec3 tinted = litColor * vFogColor;
   litColor = mix(litColor, tinted, uTintByFogColor);
   vec3 finalColor = mix(litColor, vFogColor, vFog);
   // Match the terrain/instanced-model horizon fade for legacy
   // model-path objects (BMP billboards and water-clipped meshes).
   // Radial distance (not forward-Z) so wide-FOV screen edges fog in
   // step with the radial CPU alpha fade -- see terrain.frag.
   float distanceFog = clamp((vRadialDist - uFogRange.x) / max(uFogRange.y - uFogRange.x, 1.0), 0.0, 1.0);
   finalColor = mix(finalColor, uDistanceFogColor, distanceFog);

   // camera-in-fog global envelope -- see terrain.frag.  Fogs the whole
   // scene by distance when the camera is submerged in a tall pocket-fog volume.
   if (uCamFogAmount > 0.001f) {
       // Near baseline so close objects are also hazed (sells "inside fog");
       // far objects still reach the full envelope amount.
       const float kNearFog = 0.25f;
       float camEnvDist = kNearFog + (1.0f - kNearFog) * (1.0f - exp(-2.5f * vViewZ / max(uFogRange.y, 1.0f)));
       float camFog = uCamFogAmount * camEnvDist;
       // Weapon phong/env-map overlays are additive passes drawn over a body
       // that already carries the envelope's fog-colour blend.  Adding the
       // fog colour a second time tinted the highlights twice as strongly as
       // the rest of the viewmodel (visible colour seam between specular and
       // diffuse weapon sections).  They attenuate toward black instead,
       // leaving the fog colour to the base pass -- mirroring why GLSky.cpp
       // zeroes the envelope for the sun, which is likewise added over an
       // already-fogged layer.  Base draws (uTintByFogColor=0) keep the
       // regular fog-colour blend.
       vec3 envelopeColor = mix(uCamFogColor, vec3(0.0f), uTintByFogColor);
       finalColor = finalColor * (1.0f - camFog) + envelopeColor * camFog;
   }

   // Grade night, restore local color/brightness, then apply the unchanged
   // warm beam contribution, OR grade held models once without beam geometry.
   if (uNightPresentation.z > 0.5) {
      ApplyNightPresentation(finalColor, uNightPresentation.x);
   } else {
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

   // RGB gain allows values above full opacity; visibility still uses vAlpha.
   FragColor = vec4(finalColor * uMoonBrightness, texColor.a * vAlpha);
}
