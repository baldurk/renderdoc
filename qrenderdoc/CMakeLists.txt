set(output_dir ${CMAKE_BINARY_DIR}/bin)

# The case here is deliberately not matching the executable name
# This means the custom command doesn't create this output file,
# which causes CMake to rerun this target every time so that Qt
# can do dependency checking and rebuild anything necessary.
add_custom_command(OUTPUT QRenderDoc
	COMMAND qmake "CONFIG+=debug" "DESTDIR=${output_dir}" ${CMAKE_CURRENT_SOURCE_DIR}
	COMMAND MAKEFLAGS= make --no-print-directory)
add_custom_target(build-qrenderdoc ALL DEPENDS QRenderDoc)
