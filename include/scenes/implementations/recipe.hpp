#pragma once

#include "grid.hpp"
#include "instancedrectangle.hpp"
#include "scene.hpp"
#include "textinput.hpp"
#include "texture.hpp"

using namespace std::chrono;

class Recipe : public Scene {
  public:
	Recipe(Scenes &scenes, bool show = true);
	Recipe(Recipe &&) = delete;
	Recipe(const Recipe &) = delete;
	Recipe &operator=(Recipe &&) = delete;
	Recipe &operator=(const Recipe &) = delete;
	~Recipe() = default;

	std::string getName() override { return "Recipe"; }

	void updateScreenParams() override;

	void updateComputeUniformBuffers() override;
	void computePass() override;

	void updateUniformBuffers() override;
	void renderPass() override;
	void renderPass1() override;
	void swapChainUpdate() override;

  private:
	unique_ptr<Grid> grid;
	unique_ptr<InstancedRectangle> modal;
	unique_ptr<Texture> addStepIcon;

	std::string randomText;

	vector<unique_ptr<Text>> steps;
	unique_ptr<TextInput> textInput;

	void createModal();
};
