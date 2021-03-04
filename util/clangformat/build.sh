#!/bin/bash
set -e
set -x

apt-get update
apt-get install -y git cmake python3 gcc g++

git clone --depth=5 https://github.com/llvm/llvm-project --branch llvmorg-15.0.7
mkdir llvm-build
cd llvm-build
cmake /llvm-project/llvm -DLLVM_ENABLE_PROJECTS=clang -DCMAKE_EXE_LINKER_FLAGS=-static -DCMAKE_BUILD_TYPE=Release
make -j8 clang-format
strip --strip-unneeded bin/clang-format
cp bin/clang-format /out
