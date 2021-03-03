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

uniform vec4 ParamsLottes;
uniform float WhitePoint;
uniform float CrossTalkCoefficient;

float Lottes(float x, vec4 abcd, float whitePoint) {
	const float a = abcd.x;
	const float b = abcd.y;
	const float c = abcd.z;
	const float d = abcd.w;

	x = min(x, whitePoint);
	const float z = pow(x, a);
	return z / (pow(z, d) * b + c);
}

vec3 Lottes(vec3 rgb, vec4 abcd, float whitePoint) {
	return vec3(Lottes(rgb.r, abcd, whitePoint),
				Lottes(rgb.g, abcd, whitePoint),
				Lottes(rgb.b, abcd, whitePoint));
}

vec3 LottesCrossTalk(vec3 rgb, vec4 abcd, float crossTalkCoefficient, float whitePoint) {
	// max is more consistent than luminance across various chrominances
	// f.e. with luminance, transition to white on blue curtain is too small to hide hue shift and is too small per se
	// max is not perfect though, green gets much less white than red and blue
	const float m = max(rgb.r, max(rgb.g, rgb.b));
	const vec3 toneMapped = vec3(Lottes(rgb.r, abcd, whitePoint),
								 Lottes(rgb.g, abcd, whitePoint),
								 Lottes(rgb.b, abcd, whitePoint));
	const float t = m * m / (crossTalkCoefficient + m); // reaches 1 at white point
	// not lerping with vec3(1), because that way it's more subtle on lower/medium values
	return mix(toneMapped, Lottes(m, abcd, whitePoint).rrr, t);
}

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

		colorHDR = LottesCrossTalk(colorHDR, ParamsLottes, CrossTalkCoefficient, WhitePoint);

		outColor = vec4(colorHDR, 1);
	}
}
