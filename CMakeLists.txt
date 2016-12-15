cmake_minimum_required(VERSION 2.8.12)

# Configure some stuff that needs to be set really early
if(BUILD_ANDROID)
    set(CMAKE_TOOLCHAIN_FILE
        "${CMAKE_SOURCE_DIR}/scripts/android.toolchain.cmake"
        CACHE STRING
        "The Android toolchain file")

    # Set same default API level for ANDROID_ABI=armeabi-v7a as for arm64-v8a.
    set( ANDROID_DEFAULT_NDK_API_LEVEL_arm 21 )

    # Default to arm32 if nothing is specified on the command line.
    # Options are {armeabi-v7a,arm64-v8a}
    set(ANDROID_ABI "armeabi-v7a" CACHE STRING "The Android ABI to build for")

    # This will be overridden later, we just need to set it now so that the
    # configuration will continue on to where the toolchain is available
    if(WIN32)
        set(CMAKE_MAKE_PROGRAM
            "android-make-not-found"
            CACHE STRING
            "The path to the NDK's make.exe to use")
    endif()
endif()

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
option(ENABLE_GLES "Enable GLES driver" OFF)

option(ENABLE_XLIB "Enable xlib windowing support" ON)
option(ENABLE_XCB "Enable xcb windowing support" ON)

if(WIN32)
    message(FATAL_ERROR "CMake is not needed on Windows, just open and build renderdoc.sln")
endif()

if(ANDROID)
    if (ENABLE_GL)
       message(STATUS "Disabling GL driver on android switching to GLES")
       set(ENABLE_GL OFF CACHE BOOL "" FORCE)
       set(ENABLE_GLES ON CACHE BOOL "" FORCE)
    endif()

    # Android doesn't support the Qt UI for obvious reasons
    message(STATUS "Disabling qrenderdoc for android build")
    set(ENABLE_QRENDERDOC OFF CACHE BOOL "" FORCE)

    message(STATUS "Using Android ABI ${ANDROID_ABI}")
    message(STATUS "Using Android native API level ${ANDROID_NATIVE_API_LEVEL}")

    if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Windows")
        set(MAKE_SEARCH_DIRS "windows-x86_64" "windows")

        # For windows, we need to use the make program in the NDK
        foreach(dir ${MAKE_SEARCH_DIRS})
            set(__makepath "${ANDROID_NDK}/prebuilt/${dir}/bin/make.exe")
            if( EXISTS "${__makepath}" )
                set(CMAKE_MAKE_PROGRAM "${__makepath}" CACHE STRING "" FORCE)
                break()
            endif()
        endforeach()

        message(STATUS "Using build tool ${CMAKE_MAKE_PROGRAM}")
    endif()
endif()

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

if(ENABLE_GL)
    add_definitions(-DRENDERDOC_SUPPORT_GL)
endif()

if(ENABLE_GLES)
    add_definitions(-DRENDERDOC_SUPPORT_GLES)
endif()

if(ENABLE_VULKAN)
    add_definitions(-DRENDERDOC_SUPPORT_VULKAN)
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_definitions(-D_RELEASE)
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
