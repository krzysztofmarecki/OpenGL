#version 440 core
layout (location = 0) out vec3 outFragColor;
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
// point lights
uniform vec3 AWsPointLightPosition[g_kNumPointLights];
uniform vec3 APointLightColor[g_kNumPointLights];
// dir light
uniform vec3 WsLightDir;
uniform vec3 DirLightColor;
// CSM
uniform float AVsFarCascade[g_kNumCascades];
uniform mat4 ReferenceShadowMatrix;
uniform vec3 AOffsetCascade[g_kNumCascades];
uniform vec3 AScaleCascade[g_kNumCascades];
// Shadow Bias
uniform float Bias;
uniform float ScaleNormalOffsetBias;

uniform vec3 ViewPos;

uniform bool NormalMapping;

const int g_kKernelSize = 5;
const float W[g_kKernelSize][g_kKernelSize] =
{
    { 0.0,0.5,1.0,0.5,0.0 },
    { 0.5,1.0,1.0,1.0,0.5 },
    { 1.0,1.0,1.0,1.0,1.0 },
    { 0.5,1.0,1.0,1.0,0.5 },
    { 0.0,0.5,1.0,0.5,0.0 }
};

// Blinn-Phong
vec3 Spec(vec3 normal, vec3 fragPos, vec3 lightColor, vec3 lightDir) {
    const vec3 viewDir = normalize(ViewPos - fragPos);
	const vec3 halfwayDir = normalize(lightDir + viewDir);  
    const float spec = pow(max(dot(normal, halfwayDir), 0.0), 64.0);
    return lightColor * spec;
}

float Attenuation(vec3 wsLightPos, vec3 wsPos) {
	const float distance = length(wsLightPos - wsPos);
    return  1.0 / (distance * distance);
}

struct DiffColor{
vec3 diff;	// pure diffuse light without affecting surface texture
vec3 color;
};
DiffColor PointLight(vec3 wsNormal, vec3 wsPos, vec3 wsLightPos, vec3 lightColor, vec3 diffColor, vec2 uv) {
	const vec3 wsLightDir = normalize(wsLightPos - wsPos);
	const float attenuation = Attenuation(wsLightPos, wsPos);

	const float nDotL = max(dot(wsNormal, wsLightDir), 0);

	const vec3 diff = nDotL * lightColor * attenuation;
	const vec3 diffuse = diff * diffColor;
	const vec3 specular = Spec(wsNormal, wsPos, lightColor, wsLightDir) * attenuation * texture(Specular, uv).r;
	
    return DiffColor(diff, diffuse + specular);
}

// functions with comments like below was rewritten to GLSL from https://github.com/TheRealMJP/Shadows/blob/master/Shadows/Mesh.hlsl

