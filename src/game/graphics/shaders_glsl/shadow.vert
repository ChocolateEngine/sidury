#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : enable

layout(push_constant) uniform Push
{
	mat4 aModel;
	int  aViewInfo;
	int  aAlbedo;
} push;

// view info
layout(set = 1, binding = 0) buffer readonly UBO_ViewInfo
{
	mat4 aProjView;
	mat4 aProjection;
	mat4 aView;
	vec3 aViewPos;
	float aNearZ;
	float aFarZ;
} gViewInfo[];

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inTexCoord;

layout(location = 0) out vec2 outTexCoord;

void main()
{
	gl_Position = gViewInfo[push.aViewInfo].aProjView * push.aModel * vec4(inPosition, 1.0);
	outTexCoord = inTexCoord;
}

