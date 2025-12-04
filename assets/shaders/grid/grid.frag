#version 450

layout(location = 0) in vec3 inWorldPos;
layout(location = 1) in vec4 inColor;
layout(location = 2) in float inCellSize;
layout(location = 3) in float lineWidth;

layout(location = 0) out vec4 outColor;

void main() {
    float cell = max(inCellSize, 1e-4);
    vec2 gridCoord = inWorldPos.xz / cell;

    vec2 deriv = fwidth(gridCoord);
    vec2 g = abs(fract(gridCoord - 0.5) - 0.5) / deriv;
    float dist = min(g.x, g.y);

    float px = max(lineWidth, 0.1); // prevent divide-by-zero

    float strength = clamp((px - dist) / px, 0.0, 1.0);
    float alpha = inColor.a * strength;

    if (alpha <= 0.001)
        discard;

    outColor = vec4(inColor.rgb, alpha);
}
