#version 450

layout(location = 0) in vec3 inPos;

// instance attributes (binding=1)
layout(location = 1) in vec4 iM0;
layout(location = 2) in vec4 iM1;
layout(location = 3) in vec4 iM2;
layout(location = 4) in vec4 iM3;
layout(location = 5) in vec4 iColor;
layout(location = 6) in vec4 iOutlineColor;
layout(location = 7) in float iOutlineWidth;
layout(location = 8) in float iBorderRadius;

layout(location = 11) out vec4 vTint;

layout(binding = 0) uniform UBO {
    mat4 model; // ignored
    mat4 view;
    mat4 proj;
} ubo;

layout(location = 0) out vec2 vLocal;
layout(location = 1) out vec4 vColor;
layout(location = 2) out vec4 vOutlineColor;
layout(location = 3) out float vOutlineWidth;
layout(location = 4) out float vBorderRadius;

void main() {
    mat4 model = mat4(iM0, iM1, iM2, iM3);
    vLocal = inPos.xy;
    vColor = iColor;
    vTint = iColor;
    vOutlineColor = iOutlineColor;
    vOutlineWidth = iOutlineWidth;
    vBorderRadius = iBorderRadius;

    gl_Position = ubo.proj * ubo.view * model * vec4(inPos, 1.0);
}
