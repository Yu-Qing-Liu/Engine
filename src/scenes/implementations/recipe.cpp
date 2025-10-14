#include "recipe.hpp"
#include "colors.hpp"
#include "engine.hpp"
#include "fonts.hpp"
#include "pipeline.hpp"
#include "scenes.hpp"
#include "shapes.hpp"
#include "textures.hpp"

Recipe::Recipe(Scenes &scenes, bool show) : Scene(scenes, show) {
	mvp = {mat4(1.0f), mat4(1.0f), ortho(0.0f, float(Engine::swapChainExtent.width), 0.0f, -float(Engine::swapChainExtent.height), -1.0f, 1.0f)};

	stepsGrid = make_unique<Grid>(this, mvp, screenParams);
	stepsGrid->grid->enableBlur(Assets::shaderRootPath + "/instanced/blur/irectblur/");
	stepsGrid->scrollBar->enableBlur(Assets::shaderRootPath + "/instanced/blur/irectblur/");
	stepsGrid->grid->enableRayTracing(true);
	stepsGrid->grid->setOnMouseClick([&](int button, int action, int mods) {
		if (!this->show) {
			return;
		}
		if (action == Events::ACTION_PRESS && button == Events::MOUSE_BUTTON_LEFT) {
			int id = stepsGrid->grid->rayTracing->hitMapped->primId;
			if (id == stepsGrid->numItems) {
				auto style = stepsGrid->grid->getInstance(id);
				style.color = Colors::Lime;
				stepsGrid->grid->updateInstance(id, style);
				scenes.showScene("AddRecipeStep");
			}
		} else if (action == Events::ACTION_RELEASE && button == Events::MOUSE_BUTTON_LEFT) {
			int id = stepsGrid->grid->rayTracing->hitMapped->primId;
			if (id == stepsGrid->numItems) {
				auto style = stepsGrid->grid->getInstance(id);
				style.color = Colors::Green;
				stepsGrid->grid->updateInstance(id, style);
			}
		}
	});

	addStepIcon = Textures::icon(this, stepsGrid->mvp, stepsGrid->sp, Assets::textureRootPath + "/icons/addfile.png", Engine::renderPass1);

	auto mInstances = std::make_shared<std::unordered_map<int, InstancedRectangleData>>();
	stepsModal = make_unique<InstancedRectangle>(this, stepsGrid->mvp, stepsGrid->bgSp, mInstances, 2);
	stepsModal->enableBlur(Assets::shaderRootPath + "/instanced/blur/irectblur/");

	Text::FontParams fp{};
	fp.fontPath = Fonts::ArialBold;
	recipeNameInput = make_unique<TextInput>(this, stepsGrid->mvp, stepsGrid->bgSp, fp, Engine::renderPass1);
	recipeNameInput->textField->showScrollBar = false;

	closeBtnIcon = Textures::icon(this, stepsGrid->mvp, stepsGrid->bgSp, Assets::textureRootPath + "/icons/close.png", Engine::renderPass1);
	closeBtn = Shapes::polygon2D(this, stepsGrid->mvp, stepsGrid->bgSp, 64, Engine::renderPass1);
	closeBtn->enableRayTracing(true);

	closeBtn->setOnMouseClick([&](int button, int action, int /*mods*/) {
		if (!this->show)
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
			const auto &recipesScene = scenes.getScene("Recipes");
			if (recipesScene) {
				recipesScene->enable();
            }
		}
	});
}

void Recipe::fetchData() {
	if (!recipeName.empty()) {
		recipe = RecipesQueries::fetchRecipe(recipeName);
	}
	Pipeline::recreateSwapChain();
	swapChainUpdate();
}

void Recipe::updateScreenParams() {
	screenParams.viewport.x = 0.0f;
	screenParams.viewport.y = 0.0f;
	screenParams.viewport.width = (float)Engine::swapChainExtent.width;
	screenParams.viewport.height = (float)Engine::swapChainExtent.height;
	screenParams.viewport.minDepth = 0.0f;
	screenParams.viewport.maxDepth = 1.0f;
	screenParams.scissor.offset = {(int32_t)screenParams.viewport.x, (int32_t)screenParams.viewport.y};
	screenParams.scissor.extent = {(uint32_t)screenParams.viewport.width, (uint32_t)screenParams.viewport.height};
}

void Recipe::createModal() {
	const float w = stepsGrid->bgSp.viewport.width;
	const float h = stepsGrid->bgSp.viewport.height;

	// Build a projection in the modal’s own viewport space
	auto projLocal = ortho(0.0f, w, 0.0f, -h, -1.0f, 1.0f);

	InstancedRectangleData m{};
	m.color = Colors::Gray(0.5f);
	m.borderRadius = 25.0f;
	m.model = translate(mat4(1.0f), vec3(w * 0.5f, h * 0.5f, 0.0f)) * scale(mat4(1.0f), vec3(w, h, 1.0f));

	stepsModal->updateInstance(0, m);
	// Pin to viewport: identity view; local projection
	stepsModal->updateMVP(std::nullopt, mat4(1.0f), projLocal);
}

