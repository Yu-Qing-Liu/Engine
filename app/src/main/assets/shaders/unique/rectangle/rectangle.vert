#version 450

layout(location = 0) in vec3 inPos;

layout(binding = 0) uniform UBO {
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

layout(location = 0) out vec2 vLocal;

void main() {
    vLocal = inPos.xy; // object-space for SDF
    gl_Position = ubo.proj * ubo.view * ubo.model * vec4(inPos, 1.0);
}
