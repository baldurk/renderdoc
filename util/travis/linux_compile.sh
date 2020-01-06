#!/bin/sh

set -e

# Switch to the gcc/clang version we want
if [ $CC == "gcc" ]; then
	export CC=gcc-5;
	export CXX=g++-5;
else
	export CC=clang-3.8;
	export CXX=clang++-3.8;
fi

export QT_SELECT=qt5

mkdir build
cd build
if [[ "$RELEASE_BUILD" == "1" ]]; then
	cmake -DCMAKE_BUILD_TYPE=Release ..
else
	cmake -DCMAKE_BUILD_TYPE=Debug ..
fi
make -j2

echo "--- Running unit tests ---"

./bin/renderdoccmd test unit
./bin/qrenderdoc --unittest
