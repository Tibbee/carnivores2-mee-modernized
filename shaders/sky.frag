#version 330 core
in vec2 vNdc;
out vec4 FragColor;
uniform PerFrame {
   mat4 uProjection;
   vec2 uFogRange;
   vec3 uDistanceFogColor;
   float uForceFog;
   vec3 uFogColor;
};
uniform sampler2D uSkyTexture;
uniform vec2 uViewport;
uniform vec2 uVideoCenter;
uniform vec3 uQ;
uniform vec3 uP;
uniform vec3 uR;
uniform float uSkyTime;
uniform float uFogBase;
uniform float uUnderwaterDepth;
uniform float uWaterLineY;
void main() {
   vec2 pixel = vec2((vNdc.x * 0.5 + 0.5) * uViewport.x,
                     (1.0 - (vNdc.y * 0.5 + 0.5)) * uViewport.y);
   float sx = pixel.x - uVideoCenter.x;
   float sy = uVideoCenter.y - pixel.y;
   float sxQ = uQ.x * sx + uQ.y * sy + uQ.z;
   float q = sign(sxQ) * max(abs(sxQ), 0.001);
   float skyU = (uP.x * sx + uP.y * sy + uP.z) / q;
   float skyV = (uR.x * sx + uR.y * sy + uR.z) / q;
   float leftQ = uQ.x * (-uVideoCenter.x) + uQ.y * sy + uQ.z;
   float rightQ = uQ.x * uVideoCenter.x + uQ.y * sy + uQ.z;
   float leftU = (uP.x * (-uVideoCenter.x) + uP.y * sy + uP.z) / max(abs(leftQ), 0.001);
   float leftV = (uR.x * (-uVideoCenter.x) + uR.y * sy + uR.z) / max(abs(leftQ), 0.001);
   float rightU = (uP.x * uVideoCenter.x + uP.y * sy + uP.z) / max(abs(rightQ), 0.001);
   float rightV = (uR.x * uVideoCenter.x + uR.y * sy + uR.z) / max(abs(rightQ), 0.001);
   float dx = rightU - leftU;
   float dy = rightV - leftV;
   float dt = sqrt(dx*dx + dy*dy) / 96.0 - 6.0;
   dt = clamp(dt, 0.0, 10.0);
   float fogFactor = clamp(max(dt * 225.0 / 10.0, uFogBase) / 255.0, 0.0, 1.0);
   fogFactor = clamp(fogFactor + uUnderwaterDepth * 0.55, 0.0, 1.0);
   float distToWaterLine = uWaterLineY - pixel.y;
   float fadeWidth = 32.0;
   if (distToWaterLine < fadeWidth && uWaterLineY < uViewport.y) {
       fogFactor = mix(1.0, fogFactor, clamp(distToWaterLine / fadeWidth, 0.0, 1.0));
   }
   vec2 uv = vec2((skyU + uSkyTime) / 256.0, (skyV - uSkyTime) / 256.0);
   vec3 skyColor = texture(uSkyTexture, uv).rgb;

   // §3.1: Horizon-zenith gradient.  Real skies are not flat: the zenith
   // is darker and more saturated, while the horizon is lighter and warmer.
   // vNdc.y is -1 at the bottom and +1 at the top; treat y = 0 as the
   // horizon.  No new uniforms — derived from vNdc and the existing fog.
   float vert = max(0.0, vNdc.y);                 // 0 at horizon, 1 at zenith

   // Zenith darkening: 1.0 at the horizon, 0.92 at the zenith.
   float zenithDark = 0.92 + 0.08 * (1.0 - vert);
   skyColor *= zenithDark;

   // Warm horizon glow, Gaussian falloff upward; fog hides it.
   float horizonGlow = exp(-vert * vert * 20.0);
   vec3 glowColor = vec3(1.0, 0.85, 0.6);        // warm golden
   float glowStrength = 0.15 * (1.0 - fogFactor);
   skyColor += glowColor * horizonGlow * glowStrength;

   FragColor = vec4(mix(skyColor, uFogColor, fogFactor), 1.0);
}
