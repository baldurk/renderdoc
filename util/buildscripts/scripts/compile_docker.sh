#!/bin/bash
set -e
set -x

rm -rf /io/*

cd /
mkdir renderdoc_build
cd renderdoc_build
CC=clang CXX=clang++ CFLAGS="-fPIC -fvisibility=hidden" LDFLAGS="-static-libstdc++" cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_INSTALL_PREFIX=/io/dist/ -DVULKAN_LAYER_FOLDER=/io/dist/etc/vulkan/implicit_layer.d -DSTATIC_QRENDERDOC=ON -DQRENDERDOC_NO_CXX11_REGEX=ON /renderdoc
make -j8
make install

# Copy python modules
mkdir /io/pymodules
cp -R lib/*.so /io/pymodules

# Copy python lib folder, and trim
mkdir -p /io/dist/share/renderdoc/pylibs/lib
cd /io/dist/share/renderdoc/pylibs/lib
cp -R /usr/lib/python3.6/ .
cd python3.6
# remove cache files
rm -rf $(find -iname __pycache__)
# remove unwanted modules
rm -rf test site-packages ensurepip distutils idlelib config-*
