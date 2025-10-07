#include "grid.hpp"
#include "engine.hpp"
#include "scene.hpp"

Grid::Grid(Scene *scene, const Model::MVP &mvp, Model::ScreenParams &screenParams) : Widget(scene, mvp, screenParams) {
	auto itemInstances = std::make_shared<std::unordered_map<int, InstancedRectangleData>>();
	grid = make_unique<InstancedRectangle>(scene, mvp, sp, itemInstances);

	auto barElements = std::make_shared<std::unordered_map<int, InstancedRectangleData>>(4);
	scrollBar = make_unique<InstancedRectangle>(scene, mvp, sp, barElements, 4);
	scrollBar->enableRayTracing(true);
	scrollBar->setOnMouseClick([&](int button, int action, int mods) {
		if (!enableControls) {
			return;
		}
		if (action == Events::ACTION_PRESS && button == Events::MOUSE_BUTTON_LEFT) {
			int id = scrollBar->rayTracing->hitMapped->primId;
			if (id == 1) {
				usingSlider = true;
				slider.color = styleParams.sliderColorPressed;
			}
		}
	});
	Events::mouseCallbacks.push_back([&](int button, int action, int mods) {
		if (!enableControls) {
			return;
		}
		if (action == Events::ACTION_RELEASE && button == Events::MOUSE_BUTTON_LEFT) {
			slider.color = styleParams.sliderColor;
			usingSlider = false;
		}
	});
}

void Grid::updateScreenParams() {
	const float swapW = screenParams.viewport.width;
	const float swapH = screenParams.viewport.height;

	// ----- Inner (grid only) -----
	const float desiredX = styleParams.gridCenter.x - styleParams.gridDim.x * 0.5f;
	const float desiredY = styleParams.gridCenter.y - styleParams.gridDim.y * 0.5f;
	const float desiredW = styleParams.gridDim.x;
	const float desiredH = styleParams.gridDim.y;

	const float ix = std::max(0.0f, desiredX);
	const float iy = std::max(0.0f, desiredY);
	const float ix2 = std::min(desiredX + desiredW, swapW);
	const float iy2 = std::min(desiredY + desiredH, swapH);
	const float iW = std::max(0.0f, ix2 - ix);
	const float iH = std::max(0.0f, iy2 - iy);

	gridW = desiredW;
	gridH = desiredH;

	sp.viewport.x = ix;
	sp.viewport.y = iy;
	sp.viewport.width = iW;
	sp.viewport.height = iH;
	// keep whatever minDepth/maxDepth you already use elsewhere
	sp.scissor.offset = {(int32_t)std::floor(ix), (int32_t)std::floor(iy)};
	sp.scissor.extent = {(uint32_t)std::floor(iW), (uint32_t)std::floor(iH)};

	// ----- Outer (grid + margins) -> bgSp -----
	const float mL = styleParams.margins.x;
	const float mT = styleParams.margins.y;
	const float mR = styleParams.margins.z;
	const float mB = styleParams.margins.w;

	const float ex = desiredX - mL;
	const float ey = desiredY - mT;
	const float eW = desiredW + mL + mR;
	const float eH = desiredH + mT + mB;

	const float bix = std::max(0.0f, ex);
	const float biy = std::max(0.0f, ey);
	const float bix2 = std::min(ex + eW, swapW);
	const float biy2 = std::min(ey + eH, swapH);
	const float bW = std::max(0.0f, bix2 - bix);
	const float bH = std::max(0.0f, biy2 - biy);

	bgSp.viewport.x = bix;
	bgSp.viewport.y = biy;
	bgSp.viewport.width = bW;
	bgSp.viewport.height = bH;
	bgSp.viewport.minDepth = sp.viewport.minDepth; // or 0.0f
	bgSp.viewport.maxDepth = sp.viewport.maxDepth; // or 1.0f
	bgSp.scissor.offset = {(int32_t)std::floor(bix), (int32_t)std::floor(biy)};
	bgSp.scissor.extent = {(uint32_t)std::floor(bW), (uint32_t)std::floor(bH)};
}

