#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : enable

#include "core.glsl"

layout(push_constant) uniform Push
{
	mat4 aModel;
	int  aViewInfo;
	int  aAlbedo;
} push;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inTexCoord;

layout(location = 0) out vec2 outTexCoord;

void main()
{
	gl_Position = gViewport[push.aViewInfo].aProjView * push.aModel * vec4(inPosition, 1.0);
	outTexCoord = inTexCoord;
}

