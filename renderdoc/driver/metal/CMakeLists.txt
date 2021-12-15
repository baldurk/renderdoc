set(sources
    official/metal-cpp.h
    official/metal-cpp.cpp)

set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17")
add_library(rdoc_metal OBJECT ${sources})
target_compile_definitions(rdoc_metal ${RDOC_DEFINITIONS})
target_include_directories(rdoc_metal ${RDOC_INCLUDES})
