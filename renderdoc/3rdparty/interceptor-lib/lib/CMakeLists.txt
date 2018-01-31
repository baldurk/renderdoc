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

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti")

set(sources
    code_generator.cc
    constant_pool_data_expr.cc
    disassembler.cc
    error.cc
    interceptor.cc
    linker.cc
    memory_manager.cc
    target.cc)

set(interceptor_arch_libs)

foreach(TARGET ${LLVM_TARGETS_TO_BUILD})
    file(GLOB arch_sources ${TARGET}/*.cc)
    list(APPEND sources ${arch_sources})

    set(needed_libs
        ${TARGET}AsmParser
        ${TARGET}Desc
        ${TARGET}Disassembler
        ${TARGET}AsmPrinter
        ${TARGET}Info)

    if(${TARGET} STREQUAL "AArch64")
        list(APPEND needed_libs ${TARGET}Utils)
    endif()

    llvm_map_components_to_libnames(arch_libs ${needed_libs})
    list(APPEND interceptor_arch_libs ${arch_libs})
endforeach()

llvm_map_components_to_libnames(interceptor_general_libs
    MCDisassembler
    Object
    MCParser
    BitReader
    MC
    Core
    Support)

set(interceptor_libs z ${interceptor_arch_libs} ${interceptor_general_libs} PARENT_SCOPE)

add_library(interceptor_lib OBJECT
    ${sources})

