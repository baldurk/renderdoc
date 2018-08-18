#!/bin/bash

mkdir -p "${REPO_ROOT}/dist"

# Get the logfile as a windows path
LOGFILE=$(cd ${REPO_ROOT}/dist && cmd.exe /C cd | tr -d '[:space:]')/MSBuild.log

echo "Building, log in ${LOGFILE}"

# Store the path to the error-mail script
ERROR_SCRIPT=$(readlink -f "${BUILD_ROOT}"/scripts/errormail.sh)

# pushd into the git checkout
pushd "${REPO_ROOT}"

# Build 32-bit Release
MSYS2_ARG_CONV_EXCL="*" msbuild.exe /nologo /fl4 /flp4':Verbosity=minimal;Encoding=ASCII;logfile='"${LOGFILE}" renderdoc.sln /t:Rebuild /p:'Configuration=Release;Platform=x86'
if [ $? -ne 0 ]; then
	$ERROR_SCRIPT /tmp/MSbuild.log
	exit 1;
fi

# Build 64-bit Release
MSYS2_ARG_CONV_EXCL="*" msbuild.exe /nologo /fl4 /flp4':Verbosity=minimal;Encoding=ASCII;logfile='"${LOGFILE}" renderdoc.sln /t:Rebuild /p:'Configuration=Release;Platform=x64'
if [ $? -ne 0 ]; then
	$ERROR_SCRIPT /tmp/MSbuild.log
	exit 1;
fi

# Step into the docs folder and build
pushd docs
./make.sh clean
./make.sh htmlhelp > /tmp/sphinx.log

if [ $? -ne 0 ]; then
	$ERROR_SCRIPT /tmp/sphinx.log
	exit 1;
fi

popd; # docs

# if we didn't produce a chm file, bail out even if sphinx didn't return an error code above
if [ ! -f ./Documentation/htmlhelp/renderdoc.chm ]; then
	echo >> /tmp/sphinx.log
	echo "Didn't auto-build chm file. Missing HTML Help Workshop?" >> /tmp/sphinx.log
	$ERROR_SCRIPT /tmp/sphinx.log
	exit 1;
fi

export PATH=$PATH:$(cygpath -u $ANDROID_SDK/tools/)

# Check that we're set up to build for android
if [ ! -d $ANDROID_SDK/tools ] ; then
	echo "\$ANDROID_SDK is not correctly configured: '$ANDROID_SDK'" > /tmp/android.log
	cat /tmp/android.log
	$ERROR_SCRIPT /tmp/android.log
	# Don't return an error code, consider android errors non-fatal other than emailing
	exit 0;
fi

if [ ! -d $LLVM_ARM32 ] || [ ! -d $LLVM_ARM64 ] ; then
	echo "llvm is not available, expected $LLVM_ARM32 and $LLVM_ARM64 respectively." > /tmp/android.log
	cat /tmp/android.log
	$ERROR_SCRIPT /tmp/android.log
	# Don't return an error code, consider android errors non-fatal other than emailing
	exit 0;
fi

# Build the arm32 variant
mkdir build-android-arm32
pushd build-android-arm32

cmake -G "MSYS Makefiles" -DBUILD_ANDROID=1 -DANDROID_ABI=armeabi-v7a -DCMAKE_BUILD_TYPE=Release -DSTRIP_ANDROID_LIBRARY=On -DLLVM_DIR=$LLVM_ARM32/lib/cmake/llvm -DUSE_INTERCEPTOR_LIB=On .. 2>&1 | tee /tmp/cmake.log
make -j8 2>&1 | tee -a /tmp/cmake.log

if ! ls bin/*.apk; then
	echo >> /tmp/cmake.log
	echo "Failed to build android?" >> /tmp/cmake.log
	$ERROR_SCRIPT /tmp/cmake.log
fi

popd # build-android-arm32

mkdir build-android-arm64
pushd build-android-arm64

cmake -G "MSYS Makefiles" -DBUILD_ANDROID=1 -DANDROID_ABI=arm64-v8a -DCMAKE_BUILD_TYPE=Release -DSTRIP_ANDROID_LIBRARY=On -DLLVM_DIR=$LLVM_ARM64/lib/cmake/llvm -DUSE_INTERCEPTOR_LIB=On .. | tee /tmp/cmake.log
make -j8 2>&1 | tee -a /tmp/cmake.log

if ! ls bin/*.apk; then
	echo >> /tmp/cmake.log
	echo "Failed to build android?" >> /tmp/cmake.log
	$ERROR_SCRIPT /tmp/cmake.log
fi

popd # build-android-arm64

popd # $REPO_ROOT

