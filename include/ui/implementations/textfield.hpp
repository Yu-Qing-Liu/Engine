#pragma once

#include "colors.hpp"
#include "text.hpp"
#include "widget.hpp"

class TextField : public Widget {
  public:
	TextField(TextField &&) = delete;
	TextField(const TextField &) = delete;
	TextField &operator=(TextField &&) = delete;
	TextField &operator=(const TextField &) = delete;
	~TextField() = default;

	struct Params {
		string text = "Placeholder";
		vec2 center = vec2(0.0f, 0.0f);	 // true center in screen pixels
		vec2 dim = vec2(800.0f, 800.0f); // width/height in pixels
		float lineSpacing = 0.0f;		 // extra spacing added to natural line height
		vec4 margins = vec4(0.0f);		 // L,T,R,B
		vec4 textColor = Colors::Red;
	};

	TextField(Scene *scene, const Model::MVP &mvp, Model::ScreenParams &screenParams, const Text::FontParams &textParams, const VkRenderPass &renderPass = Engine::renderPass);

	void updateScreenParams();
	void swapChainUpdate();
	void updateUniformBuffers(const Model::MVP &mvp);
	void render() override;

	Params params{};
	Text::FontParams textParams;

	Model::ScreenParams sp;	  // inner scissor/viewport (text)
	Model::ScreenParams bgSp; // outer (+margins) if you want background widgets to use it

	std::unique_ptr<Text> textModel;

	Model::MVP mvpLocal{};
	glm::mat4 projLocal{1.0f};

  private:
	void wrap();
};
