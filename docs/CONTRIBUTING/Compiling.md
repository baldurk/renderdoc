# Compiling

## Windows

The main [renderdoc.sln](renderdoc.sln) is a VS2015 solution. It should also compile in later VS versions, just select to update the compilers if you don't have the 2015 compilers available.

There are no external dependencies apart from the Windows SDK and any version will work, otherwise all libraries/headers needed to build are included in the git checkout.

On windows, the `Development` configuration is recommended for day-to-day dev. It's debuggable but not too slow. The `Release` configuration is then obviously what you should compile for any builds you'll send out to people or if you want to evaluate performance.

## Linux

First check that you have all of the [required dependencies](Dependencies.md#linux).

RenderDoc only supports building on 64-bit x86 linux. 32-Bit x86 and any ARM/other platforms are not supported.

Currently linux should work with gcc 5+ and clang 3.4+ as it requires C++14 compiler support. The CI builds with gcc-5.0 and clang-3.8. Within reason other compilers will be supported if the required patches are minimal. Distribution packages should be built with the `Release` CMake build type so that warnings do not trigger errors. To build just run:

```
cmake -DCMAKE_BUILD_TYPE=Debug -Bbuild -H.
make -C build
```

Configuration is available for cmake, [documented elsewhere](https://cmake.org/documentation/). You can override the compiler with environment variables `CC` and `CXX`, and there are some options you can toggle in the root CMakeLists files such as `cmake -DENABLE_GL=OFF`.

## Mac

First check that you have all of the [required dependencies](Dependencies.md#mac).

Mac support is pretty early and while it will compile, it's not usable for debugging yet and is not officially supported. 

To build with make, use cmake the same way as Linux.

To build with Xcode, use the cmake Xcode generator to create the Xcode project:

```
cmake -DCMAKE_BUILD_TYPE=Debug -Bbuild -H. -GXcode
```

Building for Mac requires a C++17 compliant compiler i.e. the Xcode clang compiler.

## Android

First check that you have all of the [required dependencies](Dependencies.md#android).

To build the components required to debug an Android target invoke cmake and enable `BUILD_ANDROID=On`:

```
mkdir build-android
cd build-android
cmake -DBUILD_ANDROID=On -DANDROID_ABI=armeabi-v7a ..
make
```

On Windows, you should always build Android from a bash shell - cygwin, msys2, Windows WSL, etc. Building from cmd may work but is not supported.

On windows cmake you need to specify the 'generator' type to the cmake invocation. The exact parameter will depend on your bash shell, but options are e.g. `-G "MSYS Makefiles"` or `-G "MinGW Makefiles"`, i.e.:

```
cmake -DBUILD_ANDROID=On -DANDROID_ABI=armeabi-v7a -G "MSYS Makefiles" ..
```

### Note:

With GLES programs on Android, the built-in hooking method doesn't always work. If you have trouble with crashes or problems capturing GLES programs, try enabling building with [interceptor-lib](../../renderdoc/3rdparty/interceptor-lib/README.md). **WARNING**: Building this requires a hefty dependency.

