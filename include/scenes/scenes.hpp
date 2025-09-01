#pragma once

#include "scene.hpp"
#include <unordered_map>

using std::make_unique;
using std::string;
using std::unique_ptr;

class Scenes {
  public:
	Scenes();
	Scenes(Scenes &&) = delete;
	Scenes(const Scenes &) = delete;
	Scenes &operator=(Scenes &&) = delete;
	Scenes &operator=(const Scenes &) = delete;
	~Scenes() = default;

	struct SceneEntry {
		unique_ptr<Scene> scene;
		bool show = false;
	};

	void renderPass();
	void swapChainUpdate();

	void showScene(const string &sceneName);
	void hideScene(const string &sceneName);

  private:
	std::unordered_map<string, SceneEntry> scenes;
};
