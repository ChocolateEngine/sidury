#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : enable

#define CH_FRAG_SHADER 1

#include "core.glsl"
#include "common_shadow.glsl"

#define VIEWPORT 0
#define MATERIAL 1

layout(push_constant) uniform Push
{
	uint aRenderable;
	uint aMaterial;
	uint aViewport;
	uint aDebugDraw;
} push;

// Material Info
layout(set = CH_DESC_SET_PER_SHADER, binding = 0) buffer readonly Buffer_Material
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

#define mat materials[ push.aMaterial ]
// #define mat materials[0]

#define texDiffuse  texSamplers[ mat.albedo ]
#define texAO       texSamplers[ mat.ao ]
#define texEmissive texSamplers[ mat.emissive ]


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


float GetLuminance( vec3 color )
{
	return dot( color, vec3( 0.2125f, 0.7154f, 0.0721f ) );
}


float GetActualLuminance( vec3 color )
{
    return dot( color, vec3(1.0 / 3.0) );
}


void main()
{
	// SurfaceDraw_t surface    = gSurfaceDraws[ push.aSurface ];
	Renderable_t renderable = gRenderables[ push.aRenderable ];

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

	if ( gCore.aNumLights[ CH_LIGHT_TYPE_WORLD ] == 0 )
		outColor = albedo;

#if 01
	// ----------------------------------------------------------------------------
	// Add World Lights

	// for ( int i = 0; i < gCore.aNumLightWorld; i++ )
	for ( int i = 0; i < gCore.aNumLights[ CH_LIGHT_TYPE_WORLD ]; i++ )
	{
		if ( gCore.aLightWorld[ i ].aColor.w == 0.f )
			continue;

		// Diffuse part
		float intensity = max( dot( inNormalWorld, gCore.aLightWorld[ i ].aDir ), 0.f );

		vec3 diff;

		// if ( push.dbgShowDiffuse )
		//	outColor.rgb += aLightWorld[ i ].aColor * vec3( max( intensity, 0.15 ) );
		// else
			diff = gCore.aLightWorld[ i ].aColor.rgb * gCore.aLightWorld[ i ].aColor.a * max( intensity, 0.15 ) * albedo.rgb;

		// shadow
		// if ( gLightsWorld[ i ].aShadow != -1 )
		// {
		// 	mat4 depthBiasMVP = gBiasMat * gLightsWorld[ i ].aProjView;
		// 	// mat4 depthBiasMVP = gBiasMat * gViewport[ 0 ].aProjView;
		// 	vec4 shadowCoord = depthBiasMVP * vec4( inPositionWorld, 1.0 );
// 
		// 	// float shadow = SampleShadowMapPCF( gLightsWorld[ i ].aShadow, shadowCoord.xyz / shadowCoord.w );
		// 	float shadow = SampleShadowMapPCF( 0, shadowCoord.xyz / shadowCoord.w );
		// 	diff *= shadow;
		// 	// diff = shadow;
		// }

		outColor.rgb += diff;
	}
#endif

#if 1
	// ----------------------------------------------------------------------------
	// Add Point Lights

	for ( int i = 0; i < gCore.aNumLights[ CH_LIGHT_TYPE_POINT ]; i++ )
	{
		if ( gCore.aLightPoint[ i ].aColor.w == 0.f )
			continue;

		// Vector to light
		vec3 lightDir = gCore.aLightPoint[ i ].aPos - inPositionWorld;

		// Distance from light to fragment position
		float dist = length( lightDir );

		//if(dist < ubo.lights[i].radius)
		{
			lightDir = normalize(lightDir);

			// Attenuation
			float atten = gCore.aLightPoint[ i ].aRadius / (pow(dist, 2.0) + 1.0);

			// Diffuse part
			vec3 vertNormal = normalize( inNormalWorld );
			float NdotL = max( 0.0, dot(vertNormal, lightDir) );
			vec3 diff = gCore.aLightPoint[ i ].aColor.rgb * gCore.aLightPoint[ i ].aColor.a * albedo.rgb * NdotL * atten;
			// vec3 diff = gCore.aLightPoint[ i ].aColor.rgb * gCore.aLightPoint[ i ].aColor.a * NdotL * atten;

			// vec3 lightColor = gCore.aLightPoint[ i ].aColor.rgb;
			// float albedoLuminance = GetLuminance( albedo.rgb );
			// lightColor = mix( lightColor, albedo.rgb * lightColor, albedoLuminance );
// 
			// vec3 diff = lightColor * gCore.aLightPoint[ i ].aColor.a * NdotL * atten;

			outColor.rgb += diff;
		}
	}

	// ----------------------------------------------------------------------------
	// Add Cone Lights

	#define CONSTANT 1
	#define LINEAR 1
	#define QUADRATIC 1

	for ( int i = 0; i < gCore.aNumLights[ CH_LIGHT_TYPE_CONE ]; i++ )
	{
		if ( gCore.aLightCone[ i ].aColor.w == 0.f )
			continue;

		// Vector to light
		vec3 lightDir = gCore.aLightCone[ i ].aPos - inPositionWorld;
		lightDir = normalize(lightDir);

		// Distance from light to fragment position
		float dist = length( lightDir );

		float theta = dot( lightDir, normalize( gCore.aLightCone[ i ].aDir.xyz ));

		// Inner Cone FOV - Outer Cone FOV
		float epsilon   = gCore.aLightCone[ i ].aFov.x - gCore.aLightCone[ i ].aFov.y;
		// float intensity = clamp( (theta - gCore.aLightCone[ i ].aFov.y) / epsilon, 0.0, 1.0 );
		float intensity = clamp( (gCore.aLightCone[ i ].aFov.y - theta) / epsilon, 0.0, 1.0 );

		// attenuation
		float atten = 1.0 / (pow(dist, 1.0) + 1.0);
		// float atten = 1.0 / (CONSTANT + LINEAR * dist + QUADRATIC * (dist * dist));
		// float atten = (CONSTANT + LINEAR * dist + QUADRATIC * (dist * dist));

		// vec3 diff = gCore.aLightCone[ i ].aColor.rgb * gCore.aLightCone[ i ].aColor.a * albedo.rgb * intensity * atten;
		// vec3 diff = gCore.aLightCone[ i ].aColor.rgb * gCore.aLightCone[ i ].aColor.a * intensity * atten;

		vec3 lightColor = gCore.aLightCone[ i ].aColor.rgb;
		float albedoLuminance = max( 0.4f, GetLuminance( albedo.rgb ) );
		lightColor = mix( lightColor, albedo.rgb * lightColor, albedoLuminance );

		vec3 diff = albedo.rgb * lightColor * gCore.aLightCone[ i ].aColor.a * intensity * atten;

		// shadow
		//if ( gCore.aLightCone[ i ].aShadow != -1 )
		//{
		//	mat4 depthBiasMVP = gBiasMat * gCore.aLightCone[ i ].aProjView;
		//	vec4 shadowCoord = depthBiasMVP * vec4( inPositionWorld, 1.0 );
		//
		//	// TODO: this could go out of bounds and lose the device
		//	// maybe add a check here to make sure we don't go out of bounds?
		//	float shadow = SampleShadowMapPCF( gCore.aLightCone[ i ].aShadow, shadowCoord.xyz / shadowCoord.w );
		//	diff *= shadow;
		//}

		outColor.rgb += diff;
	}
#endif

	// ----------------------------------------------------------------------------

	// add ambient occlusion (only one channel is needed here, so just use red)
    if ( mat.aoPower > 0.0 )
		outColor *= mix( 1, texture(texAO, fragTexCoord).r, mat.aoPower );

	// add emission
    if ( mat.emissivePower > 0.0 )
	    outColor.rgb += mix( vec3(0, 0, 0), texture(texEmissive, fragTexCoord).rgb, mat.emissivePower );
}

// look at this later for cubemaps:
// https://forum.processing.org/two/discussion/27871/use-samplercube-and-sampler2d-in-a-single-glsl-shader.html

