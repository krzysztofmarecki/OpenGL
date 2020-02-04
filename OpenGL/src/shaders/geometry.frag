#version 440 core
layout (location = 0) out vec3 outColor;
layout (location = 1) out float outDiffuseLight;

const int g_kNumCascades = 4;
const int g_kNumPointLights = 4;
in VS_OUT {
    vec3 WsPos;
    vec3 WsNormal;
	vec3 WsTangent;
    vec2 UV;
	float VsDepth;
} Input;

layout (binding = 0) uniform sampler2DArrayShadow ShadowMapArray;
layout (binding = 1) uniform sampler2D Diffuse;
layout (binding = 2) uniform sampler2D Specular;
layout (binding = 3) uniform sampler2D Normal;
#ifdef TRANSPARENCY
layout (binding = 4) uniform sampler2D Mask;
#endif
layout (binding = 5) uniform sampler3D RandomRotations;
// point lights
uniform vec3 AWsPointLightPosition[g_kNumPointLights];
uniform vec3 APointLightColor[g_kNumPointLights];
// dir light
uniform vec3 WsDirLight;
uniform vec3 ColorDirLight;
// CSM
uniform float AVsFarCascade[g_kNumCascades];
uniform mat4 ReferenceShadowMatrix;
uniform vec3 AOffsetCascade[g_kNumCascades];
uniform vec3 AScaleCascade[g_kNumCascades];
// Shadow Bias
uniform float Bias;
uniform float ScaleNormalOffsetBias;
// poisson disc PCF
uniform float SizeFilter;
uniform int NumDiscSamples;

uniform vec3 WsPosCamera;

uniform bool NormalMapping;

const int g_kSizeKernel = 5;
const float W[g_kSizeKernel][g_kSizeKernel] =
{
    { 0.0,0.5,1.0,0.5,0.0 },
    { 0.5,1.0,1.0,1.0,0.5 },
    { 1.0,1.0,1.0,1.0,1.0 },
    { 0.5,1.0,1.0,1.0,0.5 },
    { 0.0,0.5,1.0,0.5,0.0 }
};
// samples sorted ascending by distance from center (0, 0)
const vec2 SortedPoissonDisc[24] = {
	vec2(-0.0730308, -0.0837733),
	vec2(-0.170629, 0.341044),
	vec2(0.370098, 0.199377),
	vec2(0.281167, -0.34373),
	vec2(-0.170568, -0.530625),
	vec2(-0.564257, -0.168859),
	vec2(0.169286, 0.578539),
	vec2(0.598559, -0.123753),
	vec2(-0.611744, 0.186621),
	vec2(-0.118259, 0.696219),
	vec2(-0.520554, 0.510849),
	vec2(0.425581, -0.732536),
	vec2(0.705985, 0.517258),
	vec2(0.923276, 0.135228),
	vec2(0.924314, -0.167028),
	vec2(0.434187, 0.853633),
	vec2(0.066744, -0.955687),
	vec2(-0.880551, 0.386151),
	vec2(-0.974242, -0.051912),
	vec2(0.019013, 0.97937),
	vec2(0.806024, -0.558519),
	vec2(-0.865108, -0.467513),
	vec2(-0.391278, 0.903195),
	vec2(-0.582202, -0.799799),
};

// Blinn-Phong
vec3 Spec(vec3 wsNormal, vec3 wsPos, vec3 colorLight, vec3 wsDirLight) {
    const vec3 wsDirView = normalize(WsPosCamera - wsPos);
	const vec3 wsHalfway = normalize(wsDirLight + wsDirView);  
    const float spec = pow(max(dot(wsNormal, wsHalfway), 0.0), 64.0);
    return colorLight * spec;
}

float Attenuation(vec3 wsPosLight, vec3 wsPos) {
	const float distance = length(wsPosLight - wsPos);
    return  1.0 / (distance * distance);
}

