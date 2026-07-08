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
in float vWaterAlphaFade;
uniform PerFrame {
   mat4 uProjection;
   vec2 uFogRange;          // (fadeStart, distance)
   vec3 uDistanceFogColor;
   float uForceFog;
   vec3 uFogColor;
   mat4 uView;
   vec4 uWaterAlphaFade;    // x=start, y=end, z=enabled, w=fade step
   float uWaterDepthFactor; // 0 at surface, 1 at max depth (§3.4)
};
uniform sampler2DArray uTerrainArray;
void main() {
   vec4 texColor = texture(uTerrainArray, vec3(vTexCoord, float(vLayer)));
   if (texColor.a < 0.05) discard;
   vec3 litColor = texColor.rgb * vLight;

   // §3.4: Water-colour-aware wavelength attenuation on terrain underwater.
   // vWaterAlphaFade > 0.5 identifies water surface vertices (which go
   // through the water fade path); terrain vertices have vWaterAlphaFade < 0.5.
   if (uWaterDepthFactor > 0.01 && vWaterAlphaFade < 0.5) {
       float depth = uWaterDepthFactor;

       // The water body's own colour tells us which wavelengths it transmits:
       // uDistanceFogColor is the (depth-modulated) water fog colour, whose
       // dominant channel(s) reveal the water's hue.  We attenuate the
       // NON-dominant channels more, letting the water's own tint survive —
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

       // Avoid full black — retain a trace of every channel.
       litColor = max(litColor, vec3(0.01));
   }

   // Per-vertex volumetric fog (volume-specific color and amount).
   vec3 volumetricFogColor = mix(litColor, vFogColor, vFog);
   // Per-pixel distance fog: smooth ramp from uFogRange.x to
   // uFogRange.y. Uses the global horizon color instead of the
   // per-vertex vFogColor, which prevents local fog volumes from
   // bleeding into the horizon fade.
   float distanceFog = clamp((vViewZ - uFogRange.x) / max(uFogRange.y - uFogRange.x, 1.0), 0.0, 1.0);
   vec3 finalColor = mix(volumetricFogColor, uDistanceFogColor, distanceFog);
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
