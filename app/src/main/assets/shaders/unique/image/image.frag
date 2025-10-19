#version 450

layout(location = 0) in vec2 vUV;
layout(location = 1) in vec4 vColor;
layout(location = 2) flat in int vTexIndex;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 1) uniform sampler2D uTex[16];

layout(set = 0, binding = 2) uniform Params {
    vec2 uvScale;
    vec2 uvOffset;
    vec4 color; // a==0 -> no override
    int texIndex;
    float _pad[3];
} P;

void main() {
    // FIT + center -> uv stays in [0,1]
    vec2 uv = vUV * P.uvScale + P.uvOffset;
    uv = clamp(uv, vec2(0.0), vec2(1.0));

    int idx = clamp(vTexIndex, 0, 127);
    vec4 texel = texture(uTex[idx], uv) * vColor;

    if (P.color.a <= 0.0) {
        outColor = texel;
        return;
    }

    // Primary mask from source alpha
    float a = texel.a;

    // Only when pixel is basically opaque (no useful alpha), derive mask from RGB
    // Assumes dark glyph on light background. Flip (luma) if opposite.
    float luma = dot(texel.rgb, vec3(0.299, 0.587, 0.114));
    float rgbMask = 1.0 - luma;
    rgbMask = smoothstep(0.35, 0.55, rgbMask);

    // Choose alpha where it’s informative; otherwise use RGB-derived mask
    float mask = (a < 0.98) ? a : rgbMask;

    outColor = vec4(P.color.rgb, mask * P.color.a);
}
