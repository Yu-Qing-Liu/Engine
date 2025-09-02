#pragma once

#include "model.hpp"
#include <vulkan/vulkan_core.h>

class Particles : public Model {
  public:
	Particles(uint32_t particleCount, uint32_t height, uint32_t width);
	Particles(Particles &&) = default;
	Particles(const Particles &) = delete;
	Particles &operator=(Particles &&) = delete;
	Particles &operator=(const Particles &) = delete;
	~Particles() = default;

	struct Particle {
		vec2 position;
		vec2 velocity;
		vec4 color;

		static VkVertexInputBindingDescription getBindingDescription() {
			VkVertexInputBindingDescription bindingDescription{};
			bindingDescription.binding = 0;
			bindingDescription.stride = sizeof(Particle);
			bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

			return bindingDescription;
		}

		static std::array<VkVertexInputAttributeDescription, 2> getAttributeDescriptions() {
			std::array<VkVertexInputAttributeDescription, 2> attributeDescriptions{};

			attributeDescriptions[0].binding = 0;
			attributeDescriptions[0].location = 0;
			attributeDescriptions[0].format = VK_FORMAT_R32G32_SFLOAT;
			attributeDescriptions[0].offset = offsetof(Particle, position);

			attributeDescriptions[1].binding = 0;
			attributeDescriptions[1].location = 1;
			attributeDescriptions[1].format = VK_FORMAT_R32G32B32A32_SFLOAT;
			attributeDescriptions[1].offset = offsetof(Particle, color);

			return attributeDescriptions;
		}
	};

	struct UniformBufferObject {
		float deltatime = 1.0f;
	};

	void render(const UBO &ubo, const ScreenParams &screenParams) override;

  protected:
	uint32_t particleCount;
	uint32_t height;
	uint32_t width;

	VkDescriptorSetLayout computeDescriptorSetLayout;
	VkPipelineLayout computePipelineLayout;
	VkPipeline computePipeline;

	vector<VkBuffer> shaderStorageBuffers;
	vector<VkDeviceMemory> shaderStorageBuffersMemory;
	vector<VkDescriptorSet> computeDescriptorSets;
	vector<VkCommandBuffer> computeCommandBuffers;

	void createComputeDescriptorSetLayout();
	void createBindingDescriptions() override;
	void createComputePipeline();
	void createShaderStorageBuffers();
	void createDescriptorPool() override;
	void createComputeDescriptorSets();
	void createComputeCommandBuffers();
};
