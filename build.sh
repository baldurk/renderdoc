
PROJECT_DIR=$(cd $(dirname $0); pwd)

echo $PROJECT_DIR

build_dir="$PROJECT_DIR/build-android-arm64"
build_abi="arm64-v8a"

if [[ $1 == "32" ]]; then
  build_dir="$PROJECT_DIR/build-android-arm32"
  build_abi="armeabi-v7a"
fi

if [ ! -d "$build_dir" ]; then
  mkdir "$build_dir"
fi

cd "$build_dir"
cmake -DBUILD_ANDROID=On -DANDROID_ABI="$build_abi" ..
make