//-------------------------------------------------------------------------------------------------
// Samples the shadow map with a fixed-size PCF kernel optimized with GatherCmp. Uses code
// from "Fast Conventional Shadow Filtering" by Holger Gruen, in GPU Pro.
//-------------------------------------------------------------------------------------------------
float SampleShadowMapFixedSizePCF(vec3 uvz, uint idxCascade) {
	const vec2 shadowMapSize = textureSize(ShadowMapArray, 0).xy;
    const float lightDepth = uvz.z + Bias;
    vec2 tc = uvz.xy;

    vec4 s = vec4(0.0f);
    const vec2 stc = (shadowMapSize * tc.xy) + vec2(0.5f, 0.5f);
    const vec2 tcs = floor(stc);
    vec2 fc;
    float w = 0.0f;

    fc.xy = stc - tcs;
	tc.xy = tcs / shadowMapSize;
		
	for(int row = 0; row < g_kKernelSize; ++row) {
		for(int col = 0; col < g_kKernelSize; ++col)
			w += W[row][col];
	}

	const int KS_2 = g_kKernelSize / 2;
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
				v1[(col + KS_2) / 2] = textureGatherOffset(ShadowMapArray, vec3(tc.xy, idxCascade), lightDepth, ivec2(col, row));
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

//-------------------------------------------------------------------------------------------------
// Samples the appropriate shadow map cascade
//-------------------------------------------------------------------------------------------------
float SampleShadowCascade(vec3 uvz, uint cascadeIdx)
{
    uvz += AOffsetCascade[cascadeIdx].xyz;
    uvz *= AScaleCascade[cascadeIdx].xyz;

    return SampleShadowMapFixedSizePCF(uvz, cascadeIdx);
}
//-------------------------------------------------------------------------------------------------
// Calculates the offset to use for sampling the shadow map, based on the surface normal
//-------------------------------------------------------------------------------------------------
vec3 GetWsShadowPosOffset(float nDotL, vec3 wsNormal)
{
    const float texelSize = 2.0f / textureSize(ShadowMapArray, 0).x;
    const float nmlOffsetScale = 1.0f - nDotL;
    return texelSize * ScaleNormalOffsetBias * nmlOffsetScale * wsNormal;
}

float ShadowVisibility(vec3 wsPos, float vsDepth, float nDotL, vec3 wsNormal)
{
	int idxCascade = g_kNumCascades-1;
    const vec3 projectionShadowPos = (ReferenceShadowMatrix * vec4(wsPos, 1.0f)).xyz;

	// projection base cascade selection
	for(int i = g_kNumCascades - 1; i >= 0; --i) {
		// Select based on whether or not the pixel is inside the projection
		vec3 cascadePos = projectionShadowPos + AOffsetCascade[i].xyz;
		cascadePos *= AScaleCascade[i].xyz;
		cascadePos = abs(cascadePos - 0.5f);
		if(cascadePos.x <= 0.5f && cascadePos.y <= 0.5f)
			idxCascade = i;
	}

    const vec3 wsOffset = GetWsShadowPosOffset(nDotL, wsNormal) / abs(AScaleCascade[idxCascade].x);
	const vec3 uvz = (ReferenceShadowMatrix * vec4(wsPos + wsOffset, 1.0f)).xyz;
	float shadowVisibility = SampleShadowCascade(uvz, idxCascade);
	
	// Sample the next cascade, and blend between the two results to smooth the transition
	const float BlendThreshold = 0.2f;

	const float nextSplit = AVsFarCascade[idxCascade];
	const float splitSize = idxCascade == 0 ? nextSplit : nextSplit - AVsFarCascade[idxCascade - 1];
	float fadeFactor = (nextSplit - vsDepth) / splitSize;
	
	vec3 cascadePos = projectionShadowPos + AOffsetCascade[idxCascade].xyz;
	cascadePos *= AScaleCascade[idxCascade].xyz;
	cascadePos = abs(cascadePos * 2.0f - 1.0f);
	const float distToEdge = 1.0f - max(max(cascadePos.x, cascadePos.y), cascadePos.z);
	fadeFactor = max(distToEdge, fadeFactor);
	
	if(fadeFactor <= BlendThreshold && idxCascade != g_kNumCascades - 1) {
		// Apply offset
		const vec3 wsNextCascadeOffset = GetWsShadowPosOffset(nDotL, wsNormal) / abs(AScaleCascade[idxCascade + 1].x);
        // Project into shadow space
		const vec3 nextCascadeShadowPosition = (ReferenceShadowMatrix * vec4(wsPos + wsNextCascadeOffset, 1.0f)).xyz;
		const float nextSplitVisibility = SampleShadowCascade(nextCascadeShadowPosition, idxCascade + 1);
		const float mixAmt = smoothstep(0.0f, BlendThreshold, fadeFactor);
		shadowVisibility = mix(nextSplitVisibility, shadowVisibility, mixAmt);
    }
	return shadowVisibility;
}

DiffColor DirrLight(vec3 wsNormal, vec3 wsPos, float vdDepth, vec3 lightColor, vec3 wsLightDir, vec3 diffColor, vec2 uv) {
	const float nDotL = max(dot(wsNormal, wsLightDir), 0);
	const vec3 diff = nDotL * lightColor;
	const vec3 diffuse = diff * diffColor;
	const vec3 specular = Spec(wsNormal, wsPos, lightColor, wsLightDir) * texture(Specular, uv).r;
	
	const float lightning = ShadowVisibility(wsPos, vdDepth, nDotL, wsNormal);
	return DiffColor(lightning * diff, lightning * (diffuse + specular));
}

void main() {
	#ifdef TRANSPARENCY
	if (texture(Mask, Input.UV).a < .5)
		discard;
	#endif
	const float vsDepth = Input.VsDepth;
	
	const vec3 diffColor = texture(Diffuse, Input.UV).rgb;
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
	vec3 diff = vec3(0);
	// dir light
	DiffColor tempSun = DirrLight(wsNormal, Input.WsPos, vsDepth, DirLightColor, WsLightDir, diffColor, Input.UV);
	color += tempSun.color;
	diff += tempSun.diff;

	// point lights
    for(int i = 0; i < g_kNumPointLights; ++i){
        DiffColor tempPoint = PointLight(wsNormal, Input.WsPos, AWsPointLightPosition[i], APointLightColor[i], diffColor, Input.UV);
		color += tempPoint.color;
		diff += tempPoint.diff;
    }

	// ambient
	color += diffColor * .3;

	outDiffuseLight = log(0.01+dot(diff, vec3(0.2126, 0.7152, 0.0722)));
	outFragColor = color;
}

