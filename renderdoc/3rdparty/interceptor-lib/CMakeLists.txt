# Copyright (C) 2017 Google Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

set(LLVM_CCACHE_BUILD OFF)
if(NOT CMAKE_HOST_WIN32)
    find_program(CCACHE_FOUND ccache)
    if(CCACHE_FOUND)
        set(LLVM_CCACHE_BUILD ON)
    endif()
endif()

function(llvm_abi android_abi out)
    if (${android_abi} STREQUAL "arm64-v8a")
        set(${out} "AArch64" PARENT_SCOPE)
    elseif(${android_abi} STREQUAL "armeabi-v7a")
        set(${out} "ARM" PARENT_SCOPE)
    elseif(${android_abi} STREQUAL "x86")
        set(${out} "X86" PARENT_SCOPE)
    else()
        message(FATAL_ERROR "Unsupported architecture for building LLVM: ${android_abi}")
    endif()
endfunction()

llvm_abi(ANDROID_ABI target_arch)
file(GLOB sources
    "lib/*.h" "lib/*.cc"
    "lib/${target_arch}/*.h" "lib/${target_arch}/*.cc"
)

if (ANDROID_ABI STREQUAL "arm64-v8a")
    set(LLVM_TARGET_ARCH "${target_arch}")
    set(LLVM_HOST_TRIPLE "aarch64-unknown-linux-android")
elseif(ANDROID_ABI STREQUAL "armeabi-v7a")
    set(LLVM_TARGET_ARCH "${target_arch}")
    set(LLVM_HOST_TRIPLE "armv8.2a-unknown-linux-android")
elseif(ANDROID_ABI STREQUAL "x86")
    set(LLVM_TARGET_ARCH "${target_arch}")
    set(LLVM_HOST_TRIPLE "i386-unknown-linux-android")
else()
    message(FATAL_ERROR "Unsupported architecture for building LLVM: ${LLVM_TARGET_ARCH}")
endif()

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

add_subdirectory(lib)

set(interceptor_libs ${interceptor_libs} PARENT_SCOPE)
