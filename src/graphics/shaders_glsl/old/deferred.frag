#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : enable

layout(push_constant) uniform Push
{
	mat4 model;
    int material;
	int projView;
} push;

layout(set = 0, binding = 0) uniform sampler2D[] texSamplers;

// Material Info
layout(set = 2, binding = 0) uniform UBO_Material
{
    int diffuse;
    int ao;
    int emissive;

    float aoPower;
    float emissivePower;
} materials[];

layout(location = 0) in vec2 fragTexCoord;
layout(location = 1) in float lightIntensity;

layout(location = 0) out vec4 outColor;

#define mat materials[push.material]
// #define mat materials[0]

#define texDiffuse  texSamplers[mat.diffuse]
#define texAO       texSamplers[mat.ao]
#define texEmissive texSamplers[mat.emissive]

void main()
{
    outColor = vec4( lightIntensity * vec3(texture(texDiffuse, fragTexCoord)), 1 );
	
	// add ambient occlusion (only one channel is needed here, so just use red)
    if ( mat.aoPower > 0.0 )
	    outColor.rgb *= mix( 1, texture(texAO, fragTexCoord).r, mat.aoPower );

	// add emission
    if ( mat.emissivePower > 0.0 )
	    outColor.rgb += mix( vec3(0, 0, 0), texture(texEmissive, fragTexCoord).rgb, mat.emissivePower );
}

// look at this later for cubemaps:
// https://forum.processing.org/two/discussion/27871/use-samplercube-and-sampler2d-in-a-single-glsl-shader.html

