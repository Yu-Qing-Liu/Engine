#version 450

layout(location = 0) in vec3 inPos;

layout(location = 1) in vec4 inModelCol0;
layout(location = 2) in vec4 inModelCol1;
layout(location = 3) in vec4 inModelCol2;
layout(location = 4) in vec4 inModelCol3;

layout(location = 5) in vec4 inColor;
layout(location = 6) in float inCellSize;
layout(location = 7) in float inLineWidth;
layout(location = 8) in uint inPlane;

layout(location = 0) out vec3 outWorldPos;
layout(location = 1) out vec4 outColor;
layout(location = 2) out float outCellSize;
layout(location = 3) out float outLineWidth;
layout(location = 4) flat out uint outPlane;

layout(binding = 0) uniform VP {
    mat4 view;
    mat4 proj;
    uint billboard;
} cam;

void main()
{
    vec3 p = inPos; // quad in XZ: (x,0,z)
    vec3 localPos;

    switch (inPlane) {
    // YZ plane: planes 0 (ZXY) and 3 (ZNXNY)
    case 0u:
    case 3u:
        localPos = vec3(0.0, p.x, p.z);
        break;

    // XY plane: planes 2 (YZX) and 5 (NYNZX)
    case 2u:
    case 5u:
        localPos = vec3(p.x, p.z, 0.0);
        break;

    // XZ plane: planes 1 (ZYNX) and 4 (ZNYX)
    case 1u:
    case 4u:
    default:
        localPos = vec3(p.x, 0.0, p.z);
        break;
    }

    mat4 model = mat4(inModelCol0, inModelCol1, inModelCol2, inModelCol3);
    vec4 worldPos = model * vec4(localPos, 1.0);

    outWorldPos  = worldPos.xyz;
    outColor     = inColor;
    outCellSize  = inCellSize;
    outLineWidth = inLineWidth;
    outPlane     = inPlane;

    gl_Position = cam.proj * cam.view * worldPos;
}