struct Foo {
vec3 colorPureDiffuse;	// pure diffuse light without affecting surface texture
vec3 color;
};
Foo PointLight(vec3 wsNormal, vec3 wsPos, vec3 wsPosLight, vec3 colorLight, vec3 colorDiffuse, vec2 uv) {
	const vec3 wsDirLight = normalize(wsPosLight - wsPos);
	const float attenuation = Attenuation(wsPosLight, wsPos);

	const float nDotL = max(dot(wsNormal, wsDirLight), 0);

	const vec3 colorPureDiffuse = nDotL * colorLight * attenuation;
	const vec3 diffuse = colorPureDiffuse * colorDiffuse;
	const vec3 specular = Spec(wsNormal, wsPos, colorLight, wsDirLight) * attenuation * texture(Specular, uv).r;
	
    return Foo(colorPureDiffuse, diffuse + specular);
}

// functions with comments like below was rewritten to GLSL from https://github.com/TheRealMJP/Shadows/blob/master/Shadows/Mesh.hlsl

//-------------------------------------------------------------------------------------------------
// Samples the shadow map with a fixed-size PCF kernel optimized with GatherCmp. Uses code
// from "Fast Conventional Shadow Filtering" by Holger Gruen, in GPU Pro.
//-------------------------------------------------------------------------------------------------
float SampleShadowMapFixedSizePCF(vec3 uvz, uint idxCascade) {
	const vec2 sizeShadowMap = textureSize(ShadowMapArray, 0).xy;
    const float depth = uvz.z + Bias;
    vec2 tc = uvz.xy;

    vec4 s = vec4(0.0f);
    const vec2 stc = (sizeShadowMap * tc.xy) + vec2(0.5f, 0.5f);
    const vec2 tcs = floor(stc);
    vec2 fc;
    float w = 0.0f;

    fc.xy = stc - tcs;
	tc.xy = tcs / sizeShadowMap;
		
	for(int row = 0; row < g_kSizeKernel; ++row) {
		for(int col = 0; col < g_kSizeKernel; ++col)
			w += W[row][col];
	}

	const int KS_2 = g_kSizeKernel / 2;
    vec4 v1[KS_2 + 1];
    vec2 v0[KS_2 + 1];

    // -- loop over the rows
    for(int row = -KS_2; row <= KS_2; row += 2) {
		for(int col = -KS_2; col <= KS_2; col += 2) {
			float value = W[row + KS_2][col + KS_2];
            if(col > -KS_2)
				value += W[row + KS_2][col + KS_2 - 1];
			if(col < KS_2)
                value += W[row + KS_2][col + KS_2 + 1];
			if(row > -KS_2) {
				value += W[row + KS_2 - 1][col + KS_2];
				if(col < KS_2)
					value += W[row + KS_2 - 1][col + KS_2 + 1];
				if(col > -KS_2)
					value += W[row + KS_2 - 1][col + KS_2 - 1];
            } // if(row > -KS_2)
			if(value != 0.0f)
				v1[(col + KS_2) / 2] = textureGatherOffset(ShadowMapArray, vec3(tc.xy, idxCascade), depth, ivec2(col, row));
            else
				v1[(col + KS_2) / 2] = vec4(0.0f);
			
			if(col == -KS_2) {
				s.x += (1.0f - fc.y) * (v1[0].w * (W[row + KS_2][col + KS_2]
					- W[row + KS_2][col + KS_2] * fc.x)
                    + v1[0].z * (fc.x * (W[row + KS_2][col + KS_2]
					- W[row + KS_2][col + KS_2 + 1])
					+ W[row + KS_2][col + KS_2 + 1]));
				s.y += fc.y * (v1[0].x * (W[row + KS_2][col + KS_2]
				- W[row + KS_2][col + KS_2] * fc.x)
				+ v1[0].y * (fc.x * (W[row + KS_2][col + KS_2]
				- W[row + KS_2][col + KS_2 + 1])
				+ W[row + KS_2][col + KS_2 + 1]));
				if(row > -KS_2) {
					s.z += (1.0f - fc.y) * (v0[0].x * (W[row + KS_2 - 1][col + KS_2]
						- W[row + KS_2 - 1][col + KS_2] * fc.x)
						+ v0[0].y * (fc.x * (W[row + KS_2 - 1][col + KS_2]
						- W[row + KS_2 - 1][col + KS_2 + 1])
						+ W[row + KS_2 - 1][col + KS_2 + 1]));
					s.w += fc.y * (v1[0].w * (W[row + KS_2 - 1][col + KS_2]
						- W[row + KS_2 - 1][col + KS_2] * fc.x)
						+ v1[0].z * (fc.x * (W[row + KS_2 - 1][col + KS_2]
						- W[row + KS_2 - 1][col + KS_2 + 1])
						+ W[row + KS_2 - 1][col + KS_2 + 1]));
				} // if(row > -KS_2)
			} // if(col == -KS_2)
			else if(col == KS_2) {
				s.x += (1 - fc.y) * (v1[KS_2].w * (fc.x * (W[row + KS_2][col + KS_2 - 1]
					- W[row + KS_2][col + KS_2]) + W[row + KS_2][col + KS_2])
					+ v1[KS_2].z * fc.x * W[row + KS_2][col + KS_2]);
				s.y += fc.y * (v1[KS_2].x * (fc.x * (W[row + KS_2][col + KS_2 - 1]
					- W[row + KS_2][col + KS_2] ) + W[row + KS_2][col + KS_2])
					+ v1[KS_2].y * fc.x * W[row + KS_2][col + KS_2]);
				if(row > -KS_2) {
					s.z += (1 - fc.y) * (v0[KS_2].x * (fc.x * (W[row + KS_2 - 1][col + KS_2 - 1]
						- W[row + KS_2 - 1][col + KS_2])
						+ W[row + KS_2 - 1][col + KS_2])
						+ v0[KS_2].y * fc.x * W[row + KS_2 - 1][col + KS_2]);
					s.w += fc.y * (v1[KS_2].w * (fc.x * (W[row + KS_2 - 1][col + KS_2 - 1]
						- W[row + KS_2 - 1][col + KS_2])
						+ W[row + KS_2 - 1][col + KS_2])
						+ v1[KS_2].z * fc.x * W[row + KS_2 - 1][col + KS_2]);
				} // if(row > -KS_2)
			} // else if(col == KS_2)
			else {
				s.x += (1 - fc.y) * (v1[(col + KS_2) / 2].w * (fc.x * (W[row + KS_2][col + KS_2 - 1]
					- W[row + KS_2][col + KS_2 + 0] ) + W[row + KS_2][col + KS_2 + 0])
					+ v1[(col + KS_2) / 2].z * (fc.x * (W[row + KS_2][col + KS_2 - 0]
					- W[row + KS_2][col + KS_2 + 1]) + W[row + KS_2][col + KS_2 + 1]));
				s.y += fc.y * (v1[(col + KS_2) / 2].x * (fc.x * (W[row + KS_2][col + KS_2-1]
					- W[row + KS_2][col + KS_2 + 0]) + W[row + KS_2][col + KS_2 + 0])
					+ v1[(col + KS_2) / 2].y * (fc.x * (W[row + KS_2][col + KS_2 - 0]
					- W[row + KS_2][col + KS_2 + 1]) + W[row + KS_2][col + KS_2 + 1]));
				if(row > -KS_2) {
					s.z += (1 - fc.y) * (v0[(col + KS_2) / 2].x * (fc.x * (W[row + KS_2 - 1][col + KS_2 - 1]
						- W[row + KS_2 - 1][col + KS_2 + 0]) + W[row + KS_2 - 1][col + KS_2 + 0])
						+ v0[(col + KS_2) / 2].y * (fc.x * (W[row + KS_2 - 1][col + KS_2 - 0]
						- W[row + KS_2 - 1][col + KS_2 + 1]) + W[row + KS_2 - 1][col + KS_2 + 1]));
					s.w += fc.y * (v1[(col + KS_2) / 2].w * (fc.x * (W[row + KS_2 - 1][col + KS_2 - 1]
						- W[row + KS_2 - 1][col + KS_2 + 0]) + W[row + KS_2 - 1][col + KS_2 + 0])
						+ v1[(col + KS_2) / 2].z * (fc.x * (W[row + KS_2 - 1][col + KS_2 - 0]
						- W[row + KS_2 - 1][col + KS_2 + 1]) + W[row + KS_2 - 1][col + KS_2 + 1]));
				} // if(row > -KS_2)
			} // else
			if(row != KS_2)
				v0[(col + KS_2) / 2] = v1[(col + KS_2) / 2].xy;
		} // for(int col = -KS_2; col <= KS_2; col += 2)
	} // for(int row = -KS_2; row <= KS_2; row += 2)

	return dot(s, vec4(1.0f)) / w;
}

