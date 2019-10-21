#version 420 core
out vec4 FragColor;

in vec2 UV;

layout(binding = 0) uniform sampler2D HDRColor;
layout(binding = 1) uniform sampler2D DiffuseLight;
layout(binding = 2) uniform sampler2DArrayShadow ShadowMapArray;
uniform float ManualExposure;
uniform bool Debug;
uniform uint IdxCascade;
uniform float LevelLastMipMap;

vec3 toneMap(vec3 x){
	return x / (x + 1);
};

float gammaCor(float x){
	if (x > 0.04045)
		return pow( ((x + 0.055)/1.055), 2.4 );
	else
		return x / 12.92;
}

vec3 gammaCor(vec3 rgb) {
	return vec3(gammaCor(rgb.r),
				gammaCor(rgb.g),
				gammaCor(rgb.b));
}
void main() {        
	if (!Debug) {
		vec3 hdrColor = texture(HDRColor, UV).rgb;
		// exposure:
		float luminance = exp(texture(DiffuseLight, UV).r);
		float avgLuminance = exp(textureLod(DiffuseLight, UV, LevelLastMipMap).r);
		hdrColor *= ManualExposure;
		hdrColor *= (luminance / avgLuminance);

		hdrColor = toneMap(hdrColor);
		hdrColor = gammaCor(hdrColor);
	
		FragColor = vec4(hdrColor, 1);
	} else {
		vec3 color = texture(ShadowMapArray, vec4(UV, IdxCascade, 0)).rrr;
		FragColor = vec4(color, 1);
	}
}

