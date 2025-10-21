#pragma once

#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
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
#include <android/log.h>
#include <android_native_app_glue.h>
#endif

#include <vulkan/vulkan_core.h>

#include "engine.hpp"
#include "platform.hpp"

namespace Assets {

namespace fs = std::filesystem;
using namespace Engine;

#if ANDROID_VK
#define LOG_TAG_ASSETS "Assets"
#define ALOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG_ASSETS, __VA_ARGS__)
#define ALOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG_ASSETS, __VA_ARGS__)
#define ALOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG_ASSETS, __VA_ARGS__)
inline bool endsWith(const std::string &s, const std::string &suffix) {
	if (s.size() < suffix.size())
		return false;
	return std::equal(suffix.rbegin(), suffix.rend(), s.rbegin());
}
#endif

// ===================== Centralized repo paths =====================
#if ANDROID_VK
// Android: these point at project assets; runtime will remap/hydrate to <files>/...
inline std::string shaderRootPath = std::string(PROJECT_ROOT_DIR) + "/app/src/main/assets/shaders";
inline std::string textureRootPath = std::string(PROJECT_ROOT_DIR) + "/app/src/main/assets/textures";
inline std::string modelRootPath = std::string(PROJECT_ROOT_DIR) + "/app/src/main/assets/meshes";
inline std::string fontRootPath = std::string(PROJECT_ROOT_DIR) + "/app/src/main/assets/fonts";
inline std::string shaderCachePath; // set at runtime to "<files>/shaders"
#else
inline std::string shaderRootPath = std::string(PROJECT_ROOT_DIR) + "/app/src/main/assets/shaders";
inline std::string textureRootPath = std::string(PROJECT_ROOT_DIR) + "/app/src/main/assets/textures";
inline std::string modelRootPath = std::string(PROJECT_ROOT_DIR) + "/app/src/main/assets/meshes";
inline std::string fontRootPath = std::string(PROJECT_ROOT_DIR) + "/app/src/main/assets/fonts";
inline std::string shaderCachePath = std::string(PROJECT_ROOT_DIR) + "/app/src/main/assets/spirv";
#endif

inline std::string appdataPath = std::string(PROJECT_ROOT_DIR) + "/app/src/main/appdata";

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

inline std::string texturePath(const std::string &rel) { return joinPath(textureRootPath, rel); }
inline std::string meshPath(const std::string &rel) { return joinPath(modelRootPath, rel); }
inline std::string fontPath(const std::string &rel) { return joinPath(fontRootPath, rel); }

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
	static const char *roots[] = {"/meshes/", "/textures/", "/fonts/", "/spirv/", "/shaders/", "meshes/", "textures/", "fonts/", "spirv/", "shaders/"};
	for (auto r : roots) {
		if (auto pos = p.find(r); pos != std::string::npos) {
			return (r[0] == '/') ? p.substr(pos + 1) : p.substr(pos);
		}
	}
	if (!p.empty() && p.front() != '/')
		return p;
	return {};
}

inline std::vector<uint8_t> loadBytes(const std::string &absOrRel) {
	// 1) Filesystem
	auto fsBytes = readAllBytes(absOrRel);
	if (!fsBytes.empty())
		return fsBytes;

#if ANDROID_VK
	// 2) APK assets fallback
	if (!g_app || !g_app->activity || !g_app->activity->assetManager) {
		ALOGE("LoadBytes: AssetManager not available");
		return {};
	}
	std::string rel = toAssetRel(absOrRel);
	if (rel.empty()) {
		ALOGE("LoadBytes: cannot map to asset-relative path: %s", absOrRel.c_str());
		return {};
	}

	AAsset *a = AAssetManager_open(g_app->activity->assetManager, rel.c_str(), AASSET_MODE_STREAMING);
	if (!a) {
		ALOGE("LoadBytes: AAssetManager_open failed for %s", rel.c_str());
		return {};
	}
	const off_t len = AAsset_getLength(a);
	std::vector<uint8_t> out;
	out.resize(static_cast<size_t>(len));
	int rd = AAsset_read(a, out.data(), static_cast<size_t>(len));
	AAsset_close(a);
	if (rd <= 0) {
		ALOGE("LoadBytes: read failed for %s", rel.c_str());
		return {};
	}
	out.resize(static_cast<size_t>(rd));
	return out;
#else
	return {};
#endif
}

