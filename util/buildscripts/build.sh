#!/bin/bash

#### realpath.sh from https://github.com/mkropat/sh-realpath/
#
# The MIT License (MIT)
# 
# Copyright (c) 2014 Michael Kropat
# 
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
# 
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
# 
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.
#
####

realpath() {
    canonicalize_path "$(resolve_symlinks "$1")"
}

resolve_symlinks() {
    _resolve_symlinks "$1"
}

_resolve_symlinks() {
    _assert_no_path_cycles "$@" || return

    local dir_context path
    path=$(readlink -- "$1")
    if [ $? -eq 0 ]; then
        dir_context=$(dirname -- "$1")
        _resolve_symlinks "$(_prepend_dir_context_if_necessary "$dir_context" "$path")" "$@"
    else
        printf '%s\n' "$1"
    fi
}

_prepend_dir_context_if_necessary() {
    if [ "$1" = . ]; then
        printf '%s\n' "$2"
    else
        _prepend_path_if_relative "$1" "$2"
    fi
}

_prepend_path_if_relative() {
    case "$2" in
        /* ) printf '%s\n' "$2" ;;
         * ) printf '%s\n' "$1/$2" ;;
    esac
}

_assert_no_path_cycles() {
    local target path

    target=$1
    shift

    for path in "$@"; do
        if [ "$path" = "$target" ]; then
            return 1
        fi
    done
}

canonicalize_path() {
    if [ -d "$1" ]; then
        _canonicalize_dir_path "$1"
    else
        _canonicalize_file_path "$1"
    fi
}

_canonicalize_dir_path() {
    (cd "$1" 2>/dev/null && pwd -P)
}

_canonicalize_file_path() {
    local dir file
    dir=$(dirname -- "$1")
    file=$(basename -- "$1")
    (cd "$dir" 2>/dev/null && printf '%s/%s\n' "$(pwd -P)" "$file")
}

# Optionally, you may also want to include:

### readlink emulation ###

readlink() {
    if _has_command readlink; then
        _system_readlink "$@"
    else
        _emulated_readlink "$@"
    fi
}

_has_command() {
    hash -- "$1" 2>/dev/null
}

_system_readlink() {
    command readlink "$@"
}

_emulated_readlink() {
    if [ "$1" = -- ]; then
        shift
    fi

    _gnu_stat_readlink "$@" || _bsd_stat_readlink "$@"
}

_gnu_stat_readlink() {
    local output
    output=$(stat -c %N -- "$1" 2>/dev/null) &&

    printf '%s\n' "$output" |
        sed "s/^‘[^’]*’ -> ‘\(.*\)’/\1/
             s/^'[^']*' -> '\(.*\)'/\1/"
    # FIXME: handle newlines
}

_bsd_stat_readlink() {
    stat -f %Y -- "$1" 2>/dev/null
}

####

TYPE=""
SNAPNAME=""
SYMSTORE=""
SKIPCOMPILE=""
LLVM_ARM32=$(realpath $(dirname $0)/support/llvm_arm32)
LLVM_ARM64=$(realpath $(dirname $0)/support/llvm_arm64)

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
		LLVM_ARM32="$(realpath "$2")"
		shift
		shift
		;;
		--llvm_arm64)
		LLVM_ARM64="$(realpath "$2")"
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
export BUILD_ROOT=$(dirname $(realpath $0))

cd "${BUILD_ROOT}"

export REPO_ROOT=$(realpath "$(pwd)/../..")

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

if [[ "$PLATFORM" == "Darwin" ]]; then
	PLATFORM=macOS
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
if [[ "$PLATFORM" == "Windows" ]]; then

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

