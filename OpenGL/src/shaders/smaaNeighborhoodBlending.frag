#version 420 core
#include "smaaSettings.gl"
#define SMAA_INCLUDE_VS 0
#define SMAA_INCLUDE_PS 1
#include "gamma.gl"
#include "SMAA.h"

out vec4 outColor;

layout (binding = 0) uniform sampler2D Color;
layout (binding = 1) uniform sampler2D Blend;
layout (binding = 2) uniform sampler2D Velocity;

in vec2 UV;
in vec4 Offset;

void main() {
    outColor = SMAANeighborhoodBlendingPS(UV, Offset, Color, Blend, Velocity);
}
