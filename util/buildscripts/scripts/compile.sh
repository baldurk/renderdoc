#!/bin/bash

## Compilation is platform-specific, dispatch to helper

if [ "$PLATFORM" == "Linux" ]; then

	./scripts/compile_linux.sh

elif [ "$PLATFORM" == "macOS" ]; then

	./scripts/compile_macos.sh

else

	./scripts/compile_win32.sh

fi
