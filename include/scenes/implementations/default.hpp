#pragma once

#include "scenes/scene.hpp"

class Default : public Scene {
  public:
	Default(VkDevice &device, VkRenderPass &renderPass, VkExtent2D &swapChainExtent);
	Default(Default &&) = default;
	Default(const Default &) = delete;
	Default &operator=(Default &&) = delete;
	Default &operator=(const Default &) = delete;
	~Default() = default;

	void render(VkCommandBuffer &commandBuffer) override;
};
