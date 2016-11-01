set(output_dir ${CMAKE_BINARY_DIR}/bin)

# The case here is deliberately not matching the executable name
# This means the custom command doesn't create this output file,
# which causes CMake to rerun this target every time so that Qt
# can do dependency checking and rebuild anything necessary.
add_custom_command(OUTPUT QRenderDoc
	COMMAND qmake "CONFIG+=debug" "DESTDIR=${output_dir}" ${CMAKE_CURRENT_SOURCE_DIR}
	COMMAND MAKEFLAGS= make --no-print-directory)
add_custom_target(build-qrenderdoc ALL DEPENDS QRenderDoc)

install (PROGRAMS ${output_dir}/qrenderdoc DESTINATION bin)

# Install supporting files for file associations etc
install (PROGRAMS share/application-x-renderdoc-capture.svg DESTINATION share/icons/hicolor/scalable/mimetypes/)
install (PROGRAMS share/magic DESTINATION share/doc/renderdoc)
install (PROGRAMS share/renderdoc.desktop DESTINATION share/applications)
install (PROGRAMS share/renderdoc.thumbnailer DESTINATION share/thumbnailers)
install (PROGRAMS share/renderdoc-capture.xml DESTINATION share/mime/packages)
