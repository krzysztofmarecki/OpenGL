#version 420 core
#include "depth.gl"
out float Ao;

in vec2 UV;

layout (binding = 0) uniform sampler2D Ssao;
layout (binding = 1) uniform sampler2D SsaoAcc;
layout (binding = 2) uniform sampler2D Velocity;
layout (binding = 3) uniform sampler2D DepthCurr;
layout (binding = 4) uniform sampler2D DepthPrev;

uniform float Near;
uniform float RateOfChange;
uniform vec2 Scaling;

void main() {
	const float ao		= texture(Ssao, UV).x;
	const vec2 velocity	= texture(Velocity, UV.xy).xy;

	// bilateral reprojection
	// https://www.khronos.org/registry/OpenGL/extensions/ARB/ARB_texture_gather.txt
	// X Y
	// W Z
	// calculate weight's for top left pixel (bilinearWeights[0]), then propagate for rest
	const vec4 depthPrev4 = textureGather(DepthPrev, UV-velocity);
	const vec4 aoAcc4	  = textureGather(SsaoAcc, UV-velocity);

	// small distortions are still visible on mouse movement (tested on Kepler)
	const vec2 pixelUVPrev = (UV - velocity) * Scaling.xy - vec2(0.5) + vec2(1./512);
	const float weightX = 1 - fract(pixelUVPrev.x);
	const float weightY = fract(pixelUVPrev.y);

	float[4] bilinearWeights;
	bilinearWeights[0] = weightX	    * weightY;		  // X
	bilinearWeights[1] = (1. - weightX) * weightY;		  // Y
	bilinearWeights[2] = (1. - weightX) * (1. - weightY); // Z
	bilinearWeights[3] = weightX	    * (1. - weightY); // W

	const float depthCurr = texture(DepthCurr, UV).x;
	const float vsDepthCurr = VsDepthFromCsDepth(depthCurr, Near);
	float weight = 0;
	float aoAccWeighted = 0;
	for (int i = 0; i < 4; i++) {
		const float vsDepthPrev = VsDepthFromCsDepth(depthPrev4[i], Near);
		// too agresive bilateral results in floating AO under WSAD camera motion
		// currently don't eliminate halo in 100%
		const float bilateralWeight = clamp(1.0 + 0.1 * (vsDepthCurr - vsDepthPrev), 0.01, 1.0);
		weight		  += bilinearWeights[i] * bilateralWeight;
		aoAccWeighted += bilinearWeights[i] * bilateralWeight * aoAcc4[i];
	}
	aoAccWeighted /= weight;

	float rateOfChange = RateOfChange;
	const vec2 uvPrevDistanceToMiddle = abs((UV - velocity) - vec2(0.5));
	if (uvPrevDistanceToMiddle.x > 0.5 || uvPrevDistanceToMiddle.y > 0.5)
		rateOfChange = 1;

	float vsDepthPrev;
	if (fract(pixelUVPrev.x) > 0.5) {   // pointing at Y or Z
		if (fract(pixelUVPrev.y) > 0.5) // pointing at X or Y
			vsDepthPrev = depthPrev4.y;
		else
			vsDepthPrev = depthPrev4.z;
	} else {							// pointing at X or W
		if (fract(pixelUVPrev.y) > 0.5) // pointing at X or Y
			vsDepthPrev = depthPrev4.x;
		else
			vsDepthPrev = depthPrev4.w;
	}
	vsDepthPrev = VsDepthFromCsDepth(vsDepthPrev, Near);

	// discard history if current fragment was occluded in previous frame
	// also helps with ghosting
	if ((-vsDepthCurr * 0.9) > -vsDepthPrev)
		rateOfChange = 1;

	Ao = mix(aoAccWeighted, ao, rateOfChange);
}
