#version 450

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec2 inUV;
layout(location = 2) in float inXNorm; // 0 at left edge, 1 at right edge
layout(location = 3) in uint inFlags; // bit0: selected, bit1: caretBefore, bit2: caretAfter
layout(location = 4) in float inQuadW; // quad width in pixels

layout(location = 0) out vec2 vUV;
layout(location = 1) out float vXNorm;
layout(location = 2) flat out uint vFlags;
layout(location = 3) flat out float vQuadW;

layout(set = 0, binding = 0) uniform UBO {
    mat4 model;
    mat4 view;
    mat4 proj;
} u;

void main() {
    vUV = inUV;
    vXNorm = inXNorm;
    vFlags = inFlags;
    vQuadW = inQuadW;
    gl_Position = u.proj * u.view * u.model * vec4(inPos, 1.0);
}
