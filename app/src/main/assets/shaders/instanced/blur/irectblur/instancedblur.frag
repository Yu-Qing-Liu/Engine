#version 450

// ===== inputs from your irectangle VS =====
layout(location = 0)  in vec2  vLocal;         // [-0.5 .. +0.5]
layout(location = 1)  in vec4  vColor;         // unused here
layout(location = 2)  in vec4  vOutlineColor;  // unused here
layout(location = 3)  in float vOutlineWidth;  // unused here
layout(location = 4)  in float vBorderRadius;  // instance radius (px)
layout(location = 11) in vec4  vTint;          // rgb=tint, a=final alpha

// per-scene sampler (with mips)
layout(set = 1, binding = 0) uniform sampler2D uScene;

// output
layout(location = 0) out vec4 outColor;

// push constants (matches your working “individual” shader)
layout(push_constant) uniform Push {
    vec2  invExtent;              // 1/width, 1/height  (full-frame path)
    float radius;                 // blur radius (px)
    float lodScale;               // blur strength scale
    vec4  tint;                   // unused; we use vTint instead
    float microTent;              // 1.0=on, 0.0=off
    float cornerRadiusPxOverride; // >=0 => use this radius (px), <0 => use instance radius
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

// rounded-rect SDF in *pixels* (circle-corner variant)
float sdRoundBoxCircle(vec2 p, vec2 b, float r) {
    vec2 q = abs(p) - b + vec2(r);
    return length(max(q, vec2(0.0))) + min(max(q.x, q.y), 0.0) - r;
}

void main() {
    // --- mip-based blur (full-frame UV) ---
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

    // --- tint mix (alpha = vTint.a) ---
    float mixAmt = clamp(vTint.a, 0.0, 1.0);
    vec3  mixed  = mix(blurred, vTint.rgb, mixAmt);

    // --- rounded-rect hard clip in pixel space (like your working shader) ---
    // estimate pixels-per-object from derivatives
    float objPerPxX = max(abs(dFdx(vLocal.x)), 1e-6);
    float objPerPxY = max(abs(dFdy(vLocal.y)), 1e-6);
    vec2  pxPerObj  = 1.0 / vec2(objPerPxX, objPerPxY);

    // map object-space [-0.5..+0.5] to pixels
    const vec2 halfObj = vec2(0.5);
    vec2 ppx = vLocal * pxPerObj;   // point in px
    vec2 bpx = halfObj * pxPerObj;  // half-size in px

    float rInst = clamp(vBorderRadius, 0.0, min(bpx.x, bpx.y));
    float rpx   = (pc.cornerRadiusPxOverride >= 0.0)
                ? clamp(pc.cornerRadiusPxOverride, 0.0, min(bpx.x, bpx.y))
                : rInst;

    float d  = sdRoundBoxCircle(ppx, bpx, rpx);
    float aa = max(fwidth(d), 1.0);

    // coverage (if you want feather), but we’ll hard clip like your “individual” shader
    float cov = 1.0 - smoothstep(0.0, aa, d);
    if (cov <= 0.0) {
        discard; // corners become truly transparent, no black
    }
    // If you prefer a soft feather, uncomment:
    // mixed *= cov;

    // final color: premul not required by your working path (keeps it identical)
    outColor = vec4(mixed, mixAmt);
}

