cmake_minimum_required(VERSION 2.8.12)

if(APPLE)
  # Building for Apple requires at least CMake 3.23.0
  cmake_minimum_required(VERSION 3.23.0)

  SET(CMAKE_OSX_DEPLOYMENT_TARGET "12.00" CACHE STRING "Minimum SDK for Mac OS X")
  SET(CMAKE_XCODE_GENERATE_SCHEME ON)
  SET(CMAKE_XCODE_SCHEME_ENABLE_GPU_FRAME_CAPTURE_MODE "Disabled")
endif()

if("${CMAKE_HOST_SYSTEM_NAME}" STREQUAL "FreeBSD")
  set(FREEBSD YES)
endif()

# Configure some stuff that needs to be set really early
if(BUILD_ANDROID)
    if(NOT DEFINED ENV{JAVA_HOME})
        message(FATAL_ERROR "JAVA_HOME environment variable must be defined for Android build")
    endif()
    message(STATUS "Using JAVA_HOME = $ENV{JAVA_HOME}")

    execute_process(COMMAND $ENV{JAVA_HOME}/bin/java -version
        RESULT_VARIABLE _result
        OUTPUT_VARIABLE _output
        ERROR_VARIABLE _output
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_STRIP_TRAILING_WHITESPACE)
    
    if(NOT _result AND _output MATCHES "version \"([0-9]+).([0-9]+)")
        message(STATUS "Java in JAVA_HOME is ${CMAKE_MATCH_1}.${CMAKE_MATCH_2}")
    else()
        message(STATUS "Java in JAVA_HOME is unknown version ${_output} ${_result}")
    endif()

    if(DEFINED ENV{ANDROID_HOME} AND EXISTS "$ENV{ANDROID_HOME}/build-tools")
        set(ANDROID_SDK_ROOT_PATH "$ENV{ANDROID_HOME}")
    elseif(DEFINED ENV{ANDROID_SDK_ROOT} AND EXISTS "$ENV{ANDROID_SDK_ROOT}/build-tools")
        set(ANDROID_SDK_ROOT_PATH "$ENV{ANDROID_SDK_ROOT}")
    elseif(DEFINED ENV{ANDROID_SDK} AND EXISTS "$ENV{ANDROID_SDK}/build-tools")
        set(ANDROID_SDK_ROOT_PATH "$ENV{ANDROID_SDK}")
    else()
        message(FATAL_ERROR "Can't locate Android SDK, set ANDROID_HOME, ANDROID_SDK_ROOT or ANDROID_SDK")
    endif()

    message(STATUS "Using Android SDK found in ${ANDROID_SDK_ROOT_PATH}")

    if(DEFINED ENV{ANDROID_NDK_HOME} AND EXISTS "$ENV{ANDROID_NDK_HOME}/build/cmake/android.toolchain.cmake")
        set(ANDROID_NDK_ROOT_PATH "$ENV{ANDROID_NDK_HOME}")
    elseif(DEFINED ENV{ANDROID_NDK_ROOT} AND EXISTS "$ENV{ANDROID_NDK_ROOT}/build/cmake/android.toolchain.cmake")
        set(ANDROID_NDK_ROOT_PATH "$ENV{ANDROID_NDK_ROOT}")
    elseif(DEFINED ENV{NDK_HOME} AND EXISTS "$ENV{NDK_HOME}/build/cmake/android.toolchain.cmake")
        set(ANDROID_NDK_ROOT_PATH "$ENV{NDK_HOME}")
    elseif(DEFINED ENV{ANDROID_NDK} AND EXISTS "$ENV{ANDROID_NDK}/build/cmake/android.toolchain.cmake")
        set(ANDROID_NDK_ROOT_PATH "$ENV{ANDROID_NDK}")
    else()
        message(FATAL_ERROR "Can't locate Android NDK, set ANDROID_NDK_HOME, ANDROID_NDK_ROOT, NDK_HOME or ANDROID_NDK")
    endif()

    message(STATUS "Using Android NDK found in ${ANDROID_NDK_ROOT_PATH}")

    set(CMAKE_TOOLCHAIN_FILE
        "${ANDROID_NDK_ROOT_PATH}/build/cmake/android.toolchain.cmake"
        CACHE STRING
        "The Android toolchain file")

    option(STRIP_ANDROID_LIBRARY "Strip the resulting android library" OFF)

    # Set default API level to 21 if not configured explicitly
    if(NOT ANDROID_PLATFORM)
        set(ANDROID_PLATFORM "android-21")
    endif()
    
    # default to libc++_static as the other options can cause crashes
    if(NOT ANDROID_STL)
        set(ANDROID_STL "c++_static")
    endif()

    # Choose clang if the NDK has both gcc and clang, since gcc sometimes fails
    set(CMAKE_ANDROID_NDK_TOOLCHAIN_VERSION "clang")

    # Default to arm32 if nothing is specified on the command line.
    # Options are {armeabi-v7a,arm64-v8a}
    set(ANDROID_ABI "armeabi-v7a" CACHE STRING "The Android ABI to build for")
