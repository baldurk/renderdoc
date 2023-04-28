# Added for RenderDoc's use (particularly when cross-compiling RenderDoc, as a separate project
# makes it easier to always compile include-bin for the host)
cmake_minimum_required(VERSION 2.8.12)
project(include-bin CXX)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")

add_executable(include-bin main.cpp)
