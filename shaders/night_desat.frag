#version 330 core
in vec2 vTexCoord;
out vec4 FragColor;
uniform sampler2D uSceneTexture;
uniform float uDesaturateStrength;
void main() {
   vec3 color = texture(uSceneTexture, vTexCoord).rgb;
   float gray = dot(color, vec3(0.299, 0.587, 0.114));
   vec3 desaturated = mix(color, vec3(gray), uDesaturateStrength);
   FragColor = vec4(desaturated, 1.0);
}
