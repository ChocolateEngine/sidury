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


void main()
{
	// SurfaceDraw_t surface    = gSurfaceDraws[ push.aSurface ];
	Renderable_t renderable = gCore.aRenderables[ push.aRenderable ];

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

	// if ( gCore.aNumLightWorld == 0 )
		outColor = albedo;

#if 0
	// ----------------------------------------------------------------------------
	// Add World Lights

	for ( int i = 0; i < gCore.aNumLightWorld; i++ )
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
		if ( gLightsWorld[ i ].aShadow != -1 )
		{
			mat4 depthBiasMVP = gBiasMat * gLightsWorld[ i ].aProjView;
			// mat4 depthBiasMVP = gBiasMat * gViewport[ 0 ].aProjView;
			vec4 shadowCoord = depthBiasMVP * vec4( inPositionWorld, 1.0 );

			// float shadow = SampleShadowMapPCF( gLightsWorld[ i ].aShadow, shadowCoord.xyz / shadowCoord.w );
			float shadow = SampleShadowMapPCF( 0, shadowCoord.xyz / shadowCoord.w );
			diff *= shadow;
			// diff = shadow;
		}

		outColor.rgb += diff;
	}

	// ----------------------------------------------------------------------------
	// Add Point Lights

	for ( int i = 0; i < gCore.aNumLightPoint; i++ )
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

	// ----------------------------------------------------------------------------
	// Add Cone Lights

	#define CONSTANT 1
	#define LINEAR 1
	#define QUADRATIC 1

	for ( int i = 0; i < gCore.aNumLightCone; i++ )
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
			mat4 depthBiasMVP = gBiasMat * gLightsCone[ i ].aProjView;
			vec4 shadowCoord = depthBiasMVP * vec4( inPositionWorld, 1.0 );

			// TODO: this could go out of bounds and lose the device
			// maybe add a check here to make sure we don't go out of bounds?
			float shadow = SampleShadowMapPCF( gLightsCone[ i ].aShadow, shadowCoord.xyz / shadowCoord.w );
			diff *= shadow;
		}

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

