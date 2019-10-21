#version 330 core
layout (location = 0) in vec3 inPos;
#ifdef TRANSPARENCY
layout (location = 1) in vec2 inUV;
out vec2 UV;
#endif

uniform mat4 ModelLightProj;

void main()
{
#ifdef TRANSPARENCY
	UV = inUV;
#endif
    gl_Position = ModelLightProj * vec4(inPos, 1.0);
}