#!/usr/bin/env bash
set -euo pipefail

rm -rf ./build/
rm -rf ./bin/

mkdir -p ./build/
cd build/

cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON ..

if command -v nproc >/dev/null 2>&1; then
  JOBS="$(nproc)"
else
  JOBS="$(sysctl -n hw.ncpu 2>/dev/null || echo 4)"
fi

make -j "$JOBS"