void Recipe::swapChainUpdate() {
	const auto w = (float)screenParams.viewport.width;
	const auto h = (float)screenParams.viewport.height;
	mvp = {mat4(1.0f), mat4(1.0f), ortho(0.0f, w, 0.0f, -h, -1.0f, 1.0f)};

	const float padT = 200;
	const float usableH = h * 0.5 - padT;
	stepsGrid->params.gridCenter = vec2(w * 0.5, padT + usableH * 0.5f);
	stepsGrid->params.gridDim = vec2(w * 0.8, usableH);
	stepsGrid->params.cellSize = vec2((w - stepsGrid->params.scrollBarWidth * 2) * 0.8, 250);
	stepsGrid->params.cellColor = Colors::DarkGreen;
	stepsGrid->params.margins = vec4(50.0f, 100.0f, 50.0f, 50.0f);
	stepsGrid->params.numCols = 1;
	stepsGrid->mvp = mvp;
	stepsGrid->numItems = recipe.steps.size();

	steps.clear();
	Text::FontParams fp{};
	for (size_t i = 0; i < stepsGrid->numItems; i++) {
		steps.emplace_back(make_unique<TextLabel>(this, stepsGrid->mvp, stepsGrid->sp, fp, Engine::renderPass1));
	}

	stepsGrid->setGridItem = [&](int idx, float x, float y, vec2 cellSize, MVP mvp) {
		if (idx == stepsGrid->numItems) {
			addStepIcon->updateMVP(translate(mat4(1.0f), vec3(x, y, 0.0f)) * scale(mat4(1.0f), vec3(cellSize.y * 0.6, cellSize.y * 0.6, 1.0f)), mvp.view, mvp.proj);
		} else {
			steps[idx]->mvp = mvp;
			steps[idx]->text = recipe.steps[idx].instruction;
			steps[idx]->params.center = vec2(x, y);
			steps[idx]->params.dim = vec2(cellSize.x - 20, cellSize.y - 20);
			steps[idx]->swapChainUpdate();
		}
	};

	stepsGrid->swapChainUpdate();

	recipeNameInput->params.center = vec2(250, 35);
	recipeNameInput->params.dim = vec2(400, 40);
	recipeNameInput->params.placeholderText = recipe.name.empty() ? "New Recipe" : recipe.name;
	recipeNameInput->textField->params.margins = vec4(5.0f, 4.0f, 0.0f, 0.0f);
	recipeNameInput->mvp = stepsGrid->mvp;
	recipeNameInput->screenParams = stepsGrid->bgSp;
	recipeNameInput->swapChainUpdate();

	closeBtnIcon->updateMVP(std::nullopt, std::nullopt, stepsGrid->mvp.proj);
	closeBtnIcon->translate(vec3(400, 35, 0.0));

	const float vw = stepsGrid->bgSp.viewport.width;
	const float vh = stepsGrid->bgSp.viewport.height;
	const glm::mat4 projLocal = ortho(0.0f, vw, 0.0f, -vh, -1.0f, 1.0f);

	const glm::mat4 viewLocal = glm::mat4(1.0f);

	closeBtn->updateMVP(std::nullopt, viewLocal, projLocal);
	closeBtnIcon->updateMVP(std::nullopt, viewLocal, projLocal);

	const float inset = 25.0f;
	const float btnSize = 35.0f;
	const float iconSize = 15.0f;

	closeBtn->params.color = Colors::Red;
	closeBtn->params.outlineColor = Colors::Red;
	closeBtn->translate(vec3(vw - (btnSize * 0.5f) - inset, (btnSize * 0.5f) + inset, 0.0f));
	closeBtn->scale(vec3(btnSize, btnSize, 1.0f), closeBtn->mvp.model);

	closeBtnIcon->translate(vec3(closeBtn->mvp.model[3].x, closeBtn->mvp.model[3].y, closeBtn->mvp.model[3].z));
	closeBtnIcon->scale(vec3(iconSize, iconSize, 1.0f), closeBtnIcon->mvp.model);

	createModal();
}

void Recipe::updateComputeUniformBuffers() {}

void Recipe::computePass() {}

void Recipe::updateUniformBuffers() {
	stepsGrid->updateUniformBuffers();
	addStepIcon->updateMVP(std::nullopt, stepsGrid->mvp.view);
	for (size_t i = 0; i < stepsGrid->numItems; i++) {
		steps[i]->updateUniformBuffers(stepsGrid->mvp);
	}
	recipeNameInput->updateUniformBuffers(stepsGrid->mvp);
}

void Recipe::renderPass() {}

void Recipe::renderPass1() {
	stepsModal->render();
	stepsGrid->render();
	addStepIcon->render();
	for (size_t i = 0; i < stepsGrid->numItems; i++) {
		steps[i]->render();
	}
	recipeNameInput->render();
	closeBtn->render();
	closeBtnIcon->render();
}
