#version 420 core
#include "depth.gl"
out float Ao;

in vec2 UV;

layout (binding = 0) uniform sampler2D Ssao;
layout (binding = 1) uniform sampler2D Depth;

uniform float Near;

void main() {	
	vec4[4] ao4s;
	vec4[4] depth4s;
	ao4s[0] = textureGather(Ssao, UV);
	ao4s[1] = textureGatherOffset(Ssao, UV, ivec2(0,-2));
	ao4s[2] = textureGatherOffset(Ssao, UV, ivec2(-2,0));
	ao4s[3] = textureGatherOffset(Ssao, UV, ivec2(-2,-2));
	depth4s[0] = textureGather(Depth, UV);
	depth4s[1] = textureGatherOffset(Depth, UV, ivec2(0,-2));
	depth4s[2] = textureGatherOffset(Depth, UV, ivec2(-2,0));
	depth4s[3] = textureGatherOffset(Depth, UV, ivec2(-2,-2));
	
	const float depthCurrent = VsDepthFromCsDepth(depth4s[0].w, Near);

	float ao = 0;
	float weight = 0;
	const float threshold = abs(0.1 * depthCurrent);
	for (int i = 0; i < 4; i++) {
		for (int j = 0; j < 4; j++) {
			const float depthDiff = abs(VsDepthFromCsDepth(depth4s[i][j], Near) - depthCurrent);
			if (depthDiff < threshold) {
				const float weightCurrent = 1. - clamp(10. * depthDiff / threshold, 0., 1.);
				ao += ao4s[i][j] * weightCurrent;
				weight += weightCurrent;
			}
		}
	}
	Ao = ao / weight;
}
