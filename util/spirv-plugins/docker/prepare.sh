#!/bin/bash
set -e
set -x

# Get new cmake for glslang and spirv-tools

wget https://github.com/Kitware/CMake/releases/download/v3.26.2/cmake-3.26.2-linux-x86_64.tar.gz
tar -zxvf cmake-3.26.2-linux-x86_64.tar.gz

export PATH=$(pwd)/cmake-3.26.2-linux-x86_64/bin:$PATH

# Update to newer clang and libc++ for spirv-tools...

wget https://github.com/llvm/llvm-project/archive/refs/heads/release/8.x.tar.gz
tar -zxf 8.x.tar.gz

cd llvm-project-release-8.x/
cd llvm/projects/
ln -s ../../libcxx
ln -s ../../libcxxabi
cd ../../

mkdir build
cd build
cmake -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_BUILD_TYPE=Release ../llvm/
make install-cxx -j8
make install-cxxabi -j8
make install -j8
cd ..

mkdir clang-build
cd clang-build
cmake -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_BUILD_TYPE=Release -DLLVM_CONFIG_PATH=/usr/bin/llvm-config ../clang
make install -j8
