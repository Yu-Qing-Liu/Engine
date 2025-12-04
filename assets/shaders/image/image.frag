#version 450
#extension GL_EXT_nonuniform_qualifier : enable

layout(location = 0) in vec2 vUV;
layout(location = 1) flat in uint vFrame;
layout(location = 2) flat in uint vCover;

layout(location = 0) out vec4 outColor;

layout(set = 1, binding = 0) uniform sampler2D uTex[];

// Push constants: framebuffer and viewport in pixels
layout(push_constant) uniform ViewPx {
    uvec4 fb; // {fbw, fbh, _, _}  (framebuffer)
    uvec4 vp; // {x, y, vw, vh}    (viewport)
} pc;

void main() {
    // ----- COVER PATH: icon/button etc. -----
    if (vCover != 0u) {
        // Optional: clamp or discard outside [0,1]
        if (vUV.x < 0.0 || vUV.y < 0.0 || vUV.x > 1.0 || vUV.y > 1.0) {
            outColor = vec4(0.0);
            return;
        }

        outColor = texture(uTex[nonuniformEXT(vFrame)], vUV);
        return;
    }

    // ----- OLD FRAMEBUFFER / VIEWPORT-ALIGNED PATH -----
    // Convert framebuffer coord -> viewport-local coord
    vec2 fragInVp = gl_FragCoord.xy - vec2(pc.vp.xy);

    // Defensively clip to viewport
    if (fragInVp.x < 0.0 || fragInVp.y < 0.0 ||
            fragInVp.x >= float(pc.vp.z) || fragInVp.y >= float(pc.vp.w)) {
        outColor = vec4(0.0);
        return;
    }

    // Map viewport pixels -> framebuffer pixels (for HiDPI)
    vec2 scale = vec2(pc.fb.xy) / vec2(max(pc.vp.zw, uvec2(1)));
    vec2 fragFBf = fragInVp * scale;
    ivec2 fragFB = ivec2(floor(fragFBf + 0.5));

    ivec2 texSz = textureSize(uTex[nonuniformEXT(vFrame)], 0);

    // Window to sample = min(framebuffer, texture) in framebuffer pixels
    ivec2 winFB = min(ivec2(pc.fb.xy), texSz);

    ivec2 screenOrgFB = (ivec2(pc.fb.xy) - winFB) / 2;
    ivec2 texOrg = (texSz - winFB) / 2;

    bvec2 ge = greaterThanEqual(fragFB, screenOrgFB);
    bvec2 lt = lessThan(fragFB, screenOrgFB + winFB);
    if (!(all(ge) && all(lt))) {
        outColor = vec4(0.0);
        return;
    }

    ivec2 tc = texOrg + (fragFB - screenOrgFB);
    outColor = texelFetch(uTex[nonuniformEXT(vFrame)], tc, 0);
}
