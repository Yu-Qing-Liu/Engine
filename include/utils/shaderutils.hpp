#pragma once

#include <shaderc/shaderc.h>
#include <shaderc/shaderc.hpp>
#include <string>
#include <unordered_map>
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
		VkShaderModule tessellation_control_shader;
		VkShaderModule tessellation_evaluation_shader;
		VkShaderModule geometry_shader;
		VkShaderModule fragment_shader;
		VkShaderModule compute_shader;
	};

	static ShaderUtils &getInstance(VkDevice &device);

	ShaderModules compileShaderProgram(std::string &shader_source_root_dir);

  private:
	struct ShaderBinaries {
		std::vector<uint32_t> vertex_shader;
		std::vector<uint32_t> tessellation_control_shader;
		std::vector<uint32_t> tessellation_evaluation_shader;
		std::vector<uint32_t> geometry_shader;
		std::vector<uint32_t> fragment_shader;
		std::vector<uint32_t> compute_shader;
	};

	VkDevice &device;

	Compiler compiler;
	CompileOptions options;

	std::string shader_root_path = std::string(PROJECT_ROOT_DIR) + "/src/shaders";
	std::string shader_cache_path = std::string(PROJECT_ROOT_DIR) + "/src/cache";

	std::unordered_map<std::string, shaderc_shader_kind> shader_extensions = {{".vert", shaderc_glsl_vertex_shader}, {".tesc", shaderc_glsl_tess_control_shader}, {".tese", shaderc_glsl_tess_evaluation_shader}, {".geom", shaderc_glsl_geometry_shader}, {".frag", shaderc_glsl_fragment_shader}, {".comp", shaderc_glsl_compute_shader}};

	ShaderBinaries compileShader(const std::vector<std::string> &shader_paths);
	std::vector<uint32_t> compileShader(const std::string &shader_path);

	std::string computeHash(const std::string &input);

	shaderc_shader_kind getShaderKind(const std::string &extension);
	void createDirectory(const std::string &path);
	std::string readFile(const std::string &file_path);

	void writeBinaryFile(const std::string &path, const std::vector<uint32_t> &data);
	std::vector<uint32_t> readBinaryFile(const std::string &path);

	VkShaderModule createShaderModule(const std::vector<uint32_t> &binary);
};