endif()

# disallow in-source builds because we have a top-level wrapper Makefile
if(CMAKE_BINARY_DIR STREQUAL CMAKE_SOURCE_DIR)
    message(FATAL_ERROR "In-source builds not allowed")
endif()

# version setting variables. See renderdoc/api/replay/version.h

set(BUILD_VERSION_HASH "" CACHE STRING "The current git commit hash. See renderdoc/replay/version.cpp")
option(BUILD_VERSION_STABLE "If this is a stable build. See RENDERDOC_STABLE_BUILD in renderdoc/api/replay/version.h" OFF)
set(BUILD_VERSION_DIST_NAME "" CACHE STRING "The name of the distribution. See DISTRIBUTION_NAME in renderdoc/api/replay/version.h")
set(BUILD_VERSION_DIST_VER "" CACHE STRING "The distribution-specific version number. See DISTRIBUTION_VERSION in renderdoc/api/replay/version.h")
set(BUILD_VERSION_DIST_CONTACT "" CACHE STRING "The URL or email to contact with issues. See DISTRIBUTION_CONTACT in renderdoc/api/replay/version.h")
set(RENDERDOC_PLUGINS_PATH "" CACHE STRING "Path to RenderDoc plugins folder after installation of RenderDoc (either absolute or relative to binary)")
set(RENDERDOC_APK_PATH "" CACHE STRING "Path to RenderDoc .apk files after installation of RenderDoc on host (either absolute or relative to binary)")

set(LIB_SUFFIX "" CACHE STRING "Suffix for 'lib' folder in target directory structure. E.g. set to '64' to use /usr/local/lib64 instead of /usr/local/lib.")
set(LIB_SUBFOLDER "" CACHE STRING "Subfolder under the 'lib' folder in target directory structure. E.g. set to 'renderdoc' to use /usr/local/lib/renderdoc instead of /usr/local/lib.")
set(VULKAN_JSON_SUFFIX "" CACHE STRING "Suffix for the vulkan implicit_layer json file. E.g. set to '.x86_64' to use renderdoc_capture.x86_64.json instead of renderdoc_capture.json")

option(INTERNAL_SELF_CAPTURE "Internal option: enable self-capture" OFF)

set(RDOC_BASE_NAME "renderdoc")
if(INTERNAL_SELF_CAPTURE)
    set(RDOC_BASE_NAME "rdocself")
    add_definitions(-DRDOC_SELFCAPTURE_LIMITEDAPI=1)
endif()
string(TOUPPER ${RDOC_BASE_NAME} RDOC_BASE_NAME_UPPER)
add_definitions(-DRDOC_BASE_NAME=${RDOC_BASE_NAME})

if(NOT LIB_SUFFIX STREQUAL "")
    add_definitions(-DRENDERDOC_LIB_SUFFIX=${LIB_SUFFIX})
endif()

set(LIB_SUBFOLDER_TRAIL_SLASH "")

if(NOT LIB_SUBFOLDER STREQUAL "")
    add_definitions(-DRENDERDOC_LIB_SUBFOLDER=${LIB_SUBFOLDER})
    set(LIB_SUBFOLDER_TRAIL_SLASH "${LIB_SUBFOLDER}/")
