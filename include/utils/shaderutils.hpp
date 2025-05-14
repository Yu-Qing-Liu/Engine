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
		VkShaderModule vertexShader;
		VkShaderModule tessellationControlShader;
		VkShaderModule tessellationEvaluationShader;
		VkShaderModule geometryShader;
		VkShaderModule fragmentShader;
		VkShaderModule computeShader;
	};

	static ShaderUtils &getInstance(VkDevice &device);

	ShaderModules compileShaderProgram(std::string &shaderRootDir);
	VkPipelineShaderStageCreateInfo createShaderStageInfo(VkShaderModule &shaderModule, VkShaderStageFlagBits stage);

  private:
	struct ShaderBinaries {
		std::vector<uint32_t> vertexShader;
		std::vector<uint32_t> tessellationControlShader;
		std::vector<uint32_t> tessellationEvaluationShader;
		std::vector<uint32_t> geometryShader;
		std::vector<uint32_t> fragmentShader;
		std::vector<uint32_t> computeShader;
	};

	VkDevice &device;

	Compiler compiler;
	CompileOptions options;

	std::string shaderRootPath = std::string(PROJECT_ROOT_DIR) + "/src/shaders";
	std::string shaderCachePath = std::string(PROJECT_ROOT_DIR) + "/src/cache";

	std::unordered_map<std::string, shaderc_shader_kind> shaderExtensions = {{".vert", shaderc_glsl_vertex_shader}, {".tesc", shaderc_glsl_tess_control_shader}, {".tese", shaderc_glsl_tess_evaluation_shader}, {".geom", shaderc_glsl_geometry_shader}, {".frag", shaderc_glsl_fragment_shader}, {".comp", shaderc_glsl_compute_shader}};

	ShaderBinaries compileShader(const std::vector<std::string> &shaderPaths);
	std::vector<uint32_t> compileShader(const std::string &shaderPath);

	std::string computeHash(const std::string &input);

	shaderc_shader_kind getShaderKind(const std::string &extension);
	void createDirectory(const std::string &path);
	std::string readFile(const std::string &filePath);

	void writeBinaryFile(const std::string &path, const std::vector<uint32_t> &data);
	std::vector<uint32_t> readBinaryFile(const std::string &path);

	VkShaderModule createShaderModule(const std::vector<uint32_t> &binary);
};
