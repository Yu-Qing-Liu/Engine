cmake_minimum_required(VERSION 3.15)
project(Engine LANGUAGES CXX)

add_definitions(-DPROJECT_ROOT_DIR=\"${CMAKE_SOURCE_DIR}\")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(SHADERC REQUIRED shaderc)

find_package(Vulkan REQUIRED)
find_package(glm REQUIRED)
find_package(glfw3 REQUIRED)
find_package(OpenSSL REQUIRED)

include_directories(
  include
  include/utils
  include/models
  include/models/implementations
  include/scenes
  include/scenes/implementations
  ${SHADERC_INCLUDE_DIRS}
)

file(GLOB UTILS ${CMAKE_CURRENT_SOURCE_DIR}/src/utils/*.cpp)
file(GLOB MODELS ${CMAKE_CURRENT_SOURCE_DIR}/src/models/*.cpp)
file(GLOB MODELS_IMPL ${CMAKE_CURRENT_SOURCE_DIR}/src/models/implementations/*.cpp)
file(GLOB SCENES ${CMAKE_CURRENT_SOURCE_DIR}/src/scenes/*.cpp)
file(GLOB SCENES_IMPL ${CMAKE_CURRENT_SOURCE_DIR}/src/scenes/implementations/*.cpp)
add_library(SOURCES ${UTILS} ${MODELS} ${MODELS_IMPL} ${SCENES} ${SCENES_IMPL})

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin")
add_executable(${PROJECT_NAME}
  ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
)
target_link_libraries(${PROJECT_NAME} PRIVATE
  SOURCES
  Vulkan::Vulkan
  glm
  glfw
  OpenSSL::Crypto
  ${SHADERC_LIBRARIES}
)
