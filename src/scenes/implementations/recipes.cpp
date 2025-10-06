#include "recipes.hpp"
#include "engine.hpp"
#include "scenes.hpp"
#include "textures.hpp"

Recipes::Recipes(Scenes &scenes, bool show) : Scene(scenes, show) {
	mvp = {mat4(1.0f), mat4(1.0f), ortho(0.0f, float(Engine::swapChainExtent.width), 0.0f, -float(Engine::swapChainExtent.height), -1.0f, 1.0f)};
	grid = make_unique<Grid>(this, mvp, screenParams);
	grid->grid->enableBlur(false);
	grid->scrollBar->enableBlur(false);
	grid->grid->blur->shaderPath = Assets::shaderRootPath + "/instanced/blur/irectblur/";
	grid->scrollBar->blur->shaderPath = Assets::shaderRootPath + "/instanced/blur/irectblur/";
	grid->grid->blur->initialize();
	grid->scrollBar->blur->initialize();
    grid->grid->enableRayTracing(true);
	grid->grid->setOnMouseClick([&](int button, int action, int mods) {
		if (!this->show) {
			return;
		}
		if (action == Events::ACTION_PRESS && button == Events::MOUSE_BUTTON_LEFT) {
			int id = grid->grid->rayTracing->hitMapped->primId;
			if (id == grid->numItems) {
                scenes.showScene("Recipe");
                grid->enableControls = false;
			}
		} 
	});

	addRecipeIcon = Textures::icon(this, grid->mvp, grid->sp, Assets::textureRootPath + "/icons/addfile.png", Engine::renderPass1);
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
}

void Recipes::swapChainUpdate() {
	const auto w = (float)screenParams.viewport.width;
	const auto h = (float)screenParams.viewport.height;
	const float padT = 100;
	const float usableH = h - padT * 2;
	grid->styleParams.gridCenter = vec2(w * 0.5, padT + usableH * 0.5f);
	grid->styleParams.gridDim = vec2(w * 0.95, usableH);
	grid->styleParams.cellSize = vec2(360.f);
    grid->numItems = 10;
    grid->setGridItem = [&](int idx, float x, float y, vec2 cellSize, MVP mvp) {
        if (idx == grid->numItems) {
			addRecipeIcon->updateMVP(translate(mat4(1.0f), vec3(x, y, 0.0f)) * scale(mat4(1.0f), vec3(cellSize.x * 0.6, cellSize.y * 0.6, 1.0f)), mvp.view, mvp.proj);
        }
    };
	grid->swapChainUpdate();
}

void Recipes::updateComputeUniformBuffers() {}

void Recipes::computePass() {}

void Recipes::updateUniformBuffers() { 
    grid->updateUniformBuffers(); 
    addRecipeIcon->updateMVP(std::nullopt, grid->mvp.view);
}

void Recipes::renderPass() {}

void Recipes::renderPass1() { 
    grid->renderPass1(); 
    addRecipeIcon->render();
}
