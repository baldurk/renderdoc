#!/bin/sh
set -ev

sudo apt-get -qq update
sudo apt-get install -y cmake

export ARCH=`uname -m`

# Pull known working tools toward the end of 2016
wget http://dl.google.com/android/repository/android-ndk-r13b-linux-${ARCH}.zip
wget https://dl.google.com/android/repository/tools_r25.2.5-linux.zip
wget https://dl.google.com/android/repository/platform-tools_r25.0.3-linux.zip
wget https://dl.google.com/android/repository/build-tools_r25.0.2-linux.zip
unzip -u -q android-ndk-r13b-linux-${ARCH}.zip
unzip -u -q tools_r25.2.5-linux.zip
unzip -u -q platform-tools_r25.0.3-linux.zip
unzip -u -q build-tools_r25.0.2-linux.zip

# Munge the build-tools layout
mkdir -p build-tools/25.0.2
mv android-7.1.1/* build-tools/25.0.2/

export ANDROID_HOME=`pwd`/tools
export JAVA_HOME="/usr/lib/jvm/java-8-oracle"
export ANDROID_NDK=`pwd`/android-ndk-r13b
export PATH=`pwd`/android-ndk-r13b:$PATH
export PATH=`pwd`/tools:$PATH
export PATH=`pwd`/platform-tools:$PATH
export PATH=`pwd`/build-tools/25.0.2:$PATH

# Answer "yes" to any license acceptance requests
(while sleep 3; do echo "y"; done) | android update sdk --no-ui -s -t android-23
