#pragma once

#include "instancedrectangle.hpp"
#include "scene.hpp"
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
	Model::MVP sceneMVP;

	unique_ptr<InstancedRectangle> steps;
	unique_ptr<InstancedRectangle> modal;
	unique_ptr<InstancedRectangle> scrollBar;

	unique_ptr<Texture> addStepIcon;

	Model::ScreenParams spGrid{};
	float gridX;
	float gridY;
	float gridW;
	float gridH;

	float scrollMinY = 0.0f;
	float scrollMaxY = 0.0f;

	// ---- Layout constants (tweak here, used everywhere) ----
	float kCellSizeW = 300.0f;
	float kCellSizeH = 150.0f;
	static constexpr float kGap = 12.0f;
	static constexpr float kPadScale = 0.5f; // pad = kCellSize * kPadScale
	static constexpr float kScrollBarWidth = 24.0f;
	static constexpr float kMinThumb = 24.0f;
	static constexpr float kEps = 1e-4f;

	// ---- Derived per-frame / swapchain (shared across functions) ----
	float pitch = kCellSizeW + kGap;

	float padL = 0.0f;
	float padR = 0.0f;
	float padT = 0.0f;

	// Cached grid viewport size (grid-local)
	float gw = 0.0f;
	float gh = 0.0f;

	// Content metrics (for scroll range & thumb sizing)
	size_t numItems = 10; // set dynamically if needed
	int rowsUsed = 0;
	float contentH = 0.0f;

	// Scrollbar geometry cached so updateSlider() can use it
	float sbW = kScrollBarWidth;
	float btnH = kScrollBarWidth; // square buttons
	float trackX = 0.0f;
	float trackY = 0.0f;
	float trackH = 0.0f;

	bool usingSlider = false;

	InstancedRectangleData slider{};

	void createModal();
	void createGrid();
	void createScrollBar();
	void updateSlider();
	void dragSliderToCursor();
};
