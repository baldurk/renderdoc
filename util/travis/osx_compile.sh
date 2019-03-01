#!/bin/sh

mkdir build
pushd build
cmake -DCMAKE_BUILD_TYPE=Debug ..
make -j2
popd # build

echo "--- Running unit tests ---"

trap 'exit' ERR
./build/bin/renderdoccmd test unit
./build/bin/qrenderdoc.app/Contents/MacOS/qrenderdoc --unittest
