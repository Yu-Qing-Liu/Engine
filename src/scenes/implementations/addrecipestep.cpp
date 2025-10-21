#include "addrecipestep.hpp"
#include "recipe.hpp"
#include "scenes.hpp"
#include "shapes.hpp"
#include "textures.hpp"

AddRecipeStep::AddRecipeStep(Scenes &scenes, bool show) : Scene(scenes, show) {
	mvp = {mat4(1.0f), mat4(1.0f), ortho(0.0f, float(Engine::swapChainExtent.width), 0.0f, -float(Engine::swapChainExtent.height), -1.0f, 1.0f)};

	auto mInstances = std::make_shared<std::unordered_map<int, InstancedRectangleData>>();
	modal = make_unique<InstancedRectangle>(this, mvp, screenParams, mInstances, 2);
	modal->enableBlur(Assets::shaderRootPath + "/instanced/blur/irectblur/");

	Text::FontParams fp{};
	textInput = make_unique<TextInput>(this, modal->mvp, modal->screenParams, fp, Engine::renderPass1);

	closeBtnIcon = Textures::icon(this, modal->mvp, modal->screenParams, Assets::textureRootPath + "/icons/close.png", Engine::renderPass1);
	closeBtn = Shapes::polygon2D(this, modal->mvp, modal->screenParams, 64, Engine::renderPass1);
	closeBtn->enableRayTracing(true);

	closeBtn->setOnMouseClick([&](int button, int action, int /*mods*/) {
		if (!this->show || disabled)
			return;
		if (button != Events::MOUSE_BUTTON_LEFT)
			return;

		if (action == Events::ACTION_PRESS) {
			// record that this button was actually pressed
			closePressed = true;
			return;
		}

		if (action == Events::ACTION_RELEASE) {
			if (!closePressed)
				return;			  // ignore stray releases (e.g. from prior scene)
			closePressed = false; // reset for next time

			this->show = false; // hide this scene
			const auto &sc = scenes.getScene("Recipe");
			if (sc) {
				sc->enable();
			}
		}
	});

	confirmBtnIcon = Textures::icon(this, modal->mvp, modal->screenParams, Assets::textureRootPath + "/icons/confirm.png", Engine::renderPass1);
	confirmBtn = Shapes::polygon2D(this, modal->mvp, modal->screenParams, 64, Engine::renderPass1);
	confirmBtn->enableRayTracing(true);

	confirmBtn->setOnMouseClick([&](int button, int action, int /*mods*/) {
		if (!this->show || disabled)
			return;
		if (button != Events::MOUSE_BUTTON_LEFT)
			return;

		if (action == Events::ACTION_PRESS) {
			// record that this button was actually pressed
			confirmPressed = true;
			return;
		}

		if (action == Events::ACTION_RELEASE) {
			if (!confirmPressed)
				return;				// ignore stray releases (e.g. from prior scene)
			confirmPressed = false; // reset for next time

			this->show = false; // hide this scene
			const auto &sc = scenes.getScene("Recipe");
			if (!sc) {
				return;
			}

			if (!textInput->text.empty()) {
				auto r = dynamic_cast<Recipe *>(sc.get());
				RecipesQueries::Step step{};
				step.instruction = textInput->text;
				r->recipe.steps.emplace_back(std::move(step));
			}

			sc->enable();
		}
	});
}

void AddRecipeStep::updateScreenParams() {
	screenParams.viewport.x = 0.0f;
	screenParams.viewport.y = 0.0f;
	screenParams.viewport.width = (float)Engine::swapChainExtent.width;
	screenParams.viewport.height = (float)Engine::swapChainExtent.height;
	screenParams.viewport.minDepth = 0.0f;
	screenParams.viewport.maxDepth = 1.0f;
	screenParams.scissor.offset = {(int32_t)screenParams.viewport.x, (int32_t)screenParams.viewport.y};
	screenParams.scissor.extent = {(uint32_t)screenParams.viewport.width, (uint32_t)screenParams.viewport.height};
}

