#version 420 core
#ifdef ALPHA_MASKED
in vec2 UV;
layout (binding = 0) uniform sampler2D Mask;
void main() {
	if (texture(Mask, UV).a < 0.5)
		discard;
}
#else
void main() {

}
#endif


