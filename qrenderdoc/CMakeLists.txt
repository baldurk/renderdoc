option(STATIC_QRENDERDOC "Compile qrenderdoc as static" OFF)
option(QRENDERDOC_NO_CXX11_REGEX "Disable C++11 regex in scintilla" OFF)

if( NOT DEFINED QT_QMAKE_EXECUTABLE )
    set( QT_QMAKE_EXECUTABLE "qmake" )
endif()

set(QMAKE_QT5_COMMAND ${QT_QMAKE_EXECUTABLE} CACHE STRING "Command to run to invoke Qt5's qmake. Normally this is qmake, possibly with qtchooser, but might be qmake-qt5")
set(RENDERDOC_SWIG_PACKAGE https://github.com/baldurk/swig/archive/renderdoc-modified-6.zip CACHE STRING "The location where RenderDoc's swig fork source can be found. By default points to the URL on github but can be pointed to a local file.")

set(QMAKE_CONFIG "debug")
set(QMAKE_LDFLAGS "")
set(QMAKE_CXXFLAGS "")
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
    set(QMAKE_CONFIG "debug static")
    set(QMAKE_LDFLAGS "-static-libstdc++ -lutil")
endif()

if(NOT APPLE)
    if(CMAKE_SIZEOF_VOID_P EQUAL 8) # 64-bit
        set(SWIG_FLAGS "-DSWIGWORDSIZE64")
    else() # 32-bit
        set(SWIG_FLAGS "-DSWIGWORDSIZE32")
    endif()
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

if(APPLE)
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
if( CMAKE_MAKE_PROGRAM MATCHES "ninja$" )
    message( "Ninja detected. Some projects compiled using add_custom_command like swig and QRenderDoc target will still be using make" )
    set( GENERATOR_MAKE "make" )
endif()

# Compile our custom SWIG that will do scoped/strong enum classes
ExternalProject_Add(custom_swig
    # using an URL to a zip directly so we don't clone the history etc
    URL ${RENDERDOC_SWIG_PACKAGE}
    BUILD_IN_SOURCE 1
    CONFIGURE_COMMAND ./autogen.sh > /dev/null 2>&1
    COMMAND CC=${SWIG_CONFIGURE_CC} CXX=${SWIG_CONFIGURE_CXX} CFLAGS=-fPIC CXXFLAGS=-fPIC ./configure --with-pcre=yes --prefix=${CMAKE_BINARY_DIR} > /dev/null
    BUILD_COMMAND ${GENERATOR_MAKE} > /dev/null 2>&1
    INSTALL_COMMAND ${GENERATOR_MAKE} install > /dev/null 2>&1)

# Lastly find PySide 2, optionally, for Qt5 Python bindings
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}")

find_package(PySide2)

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
     "\n"
     "OSX_ICONFILE=${CMAKE_CURRENT_BINARY_DIR}/RenderDoc.icns\n")

# Ignore warnings - qmake has no way to do this per-file, so we must do it globally
if(CMAKE_COMPILER_IS_GNUCXX)
    file(APPEND
        ${CMAKE_BINARY_DIR}/qrenderdoc/qrenderdoc_cmake.pri
        "QMAKE_CXXFLAGS+=-Wno-unknown-warning -Wno-implicit-fallthrough -Wno-cast-function-type -Wno-stringop-truncation\n")
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

if(PYSIDE2_FOUND)
    # Add configuration for PySide2
    file(APPEND
         ${CMAKE_BINARY_DIR}/qrenderdoc/qrenderdoc_cmake.pri
         "DEFINES+=PYSIDE2_ENABLED=1\n"
         "DEFINES+=PYSIDE2_SYS_PATH=${PYSIDE2_PYTHON_PATH}\n"
         "INCLUDEPATH+=${PYSIDE2_INCLUDE_DIR}/PySide2\n"
         "INCLUDEPATH+=${PYSIDE2_INCLUDE_DIR}/PySide2/QtCore\n"
         "INCLUDEPATH+=${PYSIDE2_INCLUDE_DIR}/PySide2/QtGui\n"
         "INCLUDEPATH+=${PYSIDE2_INCLUDE_DIR}/PySide2/QtWidgets\n"
         "INCLUDEPATH+=${PYSIDE2_INCLUDE_DIR}/shiboken2\n"
         "LIBS+=-L${PYSIDE2_LIBRARY_DIR}\n"
         "LIBS+=-lshiboken2\n")
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

    add_custom_command(OUTPUT ${swig_file}.py.c
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
            COMMAND ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/include-bin ${swig_file}.py ${swig_file}.py.c
            DEPENDS ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/include-bin
            DEPENDS ${swig_file}_python.cxx)

    list(APPEND swig_output ${swig_file}_python.cxx)
    list(APPEND swig_output ${swig_file}.py.c)
endforeach()

add_custom_target(swig-bindings ALL DEPENDS ${swig_output})

if(ENABLE_QRENDERDOC)

# The case here is deliberately not matching the executable name
# This means the custom command doesn't create this output file,
# which causes CMake to rerun this target every time so that Qt
# can do dependency checking and rebuild anything necessary.
add_custom_command(OUTPUT QRenderDoc
    COMMAND ${QMAKE_QT5_COMMAND} "CMAKE_DIR=${CMAKE_BINARY_DIR}" ${CMAKE_CURRENT_SOURCE_DIR}
    COMMAND ${GENERATOR_MAKE}
    DEPENDS RenderDoc.icns)
add_custom_target(build-qrenderdoc ALL DEPENDS QRenderDoc DEPENDS renderdoc DEPENDS swig-bindings)

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
if(ENABLE_PYRENDERDOC AND UNIX AND NOT APPLE)
    add_subdirectory(Code/pyrenderdoc)
endif()
