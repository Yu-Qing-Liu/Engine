#pragma once

#include "scene.hpp"
#include <unordered_map>

class Scenes {
  public:
	Scenes();
	Scenes(Scenes &&) = default;
	Scenes(const Scenes &) = default;
	Scenes &operator=(Scenes &&) = delete;
	Scenes &operator=(const Scenes &) = delete;
	~Scenes() = default;

	void renderPass();
	void drawFrame();

  private:
	std::unordered_map<Engine::Scene, std::unique_ptr<Scene>> scenes;
};
