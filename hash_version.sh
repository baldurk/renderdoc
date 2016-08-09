#!/bin/sh

GIT_HASH=$(sh ./calc_hash.sh)

# NOTICE:
# don't use sed -i, because it's not portable between Linux, OS X and some BSDs
rm -f ver
sed "s/NO_GIT_COMMIT_HASH_DEFINED/$GIT_HASH$1/" renderdoc/data/resource.h > ver && mv ver renderdoc/data/resource.h
sed "s/NO_GIT_COMMIT_HASH_DEFINED/$GIT_HASH$1/" renderdocui/Properties/AssemblyInfo.cs > ver && mv ver renderdocui/Properties/AssemblyInfo.cs
rm -f ver
