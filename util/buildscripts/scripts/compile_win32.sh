#!/bin/bash

mkdir -p "${REPO_ROOT}/dist"

# pushd into the git checkout
pushd "${REPO_ROOT}"

# Build 32-bit Release
MSYS2_ARG_CONV_EXCL="*" msbuild.exe /nologo /fl4 /flp4':Verbosity=minimal;Encoding=ASCII' renderdoc.sln /t:Rebuild /p:'Configuration=Release;Platform=x86'

# Build 64-bit Release
MSYS2_ARG_CONV_EXCL="*" msbuild.exe /nologo /fl4 /flp4':Verbosity=minimal;Encoding=ASCII' renderdoc.sln /t:Rebuild /p:'Configuration=Release;Platform=x64'

# Step into the docs folder and build
pushd docs
./make.sh clean
./make.sh htmlhelp

popd; # docs

# if we didn't produce a chm file, bail out even if sphinx didn't return an error code above
if [ ! -f ./Documentation/htmlhelp/renderdoc.chm ]; then
	echo "Didn't auto-build chm file. Missing HTML Help Workshop?"
	exit 1;
fi

# Transform ANDROID_SDK / ANDROID_NDK to native paths if needed
if echo "${ANDROID_SDK}" | grep -q :; then
        NATIVE_ANDROID_SDK_PATH=$(echo "${ANDROID_SDK}" | sed -e '{s#^\(.\):[/\]#\1/#g}' | tr '\\' '/')
        # Add on wherever windows drives are
        ANDROID_SDK="${WIN_ROOT}${NATIVE_ANDROID_SDK_PATH}"

        export ANDROID_SDK
fi

if echo "${ANDROID_NDK}" | grep -q :; then
        NATIVE_ANDROID_NDK_PATH=$(echo "${ANDROID_NDK}" | sed -e '{s#^\(.\):[/\]#\1/#g}' | tr '\\' '/')
        # Add on wherever windows drives are
        ANDROID_NDK="${WIN_ROOT}${NATIVE_ANDROID_NDK_PATH}"

        export ANDROID_NDK
fi

export PATH=$PATH:"${ANDROID_SDK}/tools"

# Check that we're set up to build for android
if [ ! -d "${ANDROID_SDK}"/tools ] ; then
	echo "\$ANDROID_SDK is not correctly configured: '$ANDROID_SDK'"
	# Don't return an error code, consider android errors non-fatal
	exit 0;
fi

if ! which cmake > /dev/null 2>&1; then
	echo "Don't have cmake, can't build android";
	exit 0;
fi

if ! which make > /dev/null 2>&1; then
	echo "Don't have make, can't build android";
	exit 0;
fi

if [ ! -d $LLVM_ARM32 ] || [ ! -d $LLVM_ARM64 ] ; then
	echo "llvm is not available, expected $LLVM_ARM32 and $LLVM_ARM64 respectively."
	# Don't return an error code, consider android errors non-fatal
	exit 0;
fi

GENERATOR="Unix Makefiles"

if uname -a | grep -iq msys; then
	GENERATOR="MSYS Makefiles"
fi

AAPT=$(ls $ANDROID_SDK/build-tools/*/aapt{,.exe} 2>/dev/null | tail -n 1)

# Check to see if we already have this built, and don't rebuild
VERSION32=$($AAPT dump badging build-android-arm32/bin/*apk 2>/dev/null | grep -Eo "versionName='[0-9a-f]*'" | grep -Eo "'.*'" | tr -d "'")
VERSION64=$($AAPT dump badging build-android-arm64/bin/*apk 2>/dev/null | grep -Eo "versionName='[0-9a-f]*'" | grep -Eo "'.*'" | tr -d "'")

if [ "$VERSION32" == "$GITTAG" ]; then

	echo "Found existing compatible arm32 build at $GITTAG, not rebuilding";

else

	# Build the arm32 variant
	mkdir -p build-android-arm32
	pushd build-android-arm32

	cmake -G "${GENERATOR}" -DBUILD_ANDROID=1 -DANDROID_ABI=armeabi-v7a -DCMAKE_BUILD_TYPE=Release -DSTRIP_ANDROID_LIBRARY=On -DLLVM_DIR=$LLVM_ARM32/lib/cmake/llvm -DUSE_INTERCEPTOR_LIB=On ..
	make -j$(nproc)

	if ! ls bin/*.apk; then
		echo "Android build failed"
	fi

	popd # build-android-arm32

fi

if [ "$VERSION64" == "$GITTAG" ]; then

	echo "Found existing compatible arm64 build at $GITTAG, not rebuilding";

else

	mkdir -p build-android-arm64
	pushd build-android-arm64

	cmake -G "${GENERATOR}" -DBUILD_ANDROID=1 -DANDROID_ABI=arm64-v8a -DCMAKE_BUILD_TYPE=Release -DSTRIP_ANDROID_LIBRARY=On -DLLVM_DIR=$LLVM_ARM64/lib/cmake/llvm -DUSE_INTERCEPTOR_LIB=On ..
	make -j$(nproc)

	if ! ls bin/*.apk; then
		echo "Android build failed"
	fi

	popd # build-android-arm64

fi

popd # $REPO_ROOT

