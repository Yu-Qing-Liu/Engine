#pragma once

#include "shaderutils.hpp"
#include <array>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <string>
#include <vulkan/vulkan_core.h>

using namespace glm;

class Model {
  public:
	Model(VkPhysicalDevice &physicalDevice, VkDevice &device, const std::string &modelRootPath, VkRenderPass &renderPass, VkExtent2D &swapChainExtent);
	Model(Model &&) = default;
	Model(const Model &) = delete;
	Model &operator=(Model &&) = delete;
	Model &operator=(const Model &) = delete;
	virtual ~Model();

	struct Vertex {
		vec2 pos;
		vec3 color;

		static VkVertexInputBindingDescription getBindingDescription() {
			VkVertexInputBindingDescription bindingDescription{};
			bindingDescription.binding = 0;
			bindingDescription.stride = sizeof(Vertex);
			bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

			return bindingDescription;
		}

		static std::array<VkVertexInputAttributeDescription, 2> getAttributeDescriptions() {
			std::array<VkVertexInputAttributeDescription, 2> attributeDescriptions{};

			attributeDescriptions[0].binding = 0;
			attributeDescriptions[0].location = 0;
			attributeDescriptions[0].format = VK_FORMAT_R32G32_SFLOAT;
			attributeDescriptions[0].offset = offsetof(Vertex, pos);

			attributeDescriptions[1].binding = 0;
			attributeDescriptions[1].location = 1;
			attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
			attributeDescriptions[1].offset = offsetof(Vertex, color);

			return attributeDescriptions;
		}
	};

	VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
	VkPipelineInputAssemblyStateCreateInfo inputAssembly{};

	std::vector<Vertex> vertices;

	virtual void setup();
	virtual void draw(VkCommandBuffer &commandBuffer, const vec3 &position = vec3(0.0f, 0.0f, 0.0f), const quat &rotation = quat(), const vec3 &scale = vec3(1.0f, 1.0f, 1.0f), const vec3 &color = vec3(1.0f, 1.0f, 1.0f));

  protected:
	ShaderUtils *shaderUtils;
	ShaderUtils::ShaderModules shaderProgram;

	VkPipelineLayout pipelineLayout;
	VkPipeline graphicsPipeline;

	VkPhysicalDevice &physicalDevice;
	VkDevice &device;
	VkRenderPass &renderPass;
	VkExtent2D &swapChainExtent;

	std::string modelRootPath;

	void createGraphicsPipeline(const std::vector<VkPipelineShaderStageCreateInfo> &shaderStages, VkPipelineVertexInputStateCreateInfo vertexInputInfo, VkPipelineInputAssemblyStateCreateInfo inputAssembly);
	uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
};
