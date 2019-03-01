#!/bin/bash

# create pushd into the build folder
pushd "${REPO_ROOT}"

mkdir -p build
pushd build

cmake -DCMAKE_BUILD_TYPE=Release ..
make -j4

popd # build

# Build android libraries and apks
export PATH=$PATH:$ANDROID_SDK/tools/

# Check that we're set up to build for android
if [ ! -d $ANDROID_SDK/tools ] ; then
	echo "\$ANDROID_SDK is not correctly configured: '$ANDROID_SDK'"
	exit 0;
fi

if [ ! -d $LLVM_ARM32 ] || [ ! -d $LLVM_ARM64 ] ; then
	echo "llvm is not available, expected $LLVM_ARM32 and $LLVM_ARM64 respectively."
	exit 0;
fi

AAPT=$(ls $ANDROID_SDK/build-tools/*/aapt 2>/dev/null | tail -n 1)

# Check to see if we already have this built, and don't rebuild
VERSION32=$($AAPT dump badging build-android-arm32/bin/*apk 2>/dev/null | grep -Eo "versionName='[0-9a-f]*'" | grep -Eo "'.*'" | tr -d "'")
VERSION64=$($AAPT dump badging build-android-arm64/bin/*apk 2>/dev/null | grep -Eo "versionName='[0-9a-f]*'" | grep -Eo "'.*'" | tr -d "'")

if [ "$VERSION32" == "$GITTAG" ]; then

	echo "Found existing compatible arm32 build at $GITTAG, not rebuilding";

else

	# Build the arm32 variant
	mkdir build-android-arm32
	pushd build-android-arm32

	cmake -DBUILD_ANDROID=1 -DANDROID_ABI=armeabi-v7a -DANDROID_NATIVE_API_LEVEL=23 -DCMAKE_BUILD_TYPE=Release -DSTRIP_ANDROID_LIBRARY=On -DLLVM_DIR=$LLVM_ARM32/lib/cmake/llvm -DUSE_INTERCEPTOR_LIB=On -DCMAKE_MAKE_PROGRAM=make ..
	make -j4

	if ! ls bin/*.apk; then
		echo "Android build failed"
		exit 0;
	fi

	popd # build-android-arm32

fi

if [ "$VERSION64" == "$GITTAG" ]; then

	echo "Found existing compatible arm64 build at $GITTAG, not rebuilding";

else

	mkdir build-android-arm64
	pushd build-android-arm64

	cmake -DBUILD_ANDROID=1 -DANDROID_ABI=arm64-v8a -DANDROID_NATIVE_API_LEVEL=23 -DCMAKE_BUILD_TYPE=Release -DSTRIP_ANDROID_LIBRARY=On -DLLVM_DIR=$LLVM_ARM64/lib/cmake/llvm -DUSE_INTERCEPTOR_LIB=On -DCMAKE_MAKE_PROGRAM=make ..
	make -j4

	if ! ls bin/*.apk; then
		echo "Android build failed"
		exit 0;
	fi

	popd # build-android-arm64

fi

popd # $REPO_ROOT/build
