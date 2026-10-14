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

// Flashlight beam and night-lighting integration; the shared block
// lives in flashlight.glsl and is expanded by the loader.
#include "flashlight.glsl"

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
