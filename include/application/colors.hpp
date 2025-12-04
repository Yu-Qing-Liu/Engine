#pragma once

#include <glm/glm.hpp>

namespace Colors {

struct Color {
	glm::vec3 rgb;
	constexpr operator glm::vec4() const { return {rgb.r, rgb.g, rgb.b, 1.f}; }
	constexpr glm::vec4 operator()(float a) const { return {rgb.r, rgb.g, rgb.b, a}; }
};

constexpr glm::vec4 inverse(const glm::vec4 &c) noexcept { return glm::vec4(1.0f - c.r, 1.0f - c.g, 1.0f - c.b, c.a); }
constexpr glm::vec4 inverse(const Color &c) noexcept { return glm::vec4(1.0f - c.rgb.r, 1.0f - c.rgb.g, 1.0f - c.rgb.b, 1.0f); }

// vec4 == Color  (all 4 components must match; Color implies a = 1)
inline bool operator==(const glm::vec4 &lhs, const Color &rhs) noexcept { return lhs.r == rhs.rgb.r && lhs.g == rhs.rgb.g && lhs.b == rhs.rgb.b && lhs.a == 1.0f; }
inline bool operator!=(const glm::vec4 &lhs, const Color &rhs) noexcept { return !(lhs == rhs); }

// Color == vec4
inline bool operator==(const Color &lhs, const glm::vec4 &rhs) noexcept { return rhs == lhs; }
inline bool operator!=(const Color &lhs, const glm::vec4 &rhs) noexcept { return !(lhs == rhs); }

// Color == Color (RGB exact)
inline bool operator==(const Color &a, const Color &b) noexcept { return a.rgb.r == b.rgb.r && a.rgb.g == b.rgb.g && a.rgb.b == b.rgb.b; }
inline bool operator!=(const Color &a, const Color &b) noexcept { return !(a == b); }

inline glm::vec4 hsv2rgba(float h, float s, float v, float a = 1.0f) {
	float i = std::floor(h * 6.0f), f = h * 6.0f - i;
	float p = v * (1.0f - s), q = v * (1.0f - f * s), t = v * (1.0f - (1.0f - f) * s);
	int ii = (int)i % 6;
	float r, g, b;
	switch (ii) {
	case 0:
		r = v;
		g = t;
		b = p;
		break;
	case 1:
		r = q;
		g = v;
		b = p;
		break;
	case 2:
		r = p;
		g = v;
		b = t;
		break;
	case 3:
		r = p;
		g = q;
		b = v;
		break;
	case 4:
		r = t;
		g = p;
		b = v;
		break;
	default:
		r = v;
		g = p;
		b = q;
		break;
	}
	return glm::vec4(r, g, b, a);
}

// Instances
inline constexpr Color Transparent{{0.f, 0.f, 0.f}};
inline constexpr Color White{{1.f, 1.f, 1.f}};
inline constexpr Color Black{{0.f, 0.f, 0.f}};
inline constexpr Color Gray{{0.33f, 0.33f, 0.33f}};
inline constexpr Color Red{{1.f, 0.f, 0.f}};
inline constexpr Color DarkRed{{0.5f, 0.f, 0.f}};
inline constexpr Color Green{{0.f, 1.f, 0.f}};
inline constexpr Color DarkGreen{{0.f, 0.4f, 0.f}};
inline constexpr Color Blue{{0.f, 0.f, 1.f}};
inline constexpr Color Yellow{{1.f, 1.f, 0.f}};
inline constexpr Color Cyan{{0.f, 1.f, 1.f}};
inline constexpr Color Purple{{0.3f, 0.f, 0.3f}};
inline constexpr Color Orange{{1.f, 0.55f, 0.f}};
inline constexpr Color DarkOrange{{1.f, 0.349f, 0.f}};
inline constexpr Color Teal{{0.f, 0.50, 0.50}};
inline constexpr Color LightBlue{{0.678f, 0.847f, 0.902f}};
inline constexpr Color Turquoise{{0.f, 1.f, 0.8f}};
inline constexpr Color Pink{{1.0f, 0.71f, 0.76f}};
inline constexpr Color Lime{{0.196f, 0.804f, 0.196f}};
inline constexpr Color DeepPink{{1.0f, 0.078f, 0.588f}};
inline constexpr Color Tan{{0.824f, 0.706f, 0.549f}};
inline constexpr Color DarkBlue{{0.f, 0.20f, 0.4f}};
inline constexpr Color Cerulean{{0.11f, 0.67f, 0.84f}};
inline constexpr Color DarkCerulean{{0.077f, 0.459f, 0.588f}};

} // namespace Colors
