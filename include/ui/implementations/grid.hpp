#pragma once

#include "instancedrectangle.hpp"
#include "widget.hpp"

class Grid : public Widget {
  public:
	Grid();
	Grid(Grid &&) = delete;
	Grid(const Grid &) = delete;
	Grid &operator=(Grid &&) = delete;
	Grid &operator=(const Grid &) = delete;
	~Grid() = default;

	struct Params {
		vec2 cellSize = vec2(50.0f);
		vec4 cellColor = Colors::Gray(0.3);
		float cellBorderRadius = 25.0f;
		float gap = 12.0f;
		float scrollBarWidth = 24.0f;
		vec2 gridCenter = vec2(0.0f, 0.0f);
		vec2 gridDim = vec2(800.0f, 800.0f);
		vec4 sliderColor = Colors::Gray(0.55);
		vec4 sliderColorPressed = Colors::Gray;
		vec4 margins = vec4(0.0f);
		int numCols = -1; // dynamic;
	};

	Grid(Scene *scene, const Model::MVP &mvp, Model::ScreenParams &screenParams, const VkRenderPass &renderPass = Engine::renderPass);

	void swapChainUpdate();
	void updateScreenParams();
	void updateUniformBuffers();
	void render() override;

	bool enableControls = true;

	Params params{};
	size_t numItems = 0;

	unique_ptr<InstancedRectangle> grid;
	unique_ptr<InstancedRectangle> scrollBar;

	std::function<void(int idx, float x, float y, vec2 cellSize, Model::MVP gridMVP)> setGridItem;

	Model::ScreenParams sp;
	Model::ScreenParams bgSp;

  private:
	int rowsUsed = 0;
	float gridW;
	float gridH;

	InstancedRectangleData slider{};

	void createGrid();

	void createScrollBar();
	void updateSlider();
	void dragSliderToCursor();
	void mouseDragY(float &scrollMinY, float &scrollMaxY, bool inverted);
};
