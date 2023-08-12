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

layout(location = 0) in vec2 inTexCoord;

void main()
{
	if ( push.aAlbedo >= 0 && texture( texSamplers[ push.aAlbedo ], inTexCoord ).a < 0.5)
		discard;
}
