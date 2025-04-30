#!/bin/sh

cd /home/admin/Containers/vulkan/Engine/build
cmake ..
make -j $(nproc)
/home/admin/Containers/vulkan/Engine/bin/Engine
