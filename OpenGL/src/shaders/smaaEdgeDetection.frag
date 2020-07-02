#version 420 core
#include "smaaSettings.gl"
#define SMAA_INCLUDE_VS 0
#define SMAA_INCLUDE_PS 1
#include "SMAA.h"

out vec2 outEdge;

layout (binding = 0) uniform sampler2D Color;

in vec2 UV;
in vec4 aOffset[3];

void main() {
    outEdge = SMAALumaEdgeDetectionPS(UV, aOffset, Color);
}
