#pragma once

#include "scene.hpp"
#include "texture.hpp"
#include "grid.hpp"

using namespace std::chrono;

class Recipes : public Scene {
  public:
	Recipes(Scenes &scenes, bool show = true);
	Recipes(Recipes &&) = delete;
	Recipes(const Recipes &) = delete;
	Recipes &operator=(Recipes &&) = delete;
	Recipes &operator=(const Recipes &) = delete;
	~Recipes() = default;

	std::string getName() override { return "Recipes"; }

	void updateScreenParams() override;

	void updateComputeUniformBuffers() override;
	void computePass() override;

	void updateUniformBuffers() override;
	void renderPass() override;
	void renderPass1() override;
	void swapChainUpdate() override;

  private:
	unique_ptr<Grid> grid;
	unique_ptr<Texture> addRecipeIcon;
};
