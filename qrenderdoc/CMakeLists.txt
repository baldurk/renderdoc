set(output_dir ${CMAKE_BINARY_DIR}/bin)

include(ExternalProject)
ExternalProject_Add(qrenderdoc
    DEPENDS renderdoc
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}
    CONFIGURE_COMMAND qmake "CONFIG+=debug" "DESTDIR=${output_dir}" ${CMAKE_CURRENT_SOURCE_DIR})
