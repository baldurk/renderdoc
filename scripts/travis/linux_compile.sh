#!/bin/sh

# ignore "error" codes in the env script below
set +e

. /opt/qt56/bin/qt56-env.sh 

set -e

mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..
make
