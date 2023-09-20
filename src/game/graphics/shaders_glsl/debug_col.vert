#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : enable

#include "core.glsl"

layout(push_constant) uniform Push
{
    mat4 aModelMatrix;
    vec4 aColor;
} push;

layout(location = 0) in vec3 inPosition;

layout(location = 0) out vec4 fragColor;

void main()
{
	gl_Position = gViewport[0].aProjView * push.aModelMatrix * vec4(inPosition, 1.0);
    fragColor   = push.aColor;
}