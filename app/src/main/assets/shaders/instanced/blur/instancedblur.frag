#version 450

// --- VS -> FS interface (match locations/types from your instanced VS) ---
layout(location = 0)  in vec4  vColor;
layout(location = 1)  in vec3  vBary;
layout(location = 2)  in vec3  vEdgeMask;
layout(location = 3)  flat in vec4  vIColor;
layout(location = 4)  flat in vec4  vIOutlineColor;
layout(location = 5)  flat in float vIOutlineWidth;
layout(location = 11) in vec4  vTint;   // used as tint (rgba)

// Scene color with mips (same set/binding as before)
layout(set = 1, binding = 0) uniform sampler2D uScene;

// Output
layout(location = 0) out vec4 outColor;

// Push constants (keep layout; cornerRadius no-op without vLocal)
layout(push_constant) uniform Push {
    vec2  invExtent;    // 1/width, 1/height
    float radius;       // blur radius (px)
    float lodScale;     // strength scale
    vec4  tint;         // unused here; using vTint instead
    float microTent;    // 1 = add 3x3 tent on top
    float cornerRadiusPxOverride; // ignored (no vLocal)
} pc;

// --- helpers ---
float maxMipLevel() {
    ivec2 sz = textureSize(uScene, 0);
    return floor(log2(float(max(sz.x, sz.y))));
}
vec3 tentSampleLOD(vec2 uv, vec2 ofs, float lod) {
    const float TENT_BIAS = 0.5;
    return textureLod(uScene, uv + ofs * TENT_BIAS, lod).rgb;
}

void main() {
    // Mip-based blur using screen-space UV
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

    // Per-instance tint: use vTint.rgb as target color and vTint.a as mix/alpha
    float mixAmt = clamp(vTint.a, 0.0, 1.0);
    vec3  mixed  = mix(blurred, vTint.rgb, mixAmt);

    outColor = vec4(mixed, mixAmt);
}

