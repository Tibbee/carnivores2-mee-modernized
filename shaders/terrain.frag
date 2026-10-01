#version 330 core
out vec4 FragColor;
in vec2 vTexCoord;
flat in int vLayer;
in float vLight;
in float vFog;
in vec3 vFogColor;
in float vAlpha;
in float vViewZ;
in float vViewDistance;
in float vRadialDist;      // radial camera distance (see terrain.vert)
in float vWaterAlphaFade;
in vec3 vViewPos;            // view-space position (camera at origin)
uniform vec3 uSunDirection;     // sun direction in view space
uniform float uSunVisibility;    // 0..1 sun visibility
uniform float uFogScatter;       // master scatter strength
uniform float uNightStrength;     // world-only night lighting (0=day, 1=night)
uniform vec3 uCamFogColor;       // camera-in-fog envelope colour
uniform float uCamFogAmount;     // camera-in-fog envelope strength (0 = off)
uniform PerFrame {
   mat4 uProjection;
   vec2 uFogRange;          // (fadeStart, distance)
   vec3 uDistanceFogColor;
   float uForceFog;
   vec3 uFogColor;
   mat4 uView;
   vec4 uWaterAlphaFade;    // x=start, y=end, z=enabled, w=fade step
   float uWaterDepthFactor; // 0 at surface, 1 at max depth
   float uCloudCover;       // 0=clear sun, 1=overcast (cloud colour temp)
};
uniform sampler2DArray uTerrainArray;

// Flashlight accessory. uFlashlight = (intensity, range, cosInner, cosOuter)
// -- half-angle cosines measured off the view axis; see Core/Flashlight.h.
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

// Classic initializer also permits these shaders with an older executable.
uniform vec4 uNightSettings = vec4(0.5, 0.6, 1.0, 0.0); // gain, desaturation, sky gain, Moonlit

// BEGIN NIGHT LIGHTING -- keep identical across the three world shaders.
void ApplyNightLighting(inout vec3 color, float distance, float cosOffAxis)
{
   if (uNightStrength <= 0.001) return;
   vec3 originalColor = color;
   float gray = dot(originalColor, vec3(0.299, 0.587, 0.114));
   color = mix(originalColor, vec3(gray), uNightSettings.y * uNightStrength);
   color *= mix(1.0, uNightSettings.x, uNightStrength);

   // Restore brightness AND color, fading with the existing beam geometry.
   // Classic mode and NV do not change the previous flashlight composition.
   if (uNightSettings.w > 0.5 && uFlashlight.x > 0.0) {
      float cone = smoothstep(uFlashlight.w, uFlashlight.z, cosOffAxis);
      float atten = clamp(1.0 - distance / max(uFlashlight.y, 1.0), 0.0, 1.0);
      atten *= atten;
      float restoration = clamp(uFlashlight.x * cone * atten, 0.0, 1.0);
      color = mix(color, originalColor, restoration);
   }
}
// END NIGHT LIGHTING

