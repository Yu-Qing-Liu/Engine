#pragma once

#include "blurpipeline.hpp"

class BlursPipeline : public BlurPipeline {
  public:
	BlursPipeline(Model *model, std::array<VkVertexInputBindingDescription, 2> &bindings, std::vector<VkBuffer> &instanceBuffers, uint32_t &instanceCount);
	BlursPipeline(BlursPipeline &&) = default;
	BlursPipeline(const BlursPipeline &) = default;
	BlursPipeline &operator=(BlursPipeline &&) = delete;
	BlursPipeline &operator=(const BlursPipeline &) = delete;
	~BlursPipeline() = default;

	void initialize() override;
	void render() override;

  protected:
	std::array<VkVertexInputBindingDescription, 2> &bindings;
	std::vector<VkBuffer> &instanceBuffers;
	uint32_t &instanceCount;

	void createPipeAndSets() override;
	void destroyPipeAndSets() override;
};
