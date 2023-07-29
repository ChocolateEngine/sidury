#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : enable

layout(set = 0, binding = 0) uniform sampler2DArray[] texSampler;

layout(location = 0) in vec2 fragTexCoord;
layout(location = 1) flat in int  index;
layout(location = 2) flat in int  layer;


layout(location = 0) out vec4 outColor;

void main() {
    outColor = texture(texSampler[index], vec3(fragTexCoord,layer));
}