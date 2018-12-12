#!/bin/bash

TYPE=""
SNAPNAME=""
SYMSTORE=""
SKIPCOMPILE=""
LLVM_ARM32=$(readlink -f $(dirname $0)/support/llvm_arm32)
LLVM_ARM64=$(readlink -f $(dirname $0)/support/llvm_arm64)

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

export -f native_path

usage() {
	echo "Usage: $0 --official|--snapshot <snapshot name> [options...]";
	echo;
	echo "  --symstore <symbol store>   [Windows only] Specify a path to a symbol store.";
	echo "  --llvm_arm32 <path>         Give the path to an ARM32 build of LLVM, for android.";
	echo "  --llvm_arm64 <path>         Give the path to an ARM64 build of LLVM, for android.";
	echo "  --skipcompile               Skip compile steps, package already compiled binaries.";
}

while [[ $# -gt 0 ]]; do
	key="$1"

	case $key in
		--official)
		TYPE="official"
		shift
		;;
		--snapshot)
		TYPE="snapshot"
		SNAPNAME="$2"
		shift
		shift
		;;

		-s|--symstore)
		SYMSTORE="$2"
		shift
		shift
		;;

		--llvm_arm32)
		LLVM_ARM32="$(readlink -f "$2")"
		shift
		shift
		;;
		--llvm_arm64)
		LLVM_ARM64="$(readlink -f "$2")"
		shift
		shift
		;;

		--skipcompile)
		SKIPCOMPILE="yes"
		shift
		;;

		-h|-?|-help|--help)
		usage;
		exit 0;
		;;

		*)    # unknown option
		echo "Unknown option '$1'";
		echo;
		usage;
		exit 1;
		;;
	esac
done

if [[ "$TYPE" != "official" ]] && [[ "$TYPE" != "snapshot" ]]; then
	echo "Must specify either --official or --snapshot as build type.";
	echo;
	usage;
	exit 1;
fi

if [[ "$TYPE" == "snapshot" ]] && [[ "$SNAPNAME" == "" ]]; then
	echo "Must give name for snapshot.";
	echo;
	usage;
	exit 1;
fi

echo "Building $TYPE";

if [[ "$SYMSTORE" != "" ]]; then
	echo "Storing symbols in $SYMSTORE";
	export SYMSTORE;
fi

echo "Using ARM32 LLVM from '$LLVM_ARM32' and ARM64 from '$LLVM_ARM64'"

export LLVM_ARM32;
export LLVM_ARM64;

# Ensure we're in the root directory where this script is
export BUILD_ROOT=$(dirname $(readlink -f $0))

cd "${BUILD_ROOT}"

export REPO_ROOT=$(readlink -f "$(pwd)/../..")

if [ ! -f "${REPO_ROOT}"/renderdoc.sln ]; then
	echo "Script misconfiguration - expected root of repository in '$REPO_ROOT'";
	exit 1;
fi

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

export PLATFORM;
export WIN_ROOT;

echo "Platform: $PLATFORM";
echo "Build root: $BUILD_ROOT";
echo "Repository root: $REPO_ROOT";

if [[ "$TYPE" == "official" ]]; then

	sed -i.bak "s%RENDERDOC_OFFICIAL_BUILD 0%RENDERDOC_OFFICIAL_BUILD 1%" "${REPO_ROOT}"/renderdoc/api/replay/version.h
	sed -i.bak "s%RENDERDOC_STABLE_BUILD 0%RENDERDOC_STABLE_BUILD 1%" "${REPO_ROOT}"/renderdoc/api/replay/version.h

	export GITTAG=v$(egrep "#define RENDERDOC_VERSION_(MAJOR|MINOR)" "${REPO_ROOT}"/renderdoc/api/replay/version.h | tr -dc '[0-9\n]' | tr '\n' '.' | egrep -o '[0-9]+\.[0-9]+')

else # snapshot

	export GITTAG=$(cd "${REPO_ROOT}" && git rev-parse HEAD)

fi;

cd "${BUILD_ROOT}"

if [[ "$SKIPCOMPILE" == "" ]]; then

	./scripts/compile.sh

fi

if [ $? -ne 0 ]; then
	exit 1;
fi

cd "${BUILD_ROOT}"

# Do some windows-specific build steps, like binary signing and symbol server processing
if [ "$PLATFORM" != "Linux" ]; then

	cd "${BUILD_ROOT}"

	./scripts/sign_files.sh

	cd "${BUILD_ROOT}"

	./scripts/prepare_symbols.sh

fi

cd "${BUILD_ROOT}"

if [[ "$TYPE" == "official" ]]; then

	FILENAME=RenderDoc_$(echo $GITTAG | tr -d 'v')

else # snapshot

	FILENAME=RenderDoc_${SNAPNAME}

fi

./scripts/make_package.sh "${FILENAME}"

