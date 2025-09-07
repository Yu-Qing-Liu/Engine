#version 450

layout(location=0) in vec3 inPos;
layout(location=1) in vec2 inUV;
layout(location=0) out vec2 vUV;

layout(set=0, binding=0) uniform UBO {
    mat4 model;
    mat4 view;
    mat4 proj; 
} u;

void main() {
    vUV = inUV;
    gl_Position = u.proj * u.view * u.model * vec4(inPos,1.0); 
}
