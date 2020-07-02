#version 430 core
#include "lightning.gl"
#include "normals.gl"

out vec3 Color;
out float DiffuseLight;
in vec2 UV;

layout (binding = 0) uniform sampler2D DiffuseSpec;
layout (binding = 1) uniform sampler2D Normal;
layout (binding = 2) uniform sampler2D Depth;
layout (binding = 3) uniform sampler2DArrayShadow ShadowMapArrayPCF;
layout (binding = 4) uniform sampler2DArray ShadowMapArrayDepth;
layout (binding = 5) uniform sampler3D RandomRotations;

// position from depth
uniform mat4 InvViewProj;
// vied depth from clip depth
uniform float Near;


vec3 WsPosFromCsDepth(float csDepth, vec2 uv) {
	vec4 wsPos = InvViewProj * vec4(uv * 2 - 1, csDepth, 1);
	return wsPos.xyz / wsPos.w;
}

float VsDepthFromCsDepth(float clipSpaceDepth, float near) {
	return -near / clipSpaceDepth;
}

void main() {
	const vec4 diffuseSpec = texelFetch(DiffuseSpec, ivec2(gl_FragCoord.xy), 0);
	const vec3 colorDiffuse = diffuseSpec.xyz;
	const float colorSpecular = diffuseSpec.w;
	const float csDepth = texelFetch(Depth, ivec2(gl_FragCoord.xy), 0).r;
	const vec3 wsPos = WsPosFromCsDepth(csDepth, UV);
	const vec3 wsNormal = texelFetch(Normal, ivec2(gl_FragCoord.xy), 0).xyz * 2 - 1;
	const float vsDepth = VsDepthFromCsDepth(csDepth, Near);
	
	vec3 color = vec3(0);
	vec3 colorPureDiffuse = vec3(0);
	// dir light
	Foo tempSun = DirrLight(wsNormal, wsPos, vsDepth, ColorDirLight, WsDirLight, colorDiffuse, WidthLight, colorSpecular,
		ShadowMapArrayPCF, ShadowMapArrayDepth, RandomRotations);
	color += tempSun.color;
	colorPureDiffuse += tempSun.colorPureDiffuse;

	// point lights
    for(int i = 0; i < g_kNumPointLights; ++i) {
        Foo tempPoint = PointLight(wsNormal, wsPos, AWsPointLightPosition[i], APointLightColor[i], colorDiffuse, colorSpecular);
		color += tempPoint.color;
		colorPureDiffuse += tempPoint.colorPureDiffuse;
    }

	// ambient
	color += colorDiffuse * .1;
	Color = color;
	DiffuseLight = log(0.01+dot(colorPureDiffuse, vec3(0.2126, 0.7152, 0.0722)));
}