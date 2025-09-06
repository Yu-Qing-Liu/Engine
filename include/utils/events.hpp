#pragma once

#include <GLFW/glfw3.h>
#include <functional>

using MouseClickCallback = std::function<void(int, int, int)>;
using KeyboardCallback = std::function<void(int, int, int, int)>;

namespace Events {

inline std::vector<MouseClickCallback> mouseCallbacks{};
inline std::vector<KeyboardCallback> keyboardCallbacks{};

inline void handleMouseCallbacks(GLFWwindow* window, int button, int action, int mods) {
    for (const auto &cb : mouseCallbacks) {
        cb(button, action, mods);
    }
}

inline void handleKeyboardCallbacks(GLFWwindow* window, int key, int scancode, int action, int mods) {
    for (const auto &cb : keyboardCallbacks) {
        cb(key, scancode, action, mods);
    }
}

} // namespace Events
