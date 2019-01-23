#!/bin/bash
set -e
set -x

# initial update
apt-get update

# for add-apt-repository
apt-get install -y software-properties-common python-software-properties wget

# for newer libstdc++
add-apt-repository -y ppa:ubuntu-toolchain-r/test
# for clang
add-apt-repository 'deb http://apt.llvm.org/precise/ llvm-toolchain-precise-3.8 main'
wget -O - http://apt.llvm.org/llvm-snapshot.gpg.key | apt-key add -
apt-get update

# install dependencies
apt-get install --force-yes -y libx11-dev libx11-xcb-dev mesa-common-dev libgl1-mesa-dev gcc g++ clang-3.8 clang++-3.8 make pkg-config git libcurl4-openssl-dev libpcre3-dev libstdc++-6-dev

# install dependencies for building qt
apt-get install --force-yes -y libproxy-dev autoconf autogen libtool xutils-dev bison

update-alternatives --install /usr/bin/clang clang /usr/bin/clang-3.8 380
update-alternatives --install /usr/bin/clang++ clang++ /usr/bin/clang++-3.8 380

# build xcb-keysyms for static linking
wget https://xcb.freedesktop.org/dist/xcb-util-keysyms-0.4.0.tar.gz
tar xzf xcb-util-keysyms-0.4.0.tar.gz
cd xcb-util-keysyms-0.4.0/
CFLAGS="-fPIC -fvisibility=hidden" ./configure --prefix=/usr --disable-shared --enable-static
make
make install
cd ..

# xcb-proto
wget http://xcb.freedesktop.org/dist/xcb-proto-1.10.tar.gz
tar -xf xcb-proto-1.10.tar.gz
cd xcb-proto-1.10
CFLAGS="-fPIC -fvisibility=hidden" ./configure --prefix=/usr
make
make install
cd ..

# libxcb
wget https://xcb.freedesktop.org/dist/libxcb-1.10.tar.gz
tar -xf libxcb-1.10.tar.gz
cd libxcb-1.10
CFLAGS="-fPIC -fvisibility=hidden" ./configure --prefix=/usr
make
make install
cd ..

# libxkbcommon
wget https://github.com/xkbcommon/libxkbcommon/archive/xkbcommon-0.7.0.tar.gz
tar -xf xkbcommon-0.7.0.tar.gz
cd libxkbcommon-xkbcommon-0.7.0
CFLAGS="-fPIC -fvisibility=hidden" ./autogen.sh --disable-shared --prefix=/usr --enable-static
make
make install
cd ..

# libfontconfig static linking seems to break, so dynamic link against libfreetype and libfontconfig
apt-get install --force-yes -y libfontconfig1-dev

# build qt for static linking
wget http://download.qt.io/archive/qt/5.6/5.6.2/single/qt-everywhere-opensource-src-5.6.2.tar.xz
tar -xf qt-everywhere-opensource-src-5.6.2.tar.xz
cd qt-everywhere-opensource-src-5.6.2

# Fix for linking static qt into a shared library:
# https://bugreports.qt.io/browse/QTBUG-52605
# https://codereview.qt-project.org/171007
cd qtbase
git apply < /static_tagging.patch
cd ..

./configure -prefix /usr -release -opensource -confirm-license -static -platform linux-clang -no-qml-debug -qt-zlib -no-mtdev -no-journald -no-syslog -qt-libpng -qt-libjpeg -system-xkbcommon-x11 -fontconfig -no-icu -qt-harfbuzz -openssl -libproxy -qt-pcre -qt-xcb -no-xinput2 -no-pulseaudio -no-alsa -v -no-cups -no-linuxfb -no-opengl -no-gstreamer -no-libinput -no-sse3 -no-ssse3 -no-sse4.1 -no-sse4.2 -no-avx -no-avx2 -skip qtdeclarative -skip qtsensors -skip qtconnectivity -skip qtmultimedia -skip qtscript -skip qtserialbus -skip qtserialport -skip qtcanvas3d -skip qtenginio -skip qtlocation -skip qtgraphicaleffects -skip qtxmlpatterns -skip qtwebview -skip qt3d -skip qttools -nomake examples -nomake tools -nomake tests
make -j4
make install
cd ..

# build cmake locally, to get ssl support for external projects
wget http://www.cmake.org/files/v3.2/cmake-3.2.2.tar.gz
tar xf cmake-3.2.2.tar.gz
cd cmake-3.2.2
./configure --prefix=/usr --system-curl --parallel=4
make -j4
make install
cd ..

# build python locally to static link against
wget https://www.python.org/ftp/python/3.6.1/Python-3.6.1.tgz
tar xf Python-3.6.1.tgz
cd Python-3.6.1
./configure --prefix=/usr
make -j4
make install
cd ..

