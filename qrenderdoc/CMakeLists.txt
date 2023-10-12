option(STATIC_QRENDERDOC "Compile qrenderdoc as static" OFF)
option(QRENDERDOC_NO_CXX11_REGEX "Disable C++11 regex in scintilla" OFF)

if( NOT DEFINED QT_QMAKE_EXECUTABLE )
    if( DEFINED QMAKE_EXECUTABLE )
        set( QT_QMAKE_EXECUTABLE "${QMAKE_EXECUTABLE}" )
    else()
        set( QT_QMAKE_EXECUTABLE "qmake" )
    endif()
endif()

set(QMAKE_QT5_COMMAND ${QT_QMAKE_EXECUTABLE} CACHE STRING "Command to run to invoke Qt5's qmake. Normally this is qmake, possibly with qtchooser, but might be qmake-qt5")
set(RENDERDOC_SWIG_PACKAGE https://github.com/baldurk/swig/archive/renderdoc-modified-7.zip CACHE STRING "The location where RenderDoc's swig fork source can be found. By default points to the URL on github but can be pointed to a local file.")

# Only check qt version if we're building qrenderdoc
if(ENABLE_QRENDERDOC)

execute_process(
    COMMAND ${QMAKE_QT5_COMMAND} -query QT_VERSION
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE QT_VERSION
    ERROR_VARIABLE QT_VERSION_STDERR
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_STRIP_TRAILING_WHITESPACE
    )

# we need the trailing .999 since 5.5.1 would be VERSION_GREATER than 5.5 which implicitly means 5.5.0
if (QT_VERSION VERSION_GREATER 5.5.999)
    message(STATUS "Building using Qt ${QT_VERSION}")
else()
    message(FATAL_ERROR "${QMAKE_QT5_COMMAND} -query QT_VERSION returned: \"${QT_VERSION}\". Error output was: \"${QT_VERSION_STDERR}\". We require Qt >= 5.6 and for the qmake executable to run Qt5. Check your distribution for how to configure Qt5, you may need to set the QT_SELECT environment variable or customise the cmake variable QMAKE_QT5_COMMAND to qmake-qt5")
endif()

endif() # if(ENABLE_QRENDERDOC)

set(QMAKE_CONFIG "debug")
set(QMAKE_LDFLAGS "")
set(QMAKE_CXXFLAGS "")
set(QMAKE_RPATHDIR "")
set(SWIG_FLAGS "")

if(ENABLE_ASAN)
    set (QMAKE_CXXFLAGS "-fsanitize=address -fno-omit-frame-pointer")
    set (QMAKE_LDFLAGS "-fsanitize=address")
endif()

if(ENABLE_TSAN)
    set (QMAKE_CXXFLAGS "-fsanitize=thread -fno-omit-frame-pointer")
    set (QMAKE_LDFLAGS "-fsanitize=thread")
endif()

if(STATIC_QRENDERDOC)
    set(QMAKE_CXXFLAGS "-DSTATIC_QRENDERDOC=1")
    set(QMAKE_CONFIG "${QMAKE_CONFIG} static")
    set(QMAKE_LDFLAGS "-lutil")
endif()

if(NOT APPLE)
    if(CMAKE_SIZEOF_VOID_P EQUAL 8) # 64-bit
        set(SWIG_FLAGS "-DSWIGWORDSIZE64")
    else() # 32-bit
        set(SWIG_FLAGS "-DSWIGWORDSIZE32")
    endif()
endif()

if(RELEASE_MODE)
    set(QMAKE_CONFIG "release")
    set(QMAKE_CXXFLAGS "-DRELEASE=1 ${QMAKE_CXXFLAGS}")
endif()

if(QRENDERDOC_NO_CXX11_REGEX)
    set(QMAKE_CXXFLAGS "-DNO_CXX11_REGEX=1 ${QMAKE_CXXFLAGS}")
endif()

if(ENABLE_GL)
    set(QMAKE_CXXFLAGS "${QMAKE_CXXFLAGS} -DRENDERDOC_SUPPORT_GL")
endif()

if(ENABLE_GLES)
    set(QMAKE_CXXFLAGS "${QMAKE_CXXFLAGS} -DRENDERDOC_SUPPORT_GLES")
endif()

if(ENABLE_VULKAN)
    set(QMAKE_CXXFLAGS "${QMAKE_CXXFLAGS} -DRENDERDOC_SUPPORT_VULKAN")
endif()

if(ENABLE_METAL)
    set(QMAKE_CXXFLAGS "${QMAKE_CXXFLAGS} -DRENDERDOC_SUPPORT_METAL")
endif()

if(APPLE)
    set(QMAKE_RPATHDIR "@executable_path/../lib")
    add_custom_command(OUTPUT RenderDoc.icns
        COMMAND echo Creating OS X Icon
        COMMAND mkdir -p RenderDoc.iconset
        COMMAND qlmanage -t -s 1024 -o . ${CMAKE_CURRENT_SOURCE_DIR}/Resources/logo.svg
        COMMAND sips -z 16 16     logo.svg.png --out RenderDoc.iconset/icon_16x16.png
        COMMAND sips -z 32 32     logo.svg.png --out RenderDoc.iconset/icon_16x16@2x.png
        COMMAND sips -z 32 32     logo.svg.png --out RenderDoc.iconset/icon_32x32.png
        COMMAND sips -z 64 64     logo.svg.png --out RenderDoc.iconset/icon_32x32@2x.png
        COMMAND sips -z 128 128   logo.svg.png --out RenderDoc.iconset/icon_128x128.png
        COMMAND sips -z 256 256   logo.svg.png --out RenderDoc.iconset/icon_128x128@2x.png
        COMMAND sips -z 256 256   logo.svg.png --out RenderDoc.iconset/icon_256x256.png
        COMMAND sips -z 512 512   logo.svg.png --out RenderDoc.iconset/icon_256x256@2x.png
        COMMAND sips -z 512 512   logo.svg.png --out RenderDoc.iconset/icon_512x512.png
        COMMAND sips -z 1024 1024 logo.svg.png --out RenderDoc.iconset/icon_512x512@2x.png
        COMMAND iconutil -c icns RenderDoc.iconset
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/Resources/logo.svg)
else()
    add_custom_command(OUTPUT RenderDoc.icns COMMAND touch RenderDoc.icns)
endif()

include(ExternalProject)

# Need bison for swig
find_package(BISON)

set(SWIG_CONFIGURE_CC ${CMAKE_C_COMPILER})
set(SWIG_CONFIGURE_CXX ${CMAKE_CXX_COMPILER})

# macOS 10.14+ ships broken compilers, need to disable CC/CXX inheritance
if(APPLE)
    set(SWIG_CONFIGURE_CC "")
    set(SWIG_CONFIGURE_CXX "")
endif()

set( GENERATOR_MAKE "$(MAKE)" )
set( GENERATOR_MAKE_PARAMS "" )
if( NOT CMAKE_MAKE_PROGRAM MATCHES "make$" )
    message( "Non make generator detected. Some projects compiled using add_custom_command like swig and QRenderDoc target will still be using make" )
    set( GENERATOR_MAKE "make" )

    include(ProcessorCount)
    ProcessorCount(PROC_COUNT)
    if(NOT PROC_COUNT EQUAL 0)
        message( "Running make with -j${PROC_COUNT} for parallel build" )
        set(GENERATOR_MAKE_PARAMS "-j${PROC_COUNT}")
    else()
        message(WARNING "!!!! Couldn't determine processor count. Make will be run non-parallel !!!!")
    endif()
endif()

if(APPLE AND CMAKE_GENERATOR STREQUAL "Xcode")
    if("${SHELL_CAPTURED_PATH}" STREQUAL "")
        set(SHELL_CAPTURED_PATH $ENV{PATH} CACHE STRING "Shell PATH" FORCE)
    endif()
    message(STATUS "Using custom path for targets custom_swig and QRenderDoc")
    set(SET_SYSTEM_PATH_COMMAND ${CMAKE_COMMAND} -E env PATH=${SHELL_CAPTURED_PATH})
endif()

if(NOT ${CMAKE_VERSION} VERSION_LESS "3.24")
    cmake_policy(SET CMP0135 NEW)
endif()

# Compile our custom SWIG that will do scoped/strong enum classes
ExternalProject_Add(custom_swig
    # using an URL to a zip directly so we don't clone the history etc
    URL ${RENDERDOC_SWIG_PACKAGE}
    BUILD_IN_SOURCE 1
    CONFIGURE_COMMAND ${SET_SYSTEM_PATH_COMMAND} ./autogen.sh > /dev/null 2>&1
    COMMAND CC=${SWIG_CONFIGURE_CC} CXX=${SWIG_CONFIGURE_CXX} CFLAGS=-fPIC CXXFLAGS=-fPIC ${SET_SYSTEM_PATH_COMMAND} ./configure --with-pcre=yes --prefix=${CMAKE_BINARY_DIR} > /dev/null
    BUILD_COMMAND ${GENERATOR_MAKE} ${GENERATOR_MAKE_PARAMS} > /dev/null 2>&1
    INSTALL_COMMAND ${GENERATOR_MAKE} install > /dev/null 2>&1)

find_package(Shiboken2 QUIET)
find_package(PySide2 QUIET)

set(PYTHON_LINK "${PYTHON_LIBRARY}")

# ensure we link the whole python library so that modules have all the exports they need
if(STATIC_QRENDERDOC)
    set(PYTHON_LINK "-rdynamic -Wl,--whole-archive ${PYTHON_LINK} -Wl,--no-whole-archive")
endif()

# Output our configuration for qmake. We output this to a separate file so that
# the user can then open the qrenderdoc.pro in qt creator and be able to build
# with these configuration entries propagated for e.g. linking against libraries
# and finding dependencies from the cmake build
file(WRITE
     ${CMAKE_BINARY_DIR}/qrenderdoc/qrenderdoc_cmake.pri
     "CONFIG+=${QMAKE_CONFIG}\n"
     "\n"
     "QMAKE_CC=${CMAKE_C_COMPILER}\n"
     "QMAKE_CXX=${CMAKE_CXX_COMPILER}\n"
     "QMAKE_LINK=${CMAKE_CXX_COMPILER}\n"
     "QMAKE_CXXFLAGS+=${warning_flags}\n"
     "QMAKE_CXXFLAGS+=${QMAKE_CXXFLAGS}\n"
     "QMAKE_LFLAGS+=${QMAKE_LDFLAGS}\n"
     "QMAKE_RPATHDIR+=${QMAKE_RPATHDIR}\n"
     "\n"
     "LIB_SUFFIX=${LIB_SUFFIX}\n"
     "LIB_SUBFOLDER_TRAIL_SLASH=${LIB_SUBFOLDER_TRAIL_SLASH}\n"
     "\n"
     # search for -lrenderdoc here
     "LIBS+=-L${CMAKE_LIBRARY_OUTPUT_DIRECTORY}\n"
     "\n"
     # include and link against python
     "INCLUDEPATH+=${PYTHON_INCLUDE_DIR}\n"
     "LIBS+=${PYTHON_LINK}\n"
     "\n"
     "RENDERDOC_VERSION=${RENDERDOC_VERSION}\n"
     "\n")

if(APPLE)
    file(APPEND
        ${CMAKE_BINARY_DIR}/qrenderdoc/qrenderdoc_cmake.pri
        "OSX_ICONFILE=${CMAKE_CURRENT_BINARY_DIR}/RenderDoc.icns\n"
        "QMAKE_MACOSX_DEPLOYMENT_TARGET=${CMAKE_OSX_DEPLOYMENT_TARGET}\n")
endif()

# Ignore warnings - qmake has no way to do this per-file, so we must do it globally
if(CMAKE_COMPILER_IS_GNUCXX)
    file(APPEND
        ${CMAKE_BINARY_DIR}/qrenderdoc/qrenderdoc_cmake.pri
        "QMAKE_CXXFLAGS+=-Wno-unknown-warning -Wno-cast-function-type -Wno-stringop-truncation\n")
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    file(APPEND
        ${CMAKE_BINARY_DIR}/qrenderdoc/qrenderdoc_cmake.pri
        "QMAKE_CXXFLAGS+=-Wno-shadow -Wno-shorten-64-to-32\n")
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER 3.9)
    file(APPEND
        ${CMAKE_BINARY_DIR}/qrenderdoc/qrenderdoc_cmake.pri
        "QMAKE_CXXFLAGS+=-Wno-comma\n")
