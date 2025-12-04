#include "destination.hpp"

Destination::Destination(Scene *scene) : ShaderQuad(scene) {
	setFragmentShader(std::string("#version 450\n"
								  "layout(location = 0) in vec2 vLocal;\n"
								  "layout(location = 0) out vec4 outColor;\n"
								  "\n"
								  "void main() {\n"
								  "    outColor = vec4(0.0, 1.0, 0.0, 1.0);\n"
								  "}\n"));
}
