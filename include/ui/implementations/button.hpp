#pragma once

#include "colors.hpp"
#include "text.hpp"
#include "widget.hpp"
#include <memory>
#include <optional>

class Button : public Widget {
  public:
	Button(Button &&) = delete;
	Button(const Button &) = delete;
	Button &operator=(Button &&) = delete;
	Button &operator=(const Button &) = delete;

	struct StyleParams {
		vec2 center{0.0f};		 // button center in screen pixels
		vec2 textCenter{0.0f};	 // text center
		vec2 dim{200.0f, 64.0f}; // button size in pixels (W,H)

		vec4 bgColor{Colors::White};	 // white fill
		vec4 outlineColor{Colors::Gray}; // black outline
		float outlineWidth{1.0f};		 // px
		float borderRadius{12.0f};		 // px

		string text;
		vec4 textColor{Colors::Black};

		vec2 iconCenter{0.0f};			  // optional icon center
		std::optional<glm::vec3> iconDim; // optional icon scale (x,y,1)
	};

	Button(Scene *scene, const Model::MVP &ubo, Model::ScreenParams &screenParams, const Text::FontParams &textParams);
	~Button() = default;

	void updateUniformBuffers(const Model::MVP &ubo) override;
	void setParams(const StyleParams &params, std::optional<std::unique_ptr<Model>> icon = std::nullopt);

	void render() override;

	// models
	std::unique_ptr<Text> textModel;
	std::unique_ptr<Model> icon;

	StyleParams styleParams{};
};
