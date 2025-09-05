#version 450

layout(set = 0, binding = 0) uniform UBO {
    mat4 model;
    mat4 view;
    mat4 proj;
} u;

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inNrm;
layout(location = 2) in vec4 inCol;
layout(location = 3) in vec2 inUV;
layout(location = 4) in vec4 inTanSgn; // xyz=tangent, w=bitangent sign
layout(location = 5) in uint inMaterialId;

layout(location = 0) out vec3 vWorldPos;
layout(location = 1) out vec3 vNrmW;
layout(location = 2) out vec2 vUV;
layout(location = 3) out vec3 vTanW;
layout(location = 4) out float vTanSign;
layout(location = 5) out vec4 vCol;
layout(location = 6) flat out uint vMatId;

void main() {
    // World position
    vec4 wp = u.model * vec4(inPos, 1.0);
    vWorldPos = wp.xyz;

    // Proper normal/tangent transform (handles non-uniform scale)
    mat3 nrmMat = transpose(inverse(mat3(u.model)));
    vec3 N = normalize(nrmMat * inNrm);
    vec3 T = normalize(nrmMat * inTanSgn.xyz);

    vNrmW = N;
    vTanW = T;
    vTanSign = inTanSgn.w;

    vUV = inUV;
    vCol = inCol;
    vMatId = inMaterialId;

    gl_Position = u.proj * u.view * wp;
}

