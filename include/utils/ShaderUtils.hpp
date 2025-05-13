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
	ShaderUtils(const ShaderUtils &) = delete;
	ShaderUtils &operator=(ShaderUtils &&) = delete;
	ShaderUtils &operator=(const ShaderUtils &) = delete;
	~ShaderUtils() = default;

	struct ShaderModules {
		VkShaderModule vertex_shader;
		VkShaderModule fragment_shader;
		VkShaderModule geometry_shader;
		VkShaderModule compute_shader;
		VkShaderModule tesselation_shader;
		VkShaderModule mesh_shader;
	};

	ShaderModules compileShaderProgram(std::string &shader_source_root_dir);

  private:
	struct ShaderBinaries {
		std::vector<uint32_t> vertex_shader;
		std::vector<uint32_t> fragment_shader;
		std::vector<uint32_t> geometry_shader;
		std::vector<uint32_t> compute_shader;
		std::vector<uint32_t> tesselation_shader;
		std::vector<uint32_t> mesh_shader;
	};

	VkDevice &device;

	Compiler compiler;
	CompileOptions options;

	std::string shader_root_path = std::string(PROJECT_ROOT_DIR) + "/src/shaders";
	std::string shader_cache_path = std::string(PROJECT_ROOT_DIR) + "/src/cache";

	ShaderBinaries compileShader(const std::vector<std::string> &shader_paths);
	std::vector<uint32_t> compileShader(const std::string &shader_path);

	void createDirectory(const std::string &path);
	std::string readFile(const std::string &file_path);
	VkShaderModule createShaderModule(const std::vector<char> &binary);
};
