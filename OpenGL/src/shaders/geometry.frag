#version 420 core
#include "normals.gl"
layout (location = 0) out vec4 outDiffuseSpec;
layout (location = 1) out vec3 outNormal;

in VS_OUT {
    vec3 WsNormal;
	vec3 WsTangent;
    vec2 UV;
} Input;

layout (binding = 1) uniform sampler2D Diffuse;
layout (binding = 2) uniform sampler2D Specular;
layout (binding = 3) uniform sampler2D Normal;
#ifdef TRANSPARENCY
layout (binding = 4) uniform sampler2D Mask;
#endif

void main() {
	#ifdef TRANSPARENCY
	if (texture(Mask, Input.UV).a < .5)
		discard;
	#endif
	
	const vec3 colorDiffuse = texture(Diffuse, Input.UV).rgb;
	const float colorSpecular = texture(Specular, Input.UV).r;
	const vec3 wsNormal = GetWsNormal(Input.WsNormal, Input.WsTangent, texture(Normal, Input.UV).rgb);
	outDiffuseSpec = vec4(colorDiffuse, colorSpecular);
	outNormal = wsNormal * 0.5 + 0.5;
}

