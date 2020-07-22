#version 420 core
#include "smaaSettings.gl"
#define SMAA_INCLUDE_VS 0
#define SMAA_INCLUDE_PS 1
#include "SMAA.h"

out vec4 outEdge;

layout (binding = 0) uniform sampler2D ColorCurr;
layout (binding = 1) uniform sampler2D ColorPrev;
layout (binding = 2) uniform sampler2D Velocity;
in vec2 UV;

void main() {
    outEdge = SMAAResolvePS(UV, ColorCurr, ColorPrev, Velocity);
}