// ===================== Desktop: compile & cache =====================
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
		ss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
	return ss.str();
}
inline void writeBinaryFile(const std::string &path, const std::vector<uint32_t> &data) {
	std::error_code ec;
	fs::create_directories(fs::path(path).parent_path(), ec);
	std::ofstream file(path, std::ios::binary);
	if (!file.is_open()) {
		std::cerr << "Failed to write: " << path << std::endl;
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
	auto it = shaderExtensions.find(ext);
	if (it == shaderExtensions.end()) {
		std::cerr << "Unsupported shader ext: " << ext << "\n";
		return {};
	}

	const std::string hash_str = computeHash(ext + shaderCode);
	const std::string basenameSpv = p.filename().string() + ".spv";
	const fs::path cache_dir(shaderCachePath);
	const fs::path cached_path = cache_dir / (hash_str + "--" + basenameSpv);

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
	writeBinaryFile(cached_path.string(), spirv);
	return spirv;
}

#else
// ===================== Android: hydrate <files> then select/scan =====================

// Remove a path if it's a directory (leftover from earlier buggy copies)
inline void removeIfDirectory(const std::string &path) {
	std::error_code ec;
	if (fs::is_directory(path, ec)) {
		ALOGW("removeIfDirectory: removing stale directory at file path %s", path.c_str());
		fs::remove_all(path, ec);
		if (ec) {
			ALOGE("removeIfDirectory: remove_all failed for %s (%d)", path.c_str(), (int)ec.value());
		}
	}
}

// Write file, healing old bad dirs that block fopen
inline bool writeWholeFile(const std::string &path, const void *data, size_t size) {
	ensureDir(fs::path(path).parent_path().string());
	removeIfDirectory(path); // heal: if a dir exists where a file should be, delete it
	FILE *f = fopen(path.c_str(), "wb");
	if (!f) {
		ALOGE("writeWholeFile: fopen failed for %s", path.c_str());
		return false;
	}
	size_t w = fwrite(data, 1, size, f);
	fclose(f);
	if (w != size) {
		ALOGE("writeWholeFile: short write for %s (%zu/%zu)", path.c_str(), w, size);
		return false;
	}
	return true;
}

// Set <files>/... roots
inline void setResourceDirectories(android_app *app, bool preferExternal = true) {
	const char *ext = app->activity->externalDataPath;
	const char *in = app->activity->internalDataPath;
	const char *base = (preferExternal && ext && *ext) ? ext : in;

	shaderCachePath = joinPath(base, "shaders"); // runtime SPIR-V location
	modelRootPath = joinPath(base, "meshes");
	fontRootPath = joinPath(base, "fonts");
	textureRootPath = joinPath(base, "textures");

	ensureDir(shaderCachePath);
	ensureDir(modelRootPath);
	ensureDir(fontRootPath);
	ensureDir(textureRootPath);

	ALOGI("Resource dirs: shaders=%s meshes=%s fonts=%s textures=%s", shaderCachePath.c_str(), modelRootPath.c_str(), fontRootPath.c_str(), textureRootPath.c_str());
}

// Recursively copy an APK "directory" to filesystem (file-first).
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
			const std::string childRel = sub.empty() ? name : (sub + "/" + name);

			// FILE FIRST: try opening as file
			if (AAsset *a = AAssetManager_open(mgr, childRel.c_str(), AASSET_MODE_STREAMING)) {
				const off_t len = AAsset_getLength(a);
				std::vector<uint8_t> buf((size_t)len);
				int rd = AAsset_read(a, buf.data(), (size_t)len);
				AAsset_close(a);
				if (rd > 0) {
					buf.resize((size_t)rd);
					const std::string outPath = joinPath(out, name);
					// heal stale dir if present
					writeWholeFile(outPath, buf.data(), buf.size());
				} else {
					ALOGW("copyAssetDir: read failed for %s", childRel.c_str());
				}
				continue;
			}

			// Otherwise treat as a subdirectory and recurse
			const std::string outSub = joinPath(out, name);
			ensureDir(outSub);
			rec(childRel, outSub);
		}
		AAssetDir_close(dir);
	};

	rec(srcSubdir, dstDir);
}

