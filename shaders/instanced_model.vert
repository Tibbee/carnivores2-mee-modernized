#version 330 core
// Per-vertex from static mesh VBO (binding 0)
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoord;
// Per-instance from instance VBO (binding 1, divisor=1)
layout (location = 4) in vec4 aWorldCol0;
layout (location = 5) in vec4 aWorldCol1;
layout (location = 6) in vec4 aWorldCol2;
layout (location = 7) in vec4 aWorldCol3;
layout (location = 8) in vec4 aInstanceLight;
layout (location = 9) in vec4 aInstanceFlags;
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
   vLight = aInstanceLight.x;
   vCutout = aInstanceFlags.x;
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
