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
	virtual ~Model() = default;

	virtual void draw(VkCommandBuffer &commandBuffer, const vec3 &position = vec3(0.0f, 0.0f, 0.0f), const quat &rotation = quat(), const vec3 &scale = vec3(1.0f, 1.0f, 1.0f), const vec3 &color = vec3(1.0f, 1.0f, 1.0f));

  protected:
	ShaderUtils *shader_utils;
	ShaderUtils::ShaderModules shader_program;

	VkPipelineLayout pipelineLayout;
	VkPipeline graphicsPipeline;

	void createGraphicsPipeline(const std::vector<VkPipelineShaderStageCreateInfo> &shaderStages, VkPipelineVertexInputStateCreateInfo vertexInputInfo, VkPipelineInputAssemblyStateCreateInfo inputAssembly);

	VkDevice &device;
	VkRenderPass &renderPass;
	VkExtent2D &swapChainExtent;

	std::string modelRootPath;
};
