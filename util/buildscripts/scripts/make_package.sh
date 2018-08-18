#!/bin/bash

## Packaging is platform-specific, dispatch to helper

if [ "$PLATFORM" == "Linux" ]; then

	./scripts/make_package_linux.sh $1

else

	./scripts/make_package_win32.sh $1

fi
