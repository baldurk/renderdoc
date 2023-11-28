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
	export CC=clang CXX=clang++ CFLAGS="-fPIC -fvisibility=hidden -stdlib=libc++" LDFLAGS="-nostdlib++ -Wl,--start-group /usr/lib/libc++.a /usr/lib/libc++fs.a /usr/lib/libc++abi.a -lpthread"
	export CXXFLAGS="${CFLAGS}"

	export PATH=$(pwd)/cmake-3.26.2-linux-x86_64/bin:$PATH
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
			strip --strip-unneeded $(ls ${OUT}/bin/* | grep -v .sh)
		fi

	popd #build

popd # SPIRV-Cross


##### glslang (and bonus SPIRV-Tools for free)
pushd glslang

	# Update external sources
	python3 update_glslang_sources.py

	# Now build
	mkdir build
	pushd build
	
		cmake .. -DSPIRV_ALLOW_TIMERS=OFF -DSPIRV_SKIP_TESTS=ON -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="${OUT}"

		if [ "${PLATFORM:0:3}" == "win" ]; then
			cmake --build . --config Release --target install
		else
			make -j$(nproc) install
			strip --strip-unneeded $(ls ${OUT}/bin/* | grep -v .sh)
		fi

	popd #build

popd # glslang