//--------------------------------------------------------------------------------------
// Samples the shadow map using a PCF kernel made up from random points on a disc
//--------------------------------------------------------------------------------------
float SampleShadowMapRandomDiscPCF(vec3 uvz, uint idxCascade, vec3 wsPos) {
	const vec2 sizeFilter = SizeFilter.xx * abs(AScaleCascade[idxCascade].xy);
	const vec2 sizeShadowMap = textureSize(ShadowMapArray, 0).xy;
    const float depth = uvz.z + Bias;

	float result;
	if(sizeFilter.x > 1 || sizeFilter.y > 1) {
		// Get a value to randomly rotate the kernel by
		const ivec3 sizeRandomRotations = textureSize(RandomRotations, 0);
		const ivec3 posSample = ivec3(wsPos * 32) % sizeRandomRotations; // multiply so shadow are less blocky (and more noisy)
		const float radAngle = texelFetch(RandomRotations, posSample, 0).x;
		const mat2x2 randomRotationMatrix = mat2x2(vec2(cos(radAngle), -sin(radAngle)),
												   vec2(sin(radAngle),  cos(radAngle)));
		const vec2 scaleSample = 0.5 * sizeFilter / sizeShadowMap;

		float sum = 0.0f;
		for(int i = 0; i < NumDiscSamples; ++i) {
			const vec2 offsetSample = (randomRotationMatrix * SortedPoissonDisc[i]) * scaleSample;
			const vec2 posSample = uvz.xy + offsetSample;
			sum += texture(ShadowMapArray, vec4(posSample, idxCascade, depth));
        }
		result = sum / NumDiscSamples;
    } else {
		result = texture(ShadowMapArray, vec4(uvz.xy, idxCascade, depth));
	}
	
	return result;
}
//-------------------------------------------------------------------------------------------------
// Samples the appropriate shadow map cascade
//-------------------------------------------------------------------------------------------------
float SampleShadowCascade(vec3 uvz, uint idxCascade, vec3 wsPos)
{
    uvz += AOffsetCascade[idxCascade].xyz;
    uvz *= AScaleCascade[idxCascade].xyz;

    //return SampleShadowMapFixedSizePCF(uvz, idxCascade);
	return SampleShadowMapRandomDiscPCF(uvz, idxCascade, wsPos);
}
//-------------------------------------------------------------------------------------------------
// Calculates the offset to use for sampling the shadow map, based on the surface normal
//-------------------------------------------------------------------------------------------------
vec3 GetWsShadowPosOffset(float nDotL, vec3 wsNormal)
{
    const float sizeTexel = 2.0f / textureSize(ShadowMapArray, 0).x;
    const float nmlOffsetScale = 1.0f - nDotL;
    return sizeTexel * ScaleNormalOffsetBias * nmlOffsetScale * wsNormal;
}

