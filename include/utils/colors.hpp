#pragma once
#include <glm/glm.hpp>

namespace Colors {

struct Color {
	glm::vec3 rgb;
	constexpr operator glm::vec4() const { return {rgb.r, rgb.g, rgb.b, 1.f}; }
	constexpr glm::vec4 operator()(float a) const { return {rgb.r, rgb.g, rgb.b, a}; }
};

// vec4 == Color  (all 4 components must match; Color implies a = 1)
inline bool operator==(const glm::vec4 &lhs, const Color &rhs) noexcept { return lhs.r == rhs.rgb.r && lhs.g == rhs.rgb.g && lhs.b == rhs.rgb.b && lhs.a == 1.0f; }
inline bool operator!=(const glm::vec4 &lhs, const Color &rhs) noexcept { return !(lhs == rhs); }

// Color == vec4
inline bool operator==(const Color &lhs, const glm::vec4 &rhs) noexcept { return rhs == lhs; }
inline bool operator!=(const Color &lhs, const glm::vec4 &rhs) noexcept { return !(lhs == rhs); }

// Color == Color (RGB exact)
inline bool operator==(const Color &a, const Color &b) noexcept { return a.rgb.r == b.rgb.r && a.rgb.g == b.rgb.g && a.rgb.b == b.rgb.b; }
inline bool operator!=(const Color &a, const Color &b) noexcept { return !(a == b); }

// Instances
inline constexpr Color Transparent{{0.f,0.f,0.f}};
inline constexpr Color White{{1.f, 1.f, 1.f}};
inline constexpr Color Black{{0.f, 0.f, 0.f}};
inline constexpr Color Gray{{0.33f, 0.33f, 0.33f}};
inline constexpr Color Red{{1.f, 0.f, 0.f}};
inline constexpr Color Green{{0.f, 1.f, 0.f}};
inline constexpr Color Blue{{0.f, 0.f, 1.f}};
inline constexpr Color Yellow{{1.f, 1.f, 0.f}};
inline constexpr Color Cyan{{0.f, 1.f, 1.f}};
inline constexpr Color Purple{{0.3f, 0.f, 0.3f}};
inline constexpr Color Orange{{1.f, 0.349f, 0.f}};
inline constexpr Color Teal{{0.f, 0.50, 0.50}};
inline constexpr Color LightBlue{{0.678f, 0.847f, 0.902f}};
inline constexpr Color Turquoise{{0.f, 1.f, 0.8f}};
inline constexpr Color Pink{{1.0f, 0.71f, 0.76f}};
inline constexpr Color Lime{{0.196f, 0.804f, 0.196f}};
inline constexpr Color DeepPink{{1.0f, 0.078f, 0.588f}};
inline constexpr Color Tan{{0.824f, 0.706f, 0.549f}};
inline constexpr Color DarkBlue{{0.f, 0.20f, 0.4f}};

} // namespace Colors
