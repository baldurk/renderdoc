if [ ! -d "build_arm64-v8a" ]; then
  mkdir build_arm64-v8a
fi
pushd build_arm64-v8a
cmake -DBUILD_ANDROID=On -DANDROID_ABI=arm64-v8a ..
make
popd

if [ ! -d "build_armeabi-v7a" ]; then
  mkdir build_armeabi-v7a
fi
pushd build_armeabi-v7a
cmake -DBUILD_ANDROID=On -DANDROID_ABI=armeabi-v7a ..
make
popd

if [ ! -d "build_x86_64" ]; then
  mkdir build_x86_64
fi
pushd build_x86_64
cmake -DBUILD_ANDROID=On -DANDROID_ABI=x86_64 ..
make
popd

if [ ! -d "build_x86" ]; then
  mkdir build_x86
fi
pushd build_x86
cmake -DBUILD_ANDROID=On -DANDROID_ABI=x86 ..
make
popd
