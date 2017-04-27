#!/bin/bash

if [ ! -f renderdoc/data/resource.h ]; then
	echo "This script should be run from the root of the checkout.";
	echo "e.g. ./scripts/hash_version.sh";
	exit;
fi

GIT_HASH=`git status > /dev/null 2>&1 && git rev-parse HEAD || echo NO_GIT_COMMIT_HASH_DEFINED`;

rm -f ver
sed "s/NO_GIT_COMMIT_HASH_DEFINED/$GIT_HASH/" renderdoc/api/replay/version.h > ver && mv ver renderdoc/api/replay/version.h
sed -b "s/NO_GIT_COMMIT_HASH_DEFINED/$GIT_HASH/" renderdocui/Properties/AssemblyInfo.cs > ver && mv ver renderdocui/Properties/AssemblyInfo.cs
rm -f ver
