#pragma once

#include "instancedrectangle.hpp"
#include "scene.hpp"
#include "textinput.hpp"
#include "polygon.hpp"
#include "texture.hpp"

using namespace std::chrono;

class AddRecipeStep : public Scene {
  public:
	AddRecipeStep(Scenes &scenes, bool show = true);
	AddRecipeStep(AddRecipeStep &&) = delete;
	AddRecipeStep(const AddRecipeStep &) = delete;
	AddRecipeStep &operator=(AddRecipeStep &&) = delete;
	AddRecipeStep &operator=(const AddRecipeStep &) = delete;
	~AddRecipeStep() = default;

	std::string getName() override { return "AddRecipeStep"; }

	void updateScreenParams() override;

	void updateComputeUniformBuffers() override;
	void computePass() override;

	void updateUniformBuffers() override;
	void renderPass() override;
	void renderPass1() override;
	void swapChainUpdate() override;

  private:
	unique_ptr<TextInput> textInput;
	unique_ptr<InstancedRectangle> modal;

	unique_ptr<Polygon> closeBtn;
	unique_ptr<Texture> closeBtnIcon;
	bool closePressed = false;

	unique_ptr<Polygon> confirmBtn;
	unique_ptr<Texture> confirmBtnIcon;
	bool confirmPressed = false;

	void createModal();
};
