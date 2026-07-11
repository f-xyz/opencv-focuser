#!/usr/bin/env bash
set -euo pipefail

rm -rfv build/
mkdir build/

echo "--------------------------------"

cmake -B build -G Ninja -DCMAKE_CXX_COMPILER=g++-14
cmake --build build

echo "--------------------------------"

./build/focuser $* param1=qwe param2=asd param3