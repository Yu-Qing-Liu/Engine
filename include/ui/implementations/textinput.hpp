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
		glm::vec2 center{0.0f};							// center in screen pixels
		glm::vec2 dim{200.0f, 64.0f};					// size in pixels (W,H)
		glm::vec4 bgColor{1.0f, 1.0f, 1.0f, 1.0f};		// white fill
		glm::vec4 outlineColor{0.0f, 0.0f, 0.0f, 1.0f}; // black outline
		float outlineWidth{1.0f};						// px
		float borderRadius{12.0f};						// px

		std::optional<std::string> placeholderText;
		std::optional<glm::vec4> placeholderTextColor;
	};

	TextInput(Scene &scene, const Model::UBO &ubo, Model::ScreenParams &screenParams, const Text::TextParams &textParams);

	void updateUniformBuffers(const Model::UBO &ubo) override;
	void setParams(const StyleParams &params, std::optional<std::unique_ptr<Model>> icon = std::nullopt);

	void render() override;

  private:
};
