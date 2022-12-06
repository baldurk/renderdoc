# Dependencies

(Once dependencies are installed, see [Compiling.md](Compiling.md))

## Windows

On Windows there are no dependencies - you can always compile the latest version just by downloading the code and compiling the solution in Visual Studio. If you want to modify the Qt UI with a WYSIWYG editor you will need a version of Qt installed - at least version 5.6.

## Linux

RenderDoc only supports building on 64-bit x86 linux. 32-Bit x86 and any ARM/other platforms are not supported.

Requirements for the core library and renderdoccmd are `libx11`, `libxcb`, `libxcb-keysyms` and `libGL`. The exact are packages for these vary by distribution.

For qrenderdoc you need Qt5 >= 5.6 along with the 'svg' and 'x11extras' packages. You also need `python3-dev` for the python integration, and `bison`, `autoconf`, `automake` and `libpcre3-dev` for building the custom SWIG tool for generating bindings.

On any distribution if you find qmake isn't available under its default name, or if `qmake -v` lists a Qt4 version, make sure you have qtchooser installed in your package manager and use it to select Qt5. This might be done by exporting `QT_SELECT=qt5`, but check with your distribution for details.

For some distributions such as CentOS and Fedora, the Qt5 qmake command is `qmake-qt5`. To select this explicitly, pass `-DQMAKE_QT5_COMMAND=qmake-qt5` when invoking `cmake`.

Below are specific per-distribution instructions. If you know the required packages for another distribution, please share (or pull request this file!)

### Ubuntu

For Ubuntu 14.04 or above you'll need:

```
sudo apt-get install libx11-dev libx11-xcb-dev mesa-common-dev libgl1-mesa-dev libxcb-keysyms1-dev cmake python3-dev bison autoconf automake libpcre3-dev
```

For the base dependencies. On Ubuntu 18.04 and above Qt is available in the default repositories:

```
sudo apt-get install qt5-qmake libqt5svg5-dev libqt5x11extras5-dev 
```