endif()

if(ENABLE_WAYLAND)
    message(WARNING "!!!! Using the Wayland Qt platform in qrenderdoc is NOT SUPPORTED !!!!")
    file(APPEND
        ${CMAKE_BINARY_DIR}/qrenderdoc/qrenderdoc_cmake.pri
        "DEFINES+=RENDERDOC_WINDOWING_WAYLAND\n")
endif()

# propagate build version info. Lots of escaping needed here to pass ""s into the define value
if(BUILD_VERSION_STABLE)
    file(APPEND
         ${CMAKE_BINARY_DIR}/qrenderdoc/qrenderdoc_cmake.pri
         "DEFINES+=RENDERDOC_STABLE_BUILD=1\n")
endif()

if(NOT BUILD_VERSION_DIST_NAME STREQUAL "")
    file(APPEND
         ${CMAKE_BINARY_DIR}/qrenderdoc/qrenderdoc_cmake.pri
         "DEFINES+=DISTRIBUTION_NAME='\\\\\"${BUILD_VERSION_DIST_NAME}\\\\\"'\n")
endif()

if(NOT BUILD_VERSION_DIST_VER STREQUAL "")
    file(APPEND
         ${CMAKE_BINARY_DIR}/qrenderdoc/qrenderdoc_cmake.pri
         "DEFINES+=DISTRIBUTION_VERSION='\\\\\"${BUILD_VERSION_DIST_VER}\\\\\"'\n")
