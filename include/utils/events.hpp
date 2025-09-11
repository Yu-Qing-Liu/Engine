#pragma once

#include <functional>
#include <vector>

#if !ANDROID_VK
  #include <GLFW/glfw3.h>
#else
  #include <android/input.h>
  #include <android_native_app_glue.h>
  #include <deque>
  #include <chrono>
#endif

// Your existing callback types:
using MouseClickCallback = std::function<void(int /*button*/, int /*action*/, int /*mods*/)>; 
using KeyboardCallback   = std::function<void(int /*key*/, int /*scancode*/, int /*action*/, int /*mods*/)>;
using CharacterInputCallback = std::function<void(unsigned int codepoint)>;

namespace Events {

// ===== Portable constants (match GLFW values where practical) =====
enum : int { ACTION_RELEASE = 0, ACTION_PRESS = 1, ACTION_REPEAT = 2 };
enum : int { MOD_SHIFT = 0x0001, MOD_CONTROL = 0x0002, MOD_ALT = 0x0004, MOD_SUPER = 0x0008 };
enum : int { MOUSE_BUTTON_LEFT = 0, MOUSE_BUTTON_RIGHT = 1, MOUSE_BUTTON_MIDDLE = 2 };

// Storage for user callbacks
inline std::vector<MouseClickCallback> mouseCallbacks{};
inline std::vector<KeyboardCallback>   keyboardCallbacks{};
inline std::vector<CharacterInputCallback> characterInputCallbacks{};

inline float pointerX = 0.0f;
inline float pointerY = 0.0f;

// ---- Common dispatchers (used by both backends) ----
inline void dispatchMouseButton(int button, int action, int mods) {
    for (const auto& cb : mouseCallbacks) cb(button, action, mods);
}

inline void dispatchKey(int key, int scancode, int action, int mods) {
    for (const auto& cb : keyboardCallbacks) cb(key, scancode, action, mods);
}

inline void dispatchCharacter(unsigned int codepoint) {
    for (const auto& cb : characterInputCallbacks) cb(codepoint);
}

#if !ANDROID_VK
// ======================= Desktop (GLFW) =======================
// Keep the exact signatures so your current code compiles unchanged.
inline void handleMouseCallbacks(GLFWwindow* /*window*/, int button, int action, int mods) {
    dispatchMouseButton(button, action, mods);
}

inline void handleKeyboardCallbacks(GLFWwindow* /*window*/, int key, int scancode, int action, int mods) {
    dispatchKey(key, scancode, action, mods);
}

inline void handleCharacterInputCallbacks(GLFWwindow*, unsigned int codepoint) {
    dispatchCharacter(codepoint);
}

#else
// ======================= Android (NativeActivity) =======================

// --- Deferred click queue ---
struct DeferredBtn {
    int button, action, mods;
    std::chrono::steady_clock::time_point when;
};
inline std::deque<DeferredBtn> gBtnQueue;

// Tweakable: 0..16 ms is typical (one 60 Hz frame is ~16.7 ms)
#ifndef ANDROID_INPUT_CLICK_DELAY_MS
#define ANDROID_INPUT_CLICK_DELAY_MS 50
#endif

inline void queueButton(int button, int action, int mods, int delay_ms) {
    const auto t = std::chrono::steady_clock::now() + std::chrono::milliseconds(delay_ms);
    gBtnQueue.push_back(DeferredBtn{button, action, mods, t});
}

// Call this once per frame (after you poll Android events)
inline void pumpDeferred() {
    const auto now = std::chrono::steady_clock::now();
    while (!gBtnQueue.empty() && gBtnQueue.front().when <= now) {
        const auto ev = gBtnQueue.front();
        gBtnQueue.pop_front();
        dispatchMouseButton(ev.button, ev.action, ev.mods);
    }
}

// Map Android meta-state to portable MOD_* flags
inline int androidMetaToMods(int32_t meta) {
    int m = 0;
    if (meta & AMETA_SHIFT_ON) m |= MOD_SHIFT;
    if (meta & AMETA_CTRL_ON)  m |= MOD_CONTROL;
    if (meta & AMETA_ALT_ON)   m |= MOD_ALT;
    if (meta & AMETA_META_ON)  m |= MOD_SUPER; // Meta == "Super" on desktop
    return m;
}

// Minimal key mapping to GLFW-like values (letters/digits use ASCII which GLFW uses too)
inline int mapAndroidKeyToPortable(int32_t code) {
    // Letters A–Z
    if (code >= AKEYCODE_A && code <= AKEYCODE_Z) return 'A' + (code - AKEYCODE_A);
    // Digits 0–9 (top row)
    if (code >= AKEYCODE_0 && code <= AKEYCODE_9) return '0' + (code - AKEYCODE_0);

    // A small set of common keys (match GLFW values)
    switch (code) {
        case AKEYCODE_ESCAPE:    return 256; // GLFW_KEY_ESCAPE
        case AKEYCODE_ENTER:     return 257; // GLFW_KEY_ENTER
        case AKEYCODE_TAB:       return 258; // GLFW_KEY_TAB
        case AKEYCODE_DEL:       return 259; // GLFW_KEY_BACKSPACE
        case AKEYCODE_DPAD_RIGHT:return 262; // GLFW_KEY_RIGHT
        case AKEYCODE_DPAD_LEFT: return 263; // GLFW_KEY_LEFT
        case AKEYCODE_DPAD_DOWN: return 264; // GLFW_KEY_DOWN
        case AKEYCODE_DPAD_UP:   return 265; // GLFW_KEY_UP
        case AKEYCODE_PAGE_UP:   return 266; // GLFW_KEY_PAGE_UP
        case AKEYCODE_PAGE_DOWN: return 267; // GLFW_KEY_PAGE_DOWN
        case AKEYCODE_MOVE_HOME: return 268; // GLFW_KEY_HOME
        case AKEYCODE_MOVE_END:  return 269; // GLFW_KEY_END
        case AKEYCODE_INSERT:    return 260; // GLFW_KEY_INSERT
        case AKEYCODE_FORWARD_DEL:return 261;// GLFW_KEY_DELETE
        case AKEYCODE_SPACE:     return 32;  // GLFW_KEY_SPACE (ASCII)
        default:                 return -1;  // unknown
    }
}

// Install in android_main(): app->onInputEvent = Events::handleAndroidInput;
inline int32_t handleAndroidInput(struct android_app* /*app*/, AInputEvent* event) {
    const int32_t type = AInputEvent_getType(event);
    static bool sGestureActive = false;

    if (type == AINPUT_EVENT_TYPE_MOTION) {
        const int32_t action       = AMotionEvent_getAction(event);
        const int32_t actionMasked = action & AMOTION_EVENT_ACTION_MASK;
        const size_t  idx          = (action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK)
                                   >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;

        auto setPointerFrom = [&](size_t i) {
            Events::pointerX = AMotionEvent_getX(event, i);
            Events::pointerY = AMotionEvent_getY(event, i);
        };

        const int mods = androidMetaToMods(AMotionEvent_getMetaState(event));

        switch (actionMasked) {
            case AMOTION_EVENT_ACTION_HOVER_MOVE:
                setPointerFrom(idx);
                return 1;

            case AMOTION_EVENT_ACTION_DOWN:
            case AMOTION_EVENT_ACTION_POINTER_DOWN:
                setPointerFrom(idx);
                Events::queueButton(Events::MOUSE_BUTTON_LEFT, Events::ACTION_PRESS, mods,
                                    ANDROID_INPUT_CLICK_DELAY_MS);
                sGestureActive = true;
                return 1;

            case AMOTION_EVENT_ACTION_MOVE:
                setPointerFrom(AMotionEvent_getPointerCount(event) ? 0 : idx);
                return 1;

            case AMOTION_EVENT_ACTION_UP:
            case AMOTION_EVENT_ACTION_POINTER_UP:
                setPointerFrom(idx);
                if (sGestureActive) {
                    // Delay release by the same amount so PRESS always lands before RELEASE.
                    Events::queueButton(Events::MOUSE_BUTTON_LEFT, Events::ACTION_RELEASE, mods,
                                        ANDROID_INPUT_CLICK_DELAY_MS);
                    sGestureActive = false;
                }
                return 1;

            case AMOTION_EVENT_ACTION_CANCEL:
                if (sGestureActive) {
                    // Flush a release immediately to avoid stuck pressed state.
                    Events::queueButton(Events::MOUSE_BUTTON_LEFT, Events::ACTION_RELEASE, mods, 0);
                    sGestureActive = false;
                }
                return 1;

            case AMOTION_EVENT_ACTION_BUTTON_PRESS:
                setPointerFrom(idx);
                Events::queueButton(Events::MOUSE_BUTTON_LEFT, Events::ACTION_PRESS, mods,
                                    ANDROID_INPUT_CLICK_DELAY_MS);
                sGestureActive = true;
                return 1;

            case AMOTION_EVENT_ACTION_BUTTON_RELEASE:
                setPointerFrom(idx);
                Events::queueButton(Events::MOUSE_BUTTON_LEFT, Events::ACTION_RELEASE, mods,
                                    ANDROID_INPUT_CLICK_DELAY_MS);
                sGestureActive = false;
                return 1;
        }
        return 1;
    }

    if (type == AINPUT_EVENT_TYPE_KEY) {
        const int32_t action   = AKeyEvent_getAction(event);
        const int32_t code     = AKeyEvent_getKeyCode(event);
        const int32_t meta     = AKeyEvent_getMetaState(event);
        const int     mods     = androidMetaToMods(meta);
        const int     key      = mapAndroidKeyToPortable(code);
        const int     scancode = AKeyEvent_getScanCode(event);

        int act = (action == AKEY_EVENT_ACTION_DOWN) ? ACTION_PRESS :
                  (action == AKEY_EVENT_ACTION_UP)   ? ACTION_RELEASE : -1;

        if (act != -1) {
            dispatchKey(key, scancode, act, mods);
            return 1;
        }
        return 0;
    }

    return 0;
}

#endif // ANDROID_VK

} // namespace Events

