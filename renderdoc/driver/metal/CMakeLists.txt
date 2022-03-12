set(sources
    metal_dispatch_table_bridge.mm
    metal_dispatch_table_bridge.h
    metal_hook_bridge.mm
    metal_hook.cpp
    metal_hook.h
    metal_device.cpp
    metal_device.h
    metal_device_bridge.mm
    metal_resources.cpp
    metal_resources.h
    metal_types_bridge.mm
    metal_types_bridge.h
    metal_types.h
    metal_library.cpp
    metal_library.h
    metal_library_bridge.mm
    metal_function.cpp
    metal_function.h
    metal_function_bridge.mm
    metal_common.h
    official/metal-cpp.h
    official/metal-cpp.cpp)

set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17")
add_library(rdoc_metal OBJECT ${sources})
target_compile_definitions(rdoc_metal ${RDOC_DEFINITIONS})
target_include_directories(rdoc_metal ${RDOC_INCLUDES})
