#include "recipes.hpp"
#include "colors.hpp"
#include "engine.hpp"
#include "events.hpp"
#include "textures.hpp"

Recipes::Recipes(Scenes &scenes, bool show) : Scene(scenes, show) {
	mvp = {mat4(1.0f), mat4(1.0f), ortho(0.0f, float(Engine::swapChainExtent.width), 0.0f, -float(Engine::swapChainExtent.height), -1.0f, 1.0f)};
	camPosOrtho = glm::vec3(0.0f);
	lookAtCoords = glm::vec3(0.0f);
	zoom = 1.0f;

	addRecipeIcon = Textures::icon(this, mvp, spGrid, Assets::textureRootPath + "/icons/addfile.png", Engine::renderPass1);

	auto itemInstances = std::make_shared<std::unordered_map<int, InstancedRectangleData>>();
	grid = make_unique<InstancedRectangle>(this, mvp, spGrid, itemInstances);
	grid->enableBlur(false);
	grid->blur->shaderPath = Assets::shaderRootPath + "/instanced/blur/irectblur/";
	grid->blur->initialize();
	grid->enableRayTracing(true);
	grid->setOnMouseClick([&](int button, int action, int mods) {
        if (!this->show) {
            return;
        }
		if (action == Events::ACTION_PRESS && button == Events::MOUSE_BUTTON_LEFT) {
			int id = grid->rayTracing->hitMapped->primId;
			if (id == numItems) {
				auto style = grid->getInstance(id);
				style.color = Colors::Gray(0.5);
				grid->updateInstance(id, style);
			}
		} else if (action == Events::ACTION_RELEASE && button == Events::MOUSE_BUTTON_LEFT) {
			int id = grid->rayTracing->hitMapped->primId;
			if (id == numItems) {
				auto style = grid->getInstance(id);
				style.color = Colors::Gray(0.1);
				grid->updateInstance(id, style);
			}
		}
	});

	auto barElements = std::make_shared<std::unordered_map<int, InstancedRectangleData>>(4);
	scrollBar = make_unique<InstancedRectangle>(this, mvp, spGrid, barElements, 4);
	scrollBar->enableRayTracing(true);
	scrollBar->setOnMouseClick([&](int button, int action, int mods) {
        if (!this->show) {
            return;
        }
		if (action == Events::ACTION_PRESS && button == Events::MOUSE_BUTTON_LEFT) {
			int id = scrollBar->rayTracing->hitMapped->primId;
			if (id == 1) {
				usingSlider = true;
				slider.color = Colors::Gray;
			}
		}
	});
	Events::mouseCallbacks.push_back([&](int button, int action, int mods) {
        if (!this->show) {
            return;
        }
		if (action == Events::ACTION_RELEASE && button == Events::MOUSE_BUTTON_LEFT) {
			slider.color = Colors::Gray(0.55f);
			usingSlider = false;
		}
	});
}

void Recipes::updateScreenParams() {
	screenParams.viewport.x = 0.0f;
	screenParams.viewport.y = 0.0f;
	screenParams.viewport.width = (float)Engine::swapChainExtent.width;
	screenParams.viewport.height = (float)Engine::swapChainExtent.height;
	screenParams.viewport.minDepth = 0.0f;
	screenParams.viewport.maxDepth = 1.0f;
	screenParams.scissor.offset = {(int32_t)screenParams.viewport.x, (int32_t)screenParams.viewport.y};
	screenParams.scissor.extent = {(uint32_t)screenParams.viewport.width, (uint32_t)screenParams.viewport.height};

	const float swapW = screenParams.viewport.width;
	const float swapH = screenParams.viewport.height;

	gridX = 25.0f;				   // left padding
	gridY = 100.0f;				   // top padding
	gridW = swapW - gridX - 25.0f; // right padding
	gridH = swapH - gridY - 25.0f; // bottom padding

	// Clamp so the viewport/scissor never exceed framebuffer bounds
	const float maxW = std::max(0.0f, swapW - gridX);
	const float maxH = std::max(0.0f, swapH - gridY);
	gridW = std::min(gridW, maxW);
	gridH = std::min(gridH, maxH);

	spGrid.viewport.x = gridX;
	spGrid.viewport.y = gridY;
	spGrid.viewport.width = gridW;
	spGrid.viewport.height = gridH;

	spGrid.scissor.offset = {(int32_t)std::floor(gridX), (int32_t)std::floor(gridY)};
	spGrid.scissor.extent = {(uint32_t)std::floor(gridW), (uint32_t)std::floor(gridH)};

	gw = spGrid.viewport.width;
	gh = spGrid.viewport.height;

	pitch = kCellSize + kGap;
	padL = kCellSize * kPadScale;
	padT = kCellSize * kPadScale;
	padR = 0.0f;

	baseW = gridW;
	baseH = gridH;
}

