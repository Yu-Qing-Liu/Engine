#pragma once

#include "widget.hpp"
#include "text.hpp"
#include <memory>
#include <optional>

class Button : public Widget {
  public:
	Button(Button &&) = delete;
	Button(const Button &) = delete;
	Button &operator=(Button &&) = delete;
	Button &operator=(const Button &) = delete;

	struct StyleParams {
		glm::vec2 center{0.0f};		  // button center in screen pixels
		glm::vec2 dim{200.0f, 64.0f}; // button size in pixels (W,H)

		glm::vec4 bgColor{1.0f, 1.0f, 1.0f, 1.0f};		// white fill
		glm::vec4 outlineColor{0.0f, 0.0f, 0.0f, 1.0f}; // black outline
		float outlineWidth{1.0f};						// px
		float borderRadius{12.0f};						// px

		std::optional<std::string> text;	// label
		std::optional<glm::vec4> textColor; // label color

		glm::vec2 iconCenter{0.0f};		  // optional icon center
		std::optional<glm::vec3> iconDim; // optional icon scale (x,y,1)
	};

	Button(Scene &scene, const Model::UBO &ubo, Model::ScreenParams &screenParams, const Text::TextParams &textParams);
	~Button() = default;

	void updateUniformBuffers(const Model::UBO &ubo) override;
	void setParams(const StyleParams &params, std::optional<std::unique_ptr<Model>> icon = std::nullopt);

	void render() override;

	// models
	std::unique_ptr<Text> textModel;
	std::unique_ptr<Model> icon;

	// state
	std::string label{"Click me!"};
	glm::vec4 labelColor{0.0f, 0.0f, 0.0f, 1.0f}; // black
};
