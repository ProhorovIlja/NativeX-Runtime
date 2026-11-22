#version 450
// Original source-built gl_VertexIndex triangle; no external graphics content.
vec2 positions[3] = vec2[3](vec2(-0.9, -0.9), vec2(0.0, 0.9), vec2(0.9, -0.9));
void main() { gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0); }
