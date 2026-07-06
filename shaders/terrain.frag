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

   // §3.4: Wavelength attenuation — only on terrain vertices underwater.
   // vWaterAlphaFade > 0.5 identifies water surface vertices (which go
   // through the water fade path); terrain vertices have vWaterAlphaFade < 0.5.
   if (uWaterDepthFactor > 0.01 && vWaterAlphaFade < 0.5) {
       float depth = uWaterDepthFactor;
       // Red absorbed fastest, green medium, blue barely attenuated
       float redLoss   = clamp(depth * 0.7, 0.0, 0.95);
       float greenLoss = clamp(depth * 0.4, 0.0, 0.70);
       litColor *= vec3(1.0 - redLoss, 1.0 - greenLoss, 1.0);
       // Avoid full black — retain trace channels
       litColor = max(litColor, vec3(0.01, 0.01, 0.02));
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
