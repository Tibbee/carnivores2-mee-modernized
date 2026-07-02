#version 330 core
in vec2 vTexCoord;
out vec4 FragColor;
uniform sampler2D uTexture;
void main() {
   vec3 rgb555 = texture(uTexture, vTexCoord).rgb;
   // Pixel value 0 = transparent (HUD background).
   if (dot(rgb555, vec3(1.0)) < 0.01) discard;
   FragColor = vec4(rgb555, 1.0);
}
