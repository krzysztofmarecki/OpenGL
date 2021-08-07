#version 430 core

out vec3 Color;
out float DiffuseLight;
in vec2 UV;

layout (binding = 0) uniform sampler2D DiffuseSpec;
layout (binding = 1) uniform sampler2D Normal;
layout (binding = 2) uniform sampler2D Depth;
layout (binding = 3) uniform sampler2D ShadowDeffered;
layout (binding = 4) uniform sampler2D GTAO;



#include "lightning.gl"
#include "normals.gl"
#include "depth.gl"



// position from depth
uniform mat4 InvViewProj;
// vied depth from clip depth
uniform float Near;

uniform bool EnableAO;

vec3 MultiBounce(float gtao, vec3 albedo)
{
	vec3 a = 2 * albedo - 0.33;
	vec3 b = -4.8 * albedo + 0.64;
	vec3 c = 2.75 * albedo + 0.69;
	return max(gtao.xxx, ((gtao * a + b) * gtao + c) * gtao);
}

void main() {
	const vec4 diffuseSpec = texelFetch(DiffuseSpec, ivec2(gl_FragCoord.xy), 0);
	const vec3 colorDiffuse = diffuseSpec.xyz;
	const float colorSpecular = diffuseSpec.w;
	const float csDepth = texelFetch(Depth, ivec2(gl_FragCoord.xy), 0).r;
	const vec3 wsPos = WsPosFromCsDepth(csDepth, UV, InvViewProj);
	const vec3 wsNormal = texelFetch(Normal, ivec2(gl_FragCoord.xy), 0).xyz * 2 - 1;

	vec4 shadow4 = textureGather(ShadowDeffered, UV);
	vec4 depth4  = textureGather(Depth, UV);
	for (int i = 0; i < 4; i++)
		depth4[i] = VsDepthFromCsDepth(depth4[i], Near);
	
	const float vsDepth = depth4[3];

	float shadowAcc = shadow4[3];
	float weightAcc = 1;
	for (int i = 0; i < 3; i++) {
		float weight = clamp(1.0 - abs(depth4[i] - vsDepth), 0.0, 1.0);
		shadowAcc += shadow4[i] * weight;
		weightAcc += weight;
	}
	shadowAcc /= weightAcc;


	vec3 color = vec3(0);
	vec3 colorPureDiffuse = vec3(0);

	// dir light
	Foo tempSun = DirrLight(wsNormal, wsPos, vsDepth, ColorDirLight, WsDirLight, colorDiffuse, WidthLight, colorSpecular,
		shadowAcc);
	color += tempSun.color;
	colorPureDiffuse += tempSun.colorPureDiffuse;

	// ambient + ambient occlusion
	vec3 ao = vec3(1);
	if (EnableAO)
		ao = MultiBounce(texture(GTAO, UV).r, colorDiffuse);

	color += colorDiffuse * 0.1 * ao;
	colorPureDiffuse += colorDiffuse * 0.1 * ao;
	

	Color = color;
	DiffuseLight = log(0.01+dot(colorPureDiffuse, vec3(0.2126, 0.7152, 0.0722)));
}