endif()

add_definitions(-DRENDERDOC_VULKAN_JSON_SUFFIX=${VULKAN_JSON_SUFFIX})

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

if(EXISTS "${CMAKE_SOURCE_DIR}/.git/HEAD")
    # Use configure_file to force a re-configure if the HEAD file changes. That means changing branch.
    # The re-configure will pick up a new git commit hash above, if it exists and is valid.
    # This will be slightly redundant if BUILD_VERSION_HASH is specified but that is not a case we expect to care about.
    configure_file("${CMAKE_SOURCE_DIR}/.git/HEAD" "${CMAKE_BINARY_DIR}/git_HEAD" COPYONLY)

    # in addition, if HEAD is a ref, do the same on the file it's pointing to (since HEAD won't change for commits to the current branch)
    # if we change branch then this will correspondingly 
    file(READ ${CMAKE_SOURCE_DIR}/.git/HEAD HEAD_CONTENTS)
    string(STRIP "${HEAD_CONTENTS}" HEAD_CONTENTS)

    if("${HEAD_CONTENTS}" MATCHES "ref: ")
        string(REPLACE "ref: " "" REF_LOCATION "${HEAD_CONTENTS}")

        if(EXISTS "${CMAKE_SOURCE_DIR}/.git/${REF_LOCATION}")
            configure_file("${CMAKE_SOURCE_DIR}/.git/${REF_LOCATION}" "${CMAKE_BINARY_DIR}/git_ref" COPYONLY)
        endif()
    endif()
endif()

project(RenderDoc CXX C)

option(ENABLE_GL "Enable GL driver" ON)
option(ENABLE_GLES "Enable GL ES driver" ON)
option(ENABLE_EGL "Enable EGL" ON)
option(ENABLE_VULKAN "Enable Vulkan driver" ON)
option(ENABLE_METAL "Enable Metal driver" OFF)
option(ENABLE_RENDERDOCCMD "Enable renderdoccmd" ON)
option(ENABLE_QRENDERDOC "Enable qrenderdoc" ON)
option(ENABLE_PYRENDERDOC "Enable renderdoc python modules" ON)

option(ENABLE_XLIB "Enable xlib windowing support" ON)
option(ENABLE_XCB "Enable xcb windowing support" ON)
option(ENABLE_UNSUPPORTED_EXPERIMENTAL_POSSIBLY_BROKEN_WAYLAND "Enable EXPERIMENTAL, POSSIBLY BROKEN, UNSUPPORTED wayland windowing support" OFF)
option(ENABLE_GGP "Enable GGP support" OFF)

option(ENABLE_ASAN "Enable address sanitizer" OFF)
option(ENABLE_TSAN "Enable thread sanitizer" OFF)
option(ENABLE_MSAN "Enable memory sanitizer" OFF)

if(APPLE)
  if (CMAKE_CXX_COMPILER_VERSION VERSION_LESS 12.0)
    message(FATAL_ERROR "Xcode 12.2 or newer is required to build for Apple")
  endif()
endif()

if(ENABLE_ASAN)
    set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -fno-omit-frame-pointer")
    message(STATUS "Enabling address sanitizer - may cause issues if capturing, only recommended for use with replay only")
endif()

if(ENABLE_TSAN)
    set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=thread -fno-omit-frame-pointer")
    message(STATUS "Enabling thread sanitizer - may cause issues if capturing, only recommended for use with replay only")
endif()

if(ENABLE_UNSUPPORTED_EXPERIMENTAL_POSSIBLY_BROKEN_WAYLAND)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(PKG_WAYLAND QUIET wayland-client)

    if(PKG_WAYLAND_FOUND)
        if(NOT ENABLE_EGL)
            message(WARNING "On Wayland EGL is required for GL, disabling GL support")
            set(ENABLE_GL OFF CACHE BOOL "" FORCE)
        endif()
    else()
        message(WARNING "Wayland not found - disabling support")
        set(ENABLE_UNSUPPORTED_EXPERIMENTAL_POSSIBLY_BROKEN_WAYLAND OFF CACHE BOOL "" FORCE)
    endif()
