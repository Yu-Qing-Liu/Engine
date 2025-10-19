#version 450

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec4 inColor;
layout(location = 2) in vec2 inUV;

layout(set = 0, binding = 0) uniform UBO {
    mat4 model;
    mat4 view;
    mat4 proj;
} u;

layout(set = 0, binding = 2) uniform Params {
    vec2 uvScale;
    vec2 uvOffset;
    vec4 color;
    int texIndex;
    float _pad[3];
} P;

layout(location = 0) out vec2 vUV;
layout(location = 1) out vec4 vColor;
layout(location = 2) flat out int vTexIndex;

void main() {
    vUV = inUV;
    vColor = inColor;
    vTexIndex = P.texIndex; // read once here; dynamically uniform within the draw
    gl_Position = u.proj * u.view * u.model * vec4(inPos, 1.0);
}
