#!/bin/sh

trap 'exit' ERR

mkdir build
pushd build
if [[ "$RELEASE_BUILD" == "1" ]]; then
	cmake -DCMAKE_BUILD_TYPE=Release ..
else
	cmake -DCMAKE_BUILD_TYPE=Debug ..
fi
make -j2
popd # build

echo "--- Running unit tests ---"

if [[ "$RELEASE_BUILD" == "1" ]]; then
	echo "Not running tests on release build"
else
	./build/bin/renderdoccmd test unit
	./build/bin/qrenderdoc.app/Contents/MacOS/qrenderdoc --unittest
fi