endif()

if(NOT BUILD_VERSION_DIST_CONTACT STREQUAL "")
    file(APPEND
         ${CMAKE_BINARY_DIR}/qrenderdoc/qrenderdoc_cmake.pri
         "DEFINES+=DISTRIBUTION_CONTACT='\\\\\"${BUILD_VERSION_DIST_CONTACT}\\\\\"'\n")
endif()

option(QRENDERDOC_ENABLE_PYSIDE2 "Enable PySide2 if found" ON)

if(NOT QRENDERDOC_ENABLE_PYSIDE2)
    message(STATUS "PySide2 integration disabled")
elseif(PySide2_FOUND AND Shiboken2_FOUND AND TARGET Shiboken2::libshiboken AND TARGET PySide2::pyside2)
    message(STATUS "Building with PySide2 ${PySide2_VERSION} from ${PySide2_DIR}")

    if(NOT PYTHONLIBS_VERSION_STRING MATCHES "${SHIBOKEN_PYTHON_VERSION_MAJOR}.${SHIBOKEN_PYTHON_VERSION_MINOR}")
        message(FATAL_ERROR "Shiboken2 that we found was linked against python ${SHIBOKEN_PYTHON_VERSION_MAJOR}.${SHIBOKEN_PYTHON_VERSION_MINOR} but the python library we're linking against is ${PYTHONLIBS_VERSION_STRING}. Ensure these versions are identical or disable PySide2 compilation with QRENDERDOC_ENABLE_PYSIDE2.")
    endif()

    get_target_property(SHIBOKEN2_INCLUDE_DIRS Shiboken2::libshiboken INTERFACE_INCLUDE_DIRECTORIES)
    get_target_property(PYSIDE2_INCLUDE_DIRS PySide2::pyside2 INTERFACE_INCLUDE_DIRECTORIES)

    # append each include dir
    foreach(DIR ${SHIBOKEN2_INCLUDE_DIRS})
        file(APPEND
             ${CMAKE_BINARY_DIR}/qrenderdoc/qrenderdoc_cmake.pri
             "INCLUDEPATH+=${DIR}\n")
    endforeach()

    foreach(DIR ${PYSIDE2_INCLUDE_DIRS})
        file(APPEND
             ${CMAKE_BINARY_DIR}/qrenderdoc/qrenderdoc_cmake.pri
             "INCLUDEPATH+=${DIR}\n")
    endforeach()

    # Add configuration for PySide2
    file(APPEND
         ${CMAKE_BINARY_DIR}/qrenderdoc/qrenderdoc_cmake.pri
         "DEFINES+=PYSIDE2_ENABLED=1\n"
         "DEFINES+=PYSIDE2_SYS_PATH=${PYSIDE_PYTHONPATH}\n"
         "LIBS+=-lshiboken2${SHIBOKEN_PYTHON_SHARED_LIBRARY_SUFFIX}\n")
