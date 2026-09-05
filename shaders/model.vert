#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoord;
layout (location = 2) in vec4 aLightFogAlphaCutout;
layout (location = 3) in vec3 aFogColor;
uniform PerFrame {
   mat4 uProjection;
   vec2 uFogRange;
   vec3 uDistanceFogColor;
   float uForceFog;
   vec3 uFogColor;
};
out vec2 vTexCoord;
out float vLight;
out float vFog;
out vec3 vFogColor;
out float vAlpha;
out float vCutout;
out float vViewZ;
out float vRadialDist;   // radial camera distance; drives distance fog (see terrain.vert)
void main() {
   gl_Position = uProjection * vec4(aPos, 1.0);
   vTexCoord = aTexCoord;
   // uint8 attributes are normalized to [0,1] by the driver.
   vLight  = aLightFogAlphaCutout.x;
   vFog    = aLightFogAlphaCutout.y;
   vAlpha  = aLightFogAlphaCutout.z;
   vCutout = aLightFogAlphaCutout.w;
   vFogColor = aFogColor;
   vViewZ = max(-aPos.z, 0.0);
   vRadialDist = length(aPos);
}
