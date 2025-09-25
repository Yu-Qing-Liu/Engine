#pragma once

#include "model.hpp"
#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

using glm::lookAt;
using glm::mat4;
using glm::perspective;
using glm::vec3;
using MVP = Model::MVP;

namespace Camera {

// Blender editor-style inputs (meters)
inline float focalLength = 0.05f; // 50 mm = 0.05 m
inline float clipStart = 0.01f;	  // 1 cm
inline float clipEnd = 1000.0f;
inline float sensorWidth = 0.036f; // 36 mm = 0.036 m

inline MVP blenderPerspectiveMVP(float screenWidth, float screenHeight, mat4 view = lookAt(vec3(1, 1, 1), vec3(0, 0, 0), vec3(0, 0, 1))) {
	const float aspect = screenWidth / screenHeight;
	const float fovH = 2.0f * std::atan((sensorWidth * 0.5f) / focalLength);
	const float fovV = 2.0f * std::atan(std::tan(fovH * 0.5f) / aspect);

	MVP u{};
	u.model = mat4(1.0f);
	u.view = view;
	u.proj = glm::perspectiveRH_ZO(fovV, aspect, clipStart, clipEnd); // ← ZO
	return u;
}

inline MVP blenderOrthographicMVP(float screenWidth, float screenHeight, float orthoScale, mat4 view = lookAt(vec3(1, 1, 1), vec3(0, 0, 0), vec3(0, 0, 1))) {
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

	MVP u{};
	u.model = mat4(1.0f);
	u.view = view;
	// near=0 is fine for ortho in Vulkan; far>near
	u.proj = glm::orthoRH_ZO(-halfW, +halfW, -halfH, +halfH, 0.0f, clipEnd); // ← ZO
	return u;
}

} // namespace Camera
