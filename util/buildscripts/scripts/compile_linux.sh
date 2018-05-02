#!/bin/bash

rm -f /tmp/compile_errors /tmp/error_email

# Store the path to the error-mail script
ERROR_SCRIPT=$(readlink -f "${BUILD_ROOT}"/scripts/errormail.sh)

# Ensure the docker image is prepared
pushd "${BUILD_ROOT}"/scripts/docker
docker build -t renderdoc-build .
popd

# Run the docker compilation script inside the container above to build the main renderdoc project
mkdir -p /tmp/rdoc_docker
cp "${BUILD_ROOT}"/scripts/compile_docker.sh /tmp/rdoc_docker
docker run --rm -v /tmp/rdoc_docker:/io -v $(readlink -f "${REPO_ROOT}"):/renderdoc:ro renderdoc-build bash /io/compile_docker.sh 2>&1 | tee /tmp/compile_errors

if [ -d /tmp/rdoc_docker/dist ]; then
	echo "No error.";
else
	echo "Error encountered.";
	iconv -f UTF-8 -t ASCII//translit /tmp/compile_errors > /tmp/error_email
	$ERROR_SCRIPT /tmp/error_email
	exit 1;
fi

# pushd into the git checkout
pushd "${REPO_ROOT}"

# Copy the dist folder structure to the git checkout
cp -R /tmp/rdoc_docker/dist .

# TODO - here we could copy off the build with symbols?

# Strip the binaries
strip --strip-unneeded dist/bin/*
strip --strip-unneeded dist/lib/*

# Copy python modules to where they'd be built natively, for documentation build
mkdir build
cp -R /tmp/rdoc_docker/pymodules build/bin

# Step into the docs folder and build
pushd docs
make clean
make html > /tmp/sphinx.log

if [ $? -ne 0 ]; then
	$ERROR_SCRIPT /tmp/sphinx.log
	exit 1;
fi

popd; # docs

# if we didn't produce an html file, bail out even if sphinx didn't return an error code above
if [ ! -f ./Documentation/html/index.html ]; then
	echo >> /tmp/sphinx.log
	echo "Didn't auto-build html docs." >> /tmp/sphinx.log
	$ERROR_SCRIPT /tmp/sphinx.log
	exit 1;
fi

# Build android libraries and apks
export PATH=$PATH:$ANDROID_SDK/tools/

# Check that we're set up to build for android
if [ ! -d $ANDROID_SDK/tools ] ; then
	echo "\$ANDROID_SDK is not correctly configured: '$ANDROID_SDK'" >> /tmp/android.log
	$ERROR_SCRIPT /tmp/android.log
	# Don't return an error code, consider android errors non-fatal other than emailing
	exit 0;
fi

if [ ! -d $LLVM_ARM32 ] || [ ! -d $LLVM_ARM64 ] ; then
	echo "llvm is not available, expected $LLVM_ARM32 and $LLVM_ARM64 respectively." >> /tmp/android.log
	$ERROR_SCRIPT /tmp/android.log
	# Don't return an error code, consider android errors non-fatal other than emailing
	exit 0;
fi

# Build the arm32 variant
mkdir build-android-arm32
pushd build-android-arm32

cmake -DBUILD_ANDROID=1 -DANDROID_ABI=armeabi-v7a -DANDROID_NATIVE_API_LEVEL=23 -DCMAKE_BUILD_TYPE=Release -DSTRIP_ANDROID_LIBRARY=On -DLLVM_DIR=$LLVM_ARM32/lib/cmake/llvm -DUSE_INTERCEPTOR_LIB=On -DCMAKE_MAKE_PROGRAM=make ..
make -j8

if ! ls bin/*.apk; then
	echo >> /tmp/cmake.log
	echo "Failed to build android?" >> /tmp/cmake.log
	$ERROR_SCRIPT /tmp/cmake.log
fi

popd # build-android-arm32

mkdir build-android-arm64
pushd build-android-arm64

cmake -DBUILD_ANDROID=1 -DANDROID_ABI=arm64-v8a -DANDROID_NATIVE_API_LEVEL=23 -DCMAKE_BUILD_TYPE=Release -DSTRIP_ANDROID_LIBRARY=On -DLLVM_DIR=$LLVM_ARM64/lib/cmake/llvm -DUSE_INTERCEPTOR_LIB=On -DCMAKE_MAKE_PROGRAM=make ..
make -j8

if ! ls bin/*.apk; then
	echo >> /tmp/cmake.log
	echo "Failed to build android?" >> /tmp/cmake.log
	$ERROR_SCRIPT /tmp/cmake.log
fi

popd # build-android-arm64

popd # $REPO_ROOT
