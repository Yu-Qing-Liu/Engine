#include "text.hpp"
#include "widget.hpp"

class TextInput : public Widget {
  public:
	TextInput(TextInput &&) = delete;
	TextInput(const TextInput &) = delete;
	TextInput &operator=(TextInput &&) = delete;
	TextInput &operator=(const TextInput &) = delete;
	~TextInput() = default;

	struct StyleParams {
		// container style
		vec2 center{0.0f};						   // center in screen pixels
		vec2 textCenter{0.0f};					   // text center
		vec2 dim{200.0f, 64.0f};				   // size in pixels (W,H)
		vec4 bgColor{1.0f, 1.0f, 1.0f, 1.0f};	   // white fill
		vec4 outlineColor{0.0f, 0.0f, 0.0f, 1.0f}; // black outline
		float outlineWidth{1.0f};				   // px
		float borderRadius{12.0f};				   // px

		string placeholderText;
		vec4 placeholderTextColor;

		vec4 textColor;
	};

	TextInput(Scene &scene, const Model::UBO &ubo, Model::ScreenParams &screenParams, const Text::TextParams &textParams);

	void updateUniformBuffers(const Model::UBO &ubo) override;
	void setParams(const StyleParams &params);

	void render() override;

	bool selected = false;

	string text{""};
	vec4 textColor{0.0f, 0.0f, 0.0f, 1.0f};

	string placeholder{"Enter Text!"};
	vec4 placeholderTextColor{0.0f, 0.0f, 0.0f, 1.0f};

	std::unique_ptr<Text> textModel;
};
