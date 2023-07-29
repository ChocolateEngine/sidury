#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : enable

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
layout(location = 1) in vec3 inColor;

layout(location = 0) out vec4 fragColor;

void main()
{
	gl_Position = gViewInfo[0].aProjView * vec4(inPosition, 1.0);
    fragColor   = vec4(inColor, 1);
}

