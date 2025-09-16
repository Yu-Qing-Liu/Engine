#include "model.hpp"
#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

using glm::lookAt;
using glm::mat4;
using glm::perspective;
using glm::vec3;
using UBO = Model::UBO;

namespace Camera {

// Blender editor-style inputs (meters)
inline float focalLength = 0.05f; // 50 mm = 0.05 m
inline float clipStart = 0.01f;	  // 1 cm
inline float clipEnd = 1000.0f;
inline float sensorWidth = 0.036f; // 36 mm = 0.036 m

inline UBO blenderPerspectiveMVP(float screenWidth, float screenHeight, mat4 view = lookAt(vec3(1.0f, 1.0f, 1.0f), vec3(0.0f, 0.0f, 0.0f), vec3(0.0f, 0.0f, 1.0f))) {
	const float aspect = screenWidth / screenHeight;
	const float fovH = 2.0f * std::atan((sensorWidth * 0.5f) / focalLength);
	const float fovV = 2.0f * std::atan(std::tan(fovH * 0.5f) / aspect);

	UBO u{};
	u.model = mat4(1.0f);
	u.view = view;
	u.proj = perspective(fovV, aspect, clipStart, clipEnd);
	return u;
}

inline UBO blenderOrthographicMVP(float screenWidth, float screenHeight, float orthoScale, mat4 view = lookAt(vec3(1.0f, 1.0f, 1.0f), vec3(0.0f, 0.0f, 0.0f), vec3(0.0f, 0.0f, 1.0f))) {
	const float aspect = screenWidth / screenHeight;

	float viewWidth, viewHeight;
	if (aspect >= 1.0f) {
		viewWidth = orthoScale;
		viewHeight = orthoScale / aspect;
	} else {
		viewHeight = orthoScale;
		viewWidth = orthoScale * aspect;
	}

	const float halfW = 0.5f * viewWidth;
	const float halfH = 0.5f * viewHeight;

	UBO u{};
	u.model = mat4(1.0f);
	u.view = view;
	u.proj = ortho(-halfW, +halfW, -halfH, +halfH, clipStart, clipEnd);

	return u;
}

} // namespace Camera
