#!/bin/sh

mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..
make -j2

echo "--- Running unit tests ---"

trap 'exit' ERR
./bin/renderdoccmd test -t unit
./bin/qrenderdoc.app/Contents/MacOS/qrenderdoc --unittest
