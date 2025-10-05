#pragma once

#include "blurpipeline.hpp"
#include "scene.hpp"
#include <map>

using std::make_shared;
using std::shared_ptr;
using std::string;

class Scenes {
  public:
	Scenes();
	Scenes(Scenes &&) = delete;
	Scenes(const Scenes &) = delete;
	Scenes &operator=(Scenes &&) = delete;
	Scenes &operator=(const Scenes &) = delete;
	~Scenes() = default;

	void updateComputeUniformBuffers();
	void computePass();

	void updateUniformBuffers();
	void renderPass();
	void renderPass1();
	void swapChainUpdate();

	shared_ptr<Scene> getScene(const string &sceneName);
	void showScene(const string &sceneName);
	void hideScene(const string &sceneName);

  private:
	std::vector<shared_ptr<Scene>> scenesContainer;
	std::map<string, shared_ptr<Scene>> scenes;

	std::unique_ptr<BlurPipeline> blur;

	VkViewport vp{};
	VkRect2D sc{};
};
