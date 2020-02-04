#version 420 core
out vec4 outColor;

in vec2 UV;

layout(binding = 0) uniform sampler2D ColorHDR;
layout(binding = 1) uniform sampler2D DiffuseLight;
layout(binding = 2) uniform sampler2DArrayShadow ShadowMapArray;
uniform float Exposure;
uniform bool Debug;
uniform uint IdxCascade;
uniform float LevelLastMipMap;

vec3 ToneMap(vec3 x){
	return x / (x + 1);
};

float GammaCor(float x){
	if (x > 0.04045)
		return pow( ((x + 0.055)/1.055), 2.4 );
	else
		return x / 12.92;
}

vec3 GammaCor(vec3 rgb) {
	return vec3(GammaCor(rgb.r),
				GammaCor(rgb.g),
				GammaCor(rgb.b));
}
void main() {        
	if (!Debug) {
		vec3 colorHDR = texture(ColorHDR, UV).rgb;
		// exposure:
		const float luminance = exp(texture(DiffuseLight, UV).r);
		const float avgLuminance = exp(textureLod(DiffuseLight, UV, LevelLastMipMap).r);
		colorHDR *= Exposure;
		colorHDR *= (luminance / avgLuminance);

		colorHDR = ToneMap(colorHDR);
		colorHDR = GammaCor(colorHDR);
	
		outColor = vec4(colorHDR, 1);
	} else {
		const vec3 color = texture(ShadowMapArray, vec4(UV, IdxCascade, 0)).rrr;
		outColor = vec4(color, 1);
	}
}

