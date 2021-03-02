#version 420 core

out float Depth;
out vec2 Velocity;
in vec2 UV;

layout (binding = 0) uniform sampler2D FullResDepth;
layout (binding = 1) uniform sampler2D FullResVelocity;

void main() {
	const vec4 depth4 = textureGather(FullResDepth, UV);
	const vec4 velX4  = textureGather(FullResVelocity, UV, 0);
	const vec4 velY4  = textureGather(FullResVelocity, UV, 1);
	float depth = depth4[0];
	vec2 velocity = vec2(velX4[0], velY4[0]);
	for (int i = 1; i < 4; i++) {
		// taking further depth, because for GTAO:
		// reduces halo
		// decrease AO casted by thin objects
		if (depth4[i] < depth) {
			depth = depth4[i];
			velocity = vec2(velX4[i], velY4[i]);
		}
	}

	Depth = depth;
	Velocity = velocity;
}