inline void initializeAndroid(android_app *app) {
	// Prefer internal on emulator to avoid storage issues
	setResourceDirectories(app, /*preferExternal=*/false);

	// Hydrate runtime copies (now file-first; won't create dirs named like files)
	copyAssetDir(app, "spirv", shaderCachePath);
	copyAssetDir(app, "meshes", modelRootPath);
	copyAssetDir(app, "textures", textureRootPath);
	copyAssetDir(app, "fonts", fontRootPath);
}

// Android load: pick exact "<files>/shaders/<basename>.spv" OR newest "*<basename>.spv"
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
		if (endsWith(name, basenameSpv)) {
			auto t = fs::last_write_time(entry.path(), ec);
			if (!ec && (!best || t > bestTime)) {
				best = entry.path();
				bestTime = t;
			}
		}
	}
	return best;
}

// Scan APK assets/spirv for "*--<basename>.spv" and return raw bytes
inline std::vector<uint8_t> loadSpvFromApkBySuffix(const std::string &basenameSpv) {
	if (!g_app || !g_app->activity || !g_app->activity->assetManager)
		return {};
	AAssetManager *mgr = g_app->activity->assetManager;

	AAssetDir *dir = AAssetManager_openDir(mgr, "spirv");
	if (!dir)
		return {};

	std::vector<uint8_t> out;
	const char *name = nullptr;
	while ((name = AAssetDir_getNextFileName(dir)) != nullptr) {
		// Names look like "<hash>--<basename>.spv"; we suffix match.
		if (!endsWith(name, basenameSpv))
			continue;

		std::string rel = std::string("spirv/") + name;
		AAsset *a = AAssetManager_open(mgr, rel.c_str(), AASSET_MODE_STREAMING);
		if (!a)
			continue;

		const off_t len = AAsset_getLength(a);
		out.resize((size_t)len);
		int rd = AAsset_read(a, out.data(), (size_t)len);
		AAsset_close(a);
		if (rd > 0) {
			out.resize((size_t)rd);
			break;
		} else {
			out.clear();
		}
	}
	AAssetDir_close(dir);
	return out;
}

