#!/bin/sh
set -e

cd "$(dirname "$0")"

mkdir -p build

IN_NIX="${IN_NIX_SHELL:-}"

if [ -n "$IN_NIX" ]; then
    USE_VCPKG=0
else
    USE_VCPKG=1
fi

# Configure
if [ "$USE_VCPKG" -eq 1 ]; then
    cmake -B build -GNinja -S . \
      -DCMAKE_TOOLCHAIN_FILE="$(pwd)/vcpkg/scripts/buildsystems/vcpkg.cmake" \
      -DCMAKE_BUILD_TYPE=Debug \
      -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
else
    cmake -B build -GNinja -S . \
      -DCMAKE_BUILD_TYPE=Debug \
      -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
      ${CMAKE_PREFIX_PATH:+-DCMAKE_PREFIX_PATH="$CMAKE_PREFIX_PATH"}
fi

# Build
cmake --build build --parallel

# Expose compile_commands.json for LSPs / clangd
ln -sf build/compile_commands.json compile_commands.json

BINARY="./build/Engine"

if [ ! -x "$BINARY" ]; then
    exit 1
fi

if [ "$USE_VCPKG" -eq 1 ]; then
    export VK_ADD_LAYER_PATH="$(pwd)/vcpkg_installed/x64-linux/share/vulkan/explicit_layer.d"
fi

cd $(pwd)/build

exec gdb --args ./Engine
