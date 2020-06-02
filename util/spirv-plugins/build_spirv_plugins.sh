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

if [ ! -f build_spirv_plugins.sh ]; then
	echo "Run this script from its folder like './build_spirv_plugins.sh'";
	exit 1;
fi

if uname -a | grep -qiE 'win|msys|cygwin|microsoft'; then
	# Windows build

	echo "Building for win32";
	./_build.sh win32 $(native_path $(pwd))/spirv-plugins-win32

	echo "Building for win64";
	./_build.sh win64 $(native_path $(pwd))/spirv-plugins-win64
else
	# Linux build

	if docker image ls | grep -q renderdoc-build; then
		echo "Building for linux";

		docker run --rm -v $(pwd):/script:ro -v $(pwd)/spirv-plugins-linux64:/out renderdoc-build bash /script/_build.sh linux64 /out

	else
		echo "Run normal RenderDoc build first to generate renderdoc-build image";
		exit 1;
	fi
fi
