#version 430 core

out float Shadow;

in vec2 UV;

layout (binding = 0) uniform sampler2D Normal;
layout (binding = 1) uniform sampler2D Depth;
layout (binding = 2) uniform sampler2DArrayShadow ShadowMapArrayPCF;
layout (binding = 3) uniform sampler2DArray ShadowMapArrayDepth;
layout (binding = 4) uniform sampler2D Noise;

#include "shadows.gl"
#include "normals.gl"
#include "depth.gl"

// position from depth
uniform mat4 InvViewProj;
// vied depth from clip depth
uniform float Near;

uniform vec3 WsDirLight;

void main() {
	const float csDepth = texelFetch(Depth, ivec2(gl_FragCoord.xy), 0).r;
	const vec3 wsPos = WsPosFromCsDepth(csDepth, UV, InvViewProj);
	const vec3 wsNormal = texelFetch(Normal, ivec2(gl_FragCoord.xy), 0).xyz * 2 - 1;
	const float vsDepth = VsDepthFromCsDepth(csDepth, Near);
	
	const float nDotL = max(dot(wsNormal, WsDirLight), 0);
	Shadow = ShadowVisibility(wsPos, vsDepth, nDotL, wsNormal, WidthLight, ShadowMapArrayPCF, ShadowMapArrayDepth, Noise);
}