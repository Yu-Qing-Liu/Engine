#version 450

layout(location=0) in vec2 vLocal;

layout(binding=1) uniform Params {
    vec4  color;         // fill
    vec4  outlineColor;  // outline
    float outlineWidth;  // pixels
    float borderRadius;  // pixels
    float _pad1, _pad2;
} P;

layout(location=0) out vec4 outColor;

float sdRoundBoxCircle(vec2 p, vec2 b, float r) {
    vec2 q = abs(p) - b + vec2(r);
    return length(max(q, vec2(0.0))) + min(max(q.x, q.y), 0.0) - r;
}

void main() {
    // object-space quad = [-0.5, +0.5]
    const vec2 halfObj = vec2(0.5);

    // per-axis pixels-per-object (axis-aligned)
    float objPerPxX = max(abs(dFdx(vLocal.x)), 1e-6);
    float objPerPxY = max(abs(dFdy(vLocal.y)), 1e-6);
    vec2  pxPerObj  = 1.0 / vec2(objPerPxX, objPerPxY);

    // map to pixel space
    vec2 ppx = vLocal * pxPerObj;
    vec2 bpx = halfObj * pxPerObj;

    float rpx = clamp(P.borderRadius, 0.0, min(bpx.x, bpx.y));
    float Wpx = max(P.outlineWidth, 0.0);

    // single SDF in *pixels*
    float d = sdRoundBoxCircle(ppx, bpx, rpx);

    // AA in pixels (floor so 1px bands survive)
    float aa = max(fwidth(d), 1.0);

    // inside (fill) coverage
    float fillCov = 1.0 - smoothstep(0.0, aa, d);

    // centered band for stroke, then clamp to inside so it’s fully within the rect
    float halfW   = 0.5 * Wpx;
    float bandCov = 1.0 - smoothstep(halfW - aa, halfW + aa, abs(d)); // |d| <= W/2
    float strokeCov = bandCov * fillCov; // keep the band inside -> always visible on all sides

    vec4 fill   = P.color        * fillCov;
    vec4 stroke = P.outlineColor * strokeCov;

    // draw stroke over fill
    outColor = stroke + fill * (1.0 - stroke.a);

    // (optional) debug: highlight boundary
    // float edge = 1.0 - smoothstep(aa, 2.0*aa, abs(d));
    // outColor = mix(outColor, vec4(1,0,0,1), 0.2 * edge);
}

