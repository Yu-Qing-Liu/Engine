#pragma once

#include "model.hpp"
#include "scene.hpp"

template <typename PC> class ShaderQuad : public Model {
  public:
	ShaderQuad(Scene *scene) : Model(scene) {}
	~ShaderQuad() = default;

	struct InstanceData {
		mat4 model{1.0f};
	};

	struct Vertex {
		vec3 pos;
	};

	void setFragmentShader(const std::string &fragmentShader) { this->fragmentShader = fragmentShader; }

	void init() override {
		engine = scene->getEngine();
		buildUnitQuadMesh();
		initInfo.instanceStrideBytes = sizeof(InstanceData);
		initInfo.shaders = Assets::compileShaderProgram(Assets::shaderRootPath + "/shaderquad", scene->getDevice());
		initInfo.shaders.fragmentShader = Assets::compileShaderProgram(fragmentShader, shaderc_glsl_fragment_shader, scene->getDevice());

		pipeline->graphicsPipeline.pushConstantRangeCount = 1;
		pipeline->graphicsPipeline.pushContantRanges.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		pipeline->graphicsPipeline.pushContantRanges.offset = 0;
		pipeline->graphicsPipeline.pushContantRanges.size = sizeof(PC);

		Model::init();

		// Ensure a default instance exists
		InstanceData placeholder{};
		upsertInstance(0, placeholder);
	}

	void record(VkCommandBuffer cmd) override {
		vkCmdPushConstants(cmd, pipeline->pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pc), &pc);
		Model::record(cmd);
	}

	void enableDepth() { enableDepth_ = true; }

  protected:
	PC pc{};

  private:
	std::string fragmentShader;
	bool enableDepth_ = false;

  private:
	void syncPickingInstances() override { Model::syncPickingInstances<InstanceData>(); }

	void createGraphicsPipeline() override {
		Model::createGraphicsPipeline();
		if (!enableDepth_) {
			pipeline->graphicsPipeline.rasterizationStateCI.cullMode = VK_CULL_MODE_NONE;
			pipeline->graphicsPipeline.depthStencilStateCI.depthTestEnable = VK_FALSE;
			pipeline->graphicsPipeline.depthStencilStateCI.depthWriteEnable = VK_FALSE;
		}
	}

  private:
	void buildUnitQuadMesh() {
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
		};

		initInfo.mesh = m;
	}
};
