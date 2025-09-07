#pragma once

#if defined(__ANDROID__)
#define ANDROID_VK 1
#include <android/asset_manager.h>
#include <android/log.h>
#include <android/native_activity.h>
#ifndef LOG_TAG
#define LOG_TAG "Engine"
#endif
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#else
#define ANDROID_VK 0
#endif
