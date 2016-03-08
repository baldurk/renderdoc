cmake_minimum_required(VERSION 2.8.12)

project(RenderDoc CXX C)

if(WIN32)
    message(FATAL_ERROR "No Win32 support")
endif()

option(ENABLE_GL "Enable GL driver" ON)
option(ENABLE_VULKAN "Enable Vulkan driver" ON)
option(ENABLE_RENDERDOCCMD "Enable renderdoccmd" ON)
option(ENABLE_QRENDERDOC "Enable qrenderdoc" ON)

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

if(${CMAKE_VERSION} VERSION_GREATER 3.0)
    set(CMAKE_CXX_STANDARD 11)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
else()
    if(CMAKE_COMPILER_IS_GNUCXX)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
    endif()
endif()

if(CMAKE_COMPILER_IS_GNUCXX)
    set(warning_flags
        -Wall
        -Wextra
        -Wno-unknown-pragmas
        -Wno-reorder
        -Wno-ignored-qualifiers
        -Wno-unused-parameter
        -Wno-unused-variable
        -Wno-missing-field-initializers
        -Wunused-but-set-variable
        -Wno-type-limits
        -Wno-unused-but-set-variable
        -Wno-unused-function)
    string(REPLACE ";" " " warning_flags "${warning_flags}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${warning_flags}")
endif()

add_subdirectory(renderdoc)

if(ENABLE_RENDERDOCCMD)
    add_subdirectory(renderdoccmd)
endif()

if(ENABLE_QRENDERDOC)
    add_subdirectory(qrenderdoc)
endif()