endif()

if(WIN32)
    message(FATAL_ERROR "CMake is not needed on Windows, just open and build renderdoc.sln")
endif()

message(STATUS "Calculating version")

file(READ ${CMAKE_SOURCE_DIR}/renderdoc/api/replay/version.h VERSION_CONTENTS)

string(REGEX MATCH "_MAJOR [0-9]+" MAJOR_VERSION_LINE "${VERSION_CONTENTS}")
string(REGEX MATCH "[0-9]+" RENDERDOC_VERSION_MAJOR "${MAJOR_VERSION_LINE}")

string(REGEX MATCH "_MINOR [0-9]+" MINOR_VERSION_LINE "${VERSION_CONTENTS}")
string(REGEX MATCH "[0-9]+" RENDERDOC_VERSION_MINOR "${MINOR_VERSION_LINE}")

set(RENDERDOC_VERSION "${RENDERDOC_VERSION_MAJOR}.${RENDERDOC_VERSION_MINOR}")

message(STATUS "Building RenderDoc version ${RENDERDOC_VERSION}")

if(RENDERDOC_VERSION_MAJOR STREQUAL "")
    message(FATAL_ERROR "Couldn't calculate major version")
endif()

if(RENDERDOC_VERSION_MINOR STREQUAL "")
    message(FATAL_ERROR "Couldn't calculate minor version")
endif()

if(APPLE)
    message(STATUS "Disabling GLES driver on apple")
    set(ENABLE_GLES OFF CACHE BOOL "" FORCE)
    set(ENABLE_EGL OFF CACHE BOOL "" FORCE)
endif()

if(INTERNAL_SELF_CAPTURE)
    message(STATUS "Disabling GL/GLES for self-capture")
    set(ENABLE_GLES OFF CACHE BOOL "" FORCE)
    set(ENABLE_EGL OFF CACHE BOOL "" FORCE)
    
    message(STATUS "Disabling qrenderdoc & python modules for self-capture")
    set(ENABLE_QRENDERDOC OFF CACHE BOOL "" FORCE)
    set(ENABLE_PYRENDERDOC OFF CACHE BOOL "" FORCE)
endif()

if(ANDROID)
    if(ENABLE_GL)
        message(STATUS "Disabling GL driver on android")
    endif()
    set(ENABLE_GL OFF CACHE BOOL "" FORCE)
    set(ENABLE_GLES ON CACHE BOOL "" FORCE)
    set(ENABLE_EGL ON CACHE BOOL "" FORCE)

    # Android doesn't support the Qt UI for obvious reasons
    message(STATUS "Disabling qrenderdoc for android build")
    set(ENABLE_QRENDERDOC OFF CACHE BOOL "" FORCE)

    # Android also doesn't support the python modules
    message(STATUS "Disabling renderdoc python modules for android build")
    set(ENABLE_PYRENDERDOC OFF CACHE BOOL "" FORCE)

    message(STATUS "Using Android ABI ${ANDROID_ABI}")
    message(STATUS "Using Android native API level ${ANDROID_NATIVE_API_LEVEL}")
endif()

if(ENABLE_GGP)
    message(STATUS "Disabling GL, GLES driver on ggp")
    set(ENABLE_GL OFF CACHE BOOL "" FORCE)
    set(ENABLE_GLES OFF CACHE BOOL "" FORCE)
    set(ENABLE_EGL OFF CACHE BOOL "" FORCE)

    # GGP doesn't support the Qt UI
    message(STATUS "Disabling qrenderdoc for ggp build")
    set(ENABLE_QRENDERDOC OFF CACHE BOOL "" FORCE)

    message(STATUS "Disabling renderdoc python modules for ggp build")
    set(ENABLE_PYRENDERDOC OFF CACHE BOOL "" FORCE)
endif()

if(ENABLE_GLES AND NOT ENABLE_EGL)
    message(FATAL_ERROR "EGL is required for GLES")
endif()

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

if(APPLE)
    # For Xcode set the output directory to be the same for all configurations
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/lib")
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin")

    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/lib")
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin")
endif()

