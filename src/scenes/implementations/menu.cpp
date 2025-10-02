#include "menu.hpp"

#include "colors.hpp"
#include "engine.hpp"
#include "geometry.hpp"
#include "shapes.hpp"

Menu::Menu(Scenes &scenes) : Scene(scenes) {
	auto now = system_clock::now();
	auto today = floor<days>(now);
	year_month_day ymd{today};
	selectedDay = weekday{today};

	mvp = {mat4(1.0f), mat4(1.0f), ortho(0.0f, float(Engine::swapChainExtent.width), 0.0f, -float(Engine::swapChainExtent.height), -1.0f, 1.0f)};
	dayBtns = Shapes::slantedRectangles(this, mvp, screenParams, 7);
	dayBtns->enableBlur(true);
	dayBtns->enableRayTracing(true);
	dayBtns->onMouseEnter = [&]() {
		if (!dayBtns->rayTracing->hitMapped) {
			return;
		}
		int id = dayBtns->rayTracing->hitMapped->primId;
		if (id >= 0 && id < 7) {
			hoveredId = id;
		}
		if (id == selectedDay.iso_encoding() - 1) {
			hoveredId = -1;
			return;
		}
		if (!dayBtns->hasInstance(id)) {
			return;
		}
		InstancedPolygonData prev = dayBtns->getInstance(id);
		prev.color = Colors::Purple(0.3);
		dayBtns->updateInstance(id, prev);
	};
	dayBtns->onMouseExit = [&]() {
		if (hoveredId == -1) {
			return;
		}
		if (hoveredId == selectedDay.iso_encoding() - 1) {
			hoveredId = -1;
			return;
		}
		if (!dayBtns->hasInstance(hoveredId)) {
			return;
		}
		InstancedPolygonData prev = dayBtns->getInstance(hoveredId);
		prev.color = Colors::Gray(0.3);
		dayBtns->updateInstance(hoveredId, prev);
		hoveredId = -1;
	};
	dayBtns->setOnMouseClick([&](int button, int action, int mods) {
		if (action == Events::ACTION_PRESS && button == Events::MOUSE_BUTTON_LEFT) {
			if (hoveredId == -1) {
				return;
			}
			if (hoveredId == selectedDay.iso_encoding() - 1) {
				hoveredId = -1;
				return;
			}
			auto sDay = selectedDay.iso_encoding() - 1;
			if (!dayBtns->hasInstance(hoveredId)) {
				return;
			}
			InstancedPolygonData prev = dayBtns->getInstance(hoveredId);
			if (!dayBtns->hasInstance(sDay)) {
				return;
			}
			InstancedPolygonData curr = dayBtns->getInstance(sDay);
			prev.color = Colors::Orange(0.3);
			curr.color = Colors::Gray(0.3);
			dayBtns->updateInstance((int)sDay, curr);
			dayBtns->updateInstance(hoveredId, prev);
			selectedDay = Monday + days{hoveredId};
			hoveredId = -1;
		}
	});

	auto mealBtnsInstances = std::make_shared<std::unordered_map<int, InstancedRectangleData>>(3);
	mealBtns = make_unique<InstancedRectangle>(this, mvp, screenParams, mealBtnsInstances, 3);
	mealBtns->enableBlur(false);
    mealBtns->blur->shaderPath = Assets::shaderRootPath + "/instanced/blur/irectblur/";
    mealBtns->blur->initialize();
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
	for (uint8_t i = 0; i < 7; ++i) {
		weekday wday = Monday + days{i};

		InstancedPolygonData day{};
		day.color = Colors::Gray(0.3);
		day.model = translate(mat4(1.0), vec3(85 + wOffset, hOffset, 0)) * scale(mat4(1.0), btnSize);
		if (wday == selectedDay) {
			day.color = Colors::Orange(0.3);
		}
		dayBtns->updateInstance(i, day);
		wOffset += offset;

		Text::FontParams ft{};
		ft.fontPath = Fonts::ArialBold;
		auto label = make_unique<Text>(this, mvp, screenParams, ft, Engine::renderPass1);
		label->textParams.text = std::format("{:%a}", wday);
		label->updateMVP(translate(mat4(1.0), vec3(day.model[3].x, day.model[3].y, day.model[3].z)));
		label->textParams.origin = Geometry::alignTextCentered(*label, label->textParams.text);
		label->textParams.color = Colors::White;
		dayLabels.emplace_back(std::move(label));
	}
	dayBtns->updateMVP(std::nullopt, std::nullopt, mvp.proj);
	for (const auto &l : dayLabels) {
		l->updateMVP(std::nullopt, std::nullopt, mvp.proj);
	}

	const int rows = 3;
	float topOffset = 165.0f;	// distance from top
	float bottomOffset = 15.0f; // distance from bottom

	const float usableHeight = h - topOffset - bottomOffset;
	const float rowH = usableHeight / float(rows);

	btnSize = vec3(w - 35, rowH - 25, 1.0f); // 90% width, ~row height
	const float cx = 0.5f * w;				 // center horizontally

	for (int i = 0; i < rows; ++i) {
		// center of each row, starting from topOffset
		float cy = topOffset + (i + 0.5f) * rowH;

		InstancedRectangleData btn{};
		btn.color = Colors::LightBlue(0.3f);
		btn.model = glm::translate(mat4(1.0f), vec3(cx, cy, 0.0f)) * glm::scale(mat4(1.0f), btnSize);
        btn.borderRadius = 50.0f;

		mealBtns->updateInstance(i, btn);
	}
	mealBtns->updateMVP(std::nullopt, std::nullopt, mvp.proj);
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
	mealBtns->render();
}
