#version 330 core
layout (location = 0) in vec3 inPos;
#ifdef ALPHA_MASKED
layout (location = 1) in vec2 inUV;
out vec2 UV;
#endif

uniform mat4 ModelLightProj;

void main()
{
#ifdef ALPHA_MASKED
	UV = inUV;
#endif
    gl_Position = ModelLightProj * vec4(inPos, 1.0);
}