void AddRecipeStep::createModal() {
	const float w = screenParams.viewport.width;
	const float h = screenParams.viewport.height;

	// Build a projection in the modal’s own viewport space
	auto projLocal = ortho(0.0f, w, 0.0f, -h, -1.0f, 1.0f);

	InstancedRectangleData m{};
	m.color = Colors::DarkBlue(0.8f);
	m.borderRadius = 25.0f;
	m.model = translate(mat4(1.0f), vec3(w * 0.5f, h * 0.5f, 0.0f)) * scale(mat4(1.0f), vec3(w * 0.8, h * 0.6, 1.0f));

	modal->updateInstance(0, m);
	// Pin to viewport: identity view; local projection
	modal->updateMVP(std::nullopt, mat4(1.0f), projLocal);
}

void AddRecipeStep::swapChainUpdate() {
	auto w = screenParams.viewport.width;
	auto h = screenParams.viewport.height;
	auto mw = w * 0.9f;
	auto mh = h * 0.2f;
	mvp = {mat4(1.0f), mat4(1.0f), ortho(0.0f, w, 0.0f, -h, -1.0f, 1.0f)};

	createModal();

	const glm::mat4 projLocal = ortho(0.0f, w, 0.0f, -h, -1.0f, 1.0f);
	const glm::mat4 viewLocal = glm::mat4(1.0f);

	const float inset = 15.0f;
	const float btnSize = 35.0f;
	const float iconSize = 15.0f;

	textInput->params.center = vec2(w * 0.5f, h * 0.5f + inset * 0.4 + btnSize / 2);
	textInput->params.dim = vec2(w * 0.8f - inset * 2, h * 0.6 - inset * 3 - btnSize);
	textInput->textField->params.scrollBarWidth = 8.0f;
	textInput->textField->params.padding = vec4(20.0f, 20.0f, 0.0f, 0.0f);
	textInput->mvp = modal->mvp;
	textInput->swapChainUpdate();

	closeBtn->params.color = Colors::DarkRed;
	closeBtn->params.outlineColor = Colors::DarkRed;
	closeBtn->translate(vec3(mw - (btnSize * 0.5f) - inset, mh + (btnSize * 0.5f) + inset, 0.0f));
	closeBtn->scale(vec3(btnSize, btnSize, 1.0f), closeBtn->mvp.model);
	closeBtn->updateMVP(std::nullopt, viewLocal, projLocal);

	closeBtnIcon->translate(closeBtn->getPosition());
	closeBtnIcon->scale(vec3(iconSize, iconSize, 1.0f), closeBtnIcon->mvp.model);
	closeBtnIcon->updateMVP(std::nullopt, viewLocal, projLocal);

	confirmBtn->params.color = Colors::DarkGreen;
	confirmBtn->params.outlineColor = Colors::DarkGreen;
	confirmBtn->translate(vec3(mw - (btnSize * 0.5f) * 2 - inset * 2 - 10, mh + (btnSize * 0.5f) + inset, 0.0f));
	confirmBtn->scale(vec3(btnSize, btnSize, 1.0f), confirmBtn->mvp.model);
	confirmBtn->updateMVP(std::nullopt, viewLocal, projLocal);

	confirmBtnIcon->translate(confirmBtn->getPosition());
	confirmBtnIcon->scale(vec3(iconSize, iconSize, 1.0f), confirmBtnIcon->mvp.model);
	confirmBtnIcon->updateMVP(std::nullopt, viewLocal, projLocal);
}

void AddRecipeStep::updateComputeUniformBuffers() {}

void AddRecipeStep::computePass() {}

void AddRecipeStep::updateUniformBuffers() { textInput->updateUniformBuffers(mvp); }

void AddRecipeStep::renderPass() {}

void AddRecipeStep::renderPass1() {
	modal->render();
	closeBtn->render();
	closeBtnIcon->render();
	confirmBtn->render();
	confirmBtnIcon->render();
	textInput->render();
}
