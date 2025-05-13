#pragma once

#include <shaderc/shaderc.hpp>
#include <string>
#include <vector>
#include <vulkan/vulkan_core.h>

using namespace shaderc;

class ShaderUtils {
  public:
	ShaderUtils(VkDevice &device);
	ShaderUtils(ShaderUtils &&) = default;
	ShaderUtils(const ShaderUtils &) = default;
	ShaderUtils &operator=(ShaderUtils &&) = delete;
	ShaderUtils &operator=(const ShaderUtils &) = delete;
	~ShaderUtils() = default;

  private:
	VkDevice &device;

	std::vector<SpvCompilationResult> compileShader(std::vector<std::string> shader_paths);

	std::vector<char> readFile(const std::string &filename);
	VkShaderModule createShaderModule(const std::vector<char> &binary);
};
