#pragma once

#include "image.hpp"
#include "instancedrectangle.hpp"
#include "polygon.hpp"
#include "scene.hpp"
#include "texture.hpp"

class AddIngredient : public Scene {
  public:
	AddIngredient(Scenes &scenes, bool show = true);
	AddIngredient(AddIngredient &&) = delete;
	AddIngredient(const AddIngredient &) = delete;
	AddIngredient &operator=(AddIngredient &&) = delete;
	AddIngredient &operator=(const AddIngredient &) = delete;
	~AddIngredient() = default;

	std::string getName() override { return "AddIngredient"; }

	void updateScreenParams() override;

	void updateComputeUniformBuffers() override;
	void computePass() override;

	void swapChainUpdate() override;
	void updateUniformBuffers() override;
	void renderPass() override;
	void renderPass1() override;

  private:
	unique_ptr<InstancedRectangle> modal;
	unique_ptr<Image> image;

	unique_ptr<Polygon> closeBtn;
	unique_ptr<Texture> closeBtnIcon;
	bool closePressed = false;

	unique_ptr<Polygon> confirmBtn;
	unique_ptr<Texture> confirmBtnIcon;
	bool confirmPressed = false;

	void createModal();
};
