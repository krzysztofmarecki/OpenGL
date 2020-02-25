#version 420 core
layout (location = 0) in vec3 inPos;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec2 inUV;
layout (location = 3) in vec3 inTangent;

out VS_OUT {
    vec3 WsPos;
    vec3 WsNormal;
	vec3 WsTangent;
    vec2 UV;
	float VsDepth;
} Output;

uniform mat4 Model;
uniform mat4 ViewProj;
uniform mat3 NormalMatrix;

void main()
{
	vec4 pos = (Model * vec4(inPos, 1));
    Output.WsPos = pos.xyz;
	Output.WsNormal  = NormalMatrix*inNormal;
	Output.WsTangent = NormalMatrix*inTangent;
    Output.UV = inUV;
	gl_Position = ViewProj * pos;
	Output.VsDepth = -gl_Position.w; // "-" because right handed coordinate system
}