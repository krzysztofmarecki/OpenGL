#version 420 core
#include "depth.gl"
layout (location = 0) out vec4 OutColor;

in vec2 UV;

layout (binding = 0) uniform sampler2D ColorCurr;
layout (binding = 1) uniform sampler2D ColorAcc;
layout (binding = 2) uniform sampler2D Velocity;
layout (binding = 3) uniform sampler2D DepthCurr;
layout (binding = 4) uniform sampler2D DepthPrev;

uniform float RateOfChange;
uniform vec4 Scaling;
uniform float Near;

vec3 YCoCgFromRGB(vec3 rgb) {
	const float y = dot(vec3(0.25, 0.5, 0.25), rgb);
	const float co = dot(vec3(0.5, 0, -0.5), rgb);
	const float cg = dot(vec3(-0.25, 0.5, -0.25), rgb);
	return vec3(y, co, cg);
}

vec3 RGBFromYCoCg(vec3 yCoCg) {
	const float r = dot(vec3(1, 1,-1), yCoCg);
	const float g = dot(vec3(1, 0, 1), yCoCg);
	const float b = dot(vec3(1,-1,-1), yCoCg);
	return vec3(r, g, b);
}

struct MinMaxAvg {
	vec3 minimum;
	vec3 maximum;
	vec3 average;
};

MinMaxAvg NeighbourhoodClamp(vec2 uv, vec2 scaling, vec3 cmc, sampler2D Color) {
	const vec2 du = vec2(scaling.x, 0);
	const vec2 dv = vec2(0, scaling.y);

	const vec3 cul = YCoCgFromRGB(texture(Color, uv + dv - du).xyz);
	const vec3 cuc = YCoCgFromRGB(texture(Color, uv + dv).xyz);
	const vec3 cur = YCoCgFromRGB(texture(Color, uv + dv + du).xyz);
	const vec3 cml = YCoCgFromRGB(texture(Color, uv - du).xyz);	
	const vec3 cmr = YCoCgFromRGB(texture(Color, uv + du).xyz);
	const vec3 cbl = YCoCgFromRGB(texture(Color, uv - dv - du).xyz);
	const vec3 cbc = YCoCgFromRGB(texture(Color, uv - dv).xyz);
	const vec3 cbr = YCoCgFromRGB(texture(Color, uv - dv + du).xyz);
	const vec3 cmin = min(cul, min(cuc, min(cur, min(cml, min(cmc, min(cmr, min(cbl, min(cbc, cbr))))))));
	const vec3 cmax = max(cul, max(cuc, max(cur, max(cml, max(cmc, max(cmr, max(cbl, max(cbc, cbr))))))));
	const vec3 cavg = (cul + cuc + cur + cml + cmc + cmr + cbl + cbc + cbr) / 9.0;
	return MinMaxAvg(cmin, cmax, cavg);
}

// https://github.com/playdeadgames/temporal/blob/master/Assets/Shaders/TemporalReprojection.shader
vec3 ClipAABB(vec3 aabb_min, vec3 aabb_max, vec3 p, vec3 q) 
{
	vec3 r = q - p;
	const vec3 rmax = aabb_max - p.xyz;
	const vec3 rmin = aabb_min - p.xyz;

	if (r.x > rmax.x)
		r *= (rmax.x / r.x);
	if (r.y > rmax.y)
		r *= (rmax.y / r.y);
	if (r.z > rmax.z)
		r *= (rmax.z / r.z);

	if (r.x < rmin.x)
		r *= (rmin.x / r.x);
	if (r.y < rmin.y)
		r *= (rmin.y / r.y);
	if (r.z < rmin.z)
		r *= (rmin.z / r.z);

	return p + r;
}

vec3 ClosestUVZ(vec2 uv, vec2 scaling, sampler2D Depth) {
	const vec2 du = vec2(scaling.x, 0);
	const vec2 dv = vec2(0, scaling.y);

	const float dul = texture(Depth, uv + dv - du).x;
	const float duc = texture(Depth, uv + dv).x;
	const float dur = texture(Depth, uv + dv + du).x;
	const float dml = texture(Depth, uv - du).x;
	const float dmc = texture(Depth, uv).x;
	const float dmr = texture(Depth, uv + du).x;
	const float dbl = texture(Depth, uv - dv - du).x;
	const float dbc = texture(Depth, uv - dv).x;
	const float dbr = texture(Depth, uv - dv + du).x;
	
	vec3 closest				 = vec3(-1,  1, dul);
	if (duc > closest.z) closest = vec3( 0,  1, duc);
	if (dur > closest.z) closest = vec3( 1,  1, dur);
	if (dml > closest.z) closest = vec3(-1,  0, dml);
	if (dmc > closest.z) closest = vec3( 0,  0, dmc);
	if (dmr > closest.z) closest = vec3( 1,  0, dmr);
	if (dbl > closest.z) closest = vec3(-1, -1, dbl);
	if (dbc > closest.z) closest = vec3( 0, -1, dbc);
	if (dbr > closest.z) closest = vec3( 1, -1, dbr);

	return vec3(uv + scaling * closest.xy, closest.z);
}

