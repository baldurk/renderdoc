#!/bin/sh

# ignore "error" codes in the env script below
set +e

. /opt/qt56/bin/qt56-env.sh

set -ev

mkdir build
pushd build

# Do a minimal build with as little as possible to get the python modules
CC=gcc-6 CXX=g++-6 cmake -DCMAKE_BUILD_TYPE=Debug -DENABLE_GL=OFF -DENABLE_GLES=OFF -DENABLE_VULKAN=OFF -DENABLE_RENDERDOCCMD=OFF -DENABLE_QRENDERDOC=OFF ..
make -j2

popd

cd docs/
make html SPHINXOPTS=-W