if(ENABLE_GL)
    add_definitions(-DRENDERDOC_SUPPORT_GL)
endif()

if(ENABLE_GLES)
    add_definitions(-DRENDERDOC_SUPPORT_GLES)
endif()

if(ENABLE_EGL)
    add_definitions(-DRENDERDOC_SUPPORT_EGL)
endif()

if(ENABLE_VULKAN)
    add_definitions(-DRENDERDOC_SUPPORT_VULKAN)
endif()

if(ENABLE_METAL)
    add_definitions(-DRENDERDOC_SUPPORT_METAL)
endif()

string(TOLOWER "${CMAKE_BUILD_TYPE}" cmake_build_type_lower)

set(RELEASE_MODE 0)

if(cmake_build_type_lower STREQUAL "release" OR
   cmake_build_type_lower STREQUAL "relwithdebinfo" OR
   cmake_build_type_lower STREQUAL "minsizerel")
    add_definitions(-DRELEASE)
    message(STATUS "Building RenderDoc in Release mode: ${CMAKE_BUILD_TYPE}")
    set(RELEASE_MODE 1)
endif()

if(APPLE)
    if (RELEASE_MODE)
        set(CMAKE_CONFIGURATION_TYPES "Release")
    else()
        set(CMAKE_CONFIGURATION_TYPES "Debug")
    endif()
endif()

if(STRIP_ANDROID_LIBRARY AND BUILD_ANDROID AND RELEASE_MODE)
    set(ANDROID_STRIP_TOOL "${ANDROID_TOOLCHAIN_ROOT}/bin/llvm-strip${ANDROID_TOOLCHAIN_SUFFIX}")
    if(NOT EXISTS "${ANDROID_STRIP_TOOL}")
        set(ANDROID_STRIP_TOOL "${CMAKE_STRIP}")
    endif()

    message(STATUS "libraries will be stripped with ${ANDROID_STRIP_TOOL}")
endif()

# Build as C++14
if (CMAKE_VERSION VERSION_LESS "3.1")
    set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14")
else ()
    set (CMAKE_CXX_STANDARD 14)
    set (CMAKE_CXX_STANDARD_REQUIRED ON)
endif ()

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-strict-aliasing")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fvisibility=hidden -fvisibility-inlines-hidden")
    if(ENABLE_GGP)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -gline-tables-only -fno-omit-frame-pointer")
    endif()

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
        
        if (CMAKE_CXX_COMPILER_VERSION VERSION_GREATER 11.9)
            list(APPEND warning_flags -Wno-unused-value)
        endif()
    endif()

    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        list(APPEND warning_flags -Wnewline-eof -Wunreachable-code-break -Wclass-varargs -Wstring-conversion -Wtautological-compare -Wtautological-constant-out-of-range-compare)
    endif()

    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER 3.9)
        list(APPEND warning_flags -Wcomma)
    endif()

    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND NOT CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9.0 AND NOT ANDROID)
        list(APPEND warning_flags -Wshadow)
        list(APPEND warning_flags -Wshorten-64-to-32)
    endif()

    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND NOT CMAKE_CXX_COMPILER_VERSION VERSION_LESS 5.0 AND NOT APPLE)
        list(APPEND warning_flags -Wno-unused-lambda-capture)
    endif()

    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND NOT CMAKE_CXX_COMPILER_VERSION VERSION_LESS 13.0 AND NOT APPLE)
        list(APPEND warning_flags -Wno-unused-but-set-variable -Wno-deprecated-copy)
    endif()

    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND NOT CMAKE_CXX_COMPILER_VERSION VERSION_LESS 13.1 AND APPLE)
        list(APPEND warning_flags -Wno-unused-but-set-variable -Wno-deprecated-copy)
    endif()

    if(NOT RELEASE_MODE)
        list(APPEND warning_flags -Werror)
    endif()

    string(REPLACE ";" " " warning_flags "${warning_flags}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${warning_flags}")
endif()

if(ANDROID)
    add_definitions(-DRENDERDOC_PLATFORM_ANDROID)
