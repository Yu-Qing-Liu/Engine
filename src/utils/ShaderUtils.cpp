#include "utils/ShaderUtils.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <shaderc/shaderc.h>
#include <shaderc/shaderc.hpp>
#include <string>
#include <vector>

using namespace std::filesystem;

ShaderUtils::ShaderUtils(VkDevice &device) : device(device) {
	createDirectory(shader_root_path);
	createDirectory(shader_cache_path);
}

// [.../xxx.vert, .../xxx.frag, .../xxx.geom, ...]
ShaderUtils::ShaderBinaries ShaderUtils::compileShader(const std::vector<std::string> &shader_paths) {
	ShaderBinaries binaries;
	for (const auto &p : shader_paths) {
		std::string extension = path(p).extension().string();

		if (extension == ".vert") {
		}
	}
	return binaries;
}

std::vector<uint32_t> ShaderUtils::compileShader(const std::string &shader_path) {
	std::string shader_code = readFile(shader_path);
	SpvCompilationResult result = compiler.CompileGlslToSpv(shader_code, shaderc_glsl_vertex_shader, shader_path.c_str());
	if (result.GetCompilationStatus() != shaderc_compilation_status_success) {
		std::cerr << "Failed to compile shader: " << shader_path << std::endl;
		std::cerr << "Error: " << result.GetErrorMessage() << std::endl;
        return {};
	} 
    std::vector<uint32_t> spirv(result.cbegin(), result.cend());
    return spirv;
}

void ShaderUtils::createDirectory(const std::string &path) {
	if (!exists(path)) {
		try {
			if (create_directory(path)) {
				std::cout << "Directory created successfully: " << path << std::endl;
			} else {
				std::cerr << "Failed to create directory: " << path << std::endl;
			}
		} catch (const filesystem_error &e) {
			std::cerr << "Error while creating directory: " << e.what() << std::endl;
		}
	} else {
		std::cout << "Directory already exists: " << path << std::endl;
	}
}

std::string ShaderUtils::readFile(const std::string &file_path) {
	std::ifstream shader_file(file_path);
	if (!shader_file.is_open()) {
		std::cerr << "Failed to open shader file: " << file_path << std::endl;
		return "";
	}
	std::stringstream buffer;
	buffer << shader_file.rdbuf();
	std::string content = buffer.str();
	return content;
}

VkShaderModule ShaderUtils::createShaderModule(const std::vector<char> &binary) {
	VkShaderModuleCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	createInfo.codeSize = binary.size();
	createInfo.pCode = reinterpret_cast<const uint32_t *>(binary.data());

	VkShaderModule shaderModule;
	if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
		throw std::runtime_error("failed to create shader module!");
	}

	return shaderModule;
}
