#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(set = 1, binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec3 inNormal;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragTexCoord;
layout(location = 2) out float lightIntensity;

// const vec3 DIRECTION_TO_LIGHT = normalize(vec3(1.0, 1.0, 1.0));
const vec3 DIRECTION_TO_LIGHT = normalize(vec3(1.0, 0.5, 1.5));

void main() {
	gl_Position = ubo.proj * ubo.view * ubo.model * vec4(inPosition, 1.0);
	
	vec3 normalWorldSpace = normalize(mat3(ubo.model) * inNormal);

	lightIntensity = max(dot(normalWorldSpace, DIRECTION_TO_LIGHT), 0.15);

    	fragColor = lightIntensity * inColor;
    	fragTexCoord = inTexCoord;
}

