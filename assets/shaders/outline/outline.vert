#version 450

// per-vertex
layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec4 inColor;
layout(location = 3) in vec2 inUV;
layout(location = 4) in vec4 inTanSgn;
layout(location = 5) in uint inMatId;
layout(location = 10) in uvec4 inBoneIds;
layout(location = 11) in vec4 inWeights;

// per-instance
layout(location = 6)  in vec4 inModel0;
layout(location = 7)  in vec4 inModel1;
layout(location = 8)  in vec4 inModel2;
layout(location = 9)  in vec4 inModel3;
layout(location = 12) in uint inBonesBase;
layout(location = 13) in vec4 inOutlineColor;
layout(location = 14) in float inOutlineWidth;

// set=0: camera
layout(std140, set = 0, binding = 0) uniform CameraUBO {
    mat4 uView;
    mat4 uProj;
    uint billboard;
} cam;

// set=1: bones
layout(std430, set = 1, binding = 0) readonly buffer BonesSSBO {
    mat4 uBones[];
};

layout(location = 0) out vec4 vOutlineColor;

void main() {
    // Instance model matrix
    mat4 model = mat4(inModel0, inModel1, inModel2, inModel3);

    // ---- Skinning (same as main pass) ----
    uint b0 = inBonesBase + inBoneIds.x;
    uint b1 = inBonesBase + inBoneIds.y;
    uint b2 = inBonesBase + inBoneIds.z;
    uint b3 = inBonesBase + inBoneIds.w;

    mat4 skin =
        uBones[b0] * inWeights.x +
        uBones[b1] * inWeights.y +
        uBones[b2] * inWeights.z +
        uBones[b3] * inWeights.w;

    vec4 skinnedPos    = skin * vec4(inPos, 1.0);
    vec3 skinnedNormal = normalize(mat3(skin) * inNormal);

    vec4 worldPos    = model * skinnedPos;
    vec3 worldNormal = normalize(mat3(model) * skinnedNormal);

    // ---- View space ----
    vec4 viewPos    = cam.uView * worldPos;
    vec3 viewNormal = normalize(mat3(cam.uView) * worldNormal);

    // ---- Outline width handling ----
    // Treat inOutlineWidth as a small “thickness” factor (0.0–1.0-ish).
    // Also scale by -viewPos.z so distant objects keep similar pixel width.
    float baseThickness = inOutlineWidth;          // e.g. 0.02
    float depthScale    = clamp(-viewPos.z, 0.1, 100.0);
    float thickness     = baseThickness * depthScale * 0.001; // tune 0.01

    // Push *back-facing* hull outwards.
    viewPos.xyz += viewNormal * thickness;

    gl_Position   = cam.uProj * viewPos;
    vOutlineColor = inOutlineColor;
}

