#include "rectangle.hpp"
#include "engine.hpp"
#include "scenes.hpp"
#include <vulkan/vulkan_core.h>

Rectangle::Rectangle(Scene *scene) : Model(scene) {}

void Rectangle::init() {
	engine = scene->getScenes().getEngine();
	buildUnitQuadMesh();
	initInfo.instanceStrideBytes = sizeof(InstanceData);
	initInfo.shaders = Assets::compileShaderProgram(Assets::shaderRootPath + "/rectangle", engine->getDevice());
	Model::init();
	InstanceData placeholder{};
	upsertInstance(0, placeholder);
}

void Rectangle::syncPickingInstances() { Model::syncPickingInstances<InstanceData>(); }

void Rectangle::buildUnitQuadMesh() {
	static const Vertex kVerts[4] = {
		{{-0.5f, -0.5f, 0.0f}},
		{{0.5f, -0.5f, 0.0f}},
		{{0.5f, 0.5f, 0.0f}},
		{{-0.5f, 0.5f, 0.0f}},
	};
	static const uint32_t kIdx[6] = {0, 1, 2, 0, 2, 3};

	Model::Mesh m{};
	m.vsrc.data = kVerts;
	m.vsrc.bytes = sizeof(kVerts);
	m.vsrc.stride = sizeof(Vertex);

	m.isrc.data = kIdx;
	m.isrc.count = 6;

	using F = VkFormat;
	m.vertexAttrs = {
		{0, 0, F::VK_FORMAT_R32G32B32_SFLOAT, uint32_t(offsetof(Vertex, pos))},

		{1, 1, F::VK_FORMAT_R32G32B32A32_SFLOAT, uint32_t(offsetof(InstanceData, model) + sizeof(glm::vec4) * 0)},
		{2, 1, F::VK_FORMAT_R32G32B32A32_SFLOAT, uint32_t(offsetof(InstanceData, model) + sizeof(glm::vec4) * 1)},
		{3, 1, F::VK_FORMAT_R32G32B32A32_SFLOAT, uint32_t(offsetof(InstanceData, model) + sizeof(glm::vec4) * 2)},
		{4, 1, F::VK_FORMAT_R32G32B32A32_SFLOAT, uint32_t(offsetof(InstanceData, model) + sizeof(glm::vec4) * 3)},
		{5, 1, F::VK_FORMAT_R32G32B32A32_SFLOAT, uint32_t(offsetof(InstanceData, color))},
        {6, 1, VK_FORMAT_R32G32B32A32_SFLOAT,    uint32_t(offsetof(InstanceData, outlineColor))},
		{7, 1, F::VK_FORMAT_R32_SFLOAT, uint32_t(offsetof(InstanceData, outlineWidth))},
		{8, 1, F::VK_FORMAT_R32_SFLOAT, uint32_t(offsetof(InstanceData, borderRadius))},
	};

	initInfo.mesh = m;
}

void Rectangle::createGraphicsPipeline() {
    Model::createGraphicsPipeline();
	pipeline->graphicsPipeline.rasterizationStateCI.cullMode = VK_CULL_MODE_NONE;

    pipeline->graphicsPipeline.depthStencilStateCI.depthTestEnable = VK_FALSE;
    pipeline->graphicsPipeline.depthStencilStateCI.depthWriteEnable = VK_FALSE;
}
