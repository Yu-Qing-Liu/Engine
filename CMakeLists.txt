cmake_minimum_required(VERSION 3.15)
project(Engine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin")

add_compile_definitions(PROJECT_ROOT_DIR="${CMAKE_SOURCE_DIR}")

if(WIN32)
    find_package(unofficial-shaderc CONFIG REQUIRED)
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SHADERC REQUIRED shaderc)
endif()

find_package(Vulkan REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(assimp CONFIG REQUIRED)

file(GLOB MODELS       ${CMAKE_CURRENT_SOURCE_DIR}/src/models/*.cpp)
file(GLOB MODELS_IMPL  ${CMAKE_CURRENT_SOURCE_DIR}/src/models/implementations/*.cpp)
file(GLOB SCENES       ${CMAKE_CURRENT_SOURCE_DIR}/src/scenes/*.cpp)
file(GLOB SCENES_IMPL  ${CMAKE_CURRENT_SOURCE_DIR}/src/scenes/implementations/*.cpp)

add_library(SOURCES ${MODELS} ${MODELS_IMPL} ${SCENES} ${SCENES_IMPL})
add_executable(${PROJECT_NAME} ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp)

if(WIN32)
    target_include_directories(SOURCES PUBLIC
        ${VCPKG_INSTALLED_DIR}/x64-windows/include
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/include/utils
        ${CMAKE_SOURCE_DIR}/include/vulkan
        ${CMAKE_SOURCE_DIR}/include/models
        ${CMAKE_SOURCE_DIR}/include/models/implementations
        ${CMAKE_SOURCE_DIR}/include/scenes
        ${CMAKE_SOURCE_DIR}/include/scenes/implementations
        ${SHADERC_INCLUDE_DIRS}
    )

    target_link_libraries(${PROJECT_NAME} PRIVATE
        SOURCES
        Vulkan::Vulkan
        glm::glm-header-only
        glfw
        OpenSSL::Crypto
        assimp::assimp
        unofficial::shaderc::shaderc
    )
else()
    target_include_directories(SOURCES PUBLIC
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/include/utils
        ${CMAKE_SOURCE_DIR}/include/vulkan
        ${CMAKE_SOURCE_DIR}/include/models
        ${CMAKE_SOURCE_DIR}/include/models/implementations
        ${CMAKE_SOURCE_DIR}/include/scenes
        ${CMAKE_SOURCE_DIR}/include/scenes/implementations
        ${SHADERC_INCLUDE_DIRS}
    )

    target_link_libraries(${PROJECT_NAME} PRIVATE
        SOURCES
        Vulkan::Vulkan
        glm
        glfw
        OpenSSL::Crypto
        ${SHADERC_LIBRARIES}
        assimp
    )
endif()
