#include "text.hpp"
#include "rectangle.hpp"

using std::make_unique;
using std::pair;
using std::unique_ptr;
using UBO = Model::UBO;
using ScreenParams = Model::ScreenParams;

class Button {
  public:
	Button(Scene &scene, const UBO &ubo, ScreenParams &screenParams, uint32_t fontSize);
	Button(Button &&) = delete;
	Button(const Button &) = delete;
	Button &operator=(Button &&) = delete;
	Button &operator=(const Button &) = delete;
	~Button() = default;

	vec4 bgColor;

	vec2 btnCenter;
	vec2 dim;

	string text;
	vec2 textCenter;
	vec4 textColor;

	vec2 iconCenter;
	vec3 iconDim;
	unique_ptr<Model> icon;

	void setParams();
	void render();

  private:
	unique_ptr<Rectangle> container;
	unique_ptr<Text> textModel;
};
