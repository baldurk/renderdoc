# Building static clang-format

The docker script will build and static link clang-format to make a portable static linked binary.

```
docker run --rm -v $(pwd):/script:ro -v $(pwd):/out ubuntu:jammy bash /script/build.sh
```

