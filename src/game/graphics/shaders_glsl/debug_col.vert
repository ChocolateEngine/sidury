#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : enable

layout(push_constant) uniform Push
{
    mat4 aModelMatrix;
    vec4 aColor;
} push;

// view info
layout(set = 0, binding = 0) buffer readonly UBO_ViewInfo
{
	mat4 aProjView;
	mat4 aProjection;
	mat4 aView;
	vec3 aViewPos;
	float aNearZ;
	float aFarZ;
} gViewInfo[];

layout(location = 0) in vec3 inPosition;

layout(location = 0) out vec4 fragColor;

void main()
{
	gl_Position = gViewInfo[0].aProjView * push.aModelMatrix * vec4(inPosition, 1.0);
    fragColor   = push.aColor;
}
