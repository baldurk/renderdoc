#!/bin/sh

echo "Preparing macOS dependencies"

./util/buildscripts/scripts/prepare_deps_macos.sh build/bin/qrenderdoc.app/Contents/MacOS/qrenderdoc

set +v

if [[ "$TRAVIS_OS_NAME" == "osx" ]] && [[ "$APPLE_BUILD" == "1" ]]; then
	echo "Uploading macOS build to make nightly builds"
	FNAME="RenderDoc_macOS_"`git rev-parse HEAD`.zip
	zip -r "${FNAME}" build/bin
	ls -lh "${FNAME}"
	lftp sftp://"${UPLOADLOCATION}" -e "cd upload; put ${FNAME}; bye"
else
	echo "Running OSX deploy on unexpected platform.";
	exit 1;
fi
