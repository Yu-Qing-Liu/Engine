cmake_minimum_required(VERSION 3.15)
project(Engine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin")

add_compile_definitions(PROJECT_ROOT_DIR="${CMAKE_SOURCE_DIR}")

if(WIN32)
    find_package(unofficial-shaderc REQUIRED)
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SHADERC REQUIRED shaderc)
    pkg_check_modules(LIBPQXX REQUIRED libpqxx)
endif()

find_package(Vulkan REQUIRED)
find_package(glm REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(assimp REQUIRED)
find_package(Freetype REQUIRED)

file(GLOB MODELS       ${CMAKE_CURRENT_SOURCE_DIR}/src/models/*.cpp)
file(GLOB MODELS_IMPL
    ${CMAKE_CURRENT_SOURCE_DIR}/src/models/implementations/unique/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/models/implementations/instanced/*.cpp
)
file(GLOB SCENES          ${CMAKE_CURRENT_SOURCE_DIR}/src/scenes/*.cpp)
file(GLOB SCENES_IMPL     ${CMAKE_CURRENT_SOURCE_DIR}/src/scenes/implementations/*.cpp)
file(GLOB SCENES_DEFAULT  ${CMAKE_CURRENT_SOURCE_DIR}/src/scenes/implementations/default/*.cpp)
file(GLOB UI              ${CMAKE_CURRENT_SOURCE_DIR}/src/ui/*.cpp)
file(GLOB UI_IMPL         ${CMAKE_CURRENT_SOURCE_DIR}/src/ui/implementations/*.cpp)
file(GLOB COMPUTE         ${CMAKE_CURRENT_SOURCE_DIR}/src/compute/*.cpp)
file(GLOB COMPUTE_IMPL    ${CMAKE_CURRENT_SOURCE_DIR}/src/compute/implementations/*.cpp)

file(GLOB CIRCUIT         ${CMAKE_CURRENT_SOURCE_DIR}/src/circuit/*.cpp)

add_library(SOURCES ${MODELS} ${MODELS_IMPL} ${SCENES} ${SCENES_IMPL} ${SCENES_DEFAULT} ${UI} ${UI_IMPL} ${COMPUTE} ${COMPUTE_IMPL} ${CIRCUIT})
add_executable(${PROJECT_NAME} ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp)

if(WIN32)
    target_include_directories(SOURCES PUBLIC
        ${VCPKG_INSTALLED_DIR}/x64-windows/include
    )
endif()

target_include_directories(SOURCES PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/utils
    ${CMAKE_SOURCE_DIR}/include/vulkan
    ${CMAKE_SOURCE_DIR}/include/models
    ${CMAKE_SOURCE_DIR}/include/models/implementations/unique
    ${CMAKE_SOURCE_DIR}/include/models/implementations/instanced
    ${CMAKE_SOURCE_DIR}/include/scenes
    ${CMAKE_SOURCE_DIR}/include/scenes/implementations
    ${CMAKE_SOURCE_DIR}/include/scenes/implementations/default
    ${CMAKE_SOURCE_DIR}/include/ui
    ${CMAKE_SOURCE_DIR}/include/ui/implementations
    ${CMAKE_SOURCE_DIR}/include/compute
    ${CMAKE_SOURCE_DIR}/include/compute/implementations
    ${SHADERC_INCLUDE_DIRS}
    ${LIBPQXX_INCLUDE_DIRS}

    ${CMAKE_SOURCE_DIR}/include/circuit/
)

if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE
        unofficial::shaderc::shaderc
        assimp::assimp
    )
else()
    target_link_libraries(${PROJECT_NAME} PRIVATE
        assimp
        ${SHADERC_LIBRARIES}
        ${LIBPQXX_LIBRARIES}
    )
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE
    SOURCES
    Vulkan::Vulkan
    glm::glm
    glfw
    OpenSSL::Crypto
    Freetype::Freetype
)
