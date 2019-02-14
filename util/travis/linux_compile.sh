#!/bin/sh

# ignore "error" codes in the env script below
set +e

. /opt/qt56/bin/qt56-env.sh 

set -e

# Switch to the gcc version we want
if [ $CC == "gcc" ]; then
	export CC=gcc-6;
	export CXX=g++-6;
fi

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
