#!/bin/sh
. /opt/qt56/bin/qt56-env.sh 
mkdir build && cd build && cmake -DCMAKE_BUILD_TYPE=Debug .. && make
