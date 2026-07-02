#version 330 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aTexCoord;
out vec2 vTexCoord;
void main() {
   gl_Position = vec4(aPos, 0.0, 1.0);
   // GDI lpVideoBuf is top-down; GL textures are bottom-up.
   // Flip by inverting the v-coordinate.
   vTexCoord = vec2(aTexCoord.x, 1.0 - aTexCoord.y);
}
