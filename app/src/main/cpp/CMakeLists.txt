cmake_minimum_required(VERSION 3.22)
project(engine_android LANGUAGES C CXX)

add_compile_definitions(ANDROID_VK=1 VK_USE_PLATFORM_ANDROID_KHR)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_ANDROID_STL_TYPE c++_shared)

# Cross-compile safety: never run test executables
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# Produce PIC for all static libs we build (assimp/freetype)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Build static third-party libs so APK only contains libengine.so
set(BUILD_SHARED_LIBS OFF)

# Repo root: from app/src/main/cpp -> up to project root (4 levels)
get_filename_component(REPO_ROOT "${CMAKE_SOURCE_DIR}/../../../.." ABSOLUTE)

# ------------------------------
# Your engine sources
# ------------------------------
file(GLOB ENGINE_MODELS        "${REPO_ROOT}/src/models/*.cpp")
file(GLOB ENGINE_MODELS_IMPL   "${REPO_ROOT}/src/models/implementations/*.cpp")
file(GLOB ENGINE_SCENES        "${REPO_ROOT}/src/scenes/*.cpp")
file(GLOB ENGINE_SCENES_IMPL   "${REPO_ROOT}/src/scenes/implementations/*.cpp")
file(GLOB ENGINE_UI            "${REPO_ROOT}/src/ui/*.cpp")
file(GLOB ENGINE_UI_IMPL       "${REPO_ROOT}/src/ui/implementations/*.cpp")

add_library(SOURCES STATIC
        ${ENGINE_MODELS}
        ${ENGINE_MODELS_IMPL}
        ${ENGINE_SCENES}
        ${ENGINE_SCENES_IMPL}
        ${ENGINE_UI}
        ${ENGINE_UI_IMPL}
)

target_include_directories(SOURCES PUBLIC
        "${REPO_ROOT}/include"
        "${REPO_ROOT}/include/utils"
        "${REPO_ROOT}/include/vulkan"
        "${REPO_ROOT}/include/models"
        "${REPO_ROOT}/include/models/implementations"
        "${REPO_ROOT}/include/scenes"
        "${REPO_ROOT}/include/scenes/implementations"
        "${REPO_ROOT}/include/ui"
        "${REPO_ROOT}/include/ui/implementations"
)

# Used by your assets.hpp
target_compile_definitions(SOURCES PUBLIC PROJECT_ROOT_DIR="${REPO_ROOT}")

# ------------------------------
# Third_party: FreeType
# Keep it tiny: no PNG/BZip2/Brotli/Harfbuzz/zlib (optional)
# ------------------------------
set(THIRD_PARTY_DIR "${CMAKE_SOURCE_DIR}/third_party")

set(FT_DISABLE_PNG      ON CACHE BOOL "" FORCE)
set(FT_DISABLE_BZIP2    ON CACHE BOOL "" FORCE)
set(FT_DISABLE_BROTLI   ON CACHE BOOL "" FORCE)
set(FT_DISABLE_HARFBUZZ ON CACHE BOOL "" FORCE)
# Either disable zlib or let it auto-detect NDK zlib. Disabling is simplest:
set(FT_DISABLE_ZLIB     ON CACHE BOOL "" FORCE)
# (Older FT uses FT_REQUIRE_ZLIB; this keeps it from erroring if OFF)
set(FT_REQUIRE_ZLIB     OFF CACHE BOOL "" FORCE)

add_subdirectory("${THIRD_PARTY_DIR}/freetype" EXCLUDE_FROM_ALL)

# Freetype's target is usually "freetype" (and an alias "Freetype::Freetype" in newer versions)
# Link PUBLIC to SOURCES so headers are visible when compiling SOURCES.
if(TARGET Freetype::Freetype)
    target_link_libraries(SOURCES PUBLIC Freetype::Freetype)
else()
    target_link_libraries(SOURCES PUBLIC freetype)
endif()

# ------------------------------
# Third_party: Assimp
# Keep it tiny & import-only (OBJ). Use bundled zlib to avoid surprises.
# ------------------------------
set(ASSIMP_BUILD_TESTS                   OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ASSIMP_TOOLS            OFF CACHE BOOL "" FORCE)
set(ASSIMP_INSTALL                       OFF CACHE BOOL "" FORCE)
set(ASSIMP_NO_EXPORT                     ON  CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ALL_IMPORTERS_BY_DEFAULT OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_OBJ_IMPORTER            ON  CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ZLIB                    ON  CACHE BOOL "" FORCE) # uses bundled zlib (C)
# Avoid treating warnings as errors (common on Android)
set(ASSIMP_WARNINGS_AS_ERRORS            OFF CACHE BOOL "" FORCE)

add_subdirectory("${THIRD_PARTY_DIR}/assimp" EXCLUDE_FROM_ALL)

# Assimp's target name is "assimp"
target_link_libraries(SOURCES PUBLIC assimp)

# ------------------------------
# Third_party: GLM (header-only)
# ------------------------------
set(GLM_TEST_ENABLE OFF CACHE BOOL "" FORCE)          # don't build GLM tests
add_subdirectory("${THIRD_PARTY_DIR}/glm" EXCLUDE_FROM_ALL)

# Use the imported target if present, else just add include dir
if (TARGET glm::glm)
    target_link_libraries(SOURCES PUBLIC glm::glm)
else()
    target_include_directories(SOURCES PUBLIC "${THIRD_PARTY_DIR}/glm")
endif()

# ------------------------------
# NDK system libs
# ------------------------------
find_library(ANDROID_LOG log REQUIRED)
find_library(ANDROID_NATIVE android REQUIRED)
find_library(VULKAN_LIB vulkan REQUIRED)
find_library(CPP_SHARED c++_shared) # STL

# NativeActivity glue (C source)
add_library(android_native_app_glue STATIC
        ${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c
)
target_include_directories(android_native_app_glue INTERFACE
        ${ANDROID_NDK}/sources/android/native_app_glue
)
target_include_directories(SOURCES PUBLIC
        "${ANDROID_NDK}/sources/android/native_app_glue"
)
set_target_properties(android_native_app_glue PROPERTIES LINKER_LANGUAGE C)

# ------------------------------
# The shared library loaded by NativeActivity
# ------------------------------
add_library(engine SHARED
        ${CMAKE_SOURCE_DIR}/android_main.cpp
)
set_target_properties(engine PROPERTIES
        OUTPUT_NAME engine
        DEBUG_POSTFIX ""     # remove the default 'd' suffix in Debug
)
target_include_directories(engine PRIVATE
        ${ANDROID_NDK}/sources/android/native_app_glue
)

# Tell your code that this is the Android Vulkan path
target_compile_definitions(engine PRIVATE ANDROID_VK=1 VK_USE_PLATFORM_ANDROID_KHR)

# Link order: your code + third-party + system
target_link_libraries(engine PRIVATE
        SOURCES
        android_native_app_glue
        ${VULKAN_LIB}
        ${ANDROID_NATIVE}
        ${ANDROID_LOG}
        ${CPP_SHARED}
)
