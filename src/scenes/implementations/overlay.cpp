#include "overlay.hpp"
#include "assets.hpp"
#include "camera.hpp"
#include "colors.hpp"
#include "graph.hpp"
#include "scenes.hpp"
#include "shapes.hpp"
#include "textures.hpp"

using Kind = Graph::Kind;

Overlay::Overlay(Scenes &scenes) : Scene(scenes) {
	crosshair = Textures::icon(this, orthographic, screenParams, Assets::textureRootPath + "/crosshair/crosshair.png");

	Text::TextParams tp{};
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
				auto &u = graph->getMVP();
				const float w = screenParams.viewport.width;
				const float h = screenParams.viewport.height;

				// Place the camera above the scene looking down
				const float dist = 10.0f;
				u.view = glm::lookAt(glm::vec3(0.0f, 0.0f, dist),  // eye
									 glm::vec3(0.0f, 0.0f, 0.0f),  // center
									 glm::vec3(0.0f, 1.0f, 0.0f)); // up (must NOT be colinear with view)

                graph->swapChainUpdate();
			} else {
				is3D = true;
				disableMouseMode(); // capture cursor, go back to FPS controls
                graph->swapChainUpdate();
			}
		}
	});

	legend = Shapes::pentagons(this, orthographic, screenParams, 10 /*node types*/);
	float legendIconSize = 50.0f;
	float y = 50.0f;
	for (int i = 0; i != static_cast<int>(Kind::End); i++) {
		Kind k = static_cast<Kind>(i);
		vec4 color = Graph::colorFor(k);
		legend->updateInstance(i, InstancedPolygonData(vec3(50, y, 0), vec3(legendIconSize), color, Colors::Black));

		unique_ptr<Text> label = make_unique<Text>(this, orthographic, screenParams, tp);
		string labelContent = Graph::stringFor(k);
		float labelWidth = label->getPixelWidth(labelContent);
		label->color = Colors::Gray;
		label->text = labelContent;
		label->updateUniformBuffer(translate(mat4(1.0f), vec3(100 + labelWidth / 2, y, 0)));
		legendLabels.push_back(std::move(label));

		y += 60.0f;
	}
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
	crosshair->updateUniformBuffer(translate(mat4(1.0f), vec3(w * 0.5, h * 0.5, 0.0)) * scale(mat4(1.0f), vec3(crosshairSize)), std::nullopt, orthographic.proj);

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
	btn3DIcon->updateUniformBuffer(translate(mat4(1.0f), vec3(w - 50.0f, 50.0f, 0.0)) * scale(mat4(1.0f), vec3(perspIconSize)), std::nullopt, orthographic.proj);
	btn2DIcon->updateUniformBuffer(translate(mat4(1.0f), vec3(w - 50.0f, 50.0f, 0.0)) * scale(mat4(1.0f), vec3(perspIconSize)), std::nullopt, orthographic.proj);

	legend->updateUniformBuffer(std::nullopt, std::nullopt, orthographic.proj);
	for (const auto &l : legendLabels) {
		l->updateUniformBuffer(std::nullopt, std::nullopt, orthographic.proj);
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
