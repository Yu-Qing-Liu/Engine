#version 450

layout(location = 0) in vec3 vWorldPos;
layout(location = 1) in vec3 vNrmW;
layout(location = 2) in vec2 vUV;
layout(location = 3) in vec3 vTanW;
layout(location = 4) in float vTanSign;
layout(location = 5) in vec4 vCol;
layout(location = 6) flat in uint vMatId;

layout(location = 0) out vec4 outColor;

// We reuse the camera matrices to recover camera position.
layout(set = 0, binding = 0) uniform UBO {
    mat4 model;
    mat4 view;
    mat4 proj;
} u;

// Descriptor set = 1
//  - binding 0: the big texture array (size must equal OBJMODEL_MAX_TEXTURES)
//  - binding 1: std430 SSBO with materials

layout(set = 1, binding = 0) uniform sampler2D uTex[64];

struct MaterialGPU {
    // 9 texture indices
    int baseColor;
    int normal;
    int roughness;
    int metallic;
    int specular;
    int ao;
    int emissive;
    int opacity;
    int displacement;

    // pad to 16B boundary
    int _padA0;
    int _padA1;
    int _padA2;

    // factors
    vec4 baseColorFactor;           // rgba
    vec4 emissiveFactor_roughness;  // xyz = emissive, w = roughness
    vec4 metallic_flags_pad;        // x = metallic, y = flags (floatBits), z/w pad
};

layout(std430, set = 1, binding = 1) readonly buffer Materials {
    MaterialGPU m[];
};

const float PI = 3.14159265359;

// Safe sampler helpers (index 0 maps to your WHITE dummy)
// We treat idx==0 as "no texture" and provide sensible defaults without sampling where possible.
vec4 sampleRGBA(int idx, vec2 uv, vec4 fallback) {
    return (idx > 0) ? texture(uTex[idx], uv) : fallback;
}
float sampleR(int idx, vec2 uv, float fallback) {
    return (idx > 0) ? texture(uTex[idx], uv).r : fallback;
}
vec3 sampleRGB(int idx, vec2 uv, vec3 fallback) {
    return (idx > 0) ? texture(uTex[idx], uv).rgb : fallback;
}

vec3 applyNormalMap(vec3 N, vec3 T, float signB, int texIdx, vec2 uv) {
    if (texIdx <= 0) return normalize(N);
    vec3 mapN = texture(uTex[texIdx], uv).xyz * 2.0 - 1.0; // tangent space
    vec3 B = normalize(cross(N, T)) * signB;
    mat3 TBN = mat3(normalize(T), B, normalize(N));
    return normalize(TBN * mapN);
}

// Minimal Cook-Torrance GGX for a single directional light.
float D_GGX(float NoH, float a) {
    float a2 = a * a;
    float d = (NoH * NoH) * (a2 - 1.0) + 1.0;
    return a2 / (PI * d * d + 1e-7);
}

float G_SchlickGGX(float NoV, float k) {
    return NoV / (NoV * (1.0 - k) + k + 1e-7);
}

float G_Smith(float NoV, float NoL, float a) {
    // UE4-style k
    float k = (a + 1.0);
    k = (k * k) / 8.0;
    return G_SchlickGGX(NoV, k) * G_SchlickGGX(NoL, k);
}

vec3 F_Schlick(vec3 F0, float HoV) {
    float f = pow(1.0 - HoV, 5.0);
    return F0 + (1.0 - F0) * f;
}

void main() {
    MaterialGPU mat = m[vMatId];

    // Base color (albedo) and alpha
    vec4 baseTex = sampleRGBA(mat.baseColor, vUV, vec4(1.0));
    vec4 base = baseTex * mat.baseColorFactor * vCol;
    float opacityTex = sampleR(mat.opacity, vUV, 1.0);
    float alpha = clamp(base.a * opacityTex, 0.0, 1.0);

    // Alpha test for cutouts (tweak threshold as needed)
    if (alpha < 0.05) discard;

    // Normal
    vec3 N = applyNormalMap(normalize(vNrmW), normalize(vTanW), vTanSign, mat.normal, vUV);

    // Roughness & Metallic (with sensible defaults)
    float roughness = clamp(sampleR(mat.roughness, vUV, mat.emissiveFactor_roughness.w), 0.04, 1.0);
    float metallic  = clamp(sampleR(mat.metallic,  vUV, mat.metallic_flags_pad.x   ), 0.0, 1.0);

    // AO
    float ao = clamp(sampleR(mat.ao, vUV, 1.0), 0.0, 1.0);

    // Emissive
    vec3 emissive = mat.emissiveFactor_roughness.xyz * sampleRGB(mat.emissive, vUV, vec3(1.0));

    // Camera position from inverse view
    mat4 invV = inverse(u.view);
    vec3 camPos = vec3(invV[3]);
    vec3 V = normalize(camPos - vWorldPos);

    // Simple single directional light
    vec3 L = normalize(vec3(0.4, 1.0, 0.3));
    vec3 H = normalize(V + L);

    float NoL = max(dot(N, L), 0.0);
    float NoV = max(dot(N, V), 0.0);
    float NoH = max(dot(N, H), 0.0);
    float HoV = max(dot(H, V), 0.0);

    // Fresnel at normal incidence
    vec3 albedo = base.rgb;
    vec3 F0 = mix(vec3(0.04), albedo, metallic);

    float a = roughness * roughness;
    float D = D_GGX(NoH, a);
    float G = G_Smith(NoV, NoL, a);
    vec3  F = F_Schlick(F0, HoV);

    vec3 spec = (D * G) * F / max(4.0 * NoL * NoV, 1e-5);
    vec3 kS   = F;
    vec3 kD   = (vec3(1.0) - kS) * (1.0 - metallic);

    vec3 direct = (kD * albedo / PI + spec) * NoL;

    // Very simple ambient term modulated by AO
    vec3 ambient = albedo * 0.03 * ao;

    vec3 color = ambient + direct + emissive;

    outColor = vec4(color, alpha);
}

