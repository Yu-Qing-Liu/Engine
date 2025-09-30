#pragma once

#include "assets.hpp"
#include <glm/glm.hpp>
#include <vector>

class Model;

class BlurPipeline {
  public:
	struct Push {
		glm::vec2 invExtent;
		float radius;
		float lodScale;
		glm::vec4 tint;
		float microTent;
		float cornerRadiusPxOverride;
	};

	explicit BlurPipeline(Model *model);
	~BlurPipeline();

	// Call once after Model is fully created (descriptorSetLayout, vertex format ready)
	virtual void initialize();

	void createCopyPipeAndSets();

	// Call on swapchain resize or when your scene viewport/scissor changes
	void updateCopyViewport(const VkViewport &vp, const VkRect2D &sc);

	// Update parameters
	void setRadius(float r) { radius = r; }
	void setTint(glm::vec4 rgba) { tint = rgba; }
	void setLodScale(float s) { lodScale = s; }
	void setMicroTent(bool on) { microTent = on ? 1.0f : 0.0f; }
	void setCornerRadiusOverride(float px) { cornerRadiusPxOverride = px; } // <0 disables

	// Draw the model in subpass 1 with blur-behind shader
	void copy(VkCommandBuffer cmd);
	virtual void render();

	VkPipeline blurPipe = VK_NULL_HANDLE;
	VkPipelineLayout blurPL = VK_NULL_HANDLE; // set0=modelDSL, set1=blurDSL, + push constants

  protected:
	// owner/model state we borrow
	Model *model = nullptr;

	VkVertexInputBindingDescription bindingDesc{};
	std::vector<VkVertexInputAttributeDescription> attribs{};
	VkDescriptorSetLayout modelDSL = VK_NULL_HANDLE; // set=0 from model (MVP UBO etc.)
	VkShaderModule modelVS = VK_NULL_HANDLE;		 // reuse model's VS

	// viewport/scissor from scene
	VkViewport copyViewport{};
	VkRect2D copyScissor{};

	// program & pipeline for subpass 1
	Assets::ShaderModules prog{}; // compile: (use model VS or dedicated VS) + blur.frag

	// params
	float radius = 64.0f;
	float alpha = 1.0;
	glm::vec4 tint = {0.0f, 0.0f, 0.0f, 0.0f};
	float lodScale = 1.0f;
	float microTent = 1.0f;				  // on by default
	float cornerRadiusPxOverride = -1.0f; // <0 => follow model UBO

	Assets::ShaderModules copyProg{}; // screen/fullscreen copy shaders
	VkPipelineLayout copyPL = VK_NULL_HANDLE;
	VkPipeline copyPipe = VK_NULL_HANDLE;

	uint32_t copiedForImage = UINT32_MAX;

	// helpers
	virtual void createPipeAndSets();
	virtual void destroyPipeAndSets();
};
