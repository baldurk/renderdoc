cmake_minimum_required(VERSION 2.8.12)

if(WIN32)
    message(FATAL_ERROR "CMake is not needed on Windows, just open and build renderdoc.sln")
endif()

# disallow in-source builds because we have a top-level wrapper Makefile
if(CMAKE_BINARY_DIR STREQUAL CMAKE_SOURCE_DIR)
    message(FATAL_ERROR "In-source builds not allowed")
endif()

project(RenderDoc CXX C)

option(ENABLE_GL "Enable GL driver" ON)
option(ENABLE_VULKAN "Enable Vulkan driver" ON)
option(ENABLE_RENDERDOCCMD "Enable renderdoccmd" ON)
option(ENABLE_QRENDERDOC "Enable qrenderdoc" ON)

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fvisibility=hidden -fvisibility-inlines-hidden")

    set(warning_flags
        -Wall
        -Wextra
        -Werror
        -Wno-unused-variable
        -Wno-type-limits
        -Wno-unused-parameter
        -Wno-missing-field-initializers
        -Wno-unknown-pragmas
        -Wno-reorder)
    if(CMAKE_COMPILER_IS_GNUCXX)
        list(APPEND warning_flags -Wno-unused-but-set-variable)
    endif()

    string(REPLACE ";" " " warning_flags "${warning_flags}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${warning_flags}")
endif()

add_definitions(-DRENDERDOC_PLATFORM_LINUX)

add_subdirectory(renderdoc)

if(ENABLE_RENDERDOCCMD)
    add_subdirectory(renderdoccmd)
endif()

if(ENABLE_QRENDERDOC)
    add_subdirectory(qrenderdoc)
endif()
