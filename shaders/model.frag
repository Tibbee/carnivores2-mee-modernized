#version 330 core
out vec4 FragColor;
in vec2 vTexCoord;
in float vLight;
in float vFog;
in vec3 vFogColor;
in float vAlpha;
in float vCutout;
in float vViewZ;
uniform PerFrame {
   mat4 uProjection;
   vec2 uFogRange;
   vec3 uDistanceFogColor;
   float uForceFog;
   vec3 uFogColor;
};
uniform sampler2D uModelTexture;
uniform float uTintByFogColor;
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
   float distanceFog = clamp((vViewZ - uFogRange.x) / max(uFogRange.y - uFogRange.x, 1.0), 0.0, 1.0);
   finalColor = mix(finalColor, uDistanceFogColor, distanceFog);
   FragColor = vec4(finalColor, texColor.a * vAlpha);
}
