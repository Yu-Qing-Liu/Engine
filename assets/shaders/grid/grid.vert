#version 450

layout(location = 0) in vec3 inPos;

layout(location = 1) in vec4 inModelCol0;
layout(location = 2) in vec4 inModelCol1;
layout(location = 3) in vec4 inModelCol2;
layout(location = 4) in vec4 inModelCol3;

layout(location = 5) in vec4 inColor;
layout(location = 6) in float inCellSize;
layout(location = 7) in float inLineWidth;

layout(location = 0) out vec3 outWorldPos;
layout(location = 1) out vec4 outColor;
layout(location = 2) out float outCellSize;
layout(location = 3) out float outLineWidth;

layout(binding = 0) uniform VP {
    mat4 view;
    mat4 proj;
    uint billboard;
} cam;

void main()
{
    mat4 model = mat4(inModelCol0, inModelCol1, inModelCol2, inModelCol3);

    vec4 worldPos = model * vec4(inPos, 1.0);

    outWorldPos = worldPos.xyz;
    outColor = inColor;
    outCellSize = inCellSize;
    outLineWidth = inLineWidth;

    gl_Position = cam.proj * cam.view * worldPos;
}
