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
    metal_types_bridge.h
    metal_types.cpp
    metal_types.h
    metal_library.cpp
    metal_library.h
    metal_library_bridge.mm
    metal_function.cpp
    metal_function.h
    metal_function_bridge.mm
    metal_common.cpp
    metal_common.h
    metal_command_queue.cpp
    metal_command_queue.h
    metal_command_queue_bridge.mm
    metal_command_buffer.cpp
    metal_command_buffer.h
    metal_command_buffer_bridge.mm
    metal_render_pipeline_state.cpp
    metal_render_pipeline_state.h
    metal_render_pipeline_state_bridge.mm
    metal_texture.cpp
    metal_texture.h
    metal_texture_bridge.mm
    metal_buffer.cpp
    metal_buffer.h
    metal_buffer_bridge.mm
    metal_blit_command_encoder.cpp
    metal_blit_command_encoder.h
    metal_blit_command_encoder_bridge.mm
    metal_render_command_encoder.cpp
    metal_render_command_encoder.h
    metal_render_command_encoder_bridge.mm
    metal_core.cpp
    metal_core.h
    metal_manager.cpp
    metal_manager.h
    metal_init_state.cpp
    metal_helpers_bridge.mm
    metal_serialise.cpp
    metal_stringise.cpp
    metal_replay.h
    metal_replay.cpp
    official/metal-cpp.h
    official/metal-cpp.cpp)

# Build as C++17
if (CMAKE_VERSION VERSION_LESS "3.1")
    set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17")
else ()
    set (CMAKE_CXX_STANDARD 17)
    set (CMAKE_CXX_STANDARD_REQUIRED ON)
endif ()

add_library(rdoc_metal OBJECT ${sources})
target_compile_definitions(rdoc_metal ${RDOC_DEFINITIONS})
target_include_directories(rdoc_metal ${RDOC_INCLUDES})
