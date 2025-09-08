#pragma once

#include "engine.hpp"
#include "events.hpp"
#include <atomic>
#include <fstream>
#include <functional>
#include <thread>

#if defined(__ANDROID__)
#define ANDROID_VK 1
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <android/log.h>
#include <android/native_activity.h>
#include <android_native_app_glue.h>

#include <sys/stat.h>
using ::mkdir;
#include <assimp/IOSystem.hpp>
#include <assimp/IOStream.hpp>

extern android_app *g_app;
inline ANativeWindow *NativeWin() { return g_app ? g_app->window : nullptr; }
#ifndef LOG_TAG
#define LOG_TAG "Engine"
#endif
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#else
#define ANDROID_VK 0
#endif

namespace Platform {

// Returns the current pointer location in *framebuffer* pixels (HiDPI-aware).
inline void GetPointerInFramebufferPixels(float &outX, float &outY) {
#if ANDROID_VK
	// Window size in "window pixels"
	int ww = 0, wh = 0;
	if (auto *win = NativeWin()) {
		ww = ANativeWindow_getWidth(win);
		wh = ANativeWindow_getHeight(win);
	}
	// Framebuffer size == swapchain extent on Android
	const uint32_t fbw = Engine::swapChainExtent.width;
	const uint32_t fbh = Engine::swapChainExtent.height;

	const float sx = ww ? (float)fbw / (float)ww : 1.0f;
	const float sy = wh ? (float)fbh / (float)wh : 1.0f;

	outX = Events::pointerX * sx;
	outY = Events::pointerY * sy;
#else
	// Desktop: read live cursor, convert window->framebuffer pixels
	double cx = 0.0, cy = 0.0;
	glfwGetCursorPos(Engine::window, &cx, &cy);

	int fbw = 0, fbh = 0, ww = 0, wh = 0;
	glfwGetFramebufferSize(Engine::window, &fbw, &fbh);
	glfwGetWindowSize(Engine::window, &ww, &wh);

	const float sx = ww ? (float)fbw / (float)ww : 1.0f;
	const float sy = wh ? (float)fbh / (float)wh : 1.0f;

	outX = (float)cx * sx;
	outY = (float)cy * sy;
#endif
}

// jthread
#if defined(__cpp_lib_jthread) && !defined(ANDROID)
using jthread = std::jthread;
using stop_token = std::stop_token;
#else
struct stop_token {
	std::shared_ptr<std::atomic<bool>> flag{};
	bool stop_requested() const noexcept { return flag && flag->load(std::memory_order_relaxed); }
};

class jthread {
	std::thread th_;
	std::shared_ptr<std::atomic<bool>> stop_ = std::make_shared<std::atomic<bool>>(false);

  public:
	jthread() = default;

	// start immediately
	template <class F, class... Args> explicit jthread(F &&f, Args &&...args) { start(std::forward<F>(f), std::forward<Args>(args)...); }

	// non-copyable
	jthread(const jthread &) = delete;
	jthread &operator=(const jthread &) = delete;

	// movable
	jthread(jthread &&) noexcept = default;
	jthread &operator=(jthread &&rhs) noexcept {
		if (this != &rhs) {
			request_stop();
			join();
			th_ = std::move(rhs.th_);
			stop_ = std::move(rhs.stop_);
		}
		return *this;
	}

	// (re)start the worker
	template <class F, class... Args> void start(F &&f, Args &&...args) {
		request_stop();
		join();
		stop_->store(false, std::memory_order_relaxed);
		th_ = std::thread([tok = stop_token{stop_}, fn = std::forward<F>(f)](auto... inner) mutable { fn(tok, std::forward<decltype(inner)>(inner)...); }, std::forward<Args>(args)...);
	}

	void request_stop() noexcept {
		if (stop_)
			stop_->store(true, std::memory_order_relaxed);
	}

	bool joinable() const noexcept { return th_.joinable(); }
	void join() noexcept {
		if (th_.joinable())
			th_.join();
	}

	~jthread() {
		request_stop();
		join();
	}
};
#endif

#if ANDROID_VK
class AAssetIOStream : public Assimp::IOStream {
public:
    AAssetIOStream(AAsset* asset) : asset_(asset), pos_(0) {}
    ~AAssetIOStream() override { if (asset_) AAsset_close(asset_); }

    size_t Read(void* pvBuffer, size_t pSize, size_t pCount) override {
        const size_t want = pSize * pCount;
        const int rd = AAsset_read(asset_, pvBuffer, want);
        if (rd > 0) pos_ += rd;
        return rd < 0 ? 0 : size_t(rd) / pSize; // return “items” read
    }

    size_t Write(const void*, size_t, size_t) override { return 0; }
    aiReturn Seek(size_t pOffset, aiOrigin pOrigin) override {
        off_t base = 0;
        if (pOrigin == aiOrigin_SET) base = 0;
        else if (pOrigin == aiOrigin_CUR) base = pos_;
        else /*END*/ base = AAsset_getLength(asset_);
        off_t target = base + off_t(pOffset);
        off_t cur = AAsset_seek(asset_, target, SEEK_SET);
        if (cur < 0) return aiReturn_FAILURE;
        pos_ = size_t(cur);
        return aiReturn_SUCCESS;
    }

    size_t Tell() const override { return pos_; }
    size_t FileSize() const override { return size_t(AAsset_getLength(asset_)); }
    void Flush() override {}

private:
    AAsset* asset_;
    size_t  pos_;
};

class AAssetIOSystem : public Assimp::IOSystem {
public:
    AAssetIOSystem(AAssetManager* mgr, std::string base = {}) : mgr_(mgr), base_(std::move(base)) {}
    ~AAssetIOSystem() override = default;

    // Assimp uses this to join relative paths for .mtl/.png, etc.
    bool Exists(const char* pFile) const override {
        auto full = join_(pFile);
        if (AAsset* a = AAssetManager_open(mgr_, full.c_str(), AASSET_MODE_STREAMING)) {
            AAsset_close(a);
            return true;
        }
        return false;
    }

    char getOsSeparator() const override { return '/'; }

    Assimp::IOStream* Open(const char* pFile, const char* /*pMode*/ = "rb") override {
        auto full = join_(pFile);
        if (AAsset* a = AAssetManager_open(mgr_, full.c_str(), AASSET_MODE_STREAMING)) {
            return new AAssetIOStream(a);
        }
        return nullptr;
    }

    void Close(Assimp::IOStream* pFile) override { delete pFile; }

private:
    std::string join_(const std::string& rel) const {
        if (rel.empty()) return base_;
        if (rel.front() == '/') return rel.substr(1);
        if (base_.empty()) return rel;
        return base_.back() == '/' ? (base_ + rel) : (base_ + "/" + rel);
    }

    AAssetManager* mgr_;
    std::string    base_; // e.g., "meshes" or "" if your paths are already asset-relative
};
#endif

} // namespace Platform
