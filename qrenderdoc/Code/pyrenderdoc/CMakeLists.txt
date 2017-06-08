# Build as C++14 for the python bindings template-fu
if (CMAKE_VERSION VERSION_LESS "3.1")
    set (CMAKE_CXX_FLAGS "--std=c++14 ${CMAKE_CXX_FLAGS}")
else ()
    set (CMAKE_CXX_STANDARD 14)
endif ()

# Fetch the include and libs parameters for Qt
execute_process(
      COMMAND pkg-config --cflags Qt5Widgets Qt5Core Qt5Gui
      OUTPUT_VARIABLE QT5_INCLUDE
      OUTPUT_STRIP_TRAILING_WHITESPACE)

execute_process(
      COMMAND pkg-config --libs Qt5Widgets Qt5Core Qt5Gui
      OUTPUT_VARIABLE QT5_LIBS
      OUTPUT_STRIP_TRAILING_WHITESPACE)

if(STATIC_QRENDERDOC)
    set(QT5_LIBS "${QT5_LIBS} -lqtpcre")
endif()

# include paths for qrenderdoc, the internal renderdoc API, and Python
set(MODULE_INCLUDES
    PRIVATE ${CMAKE_SOURCE_DIR}/qrenderdoc
    PRIVATE ${CMAKE_SOURCE_DIR}/renderdoc/api/replay
    PRIVATE ${PYTHON_INCLUDE_DIR})
  
# preprocessor definitions for compiling
set(MODULE_DEFINES
    PRIVATE -DRENDERDOC_PLATFORM_LINUX)

# Set up rpath to find librenderdoc.so
set(CMAKE_SKIP_BUILD_RPATH TRUE)
set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
set(CMAKE_INSTALL_RPATH "$ORIGIN/:$ORIGIN/../lib/")

# Add python library
set (CMAKE_SHARED_LINKER_FLAGS "${PYTHON_LIBRARY} ${CMAKE_SHARED_LINKER_FLAGS}")

add_library(_renderdoc SHARED
    ${CMAKE_BINARY_DIR}/qrenderdoc/renderdoc_python.cxx
    pyrenderdoc_stub.cpp)

set_source_files_properties(${CMAKE_BINARY_DIR}/qrenderdoc/renderdoc_python.cxx
                            PROPERTIES GENERATED TRUE)

target_compile_definitions(_renderdoc ${MODULE_DEFINES})
target_include_directories(_renderdoc ${MODULE_INCLUDES})
target_link_libraries(_renderdoc PRIVATE renderdoc)
            
add_library(_qrenderdoc SHARED
    ${CMAKE_BINARY_DIR}/qrenderdoc/qrenderdoc_python.cxx
    ../Interface/CommonPipelineState.cpp
    ../Interface/PersistantConfig.cpp
    ../Interface/QRDInterface.cpp
    ../Interface/RemoteHost.cpp
    pyrenderdoc_stub.cpp
    qrenderdoc_stub.cpp)

set_source_files_properties(${CMAKE_BINARY_DIR}/qrenderdoc/qrenderdoc_python.cxx
                            PROPERTIES GENERATED TRUE)

# Add in Qt includes and libraries
set (CMAKE_CXX_FLAGS "${QT5_INCLUDE} ${CMAKE_CXX_FLAGS}")
set (CMAKE_SHARED_LINKER_FLAGS "${QT5_LIBS} ${CMAKE_SHARED_LINKER_FLAGS}")
set (MODULE_LIBRARIES
    PRIVATE renderdoc
    ${QT5_LIBS})

target_compile_definitions(_qrenderdoc ${MODULE_DEFINES})
target_include_directories(_qrenderdoc ${MODULE_INCLUDES})
target_link_libraries(_qrenderdoc ${MODULE_LIBRARIES})

# Don't prefix with lib, python expects a bare .so
set_target_properties(_renderdoc PROPERTIES PREFIX "")
set_target_properties(_qrenderdoc PROPERTIES PREFIX "")

# Make sure we build after the wrappers are generated
add_dependencies(_renderdoc build-qrenderdoc)
add_dependencies(_qrenderdoc build-qrenderdoc)

# Copy in the .py wrappers from qrenderdoc's build
add_custom_command(
        TARGET _renderdoc POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
                ${CMAKE_BINARY_DIR}/qrenderdoc/renderdoc.py
                ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/renderdoc.py)

add_custom_command(
        TARGET _qrenderdoc POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
                ${CMAKE_BINARY_DIR}/qrenderdoc/qrenderdoc.py
                ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/qrenderdoc.py)

