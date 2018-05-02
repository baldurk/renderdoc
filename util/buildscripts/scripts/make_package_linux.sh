#!/bin/bash

FILENAME="$1"

if [ $# -ne 1 ]; then
	echo "Usage: $0 FILENAME";
	exit 1;
fi

if [ ! -d "${REPO_ROOT}"/dist/bin ]; then
	echo "Expected 'dist' folder in renderdoc. When building, set CMAKE_INSTALL_PREFIX to dist and make install";
	exit 1;
fi

FILENAME="$(echo $FILENAME | tr A-Z a-z)";

rm -rf "${REPO_ROOT}"/package
mkdir "${REPO_ROOT}"/package
pushd "${REPO_ROOT}"/package

mkdir -p ${FILENAME}/

# copy in the target directory tree, cmake will have done most of our packaging for us
cp -R "${REPO_ROOT}"/dist/* "./${FILENAME}/"

# copy readme and license to the package root
cp "./${FILENAME}"/share/doc/renderdoc/{LICENSE.md,README} "./${FILENAME}"

# copy in html documentation
if [ -d "${REPO_ROOT}"/Documentation/html ]; then
	cp -R "${REPO_ROOT}"/Documentation/html "./${FILENAME}/share/doc/renderdoc/html"
else
	echo "WARNING: Documentation not built! run 'make html' in docs folder";
fi

# copy in plugins
if [ -d "${REPO_ROOT}"/plugins-linux64 ]; then
	cp -R "${REPO_ROOT}"/plugins-linux64 "./${FILENAME}/share/renderdoc/plugins"
	chmod +x -R "./${FILENAME}/share/renderdoc/plugins"/*
else
	echo "WARNING: Plugins not present. Download and extract https://renderdoc.org/plugins.tgz in root folder";
fi

# copy in all of the android files.
mkdir -p "./${FILENAME}/share/renderdoc/plugins/android/"

if ls "${REPO_ROOT}"/build-android*/bin/*.apk; then
	cp "${REPO_ROOT}"/build-android*/bin/*.apk "./${FILENAME}/share/renderdoc/plugins/android/"
else
	echo "WARNING: Android build not present. Build arm32 and arm64 apks in build-android-arm{32,64} folders";
fi

# compress the folder and GPG sign it
tar -zcf ${FILENAME}.tar.gz "./${FILENAME}/"* --xform 's#^./##g'
gpg -o ${FILENAME}.tar.gz.sig --detach-sign --armor ${FILENAME}.tar.gz

rm -rf "./${FILENAME}"

popd # $REPO_ROOT/package