float ShadowVisibility(vec3 wsPos, float vsDepth, float nDotL, vec3 wsNormal)
{
    const vec3 projectionShadowPos = (ReferenceShadowMatrix * vec4(wsPos, 1.0f)).xyz;
	
	int idxCascade = g_kNumCascades-1;
	// projection base cascade selection
	for(int i = g_kNumCascades - 1; i >= 0; --i) {
		// Select based on whether or not the pixel is inside the projection
		vec2 uv = projectionShadowPos.xy + AOffsetCascade[i].xy;
		uv *= AScaleCascade[i].xy;
		uv = abs(uv - 0.5f);
		if(uv.x <= 0.5f && uv.y <= 0.5f)
			idxCascade = i;
	}

    const vec3 wsPosOffset = GetWsShadowPosOffset(nDotL, wsNormal);
	const vec3 uvz = (ReferenceShadowMatrix * vec4(wsPos + wsPosOffset, 1.0f)).xyz;
	float shadowVisibility = SampleShadowCascade(uvz, idxCascade, wsPos);
	
	// Sample the next cascade, and blend between the two results to smooth the transition
	const float kBlendThreshold = 0.2f;

	const float vsNextSplit = AVsFarCascade[idxCascade];
	const float vsSizeSplit = idxCascade == 0 ? vsNextSplit : vsNextSplit - AVsFarCascade[idxCascade - 1];
	float fadeFactor = (vsNextSplit - vsDepth) / vsSizeSplit;
	
	vec3 posCascade = projectionShadowPos + AOffsetCascade[idxCascade].xyz;
	posCascade *= AScaleCascade[idxCascade].xyz;
	posCascade = abs(posCascade * 2.0f - 1.0f);
	const float distToEdge = 1.0f - max(max(posCascade.x, posCascade.y), posCascade.z);
	fadeFactor = max(distToEdge, fadeFactor);
	
	if(fadeFactor <= kBlendThreshold && idxCascade != (g_kNumCascades - 1)) {
		const float nextSplitShadowVisibility = SampleShadowCascade(uvz, idxCascade + 1, wsPos);
		const float mixAmt = smoothstep(0.0f, kBlendThreshold, fadeFactor);
		shadowVisibility = mix(nextSplitShadowVisibility, shadowVisibility, mixAmt);
    }
	return shadowVisibility;
}

