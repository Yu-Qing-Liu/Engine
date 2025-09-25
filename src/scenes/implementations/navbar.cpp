#include "navbar.hpp"
#include "colors.hpp"
#include "engine.hpp"

NavBar::NavBar(Scenes &scenes) : Scene(scenes) {
	mvp = {mat4(1.0f), mat4(1.0f), ortho(0.0f, float(Engine::swapChainExtent.width), 0.0f, -float(Engine::swapChainExtent.height), -1.0f, 1.0f)};

	btn = make_unique<Rectangle>(this, mvp, screenParams);
    btn->enableBlur(true);
    btn->blur->setTint(Colors::Purple(0.5));
    btn->blur->setCornerRadiusOverride(24);

    Text::TextParams tp{};
    btnText = make_unique<Text>(this, mvp, screenParams, tp, Engine::renderPass1);
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

    btn->updateMVP(
        translate(mat4(1.0f), vec3(w * 0.5f, 100, 0.0f)) * scale(mat4(1.0f), vec3(w * 0.5, h * 0.05, 1.0)),
        std::nullopt,
        mvp.proj
    );
    btnText->updateMVP(
        translate(mat4(1.0f), vec3(w * 0.5f, 100, 0.0f)),
        std::nullopt,
        mvp.proj
    );
}

void NavBar::updateComputeUniformBuffers() {}

void NavBar::computePass() {}

void NavBar::updateUniformBuffers() {}

void NavBar::renderPass() {}

void NavBar::renderPass1() {
    btn->render();
    btnText->renderText("TEST");
}
