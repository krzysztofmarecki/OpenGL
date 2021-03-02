#version 420 core
out vec4 outColor;

in vec2 UV;

layout(binding = 0) uniform sampler2D ColorHDR;
layout(binding = 1) uniform sampler2D DiffuseLight;
layout(binding = 2) uniform sampler2DArray ShadowMapArray;

uniform float Exposure;
uniform float LevelLastMipMap;

uniform bool ShowShadowMap;
uniform uint IdxCascade;
uniform bool ShowAO;


vec3 ToneMap(vec3 x) {
	return x / (x + 1);
};

void main() {
	if (ShowShadowMap) {
		const vec3 color = texture(ShadowMapArray, vec3(UV, IdxCascade)).rrr;
		outColor = vec4(color, 1);
	} else if (ShowAO) {
		const vec3 color = texture(ColorHDR, UV).rrr;
		outColor = vec4(color, 1);
	} else {
		
		vec3 colorHDR = texture(ColorHDR, UV).rgb;
		// exposure:
		const float luminance = exp(texture(DiffuseLight, UV).r);
		const float avgLuminance = exp(textureLod(DiffuseLight, UV, LevelLastMipMap).r);
		colorHDR *= Exposure;
		colorHDR *= (luminance / avgLuminance);

		colorHDR = ToneMap(colorHDR);
	
		outColor = vec4(colorHDR, 1);
	}
}

