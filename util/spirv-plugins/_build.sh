#!/bin/bash

PLATFORM=$1
OUT=$2

set -ex

# Freshly clone repositories
rm -rf glslang SPIRV-Cross
git clone https://github.com/KhronosGroup/glslang
git clone https://github.com/KhronosGroup/SPIRV-Cross

GENERATOR="Unix Makefiles"

if [ "$PLATFORM" == "win64" ]; then
	GENERATOR="Visual Studio 15 2017 Win64";
elif [ "$PLATFORM" == "win32" ]; then
	GENERATOR="Visual Studio 15 2017";
else
	export CC=clang CXX=clang++ CFLAGS="-fPIC -fvisibility=hidden" LDFLAGS="-static-libstdc++"
fi

##### SPIRV-Cross

pushd SPIRV-Cross

	mkdir build
	pushd build

		cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="${OUT}"

		if [ "${PLATFORM:0:3}" == "win" ]; then
			cmake --build . --config Release --target install
		else
			make -j$(nproc) install
			strip --strip-unneeded ${OUT}/bin/*
		fi

	popd #build

popd # SPIRV-Cross


##### glslang (and bonus SPIRV-Tools for free)
pushd glslang

	# Update external sources
	python update_glslang_sources.py

	# Now build
	mkdir build
	pushd build
	
		cmake .. -DSPIRV_ALLOW_TIMERS=OFF -DSPIRV_SKIP_TESTS=ON -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="${OUT}"

		if [ "${PLATFORM:0:3}" == "win" ]; then
			cmake --build . --config Release --target install
		else
			make -j$(nproc) install
			strip --strip-unneeded ${OUT}/bin/*
		fi

	popd #build

popd # glslang


