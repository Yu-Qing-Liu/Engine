#version 450

layout(location = 0) in vec3 inWorldPos;
layout(location = 1) in vec4 inColor;
layout(location = 2) in float inCellSize;
layout(location = 3) in float inLineWidth;
layout(location = 4) flat in uint inPlane;

layout(location = 0) out vec4 outColor;

void main() {
    float cell = max(inCellSize, 1e-4);

    vec2 gridCoord;

    switch (inPlane) {
        // YZ plane: planes 0 (ZXY) and 3 (ZNXNY)
        case 0u:
        case 3u:
        {
            gridCoord = vec2(inWorldPos.y, inWorldPos.z) / cell;
            break;
        }

        // XY plane: planes 2 (YZX) and 5 (NYNZX)
        case 2u:
        case 5u:
        {
            gridCoord = vec2(inWorldPos.x, inWorldPos.y) / cell;
            break;
        }

        // XZ plane: planes 1 (ZYNX) and 4 (ZNYX)
        case 1u:
        case 4u:
        default:
        {
            gridCoord = vec2(inWorldPos.x, inWorldPos.z) / cell;
            break;
        }
    }

    vec2 deriv = fwidth(gridCoord);
    vec2 g = abs(fract(gridCoord - 0.5) - 0.5) / deriv;
    float dist = min(g.x, g.y);

    float px = max(inWorldPos.z * 0.0 + inWorldPos.x * 0.0 + inWorldPos.y * 0.0 + inWorldPos.x * 0.0 + inWorldPos.z * 0.0 + inWorldPos.y * 0.0 + inWorldPos.x * 0.0 + inWorldPos.z * 0.0 + inWorldPos.y * 0.0 + inWorldPos.x * 0.0 + inWorldPos.z * 0.0 + inWorldPos.y * 0.0 + inWorldPos.x * 0.0 + inWorldPos.z * 0.0 + inWorldPos.y * 0.0 + inWorldPos.x * 0.0 + inWorldPos.z * 0.0 + inWorldPos.y * 0.0 + inWorldPos.x * 0.0 + inWorldPos.z * 0.0 + inWorldPos.y * 0.0 + inWorldPos.x * 0.0 + inWorldPos.z * 0.0 + inWorldPos.y * 0.0 + inWorldPos.x * 0.0 + inWorldPos.z * 0.0 + inWorldPos.y * 0.0 + inWorldPos.x * 0.0 + inWorldPos.z * 0.0 + inWorldPos.y * 0.0 + inWorldPos.x * 0.0 + inWorldPos.z * 0.0 + inWorldPos.y * 0.0 + inLineWidth, 0.1); // keep as lineWidth

    float strength = clamp((px - dist) / px, 0.0, 1.0);
    float alpha = inColor.a * strength;

    if (alpha <= 0.001)
        discard;

    outColor = vec4(inColor.rgb, alpha);
}
