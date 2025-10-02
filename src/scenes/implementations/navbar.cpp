#include "navbar.hpp"
#include "colors.hpp"
#include "engine.hpp"
#include "textures.hpp"

NavBar::NavBar(Scenes &scenes) : Scene(scenes) {
	mvp = {mat4(1.0f), mat4(1.0f), ortho(0.0f, float(Engine::swapChainExtent.width), 0.0f, -float(Engine::swapChainExtent.height), -1.0f, 1.0f)};

	inventoryBtn = make_unique<Rectangle>(this, mvp, screenParams);
    inventoryBtn->enableBlur(true);
    inventoryBtn->blur->setTint(Colors::Orange(0.1));
    inventoryBtn->blur->setCornerRadiusOverride(24);

    inventoryBtnIcon = Textures::icon(this, mvp, screenParams, Assets::textureRootPath + "/icons/inventory.png", Engine::renderPass1);

	recipesBtn = make_unique<Rectangle>(this, mvp, screenParams);
    recipesBtn->enableBlur(true);
    recipesBtn->blur->setTint(Colors::Blue(0.1));
    recipesBtn->blur->setCornerRadiusOverride(24);

    recipesBtnIcon = Textures::icon(this, mvp, screenParams, Assets::textureRootPath + "/icons/recipebook.png", Engine::renderPass1);

	cartBtn = make_unique<Rectangle>(this, mvp, screenParams);
    cartBtn->enableBlur(true);
    cartBtn->blur->setTint(Colors::Green(0.1));
    cartBtn->blur->setCornerRadiusOverride(24);

    cartBtnIcon = Textures::icon(this, mvp, screenParams, Assets::textureRootPath + "/icons/grocerybasket.png", Engine::renderPass1);

	calendarBtn = make_unique<Rectangle>(this, mvp, screenParams);
    calendarBtn->enableBlur(true);
    calendarBtn->blur->setTint(Colors::White(0.1));
    calendarBtn->blur->setCornerRadiusOverride(24);

    calendarBtnIcon = Textures::icon(this, mvp, screenParams, Assets::textureRootPath + "/icons/calendar.png", Engine::renderPass1);
}

void NavBar::updateScreenParams() {
	screenParams.viewport.x = 0.0f;
	screenParams.viewport.y = 0.0f;
	screenParams.viewport.width = (float)Engine::swapChainExtent.width;
	screenParams.viewport.height = (float)Engine::swapChainExtent.height;
	screenParams.viewport.minDepth = 0.0f;
	screenParams.viewport.maxDepth = 1.0f;
	screenParams.scissor.offset = {(int32_t)screenParams.viewport.x, (int32_t)screenParams.viewport.y};
	screenParams.scissor.extent = {(uint32_t)screenParams.viewport.width, (uint32_t)screenParams.viewport.height};
}

void NavBar::swapChainUpdate() {
	float w = screenParams.viewport.width;
	float h = screenParams.viewport.height;
    mvp.proj = ortho(0.0f, w, 0.0f, -h, -1.0f, 1.0f);

    float wOffset = 50;
    float offset = 85;

    float hOffset = 50;

    float btnSize = 75;
    float iconSize = 50;

    inventoryBtn->updateMVP(
        translate(mat4(1.0f), vec3(w - wOffset, hOffset, 0.0f)) * scale(mat4(1.0f), vec3(btnSize, btnSize, 1.0)),
        std::nullopt,
        mvp.proj
    );
    inventoryBtnIcon->updateMVP(
        translate(mat4(1.0f), vec3(w - wOffset, hOffset, 0.0f)) * scale(mat4(1.0f), vec3(iconSize, iconSize, 1.0)),
        std::nullopt,
        mvp.proj
    );
    wOffset += offset;

    recipesBtn->updateMVP(
        translate(mat4(1.0f), vec3(w - wOffset, hOffset, 0.0f)) * scale(mat4(1.0f), vec3(btnSize, btnSize, 1.0)),
        std::nullopt,
        mvp.proj
    );
    recipesBtnIcon->updateMVP(
        translate(mat4(1.0f), vec3(w - wOffset, hOffset, 0.0f)) * scale(mat4(1.0f), vec3(iconSize, iconSize, 1.0)),
        std::nullopt,
        mvp.proj
    );
    wOffset += offset;

    cartBtn->updateMVP(
        translate(mat4(1.0f), vec3(w - wOffset, hOffset, 0.0f)) * scale(mat4(1.0f), vec3(btnSize, btnSize, 1.0)),
        std::nullopt,
        mvp.proj
    );
    cartBtnIcon->updateMVP(
        translate(mat4(1.0f), vec3(w - wOffset, hOffset, 0.0f)) * scale(mat4(1.0f), vec3(iconSize, iconSize, 1.0)),
        std::nullopt,
        mvp.proj
    );
    wOffset += offset;

    calendarBtn->updateMVP(
        translate(mat4(1.0f), vec3(w - wOffset, hOffset, 0.0f)) * scale(mat4(1.0f), vec3(btnSize, btnSize, 1.0)),
        std::nullopt,
        mvp.proj
    );
    calendarBtnIcon->updateMVP(
        translate(mat4(1.0f), vec3(w - wOffset, hOffset, 0.0f)) * scale(mat4(1.0f), vec3(iconSize, iconSize, 1.0)),
        std::nullopt,
        mvp.proj
    );
}

void NavBar::updateComputeUniformBuffers() {}

void NavBar::computePass() {}

void NavBar::updateUniformBuffers() {}

void NavBar::renderPass() {}

void NavBar::renderPass1() {
    inventoryBtn->render();
    inventoryBtnIcon->render();

    recipesBtn->render();
    recipesBtnIcon->render();

    cartBtn->render();
    cartBtnIcon->render();

    calendarBtn->render();
    calendarBtnIcon->render();
}
