set(sources
    gles_api_tracer.cpp
    gles_chunks.h
    gles_common.cpp
    gles_common.h
    gles_counters.cpp
    gles_debug.cpp
    gles_driver.cpp
    gles_driver.h
    gles_enum.h
    gles_hookset_defs.h
    gles_hookset.h
    gles_manager.cpp
    gles_manager.h
    gles_renderstate.cpp
    gles_renderstate.h
    gles_replay.cpp
    gles_replay.h
    gles_resources.cpp
    gles_resources.h
    gles_shader_refl.cpp
    gles_shader_refl.h
    gles_state_exporter.cpp
    official/gl32.h
    official/gl2ext.h
    official/gl3platform.h
    official/wglext.h
    wrappers/gles_buffer_funcs.cpp
    wrappers/gles_debug_funcs.cpp
    wrappers/gles_draw_funcs.cpp
    wrappers/gles_emulated.cpp
    wrappers/gles_framebuffer_funcs.cpp
    wrappers/gles_get_funcs.cpp
    wrappers/gles_query_funcs.cpp
    wrappers/gles_sampler_funcs.cpp
    wrappers/gles_shader_funcs.cpp
    wrappers/gles_state_funcs.cpp
    wrappers/gles_texture_funcs.cpp
    wrappers/gles_uniform_funcs.cpp
    wrappers/gles_compatibility.cpp
)

if((ANDROID) OR (UNIX))
    list(APPEND sources
        gles_replay_posix.cpp
        gles_hooks_posix.cpp)
elseif(APPLE)
    list(APPEND sources
        gles_replay_apple.cpp
        gles_hooks_apple.cpp)
endif()

add_library(rdoc_gles OBJECT ${sources})
target_compile_definitions(rdoc_gles ${RDOC_DEFINITIONS})
target_include_directories(rdoc_gles ${RDOC_INCLUDES})
