#pragma once

#include "rng.hpp"

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#include <GLFW/glfw3.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

using MouseClickCallback = std::function<void(int /*button*/, int /*action*/, int /*mods*/)>;
using KeyboardCallback = std::function<void(int /*key*/, int /*scancode*/, int /*action*/, int /*mods*/)>;
using CharacterInputCallback = std::function<void(unsigned int codepoint)>;
using WindowFocusedCallback = std::function<void(GLFWwindow *win, int focused)>;
using CursorCallback = std::function<void(GLFWwindow *win, float x, float y)>;
using ScrollCallback = std::function<void(double xoff, double yoff)>;
using UpdateCallback = std::function<void(float dt)>;

using std::string;

namespace Events {

static constexpr int ACTION_RELEASE = 0;
static constexpr int ACTION_PRESS = 1;
static constexpr int ACTION_REPEAT = 2;

static constexpr int MOD_SHIFT_KEY = 0x0001;
static constexpr int MOD_CONTROL_KEY = 0x0002;
static constexpr int MOD_ALT_KEY = 0x0004;
static constexpr int MOD_SUPER_KEY = 0x0008;

static constexpr int MOUSE_BUTTON_LEFT = 0;
static constexpr int MOUSE_BUTTON_RIGHT = 1;
static constexpr int MOUSE_BUTTON_MIDDLE = 2;

// Generic registry that supports register/unregister/dispatch
template <class Fn> class Registry {
  public:
	// Register with explicit eventId; returns the actual key used (in case of collisions)
	std::string add(std::string eventId, Fn cb) {
		if (eventId.empty())
			eventId = RNG::uuid_v4();
		handlers.emplace(eventId, std::move(cb));
		return eventId;
	}
	// Register with auto-generated eventId; returns the generated key
	std::string add(Fn cb) { return add(RNG::uuid_v4(), std::move(cb)); }
	bool remove(const std::string &eventId) { return handlers.erase(eventId) > 0; }
	void clear() { handlers.clear(); }

	template <class... Args> void dispatch(Args &&...args) const {
		// Snapshot keys to allow safe (un)registration during callbacks
		std::vector<std::pair<std::string, const Fn *>> snapshot;
		snapshot.reserve(handlers.size());
		for (auto &kv : handlers)
			snapshot.emplace_back(kv.first, &kv.second);
		for (auto &it : snapshot) {
			// handler might have been removed; check it still exists
			auto found = handlers.find(it.first);
			if (found != handlers.end()) {
				found->second(std::forward<Args>(args)...);
			}
		}
	}

  private:
	std::unordered_map<std::string, Fn> handlers;
};

// One registry per signal
inline Registry<MouseClickCallback> onMouseClick;
inline Registry<KeyboardCallback> onKey;
inline Registry<CharacterInputCallback> onChar;
inline Registry<WindowFocusedCallback> onFocus;
inline Registry<CursorCallback> onCursor;
inline Registry<ScrollCallback> onScroll;
inline Registry<UpdateCallback> onUpdate;

// Registration helpers (explicit eventId)
inline std::string registerMouseClick(MouseClickCallback cb, const string &eventId = RNG::uuid_v4()) { return onMouseClick.add(eventId, std::move(cb)); }
inline std::string registerKeyPress(KeyboardCallback cb, const string &eventId = RNG::uuid_v4()) { return onKey.add(eventId, std::move(cb)); }
inline std::string registerCharacterInput(CharacterInputCallback cb, const string &eventId = RNG::uuid_v4()) { return onChar.add(eventId, std::move(cb)); }
inline std::string registerFocus(WindowFocusedCallback cb, const string &eventId = RNG::uuid_v4()) { return onFocus.add(eventId, std::move(cb)); }
inline std::string registerCursor(CursorCallback cb, const string &eventId = RNG::uuid_v4()) { return onCursor.add(eventId, std::move(cb)); }
inline std::string registerScroll(ScrollCallback cb, const string &eventId = RNG::uuid_v4()) { return onScroll.add(eventId, std::move(cb)); }
inline std::string registerUpdate(UpdateCallback cb, const string &eventId = RNG::uuid_v4()) { return onUpdate.add(eventId, std::move(cb)); }

// Unregister helpers
inline bool unregisterMouseClick(const std::string &eventId) { return onMouseClick.remove(eventId); }
inline bool unregisterKeyPress(const std::string &eventId) { return onKey.remove(eventId); }
inline bool unregisterCharacter(const std::string &eventId) { return onChar.remove(eventId); }
inline bool unregisterFocus(const std::string &eventId) { return onFocus.remove(eventId); }
inline bool unregisterCursor(const std::string &eventId) { return onCursor.remove(eventId); }
inline bool unregisterScroll(const std::string &eventId) { return onScroll.remove(eventId); }
inline bool unregisterUpdate(const std::string &eventId) { return onUpdate.remove(eventId); }

// Application registration hooks (GLFW/ImGui glue)
inline void handleMouseCallbacks(GLFWwindow *window, int button, int action, int mods) {
	onMouseClick.dispatch(button, action, mods);
	ImGui_ImplGlfw_MouseButtonCallback(window, button, action, mods);
}

inline void handleKeyboardCallbacks(GLFWwindow *window, int key, int scancode, int action, int mods) {
	onKey.dispatch(key, scancode, action, mods);
	ImGui_ImplGlfw_KeyCallback(window, key, scancode, action, mods);
}

inline void handleCharacterInputCallbacks(GLFWwindow *window, unsigned int codepoint) {
	onChar.dispatch(codepoint);
	ImGui_ImplGlfw_CharCallback(window, codepoint);
}

inline void handleWindowFocusedCallbacks(GLFWwindow *win, int focused) { onFocus.dispatch(win, focused); }

inline void handleCursorCallbacks(GLFWwindow *win, float x, float y) { onCursor.dispatch(win, x, y); }

inline void handleScrollCallbacks(GLFWwindow *win, double xoff, double yoff) {
	onScroll.dispatch(xoff, yoff);
	ImGui_ImplGlfw_ScrollCallback(win, xoff, yoff);
}

}; // namespace Events
