#!/bin/bash

GIT_HASH=`sh ./calc_hash.sh`;

rm -f ver
sed -b "s/NO_GIT_COMMIT_HASH_DEFINED/$GIT_HASH$1/" renderdoc/data/resource.h > ver && mv ver renderdoc/data/resource.h
sed -b "s/NO_GIT_COMMIT_HASH_DEFINED/$GIT_HASH$1/" renderdocui/Properties/AssemblyInfo.cs > ver && mv ver renderdocui/Properties/AssemblyInfo.cs
rm -f ver
