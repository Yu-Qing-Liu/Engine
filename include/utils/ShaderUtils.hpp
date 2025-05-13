#pragma once

#include <string>
#include <vector>
#include <vulkan/vulkan_core.h>

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

	std::vector<char> readFile(const std::string &filename);
	VkShaderModule createShaderModule(const std::vector<char> &binary);
};
