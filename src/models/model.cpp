#include "models/model.hpp"
#include <string>

Model::Model(VkDevice &device, std::string &model_root) : device(device), model_root(model_root) {
	shader_utils = &ShaderUtils::getInstance(device);
	shader_program = shader_utils->compileShaderProgram(model_root);
}

void Model::draw(const vec3 &position, const vec3 &rotation, const vec3 &scale, const vec3 &color) {}
