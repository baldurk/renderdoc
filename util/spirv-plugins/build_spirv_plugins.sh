#!/bin/bash

if [ ! -f build_spirv_plugins.sh ]; then
	echo "Run this script from its folder like './build_spirv_plugins.sh'";
	exit 1;
fi

if uname -a | grep -qiE 'win|msys|cygwin|microsoft'; then
	# Windows build

	echo "Building for win32";
	./_build.sh win32 $(pwd)/spirv-plugins-win32

	echo "Building for win64";
	./_build.sh win64 $(pwd)/spirv-plugins-win64
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
