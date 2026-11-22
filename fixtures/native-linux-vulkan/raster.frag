#version 450
// Original known color for exact native Vulkan RGBA8 readback oracle.
layout(location = 0) out vec4 color;
void main() { color = vec4(0.25, 0.50, 1.00, 1.00); }
