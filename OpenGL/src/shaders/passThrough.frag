#version 420 core
out vec4 outColor;

in vec2 UV;

layout(binding = 0) uniform sampler2D ColorHDR;

void main() {
	outColor = texture(ColorHDR, UV);
}

