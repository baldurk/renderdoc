get_git_hash(GIT_COMMIT_HASH)
string(STRIP ${GIT_COMMIT_HASH} GIT_COMMIT_HASH)

option(STATIC_QRENDERDOC "Compile qrenderdoc as static" OFF)
option(QRENDERDOC_NO_CXX11_REGEX "Disable C++11 regex in scintilla" OFF)

set(QMAKE_CONFIG "debug")
set(QMAKE_LDFLAGS "")

if(STATIC_QRENDERDOC)
    set(QMAKE_CONFIG "debug static")
    set(QMAKE_LDFLAGS "-static-libstdc++")
endif()

set(QMAKE_CXXFLAGS "")

if(QRENDERDOC_NO_CXX11_REGEX)
    set(QMAKE_CXXFLAGS "-DNO_CXX11_REGEX=1")
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
     # search for -lrenderdoc here
     "LIBS+=-L${CMAKE_RUNTIME_OUTPUT_DIRECTORY}\n"
     "DEFINES+=GIT_COMMIT_HASH_LITERAL=${GIT_COMMIT_HASH}\n"
     "\n"
     "RENDERDOC_VERSION=${RENDERDOC_VERSION}\n"
     "\n"
     "OSX_ICONFILE=${CMAKE_CURRENT_BINARY_DIR}/RenderDoc.icns\n")
# The case here is deliberately not matching the executable name
# This means the custom command doesn't create this output file,
# which causes CMake to rerun this target every time so that Qt
# can do dependency checking and rebuild anything necessary.
add_custom_command(OUTPUT QRenderDoc
	COMMAND qmake "CMAKE_DIR=${CMAKE_BINARY_DIR}" ${CMAKE_CURRENT_SOURCE_DIR}
	COMMAND $(MAKE)
	DEPENDS RenderDoc.icns)
add_custom_target(build-qrenderdoc ALL DEPENDS QRenderDoc DEPENDS renderdoc)

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
