#version 430 core
#include "lightning.gl"
#include "normals.gl"
#include "gamma.gl"
layout (location = 0) out vec3 outColor;
layout (location = 1) out float outDiffuseLight;
layout (location = 2) out vec2 outVelocity;
in VS_OUT {
	vec3 PosCur;
	vec3 PosPrev;
    vec3 WsPos;
    vec3 WsNormal;
	vec3 WsTangent;
    vec2 UV;
	float VsDepth;
} Input;

layout (binding = 0) uniform sampler2DArrayShadow ShadowMapArrayPCF;
layout (binding = 1) uniform sampler2D Diffuse;
layout (binding = 2) uniform sampler2D Specular;
layout (binding = 3) uniform sampler2D Normal;
#ifdef TRANSPARENCY
layout (binding = 4) uniform sampler2D Mask;
#endif
layout (binding = 5) uniform sampler3D RandomRotations;
layout (binding = 6) uniform sampler2DArray ShadowMapArrayDepth;

void main() {
	#ifdef TRANSPARENCY
	if (texture(Mask, Input.UV).a < .5)
		discard;
	#endif
	const float vsDepth = Input.VsDepth;
	
	const vec3 colorDiffuse = LinearFromGamma(texture(Diffuse, Input.UV).rgb);
	const float colorSpecular = texture(Specular, Input.UV).r;
	const vec3 wsNormal = GetWsNormal(Input.WsNormal, Input.WsTangent, texture(Normal, Input.UV).rgb);
	// you may want to pass vertex normal for normal offset bias even if normal mapping is enabled

	vec3 color = vec3(0);
	vec3 colorPureDiffuse = vec3(0);
	// dir light
	Foo tempSun = DirrLight(wsNormal, Input.WsPos, vsDepth, ColorDirLight, WsDirLight, colorDiffuse, WidthLight, colorSpecular,
		ShadowMapArrayPCF, ShadowMapArrayDepth, RandomRotations);
	color += tempSun.color;
	colorPureDiffuse += tempSun.colorPureDiffuse;

	// point lights
    for(int i = 0; i < g_kNumPointLights; ++i) {
        Foo tempPoint = PointLight(wsNormal, Input.WsPos, AWsPointLightPosition[i], APointLightColor[i], colorDiffuse, colorSpecular);
		color += tempPoint.color;
		colorPureDiffuse += tempPoint.colorPureDiffuse;
    }

	// ambient
	color += colorDiffuse * .1;

	outDiffuseLight = log(0.01+dot(colorPureDiffuse, vec3(0.2126, 0.7152, 0.0722)));
	outColor = color;
	outVelocity = (Input.PosCur.xy / Input.PosCur.z) - (Input.PosPrev.xy / Input.PosPrev.z);
}

