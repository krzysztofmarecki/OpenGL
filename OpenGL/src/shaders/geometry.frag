#version 420 core
#include "normals.gl"
#include "gamma.gl"
layout (location = 0) out vec4 outDiffuseSpec;
layout (location = 1) out vec3 outNormal;
layout (location = 2) out vec2 outVelocity;
in VS_OUT {
	vec3 PosCur;
	vec3 PosPrev;
    vec3 WsNormal;
	vec3 WsTangent;
    vec2 UV;
} Input;

layout (binding = 1) uniform sampler2D Diffuse;
layout (binding = 2) uniform sampler2D Specular;
layout (binding = 3) uniform sampler2D Normal;
#ifdef ALPHA_MASKED
layout (binding = 4) uniform sampler2D Mask;
#endif

void main() {
	#ifdef ALPHA_MASKED
	if (texture(Mask, Input.UV).a < .5)
		discard;
	#endif
	
	const vec3 colorDiffuse = LinearFromGamma(texture(Diffuse, Input.UV).rgb);
	const float colorSpecular = texture(Specular, Input.UV).r;
	const vec3 wsNormal = GetWsNormal(Input.WsNormal, Input.WsTangent, texture(Normal, Input.UV).rgb);
	outDiffuseSpec = vec4(colorDiffuse, colorSpecular);
	outNormal = wsNormal * 0.5 + 0.5;
	outVelocity = (Input.PosCur.xy / Input.PosCur.z) - (Input.PosPrev.xy / Input.PosPrev.z);
}

