#!/bin/sh

CORES=$(nproc) || echo 4
mkdir -p build-android && cd build-android
cmake -DBUILD_ANDROID=On -DANDROID_ABI=armeabi-v7a -DANDROID_NATIVE_API_LEVEL=23 ..
make -j $CORES
