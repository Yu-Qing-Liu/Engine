#version 450

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec2 inUV;

// Instance attributes (binding = 1)
layout(location = 2) in mat4 iModel; // uses 2,3,4,5
layout(location = 6) in uint iFrame; // R32_UINT
layout(location = 7) in uint iCover; // R32_UINT
layout(location = 8) in vec2 iUVScale; // R32G32_SFLOAT
layout(location = 9) in vec2 iUVOff; // R32G32_SFLOAT

// View/Proj (set = 0, binding = 0)
layout(set = 0, binding = 0) uniform VP {
    mat4 view;
    mat4 proj;
    uint billboard;
} cam;

layout(location = 0) out vec2 vUV;
layout(location = 1) flat out uint vFrame;
layout(location = 2) flat out uint vCover;

void main() {
    // Switch based on iCover
    if (iCover == 0u) {
        vUV = inUV * iUVScale + iUVOff;
    } else {
        vUV = inUV;
    }

    vFrame = iFrame;
    vCover = iCover;

    if (cam.billboard != 0u) {
        // 1) Billboard center = instance origin (local (0,0,0))
        vec4 centerWorld = iModel * vec4(0.0, 0.0, 0.0, 1.0);
        vec4 centerClip = cam.proj * cam.view * centerWorld;

        // 2) Perspective divide -> NDC [-1,1]
        centerClip /= centerClip.w;

        // 3) Offset in NDC using quad vertex (assumed in [-0.5,0.5])
        //    This is the *screen-space* size of the quad.
        const vec2 billboardSizeNDC = vec2(0.2, 0.2); // tweak or make a uniform
        centerClip.xy += inPos.xy * billboardSizeNDC;

        // 4) Write final position (still NDC; w==1 after the division above)
        gl_Position = centerClip;
    } else {
        // Normal 3D mesh path
        vec4 world = iModel * vec4(inPos, 1.0);
        gl_Position = cam.proj * cam.view * world;
    }
}
