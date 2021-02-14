To use a linked version of shaderc run `./build-shaderc.sh`.

Rough manual steps:

1. `git clone https://github.com/google/shaderc`
2. `cd shaderc && ./utils/git-sync-deps`
3. Build for x64 and set `CMAKE_INSTALL_PREFIX` to a `x64` subfolder here.
4. Optionally build for x86 and install to a `x86` subfolder.

On linux install to `linux64` or `linux32` (to allow sharing a checkout with windows).
