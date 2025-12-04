#version 450

layout(location = 0) in vec2 vLocal;
layout(location = 1) in vec4 vColor;
layout(location = 2) in vec4 vOutlineColor;
layout(location = 3) in float vOutlineWidth;
layout(location = 4) in float vBorderRadius;

layout(location = 0) out vec4 outColor;

float sdRoundBoxCircle(vec2 p, vec2 b, float r) {
    vec2 q = abs(p) - b + vec2(r);
    return length(max(q, vec2(0.0))) + min(max(q.x, q.y), 0.0) - r;
}

void main() {
    const vec2 halfObj = vec2(0.5);
    float objPerPxX = max(abs(dFdx(vLocal.x)), 1e-6);
    float objPerPxY = max(abs(dFdy(vLocal.y)), 1e-6);
    vec2 pxPerObj = 1.0 / vec2(objPerPxX, objPerPxY);

    vec2 ppx = vLocal * pxPerObj;
    vec2 bpx = halfObj * pxPerObj;

    float Wpx = max(vOutlineWidth, 0.0);
    float rpx = clamp(vBorderRadius, 0.0, min(bpx.x, bpx.y));

    float d = sdRoundBoxCircle(ppx, bpx, rpx);
    float aa = max(fwidth(d), 1.0);

    float fillCov = 1.0 - smoothstep(0.0, aa, d);
    float halfW = 0.5 * Wpx;
    float bandCov = 1.0 - smoothstep(halfW - aa, halfW + aa, abs(d));
    float strokeCov = bandCov * fillCov;

    vec4 fill = vColor * fillCov;
    vec4 stroke = vOutlineColor * strokeCov;

    outColor = stroke + fill * (1.0 - stroke.a);
}
