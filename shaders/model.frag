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

// Flashlight accessory. uFlashlight = (intensity, range, cosInner, cosOuter).
// This block must stay byte-identical across terrain.frag, model.frag and
// instanced_model.frag: GLSL has no include mechanism here, and the three
// copies are the only thing keeping the beam consistent between the world,
// its water and the objects standing on it.
uniform vec4 uFlashlight;      // x = 0 when the light is off
uniform vec3 uFlashlightColor; // warm white tint

void ApplyFlashlight(inout vec3 color, float distance, float cosOffAxis)
{
   // Early out: the toggle is off for most frames, and the whole block is
   // four multiplies once active.
   if (uFlashlight.x <= 0.0) return;

   // Cone term. Because the camera sits at the origin in view space, the
   // cosine of the fragment's off-axis angle is simply depth / radial
   // distance -- no normal and no trig needed. This is what the reference
   // Reshade shader could not do: it only had a radial screen gradient, so
   // it lit everything at the centre of the screen regardless of range.
   float cone = smoothstep(uFlashlight.w, uFlashlight.z, cosOffAxis);
   if (cone <= 0.0) return;

   // Squared linear falloff, reaching zero exactly at the beam range.
   float atten = clamp(1.0 - distance / max(uFlashlight.y, 1.0), 0.0, 1.0);
   atten *= atten;

   float amount = uFlashlight.x * cone * atten;
   if (amount <= 0.0) return;

   // Headroom roll-off, cubed. A plain multiplicative boost pins every lit
   // channel at 1.0 and the surface's own shading collapses into flat paint --
   // the washed-out look. Scaling by how much headroom the pixel has left
   // keeps texture detail intact: near-black night ground still takes most of
   // the beam, near-white highlights take almost none. See Core/Flashlight.h.
   float luma = dot(color, vec3(0.299, 0.587, 0.114));
   float headroom = 1.0 - clamp(luma, 0.0, 1.0);
   headroom *= headroom * headroom;
   amount *= headroom;
   if (amount <= 0.0) return;

   vec3 beam = uFlashlightColor * amount;
   // Multiplicative boost tinted by the beam, so texture detail is preserved
   // rather than replaced...
   color *= (1.0 + beam);
   // ...plus the reference shader's "blend fix" floor: a pixel at exactly
   // black does not survive a multiply, so without this the beam vanishes on
   // unlit night ground. Kept small on purpose; a larger value is what makes
   // shadowed detail read as a wash.
   color = max(color, beam * 0.03);
}

// Night defaults; the matching engine pushes configured values per draw.
uniform vec4 uNightSettings = vec4(0.22, 0.85, 0.60, 1.0); // gain, desaturation, sky gain, normal-night treatment

// BEGIN NIGHT LIGHTING -- keep identical across the three world shaders.
void ApplyNightLighting(inout vec3 color, float distance, float cosOffAxis)
{
   if (uNightStrength <= 0.001) return;
   vec3 originalColor = color;
   float gray = dot(originalColor, vec3(0.299, 0.587, 0.114));
   color = mix(originalColor, vec3(gray), uNightSettings.y * uNightStrength);
   color *= mix(1.0, uNightSettings.x, uNightStrength);

   // Restore brightness AND color, fading with the existing beam geometry.
   // Night vision bypasses this world grade and its color restoration.
   if (uNightSettings.w > 0.5 && uFlashlight.x > 0.0) {
      float cone = smoothstep(uFlashlight.w, uFlashlight.z, cosOffAxis);
      float atten = clamp(1.0 - distance / max(uFlashlight.y, 1.0), 0.0, 1.0);
      atten *= atten;
      float restoration = clamp(uFlashlight.x * cone * atten, 0.0, 1.0);
      color = mix(color, originalColor, restoration);
   }
}
// END NIGHT LIGHTING

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