inline std::vector<uint32_t> compileShader(const std::string &shaderPath /*logical*/) {
	fs::path p(shaderPath);
	const std::string basenameSpv = p.filename().string() + ".spv"; // e.g. "raytracing.comp.spv"
	const fs::path cache_dir(shaderCachePath);

	// 1) Prefer hydrated cache in <files>/shaders: exact or suffix match "*<basename>.spv"
	if (auto chosen = selectCachedBinaryPath(cache_dir, basenameSpv)) {
		return readBinaryFileU32(chosen->string());
	}

	// 2) Fallback: search inside APK assets/spirv for any "<hash>--<basename>.spv"
	auto bytes = loadSpvFromApkBySuffix(basenameSpv);
	if (!bytes.empty() && (bytes.size() % 4 == 0)) {
		// Seed the cache with a non-hashed name for quick exact hits
		const std::string cache_path = (cache_dir / basenameSpv).string();
		writeWholeFile(cache_path, bytes.data(), bytes.size());

		std::vector<uint32_t> out(bytes.size() / 4);
		std::memcpy(out.data(), bytes.data(), bytes.size());
		return out;
	}

	return {};
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

inline ShaderModules compileShaderProgram(const std::string &shaderRootDir) {
	ShaderModules modules;

#if !ANDROID_VK
	std::vector<std::string> shader_paths;
	for (const auto &entry : fs::directory_iterator(shaderRootDir)) {
		if (entry.is_regular_file()) {
			const std::string ext = entry.path().extension().string();
			if (shaderExtensions.contains(ext))
				shader_paths.push_back(entry.path().string());
		}
	}
	auto bins = compileShader(shader_paths);
#else
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

// ---------- helpers: executable dir & directory copy ----------
#if !ANDROID_VK
inline std::string executableDir() {
#if defined(_WIN32)
	char buf[MAX_PATH];
	DWORD len = GetModuleFileNameA(nullptr, buf, MAX_PATH);
	std::string p(buf, (len ? len : 0));
	auto pos = p.find_last_of("\\/");
	return (pos == std::string::npos) ? std::string(".") : p.substr(0, pos);
#elif defined(__APPLE__)
	char buf[4096];
	uint32_t size = sizeof(buf);
	std::string p;
	if (_NSGetExecutablePath(buf, &size) == 0) {
		p.assign(buf);
	} else {
		std::vector<char> dyn(size);
		if (_NSGetExecutablePath(dyn.data(), &size) == 0)
			p.assign(dyn.data());
	}
	auto pos = p.find_last_of('/');
	return (pos == std::string::npos) ? std::string(".") : p.substr(0, pos);
#else // Linux / BSD
	char buf[4096];
	ssize_t n = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
	if (n <= 0)
		return ".";
	buf[n] = '\0';
	std::string p(buf);
	auto pos = p.find_last_of('/');
	return (pos == std::string::npos) ? std::string(".") : p.substr(0, pos);
#endif
}

inline void copyDirRecursive(const fs::path &src, const fs::path &dst) {
	std::error_code ec;

	if (!fs::exists(src, ec) || !fs::is_directory(src, ec))
		return;

	fs::create_directories(dst, ec);

	for (auto it = fs::recursive_directory_iterator(src, ec); it != fs::recursive_directory_iterator(); ++it) {
		if (ec)
			break;

		const fs::path rel = fs::relative(it->path(), src, ec);
		const fs::path out = dst / rel;

		if (it->is_directory(ec)) {
			fs::create_directories(out, ec);
		} else if (it->is_regular_file(ec)) {
			// Only copy if missing or source is newer
			bool doCopy = true;
			if (fs::exists(out, ec)) {
				auto tSrc = fs::last_write_time(it->path(), ec);
				auto tDst = fs::last_write_time(out, ec);
				if (!ec && tDst >= tSrc)
					doCopy = false;
			}
			if (doCopy) {
				fs::create_directories(out.parent_path(), ec);
				fs::copy_file(it->path(), out, fs::copy_options::overwrite_existing, ec);
			}
		}
	}
}
#endif // !ANDROID_VK

inline void initialize() {
#if !ANDROID_VK
	// Always ensure the configured roots exist
	ensureDir(shaderRootPath);
	ensureDir(textureRootPath);
	ensureDir(modelRootPath);
	ensureDir(fontRootPath);
    ensureDir(appdataPath);

	const fs::path binDir = executableDir();
	const fs::path outRoot = binDir / "assets";

	const fs::path dstShaders = outRoot / "shaders";
	const fs::path dstTextures = outRoot / "textures";
	const fs::path dstMeshes = outRoot / "meshes";
	const fs::path dstFonts = outRoot / "fonts";
	const fs::path dstSpirv = outRoot / "spirv";
    const fs::path dstAppdata = binDir / "appdata";

	copyDirRecursive(shaderRootPath, dstShaders);
	copyDirRecursive(textureRootPath, dstTextures);
	copyDirRecursive(modelRootPath, dstMeshes);
	copyDirRecursive(fontRootPath, dstFonts);
	copyDirRecursive(shaderCachePath, dstSpirv);
    copyDirRecursive(appdataPath, dstAppdata);

    shaderRootPath = "./assets/shaders";
    textureRootPath = "./assets/textures";
    modelRootPath = "./assets/meshes";
    fontRootPath = "./assets/fonts";
    shaderCachePath = "./assets/spirv";
    appdataPath = "./appdata";
#endif // !ANDROID_VK
	ensureDir(shaderCachePath);
}

} // namespace Assets
