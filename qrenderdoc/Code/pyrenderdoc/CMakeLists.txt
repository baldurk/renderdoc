# include paths for qrenderdoc, the internal renderdoc API, and Python
set(MODULE_INCLUDES
    PRIVATE ${CMAKE_SOURCE_DIR}/qrenderdoc
    PRIVATE ${CMAKE_SOURCE_DIR}/renderdoc/api/replay
    PRIVATE ${PYTHON_INCLUDE_DIR})
  
# preprocessor definitions for compiling
set(MODULE_DEFINES
    PRIVATE -DRENDERDOC_PLATFORM_LINUX)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unknown-warning-option -Wno-unused-private-field -Wno-shadow -Wno-shorten-64-to-32")
endif()

if(CMAKE_COMPILER_IS_GNUCXX)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-cast-function-type -Wno-stringop-truncation")
endif()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-deprecated-declarations")

# Set up rpath to find librenderdoc.so
set(CMAKE_SKIP_BUILD_RPATH TRUE)
set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
if (APPLE)
    set(CMAKE_MACOSX_RPATH TRUE)
    set(CMAKE_INSTALL_RPATH "@loader_path/../lib")
    set(CMAKE_BUILD_WITH_INSTALL_NAME_DIR TRUE)
else()
    set(CMAKE_INSTALL_RPATH "$ORIGIN/:$ORIGIN/../lib${LIB_SUFFIX}/${LIB_SUBFOLDER_TRAIL_SLASH}")
endif()

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
    ${CMAKE_BINARY_DIR}/qrenderdoc/renderdoc_python.cxx
    ${CMAKE_BINARY_DIR}/qrenderdoc/qrenderdoc_python.cxx
    pyrenderdoc_stub.cpp
    qrenderdoc_stub.cpp)

set_source_files_properties(${CMAKE_BINARY_DIR}/qrenderdoc/qrenderdoc_python.cxx
                            PROPERTIES GENERATED TRUE)

target_compile_definitions(_qrenderdoc ${MODULE_DEFINES})
target_include_directories(_qrenderdoc ${MODULE_INCLUDES})
target_link_libraries(_qrenderdoc PRIVATE renderdoc)

# Python expects .so extension when importing shared libraries on non-Windows platform
if(APPLE)
    set_target_properties(_renderdoc PROPERTIES SUFFIX ".so")
    set_target_properties(_qrenderdoc PROPERTIES SUFFIX ".so")
endif()

# Don't prefix with lib, python expects a bare .so.
# Also rename to non-underscore due to this (We couldn't call the
# python library 'renderdoc' since that would clash with the
# prefixed target)
set_target_properties(_renderdoc PROPERTIES PREFIX "")
set_target_properties(_qrenderdoc PROPERTIES PREFIX "")
set_target_properties(_renderdoc PROPERTIES OUTPUT_NAME "renderdoc")
set_target_properties(_qrenderdoc PROPERTIES OUTPUT_NAME "qrenderdoc")

# Make sure we build after the wrappers are generated
add_dependencies(_renderdoc swig-bindings)
add_dependencies(_qrenderdoc swig-bindings)