For older versions of Ubuntu they might not include a recent enough Qt version, so you can use [Stephan Binner's ppas](https://launchpad.net/~beineri) to install a more recent version of Qt. At least 5.6.2 is required. If you choose to instead install an [official Qt release](https://download.qt.io/official_releases/qt/) or build Qt from source, add `-DQMAKE_QT5_COMMAND=/path/to/qmake` to your cmake arguments.

### Archlinux

For Archlinux (as of 2019.04.12) you'll need:

```
sudo pacman -S libx11 libxcb xcb-util-keysyms mesa libgl qt5-base qt5-svg qt5-x11extras cmake python3 bison autoconf automake pcre make pkg-config
```

### Gentoo

For Gentoo (as of 2017.04.18), you'll need:

```
sudo emerge --ask x11-libs/libX11 x11-libs/libxcb x11-libs/xcb-util-keysyms dev-util/cmake dev-qt/qtcore dev-qt/qtgui dev-qt/qtwidgets dev-qt/qtsvg dev-qt/qtx11extras sys-devel/bison sys-devel/autoconf sys-devel/automake dev-lang/python dev-libs/libpcre
```

Checking that at least Qt 5.6 installs.

### CentOS

On CentOS 7 (as of 2018.01.18), you'll need to install from several repos:

```
# Dependencies in default repo
yum install libX11-devel libxcb-devel mesa-libGL-devel xcb-util-keysyms-devel cmake qt5-qtbase-devel qt5-qtsvg-devel qt5-qtx11extras-devel bison autoconf automake pcre-devel

# python3 via EPEL
yum install epel-release
yum install python34-devel

# Newer GCC via SCL's devtoolset-7
yum install centos-release-scl
yum install devtoolset-7
```

Then when building, you must first set up the devtoolset-7 from SCL:
```
scl enable devtoolset-7 bash
```

And build within the resulting bash shell, which has the tools first in PATH.

### Fedora

On Fedora 33 (as of 2020.11.05), you'll need:

```
sudo yum install libX11-devel libxcb-devel mesa-libGL-devel xcb-util-keysyms-devel cmake qt5-qtbase-devel qt5-qtsvg-devel qt5-qtx11extras-devel bison autoconf automake pcre-devel python3-devel
```

### Debian

Debian 9+ (stretch):
```
sudo apt-get install libx11-dev libx11-xcb-dev mesa-common-dev libgl1-mesa-dev libxcb-keysyms1-dev cmake python3-dev bison autoconf automake libpcre3-dev qt5-qmake libqt5svg5-dev libqt5x11extras5-dev 
```

## Mac

Mac requires Xcode version 12.2 or newer, CMake version 3.20 or newer, `autoconf`, `automake`, `pcre` and Qt5 version 5.15.2 or newer. If you're using [homebrew](http://brew.sh) then this will do the trick:

```
brew install cmake autoconf automake pcre qt5
brew link qt5 --force
```

## Android

To build for Android, you must download components of the Android SDK, the Android NDK, and Java Development Kit.

RenderDoc is currently known to build with NDK 14b, SDK tools 3859397, SDK build-tools 26.0.1, SDK platform android-23, Java 8 (also known as 1.8). If you use different versions of any of these it is up to you to ensure you have compatible versions of all as otherwise build failures may occur as versions of some components may not be compatible with different (even newer) versions of other components.

If you've already got the tools required, simply set the following three environment variables:

```
export ANDROID_SDK=<path_to_sdk_root>
export ANDROID_NDK=<path_to_ndk_root>
export JAVA_HOME=<path_to_jdk_root>
```

You must also make sure that you have the `java` from `JAVA_HOME` in your `PATH`, as some Android build commands run java directly without respecting `JAVA_HOME`.

Otherwise, below are steps to acquire the tools for each platform. These steps download specifically the versions listed above, other versions may work but are not guaranteed.

### Android Dependencies on Windows

JDK 8 can be installed from the following [link](http://www.oracle.com/technetwork/java/javase/downloads/jdk8-downloads-2133151.html).

```
set JAVA_HOME=<path_to_jdk_root>
```

Android NDK and SDK:

```
# Set up the Android SDK
set ANDROID_SDK=<path_to_desired_setup>
cd %ANDROID_SDK%
wget https://dl.google.com/android/repository/sdk-tools-windows-3859397.zip
unzip sdk-tools-windows-3859397.zip
cd tools\bin
sdkmanager --sdk_root=%ANDROID_SDK% "build-tools;26.0.1" "platforms;android-23"
# Accept the license

# Set up the Android NDK
cd %ANDROID_SDK%
wget http://dl.google.com/android/repository/android-ndk-r14b-windows-x86_64.zip
unzip android-ndk-r14b-windows-x86_64.zip
set ANDROID_NDK=%ANDROID_SDK%\android-ndk-r14b
```

### Android Dependencies on Linux

The JDK 8 can be installed with:

```
sudo apt-get install openjdk-8-jdk
export JAVA_HOME=/usr/lib/jvm/java-8-openjdk-amd64
```

The Android SDK and NDK can be set up with the following steps.

SDK links are pulled from [here](https://web.archive.org/web/20171026083141/https://developer.android.com/studio/index.html) (older versions are no longer linked from the android site, but the downloads still work).

NDK links are pulled from [here](https://developer.android.com/ndk/downloads/older_releases.html).

```
# Set up Android SDK
export ANDROID_SDK=<path_to_desired_setup>
pushd $ANDROID_SDK
wget http://dl.google.com/android/repository/sdk-tools-linux-3859397.zip
unzip sdk-tools-linux-3859397.zip
cd tools/bin/
./sdkmanager --sdk_root=$ANDROID_SDK "build-tools;26.0.1" "platforms;android-23"
# Accept the license

# Set up Android NDK
pushd $ANDROID_SDK
wget http://dl.google.com/android/repository/android-ndk-r14b-linux-x86_64.zip
unzip android-ndk-r14b-linux-x86_64.zip
export ANDROID_NDK=$ANDROID_SDK/android-ndk-r14b
```

### Android Dependencies on Mac

JDK can be installed with brew:

```
brew cask install java
export JAVA_HOME="$(/usr/libexec/java_home)"
```

Android NDK and SDK:

```
# Set up Android SDK
export ANDROID_SDK=<path_to_desired_setup>
pushd $ANDROID_SDK
wget https://dl.google.com/android/repository/sdk-tools-darwin-3859397.zip
unzip sdk-tools-darwin-3859397.zip
cd tools/bin/
./sdkmanager --sdk_root=$ANDROID_SDK "build-tools;26.0.1" "platforms;android-23"
# Accept the license

# Set up Android NDK
pushd $ANDROID_SDK
wget https://dl.google.com/android/repository/android-ndk-r14b-darwin-x86_64.zip
unzip android-ndk-r14b-darwin-x86_64.zip
export ANDROID_NDK=$ANDROID_SDK/android-ndk-r14b
```

