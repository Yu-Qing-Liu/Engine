#include "button.hpp"
#include "rectangle.hpp"
#include "text.hpp"

Button::Button(Scene &scene, const UBO &ubo, ScreenParams &screenParams, uint32_t fontSize) : Widget(scene, ubo, screenParams) {
    Text::TextParams tp{ Assets::fontRootPath + "/arial.ttf", fontSize };
    container = make_shared<Rectangle>(scene, ubo, screenParams);
    text = make_shared<Text>(scene, ubo, screenParams, tp);
    components.push_back(container);
    components.push_back(text);
}
