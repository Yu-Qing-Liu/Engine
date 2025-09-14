#version 450

// Per-frame UBO (view/proj used; model kept for compatibility)
layout(set = 0, binding = 0) uniform UBO {
    mat4 model; // unused when instancing
    mat4 view;
    mat4 proj;
} u;

// Per-vertex attributes (binding = 0)
layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inNrm;
layout(location = 2) in vec4 inCol;
layout(location = 3) in vec2 inUV;
layout(location = 4) in vec4 inTanSgn; // xyz = tangent, w = bitangent sign
layout(location = 5) in uint inMaterialId;

// Per-instance transform (binding = 1): mat4 as 4x vec4 columns
layout(location = 6) in vec4 iM0;
layout(location = 7) in vec4 iM1;
layout(location = 8) in vec4 iM2;
layout(location = 9) in vec4 iM3;

// Varyings to fragment shader (match your FS)
layout(location = 0) out vec3 vWorldPos;
layout(location = 1) out vec3 vNrmW;
layout(location = 2) out vec2 vUV;
layout(location = 3) out vec3 vTanW;
layout(location = 4) out float vTanSign;
layout(location = 5) out vec4 vCol;
layout(location = 6) flat out uint vMatId;

void main() {
    // Build the per-instance model matrix
    mat4 model = mat4(iM0, iM1, iM2, iM3);

    // World position
    vec4 wp = model * vec4(inPos, 1.0);
    vWorldPos = wp.xyz;

    // Proper normal/tangent transform (handles non-uniform scale)
    mat3 nrmMat = transpose(inverse(mat3(model)));
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
