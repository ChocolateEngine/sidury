#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : enable

// TODO: having this here breaks the shader and doesn't bind texSampler descriptor set wtf
// layout(push_constant) uniform Push{
//     int  index;
// } push;

layout(set = 0, binding = 0) uniform sampler2D[] texSampler;

layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) flat in int  index;

layout(location = 0) out vec4 outColor;

// https://gamedev.stackexchange.com/questions/92015/optimized-linear-to-srgb-glsl
// Converts a color from linear light gamma to sRGB gamma
vec4 fromLinear( vec4 linearRGB )
{
    bvec3 cutoff = lessThan(linearRGB.rgb, vec3(0.0031308));
    vec3 higher = vec3(1.055)*pow(linearRGB.rgb, vec3(1.0/2.4)) - vec3(0.055);
    vec3 lower = linearRGB.rgb * vec3(12.92);

    return vec4(mix(higher, lower, cutoff), linearRGB.a);
}

// Converts a color from sRGB gamma to linear light gamma
// REALLY close, but still not perfect
vec4 toLinear( vec4 sRGB )
{
    bvec3 cutoff = lessThan(sRGB.rgb, vec3(0.04045));
    vec3 higher = pow((sRGB.rgb + vec3(0.055))/vec3(1.055), vec3(2.4));
    vec3 lower = sRGB.rgb/vec3(12.92);

    return vec4(mix(higher, lower, cutoff), sRGB.a);
}

void main()
{
    // Convert Color Space
    outColor = toLinear( texture( texSampler[index], fragTexCoord ) );
}
