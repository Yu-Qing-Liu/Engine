#!/bin/sh

set -e  # exit immediately on any error

# Bootstrap vcpkg if not already present
if [ ! -d "vcpkg" ]; then
    echo "Cloning vcpkg..."
    git clone https://github.com/microsoft/vcpkg.git --recursive
    ./vcpkg/bootstrap-vcpkg.sh
else
    echo "vcpkg already exists, skipping clone."
fi

echo "Installing dependencies via vcpkg..."
./vcpkg/vcpkg install --clean-after-build
