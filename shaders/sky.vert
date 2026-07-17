#version 330 core
out vec2 vNdc;
out vec3 vWorldDir;     // world-space view-ray direction (set from camera basis)
uniform vec3 uCamRight;   // camera right axis * tan(halfFovX)
uniform vec3 uCamUp;      // camera up axis * tan(halfFovY)
uniform vec3 uCamForward; // camera look direction (world space)
const vec2 kPositions[3] = vec2[3](
   vec2(-1.0, -1.0),
   vec2( 3.0, -1.0),
   vec2(-1.0,  3.0)
);
void main() {
   vec2 pos = kPositions[gl_VertexID];
   vNdc = pos;
   // Build the (unnormalized) world-space view ray for this NDC point.
   // uCamForward is already the camera look axis (R^(-1)*(0,0,-1)), so it
   // must be added here. Its .y is the world elevation used by the
   // gradient / pocket-fog fade.
   vWorldDir = pos.x * uCamRight + pos.y * uCamUp + uCamForward;
   gl_Position = vec4(pos, 0.0, 1.0);
}
