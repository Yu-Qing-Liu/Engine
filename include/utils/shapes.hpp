#pragma once

#include "instancedpolygon.hpp"
#include "model.hpp"
#include "polygon.hpp"
#include <cmath>

using std::make_shared;
using std::make_unique;
using std::shared_ptr;
using std::unique_ptr;
using std::unordered_map;
using MVP = Model::MVP;
using ScreenParams = Model::ScreenParams;

namespace Shapes {

inline unique_ptr<Polygon> cube(Scene *scene, const MVP &ubo, ScreenParams &screenParams) {
	return make_unique<Polygon>(scene, ubo, screenParams,
								std::vector<Polygon::Vertex>{
									// idx, position                 // color (RGBA)
									/*0*/ {{-0.5f, -0.5f, -0.5f}, {1.0f, 1.0f, 1.0f, 1.0f}}, // LBB
									/*1*/ {{0.5f, -0.5f, -0.5f}, {1.0f, 1.0f, 1.0f, 1.0f}},	 // RBB
									/*2*/ {{0.5f, 0.5f, -0.5f}, {1.0f, 1.0f, 1.0f, 1.0f}},	 // RTB
									/*3*/ {{-0.5f, 0.5f, -0.5f}, {1.0f, 1.0f, 1.0f, 1.0f}},	 // LTB
									/*4*/ {{-0.5f, -0.5f, 0.5f}, {1.0f, 1.0f, 1.0f, 1.0f}},	 // LBF
									/*5*/ {{0.5f, -0.5f, 0.5f}, {1.0f, 1.0f, 1.0f, 1.0f}},	 // RBF
									/*6*/ {{0.5f, 0.5f, 0.5f}, {1.0f, 1.0f, 1.0f, 1.0f}},	 // RTF
									/*7*/ {{-0.5f, 0.5f, 0.5f}, {1.0f, 1.0f, 1.0f, 1.0f}},	 // LTF
								},
								std::vector<uint32_t>{
									4, 5, 6, 6, 7, 4, 1, 0, 3, 3, 2, 1, 0, 4, 7, 7, 3, 0, 5, 1, 2, 2, 6, 5, 3, 7, 6, 6, 2, 3, 0, 1, 5, 5, 4, 0,
								});
}

inline unique_ptr<InstancedPolygon> cubes(Scene *scene, const MVP &ubo, ScreenParams &screenParams, int instances = 65536) {
	shared_ptr<unordered_map<int, InstancedPolygonData>> polygonInstances = make_shared<unordered_map<int, InstancedPolygonData>>(instances);
	return make_unique<InstancedPolygon>(scene, ubo, screenParams,
										 std::vector<InstancedPolygon::Vertex>{
											 // idx, position                 // color (RGBA)
											 /*0*/ {{-0.5f, -0.5f, -0.5f}, {1.0f, 1.0f, 1.0f, 1.0f}}, // LBB
											 /*1*/ {{0.5f, -0.5f, -0.5f}, {1.0f, 1.0f, 1.0f, 1.0f}},  // RBB
											 /*2*/ {{0.5f, 0.5f, -0.5f}, {1.0f, 1.0f, 1.0f, 1.0f}},	  // RTB
											 /*3*/ {{-0.5f, 0.5f, -0.5f}, {1.0f, 1.0f, 1.0f, 1.0f}},  // LTB
											 /*4*/ {{-0.5f, -0.5f, 0.5f}, {1.0f, 1.0f, 1.0f, 1.0f}},  // LBF
											 /*5*/ {{0.5f, -0.5f, 0.5f}, {1.0f, 1.0f, 1.0f, 1.0f}},	  // RBF
											 /*6*/ {{0.5f, 0.5f, 0.5f}, {1.0f, 1.0f, 1.0f, 1.0f}},	  // RTF
											 /*7*/ {{-0.5f, 0.5f, 0.5f}, {1.0f, 1.0f, 1.0f, 1.0f}},	  // LTF
										 },
										 std::vector<uint32_t>{
											 4, 5, 6, 6, 7, 4, 1, 0, 3, 3, 2, 1, 0, 4, 7, 7, 3, 0, 5, 1, 2, 2, 6, 5, 3, 7, 6, 6, 2, 3, 0, 1, 5, 5, 4, 0,
										 },
										 polygonInstances, instances);
}

inline unique_ptr<Polygon> sphere(Scene *scene, const MVP &ubo, ScreenParams &screenParams, uint32_t latitudeSegments = 16, uint32_t longitudeSegments = 32, float radius = 0.5f) {
	// Clamp to sane minimums so we can form triangles
	latitudeSegments = std::max<uint32_t>(latitudeSegments, 2);
	longitudeSegments = std::max<uint32_t>(longitudeSegments, 3);

	std::vector<Polygon::Vertex> vertices;
	std::vector<uint32_t> indices;

	vertices.reserve((latitudeSegments + 1) * (longitudeSegments + 1));
	indices.reserve(latitudeSegments * longitudeSegments * 6);

	// Build vertices (Y up). Theta: [0..pi], Phi: [0..2pi)
	for (uint32_t lat = 0; lat <= latitudeSegments; ++lat) {
		const float v = static_cast<float>(lat) / static_cast<float>(latitudeSegments);
		const float theta = v * static_cast<float>(M_PI); // 0 (north pole) .. π (south pole)
		const float sinT = std::sin(theta);
		const float cosT = std::cos(theta);

		for (uint32_t lon = 0; lon <= longitudeSegments; ++lon) {
			const float u = static_cast<float>(lon) / static_cast<float>(longitudeSegments);
			const float phi = u * 2.0f * static_cast<float>(M_PI); // 0 .. 2π
			const float sinP = std::sin(phi);
			const float cosP = std::cos(phi);

			const float x = radius * sinT * cosP;
			const float y = radius * cosT; // Y-up
			const float z = radius * sinT * sinP;

			vertices.push_back(Polygon::Vertex{
				{x, y, z}, {1.0f, 1.0f, 1.0f, 1.0f} // white, like cube()
			});
		}
	}

	// Build indices (two triangles per quad), CCW winding facing outward
	const uint32_t ringStride = longitudeSegments + 1;
	for (uint32_t lat = 0; lat < latitudeSegments; ++lat) {
		for (uint32_t lon = 0; lon < longitudeSegments; ++lon) {
			const uint32_t a = lat * ringStride + lon;
			const uint32_t b = (lat + 1) * ringStride + lon;
			const uint32_t c = b + 1;
			const uint32_t d = a + 1;

			// CCW winding facing outward
			indices.push_back(a);
			indices.push_back(b);
			indices.push_back(c);

			indices.push_back(a);
			indices.push_back(c);
			indices.push_back(d);
		}
	}

	return make_unique<Polygon>(scene, ubo, screenParams, std::move(vertices), std::move(indices));
}

inline unique_ptr<InstancedPolygon> spheres(Scene *scene, const MVP &ubo, ScreenParams &screenParams, int instances = 65536, uint32_t latitudeSegments = 16, uint32_t longitudeSegments = 32, float radius = 0.5f) {
	// Clamp to reasonable minimums
	latitudeSegments = std::max<uint32_t>(latitudeSegments, 2);
	longitudeSegments = std::max<uint32_t>(longitudeSegments, 3);

	std::vector<InstancedPolygon::Vertex> vertices;
	std::vector<uint32_t> indices;

	vertices.reserve((latitudeSegments + 1) * (longitudeSegments + 1));
	indices.reserve(latitudeSegments * longitudeSegments * 6);

	// Build sphere vertices
	for (uint32_t lat = 0; lat <= latitudeSegments; ++lat) {
		float v = static_cast<float>(lat) / static_cast<float>(latitudeSegments);
		float theta = v * static_cast<float>(M_PI);
		float sinT = std::sin(theta);
		float cosT = std::cos(theta);

		for (uint32_t lon = 0; lon <= longitudeSegments; ++lon) {
			float u = static_cast<float>(lon) / static_cast<float>(longitudeSegments);
			float phi = u * 2.0f * static_cast<float>(M_PI);
			float sinP = std::sin(phi);
			float cosP = std::cos(phi);

			float x = radius * sinT * cosP;
			float y = radius * cosT;
			float z = radius * sinT * sinP;

			vertices.push_back(InstancedPolygon::Vertex{
				{x, y, z}, {1.0f, 1.0f, 1.0f, 1.0f} // white like cubes()
			});
		}
	}

	// Build indices (two triangles per quad)
	const uint32_t ringStride = longitudeSegments + 1;
	for (uint32_t lat = 0; lat < latitudeSegments; ++lat) {
		for (uint32_t lon = 0; lon < longitudeSegments; ++lon) {
			const uint32_t a = lat * ringStride + lon;
			const uint32_t b = (lat + 1) * ringStride + lon;
			const uint32_t c = b + 1;
			const uint32_t d = a + 1;

			// CCW winding facing outward
			indices.push_back(a);
			indices.push_back(b);
			indices.push_back(c);

			indices.push_back(a);
			indices.push_back(c);
			indices.push_back(d);
		}
	}

	// Shared map of per-instance data
	auto polygonInstances = std::make_shared<std::unordered_map<int, InstancedPolygonData>>(instances);

	return std::make_unique<InstancedPolygon>(scene, ubo, screenParams, std::move(vertices), std::move(indices), polygonInstances, instances);
}

inline unique_ptr<Polygon> dodecahedron(Scene *scene, const MVP &ubo, ScreenParams &screenParams) {
	float phi = (1.0f + std::sqrt(5.0f)) * 0.5f; // φ
	float invphi = 1.0f / phi;					 // 1/φ
	// All listed coordinates have the same radius √3; scale to radius 0.5
	float s = 0.5f / std::sqrt(3.0f);

	using V = Polygon::Vertex;
	std::vector<V> vertices = {
		// 0..7: (±1, ±1, ±1)
		{{s * +1, s * +1, s * +1}, {1, 1, 1, 1}}, // 0
		{{s * +1, s * +1, s * -1}, {1, 1, 1, 1}}, // 1
		{{s * +1, s * -1, s * +1}, {1, 1, 1, 1}}, // 2
		{{s * +1, s * -1, s * -1}, {1, 1, 1, 1}}, // 3
		{{s * -1, s * +1, s * +1}, {1, 1, 1, 1}}, // 4
		{{s * -1, s * +1, s * -1}, {1, 1, 1, 1}}, // 5
		{{s * -1, s * -1, s * +1}, {1, 1, 1, 1}}, // 6
		{{s * -1, s * -1, s * -1}, {1, 1, 1, 1}}, // 7

		// 8..11: (0, ±1/φ, ±φ)
		{{s * 0.0f, s * +invphi, s * +phi}, {1, 1, 1, 1}}, // 8
		{{s * 0.0f, s * +invphi, s * -phi}, {1, 1, 1, 1}}, // 9
		{{s * 0.0f, s * -invphi, s * +phi}, {1, 1, 1, 1}}, // 10
		{{s * 0.0f, s * -invphi, s * -phi}, {1, 1, 1, 1}}, // 11

		// 12..15: (±1/φ, ±φ, 0)
		{{s * +invphi, s * +phi, s * 0.0f}, {1, 1, 1, 1}}, // 12
		{{s * +invphi, s * -phi, s * 0.0f}, {1, 1, 1, 1}}, // 13
		{{s * -invphi, s * +phi, s * 0.0f}, {1, 1, 1, 1}}, // 14
		{{s * -invphi, s * -phi, s * 0.0f}, {1, 1, 1, 1}}, // 15

		// 16..19: (±φ, 0, ±1/φ)
		{{s * +phi, s * 0.0f, s * +invphi}, {1, 1, 1, 1}}, // 16
		{{s * +phi, s * 0.0f, s * -invphi}, {1, 1, 1, 1}}, // 17
		{{s * -phi, s * 0.0f, s * +invphi}, {1, 1, 1, 1}}, // 18
		{{s * -phi, s * 0.0f, s * -invphi}, {1, 1, 1, 1}}, // 19
	};

	// 12 pentagons, triangulated as fan (a,b,c), (a,c,d), (a,d,e)
	const std::array<std::array<uint32_t, 5>, 12> faces = {{
		{{0, 8, 10, 2, 16}},  // top-front-right
		{{0, 12, 14, 4, 8}},  // top-left-front
		{{0, 16, 17, 1, 12}}, // top-right-back
		{{1, 9, 5, 14, 12}},  // top-left-back
		{{1, 17, 3, 11, 9}},  // right-back
		{{2, 10, 6, 15, 13}}, // left-front
		{{2, 13, 3, 17, 16}}, // right-front
		{{3, 13, 15, 7, 11}}, // bottom-back-right
		{{4, 14, 5, 19, 18}}, // top-left
		{{4, 18, 6, 10, 8}},  // front-left
		{{5, 9, 11, 7, 19}},  // back-left
		{{6, 18, 19, 7, 15}}, // bottom-left
	}};

	std::vector<uint32_t> indices;
	indices.reserve(12 * 3 * 3); // 12 faces * (5-2) triangles * 3
	for (const auto &f : faces) {
		const uint32_t a = f[0], b = f[1], c = f[2], d = f[3], e = f[4];
		indices.insert(indices.end(), {a, b, c, a, c, d, a, d, e});
	}

	return make_unique<Polygon>(scene, ubo, screenParams, std::move(vertices), std::move(indices));
}

inline unique_ptr<InstancedPolygon> dodecahedra(Scene *scene, const MVP &ubo, ScreenParams &screenParams, int instances = 65536) {
	float phi = (1.0f + std::sqrt(5.0f)) * 0.5f;
	float invphi = 1.0f / phi;
	float s = 0.5f / std::sqrt(3.0f);

	using V = InstancedPolygon::Vertex;
	std::vector<V> vertices = {
		{{s * +1, s * +1, s * +1}, {1, 1, 1, 1}},		   // 0
		{{s * +1, s * +1, s * -1}, {1, 1, 1, 1}},		   // 1
		{{s * +1, s * -1, s * +1}, {1, 1, 1, 1}},		   // 2
		{{s * +1, s * -1, s * -1}, {1, 1, 1, 1}},		   // 3
		{{s * -1, s * +1, s * +1}, {1, 1, 1, 1}},		   // 4
		{{s * -1, s * +1, s * -1}, {1, 1, 1, 1}},		   // 5
		{{s * -1, s * -1, s * +1}, {1, 1, 1, 1}},		   // 6
		{{s * -1, s * -1, s * -1}, {1, 1, 1, 1}},		   // 7
		{{s * 0.0f, s * +invphi, s * +phi}, {1, 1, 1, 1}}, // 8
		{{s * 0.0f, s * +invphi, s * -phi}, {1, 1, 1, 1}}, // 9
		{{s * 0.0f, s * -invphi, s * +phi}, {1, 1, 1, 1}}, // 10
		{{s * 0.0f, s * -invphi, s * -phi}, {1, 1, 1, 1}}, // 11
		{{s * +invphi, s * +phi, s * 0.0f}, {1, 1, 1, 1}}, // 12
		{{s * +invphi, s * -phi, s * 0.0f}, {1, 1, 1, 1}}, // 13
		{{s * -invphi, s * +phi, s * 0.0f}, {1, 1, 1, 1}}, // 14
		{{s * -invphi, s * -phi, s * 0.0f}, {1, 1, 1, 1}}, // 15
		{{s * +phi, s * 0.0f, s * +invphi}, {1, 1, 1, 1}}, // 16
		{{s * +phi, s * 0.0f, s * -invphi}, {1, 1, 1, 1}}, // 17
		{{s * -phi, s * 0.0f, s * +invphi}, {1, 1, 1, 1}}, // 18
		{{s * -phi, s * 0.0f, s * -invphi}, {1, 1, 1, 1}}, // 19
	};

	const std::array<std::array<uint32_t, 5>, 12> faces = {{
		{{0, 8, 10, 2, 16}},
		{{0, 12, 14, 4, 8}},
		{{0, 16, 17, 1, 12}},
		{{1, 9, 5, 14, 12}},
		{{1, 17, 3, 11, 9}},
		{{2, 10, 6, 15, 13}},
		{{2, 13, 3, 17, 16}},
		{{3, 13, 15, 7, 11}},
		{{4, 14, 5, 19, 18}},
		{{4, 18, 6, 10, 8}},
		{{5, 9, 11, 7, 19}},
		{{6, 18, 19, 7, 15}},
	}};

	std::vector<uint32_t> indices;
	indices.reserve(12 * 3 * 3);
	for (const auto &f : faces) {
		const uint32_t a = f[0], b = f[1], c = f[2], d = f[3], e = f[4];
		indices.insert(indices.end(), {a, b, c, a, c, d, a, d, e});
	}

	auto polygonInstances = std::make_shared<std::unordered_map<int, InstancedPolygonData>>(instances);

	return std::make_unique<InstancedPolygon>(scene, ubo, screenParams, std::move(vertices), std::move(indices), polygonInstances, instances);
}

inline std::unique_ptr<Polygon> pentagon(Scene *scene, const MVP &ubo, ScreenParams &screenParams, float radius = 0.5f) {
	using V = Polygon::Vertex;
	std::vector<V> vertices;
	vertices.reserve(5);

	// Centered at origin, CCW around +Z
	const float twoPi = 6.283185307179586f;
	// Start angle so a point is "up" (nice for UI): -90° = -π/2
	const float start = -0.5f * 3.14159265358979323846f;

	for (int i = 0; i < 5; ++i) {
		float a = start + (twoPi * i) / 5.0f;
		vertices.push_back({{radius * std::cos(a), radius * std::sin(a), 0.0f}, {1, 1, 1, 1}});
	}

	// Triangulate as fan: (0,1,2), (0,2,3), (0,3,4)
	std::vector<uint32_t> indices = {0, 1, 2, 0, 2, 3, 0, 3, 4};

	return std::make_unique<Polygon>(scene, ubo, screenParams, std::move(vertices), std::move(indices));
}

inline std::unique_ptr<InstancedPolygon> pentagons(Scene *scene, const MVP &ubo, ScreenParams &screenParams, int instances = 65536, float radius = 0.5f) {
	using V = InstancedPolygon::Vertex;
	std::vector<V> vertices;
	vertices.reserve(5);

	const float twoPi = 6.283185307179586f;
	const float start = -0.5f * 3.14159265358979323846f;

	for (int i = 0; i < 5; ++i) {
		float a = start + (twoPi * i) / 5.0f;
		vertices.push_back({{radius * std::cos(a), radius * std::sin(a), 0.0f}, {1, 1, 1, 1}});
	}

	std::vector<uint32_t> indices = {0, 1, 2, 0, 2, 3, 0, 3, 4};

	auto polygonInstances = std::make_shared<std::unordered_map<int, InstancedPolygonData>>(instances);

	return std::make_unique<InstancedPolygon>(scene, ubo, screenParams, std::move(vertices), std::move(indices), polygonInstances, instances);
}

} // namespace Shapes
