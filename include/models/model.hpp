#pragma once

#include "shaderutils.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <string>
#include <vulkan/vulkan_core.h>

using namespace glm;

class Model {
  public:
	Model(VkDevice &device, const std::string &modelRootPath, VkRenderPass &renderPass, VkExtent2D &swapChainExtent);
	Model(Model &&) = default;
	Model(const Model &) = delete;
	Model &operator=(Model &&) = delete;
	Model &operator=(const Model &) = delete;
	virtual ~Model();

	virtual void draw(VkCommandBuffer &commandBuffer, const vec3 &position = vec3(0.0f, 0.0f, 0.0f), const quat &rotation = quat(), const vec3 &scale = vec3(1.0f, 1.0f, 1.0f), const vec3 &color = vec3(1.0f, 1.0f, 1.0f));

	void setVPMatrix(const mat4 &view, const mat4 &proj);

  protected:
	ShaderUtils *shaderUtils;
	ShaderUtils::ShaderModules shader_program;

	VkPipelineLayout pipelineLayout;
	VkPipeline graphicsPipeline;

	VkDevice &device;
	VkRenderPass &renderPass;
	VkExtent2D &swapChainExtent;

	std::string modelRootPath;
	mat4 vp = mat4(1);

	void createGraphicsPipeline(const std::vector<VkPipelineShaderStageCreateInfo> &shaderStages, VkPipelineVertexInputStateCreateInfo vertexInputInfo, VkPipelineInputAssemblyStateCreateInfo inputAssembly);
};
