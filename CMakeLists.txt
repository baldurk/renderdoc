cmake_minimum_required(VERSION 2.8.12)

# Configure some stuff that needs to be set really early
if(BUILD_ANDROID)
    set(CMAKE_TOOLCHAIN_FILE
        "${CMAKE_SOURCE_DIR}/util/android.toolchain.cmake"
        CACHE STRING
        "The Android toolchain file")

    option(STRIP_ANDROID_LIBRARY "Strip the resulting android library" OFF)

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

# version setting variables. See renderdoc/api/replay/version.h

set(BUILD_VERSION_HASH "" CACHE STRING "The current git commit hash. See GIT_COMMIT_HASH in renderdoc/api/replay/version.cpp")
option(BUILD_VERSION_STABLE "If this is a stable build. See RENDERDOC_STABLE_BUILD in renderdoc/api/replay/version.h" OFF)
set(BUILD_VERSION_DIST_NAME "" CACHE STRING "The name of the distribution. See DISTRIBUTION_NAME in renderdoc/api/replay/version.h")
set(BUILD_VERSION_DIST_VER "" CACHE STRING "The distribution-specific version number. See DISTRIBUTION_VERSION in renderdoc/api/replay/version.h")
set(BUILD_VERSION_DIST_CONTACT "" CACHE STRING "The URL or email to contact with issues. See DISTRIBUTION_CONTACT in renderdoc/api/replay/version.h")
set(RENDERDOC_PLUGINS_PATH "" CACHE STRING "Path to RenderDoc plugins folder after installation of RenderDoc (either absolute or relative to binary)")
set(RENDERDOC_APK_PATH "" CACHE STRING "Path to RenderDoc .apk files after installation of RenderDoc on host (either absolute or relative to binary)")

set(LIB_SUFFIX "" CACHE STRING "Suffix for 'lib' folder in target directory structure. E.g. set to '64' to use /usr/local/lib64 instead of /usr/local/lib.")
set(LIB_SUBFOLDER "" CACHE STRING "Subfolder under the 'lib' folder in target directory structure. E.g. set to 'renderdoc' to use /usr/local/lib/renderdoc instead of /usr/local/lib.")

if(NOT LIB_SUFFIX STREQUAL "")
    add_definitions(-DRENDERDOC_LIB_SUFFIX=${LIB_SUFFIX})
endif()

set(LIB_SUBFOLDER_TRAIL_SLASH "")

if(NOT LIB_SUBFOLDER STREQUAL "")
    add_definitions(-DRENDERDOC_LIB_SUBFOLDER=${LIB_SUBFOLDER})
    set(LIB_SUBFOLDER_TRAIL_SLASH "${LIB_SUBFOLDER}/")
endif()

if(BUILD_VERSION_STABLE)
    add_definitions(-DRENDERDOC_STABLE_BUILD=1)
endif()

if(NOT BUILD_VERSION_DIST_NAME STREQUAL "")
    add_definitions(-DDISTRIBUTION_NAME="${BUILD_VERSION_DIST_NAME}")
endif()

if(NOT BUILD_VERSION_DIST_VER STREQUAL "")
    add_definitions(-DDISTRIBUTION_VERSION="${BUILD_VERSION_DIST_VER}")
endif()

if(NOT BUILD_VERSION_DIST_CONTACT STREQUAL "")
    add_definitions(-DDISTRIBUTION_CONTACT="${BUILD_VERSION_DIST_CONTACT}")
endif()

if(NOT RENDERDOC_PLUGINS_PATH STREQUAL "")
    message(STATUS "Detected custom path to RenderDoc plugins: ${RENDERDOC_PLUGINS_PATH}")
    add_definitions(-DRENDERDOC_PLUGINS_PATH="${RENDERDOC_PLUGINS_PATH}")
endif()

if(NOT RENDERDOC_APK_PATH STREQUAL "")
    message(STATUS "Detected custom path to RenderDocCmd.apk: ${RENDERDOC_APK_PATH}")
    add_definitions(-DRENDERDOC_APK_PATH="${RENDERDOC_APK_PATH}")
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
  if(BUILD_VERSION_HASH)
    set(GIT_HASH "${BUILD_VERSION_HASH}")
  endif()
  set(${_git_hash} "${GIT_HASH}" PARENT_SCOPE)
endfunction(get_git_hash)

