#version 450

// Per-vertex
layout(location = 0) in vec3 inPos;
layout(location = 1) in vec4 inColor;
layout(location = 2) in vec3 inBary;
layout(location = 3) in vec3 inEdgeMask;

// Per-instance (binding 1)
layout(location = 4) in vec4 iM0; // model row 0
layout(location = 5) in vec4 iM1; // model row 1
layout(location = 6) in vec4 iM2; // model row 2
layout(location = 7) in vec4 iM3; // model row 3
layout(location = 8) in vec4 iColor; // instance fill color multiplier
layout(location = 9) in vec4 iOutlineColor; // instance outline color
layout(location = 10) in float iOutlineWidth;

// Global camera UBO
layout(set = 0, binding = 0) uniform VP {
    mat4 view;
    mat4 proj;
    uint billboard;
} cam;

layout(location = 0) out vec4 vColor;
layout(location = 1) out vec3 vBary;
layout(location = 2) out vec3 vEdgeMask;

// per-instance to fragment (flat = no interpolation)
layout(location = 3) flat out vec4 vIColor;
layout(location = 4) flat out vec4 vIOutlineColor;
layout(location = 5) flat out float vIOutlineWidth;

void main() {
    mat4 M = mat4(iM0, iM1, iM2, iM3);

    vColor = inColor;
    vBary = inBary;
    vEdgeMask = inEdgeMask;

    vIColor = iColor;
    vIOutlineColor = iOutlineColor;
    vIOutlineWidth = iOutlineWidth;

    // Normal world / clip position
    vec4 world = M * vec4(inPos, 1.0);
    vec4 clipPos = cam.proj * cam.view * world;

    if (cam.billboard != 0u) {
        vec4 centerWorld = M * vec4(0.0, 0.0, 0.0, 1.0);
        vec4 centerView = cam.view * centerWorld;

        // camera-space right/up; for no camera roll you can use world X/Y
        vec3 right = vec3(1.0, 0.0, 0.0);
        vec3 up = vec3(0.0, 1.0, 0.0);

        float sx = length(M[0].xyz);
        float sy = length(M[1].xyz);
        const vec2 baseSize = vec2(0.5, 0.5);
        vec2 size = baseSize * vec2(sx, sy);

        // Shift in view space
        vec4 viewPos = centerView
                + vec4(right * (inPos.x * size.x)
                        + up * (inPos.y * size.y), 0.0);

        // Then project normally
        gl_Position = cam.proj * viewPos;
    } else {
        gl_Position = clipPos;
    }
}
