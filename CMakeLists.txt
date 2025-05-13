cmake_minimum_required(VERSION 3.15)
project(Engine LANGUAGES CXX)

add_definitions(-DPROJECT_ROOT_DIR=\"${CMAKE_SOURCE_DIR}\")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(PkgConfig REQUIRED)

find_package(Vulkan REQUIRED)
find_package(glm REQUIRED)
find_package(glfw3 REQUIRED)

include_directories(
  include
  include/utils
)

file(GLOB UTILS ${CMAKE_CURRENT_SOURCE_DIR}/src/utils/*.cpp)
add_library(UTILS ${UTILS})

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin")
add_executable(${PROJECT_NAME}
  ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
)
target_link_libraries(${PROJECT_NAME} PRIVATE
  UTILS
  Vulkan::Vulkan
  glm
  glfw
)
