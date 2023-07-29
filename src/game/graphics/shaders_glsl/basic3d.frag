#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : enable

#include "common_shadow.glsl"

layout(push_constant) uniform Push
{
	mat4 model;
    int material;
	int aViewInfo;

	bool aPCF;
	int aDebugDraw;
} push;

layout(set = 0, binding = 0) uniform sampler2D[] texSamplers;

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

// TODO: THIS SHOULD NOT BE VARIABLE
layout(set = 2, binding = 0) buffer readonly UBO_LightInfo
{
	int aCountWorld;
	int aCountPoint;
	int aCountCone;
	int aCountCapsule;
} gLightInfoTmp[];

#define gLightInfo gLightInfoTmp[0]

layout(set = 3, binding = 0) buffer readonly UBO_LightWorld
{
    vec4 aColor;
    vec3 aDir;
	int  aViewInfo;  // view info for light/shadow
	int  aShadow;  // shadow texture index
} gLightsWorld[];

layout(set = 4, binding = 0) buffer readonly UBO_LightPoint
{
	vec4  aColor;
	vec3  aPos;
	float aRadius;  // TODO: look into using constant, linear, and quadratic lighting, more customizable than this
} gLightsPoint[];

layout(set = 5, binding = 0) buffer readonly UBO_LightCone
{
	vec4 aColor;
	vec3 aPos;
	vec3 aDir;
	vec2 aFov;  // x is inner FOV, y is outer FOV
	int  aViewInfo;  // view info for light/shadow
	int  aShadow;  // shadow texture index
} gLightsCone[];

// layout(set = 6, binding = 0) buffer readonly UBO_LightCapsule
// {
// 	vec4  aColor;
// 	vec3  aDir;
// 	float aLength;
// 	float aThickness;
// } gLightsCapsule[];

// Material Info
layout(set = 6, binding = 0) buffer readonly UBO_Material
{
    int albedo;
    int ao;
    int emissive;

    float aoPower;
    float emissivePower;

	bool aAlphaTest;
} materials[];

layout(location = 0) in vec2 fragTexCoord;
layout(location = 1) in vec3 inPosition;
layout(location = 2) in vec3 inPositionWorld;
layout(location = 3) in vec3 inNormal;
layout(location = 4) in vec3 inNormalWorld;
layout(location = 5) in vec3 inTangent;

layout(location = 0) out vec4 outColor;

#define mat materials[push.material]
// #define mat materials[0]

#define texDiffuse  texSamplers[mat.albedo]
#define texAO       texSamplers[mat.ao]
#define texEmissive texSamplers[mat.emissive]


const mat4 gBiasMat = mat4(
	0.5, 0.0, 0.0, 0.0,
	0.0, 0.5, 0.0, 0.0,
	0.0, 0.0, 1.0, 0.0,
	0.5, 0.5, 0.0, 1.0 );

const float gAmbient = 0.0f;

float LinearizeDepth( float sNearZ, float sFarZ, float sDepth )
{
	float n = sNearZ;
	float f = sFarZ;
	float z = sDepth;
	return (2.0 * n) / (f + n - z * (f - n));
}


