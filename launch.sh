#!/bin/sh

rm -rf ./build/
rm -rf ./bin/

mkdir -p ./build/
cd build/
cmake ..
make -j $(nproc)
cd ../bin
./Engine
