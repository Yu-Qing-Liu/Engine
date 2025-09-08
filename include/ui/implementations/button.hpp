#include "widget.hpp"

using std::pair;
using std::unique_ptr;

class Button : public Widget {
  public:
	Button(Scene &scene, const UBO &ubo, ScreenParams &screenParams, uint32_t fontSize);
	Button(Button &&) = delete;
	Button(const Button &) = delete;
	Button &operator=(Button &&) = delete;
	Button &operator=(const Button &) = delete;
	~Button() = default;

	pair<float, float> getCenter();

	void setLocation(const pair<float, float> &coords, float buttonWidth, float buttonHeight);
	void setLocation(const pair<float, float> &x1y1, const pair<float, float> &x2y2);

	void setText(const std::string &text, const pair<float, float> &textCenter, const vec4 &textColor);
	void setModel(unique_ptr<Model> model, const pair<float, float> &modelCenter);
	void setContent(unique_ptr<Model> model, const std::string &text, const pair<float, float> &modelCenter, const pair<float, float> &textCenter, const vec4 &textColor);

  private:
	shared_ptr<Model> container;
	shared_ptr<Model> text;
};
