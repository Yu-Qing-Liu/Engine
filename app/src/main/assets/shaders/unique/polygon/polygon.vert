#version 450

layout(location=0) in vec3 inPos;
layout(location=1) in vec4 inColor;
layout(location=2) in vec3 inBary;
layout(location=3) in vec3 inEdgeMask;

layout(binding=0) uniform UBO {
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

layout(location=0) out vec4 vColor;
layout(location=1) out vec3 vBary;
layout(location=2) out vec3 vEdgeMask;

void main() {
    gl_Position = ubo.proj * ubo.view * ubo.model * vec4(inPos, 1.0);
    vColor     = inColor;
    vBary      = inBary;
    vEdgeMask  = inEdgeMask; // which triangle edges are “hard”
}

