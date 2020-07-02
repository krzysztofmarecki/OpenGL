#version 400 core
#include "smaaSettings.gl"
#define SMAA_INCLUDE_VS 1
#define SMAA_INCLUDE_PS 0
#include "SMAA.h"

layout (location = 0) in vec3 inPos;
layout (location = 1) in vec2 inUV;

out vec2 UV;
out vec4 Offset;

void main() {
    UV = inUV;
    SMAANeighborhoodBlendingVS(inUV, Offset);
    gl_Position = vec4(inPos, 1.0);
}
