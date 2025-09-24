#include "computepipeline.hpp"
#include "engine.hpp"

ComputePipeline::~ComputePipeline() {
	if (computePipeline != VK_NULL_HANDLE) {
		vkDestroyPipeline(Engine::device, computePipeline, nullptr);
	}
	if (computePipelineLayout != VK_NULL_HANDLE) {
		vkDestroyPipelineLayout(Engine::device, computePipelineLayout, nullptr);
	}
	if (computePool != VK_NULL_HANDLE) {
		vkDestroyDescriptorPool(Engine::device, computePool, nullptr);
	}
	if (computeDescriptorSetLayout != VK_NULL_HANDLE) {
		vkDestroyDescriptorSetLayout(Engine::device, computeDescriptorSetLayout, nullptr);
	}
}
