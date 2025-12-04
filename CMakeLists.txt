cmake_minimum_required(VERSION 3.15)
project(Engine LANGUAGES C CXX)

# --- Build settings ---
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_compile_definitions(PROJECT_ROOT_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
add_definitions(-DNOMINMAX)

# --- vcpkg toolchain fallback ---
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  set(CMAKE_TOOLCHAIN_FILE
      "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake"
      CACHE FILEPATH "Vcpkg toolchain file"
  )
endif()

if(POLICY CMP0167)
  cmake_policy(SET CMP0167 NEW)
endif()

# --- Dependencies ---
if (DEFINED ENV{IN_NIX_SHELL})
find_package(PkgConfig REQUIRED)
pkg_check_modules(SHADERC REQUIRED shaderc)
find_package(Vulkan REQUIRED)
find_package(glm REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(assimp REQUIRED)
find_package(Freetype REQUIRED)
find_package(imgui REQUIRED)
find_package(CycloneDDS REQUIRED)
find_package(lunasvg REQUIRED)
find_package(Boost REQUIRED COMPONENTS graph)
else()
find_package(unofficial-shaderc CONFIG REQUIRED)
find_package(Vulkan REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(assimp CONFIG REQUIRED)
find_package(Freetype REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(CycloneDDS CONFIG REQUIRED)
find_package(lunasvg CONFIG REQUIRED)
find_package(Boost CONFIG REQUIRED COMPONENTS graph)
endif()

# --- Sources ---
file(GLOB sources
  ${CMAKE_CURRENT_SOURCE_DIR}/src/application/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/backend/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/vulkan/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/models/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/models/implementations/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/models/implementations/shaderquads/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/scenes/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/scenes/implementations/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/widgets/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/widgets/implementations/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/pipelines/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/pipelines/compute/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/pipelines/graphics/*.cpp
)

add_library(libraries ${sources})
add_library(sqlite3 STATIC ${CMAKE_SOURCE_DIR}/include/thirdparty/sqlite3/sqlite3.c)

target_include_directories(sqlite3 PUBLIC
  ${CMAKE_SOURCE_DIR}/include/thirdparty/sqlite3
)

target_include_directories(libraries PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include/application
  ${CMAKE_CURRENT_SOURCE_DIR}/include/backend
  ${CMAKE_CURRENT_SOURCE_DIR}/include/thirdparty/stb_image
  ${CMAKE_CURRENT_SOURCE_DIR}/include/utils
  ${CMAKE_CURRENT_SOURCE_DIR}/include/persistence
  ${CMAKE_CURRENT_SOURCE_DIR}/include/vulkan
  ${CMAKE_CURRENT_SOURCE_DIR}/include/models
  ${CMAKE_CURRENT_SOURCE_DIR}/include/models/implementations
  ${CMAKE_CURRENT_SOURCE_DIR}/include/models/implementations/shaderquads
  ${CMAKE_CURRENT_SOURCE_DIR}/include/scenes
  ${CMAKE_CURRENT_SOURCE_DIR}/include/scenes/implementations
  ${CMAKE_CURRENT_SOURCE_DIR}/include/widgets
  ${CMAKE_CURRENT_SOURCE_DIR}/include/widgets/implementations
  ${CMAKE_CURRENT_SOURCE_DIR}/include/pipelines
  ${CMAKE_CURRENT_SOURCE_DIR}/include/pipelines/compute
  ${CMAKE_CURRENT_SOURCE_DIR}/include/pipelines/graphics
)

if (DEFINED ENV{IN_NIX_SHELL})
add_library(imguivulkan STATIC ${CMAKE_SOURCE_DIR}/include/thirdparty/imgui/imgui_impl_vulkan.cpp)
target_include_directories(imguivulkan PUBLIC
  ${CMAKE_SOURCE_DIR}/include/thirdparty/imgui
)
target_include_directories(libraries PUBLIC
  ${SHADERC_INCLUDE_DIRS}
)
target_link_libraries(libraries PUBLIC
  # shaderc
  ${SHADERC_LIBRARIES}

  # vulkan runtime
  Vulkan::Vulkan

  # math / window / IO
  glm::glm
  glfw

  # assets / text
  assimp
  Freetype::Freetype
  imgui
  imguivulkan

  # crypto / tls
  OpenSSL::SSL
  OpenSSL::Crypto

  # CycloneDDS
  CycloneDDS::ddsc

  # LunaSVG
  lunasvg::lunasvg

  # Boost graph
  Boost::graph
)
else()
target_link_libraries(libraries PUBLIC
  # shaderc
  unofficial::shaderc::shaderc

  # vulkan runtime
  Vulkan::Vulkan

  # math / window / IO
  glm::glm
  glfw

  # assets / text
  assimp::assimp
  Freetype::Freetype
  imgui::imgui

  # crypto / tls
  OpenSSL::SSL
  OpenSSL::Crypto

  # CycloneDDS
  CycloneDDS::ddsc

  # LunaSVG
  lunasvg::lunasvg

  # Boost graph
  Boost::graph
)
endif()

add_executable(${PROJECT_NAME}
  ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
)

target_link_libraries(${PROJECT_NAME} PRIVATE
  libraries
)