void main()
{
    // outColor = vec4( lightIntensity * vec3(texture(texDiffuse, fragTexCoord)), 1 );
    vec4 albedo = texture( texDiffuse, fragTexCoord );

	// TODO: Alpha Reference
	if ( mat.aAlphaTest && albedo.a < 0.5f )
		discard;

	// outColor = albedo;
	// return;

	// Calculate normal in tangent space
	vec3 vertNormal = normalize( inNormal );
	vec3 T = inTangent;
	vec3 B = cross(vertNormal, T);
	mat3 TBN = mat3(T, B, vertNormal);
	// vec3 tnorm = TBN * normalize(texture(samplerNormalMap, inUV).xyz * 2.0 - vec3(1.0));
	vec3 tnorm = TBN * vec3(1.0, 1.0, 1.0);
	// outNormal = vec4( tnorm, 1.0 );
	// outNormal = vec4( inTangent, 1.0 );
	// outNormal = vec4( inNormal, 1.0 );

	if ( push.aDebugDraw == 1 )
		albedo = vec4(1, 1, 1, 1);

	if ( push.aDebugDraw > 1 && push.aDebugDraw < 5 )
	{
		if ( push.aDebugDraw == 2 )
			outColor = vec4(inNormalWorld, 1);

		if ( push.aDebugDraw == 3 )
			outColor = vec4(inNormal, 1);

		if ( push.aDebugDraw == 4 )
			outColor = vec4(inTangent, 1);

		return;
	}

	outColor = vec4(0, 0, 0, albedo.a);

	//if ( gLightInfo.aCountWorld == 0 )
		outColor = albedo;

	for ( int i = 0; i < gLightInfo.aCountWorld; i++ )
	{
		if ( gLightsWorld[ i ].aColor.w == 0.f )
			continue;

		// Diffuse part
		float intensity = max( dot( inNormalWorld, gLightsWorld[ i ].aDir ), 0.f );

		vec3 diff;

		// if ( push.dbgShowDiffuse )
		//	outColor.rgb += gLightsWorld[ i ].aColor * vec3( max( intensity, 0.15 ) );
		// else
			diff = gLightsWorld[ i ].aColor.rgb * gLightsWorld[ i ].aColor.a * max( intensity, 0.15 ) * albedo.rgb;

		// shadow
		//if ( gLightsWorld[ i ].aShadow != -1 )
		//{
		//	mat4 depthBiasMVP = gBiasMat * gViewInfo[ gLightsWorld[ i ].aViewInfo ].aProjView;
		//	// mat4 depthBiasMVP = gBiasMat * gViewInfo[ 0 ].aProjView;
		//	vec4 shadowCoord = depthBiasMVP * vec4( inPositionWorld, 1.0 );
		//
		//	// float shadow = SampleShadowMapPCF( gLightsWorld[ i ].aShadow, shadowCoord.xyz / shadowCoord.w );
		//	float shadow = SampleShadowMapPCF( 0, shadowCoord.xyz / shadowCoord.w );
		//	diff *= shadow;
		//	// diff = shadow;
		//}

		outColor.rgb += diff;
	}

	for ( int i = 0; i < gLightInfo.aCountPoint; i++ )
	{
		if ( gLightsPoint[ i ].aColor.w == 0.f )
			continue;

		// Vector to light
		vec3 lightDir = gLightsPoint[ i ].aPos - inPositionWorld;

		// Distance from light to fragment position
		float dist = length( lightDir );

		//if(dist < ubo.lights[i].radius)
		{
			lightDir = normalize(lightDir);

			// Attenuation
			float atten = gLightsPoint[ i ].aRadius / (pow(dist, 2.0) + 1.0);

			// Diffuse part
			vec3 vertNormal = normalize( inNormalWorld );
			float NdotL = max( 0.0, dot(vertNormal, lightDir) );
			vec3 diff = gLightsPoint[ i ].aColor.rgb * gLightsPoint[ i ].aColor.a * albedo.rgb * NdotL * atten;

			outColor.rgb += diff;
		}
	}

	#define CONSTANT 1
	#define LINEAR 1
	#define QUADRATIC 1

	for ( int i = 0; i < gLightInfo.aCountCone; i++ )
	{
		if ( gLightsCone[ i ].aColor.w == 0.f )
			continue;

		// Vector to light
		vec3 lightDir = gLightsCone[ i ].aPos - inPositionWorld;
		lightDir = normalize(lightDir);

		// Distance from light to fragment position
		float dist = length( lightDir );

		float theta = dot( lightDir, normalize( gLightsCone[ i ].aDir.xyz ));

		// Inner Cone FOV - Outer Cone FOV
		float epsilon   = gLightsCone[ i ].aFov.x - gLightsCone[ i ].aFov.y;
		// float intensity = clamp( (theta - gLightsCone[ i ].aFov.y) / epsilon, 0.0, 1.0 );
		float intensity = clamp( (gLightsCone[ i ].aFov.y - theta) / epsilon, 0.0, 1.0 );

		// attenuation
		float atten = 1.0 / (pow(dist, 1.0) + 1.0);
		// float atten = 1.0 / (CONSTANT + LINEAR * dist + QUADRATIC * (dist * dist));
		// float atten = (CONSTANT + LINEAR * dist + QUADRATIC * (dist * dist));

		vec3 diff = gLightsCone[ i ].aColor.rgb * gLightsCone[ i ].aColor.a * albedo.rgb * intensity * atten;

		// shadow
		if ( gLightsCone[ i ].aShadow != -1 )
		{
			mat4 depthBiasMVP = gBiasMat * gViewInfo[ gLightsCone[ i ].aViewInfo ].aProjView;
			vec4 shadowCoord = depthBiasMVP * vec4( inPositionWorld, 1.0 );

			// TODO: this could go out of bounds and lose the device
			// maybe add a check here to make sure we don't go out of bounds?

			// TODO: For some reason, SampleShadowMapPCF causes the GTX 1050 Ti I have to return VK_ERROR_DEVICE_LOST here
			// and the original way i did it doesn't
			float shadow = 1.0;
			if ( push.aPCF )
			{
				shadow = SampleShadowMapPCF( gLightsCone[ i ].aShadow, shadowCoord.xyz / shadowCoord.w );
			}
			else
			{
				shadow = SampleShadowMapBasic( gLightsCone[ i ].aShadow, shadowCoord.xyz / shadowCoord.w );
			}

			// float shadow = SampleShadowMapPCF( gLightsCone[ i ].aShadow, shadowCoord.xyz / shadowCoord.w );
			// float shadow = SampleShadowMapPCF( 0, shadowCoord.xyz / shadowCoord.w );
			// float shadow = SampleShadowMapBasic( 0, vec4( shadowCoord / shadowCoord.w ) );
			diff *= shadow;
		}

		outColor.rgb += diff;
	}

	// add ambient occlusion (only one channel is needed here, so just use red)
    if ( mat.aoPower > 0.0 )
		outColor *= mix( 1, texture(texAO, fragTexCoord).r, mat.aoPower );

	// add emission
    if ( mat.emissivePower > 0.0 )
	    outColor.rgb += mix( vec3(0, 0, 0), texture(texEmissive, fragTexCoord).rgb, mat.emissivePower );
}

// look at this later for cubemaps:
// https://forum.processing.org/two/discussion/27871/use-samplercube-and-sampler2d-in-a-single-glsl-shader.html

