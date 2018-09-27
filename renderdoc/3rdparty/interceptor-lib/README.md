# interceptor-lib

This interceptor-lib is taken from Google's [gapid project](https://github.com/google/gapid/tree/master/gapii/interceptor-lib), under the Apache 2.0 License.

Minor modifications have been made to include it in RenderDoc's build system.

# LICENSE

See [LICENSE](LICENSE) file in this directory.

# LLVM Build instructions

To use this in RenderDoc you must set `-DUSE_INTERCEPTOR_LIB=On -DLLVM_DIR=/path/to/llvm` pointing to a compatible build of LLVM 4.0 for the target you are building. Below are instructions on how to build that compatible LLVM.

You'll need the Android NDK r16b, and you'll need python available in your path.

### These instructions currently do not work for cygwin. I have not been able to figure out how to build LLVM for android from windows. If you know a way, it would be very welcome so please get in touch.

Before we get started, I found the LLVMHello pass would fail to compile on both windows and linux, so simply comment the `add_subdirectory(Hello)` line in `lib/Transforms/CMakeLists.txt` as well as removing the LLVMHello line in `test/CMakeLists.txt`.

On windows if you checked out git outside of cygwin and are building inside it, then the config.guess script might have dos newlines which will cause it to fail. If you run into problems with config.guess or config-ix.cmake, try running unix2dos on config.guess. If running outside of cygwin at all you might find that it fails to launch to fetch the default during the arm build process - just comment out that call and replace it with a default value.

To begin, clone llvm at the `release_40` tag:

```
git clone -b release_40 --depth=10 https://github.com/llvm-mirror/llvm
```

Now make a `build_native` folder and do a plain build, to create llvm-tblgen. We'll use this when making the android builds:

```
cd path/to/llvm
mkdir build_native
cd build_native

cmake ..
make -j$(nproc) llvm-tblgen
TBLGEN_PATH=$(readlink -f bin/llvm-tblgen) # remember this path
```

Next we'll make `build_arm32` and `build_arm64` folders and build for each. The instructions are listed once here, pointing out where things need to change between one and the other:


```
cd path/to/llvm
mkdir build_arm32 # or build_arm64
cd build_arm32 # or build_arm64

LLVM_ANDROID_ABI=armeabi-v7a # this is arm64-v8a for arm64
LLVM_TRIPLE=armv8.2a-unknown-linux-android # this is aarch64-unknown-linux-android for arm64
LLVM_ARCH=ARM # this is AArch64 for arm64

# Should be set above, but if not set it manually here
# TBLGEN_PATH=/path/from/above/to/bin/llvm-tblgen
NDK_PATH=/path/to/ndk16
TARGET_PATH=/path/to/llvm/install_arm32 # or /path/to/llvm/install_arm64 for arm64

cmake -DLLVM_HOST_TRIPLE:STRING=$LLVM_TRIPLE -LLVM_TARGET_ARCH:STRING=$LLVM_ARCH \
      -DLLVM_TARGETS_TO_BUILD:STRING=$LLVM_ARCH -DANDROID_ABI=$LLVM_ANDROID_ABI \
      -DANDROID_NATIVE_API_LEVEL=21 -DANDROID_TOOLCHAIN=clang -DANDROID_STL="c++_static" \
      -DCMAKE_TOOLCHAIN_FILE:PATH=$NDK_PATH/build/cmake/android.toolchain.cmake \
      -DCMAKE_INSTALL_PREFIX=$TARGET_PATH -DCMAKE_BUILD_TYPE=RelWithDebInfo \
      -DLLVM_BUILD_RUNTIME=Off -DLLVM_INCLUDE_TESTS=Off -DLLVM_INCLUDE_EXAMPLES=Off \
      -DLLVM_ENABLE_BACKTRACES=Off -DLLVM_TABLEGEN=$TBLGEN_PATH \
      -DLLVM_BUILD_TOOLS=Off -DLLVM_INCLUDE_TOOLS=Off -DLLVM_USE_HOST_TOOLS=Off ..
make -j8 install
```

There is one extra step needed to copy some non-public headers into the install, that are required by interceptor-lib. From the `build_armXX` folder:

```

# for arm32
cp -R ../lib/Target/ARM/MCTargetDesc/ $TARGET_PATH/include/
cp ./lib/Target/ARM/ARM*.inc $TARGET_PATH/include/MCTargetDesc/

# for arm64
cp -R ../lib/Target/AArch64/MCTargetDesc/ $TARGET_PATH/include/
cp ./lib/Target/AArch64/AArch64*.inc  $TARGET_PATH/include/MCTargetDesc/
```

Then you can set `-DUSE_INTERCEPTOR_LIB=On -DLLVM_DIR=/path/to/llvm/install_armXX/lib/cmake/llvm` when building renderdoc and you should see a line saying that LLVM is being used to compile interceptor-lib!
