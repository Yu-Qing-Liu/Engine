#include "background.hpp"
#include "particles.hpp"
#include "textures.hpp"

Background::Background(Scenes &scenes) : Scene(scenes) {
	mvp = {mat4(1.0f), mat4(1.0f), ortho(0.0f, float(Engine::swapChainExtent.width), 0.0f, -float(Engine::swapChainExtent.height), -1.0f, 1.0f)};
	particles = make_unique<Particles>(this, mvp, screenParams, 16, screenParams.viewport.width, screenParams.viewport.height);
	backgroundImage = Textures::icon(this, mvp, screenParams, Assets::textureRootPath + "/background/basil.jpg");
}

void Background::updateScreenParams() {
	screenParams.viewport.x = 0.0f;
	screenParams.viewport.y = 0.0f;
	screenParams.viewport.width = (float)Engine::swapChainExtent.width;
	screenParams.viewport.height = (float)Engine::swapChainExtent.height;
	screenParams.viewport.minDepth = 0.0f;
	screenParams.viewport.maxDepth = 1.0f;
	screenParams.scissor.offset = {(int32_t)screenParams.viewport.x, (int32_t)screenParams.viewport.y};
	screenParams.scissor.extent = {(uint32_t)screenParams.viewport.width, (uint32_t)screenParams.viewport.height};
}

void Background::swapChainUpdate() {
	float w = screenParams.viewport.width;
	float h = screenParams.viewport.height;

	mvp.proj = ortho(0.0f, w, 0.0f, -h, -1.0f, 1.0f);
	particles->updateUniformBuffer(std::nullopt, std::nullopt, mvp.proj);

	float texW = (float)backgroundImage->texW; // expose getters if private
	float texH = (float)backgroundImage->texH;

	float screenAspect = w / h;
	float texAspect = texW / texH;

	float quadW, quadH; // keep image aspect, fill screen (cover)
	if (screenAspect > texAspect) {
		quadW = w;
		quadH = w / texAspect; // taller than screen → cropped vertically
	} else {
		quadH = h;
		quadW = h * texAspect; // wider than screen → cropped horizontally
	}

	mat4 model = translate(mat4(1.0f), vec3(w * 0.5f, h * 0.5f, 0.0f)) * scale(mat4(1.0f), vec3(quadW, quadH, 1.0f));

    backgroundImage->computeAspectUV();
	backgroundImage->updateUniformBuffer(model, std::nullopt, mvp.proj);
}

void Background::updateComputeUniformBuffers() { particles->updateComputeUniformBuffer(); }

void Background::computePass() { particles->compute(); }

void Background::updateUniformBuffers() {}

void Background::renderPass() {
	backgroundImage->render();
	particles->render();
}
