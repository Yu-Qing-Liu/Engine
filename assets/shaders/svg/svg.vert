#version 460

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec2 inUV;

// Instance attributes (binding = 1)
layout(location = 2) in mat4 inModel;
layout(location = 6) in uint inFrameIndex;
layout(location = 7) in vec4 inColor;
layout(location = 8) in vec2 inUVScale;
layout(location = 9) in vec2 inUVOffset;

// View/projection uniforms (same as image shaders)
layout(set = 0, binding = 0) uniform VP {
    mat4 view;
    mat4 proj;
    uint billboard;
} vp;

// Outputs to frag
layout(location = 0) out vec2 vUV;
layout(location = 1) flat out uint vFrameIndex;
layout(location = 2) out vec4 vColor;

void main()
{
    // Apply cropping/scaling from CPU
    vUV = inUV * inUVScale + inUVOffset;
    vFrameIndex = inFrameIndex;
    vColor = inColor;

    // Billboard support, same pattern as image shader
    vec4 world = inModel * vec4(inPos, 1.0);

    if (vp.billboard != 0u) {
        // simple billboard (optional)
    }

    gl_Position = vp.proj * vp.view * world;
}
