#version 330 core
// Per-vertex from static mesh VBO (binding 0)
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoord;
layout (location = 3) in vec4 aVertexLight; // legacy VLight[0..3], in 0..255 light units
layout (location = 15) in float aCutout;    // per-face sfOpacity classification
// Per-instance from instance VBO (binding 1, divisor=1)
layout (location = 4) in vec4 aWorldCol0;
layout (location = 5) in vec4 aWorldCol1;
layout (location = 6) in vec4 aWorldCol2;
layout (location = 7) in vec4 aWorldCol3;
layout (location = 8) in vec4 aInstanceLight;
layout (location = 9) in vec4 aInstanceFlags;
layout (location = 10) in vec4 aGroundLightRow0; // normalized 4x4 VMap sample grid
layout (location = 11) in vec4 aGroundLightRow1;
layout (location = 12) in vec4 aGroundLightRow2;
layout (location = 13) in vec4 aGroundLightRow3;
layout (location = 14) in vec4 aGroundParams; // .x enabled, .yz object-centre offset from grid origin
uniform PerFrame {
   mat4 uProjection;
   vec2 uFogRange;
   vec3 uDistanceFogColor;
   float uForceFog;
   vec3 uFogColor;
   mat4 uView;              // Phase 2.4: view matrix (identity for now)
};
out vec2 vTexCoord;
out float vLight;
out float vViewZ;           // Phase 2.5: view-space Z for per-pixel fog
out float vRadialDist;      // radial camera distance; drives distance fog (see terrain.vert)
out vec3 vWorldNormal;      // Phase 2.5: face normal for directional light
out float vAlpha;
out float vCutout;
out float vTintByFog;
out vec3 vVolumetricFogColor; // Phase 2.6: per-instance pocket fog colour
out float vVolumetricFog;     // Phase 2.6: per-vertex pocket fog amount
void main() {
   mat4 iWorld = mat4(aWorldCol0, aWorldCol1, aWorldCol2, aWorldCol3);
   vec4 viewPos = uView * iWorld * vec4(aPos, 1.0);
   gl_Position = uProjection * viewPos;
   vTexCoord = aTexCoord;

   int orientation = clamp(int(aInstanceFlags.x), 0, 3);
   vCutout = aCutout;

   if (aGroundParams.x > 0.5) {
      // Exact GPU equivalent of CalcModelGroundLight/GetLandLt2 for models
      // spanning at most three 512-unit interpolation cells per axis.
      // Still-larger models are conservatively sent through the CPU path.
      vec2 rotatedXZ;
      if (orientation == 0) {
         rotatedXZ = vec2(aPos.x, aPos.z);
      } else if (orientation == 1) {
         rotatedXZ = vec2(aPos.z, -aPos.x);
      } else if (orientation == 2) {
         rotatedXZ = vec2(-aPos.x, -aPos.z);
      } else {
         rotatedXZ = vec2(-aPos.z, aPos.x);
      }
      vec2 gridUnits = floor(aGroundParams.yz + rotatedXZ);
      ivec2 cell = clamp(ivec2(floor(gridUnits / 512.0)), ivec2(0), ivec2(2));
      vec2 cellUV = (gridUnits - vec2(cell) * 512.0) / 512.0;
      vec4 upper = cell.y == 0 ? aGroundLightRow0
                 : cell.y == 1 ? aGroundLightRow1
                               : aGroundLightRow2;
      vec4 lower = cell.y == 0 ? aGroundLightRow1
                 : cell.y == 1 ? aGroundLightRow2
                               : aGroundLightRow3;
      float top = mix(upper[cell.x], upper[cell.x + 1], cellUV.x);
      float bottom = mix(lower[cell.x], lower[cell.x + 1], cellUV.x);
      vLight = clamp(mix(top, bottom, cellUV.y), 0.0, 1.0);
   } else {
      float vertexLight = orientation == 0 ? aVertexLight.x
                        : orientation == 1 ? aVertexLight.y
                        : orientation == 2 ? aVertexLight.z
                                           : aVertexLight.w;
      vLight = clamp(aInstanceLight.x + vertexLight / 255.0, 0.0, 1.0);
   }
   // Phase 2.x: tintByFog not used for instanced; slot .y is now fogGrad.
   vTintByFog = 0.0;
   vAlpha = aInstanceFlags.w;
   // Phase 2.5: vViewZ = view-space depth (positive in front of camera).
   // Matches the terrain shader's vViewZ = max(-aPos.z, 0.0).
   vViewZ = max(-viewPos.z, 0.0);
   vRadialDist = length(viewPos.xyz);
   // Phase 2.5: transform face normal for directional light
   vWorldNormal = mat3(iWorld) * aNormal;
   // Phase 2.x: 3DFX-style height-graded pocket fog.
   // fogGrad is the Y-gradient (dFog/dY), aInstanceFlags.z is fogBase.
   // Per-vertex fog = fogBase + modelSpaceY * fogGrad, clamped.
   float fogGrad = aInstanceFlags.y;
   float perVertexFog = aInstanceFlags.z + aPos.y * fogGrad;
   vVolumetricFog = clamp(perVertexFog, 0.0, 1.0);
   vVolumetricFogColor = aInstanceLight.yzw;
}
