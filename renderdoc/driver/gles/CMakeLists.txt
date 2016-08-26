set(sources
    gles_driver.cpp
    gles_manager.cpp
    gles_replay.cpp
    wrappers/gles_state_funcs.cpp
    )

set(others
    official/glcorearb.h
    official/glext.h
    official/glxext.h
    official/wglext.h
    wrappers/gl_buffer_funcs.cpp
    wrappers/gl_debug_funcs.cpp
    wrappers/gl_draw_funcs.cpp
    wrappers/gl_emulated.cpp
    wrappers/gl_framebuffer_funcs.cpp
    wrappers/gl_get_funcs.cpp
    wrappers/gl_query_funcs.cpp
    wrappers/gl_sampler_funcs.cpp
    wrappers/gl_shader_funcs.cpp
    wrappers/gl_state_funcs.cpp
    wrappers/gl_texture_funcs.cpp
    wrappers/gl_uniform_funcs.cpp)

if(UNIX)
    list(APPEND sources
        gles_hooks_linux.cpp
        gles_replay_linux.cpp
        #gles_hooks_linux_egl.cpp
    )
endif()

add_library(rdoc_gles OBJECT ${sources})
target_compile_definitions(rdoc_gles ${RDOC_DEFINITIONS})
target_include_directories(rdoc_gles ${RDOC_INCLUDES})
