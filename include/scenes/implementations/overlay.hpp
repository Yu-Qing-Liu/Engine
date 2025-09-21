#pragma once

#include "button.hpp"
#include "engine.hpp"
#include "instancedpolygon.hpp"
#include "scene.hpp"
#include "texture.hpp"

using std::make_unique;

class Overlay : public Scene {
  public:
	Overlay(Scenes &scenes);
	Overlay(Overlay &&) = delete;
	Overlay(const Overlay &) = delete;
	Overlay &operator=(Overlay &&) = delete;
	Overlay &operator=(const Overlay &) = delete;
	~Overlay() = default;

	std::string getName() override { return "Overlay"; }

	void updateScreenParams() override;

	void updateComputeUniformBuffers() override;
	void computePass() override;

	void updateUniformBuffers() override;
	void renderPass() override;
	void swapChainUpdate() override;

  private:
	Model::UBO orthographic{mat4(1.0f), mat4(1.0f), ortho(0.0f, float(Engine::swapChainExtent.width), 0.0f, -float(Engine::swapChainExtent.height), -1.0f, 1.0f)};

	unique_ptr<InstancedPolygon> legend;
	vector<unique_ptr<Text>> legendLabels;

	unique_ptr<Texture> crosshair;
	unique_ptr<Button> perspectiveBtn;
	unique_ptr<Texture> btn3DIcon;
	unique_ptr<Texture> btn2DIcon;
};
