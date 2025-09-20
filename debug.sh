#!/bin/sh

mkdir -p ./build/
cd build/
cmake ..
make -j $(nproc)
cd ../bin
gdb --args ./Engine

