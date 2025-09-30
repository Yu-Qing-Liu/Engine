#version 450

// Matches VS
layout(location = 0) in vec2 vLocal;
layout(location = 11) in vec4 vTint;

// Scene color with mips (set=1 binding=0 just like your base blur)
layout(set = 1, binding = 0) uniform sampler2D uScene;

// Output
layout(location = 0) out vec4 outColor;

// Push constants (must match your C++ 'Push' struct layout exactly)
layout(push_constant) uniform Push {
    vec2  invExtent;              // 1/width, 1/height
    float radius;                 // blur radius (px)
    float lodScale;               // strength scale
    vec4  tint;                   // mix color; tint.a = mix amount AND final alpha
    float microTent;              // 1=on
    float cornerRadiusPxOverride; // >=0 => rounded clip; <0 => none
} pc;

// ---- helpers ----
float maxMipLevel() {
    ivec2 sz = textureSize(uScene, 0);
    return floor(log2(float(max(sz.x, sz.y))));
}

vec3 tentSampleLOD(vec2 uv, vec2 ofs, float lod) {
    const float TENT_BIAS = 0.5;
    return textureLod(uScene, uv + ofs * TENT_BIAS, lod).rgb;
}

float sdRoundBoxCircle(vec2 p, vec2 b, float r) {
    vec2 q = abs(p) - b + vec2(r);
    return length(max(q, vec2(0.0))) + min(max(q.x, q.y), 0.0) - r;
}

void main() {
    vec2  uv     = gl_FragCoord.xy * pc.invExtent;
    float maxLod = maxMipLevel();
    float lod    = clamp(log2(max(pc.radius, 1.0)) * pc.lodScale, 0.0, maxLod);

    float lod0 = floor(lod);
    float lod1 = min(lod0 + 1.0, maxLod);
    float t    = lod - lod0;

    vec3 c0 = textureLod(uScene, uv, lod0).rgb;
    vec3 c1 = textureLod(uScene, uv, lod1).rgb;
    vec3 blurred = mix(c0, c1, t);

    if (pc.microTent > 0.0) {
        ivec2 msz   = textureSize(uScene, int(lod0));
        vec2  texel = 1.0 / vec2(msz);

        vec3 acc = blurred;
        acc += tentSampleLOD(uv, vec2(+texel.x, 0.0), lod);
        acc += tentSampleLOD(uv, vec2(-texel.x, 0.0), lod);
        acc += tentSampleLOD(uv, vec2(0.0, +texel.y), lod);
        acc += tentSampleLOD(uv, vec2(0.0, -texel.y), lod);

        acc += tentSampleLOD(uv, vec2(+texel.x, +texel.y), lod);
        acc += tentSampleLOD(uv, vec2(-texel.x, +texel.y), lod);
        acc += tentSampleLOD(uv, vec2(+texel.x, -texel.y), lod);
        acc += tentSampleLOD(uv, vec2(-texel.x, -texel.y), lod);

        blurred = acc * (1.0 / 9.0);
    }

    // tint
    float mixAmt = clamp(vTint.a, 0.0, 1.0);
    vec3  mixed  = mix(blurred, vTint.rgb, mixAmt);

    // Optional rounded clip in pixel space
    if (pc.cornerRadiusPxOverride >= 0.0) {
        float objdx = max(abs(dFdx(vLocal.x)), 1e-6);
        float objdy = max(abs(dFdy(vLocal.y)), 1e-6);
        vec2  pxPerObj = 1.0 / vec2(objdx, objdy);

        vec2 ppx = vLocal * pxPerObj;
        vec2 bpx = vec2(0.5) * pxPerObj;
        float rpx = clamp(pc.cornerRadiusPxOverride, 0.0, min(bpx.x, bpx.y));

        float d  = sdRoundBoxCircle(ppx, bpx, rpx);
        float aa = max(fwidth(d), 1.0);

        float cov = 1.0 - smoothstep(0.0, aa, d);
        if (cov <= 0.0) discard;
        // If you want feathering instead of hard clip:
        // mixed *= cov;
    }

    outColor = vec4(mixed, mixAmt);
}

