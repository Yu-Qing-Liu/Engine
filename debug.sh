#!/bin/sh

mkdir -p /home/admin/Containers/vulkan/Engine/build
cd /home/admin/Containers/vulkan/Engine/build
cmake ..
make -j $(nproc)
gdb /home/admin/Containers/vulkan/Engine/bin/Engine
