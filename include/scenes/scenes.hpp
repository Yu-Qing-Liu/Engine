#pragma once

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

	struct SceneEntry {
		shared_ptr<Scene> scene;
		bool show = false;
	};

	void updateComputeUniformBuffers();
	void computePass();

	void updateUniformBuffers();
	void renderPass();
	void swapChainUpdate();

	shared_ptr<Scene> getScene(const string &sceneName);
	void showScene(const string &sceneName);
	void hideScene(const string &sceneName);

  private:
	std::vector<shared_ptr<Scene>> scenesContainer;
	std::map<string, SceneEntry> scenes;
};