Foo DirrLight(vec3 wsNormal, vec3 wsPos, float vdDepth, vec3 colorLight, vec3 wsDirLight, vec3 diffColor, vec2 uv) {
	const float nDotL = max(dot(wsNormal, wsDirLight), 0);
	const vec3 colorPureDiffuse = nDotL * colorLight;
	const vec3 diffuse = colorPureDiffuse * diffColor;
	const vec3 specular = Spec(wsNormal, wsPos, colorLight, wsDirLight) * texture(Specular, uv).r;
	
	const float lightning = ShadowVisibility(wsPos, vdDepth, nDotL, wsNormal);
	return Foo(lightning * colorPureDiffuse, lightning * (diffuse + specular));
}

void main() {
	#ifdef TRANSPARENCY
	if (texture(Mask, Input.UV).a < .5)
		discard;
	#endif
	const float vsDepth = Input.VsDepth;
	
	const vec3 colorDiffuse = texture(Diffuse, Input.UV).rgb;
	const vec3 N = normalize(Input.WsNormal);
	vec3 T = normalize(Input.WsTangent);
	T = normalize(T - dot(T, N) * N); // re-orthogonalize T with respect to N
	const vec3 B = cross(N,T);
	
	const mat3 TBN = mat3(T,B, N);

	vec3 wsNormal;
	if (NormalMapping)
		wsNormal = TBN * normalize(texture(Normal, Input.UV).rgb * 2 - 1);
	else
		wsNormal = N;
	// you may want to pass vertex normal for normal offset bias even if normal mapping is enabled

	vec3 color = vec3(0);
	vec3 colorPureDiffuse = vec3(0);
	// dir light
	Foo tempSun = DirrLight(wsNormal, Input.WsPos, vsDepth, ColorDirLight, WsDirLight, colorDiffuse, Input.UV);
	color += tempSun.color;
	colorPureDiffuse += tempSun.colorPureDiffuse;

	// point lights
    for(int i = 0; i < g_kNumPointLights; ++i){
        Foo tempPoint = PointLight(wsNormal, Input.WsPos, AWsPointLightPosition[i], APointLightColor[i], colorDiffuse, Input.UV);
		color += tempPoint.color;
		colorPureDiffuse += tempPoint.colorPureDiffuse;
    }

	// ambient
	color += colorDiffuse * .3;

	outDiffuseLight = log(0.01+dot(colorPureDiffuse, vec3(0.2126, 0.7152, 0.0722)));
	outColor = color;
}

