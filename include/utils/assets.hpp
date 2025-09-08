#pragma once

#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <unordered_map>
#include <vector>

#if !ANDROID_VK
#include <openssl/sha.h>
#include <shaderc/shaderc.h>
#include <shaderc/shaderc.hpp>
using namespace shaderc;
#else
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <android_native_app_glue.h>
#endif

#include <vulkan/vulkan_core.h>

#include "engine.hpp"
#include "platform.hpp"

namespace Assets {

namespace fs = std::filesystem;
using namespace Engine;

// ===================== Centralized repo paths (Option A) =====================
// Desktop reads/writes here directly. Android uses these only as APK assets;
// at runtime it reads from <files>/... after hydration.
inline std::string shaderRootPath = std::string(PROJECT_ROOT_DIR) + "/app/src/main/assets/shaders";
inline std::string textureRootPath = std::string(PROJECT_ROOT_DIR) + "/app/src/main/assets/textures";
inline std::string modelRootPath = std::string(PROJECT_ROOT_DIR) + "/app/src/main/assets/meshes";
inline std::string fontRootPath = std::string(PROJECT_ROOT_DIR) + "/app/src/main/assets/fonts";

#if ANDROID_VK
// On Android we *read* from <files>/... (set at runtime). We still ship
// SPIR-V in assets/spirv, but at runtime we hydrate <files>/shaders.
inline std::string shaderCachePath; // set to "<files>/shaders" at runtime
#else
// On desktop we *write* compiled SPIR-V into assets/spirv (so APK picks it up).
inline std::string shaderCachePath = std::string(PROJECT_ROOT_DIR) + "/app/src/main/assets/spirv";
#endif

// --------------------------- helpers ---------------------------
inline std::string joinPath(const std::string &a, const std::string &b) {
	if (a.empty())
		return b;
	return a.back() == '/' ? (a + b) : (a + "/" + b);
}
inline void ensureDir(const std::string &p) {
	if (p.empty())
		return;
	std::error_code ec;
	fs::create_directories(p, ec);
}

inline std::string texturePath(const std::string &rel) { return joinPath(textureRootPath, rel); }
inline std::string meshPath(const std::string &rel) { return joinPath(modelRootPath, rel); }
inline std::string fontPath(const std::string &rel) { return joinPath(fontRootPath, rel); }

inline bool fileExists(const std::string &p) {
	std::error_code ec;
	return fs::exists(p, ec) && fs::is_regular_file(p, ec);
}
inline std::vector<uint8_t> readAllBytes(const std::string &p) {
	std::ifstream f(p, std::ios::binary);
	if (!f)
		return {};
	return std::vector<uint8_t>((std::istreambuf_iterator<char>(f)), {});
}
inline std::vector<uint32_t> readBinaryFileU32(const std::string &path) {
	auto bytes = readAllBytes(path);
	if (bytes.empty() || (bytes.size() % 4) != 0)
		return {};
	std::vector<uint32_t> out(bytes.size() / 4);
	std::memcpy(out.data(), bytes.data(), bytes.size());
	return out;
}

inline std::unordered_map<std::string, int> shaderExtensions = {{".vert", 0}, {".tesc", 1}, {".tese", 2}, {".geom", 3}, {".frag", 4}, {".comp", 5}};

struct ShaderModules {
	VkShaderModule vertexShader = VK_NULL_HANDLE;
	VkShaderModule tessellationControlShader = VK_NULL_HANDLE;
	VkShaderModule tessellationEvaluationShader = VK_NULL_HANDLE;
	VkShaderModule geometryShader = VK_NULL_HANDLE;
	VkShaderModule fragmentShader = VK_NULL_HANDLE;
	VkShaderModule computeShader = VK_NULL_HANDLE;
};
struct ShaderBinaries {
	std::vector<uint32_t> vertexShader;
	std::vector<uint32_t> tessellationControlShader;
	std::vector<uint32_t> tessellationEvaluationShader;
	std::vector<uint32_t> geometryShader;
	std::vector<uint32_t> fragmentShader;
	std::vector<uint32_t> computeShader;
};

inline std::string toAssetRel(const std::string &p) {
	// Known asset roots we ship
	static const char *roots[] = {"/meshes/", "/textures/", "/fonts/", "/spirv/", "/shaders/", "meshes/", "textures/", "fonts/", "spirv/", "shaders/"};
	for (auto r : roots) {
		if (auto pos = p.find(r); pos != std::string::npos) {
			// Drop a leading slash if present
			return (r[0] == '/') ? p.substr(pos + 1) : p.substr(pos);
		}
	}
	// If it's already relative (no leading slash), just return it
	if (!p.empty() && p.front() != '/')
		return p;
	return {}; // not mappable
}

// Read bytes from FS if present; else (on Android) try the APK assets with a derived relative path.
inline std::vector<uint8_t> loadBytes(const std::string &absOrRel) {
	// 1) Filesystem
	auto fsBytes = readAllBytes(absOrRel);
	if (!fsBytes.empty())
		return fsBytes;

#if ANDROID_VK
	// 2) APK assets fallback
	if (!g_app || !g_app->activity || !g_app->activity->assetManager) {
		LOGE("LoadBytes: AssetManager not available");
		return {};
	}
	std::string rel = toAssetRel(absOrRel);
	if (rel.empty()) {
		LOGE("LoadBytes: cannot map to asset-relative path: %s", absOrRel.c_str());
		return {};
	}

	AAsset *a = AAssetManager_open(g_app->activity->assetManager, rel.c_str(), AASSET_MODE_STREAMING);
	if (!a) {
		LOGE("LoadBytes: AAssetManager_open failed for %s", rel.c_str());
		return {};
	}
	const off_t len = AAsset_getLength(a);
	std::vector<uint8_t> out;
	out.resize(static_cast<size_t>(len));
	int rd = AAsset_read(a, out.data(), static_cast<size_t>(len));
	AAsset_close(a);
	if (rd <= 0) {
		LOGE("LoadBytes: read failed for %s", rel.c_str());
		return {};
	}
	out.resize(static_cast<size_t>(rd));
	return out;
#else
	return {};
#endif
}

// ===================== Desktop: compile & cache into assets/spirv =====================
#if !ANDROID_VK
inline Compiler compiler;
inline CompileOptions options;

inline std::string readTextFile(const std::string &filePath) {
	std::ifstream f(filePath);
	if (!f.is_open())
		return {};
	std::stringstream ss;
	ss << f.rdbuf();
	return ss.str();
}
inline std::string computeHash(const std::string &input) {
	unsigned char hash[SHA_DIGEST_LENGTH];
	SHA1(reinterpret_cast<const unsigned char *>(input.data()), input.size(), hash);
	std::stringstream ss;
	for (int i = 0; i < SHA_DIGEST_LENGTH; ++i)
		ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(hash[i]);
	return ss.str();
}
inline void writeBinaryFile(const std::string &path, const std::vector<uint32_t> &data) {
	std::error_code ec;
	fs::create_directories(fs::path(path).parent_path(), ec);
	std::ofstream file(path, std::ios::binary);
	if (!file.is_open()) {
		std::cerr << "Failed to write cache file: " << path << std::endl;
		return;
	}
	file.write(reinterpret_cast<const char *>(data.data()), data.size() * sizeof(uint32_t));
}
inline void deleteOldBinaries(const fs::path &cache_dir, const std::string &basenameSpv) {
	std::error_code ec;
	if (!fs::exists(cache_dir, ec))
		return;
	for (const auto &entry : fs::directory_iterator(cache_dir, ec)) {
		if (ec)
			break;
		if (!entry.is_regular_file())
			continue;
		const std::string name = entry.path().filename().string();
		if (name.size() >= basenameSpv.size() && name.compare(name.size() - basenameSpv.size(), basenameSpv.size(), basenameSpv) == 0) {
			std::error_code ec2;
			fs::remove(entry.path(), ec2);
		}
	}
}
inline std::vector<uint32_t> compileShader(const std::string &shaderPath) {
	std::string shaderCode = readTextFile(shaderPath);
	if (shaderCode.empty())
		return {};
	fs::path p(shaderPath);
	const std::string ext = p.extension().string();
	const auto it = shaderExtensions.find(ext);
	if (it == shaderExtensions.end()) {
		std::cerr << "Unsupported shader ext: " << ext << "\n";
		return {};
	}

	const std::string hash_str = computeHash(ext + shaderCode);
	constexpr const char *kSep = "--";
	const std::string basenameSpv = p.filename().string() + ".spv";
	const fs::path cache_dir(shaderCachePath); // = assets/spirv
	const fs::path cached_path = cache_dir / (hash_str + kSep + basenameSpv);

	if (fileExists(cached_path.string())) {
		auto cached_binary = readBinaryFileU32(cached_path.string());
		if (!cached_binary.empty())
			return cached_binary;
	}

	deleteOldBinaries(cache_dir, basenameSpv);

	shaderc_shader_kind kind = shaderc_glsl_infer_from_source;
	switch (it->second) {
	case 0:
		kind = shaderc_glsl_vertex_shader;
		break;
	case 1:
		kind = shaderc_glsl_tess_control_shader;
		break;
	case 2:
		kind = shaderc_glsl_tess_evaluation_shader;
		break;
	case 3:
		kind = shaderc_glsl_geometry_shader;
		break;
	case 4:
		kind = shaderc_glsl_fragment_shader;
		break;
	case 5:
		kind = shaderc_glsl_compute_shader;
		break;
	}

	SpvCompilationResult result = compiler.CompileGlslToSpv(shaderCode, kind, shaderPath.c_str(), options);
	if (result.GetCompilationStatus() != shaderc_compilation_status_success) {
		std::cerr << "Failed to compile shader: " << shaderPath << "\nError: " << result.GetErrorMessage() << std::endl;
		return {};
	}

	std::vector<uint32_t> spirv(result.cbegin(), result.cend());
	writeBinaryFile(cached_path.string(), spirv); // write into assets/spirv
	return spirv;
}

#else
// ===================== Android: hydrate <files> from APK assets, then select-only =====================

// Set <files>/... roots (prefer external if available for easier inspection).
inline void setResourceDirectories(android_app *app, bool preferExternal = true) {
	const char *ext = app->activity->externalDataPath;
	const char *in = app->activity->internalDataPath;
	const char *base = (preferExternal && ext && *ext) ? ext : in;

	shaderCachePath = joinPath(base, "shaders"); // SPIR-V destination
	// These are the *runtime* read roots:
	modelRootPath = joinPath(base, "meshes");
	fontRootPath = joinPath(base, "fonts");
	textureRootPath = joinPath(base, "textures");

	ensureDir(shaderCachePath);
	ensureDir(modelRootPath);
	ensureDir(fontRootPath);
	ensureDir(textureRootPath);
}

// Recursively copy a directory from APK assets to a filesystem folder (skip if same size exists).
inline void copyAssetDir(android_app *app, const std::string &srcSubdir, const std::string &dstDir) {
	AAssetManager *mgr = app->activity->assetManager;
	ensureDir(dstDir);

	std::function<void(const std::string &, const std::string &)> rec;
	rec = [&](const std::string &sub, const std::string &out) {
		AAssetDir *dir = AAssetManager_openDir(mgr, sub.c_str());
		if (!dir)
			return;
		const char *name = nullptr;
		while ((name = AAssetDir_getNextFileName(dir)) != nullptr) {
			std::string child = sub.empty() ? name : (sub + "/" + name);
			// Try open as file:
			AAssetDir *d = AAssetManager_openDir(mgr, child.c_str());
			if (d) {
				// it's a dir
				AAssetDir_close(d);
				std::string outSub = joinPath(out, name);
				ensureDir(outSub);
				rec(child, outSub);
				continue;
			}
			// Otherwise try file
			if (AAsset *a = AAssetManager_open(mgr, child.c_str(), AASSET_MODE_STREAMING)) {
				// ... copy file as you already do ...
				AAsset_close(a);
			}
		}
		AAssetDir_close(dir);
	};

	rec(srcSubdir, dstDir);
}

// One-shot init used by android_main.cpp
inline void initializeAndroid(android_app *app) {
	// Point runtime to <files>/...
	setResourceDirectories(app, /*preferExternal=*/false);

	// Hydrate <files> from APK assets (smart: copy only if needed)
	// NOTE: assets/spirv (in APK) → <files>/shaders (runtime)
	copyAssetDir(app, "spirv", shaderCachePath);
	copyAssetDir(app, "meshes", modelRootPath);
	copyAssetDir(app, "textures", textureRootPath);
	copyAssetDir(app, "fonts", fontRootPath);
}

// Android load: pick exact "<files>/shaders/<basename>.spv" or newest "*<basename>.spv"
inline std::optional<fs::path> selectCachedBinaryPath(const fs::path &cache_dir, const std::string &basenameSpv) {
	std::error_code ec;
	fs::path exact = cache_dir / basenameSpv;
	if (fs::exists(exact, ec) && fs::is_regular_file(exact, ec))
		return exact;

	if (!fs::exists(cache_dir, ec))
		return std::nullopt;

	std::optional<fs::path> best;
	fs::file_time_type bestTime{};
	for (const auto &entry : fs::directory_iterator(cache_dir, ec)) {
		if (ec)
			break;
		if (!entry.is_regular_file())
			continue;
		const std::string name = entry.path().filename().string();
		if (name.size() >= basenameSpv.size() && name.compare(name.size() - basenameSpv.size(), basenameSpv.size(), basenameSpv) == 0) {
			auto t = fs::last_write_time(entry.path(), ec);
			if (!ec && (!best || t > bestTime)) {
				best = entry.path();
				bestTime = t;
			}
		}
	}
	return best;
}

inline std::vector<uint32_t> compileShader(const std::string &shaderPath /*logical*/) {
	fs::path p(shaderPath);
	const std::string basenameSpv = p.filename().string() + ".spv";
	const fs::path cache_dir(shaderCachePath);

	auto chosen = selectCachedBinaryPath(cache_dir, basenameSpv);
	if (!chosen) {
		std::cerr << "ERROR: No SPIR-V for " << basenameSpv << " in " << cache_dir << " (Android select-only)\n";
		return {};
	}
	return readBinaryFileU32(chosen->string());
}
#endif // ANDROID_VK

// ===================== Bundling & modules =====================
inline ShaderBinaries compileShader(const std::vector<std::string> &shaderPaths) {
	ShaderBinaries bins;
	for (const auto &sp : shaderPaths) {
		const std::string ext = fs::path(sp).extension().string();
		if (ext == ".vert")
			bins.vertexShader = compileShader(sp);
		else if (ext == ".tesc")
			bins.tessellationControlShader = compileShader(sp);
		else if (ext == ".tese")
			bins.tessellationEvaluationShader = compileShader(sp);
		else if (ext == ".geom")
			bins.geometryShader = compileShader(sp);
		else if (ext == ".frag")
			bins.fragmentShader = compileShader(sp);
		else if (ext == ".comp")
			bins.computeShader = compileShader(sp);
		else {
			std::cerr << "Unsupported shader type: " << ext << std::endl;
			std::exit(1);
		}
	}
	return bins;
}

inline VkShaderModule createShaderModule(const std::vector<uint32_t> &binary) {
	if (binary.empty())
		throw std::runtime_error("empty SPIR-V blob");
	VkShaderModuleCreateInfo ci{};
	ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	ci.codeSize = binary.size() * sizeof(uint32_t);
	ci.pCode = binary.data();
	VkShaderModule m{};
	if (vkCreateShaderModule(device, &ci, nullptr, &m) != VK_SUCCESS)
		throw std::runtime_error("failed to create shader module!");
	return m;
}

// Desktop: shaderRootDir is a real folder. Android: we synthesize stage names.
inline ShaderModules compileShaderProgram(const std::string &shaderRootDir) {
	ShaderModules modules;

#if !ANDROID_VK
	std::vector<std::string> shader_paths;
	try {
		for (const auto &entry : fs::directory_iterator(shaderRootDir)) {
			if (entry.is_regular_file()) {
				const std::string ext = entry.path().extension().string();
				if (shaderExtensions.contains(ext))
					shader_paths.push_back(entry.path().string());
			}
		}
	} catch (const fs::filesystem_error &e) {
		std::cerr << "Failed to read shader directory: " << e.what() << "\n";
		std::exit(1);
	}

	if (shader_paths.empty()) {
		std::cerr << "No shader files found in directory: " << shaderRootDir << "\n";
		std::exit(1);
	}

	auto bins = compileShader(shader_paths);
#else
	// Android: try known stages using the base id
	std::vector<std::string> shader_paths;
	auto add = [&](const char *ext) { shader_paths.emplace_back(shaderRootDir + std::string(ext)); };
	add(".vert");
	add(".tesc");
	add(".tese");
	add(".geom");
	add(".frag");
	add(".comp");
	auto bins = compileShader(shader_paths);
#endif

	auto mk = [&](const std::vector<uint32_t> &bin) -> VkShaderModule { return bin.empty() ? VK_NULL_HANDLE : createShaderModule(bin); };
	modules.vertexShader = mk(bins.vertexShader);
	modules.tessellationControlShader = mk(bins.tessellationControlShader);
	modules.tessellationEvaluationShader = mk(bins.tessellationEvaluationShader);
	modules.geometryShader = mk(bins.geometryShader);
	modules.fragmentShader = mk(bins.fragmentShader);
	modules.computeShader = mk(bins.computeShader);

	return modules;
}

// Desktop init (keeps behavior)
inline void initialize() {
#if !ANDROID_VK
	ensureDir(shaderRootPath); // contains GLSL
	ensureDir(textureRootPath);
	ensureDir(modelRootPath);
	ensureDir(fontRootPath);
#endif
	ensureDir(shaderCachePath); // desktop: assets/spirv ; android: <files>/shaders (set at runtime)
}

} // namespace Assets
