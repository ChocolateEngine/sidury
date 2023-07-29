#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : enable

layout(push_constant) uniform Push
{
    vec3 viewPos;
    int debugView;
    int position;
    int normal;
    int diffuse;
    int ao;
    int emissive;
} push;

// RENAME ME TO deferred_composition

// Lighting Info

#define ELightType_Point 0
#define ELightType_Ortho 1

layout(set = 0, binding = 0) uniform sampler2D[] texSamplers;

// projView info
// layout(set = 1, binding = 0) uniform UBO_ProjView
// {
// 	mat4 projView;
// } projView[];

// TODO: THIS SHOULD NOT BE VARIABLE
layout(set = 1, binding = 0) uniform UBO_LightInfo
{
	int countWorld;
	int countPoint;
	int countCone;
	int countCapsule;
} gLightInfoTmp[];

#define gLightInfo gLightInfoTmp[0]

// TODO: THIS SHOULD NOT BE VARIABLE
layout(set = 2, binding = 0) uniform UBO_WorldLight
{
    vec3 color;
    vec3 dir;
} gWorldLightTmp[];

#define gWorldLight gWorldLightTmp[0]

// ALSO SHOULD NOT BE VARIABLE

// layout(set = 4, binding = 0) uniform UBO_Light
// {
//     vec3  pos;
//     vec4  color;
//     float radius;
//     int   type;
// } gLights[];

layout(location = 0) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

#define texPosition texSamplers[push.position]
#define texNormal   texSamplers[push.normal]
#define texDiffuse  texSamplers[push.diffuse]
#define texAO       texSamplers[push.ao]
#define texEmissive texSamplers[push.emissive]

// const vec3 DIRECTION_TO_LIGHT = normalize(vec3(1.0, 0.5, 1.5));

void main()
{
    // Get G-Buffer values
	vec3  fragPos  = texture( texPosition, fragTexCoord ).rgb;
	vec3  normal   = texture( texNormal, fragTexCoord ).rgb;
	vec4  diffuse  = texture( texDiffuse, fragTexCoord );
	// float ao       = texture( texAO, fragTexCoord ).r;
	// vec4  emissive = texture( texEmissive, fragTexCoord );

	// Render-target composition

	// Debug display
	if ( push.debugView > 0 && push.debugView != 7 )
    {
		switch ( push.debugView )
        {
			case 1:
				outColor.rgb = fragPos;
				break;
			case 2:
				outColor.rgb = normal;
				break;
			// default:
			case 3: 
				outColor.rgb = diffuse.rgb;
				break;
			case 4:
				outColor.rgb = texture( texAO, fragTexCoord ).rrr;
				break;
			case 5:
				outColor.rgb = texture( texEmissive, fragTexCoord ).rgb;
				break;
			case 6:
				outColor.rgb = diffuse.aaa;
				break;
		}
		outColor.a = 1.0;
		return;
	}

	// Ambient part
	vec3 fragcolor = diffuse.rgb;

    // -------------------------------------------
    // WORLD LIGHT

	{
		// Viewer to fragment
		vec3 V = push.viewPos.xyz - fragPos;
		V = normalize(V);

		// Diffuse part
		vec3 N = normalize( normal );

		float intensity = max( dot( N, gWorldLight.dir ), 0.0 );

		// fragcolor = max( intensity * diffuse.rgb, 0.15 );
		fragcolor = max( intensity, 0.15 ) * diffuse.rgb;

		// vec3 worldLightDir = normalize( gWorldLight.dir );
		// float lightIntensity = max( dot(normal, worldLightDir), 0.15 );

		// fragcolor *= diffuse.rgb * Nmax;

		// float lightIntensity = 0.15;
		// fragcolor *= lightIntensity;
	}

    // -------------------------------------------
    // SCENE LIGHTS

	#define lightCount 1
	#define RADIUS 100
    // #define LIGHT_POS vec3(639.010864, 435.231171, 373.506104)
    #define LIGHT_POS push.viewPos.xyz
	#define COLOR vec3(0.5, 100, 100)

	if ( push.debugView != 7 )
	{
		for(int i = 0; i < lightCount; ++i)
		{
			// Vector to light
			vec3 L = LIGHT_POS - fragPos;
			// Distance from light to fragment position
			float dist = length(L);

			// Viewer to fragment
			vec3 V = push.viewPos.xyz - fragPos;
			V = normalize(V);
			
			//if(dist < ubo.lights[i].radius)
			{
				// Light to fragment
				L = normalize(L);

				// Attenuation
				float atten = RADIUS / (pow(dist, 2.0) + 1.0);

				// Diffuse part
				vec3 N = normalize(normal);
				float NdotL = max(0.0, dot(N, L));
				vec3 diff = COLOR * diffuse.rgb * NdotL * atten;
				// vec3 diff = COLOR * NdotL * atten;
				// vec3 diff = vec3(NdotL, NdotL, NdotL);
				// vec3 diff = vec3(atten, atten, atten);
				// vec3 diff = COLOR * diffuse.rgb * atten;

				// Specular part
				// Specular map values are stored in alpha of diffuse basic3d
				// vec3 R = reflect(-L, N);
				// float NdotR = max(0.0, dot(R, V));
				// vec3 spec = ubo.lights[i].color * diffuse.a * pow(NdotR, 16.0) * atten;

				// fragcolor += diff + spec;
				fragcolor += diff;
			}
		}
	}

    outColor = vec4( fragcolor, 1.0 );

	// add ambient occlusion (only one channel is needed here, so just use red)
    outColor.rgb *= texture(texAO, fragTexCoord).r;

	// add emission
    outColor.rgb += texture(texEmissive, fragTexCoord).rgb;
}