void Grid::dragSliderToCursor() {
	GLFWwindow *w = Engine::window;
	if (!w)
		return;

	double mx, my;
	glfwGetCursorPos(w, &mx, &my);

	float xscale = 1.0f, yscale = 1.0f;
	glfwGetWindowContentScale(w, &xscale, &yscale); // e.g., 2.0 on Retina

	const float cursorFbY = float(my) * yscale;

	const float localY = cursorFbY - sp.viewport.y;

	const auto gh = styleParams.gridDim.y;
	const auto btnH = styleParams.scrollBarWidth;

	const float innerH = std::max(0.0f, gh - 2.0f * btnH);
	float thumbH = (contentH <= gh || innerH <= 0.0f) ? innerH : std::max(btnH, innerH * (gh / std::max(gh, contentH))) / 2.0f;

	if (innerH <= 0.0f || thumbH <= 0.0f || scrollMaxY <= scrollMinY + 1e-6f)
		return;

	const float minCenter = btnH + thumbH * 0.5f;
	const float maxCenter = gh - btnH - thumbH * 0.5f;

	const float centerY = glm::clamp(localY, minCenter, maxCenter);

	const float denom = std::max(1e-6f, innerH - thumbH);
	const float t = (centerY - minCenter) / denom; // 0..1
	const float newY = scrollMinY + t * (scrollMaxY - scrollMinY);

	const float clamped = glm::clamp(newY, scrollMinY, scrollMaxY);
	camPosOrtho.y = clamped;
	lookAtCoords.y = clamped;
	camTarget.y = clamped;
}

void Grid::applyVerticalDeltaClamped(float dy, float minY, float maxY) {
	// Clamp to [scrollMinY, scrollMaxY]
	const float proposed = lookAtCoords.y + dy;
	const float clamped = glm::clamp(proposed, minY, maxY);
	const float applied = clamped - lookAtCoords.y;

	camPosOrtho.y += applied;
	lookAtCoords.y += applied;
	camTarget.y += applied;
}

