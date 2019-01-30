set(sources
    gl_common.cpp
    gl_common.h
    gl_counters.cpp
    gl_debug.cpp
    gl_postvs.cpp
    gl_overlay.cpp
    gl_outputwindow.cpp
    gl_rendermesh.cpp
    gl_rendertexture.cpp
    gl_rendertext.cpp
    gl_msaa_array_conv.cpp
    gl_driver.cpp
    gl_driver.h
    gl_enum.h
    gl_dispatch_table_defs.h
    gl_dispatch_table.h
    gl_initstate.h
    gl_initstate.cpp
    gl_manager.cpp
    gl_manager.h
    gl_renderstate.cpp
    gl_renderstate.h
    gl_replay.cpp
    gl_replay.h
    gl_resources.cpp
    gl_resources.h
    gl_program_iterate.cpp
    gl_shader_refl.cpp
    gl_shader_refl.h
    gl_stringise.cpp
    official/gl32.h
    official/gl3platform.h
    official/glcorearb.h
    official/glext.h
    official/khrplatform.h
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

# Common hooking code for any GL/GLES on any platform
list(APPEND sources gl_hooks.cpp)

if(APPLE)
    list(APPEND sources
        cgl_dispatch_table.h
        cgl_platform.mm
        cgl_platform.cpp
        cgl_hooks.cpp)
else()
    if(ENABLE_GL)
        list(APPEND sources
            official/glxext.h
            glx_dispatch_table.h
            glx_platform.cpp
            glx_hooks.cpp
            glx_fake_vk_hooks.cpp)
    endif()
    if(ENABLE_GLES)
        list(APPEND sources
            official/egl.h
            official/eglext.h
            official/eglplatform.h
            official/glesext.h
            egl_dispatch_table.h
            egl_platform.cpp
            egl_hooks.cpp)
    endif()
endif()

add_library(rdoc_gl OBJECT ${sources})
target_compile_definitions(rdoc_gl ${RDOC_DEFINITIONS})
target_include_directories(rdoc_gl ${RDOC_INCLUDES})
