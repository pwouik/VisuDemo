#version 460 core
out vec2 tex_coord;
void main() {
    vec2 pos = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2) * 2.0 - 1.0;
    tex_coord = pos * 0.5 + 0.5; // Map to [0,1]
    gl_Position = vec4(pos, 0.0, 1.0);
}