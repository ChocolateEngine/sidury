#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : enable

#include "core.glsl"

layout(push_constant) uniform Push{
	mat4 matrix;
    int sky;
} push;

layout(location = 0) in vec3 fragTexCoord;

layout(location = 0) out vec4 outColor;

void main()
{
    outColor = texture(texSampler[push.sky], fragTexCoord);
}