void Recipes::createGrid() {
	// Grid-local placement
	float curX = padL;
	float curY = padT;

	const float rightEdge = std::max(padL + kCellSize, gw - padR);

	int lastRow = 0;
	for (size_t i = 0; i <= numItems; ++i) {
		if (curX + kCellSize + kScrollBarWidth + kGap > rightEdge + kEps + padL) {
			curX = padL;
			curY += pitch;
			++lastRow;
		}

		const float x = curX;
		const float y = curY;

		InstancedRectangleData fr{};
		fr.color = Colors::Gray(0.1f);
		fr.borderRadius = 25.0f;
		fr.model = glm::translate(glm::mat4(1.0f), glm::vec3(x, y, 0.0f)) * glm::scale(glm::mat4(1.0f), glm::vec3(kCellSize, kCellSize, 1.0f));
		grid->updateInstance((int)i, fr);

		if (i == numItems) {
			addRecipeIcon->updateMVP(translate(mat4(1.0f), vec3(x, y, 0.0f)) * scale(mat4(1.0f), vec3(kCellSize * 0.6, kCellSize * 0.6, 1.0f)), mvp.view, mvp.proj);
		}

		curX += kCellSize + kGap;
	}

	rowsUsed = (numItems > 0) ? (lastRow + 1) : 0;
	contentH = (rowsUsed > 0) ? (padT + (rowsUsed - 1) * pitch + kCellSize) : 0.0f;

	const float maxTop = std::max(0.0f, contentH - gridH);
	const float desiredTop = lookAtCoords.y;
	const float snappedTop = glm::clamp(std::floor(desiredTop / pitch) * pitch, 0.0f, maxTop);

	mvp.view = glm::translate(glm::mat4(1.0f), glm::vec3(-lookAtCoords.x, -snappedTop, 0.0f));
	grid->updateMVP(std::nullopt, mvp.view, mvp.proj);
}

void Recipes::createScrollBar() {
	// Scroll limits from content
	scrollMinY = 0.0f;
	scrollMaxY = std::max(0.0f, contentH - gh);

	// Track geometry (cached for updateSlider)
	sbW = kScrollBarWidth;
	btnH = kScrollBarWidth;
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

void Recipes::updateSlider() {
	const float innerH = std::max(0.0f, trackH - 2.0f * btnH);

	float thumbH = (contentH <= gh || innerH <= 0.0f) ? innerH : std::max(kMinThumb, innerH * (gh / std::max(gh, contentH))) / 2;

	float scrollY = glm::clamp(lookAtCoords.y, scrollMinY, scrollMaxY);
	float t = (scrollMaxY > 0.0f && innerH > thumbH) ? (scrollY / scrollMaxY) : 0.0f;
	float thumbY = btnH + t * (innerH - thumbH);

	slider.model = glm::translate(glm::mat4(1.0f), glm::vec3(trackX, thumbY + thumbH / 2, 0.0f)) * glm::scale(glm::mat4(1.0f), glm::vec3(sbW, thumbH, 1.0f));
	scrollBar->updateInstance(1, slider);
}

void Recipes::dragSliderToCursor() {
	GLFWwindow *w = Engine::window;
	if (!w || !usingSlider)
		return;

	// Convert cursor (window coords) -> framebuffer coords
	double mx, my;
	glfwGetCursorPos(w, &mx, &my);

	float xscale = 1.0f, yscale = 1.0f;
	glfwGetWindowContentScale(w, &xscale, &yscale); // e.g., 2.0 on Retina

	// Cursor in framebuffer-space, same space as spGrid.viewport/gw/gh
	const float cursorFbY = float(my) * yscale;

	// Grid-local Y (framebuffer space)
	const float localY = cursorFbY - spGrid.viewport.y;

	// --- Mirror updateSlider() exactly (framebuffer units) ---
	const float innerH = std::max(0.0f, gh - 2.0f * btnH);
	float thumbH = (contentH <= gh || innerH <= 0.0f) ? innerH : std::max(kMinThumb, innerH * (gh / std::max(gh, contentH))) / 2.0f;

	if (innerH <= 0.0f || thumbH <= 0.0f || scrollMaxY <= scrollMinY + 1e-6f)
		return;

	const float minCenter = btnH + thumbH * 0.5f;
	const float maxCenter = gh - btnH - thumbH * 0.5f;

	// Clamp cursor to the thumb center travel range
	const float centerY = glm::clamp(localY, minCenter, maxCenter);

	// center -> t -> scrollY (same inverse as updateSlider, no mysterious *2)
	const float denom = std::max(1e-6f, innerH - thumbH);
	const float t = (centerY - minCenter) / denom; // 0..1
	const float newY = scrollMinY + t * (scrollMaxY - scrollMinY);

	const float clamped = glm::clamp(newY, scrollMinY, scrollMaxY);
	camPosOrtho.y = clamped;
	lookAtCoords.y = clamped;
	camTarget.y = clamped;
}

void Recipes::swapChainUpdate() {
	mvp.proj = ortho(0.0f, gridW, 0.0f, -gridH, -1.0f, 1.0f);
	createGrid();
	createScrollBar();
}

void Recipes::updateComputeUniformBuffers() {}

void Recipes::computePass() {}

void Recipes::updateUniformBuffers() {
	if (!usingSlider) {
		scrollBarMouseControls(scrollMinY, scrollMaxY, /*inverted=*/true);
	} else {
		dragSliderToCursor();
	}
	mvp.view = translate(mat4(1.0f), vec3(-lookAtCoords.x, -lookAtCoords.y, 0.0f));
	grid->updateMVP(std::nullopt, mvp.view);
	updateSlider();
	addRecipeIcon->updateMVP(std::nullopt, mvp.view);
}

void Recipes::renderPass() { scrollBar->render(); }

void Recipes::renderPass1() {
	grid->render();
	addRecipeIcon->render();
}
