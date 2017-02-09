set(output_dir ${CMAKE_BINARY_DIR}/bin)

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

# The case here is deliberately not matching the executable name
# This means the custom command doesn't create this output file,
# which causes CMake to rerun this target every time so that Qt
# can do dependency checking and rebuild anything necessary.
add_custom_command(OUTPUT QRenderDoc
	COMMAND qmake "CONFIG+=${QMAKE_CONFIG}" "QMAKE_CC=${CMAKE_C_COMPILER}" "QMAKE_CXX=${CMAKE_CXX_COMPILER}" "QMAKE_LINK=${CMAKE_CXX_COMPILER}" "QMAKE_CXXFLAGS+=${warning_flags}" "QMAKE_CXXFLAGS+=${QMAKE_CXXFLAGS}" "QMAKE_LFLAGS+=${QMAKE_LDFLAGS}" "DEFINES+=GIT_COMMIT_HASH_LITERAL=${GIT_COMMIT_HASH}" "DESTDIR=${output_dir}" ${CMAKE_CURRENT_SOURCE_DIR}
	COMMAND $(MAKE))
add_custom_target(build-qrenderdoc ALL DEPENDS QRenderDoc DEPENDS renderdoc)

install (PROGRAMS ${output_dir}/qrenderdoc DESTINATION bin)

# Install supporting files for file associations etc
install (PROGRAMS share/application-x-renderdoc-capture.svg DESTINATION share/icons/hicolor/scalable/mimetypes/)
install (PROGRAMS share/renderdoc-icon-16x16.xpm DESTINATION share/pixmaps/)
install (PROGRAMS share/renderdoc-icon-32x32.xpm DESTINATION share/pixmaps/)
install (PROGRAMS share/magic DESTINATION share/doc/renderdoc)
install (PROGRAMS share/menu DESTINATION share/menu RENAME renderdoc)
install (PROGRAMS share/renderdoc.desktop DESTINATION share/applications)
install (PROGRAMS share/renderdoc.thumbnailer DESTINATION share/thumbnailers)
install (PROGRAMS share/renderdoc-capture.xml DESTINATION share/mime/packages)

install (CODE "MESSAGE(\"You now need to update some caches.\")")
install (CODE "MESSAGE(\"e.g.\")")
install (CODE "MESSAGE(\"sudo update-desktop-database\")")
install (CODE "MESSAGE(\"sudo update-menus\")")
install (CODE "MESSAGE(\"sudo update-mime-database /usr/share/mime/\")")
install (CODE "MESSAGE(\"sudo gtk-update-icon-cache /usr/share/icons/hicolor/\")")
install (CODE "MESSAGE(\"NB: Your paths may vary.\")")
