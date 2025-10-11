#pragma once

#include "colors.hpp"
#include "textfield.hpp"
#include "widget.hpp"

class TextLabel : public Widget {
  public:
	TextLabel(TextLabel &&) = delete;
	TextLabel(const TextLabel &) = delete;
	TextLabel &operator=(TextLabel &&) = delete;
	TextLabel &operator=(const TextLabel &) = delete;
	~TextLabel() = default;

	struct StyleParams {
		// container style
		vec2 center{0.0f};				   // center in screen pixels
		vec2 textCenter{0.0f};			   // text center
		vec2 dim{200.0f, 64.0f};		   // size in pixels (W,H)
		vec4 bgColor{Colors::White(0.05)}; // white fill
		vec4 activeBgColor{Colors::White(0.10)};
		vec4 outlineColor{Colors::Gray(0.25)}; // black outline
		vec4 activeOutlineColor{Colors::Blue};
		float outlineWidth{1.0f};  // px
		float borderRadius{12.0f}; // px
		float lineSpacing{0.0f};
		vec4 textColor{Colors::White};
	};

	TextLabel(Scene *scene, const Model::MVP &ubo, Model::ScreenParams &screenParams, const Text::FontParams &textParams, const VkRenderPass &renderPass = Engine::renderPass);

	void swapChainUpdate();

	void updateUniformBuffers(optional<Model::MVP> mvp = std::nullopt);

	void render() override;

	bool selected = false;
	StyleParams params{};
	string text{"Text Label"};

	std::unique_ptr<TextField> textField;
	Text *textModel;
};
