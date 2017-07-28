#!/bin/sh

mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..
echo "Building with $(nproc) jobs"
make -j2
