#include "overlay.hpp"
#include "assets.hpp"
#include "camera.hpp"
#include "colors.hpp"
#include "scenes.hpp"
#include "shapes.hpp"
#include "textures.hpp"

Overlay::Overlay(Scenes &scenes) : Scene(scenes) {
	crosshair = Textures::icon(this, orthographic, screenParams, Assets::textureRootPath + "/crosshair/crosshair.png");

	Text::FontParams tp{};
	perspectiveBtn = make_unique<Button>(this, orthographic, screenParams, tp);
	btn3DIcon = Textures::icon(this, orthographic, screenParams, Assets::textureRootPath + "/icons/3D.png");
	btn2DIcon = Textures::icon(this, orthographic, screenParams, Assets::textureRootPath + "/icons/2D.png");

	perspectiveBtn->setOnMouseClick([&](int button, int action, int mods) {
		if (action == Events::ACTION_PRESS && button == Events::MOUSE_BUTTON_LEFT) {
			auto graph = scenes.getScene("Graph");
			if (!graph)
				return;

			if (is3D) {
                is3D = false;
				graph->is3D = false;
				enableMouseMode(); // capture cursor, go back to FPS controls
				graph->swapChainUpdate();
			} else {
                is3D = true;
				graph->is3D = true;
				disableMouseMode(); // capture cursor, go back to FPS controls
				graph->swapChainUpdate();
			}
		}
	});

	legend = Shapes::pentagons(this, orthographic, screenParams, 10 /*node types*/);
	float legendIconSize = 50.0f;
	float y = 50.0f;
}

void Overlay::updateScreenParams() {
	screenParams.viewport.x = 0.0f;
	screenParams.viewport.y = 0.0f;
	screenParams.viewport.width = (float)Engine::swapChainExtent.width;
	screenParams.viewport.height = (float)Engine::swapChainExtent.height;
	screenParams.viewport.minDepth = 0.0f;
	screenParams.viewport.maxDepth = 1.0f;
	screenParams.scissor.offset = {(int32_t)screenParams.viewport.x, (int32_t)screenParams.viewport.y};
	screenParams.scissor.extent = {(uint32_t)screenParams.viewport.width, (uint32_t)screenParams.viewport.height};
}

void Overlay::swapChainUpdate() {
	auto w = screenParams.viewport.width;
	auto h = screenParams.viewport.height;
	orthographic.proj = ortho(0.0f, w, 0.0f, -h, -1.0f, 1.0f);

	float crosshairSize = 50.0f;
	crosshair->updateMVP(translate(mat4(1.0f), vec3(w * 0.5, h * 0.5, 0.0)) * scale(mat4(1.0f), vec3(crosshairSize)), std::nullopt, orthographic.proj);

	perspectiveBtn->updateUniformBuffers(orthographic);
	Button::StyleParams bp;
	bp.dim = {140.0f, 80.0f};
	bp.outlineWidth = 0.0f;
	bp.borderRadius = 16.0f;
	bp.text = is3D ? "3D" : "2D";
	bp.textColor = Colors::White;
	bp.bgColor = Colors::White(0.1);
	bp.outlineColor = Colors::Transparent;
	float textLength = perspectiveBtn->textModel->getPixelWidth(bp.text);
	bp.center = {screenParams.viewport.width - 75.0f, 50.0f};
	bp.textCenter = {screenParams.viewport.width - 90.0f - textLength / 2.0f, 50.0f};
	perspectiveBtn->setParams(bp);

	float perspIconSize = 50.0f;
	btn3DIcon->updateMVP(translate(mat4(1.0f), vec3(w - 50.0f, 50.0f, 0.0)) * scale(mat4(1.0f), vec3(perspIconSize)), std::nullopt, orthographic.proj);
	btn2DIcon->updateMVP(translate(mat4(1.0f), vec3(w - 50.0f, 50.0f, 0.0)) * scale(mat4(1.0f), vec3(perspIconSize)), std::nullopt, orthographic.proj);

	legend->updateMVP(std::nullopt, std::nullopt, orthographic.proj);
	for (const auto &l : legendLabels) {
		l->updateMVP(std::nullopt, std::nullopt, orthographic.proj);
	}
}

void Overlay::updateComputeUniformBuffers() {}

void Overlay::computePass() {}

void Overlay::updateUniformBuffers() {}

void Overlay::renderPass() {
	perspectiveBtn->render();
	legend->render();
	for (const auto &l : legendLabels) {
		l->render();
	}
	if (is3D) {
		crosshair->render();
		perspectiveBtn->styleParams.text = "3D";
		btn3DIcon->render();
	} else {
		perspectiveBtn->styleParams.text = "2D";
		btn2DIcon->render();
	}
}
