set(sources
    gl_common.cpp
    gl_common.h
    gl_counters.cpp
    gl_debug.cpp
    gl_driver.cpp
    gl_driver.h
    gl_enum.h
    gl_hookset_defs.h
    gl_hookset.h
    gl_manager.cpp
    gl_manager.h
    gl_renderstate.cpp
    gl_renderstate.h
    gl_replay.cpp
    gl_replay.h
    gl_resources.cpp
    gl_resources.h
    gl_shader_refl.cpp
    gl_shader_refl.h
    official/glcorearb.h
    official/glext.h
    wrappers/gl_buffer_funcs.cpp
    wrappers/gl_debug_funcs.cpp
    wrappers/gl_draw_funcs.cpp
    wrappers/gl_emulated.cpp
    wrappers/gl_framebuffer_funcs.cpp
    wrappers/gl_get_funcs.cpp
    wrappers/gl_interop_funcs.cpp
    wrappers/gl_query_funcs.cpp
    wrappers/gl_sampler_funcs.cpp
    wrappers/gl_shader_funcs.cpp
    wrappers/gl_state_funcs.cpp
    wrappers/gl_texture_funcs.cpp
    wrappers/gl_uniform_funcs.cpp)

if(ANDROID)
    list(APPEND sources
        gl_replay_android.cpp
        gl_hooks_android.cpp)
elseif(APPLE)
    list(APPEND sources
        gl_replay_apple.cpp
        gl_hooks_apple.cpp)
elseif(UNIX)
    list(APPEND sources
        gl_hooks_linux_shared.cpp)

    if(ENABLE_GL)
        list(APPEND sources
            official/glxext.h
            official/wglext.h
            gl_replay_linux.cpp
            glx_hooks_linux.cpp
            gl_hooks_linux.cpp)
    elseif(ENABLE_GLES)
        list(APPEND sources
            gl_replay_egl.cpp)
    endif()
    if(ENABLE_GLES)
        list(APPEND sources
            gl_hooks_egl.cpp)
    endif()
endif()

add_library(rdoc_gl OBJECT ${sources})
target_compile_definitions(rdoc_gl ${RDOC_DEFINITIONS})
target_include_directories(rdoc_gl ${RDOC_INCLUDES})
