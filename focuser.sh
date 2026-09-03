#!/usr/bin/env bash
set -euo pipefail

mkdir -p build/

echo "--------------------------------"

cmake -B build -G Ninja -DCMAKE_CXX_COMPILER=g++-14
cmake --build build --parallel

echo "--------------------------------"

./build/focuser $*