void Grid::mouseDragY(float &scrollMinY, float &scrollMaxY, bool inverted) {
	GLFWwindow *win = Engine::window;
	if (!win || !glfwGetWindowAttrib(win, GLFW_FOCUSED))
		return;

	// Remember the initial Y once (top-most allowed position)
	if (!s_initY) {
		s_initialY = camPosOrtho.y;
		s_initY = true;
	}

	// Hook wheel: vertical pan only (no zoom)
	if (!s_hookedScroll) {
		Events::scrollCallbacks.push_back([&, inverted](double /*xoff*/, double yoff) {
			if (!enableControls || usingSlider)
				return;

			double cx, cy;
			glfwGetCursorPos(Engine::window, &cx, &cy);
			float xs = 1.f, ys = 1.f;
			glfwGetWindowContentScale(Engine::window, &xs, &ys);
			const float fx = float(cx) * xs;
			const float fy = float(cy) * ys;
			const bool inside = fx >= sp.viewport.x && fx <= sp.viewport.x + sp.viewport.width && fy >= sp.viewport.y && fy <= sp.viewport.y + sp.viewport.height;
			if (!inside)
				return;

			if (gridH == 0.0f || gridW == 0.0f)
				return;

			int winW = 0, winH = 0;
			glfwGetWindowSize(Engine::window, &winW, &winH);
			if (winW <= 0 || winH <= 0)
				return;

			const float aspect = sp.viewport.height > 0.f ? (sp.viewport.width / sp.viewport.height) : 1.0f;
			const float visH = gridH / glm::max(zoom, 1e-6f);
			const float visW = gridW / glm::max(zoom, 1e-6f);
			const float effH = (aspect >= 1.0f) ? (visW / aspect) : visH;

			const float worldPerPxY = effH / float(winH);
			const float scrollPixels = 40.0f;
			const float dyWorld = float(yoff) * worldPerPxY * scrollPixels;

			applyVerticalDeltaClamped(inverted ? -dyWorld : dyWorld, scrollMinY, scrollMaxY);
		});
		s_hookedScroll = true;
	}

	{
		double mx, my;
		glfwGetCursorPos(win, &mx, &my);
		float xs = 1.f, ys = 1.f;
		glfwGetWindowContentScale(win, &xs, &ys);
		const float fx = float(mx) * xs;
		const float fy = float(my) * ys;

		const bool inside = fx >= sp.viewport.x && fx <= sp.viewport.x + sp.viewport.width - styleParams.scrollBarWidth - styleParams.gap && fy >= sp.viewport.y && fy <= sp.viewport.y + sp.viewport.height;

		if (!inside) {
			// reset anchor so there’s no jump when re-entering
			lastPointerX = -1.0;
			lastPointerY = -1.0;
			return;
		}
	}

	// Drag-to-pan (vertical only)
	double mx = 0.0, my = 0.0;
	glfwGetCursorPos(win, &mx, &my);
	const bool lmb = glfwGetMouseButton(win, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;

	if (lastPointerX < 0.0 || lastPointerY < 0.0) {
		lastPointerX = mx;
		lastPointerY = my;
		return;
	}

	const double dy = my - lastPointerY;
	lastPointerX = mx;
	lastPointerY = my;

	if (!lmb)
		return;

	int winW = 0, winH = 0;
	glfwGetWindowSize(win, &winW, &winH);
	if (winW <= 0 || winH <= 0)
		return;

	const float aspect = sp.viewport.height > 0.f ? (sp.viewport.width / sp.viewport.height) : 1.0f;
	const float visH = gridH / glm::max(zoom, 1e-6f);
	const float visW = gridW / glm::max(zoom, 1e-6f);
	const float effH = (aspect >= 1.0f) ? (visW / aspect) : visH;
	const float worldPerPxY = effH / float(winH);

	applyVerticalDeltaClamped(inverted ? -float(dy) * worldPerPxY : float(dy) * worldPerPxY, scrollMinY, scrollMaxY);
}

void Grid::createGrid() {
	float curX = 0;
	float curY = 0;
	const auto gw = styleParams.gridDim.x;
	const auto gh = styleParams.gridDim.y;
	const auto cellSizeW = styleParams.cellSize.x;
	const auto cellSizeH = styleParams.cellSize.y;
	const auto scrollBarWidth = styleParams.scrollBarWidth;
	const auto gap = styleParams.gap;
	const auto pitch = cellSizeH + gap;

	const float rightEdge = std::max(cellSizeW, gw);

	int lastRow = 0;
	for (size_t i = 0; i <= numItems; ++i) {
		if (styleParams.numCols != -1 || curX + cellSizeW + scrollBarWidth + gap > rightEdge) {
			curX = 0;
			++lastRow;
		}

		const float x = curX + cellSizeW * 0.5;
		const float y = curY + cellSizeH * 0.5;

		InstancedRectangleData fr{};
		fr.color = styleParams.cellColor;
		fr.borderRadius = styleParams.cellBorderRadius;
		fr.model = glm::translate(glm::mat4(1.0f), glm::vec3(x, y, 0.0f)) * glm::scale(glm::mat4(1.0f), glm::vec3(cellSizeW, cellSizeH, 1.0f));
		grid->updateInstance((int)i, fr);

		if (setGridItem) {
			setGridItem(i, x, y, styleParams.cellSize, mvp);
		}

		curX += cellSizeW + gap;

		if (styleParams.numCols != -1 || curX + cellSizeW + scrollBarWidth + gap > rightEdge) {
			curY += pitch;
		}
	}

	rowsUsed = (numItems > 0) ? (lastRow + 1) : 0;
	contentH = (rowsUsed > 0) ? ((rowsUsed - 1) * pitch + cellSizeH) : 0.0f;

	const float maxTop = std::max(0.0f, contentH - gridH);
	const float desiredTop = lookAtCoords.y;
	const float snappedTop = glm::clamp(std::floor(desiredTop / pitch) * pitch, 0.0f, maxTop);

	mvp.view = glm::translate(glm::mat4(1.0f), glm::vec3(-lookAtCoords.x, -snappedTop, 0.0f));
	grid->updateMVP(std::nullopt, mvp.view, mvp.proj);
}

void Grid::createScrollBar() {
	const auto sbW = styleParams.scrollBarWidth;
	const auto btnH = styleParams.scrollBarWidth;
	const auto gw = styleParams.gridDim.x;
	const auto gh = styleParams.gridDim.y;

	// Scroll limits from content
	scrollMinY = 0.0f;
	scrollMaxY = std::max(0.0f, contentH - gh);

	// Track geometry (cached for updateSlider)
	trackX = gw - sbW;
	trackY = 0.0f;
	trackH = gh;

	// Track/container (idx 0)
	{
		InstancedRectangleData r{};
		r.color = Colors::Gray(0.05f);
		r.borderRadius = 6.0f;
		r.model = glm::translate(glm::mat4(1.0f), glm::vec3(trackX, trackY, -0.1f)) * glm::scale(glm::mat4(1.0f), glm::vec3(sbW, trackH * 2, 1.0f));
		scrollBar->updateInstance(0, r);
	}

	// Up button (idx 2)
	{
		InstancedRectangleData r{};
		r.color = Colors::Gray(0.35f);
		r.borderRadius = 6.0f;
		r.model = glm::translate(glm::mat4(1.0f), glm::vec3(trackX, btnH / 2, -0.1f)) * glm::scale(glm::mat4(1.0f), glm::vec3(sbW, btnH, 1.0f));
		scrollBar->updateInstance(2, r);
	}

	// Down button (idx 3)
	{
		InstancedRectangleData r{};
		r.color = Colors::Gray(0.35f);
		r.borderRadius = 6.0f;
		r.model = glm::translate(glm::mat4(1.0f), glm::vec3(trackX, gh - btnH / 2, -0.1f)) * glm::scale(glm::mat4(1.0f), glm::vec3(sbW, btnH, 1.0f));
		scrollBar->updateInstance(3, r);
	}

	// Thumb (idx 1)
	slider.borderRadius = 25.0f;
	slider.color = Colors::Gray(0.55f);
	updateSlider();

	// Pin to viewport (identity view for bar)
	scrollBar->updateMVP(std::nullopt, glm::mat4(1.0f), mvp.proj);
}

void Grid::updateSlider() {
	const auto sbW = styleParams.scrollBarWidth;
	const auto btnH = styleParams.scrollBarWidth;
	const auto gw = styleParams.gridDim.x;
	const auto gh = styleParams.gridDim.y;

	const float innerH = std::max(0.0f, trackH - 2.0f * btnH);

	float thumbH = (contentH <= gh || innerH <= 0.0f) ? innerH : std::max(sbW, innerH * (gh / std::max(gh, contentH))) / 2;

	float scrollY = glm::clamp(lookAtCoords.y, scrollMinY, scrollMaxY);
	float t = (scrollMaxY > 0.0f && innerH > thumbH) ? (scrollY / scrollMaxY) : 0.0f;
	float thumbY = btnH + t * (innerH - thumbH);

	slider.model = glm::translate(glm::mat4(1.0f), glm::vec3(trackX, thumbY + thumbH / 2, 0.0f)) * glm::scale(glm::mat4(1.0f), glm::vec3(sbW, thumbH, 1.0f));
	scrollBar->updateInstance(1, slider);
}

void Grid::swapChainUpdate() {
	updateScreenParams();
	mvp.proj = ortho(0.0f, styleParams.gridDim.x, 0.0f, -styleParams.gridDim.y, -1.0f, 1.0f);
	createGrid();
	createScrollBar();
}

void Grid::updateUniformBuffers() {
	if (enableControls) {
		if (!usingSlider) {
			mouseDragY(scrollMinY, scrollMaxY, /*inverted=*/true);
		} else {
			dragSliderToCursor();
		}
	}
	mvp.view = translate(mat4(1.0f), vec3(-lookAtCoords.x, -lookAtCoords.y, 0.0f));
	grid->updateMVP(std::nullopt, this->mvp.view);
	updateSlider();
}

void Grid::render() {
	grid->render();
	scrollBar->render();
}
