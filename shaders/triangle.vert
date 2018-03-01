#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 0) uniform uniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec3 inColor;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 outUV;

const vec2 madd=vec2(0.5,0.5);

out gl_PerVertex {
    vec4 gl_Position;
};

void main() {
    gl_Position = ubo.proj * ubo.view * vec4(inPosition, 0.0f, 1.0f);
    fragColor = inColor;
    outUV = inPosition * madd + madd;
}