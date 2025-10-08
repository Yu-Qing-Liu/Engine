#pragma once
#include "raytracingpipeline.hpp"
#include <span>

// One quad per glyph; positions in the same space as your draw (world space)
class TextRayTracing : public RayTracingPipeline {
  public:
	struct GlyphSpanGPU {
		glm::vec4 p0, p1, p2, p3;
		uint32_t letterIndex; // 0-based index of the letter in the string (codepoint or byte → your choice)
		uint32_t _p0, _p1, _p2;
	};

	TextRayTracing(Model *model, uint32_t maxGlyphs = 8192, const std::string &shaderDir = Assets::shaderRootPath + "/textraytracing") : RayTracingPipeline(model), maxGlyphs(maxGlyphs) {
		rayTracingShaderPath = shaderDir; // overrides base path
	}

	virtual ~TextRayTracing() {
		// Unmap first
		if (spansMapped) {
			vkUnmapMemory(Engine::device, spansMem);
			spansMapped = nullptr;
		}
		if (pickUBOMapped) {
			vkUnmapMemory(Engine::device, pickUBOMem);
			pickUBOMapped = nullptr;
		}
		if (hitMapped) {
			vkUnmapMemory(Engine::device, hitMem);
			hitMapped = nullptr;
		}

		// Then destroy buffers & free memory
		if (spansBuf) {
			vkDestroyBuffer(Engine::device, spansBuf, nullptr);
			spansBuf = VK_NULL_HANDLE;
		}
		if (spansMem) {
			vkFreeMemory(Engine::device, spansMem, nullptr);
			spansMem = VK_NULL_HANDLE;
		}

		if (pickUBO) {
			vkDestroyBuffer(Engine::device, pickUBO, nullptr);
			pickUBO = VK_NULL_HANDLE;
		}
		if (pickUBOMem) {
			vkFreeMemory(Engine::device, pickUBOMem, nullptr);
			pickUBOMem = VK_NULL_HANDLE;
		}

		if (hitBuf) {
			vkDestroyBuffer(Engine::device, hitBuf, nullptr);
			hitBuf = VK_NULL_HANDLE;
		}
		if (hitMem) {
			vkFreeMemory(Engine::device, hitMem, nullptr);
			hitMem = VK_NULL_HANDLE;
		}

		// Pipeline / descriptors
		if (computePipeline) {
			vkDestroyPipeline(Engine::device, computePipeline, nullptr);
			computePipeline = VK_NULL_HANDLE;
		}
		if (computePipelineLayout) {
			vkDestroyPipelineLayout(Engine::device, computePipelineLayout, nullptr);
			computePipelineLayout = VK_NULL_HANDLE;
		}
		if (computePool) {
			vkDestroyDescriptorPool(Engine::device, computePool, nullptr);
			computePool = VK_NULL_HANDLE;
		}
		if (computeDescriptorSetLayout) {
			vkDestroyDescriptorSetLayout(Engine::device, computeDescriptorSetLayout, nullptr);
			computeDescriptorSetLayout = VK_NULL_HANDLE;
		}

		// Shader module if your Assets loader gave you ownership
		if (rayTracingProgram.computeShader) {
			vkDestroyShaderModule(Engine::device, rayTracingProgram.computeShader, nullptr);
			rayTracingProgram.computeShader = VK_NULL_HANDLE;
		}
	}

	// Engine calls:
	void initialize() override {
		if (!initialized) {
			createComputeDescriptorSetLayout();
			createShaderStorageBuffers();
			createComputeDescriptorSets();
			createComputePipeline();
			initialized = true;
		}
	}
	void updateComputeUniformBuffer() override; // set _pad = glyphCount, read hit
	void compute() override;					// dispatch based on glyphCount

	// Upload/resize helpers driven by Text
	void setGlyphCount(uint32_t n) { glyphCount = std::min(n, maxGlyphs); }
	void uploadSpans(std::span<const GlyphSpanGPU> spans);

  protected:
	// Replace BVH layout with [UBO, spans, hit]
	void createComputeDescriptorSetLayout() override;
	void createShaderStorageBuffers() override;
	void createComputeDescriptorSets() override;
	void createComputePipeline() override;

	// GPU buffers for text picking
	VkBuffer spansBuf = VK_NULL_HANDLE;
	VkDeviceMemory spansMem = VK_NULL_HANDLE;
	void *spansMapped = nullptr;

	// Reuse base pickUBO/hitBuf/hitMapped from RayTracingPipeline
	uint32_t glyphCount = 0;
	uint32_t maxGlyphs = 0;
};
