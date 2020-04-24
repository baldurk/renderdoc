#!/bin/bash

native_path() {
	if echo "${1}" | grep -q :; then
		echo "${1}";
	elif which cygpath >/dev/null 2>&1; then
		cygpath -w "${1}";
	elif which wslpath >/dev/null 2>&1; then
		wslpath -w "${1}";
	else
		echo "${1}";
	fi;
}

PLATFORM=$(uname)

if uname -a | grep -qiE 'msys|cygwin|microsoft'; then
	PLATFORM=Windows

	if [ -d /c/Windows ]; then
		WIN_ROOT=/
	elif [ -d /mnt/c/Windows ]; then
		WIN_ROOT=/mnt/
	elif [ -d /cygdrive/c/Windows ]; then
		WIN_ROOT=/cygdrive/
	else
		echo "Can't locate Windows root";
		exit 1;
	fi
fi

INSTALL_PATH=$(pwd)
INSTALL_PATH=$(native_path "${INSTALL_PATH}")

# Clone the repository if it's not already cloned
if [ ! -d shaderc ]; then
	git clone https://github.com/google/shaderc
fi

pushd shaderc

# Update to latest
git pull
./utils/git-sync-deps

if [[ "$PLATFORM" == "Windows" ]]; then

	if [[ "$CMAKE_GENERATOR" == "" ]]; then
		CMAKE_GENERATOR="Visual Studio 14 2015"
	fi

	# CMake configure if the folders don't exist
	cmake -DSHADERC_SKIP_TESTS=ON -DSHADERC_ENABLE_SPVC=OFF -DCMAKE_INSTALL_PREFIX="$INSTALL_PATH"/x64 -DCMAKE_BUILD_TYPE=Release -G "$CMAKE_GENERATOR Win64" -Bbuild64 -H.
	cmake -DSHADERC_SKIP_TESTS=ON -DSHADERC_ENABLE_SPVC=OFF -DCMAKE_INSTALL_PREFIX="$INSTALL_PATH"/x86 -DCMAKE_BUILD_TYPE=Release -G "$CMAKE_GENERATOR" -Bbuild32 -H.

	# Build and install
	cmake --build build64 --config Release --target install --parallel 8
	cmake --build build32 --config Release --target install --parallel 8

else

	cmake -DSHADERC_SKIP_TESTS=ON -DSHADERC_ENABLE_SPVC=OFF -DCMAKE_INSTALL_PREFIX="$INSTALL_PATH"/linux64 -DCMAKE_BUILD_TYPE=Release -Bbuild64 -H.
	
	cmake --build build64 --target install -- -j 8

fi

rm -rf build64 build32
rm -rf $INSTALL_PATH/{x64,x86}/lib/SPIRV*lib $INSTALL_PATH/{x64,x86}/lib/glslang*lib
rm -rf $INSTALL_PATH/{x64,x86}/bin