// from Filmic SMAA
vec3 CatmulRom5Tap(vec2 uv, vec4 scaling, sampler2D Color)
{
    const vec2 samplePos = uv * scaling.xy;
    const vec2 tc1 = floor(samplePos - 0.5) + 0.5;
    const vec2 f = samplePos - tc1;
    const vec2 f2 = f * f;
    const vec2 f3 = f * f2;

    const float c = 0.25; // sharpening, too high values produse artifacts

    const vec2 w0 = -c         * f3 +  2.0 * c         * f2 - c * f;
    const vec2 w1 =  (2.0 - c) * f3 - (3.0 - c)        * f2          + 1.0;
    const vec2 w2 = -(2.0 - c) * f3 + (3.0 - 2.0 * c)  * f2 + c * f;
    const vec2 w3 = c          * f3 - c                * f2;

    const vec2 w12 = w1 + w2;
    const vec2 tc0 = scaling.zw   * (tc1 - 1.0);
    const vec2 tc3 = scaling.zw   * (tc1 + 2.0);
    const vec2 tc12 = scaling.zw  * (tc1 + w2 / w12);

	vec3 result = vec3(0);
    result += texture(Color, vec2(tc12.x, tc0.y)).rgb * w12.x * w0.y;
    result += texture(Color, vec2(tc0.x, tc12.y)).rgb * w0.x * w12.y;
    result += texture(Color, vec2(tc12.x, tc12.y)).rgb * w12.x * w12.y;
    result += texture(Color, vec2(tc3.x, tc0.y)).rgb * w3.x * w12.y;
    result += texture(Color, vec2(tc12.x, tc3.y)).rgb * w12.x *  w3.y;

	return result;
}

void main() {
	const vec3 colorCurr = YCoCgFromRGB(texture(ColorCurr, UV).rgb);
	const vec3 closestUVZ = ClosestUVZ(UV, Scaling.zw, DepthCurr);
	const vec2 velocity = texture(Velocity, closestUVZ.xy).xy;
	vec3 colorAcc = YCoCgFromRGB(CatmulRom5Tap(UV - velocity, Scaling, ColorAcc));

	MinMaxAvg minMaxAvg = NeighbourhoodClamp(UV, Scaling.zw, colorCurr, ColorCurr);

	const vec3 colorMin = minMaxAvg.minimum;
    const vec3 colorMax = minMaxAvg.maximum;    
	const vec3 colorAvg = minMaxAvg.average;
	
	// cliping towards colorAvg (instead of colorCurr) produses less flickering
	// but also produse strange artifacts on my "skybox" at certain angles with very high exposure
	// but I don't care about that "sky"
	colorAcc = ClipAABB(colorMin, colorMax, colorAvg, colorAcc);
	
	const float distToClamp = min(abs(colorMin.x - colorAcc.x), abs(colorMax.x - colorAcc.x));
	float rateOfChange = clamp((RateOfChange * distToClamp) / (distToClamp + colorMax.x - colorMin.x), 0, 1);
	
	const vec2 uvPrevDistanceToMiddle = abs((UV - velocity) - vec2(0.5));
	if (uvPrevDistanceToMiddle.x > 0.5 || uvPrevDistanceToMiddle.y > 0.5)
		rateOfChange = 1;

	// taking single sample produces less flickering than taking closest depth in 3x3 from DepthPrev
	// BUT closest depth in 3x3 from DepthCurrent is better
	const float depthPrev = texture(DepthPrev, UV - velocity).r;
	if ((-VsDepthFromCsDepth(closestUVZ.z, Near) * 0.9) > -VsDepthFromCsDepth(depthPrev, Near))
		rateOfChange = 1;
	
	OutColor = vec4(RGBFromYCoCg(mix(colorAcc, colorCurr, rateOfChange)), 1);
}