else()
    message(STATUS "PySide2 not found - Qt will not be accessible in python scripting. See https://github.com/baldurk/renderdoc/wiki/PySide2")

    file(APPEND
         ${CMAKE_BINARY_DIR}/qrenderdoc/qrenderdoc_cmake.pri
         "DEFINES+=PYSIDE2_ENABLED=0\n")
endif()

# generate the SWIG interface files
set(swig_interfaces
    Code/pyrenderdoc/renderdoc.i
    Code/pyrenderdoc/qrenderdoc.i)

set(swig_output)

file(GLOB RDOC_REPLAY_FILES ${CMAKE_SOURCE_DIR}/renderdoc/api/replay/*.h)
file(GLOB QRD_INTERFACE_FILES ${CMAKE_CURRENT_SOURCE_DIR}/Code/Interface/*.h)

list(SORT RDOC_REPLAY_FILES)
list(SORT QRD_INTERFACE_FILES)

foreach(in ${swig_interfaces})
    get_filename_component(swig_file ${in} NAME_WE)

    add_custom_command(OUTPUT ${swig_file}_python.cxx ${swig_file}.py
            COMMAND ${CMAKE_BINARY_DIR}/bin/swig -v -Wextra -Werror ${SWIG_FLAGS} -O -c++ -python -modern -interface ${swig_file} -modernargs -enumclass -fastunpack -py3 -builtin -I${CMAKE_CURRENT_SOURCE_DIR} -I${CMAKE_SOURCE_DIR}/renderdoc/api/replay -outdir ${CMAKE_CURRENT_BINARY_DIR} -o ${CMAKE_CURRENT_BINARY_DIR}/${swig_file}_python.cxx ${CMAKE_CURRENT_SOURCE_DIR}/${in}
            DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${in}
            DEPENDS custom_swig
            DEPENDS ${RDOC_REPLAY_FILES}
            DEPENDS ${QRD_INTERFACE_FILES})

    list(APPEND swig_output ${swig_file}_python.cxx)
endforeach()

add_custom_target(swig-bindings ALL DEPENDS ${swig_output})

if(ENABLE_QRENDERDOC)

set(QRD_DEPENDENCIES RenderDoc.icns)

if(APPLE AND CMAKE_GENERATOR STREQUAL "Xcode")
    file(GLOB_RECURSE CODE_FILES Code/*.h Code/*.cpp Code/*.mm Code/*.i)
    list(SORT CODE_FILES COMPARE FILE_BASENAME)
    list(APPEND QRD_XCODE_SOURCES ${CODE_FILES})

    file(GLOB_RECURSE STYLES_FILES Styles/*.h Styles/*.cpp)
    list(SORT STYLES_FILES COMPARE FILE_BASENAME)
    list(APPEND QRD_XCODE_SOURCES ${STYLES_FILES})

    file(GLOB_RECURSE WIDGETS_FILES Widgets/*.h Widgets/*.cpp)
    list(SORT WIDGETS_FILES COMPARE FILE_BASENAME)
    list(APPEND QRD_XCODE_SOURCES ${WIDGETS_FILES})

    file(GLOB_RECURSE WINDOWS_FILES Windows/*.h Windows/*.cpp Windows/*.ui)
    list(SORT WINDOWS_FILES COMPARE FILE_BASENAME)
    list(APPEND QRD_XCODE_SOURCES ${WINDOWS_FILES})

    file(GLOB_RECURSE THIRDPARTY_FILES 
        3rdParty/toolwindowmanager/*.h
        3rdParty/toolwindowmanager/*.cpp
        3rdParty/flowlayout/*.h
        3rdParty/flowlayout/*.cpp
        3rdParty/scintilla/*.h
        3rdParty/scintilla/*.cxx
        3rdParty/scintilla/*.cpp)
    list(SORT THIRDPARTY_FILES COMPARE FILE_BASENAME)
    list(APPEND QRD_XCODE_SOURCES ${THIRDPARTY_FILES})

    list(APPEND QRD_DEPENDENCIES ${CMAKE_BINARY_DIR}/qrenderdoc/renderdoc_python.cxx)
    list(APPEND QRD_DEPENDENCIES ${CMAKE_BINARY_DIR}/qrenderdoc/qrenderdoc_python.cxx)
    list(APPEND QRD_DEPENDENCIES ${QRD_XCODE_SOURCES})
endif()

# The case here is deliberately not matching the executable name
# This means the custom command doesn't create this output file,
# which causes CMake to rerun this target every time so that Qt
# can do dependency checking and rebuild anything necessary.
# Xcode does not match this behaviour. Add broad brush file dependencies.
# Xcode has an option to match this behaviour (not exposed by cmake).
add_custom_command(OUTPUT QRenderDoc
    COMMAND ${SET_SYSTEM_PATH_COMMAND} ${QMAKE_QT5_COMMAND} "CMAKE_DIR=${CMAKE_BINARY_DIR}" ${CMAKE_CURRENT_SOURCE_DIR}
    COMMAND ${GENERATOR_MAKE} ${GENERATOR_MAKE_PARAMS}
    DEPENDS ${QRD_DEPENDENCIES})
add_custom_target(build-qrenderdoc ALL DEPENDS QRenderDoc DEPENDS renderdoc DEPENDS swig-bindings)

if(APPLE AND CMAKE_GENERATOR STREQUAL "Xcode")
    set_target_properties(build-qrenderdoc PROPERTIES XCODE_SCHEME_DEBUG_DOCUMENT_VERSIONING OFF)
    set_target_properties(build-qrenderdoc PROPERTIES XCODE_SCHEME_EXECUTABLE ${CMAKE_BINARY_DIR}/bin/qrenderdoc.app)
    target_sources(build-qrenderdoc PRIVATE ${QRD_XCODE_SOURCES})
    source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${CODE_FILES})
    source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${STYLES_FILES})
    source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${WIDGETS_FILES})
    source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${WINDOWS_FILES})
    source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${THIRDPARTY_FILES})
endif()

install (PROGRAMS ${CMAKE_BINARY_DIR}/bin/qrenderdoc DESTINATION bin)

# Install supporting files for file associations etc
install (FILES share/application-x-renderdoc-capture.svg DESTINATION share/icons/hicolor/scalable/mimetypes/)
install (FILES share/renderdoc-icon-16x16.xpm DESTINATION share/pixmaps/)
install (FILES share/renderdoc-icon-32x32.xpm DESTINATION share/pixmaps/)
install (FILES share/magic DESTINATION share/doc/renderdoc)
install (FILES share/menu DESTINATION share/menu RENAME renderdoc)
install (FILES share/renderdoc.desktop DESTINATION share/applications)
install (FILES share/renderdoc.thumbnailer DESTINATION share/thumbnailers)
install (FILES share/renderdoc-capture.xml DESTINATION share/mime/packages)

install (CODE "MESSAGE(\"You now need to update some caches.\")")
install (CODE "MESSAGE(\"e.g.\")")
install (CODE "MESSAGE(\"sudo update-desktop-database\")")
install (CODE "MESSAGE(\"sudo update-menus\")")
install (CODE "MESSAGE(\"sudo update-mime-database /usr/share/mime/\")")
install (CODE "MESSAGE(\"sudo gtk-update-icon-cache /usr/share/icons/hicolor/\")")
install (CODE "MESSAGE(\"NB: Your paths may vary.\")")

endif() # if(ENABLE_QRENDERDOC)

# Build python modules - primarily used for constructing documentation
if(ENABLE_PYRENDERDOC AND UNIX)
    add_subdirectory(Code/pyrenderdoc)
endif()
