#include "menu.hpp"

#include <chrono>

#include "colors.hpp"
#include "engine.hpp"
#include "geometry.hpp"
#include "shapes.hpp"

using namespace std::chrono;

Menu::Menu(Scenes &scenes) : Scene(scenes) {
	mvp = {mat4(1.0f), mat4(1.0f), ortho(0.0f, float(Engine::swapChainExtent.width), 0.0f, -float(Engine::swapChainExtent.height), -1.0f, 1.0f)};
	dayBtns = Shapes::slantedRectangles(this, mvp, screenParams, 7);
	dayBtns->enableBlur(true);
}

void Menu::updateScreenParams() {
	screenParams.viewport.x = 0.0f;
	screenParams.viewport.y = 0.0f;
	screenParams.viewport.width = (float)Engine::swapChainExtent.width;
	screenParams.viewport.height = (float)Engine::swapChainExtent.height;
	screenParams.viewport.minDepth = 0.0f;
	screenParams.viewport.maxDepth = 1.0f;
	screenParams.scissor.offset = {(int32_t)screenParams.viewport.x, (int32_t)screenParams.viewport.y};
	screenParams.scissor.extent = {(uint32_t)screenParams.viewport.width, (uint32_t)screenParams.viewport.height};
}

void Menu::swapChainUpdate() {
	float w = screenParams.viewport.width;
	float h = screenParams.viewport.height;
	mvp.proj = ortho(0.0f, w, 0.0f, -h, -1.0f, 1.0f);

	float wOffset = 0;
	float hOffset = 130;
	float offset = 135;
	vec3 btnSize = vec3(120, 50, 1);
	for (uint8_t i = 1; i < 8; ++i) {
		InstancedPolygonData day{};
		day.color = Colors::Gray(0.3);
		day.model = translate(mat4(1.0), vec3(85 + wOffset, hOffset, 0)) * scale(mat4(1.0), btnSize);
		dayBtns->updateInstance(i, day);
		wOffset += offset;

		weekday wd{i};
		Text::FontParams ft{};
        ft.fontPath = Fonts::ArialBold;
		auto label = make_unique<Text>(this, mvp, screenParams, ft, Engine::renderPass1);
        label->textParams.text = std::format("{:%a}", wd);
        label->updateMVP(translate(mat4(1.0), vec3(day.model[3].x, day.model[3].y, day.model[3].z)));
        label->textParams.origin = Geometry::alignTextCentered(*label, label->textParams.text);
        label->textParams.color = Colors::White;
		dayLabels.emplace_back(std::move(label));
	}
	dayBtns->updateMVP(std::nullopt, std::nullopt, mvp.proj);
}

void Menu::updateComputeUniformBuffers() {}

void Menu::computePass() {}

void Menu::updateUniformBuffers() {}

void Menu::renderPass() {}

void Menu::renderPass1() { 
    dayBtns->render(); 
    for (const auto &l : dayLabels) {
        l->render();
    }
}
