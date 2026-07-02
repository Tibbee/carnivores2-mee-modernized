#version 330 core
out vec2 vNdc;
const vec2 kPositions[3] = vec2[3](
   vec2(-1.0, -1.0),
   vec2( 3.0, -1.0),
   vec2(-1.0,  3.0)
);
void main() {
   vec2 pos = kPositions[gl_VertexID];
   vNdc = pos;
   gl_Position = vec4(pos, 0.0, 1.0);
}
