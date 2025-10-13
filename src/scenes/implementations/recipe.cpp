#include "recipe.hpp"
#include "colors.hpp"
#include "engine.hpp"
#include "pipeline.hpp"
#include "textures.hpp"
#include "scenes.hpp"

Recipe::Recipe(Scenes &scenes, bool show) : Scene(scenes, show) {
	mvp = {mat4(1.0f), mat4(1.0f), ortho(0.0f, float(Engine::swapChainExtent.width), 0.0f, -float(Engine::swapChainExtent.height), -1.0f, 1.0f)};

	grid = make_unique<Grid>(this, mvp, screenParams);
	grid->grid->enableBlur(Assets::shaderRootPath + "/instanced/blur/irectblur/");
	grid->scrollBar->enableBlur(Assets::shaderRootPath + "/instanced/blur/irectblur/");
	grid->grid->enableRayTracing(true);
	grid->grid->setOnMouseClick([&](int button, int action, int mods) {
		if (!this->show) {
			return;
		}
		if (action == Events::ACTION_PRESS && button == Events::MOUSE_BUTTON_LEFT) {
			int id = grid->grid->rayTracing->hitMapped->primId;
			if (id == grid->numItems) {
				auto style = grid->grid->getInstance(id);
				style.color = Colors::Lime;
				grid->grid->updateInstance(id, style);
                scenes.showScene("AddRecipeStep");
			}
		} else if (action == Events::ACTION_RELEASE && button == Events::MOUSE_BUTTON_LEFT) {
			int id = grid->grid->rayTracing->hitMapped->primId;
			if (id == grid->numItems) {
				auto style = grid->grid->getInstance(id);
				style.color = Colors::Green;
				grid->grid->updateInstance(id, style);
			}
		}
	});

	addStepIcon = Textures::icon(this, grid->mvp, grid->sp, Assets::textureRootPath + "/icons/addfile.png", Engine::renderPass1);

	auto mInstances = std::make_shared<std::unordered_map<int, InstancedRectangleData>>();
	modal = make_unique<InstancedRectangle>(this, grid->mvp, grid->bgSp, mInstances, 2);
	modal->enableBlur(Assets::shaderRootPath + "/instanced/blur/irectblur/");
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
	const float w = grid->bgSp.viewport.width;
	const float h = grid->bgSp.viewport.height;

	// Build a projection in the modal’s own viewport space
	auto projLocal = ortho(0.0f, w, 0.0f, -h, -1.0f, 1.0f);

	InstancedRectangleData m{};
	m.color = Colors::Gray(0.5f);
	m.borderRadius = 25.0f;
	m.model = translate(mat4(1.0f), vec3(w * 0.5f, h * 0.5f, 0.0f)) * scale(mat4(1.0f), vec3(w, h, 1.0f));

	modal->updateInstance(0, m);
	// Pin to viewport: identity view; local projection
	modal->updateMVP(std::nullopt, mat4(1.0f), projLocal);
}

void Recipe::swapChainUpdate() {
	const auto w = (float)screenParams.viewport.width;
	const auto h = (float)screenParams.viewport.height;
	const float padT = 175;
	const float usableH = h * 0.5 - padT;
	grid->params.gridCenter = vec2(w * 0.5, padT + usableH * 0.5f);
	grid->params.gridDim = vec2(w * 0.8, usableH);
	grid->params.cellSize = vec2((w - grid->params.scrollBarWidth * 2) * 0.8, 250);
	grid->params.cellColor = Colors::DarkGreen;
	grid->params.margins = vec4(50.0f);
	grid->params.numCols = 1;
	grid->numItems = recipe.steps.size();

	Text::FontParams fp{};
	for (size_t i = 0; i < grid->numItems; i++) {
		steps.emplace_back(make_unique<TextLabel>(this, grid->mvp, grid->sp, fp, Engine::renderPass1));
	}

	grid->setGridItem = [&](int idx, float x, float y, vec2 cellSize, MVP mvp) {
		if (idx == grid->numItems) {
			addStepIcon->updateMVP(translate(mat4(1.0f), vec3(x, y, 0.0f)) * scale(mat4(1.0f), vec3(cellSize.y * 0.6, cellSize.y * 0.6, 1.0f)), mvp.view, mvp.proj);
		} else {
            steps[idx]->mvp = mvp;
			steps[idx]->text = recipe.steps[idx].instruction;
			steps[idx]->params.center = vec2(x, y);
			steps[idx]->params.dim = vec2(cellSize.x - 20, cellSize.y - 20);
            steps[idx]->swapChainUpdate();
		}
	};

	grid->swapChainUpdate();

	createModal();
}

void Recipe::updateComputeUniformBuffers() {}

void Recipe::computePass() {}

void Recipe::updateUniformBuffers() {
	grid->updateUniformBuffers();
	addStepIcon->updateMVP(std::nullopt, grid->mvp.view);
	for (size_t i = 0; i < grid->numItems; i++) {
        steps[i]->updateUniformBuffers(grid->mvp);
	}
}

void Recipe::renderPass() {}

void Recipe::renderPass1() {
	modal->render();
	grid->render();
	addStepIcon->render();
	for (size_t i = 0; i < grid->numItems; i++) {
		steps[i]->render();
	}
}