void main() {
   vec4 texColor = texture(uTerrainArray, vec3(vTexCoord, float(vLayer)));
   if (texColor.a < 0.05) discard;
   vec3 litColor = texColor.rgb * vLight;

   // Cloud colour temperature.  When the sun is obscured (overcast),
   // the remaining light is cooler/bluer skylight, so shift the terrain tint
   // toward blue in cloud shadow.  uCloudCover (0=clear sun, 1=overcast) is
   // derived from sun visibility in UpdatePerFrameUBO().
   vec3 cloudTint = mix(vec3(1.0), vec3(0.92, 0.95, 1.05), uCloudCover * 0.3);
   litColor *= cloudTint;

   // Water-colour-aware wavelength attenuation on terrain underwater.
   // vWaterAlphaFade > 0.5 identifies water surface vertices (which go
   // through the water fade path); terrain vertices have vWaterAlphaFade < 0.5.
   if (uWaterDepthFactor > 0.01 && vWaterAlphaFade < 0.5) {
       float depth = uWaterDepthFactor;

       // The water body's own colour tells us which wavelengths it transmits:
       // uDistanceFogColor is the (depth-modulated) water fog colour, whose
       // dominant channel(s) reveal the water's hue.  We attenuate the
       // NON-dominant channels more, letting the water's own tint survive --
       // so blue ocean keeps blue while brown swamp water keeps its brown.
       // The old fixed "red/green lost, blue kept" curve wrongly turned swamp
       // water blue at depth.  Mirrors ModulateWaterColorByDepth() in C++.
       vec3 waterTint = uDistanceFogColor;
       float maxc  = max(waterTint.r, max(waterTint.g, waterTint.b));
       const float kFloor = 0.40;
       const float kScale = 2.20;
       float invMax = 1.0 / max(maxc, 0.001);
       float kR = kFloor + kScale * (1.0 - waterTint.r * invMax);
       float kG = kFloor + kScale * (1.0 - waterTint.g * invMax);
       float kB = kFloor + kScale * (1.0 - waterTint.b * invMax);
       litColor *= vec3(exp(-depth * kR), exp(-depth * kG), exp(-depth * kB));

       // Avoid full black -- retain a trace of every channel.
       litColor = max(litColor, vec3(0.01));
   }

   // Per-vertex volumetric fog (volume-specific color and amount).
   vec3 volumetricFogColor = mix(litColor, vFogColor, vFog);
   // per-pixel sun-fog forward-scatter glow.  The warm glow only
   // appears where there is fog (vFog) and only when looking toward the
   // sun (view ray aligned with the view-space sun direction).  Evaluating
   // per fragment avoids the blocky per-tile approximation used before.
   if (uFogScatter > 0.0 && vFog > 0.01) {
       vec3 viewDir = vViewPos / max(length(vViewPos), 1e-3f);  // camera at origin in view space; guard zero-length
       float scatter = pow(max(0.0, dot(viewDir, uSunDirection)), 8.0);
       vec3 warmGlow = vec3(1.0, 0.85, 0.5) * scatter * uSunVisibility * uFogScatter * vFog;
       volumetricFogColor += warmGlow;
   }
   // Per-pixel distance fog: smooth ramp from uFogRange.x to
   // uFogRange.y. Uses the global horizon color instead of the
   // per-vertex vFogColor, which prevents local fog volumes from
   // bleeding into the horizon fade. Uses the RADIAL camera distance
   // (not forward-only vViewZ) so the colour ramp stays aligned with
   // the CPU alpha fade (CalcTerrainAlpha), which is also radial.
   // With forward-Z here, wide-FOV screen edges (large off-axis angle,
   // forward depth << radial distance) stayed unfogged until the radial
   // alpha cull cut them, exposing the view-distance boundary as a hard
   // terrain edge (visible at max FOV). Radial fog paints the boundary
   // sky-coloured before the alpha cull removes it, hiding the edge.
   float distanceFog = clamp((vRadialDist - uFogRange.x) / max(uFogRange.y - uFogRange.x, 1.0), 0.0, 1.0);
   vec3 finalColor = mix(volumetricFogColor, uDistanceFogColor, distanceFog);

   // camera-in-fog global envelope.  When the camera is submerged in a
   // tall pocket-fog volume, fog the whole scene by distance so the world
   // reads as enveloped (not just geometry that sits inside the volume).  The
   // sky horizon is fogged separately.  uCamFogAmount is 0 when the
   // camera is in a shallow foot-level puddle, so it self-disables there.
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

   float waterAlphaFade = 1.0;
   if (vWaterAlphaFade > 0.5 && uWaterAlphaFade.z > 0.5) {
      // vViewDistance is now squared distance; compute sqrt
      // only for water pixels to recover the linear distance.
      float distance = sqrt(vViewDistance);
      float zz = distance - uWaterAlphaFade.y;
      if (zz > 0.0) {
         waterAlphaFade = clamp((255.0 - zz / max(uWaterAlphaFade.w, 1.0)) / 255.0, 0.0, 1.0);
      }
   }
   FragColor = vec4(finalColor, texColor.a * vAlpha * waterAlphaFade);
}
