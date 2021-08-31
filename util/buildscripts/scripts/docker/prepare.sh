#!/bin/bash
set -e
set -x

# fix EOL ubuntu package URLs
sed -i.bak -r 's/(archive|security).ubuntu.com/old-releases.ubuntu.com/g' /etc/apt/sources.list

# initial update
apt-get update

# for add-apt-repository
apt-get install -y software-properties-common python-software-properties wget

# to allow https apt repositories
apt-get install -y apt-transport-https ca-certificates

# for newer libstdc++
add-apt-repository -y ppa:ubuntu-toolchain-r/test
# for clang
add-apt-repository 'deb https://apt.llvm.org/precise/ llvm-toolchain-precise-3.8 main'
wget -O - http://apt.llvm.org/llvm-snapshot.gpg.key | apt-key add -
apt-get update

# install dependencies
apt-get install --force-yes -y libx11-dev libx11-xcb-dev mesa-common-dev libgl1-mesa-dev gcc-5 g++-5 gcc g++ clang-3.8 clang++-3.8 make pkg-config git libcurl4-openssl-dev libpcre3-dev

# install dependencies for building qt
apt-get install --force-yes -y libproxy-dev autoconf autogen libtool xutils-dev bison

update-alternatives --install /usr/bin/clang clang /usr/bin/clang-3.8 380
update-alternatives --install /usr/bin/clang++ clang++ /usr/bin/clang++-3.8 380

update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-5 500
update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-5 500

static_build_freedesktop_lib() {
  wget https://xcb.freedesktop.org/dist/$1.tar.gz
  tar -xf $1.tar.gz
  cd $1
  CFLAGS="-fPIC -fvisibility=hidden" ./configure --prefix=/usr --libdir=/usr/lib/x86_64-linux-gnu --disable-shared --enable-static
  make && make install
  cd ..
}

# First build xcb-proto required by libxcb
static_build_freedesktop_lib xcb-proto-1.11

# now build libxcb once with everything enabled and static only. This will produce all the xcb-*
# libraries like libxcb-xinerama and libxcb-xfixes statically linked. We want these available to
# Qt so we don't just disable them, but we don't want to dynamic link since on some distros
# these libraries may not be installed yet and we want our package dependency footprint to be
# minimal, ideally only packages that we expect to be installed
wget https://xcb.freedesktop.org/dist/libxcb-1.11.tar.gz
tar -xf libxcb-1.11.tar.gz
cd libxcb-1.11
CFLAGS="-fPIC -fvisibility=hidden" ./configure --prefix=/usr --libdir=/usr/lib/x86_64-linux-gnu --disable-shared --enable-static
make
make install
cd ..

# However to build properly we need the base xcb library as shared, so rebuild it but with all
# the optional libraries above turned off
rm -rf libxcb-1.11
tar -xf libxcb-1.11.tar.gz
cd libxcb-1.11
CFLAGS="-fPIC" ./configure --prefix=/usr --libdir=/usr/lib/x86_64-linux-gnu --enable-shared --disable-static --disable-randr --disable-render --disable-shape --disable-shm --disable-sync --disable-xfixes --disable-xinerama --disable-xkb
make
make install
cd ..

# build static versions of other xcb libraries Qt needs to build
for I in xcb-util-keysyms-0.4.0 xcb-util-0.3.9 xcb-util-image-0.4.0 xcb-util-renderutil-0.3.9 xcb-util-wm-0.4.1; do
  static_build_freedesktop_lib $I;
done

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

# remove kerberos dev package so Qt doesn't link against it. There doesn't seem to be a configure
# option to disable it, blech
apt-get remove --force-yes -y libkrb5-dev

# Qt 5.15 needs openssl 1.1.1 headers even for runtime openssl. Presumably this will fail if the
# resulting system doesn't have openssl 1.1.1, but we accept that since at worst it means no analytics.
wget https://github.com/openssl/openssl/archive/OpenSSL_1_1_1.tar.gz
tar -xf OpenSSL_1_1_1.tar.gz
cd openssl-OpenSSL_1_1_1
./config --prefix=/usr --libdir=lib/x86_64-linux-gnu --openssldir=/usr/lib/ssl
make -j4
make install
cd ..

# build qt for static linking
wget https://download.qt.io/official_releases/qt/5.15/5.15.2/single/qt-everywhere-src-5.15.2.tar.xz
echo "e1447db4f06c841d8947f0a6ce83a7b5  qt-everywhere-src-5.15.2.tar.xz" | md5sum -c -

tar -xf qt-everywhere-src-5.15.2.tar.xz
cd qt-everywhere-src-5.15.2

./configure -prefix /usr -release -opensource -confirm-license -static -platform linux-clang -qt-zlib -no-mtdev -no-journald -no-syslog -qt-libpng -qt-libjpeg -fontconfig -no-icu -qt-harfbuzz -openssl-runtime -libproxy -qt-pcre -xcb -bundled-xcb-xinput -v -no-cups -no-linuxfb -no-opengl -no-libinput -no-sse3 -no-ssse3 -no-sse4.1 -no-sse4.2 -no-avx -no-avx2 -skip qtdeclarative -skip qtsensors -skip qtconnectivity -skip qtmultimedia -skip qtscript -skip qtserialbus -skip qtserialport -skip qtcanvas3d -skip qtlocation -skip qtgraphicaleffects -skip qtxmlpatterns -skip qtwebview -skip qt3d -skip qttools -nomake examples -nomake tools -nomake tests -platform linux-g++

make -j$(nproc)
make install
cd ..

# we can now install libcurl *after* building Qt, so that cmake can use it for curl with
# ssl support. We can't do this before because it pulls in kerberos which we explicitly
# uninstalled earlier
apt-get install --force-yes -y libcurl4-openssl-dev

# build cmake locally, to get ssl support for external projects
wget http://www.cmake.org/files/v3.2/cmake-3.2.2.tar.gz
tar xf cmake-3.2.2.tar.gz
cd cmake-3.2.2
./configure --prefix=/usr --system-curl --parallel=4
make -j$(nproc)
make install
cd ..

# build python locally to static link against
wget https://www.python.org/ftp/python/3.6.1/Python-3.6.1.tgz
tar xf Python-3.6.1.tgz
cd Python-3.6.1
./configure --prefix=/usr
make -j$(nproc)
make install
cd ..

