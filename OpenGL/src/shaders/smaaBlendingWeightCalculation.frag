#version 420 core
#include "smaaSettings.gl"
#define SMAA_INCLUDE_VS 0
#define SMAA_INCLUDE_PS 1
#include "SMAA.h"

out vec4 outBlendingWeights;

layout (binding = 0) uniform sampler2D Edge;
layout (binding = 1) uniform sampler2D Area;
layout (binding = 2) uniform sampler2D Search;

in vec2 UV;
in vec2 pixcoord;
in vec4 aOffset[3];

uniform vec4 SubsampleIndices;

void main() {
    outBlendingWeights = SMAABlendingWeightCalculationPS(UV, pixcoord, aOffset, Edge, Area, Search, SubsampleIndices);
}
