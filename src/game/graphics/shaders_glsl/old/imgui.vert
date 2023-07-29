#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(push_constant) uniform Push{
    int  index;
} push;

layout(location = 1) out vec2 fragTexCoord;
layout(location = 2) out int  index;

void main()
{
	fragTexCoord = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    gl_Position = vec4(fragTexCoord * 2.0f + -1.0f, 0.0f, 1.0f);
	index = push.index;
}
