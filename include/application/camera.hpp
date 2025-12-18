#pragma once

#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

using glm::lookAt;
using glm::mat4;
using glm::vec3;

namespace Camera {

inline float focalLength = 0.05f;
inline float clipStart = 0.01f;
inline float clipEnd = 1000.0f;
inline float sensorWidth = 0.036f;

enum AxisPlane { ZXY, YZX, ZNYX, ZYNX, NYNZX, ZNXNY };

inline mat4 blenderPerspectiveProjection(float vw, float vh) {
	const float safeH = (vh > 0.0f) ? vh : 1.0f;
	const float aspect = vw / safeH;

	const float fovH = 2.0f * std::atan((sensorWidth * 0.5f) / focalLength);
	const float fovV = 2.0f * std::atan(std::tan(fovH * 0.5f) / aspect);

	auto m = glm::perspective(fovV, aspect, clipStart, clipEnd);
	m[1][1] *= -1;
	return m;
}

inline mat4 blenderOrthographicProjection(float vw, float vh, float orthoScale = 1.0f) {
	const float safeH = (vh > 0.0f) ? vh : 1.0f;
	const float aspect = vw / safeH;

	float viewWidth = (aspect >= 1.f) ? orthoScale : orthoScale * aspect;
	float viewHeight = (aspect >= 1.f) ? orthoScale / aspect : orthoScale;

	const float halfW = 0.5f * viewWidth;
	const float halfH = 0.5f * viewHeight;

	auto m = glm::ortho(-halfW, +halfW, -halfH, +halfH, clipStart, clipEnd);
	m[1][1] *= -1.0f;
	return m;
}

inline float camDist(const mat4 &view, const vec3 &lookAt = vec3(0.0f)) {
	mat4 inv = inverse(view);
	vec3 camPos = vec3(inv[3]);
	return length(camPos - lookAt);
}

inline void lookFromAbove(glm::mat4 &view, const glm::vec3 &camTarget) {
	// Inverse view gives us camera position and basis
	glm::mat4 invView = glm::inverse(view);
	glm::vec3 camPos = glm::vec3(invView[3]);
	glm::vec3 oldUp = glm::vec3(invView[1]);

	if (!glm::any(glm::isnan(oldUp)))
		oldUp = glm::normalize(oldUp);
	else
		oldUp = glm::vec3(0.0f, 1.0f, 0.0f);

	// Current forward from camera to scene
	glm::vec3 f = -glm::vec3(invView[2]);
	if (!glm::any(glm::isnan(f)))
		f = glm::normalize(f);
	else
		f = glm::vec3(0.0f, -1.0f, 0.0f);

	// 1) Snap forward to the nearest world axis (±X, ±Y, ±Z)
	const glm::vec3 worldAxes[3] = {glm::vec3(1, 0, 0), glm::vec3(0, 1, 0), glm::vec3(0, 0, 1)};

	float bestAbsDot = -1.0f;
	glm::vec3 forward = f;

	for (int i = 0; i < 3; ++i) {
		float d = glm::dot(f, worldAxes[i]);
		float ad = std::abs(d);
		if (ad > bestAbsDot) {
			bestAbsDot = ad;
			forward = (d >= 0.0f) ? worldAxes[i] : -worldAxes[i];
		}
	}

	// 2) Choose an up axis orthogonal to forward, as close as possible to oldUp
	glm::vec3 upCandidates[3] = {glm::vec3(1, 0, 0), glm::vec3(0, 1, 0), glm::vec3(0, 0, 1)};

	glm::vec3 up = glm::vec3(0, 1, 0);
	float bestUpScore = -1.0f;

	for (int i = 0; i < 3; ++i) {
		glm::vec3 c = upCandidates[i];

		// Must be (almost) orthogonal to forward
		if (std::abs(glm::dot(c, forward)) > 0.01f)
			continue;

		float score = std::abs(glm::dot(c, oldUp));
		if (score > bestUpScore) {
			bestUpScore = score;
			// Keep same general "sense" as oldUp if possible
			up = (glm::dot(c, oldUp) >= 0.0f) ? c : -c;
		}
	}

	// Re-orthogonalize just in case
	glm::vec3 right = glm::normalize(glm::cross(forward, up));
	up = glm::normalize(glm::cross(right, forward));

	// 3) Keep the signed distance along forward from target to current eye
	float dist = glm::dot(camPos - camTarget, forward);
	if (std::abs(dist) < 1e-4f) {
		dist = glm::length(camPos - camTarget);
		if (dist < 0.001f)
			dist = 1.0f;
	}

	// 4) New eye: same axis, same distance, centered on camTarget
	glm::vec3 newEye = camTarget + forward * dist;

	view = glm::lookAt(newEye, camTarget, up);
}

inline void lookFromBehind(mat4 &view, const vec3 &camTarget, float dist = 3.0f, float height = 3.0f) {
	const vec3 up(0.0f, 0.0f, 1.0f);
	vec3 eye = camTarget + vec3(-dist, 0.f, height);
	view = glm::lookAt(eye, camTarget, up);
}

inline void cameraOrthographic(mat4 &view, AxisPlane plane, float dist = 1.f, vec3 camLookAt = vec3(0.f)) {
	vec3 eye(0.0f);
	vec3 up(dist);
	switch (plane) {
	case ZXY: {
		/*
		 *   z
		 *   |
		 *  x└─── y
		 * */
		eye = vec3(dist, 0.0f, 0.0f);
		up = vec3(0.0f, 0.0f, 1.0f);
		break;
	}
	case YZX: {
		/*
		 *   y
		 *   |
		 *  z└─── x
		 * */
		eye = vec3(0.0f, 0.0f, dist);
		up = vec3(0.0f, 1.0f, 0.0f);
		break;
	}
	case ZNYX: {
		/*
		 *    z
		 *    |
		 *  -y└─── x
		 * */
		eye = vec3(0.0f, -dist, 0.0f);
		up = vec3(0.0f, 0.0f, 1.0f);
		break;
	}
	case ZYNX: {
		/*
		 *   z
		 *   |
		 *  y└─── -x
		 * */
		eye = vec3(0.0f, dist, 0.0f);
		up = vec3(0.0f, 0.0f, 1.0f);
		break;
	}
	case NYNZX: {
		/*
		 *    -y
		 *     |
		 *  -z └─── x
		 * */
		eye = vec3(0.0f, 0.0f, -dist);
		up = vec3(0.0f, -1.0f, 0.0f);
		break;
	}
	case ZNXNY: {
		/*
		 *    z
		 *    |
		 *  -x└─── y
		 * */
		eye = vec3(-dist, 0.0f, 0.0f);
		up = vec3(0.0f, 0.0f, 1.0f);
		break;
	}
	default: {
		break;
	}
	}

	view = lookAt(eye, camLookAt, up);
}

} // namespace Camera
