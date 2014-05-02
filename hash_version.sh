#!/bin/bash

GIT_HASH=`git rev-parse HEAD`;

rm -f ver
sed -b "s/NO_GIT_COMMIT_HASH_DEFINED/$GIT_HASH/" renderdoc/data/resource.h > ver && mv ver renderdoc/data/resource.h
sed -b "s/NO_GIT_COMMIT_HASH_DEFINED/$GIT_HASH/" renderdocui/Properties/AssemblyInfo.cs > ver && mv ver renderdocui/Properties/AssemblyInfo.cs
rm -f ver
