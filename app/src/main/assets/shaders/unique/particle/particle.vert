#version 450

layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec4 inColor;
layout(location = 2) in float inSize;

layout(location = 0) out vec4 vColor;

void main() {
    gl_PointSize = inSize;
    gl_Position = vec4(inPosition, 1.0, 1.0);
    vColor = inColor;
}