elseif(APPLE)
    add_definitions(-DRENDERDOC_PLATFORM_APPLE)
elseif(ENABLE_GGP)
    add_definitions(-DRENDERDOC_PLATFORM_GGP)
    add_definitions(-DRENDERDOC_WINDOWING_GGP)
elseif(UNIX)
    add_definitions(-DRENDERDOC_PLATFORM_LINUX)

    if(ENABLE_XLIB)
        add_definitions(-DRENDERDOC_WINDOWING_XLIB)
    endif()

    if(ENABLE_XCB)
        add_definitions(-DRENDERDOC_WINDOWING_XCB)
    endif()

    if(ENABLE_UNSUPPORTED_EXPERIMENTAL_POSSIBLY_BROKEN_WAYLAND)
        add_definitions(-DRENDERDOC_WINDOWING_WAYLAND)
    endif()
endif()

add_subdirectory(renderdoc)

# these variables are handled within the CMakeLists.txt in qrenderdoc,
# but we need to add it if either is enabled since the swig bindings
# are handled in common
if(ENABLE_QRENDERDOC OR ENABLE_PYRENDERDOC)
    # Make sure Python 3 is found
    set(Python_ADDITIONAL_VERSIONS 3.12 3.11 3.10 3.9 3.8 3.7 3.6 3.5 3.4)
    if(NOT ${CMAKE_VERSION} VERSION_LESS "3.12")
        find_package(Python3 COMPONENTS Interpreter Development REQUIRED)
        set(PYTHON_EXECUTABLE ${Python3_EXECUTABLE})
        set(PYTHON_LIBRARY ${Python3_LIBRARIES})
        set(PYTHON_INCLUDE_DIR ${Python3_INCLUDE_DIRS})
        set(PythonLibs_FOUND ${Python3_FOUND})
        set(PYTHON_VERSION_MINOR ${Python3_VERSION_MINOR})
    else()
        find_package(PythonInterp 3 REQUIRED)
        find_package(PythonLibs 3 REQUIRED)
    endif()

    # we also need python3-config for swig, but it's only for the examples
    # so it doesn't matter which one we find as long as we find something!
    # Importantly it doesn't have to match the version we link against
    if(NOT EXISTS "${PYTHON_EXECUTABLE}-config" AND NOT EXISTS "${PYTHON_EXECUTABLE}.${PYTHON_VERSION_MINOR}-config")
        message(FATAL_ERROR "We require ${PYTHON_EXECUTABLE}-config or ${PYTHON_EXECUTABLE}.${PYTHON_VERSION_MINOR}-config to build swig, please install the python dev package for your system.")
    endif()

    add_subdirectory(qrenderdoc)
endif()

if(ENABLE_RENDERDOCCMD)
    add_subdirectory(renderdoccmd)
endif()

# install documentation files
install (FILES util/LINUX_DIST_README DESTINATION share/doc/renderdoc RENAME README)
install (FILES LICENSE.md DESTINATION share/doc/renderdoc)

message(STATUS "Enabled APIs:")
if(ENABLE_GL)
    if(ENABLE_EGL)
        message(STATUS "  - OpenGL (with additional EGL support)")
    else()
        message(STATUS "  - OpenGL")
    endif()
endif()

if(ENABLE_GLES)
    message(STATUS "  - OpenGL ES (EGL)")
endif()

if(ENABLE_VULKAN)
    message(STATUS "  - Vulkan")
endif()

if(ENABLE_METAL)
    message(STATUS "  - Metal")
endif()

if(UNIX AND NOT ANDROID AND NOT APPLE AND NOT ENABLE_GGP)
    message(STATUS "Enabled Window System Support:")

    if(ENABLE_XLIB)
        message(STATUS "  - XLib")
    endif()

    if(ENABLE_XCB)
        message(STATUS "  - XCB")
    endif()

    if(ENABLE_UNSUPPORTED_EXPERIMENTAL_POSSIBLY_BROKEN_WAYLAND)
        message(STATUS "  - Wayland")
    endif()
endif()

if(INTERNAL_SELF_CAPTURE)
    message(STATUS "INTERNAL: Configuring for self-capture")
endif()