# get git commit hash
get_git_hash(GIT_COMMIT_HASH)
string(STRIP ${GIT_COMMIT_HASH} GIT_COMMIT_HASH)
add_definitions(-DGIT_COMMIT_HASH="${GIT_COMMIT_HASH}")

project(RenderDoc CXX C)

option(ENABLE_GL "Enable GL driver" ON)
option(ENABLE_GLES "Enable GL ES driver" ON)
option(ENABLE_VULKAN "Enable Vulkan driver" ON)
option(ENABLE_RENDERDOCCMD "Enable renderdoccmd" ON)
option(ENABLE_QRENDERDOC "Enable qrenderdoc" ON)
option(ENABLE_PYRENDERDOC "Enable renderdoc python modules" ON)

option(ENABLE_XLIB "Enable xlib windowing support" ON)
option(ENABLE_XCB "Enable xcb windowing support" ON)

if(WIN32)
    message(FATAL_ERROR "CMake is not needed on Windows, just open and build renderdoc.sln")
endif()

message(STATUS "Calculating version")

execute_process(
    COMMAND awk "{ if (/_MAJOR [0-9]/) { print $3 } }" renderdoc/api/replay/version.h
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE RENDERDOC_VERSION_MAJOR
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )

execute_process(
    COMMAND awk "{ if (/_MINOR [0-9]/) { print $3 } }" renderdoc/api/replay/version.h
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE RENDERDOC_VERSION_MINOR
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )

set(RENDERDOC_VERSION "${RENDERDOC_VERSION_MAJOR}.${RENDERDOC_VERSION_MINOR}")

message(STATUS "Building RenderDoc version ${RENDERDOC_VERSION}")

if(APPLE)
    message(STATUS "Disabling GLES driver on apple")
    set(ENABLE_GLES OFF CACHE BOOL "" FORCE)
endif()

if(ANDROID)
    if(ENABLE_GL)
        message(STATUS "Disabling GL driver on android")
    endif()
    set(ENABLE_GL OFF CACHE BOOL "" FORCE)
    set(ENABLE_GLES ON CACHE BOOL "" FORCE)

    # Android doesn't support the Qt UI for obvious reasons
    message(STATUS "Disabling qrenderdoc for android build")
    set(ENABLE_QRENDERDOC OFF CACHE BOOL "" FORCE)

    # Android also doesn't support the python modules
    message(STATUS "Disabling renderdoc python modules for android build")
    set(ENABLE_PYRENDERDOC OFF CACHE BOOL "" FORCE)

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
        list(APPEND warning_flags -Wno-unused-but-set-variable -Wno-maybe-uninitialized -Wno-class-memaccess)

        # We keep the implicit fallthrough warning for now, but allow more
        # comments to silence it.
        if (CMAKE_CXX_COMPILER_VERSION VERSION_GREATER 7.0)
            list(APPEND warning_flags -Wimplicit-fallthrough=2)
        endif()
    endif()

    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND NOT CMAKE_CXX_COMPILER_VERSION VERSION_LESS 5.0 AND NOT APPLE)
        list(APPEND warning_flags -Wno-unused-lambda-capture)
    endif()

    if(NOT CMAKE_BUILD_TYPE STREQUAL "Release")
        list(APPEND warning_flags -Werror)
    endif()

    string(REPLACE ";" " " warning_flags "${warning_flags}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${warning_flags}")
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

# these variables are handled within the CMakeLists.txt in qrenderdoc,
# but we need to add it if either is enabled since the swig bindings
# are handled in common
if(ENABLE_QRENDERDOC OR ENABLE_PYRENDERDOC)
    add_subdirectory(qrenderdoc)
endif()

# install documentation files
install (FILES util/LINUX_DIST_README DESTINATION share/doc/renderdoc RENAME README)
install (FILES LICENSE.md DESTINATION share/doc/renderdoc)

message(STATUS "Enabled APIs:")
if(ENABLE_GL)
    message(STATUS "  - OpenGL")
endif()

if(ENABLE_GLES)
    message(STATUS "  - OpenGL ES (EGL)")
endif()

if(ENABLE_VULKAN)
    message(STATUS "  - Vulkan")
endif()

if(UNIX AND NOT ANDROID AND NOT APPLE)
    message(STATUS "Enabled Window System Support:")

    if(ENABLE_XLIB)
        message(STATUS "  - XLib")
    endif()

    if(ENABLE_XCB)
        message(STATUS "  - XCB")
    endif()
endif()

