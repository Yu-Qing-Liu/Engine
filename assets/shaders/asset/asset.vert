#version 450

// per-vertex
layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec4 inColor;
layout(location = 3) in vec2 inUV;
layout(location = 4) in vec4 inTanSgn;
layout(location = 5) in uint inMatId;
layout(location = 10) in uvec4 inBoneIds; // NEW
layout(location = 11) in vec4 inWeights; // NEW

// per-instance
layout(location = 6) in vec4 inModel0;
layout(location = 7) in vec4 inModel1;
layout(location = 8) in vec4 inModel2;
layout(location = 9) in vec4 inModel3;
layout(location = 12) in uint inBonesBase; // NEW: base index into SSBO
layout(location = 13) in vec4 inOutlineColor;
layout(location = 14) in float inOutlineWidth;

// set=0: camera (from Model)
layout(std140, set = 0, binding = 0) uniform CameraUBO {
    mat4 uView;
    mat4 uProj;
    uint billboard;
} cam;

// set=1: all instance palettes in one big SSBO
layout(std430, set = 1, binding = 0) readonly buffer BonesSSBO {
    mat4 uBones[];
};

layout(location = 0) out vec3 vNormal;
layout(location = 1) out vec2 vUV;
layout(location = 2) out vec4 vColor;
layout(location = 3) out vec4 vOutlineColor;
layout(location = 4) out float vOutlineWidth;

void main() {
    mat4 model = mat4(inModel0, inModel1, inModel2, inModel3);

    // Resolve per-vertex bone matrices from SSBO using per-instance base
    uint b0 = inBonesBase + inBoneIds.x;
    uint b1 = inBonesBase + inBoneIds.y;
    uint b2 = inBonesBase + inBoneIds.z;
    uint b3 = inBonesBase + inBoneIds.w;

    mat4 skin =
        uBones[b0] * inWeights.x
            + uBones[b1] * inWeights.y
            + uBones[b2] * inWeights.z
            + uBones[b3] * inWeights.w;

    vec4 skinnedPos = skin * vec4(inPos, 1.0);
    vec3 skinnedNormal = normalize(mat3(skin) * inNormal);

    vec4 worldPos = model * skinnedPos;

    vNormal = normalize(mat3(model) * skinnedNormal);
    vUV = inUV;
    vColor = inColor;
    vOutlineColor = inOutlineColor;
    vOutlineWidth = inOutlineWidth;

    gl_Position = cam.uProj * cam.uView * worldPos;
}
