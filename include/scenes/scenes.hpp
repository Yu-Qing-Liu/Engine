#pragma once

#include "scene.hpp"
#include <unordered_map>

class Scenes {
  public:
	Scenes(VkDevice &device, VkRenderPass &renderPass, VkExtent2D &swapChainExtent);
	Scenes(Scenes &&) = default;
	Scenes(const Scenes &) = default;
	Scenes &operator=(Scenes &&) = delete;
	Scenes &operator=(const Scenes &) = delete;
	~Scenes() = default;

	enum SceneName {
		DEFAULT,
	};

	void render();

  private:
	std::unordered_map<SceneName, std::unique_ptr<Scene>> scenes;
};
