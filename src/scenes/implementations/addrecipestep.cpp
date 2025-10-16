#include "addrecipestep.hpp"

AddRecipeStep::AddRecipeStep(Scenes &scenes, bool show) : Scene(scenes, show) {
	mvp = {mat4(1.0f), mat4(1.0f), ortho(0.0f, float(Engine::swapChainExtent.width), 0.0f, -float(Engine::swapChainExtent.height), -1.0f, 1.0f)};

	Text::FontParams fp{};
	textInput = make_unique<TextInput>(this, mvp, screenParams, fp, Engine::renderPass1);

	auto mInstances = std::make_shared<std::unordered_map<int, InstancedRectangleData>>();
	modal = make_unique<InstancedRectangle>(this, textInput->mvp, textInput->textField->bgSp, mInstances, 2);
	modal->enableBlur(Assets::shaderRootPath + "/instanced/blur/irectblur/");
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
	const float w = textInput->textField->bgSp.viewport.width;
	const float h = textInput->textField->bgSp.viewport.height;

	// Build a projection in the modal’s own viewport space
	auto projLocal = ortho(0.0f, w, 0.0f, -h, -1.0f, 1.0f);

	InstancedRectangleData m{};
	m.color = Colors::DarkBlue(0.5f);
	m.borderRadius = 25.0f;
	m.model = translate(mat4(1.0f), vec3(w * 0.5f, h * 0.5f, 0.0f)) * scale(mat4(1.0f), vec3(w, h, 1.0f));

	modal->updateInstance(0, m);
	// Pin to viewport: identity view; local projection
	modal->updateMVP(std::nullopt, mat4(1.0f), projLocal);
}

void AddRecipeStep::swapChainUpdate() {
	auto w = screenParams.viewport.width;
	auto h = screenParams.viewport.height;
	mvp = {mat4(1.0f), mat4(1.0f), ortho(0.0f, w, 0.0f, -h, -1.0f, 1.0f)};

	textInput->params.center = vec2(w * 0.5f, h * 0.5f);
	textInput->params.dim = vec2(200, 100);
	textInput->textField->params.margins = vec4(50.0f, 50.0f, 50.0f, 50.0f);
	textInput->textField->params.scrollBarWidth = 8.0f;
	textInput->mvp = mvp;
	textInput->swapChainUpdate();
	createModal();
}

void AddRecipeStep::updateComputeUniformBuffers() {}

void AddRecipeStep::computePass() {}

void AddRecipeStep::updateUniformBuffers() { textInput->updateUniformBuffers(mvp); }

void AddRecipeStep::renderPass() {}

void AddRecipeStep::renderPass1() {
	modal->render();
	textInput->render();
}
