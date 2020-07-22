#version 420 core
layout (location = 0) in vec3 inPos;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec2 inUV;
layout (location = 3) in vec3 inTangent;

out VS_OUT {
	vec3 PosCur;
	vec3 PosPrev;
    vec3 WsNormal;
	vec3 WsTangent;
    vec2 UV;
} Output;

uniform mat4 ModelViewProj;
uniform mat4 ModelViewProjPrev;
uniform mat3 NormalMatrix;

void main() {
	Output.WsNormal  = NormalMatrix*inNormal;
	Output.WsTangent = NormalMatrix*inTangent;
    Output.UV = inUV;
	gl_Position = ModelViewProj * vec4(inPos, 1);
	Output.PosCur = gl_Position.xyw;
	Output.PosPrev = (ModelViewProjPrev * vec4(inPos, 1)).xyw;
	Output.PosCur.xy *= 0.5;
	Output.PosPrev.xy *= 0.5;
}