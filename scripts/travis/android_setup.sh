#!/bin/sh
set -ev

sudo apt-get -qq update
sudo apt-get install -y cmake

export ARCH=`uname -m`

# Pull known working tools August 2017
wget http://dl.google.com/android/repository/sdk-tools-linux-3859397.zip
wget http://dl.google.com/android/repository/android-ndk-r14b-linux-${ARCH}.zip
unzip -u -q android-ndk-r14b-linux-${ARCH}.zip
unzip -u -q sdk-tools-linux-3859397.zip

export JAVA_HOME="/usr/lib/jvm/java-8-oracle"
export ANDROID_NDK=$TRAVIS_BUILD_DIR/android-ndk-r14b
export ANDROID_SDK=$TRAVIS_BUILD_DIR

# Answer "yes" to any license acceptance requests
pushd tools/bin
(while sleep 3; do echo "y"; done) | ./sdkmanager --sdk_root=$TRAVIS_BUILD_DIR "build-tools;26.0.1" "platforms;android-23"
popd
