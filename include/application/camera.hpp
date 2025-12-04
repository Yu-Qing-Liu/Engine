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
