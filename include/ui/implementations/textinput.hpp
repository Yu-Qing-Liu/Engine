#pragma once

#include "colors.hpp"
#include "textfield.hpp"
#include "widget.hpp"

class TextInput : public Widget {
  public:
	TextInput(TextInput &&) = delete;
	TextInput(const TextInput &) = delete;
	TextInput &operator=(TextInput &&) = delete;
	TextInput &operator=(const TextInput &) = delete;
	~TextInput() = default;

	struct Params {
		// container style
		vec2 center{0.0f};				   // center in screen pixels
		vec2 textCenter{0.0f};			   // text center
		vec2 dim{200.0f, 64.0f};		   // size in pixels (W,H)
		vec4 bgColor{Colors::White(0.05)}; // white fill
		vec4 activeBgColor{Colors::White(0.10)};
		vec4 outlineColor{Colors::Gray(0.25)}; // outline
		vec4 activeOutlineColor{Colors::Blue};
		float outlineWidth{1.0f};  // px
		float borderRadius{12.0f}; // px
		float lineSpacing{0.0f};

		std::string placeholderText{"Enter Text!"};
		vec4 placeholderTextColor{Colors::Gray};

		vec4 textColor{Colors::White};
		vec4 activeTextColor{Colors::Green};
	};

	TextInput(Scene *scene, const Model::MVP &ubo, Model::ScreenParams &screenParams, const Text::FontParams &textParams, const VkRenderPass &renderPass = Engine::renderPass);

	void swapChainUpdate();
	void updateUniformBuffers(std::optional<Model::MVP> mvp = std::nullopt);
	void render() override;

	bool selected = false;
	Params params{};
	std::string text{""};

	std::unique_ptr<TextField> textField;
};
