cmake_minimum_required(VERSION 2.8.12)

# disallow in-source builds because we have a top-level wrapper Makefile
if(CMAKE_BINARY_DIR STREQUAL CMAKE_SOURCE_DIR)
    message(FATAL_ERROR "In-source builds not allowed")
endif()

function(get_git_hash _git_hash)
  if(EXISTS "${CMAKE_SOURCE_DIR}/.git")
    execute_process(
      COMMAND git rev-parse HEAD
      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
      OUTPUT_VARIABLE GIT_HASH
      OUTPUT_STRIP_TRAILING_WHITESPACE
    )
  endif()
  if(NOT GIT_HASH)
    set(GIT_HASH "NO_GIT_COMMIT_HASH_DEFINED")
  endif()
  set(${_git_hash} "${GIT_HASH}" PARENT_SCOPE)
endfunction(get_git_hash)

project(RenderDoc CXX C)

option(ENABLE_GL "Enable GL driver" ON)
option(ENABLE_VULKAN "Enable Vulkan driver" ON)
option(ENABLE_RENDERDOCCMD "Enable renderdoccmd" ON)
option(ENABLE_QRENDERDOC "Enable qrenderdoc" ON)

option(ENABLE_XLIB "Enable xlib windowing support" ON)
option(ENABLE_XCB "Enable xcb windowing support" ON)

if(WIN32)
    message(FATAL_ERROR "CMake is not needed on Windows, just open and build renderdoc.sln")
endif()

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

if(ENABLE_GL)
    add_definitions(-DRENDERDOC_SUPPORT_GL)
endif()

if(ENABLE_VULKAN)
    add_definitions(-DRENDERDOC_SUPPORT_VULKAN)
endif()

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -fstrict-aliasing")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fvisibility=hidden -fvisibility-inlines-hidden")

    set(warning_flags
        -Wall
        -Wextra
        -Wno-unused-variable
        -Wno-unused-parameter
        -Wno-unused-result
        -Wno-type-limits
        -Wno-missing-field-initializers
        -Wno-unknown-pragmas
        -Wno-reorder)
    if(CMAKE_COMPILER_IS_GNUCXX)
        list(APPEND warning_flags -Wno-unused-but-set-variable)
    endif()

    string(REPLACE ";" " " warning_flags "${warning_flags}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${warning_flags}")

    if(NOT CMAKE_BUILD_TYPE STREQUAL "Release")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror")
    endif()
endif()

add_definitions(-DRENDERDOC_PLATFORM_POSIX)

if(ANDROID)
    add_definitions(-DRENDERDOC_PLATFORM_ANDROID)
elseif(APPLE)
    add_definitions(-DRENDERDOC_PLATFORM_APPLE)
elseif(UNIX)
    add_definitions(-DRENDERDOC_PLATFORM_LINUX)

    if(ENABLE_XLIB)
        add_definitions(-DRENDERDOC_WINDOWING_XLIB)
    endif()

    if(ENABLE_XCB)
        add_definitions(-DRENDERDOC_WINDOWING_XCB)
    endif()
endif()

add_subdirectory(renderdoc)

if(ENABLE_RENDERDOCCMD)
    add_subdirectory(renderdoccmd)
endif()

if(ENABLE_QRENDERDOC)
    add_subdirectory(qrenderdoc)
endif()
