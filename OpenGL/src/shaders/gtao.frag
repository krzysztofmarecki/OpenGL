#version 430 core
#include "kernels.gl"
#include "depth.gl"

layout (location = 0) out float Ao;

in vec2 UV;

layout (binding = 0) uniform sampler2D Depth;

uniform mat4 InvProj;
uniform float WsRadius;
uniform vec4 Scaling;
uniform float RadRotationTemporal;
const float kPi = 3.141592653589793238;

vec3 VsPosFromCsDepth(sampler2D Depth, vec2 uv, mat4 invProj) {
	float depth;

	if (true) { // reduces occlusion on thin objects without affecting others
		const vec4 depth4 = textureGather(Depth, uv);
		depth = min(min(depth4.x, depth4.y), min(depth4.z, depth4.w));
	} else {
		depth = texture(Depth, uv).x;
	}
	const vec4 vsPos = invProj * vec4(uv * 2 - 1, depth, 1);
	return vsPos.xyz / vsPos.w;
} 

vec3 VsNormalFromDepth(sampler2D Depth, vec2 uv, float depthCenter, vec3 vsCenterPos, vec2 scaling) {
	const vec2 up	 = vec2(0, scaling.y);
	const vec2 right = vec2(scaling.x, 0);
	const float depthUp	 = texture(Depth, UV + up).x;
	const float depthDown  = texture(Depth, UV - up).x;
	const float depthRight = texture(Depth, UV + right).x;
	const float depthLeft  = texture(Depth, UV - right).x;
	const bool isUpCloser = abs(depthUp - depthCenter) < abs(depthDown - depthCenter) ? true : false;
	const bool isRightCloser = abs(depthRight - depthCenter) < abs(depthLeft - depthCenter) ? true : false;

	vec3 p0;
	vec3 p1;
	// CCW
	if (isUpCloser && isRightCloser) {
		p0 = vec3(UV + right, depthRight);
		p1 = vec3(UV + up, depthUp);
	} else if (isUpCloser && !isRightCloser) {
		p0 = vec3(UV + up, depthUp);
		p1 = vec3(UV - right, depthLeft);
	} else if (!isUpCloser && isRightCloser) {
		p0 = vec3(UV - up, depthDown);
		p1 = vec3(UV + right, depthRight);
	} else if (!isUpCloser && !isRightCloser) {
		p0 = vec3(UV - right, depthLeft);
		p1 = vec3(UV - up, depthDown);
	}

	const vec3 vsP0 = VsPosFromCsDepth(p0.z, p0.xy, InvProj);
	const vec3 vsP1 = VsPosFromCsDepth(p1.z, p1.xy, InvProj);
	return -normalize(cross(vsP1 - vsCenterPos, vsP0 - vsCenterPos));
}

void main() {
	const float csDepth = texture(Depth, UV).x;
	const vec3 vsCenterPos = VsPosFromCsDepth(csDepth, UV, InvProj);
	const vec3 vsV = normalize(-vsCenterPos);
	const vec3 vsNormal = VsNormalFromDepth(Depth, UV, csDepth, vsCenterPos, Scaling.zw);
	const float radius = min(WsRadius / abs(vsCenterPos.z), WsRadius);

	const ivec2 xy = ivec2(gl_FragCoord);
	const float radAngle = RadRotationTemporal +
		(1.0 / 16.0) * ((((xy.x+xy.y) & 0x3) << 2) + (xy.x & 0x3)) * kPi * 2;

	const vec3 direction = vec3(cos(radAngle), sin(radAngle), 0);
	const vec3 orthoDirection = direction - dot(direction, vsV) * vsV;
	const vec3 vsAxis = cross(direction, vsV);
	const vec3 vsProjectedNormal = vsNormal - dot(vsNormal, vsAxis) * vsAxis;

	const float signN = sign(dot(orthoDirection, vsProjectedNormal));
	const float cosN = clamp(dot(vsProjectedNormal, vsV) / length(vsProjectedNormal), 0, 1);
	const float n = signN * acos(cosN);

	const int kNumDirectionSamples = 6;
	const float ssStep = radius / kNumDirectionSamples;
	float ao = 0;
	for (int side = 0; side <= 1; side++) {
		float cosHorizon = -1;
		vec2 uv = UV;
		uv += (-1 + 2 * side) * direction.xy *
			0.25 * ((xy.y - xy.x) & 0x3) * Scaling.zw;
		for (int i = 0; i < kNumDirectionSamples; i++) {
			uv += (-1 + 2 * side) * direction.xy * (ssStep);
			const vec3 vsSamplePos = VsPosFromCsDepth(Depth, uv, InvProj);
			const vec3 vsHorizonVec = (vsSamplePos - vsCenterPos);
			const float lenHorizonVec = length(vsHorizonVec);
			const float cosHorizonCurrent =  dot(vsHorizonVec, vsV) / lenHorizonVec;
			if (lenHorizonVec < 56.89/4)
				cosHorizon = max(cosHorizon, cosHorizonCurrent);
		}
		const float radHorizon = n + clamp((-1 + 2*side) * acos(cosHorizon) - n, -kPi/2, kPi/2);
		ao += length(vsProjectedNormal) * 0.25 * (cosN + 2 * radHorizon * sin(n) - cos(2 * radHorizon -n));
	}

	Ao = ao;
}

