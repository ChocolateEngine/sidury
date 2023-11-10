#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : enable

#include "core.glsl"

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;

layout(location = 0) out vec4 fragColor;

void main()
{
	gl_Position = gViewport[0].aProjView * vec4(inPosition, 1.0);
    fragColor   = vec4(inColor, 1);
}

