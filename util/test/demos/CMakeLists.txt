cmake_minimum_required(VERSION 2.8.12)

if(APPLE)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/../bin")
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
endif()

set(PROJECT demos)

set(VULKAN_SRC
        3rdparty/volk/volk.c
        vk/vk_headers.h
        vk/vk_helpers.cpp
        vk/vk_helpers.h
        vk/vk_test.cpp
        vk/vk_test.h
        vk/vk_adv_cbuffer_zoo.cpp
        vk/vk_buffer_truncation.cpp
        vk/vk_cbuffer_zoo.cpp
        vk/vk_compute_only.cpp
        vk/vk_custom_border_color.cpp
        vk/vk_dedicated_allocation.cpp
        vk/vk_descriptor_index.cpp
        vk/vk_descriptor_reuse.cpp
        vk/vk_descriptor_variable_count.cpp
        vk/vk_discard_rects.cpp
        vk/vk_discard_zoo.cpp
        vk/vk_draw_zoo.cpp
        vk/vk_dynamic_rendering.cpp
        vk/vk_empty_capture.cpp
        vk/vk_ext_buffer_address.cpp
        vk/vk_extended_dyn_state.cpp
        vk/vk_graphics_pipeline.cpp
        vk/vk_image_layouts.cpp
        vk/vk_imageless_framebuffer.cpp
        vk/vk_indirect.cpp
        vk/vk_int8_ibuffer.cpp
        vk/vk_khr_buffer_address.cpp
        vk/vk_large_buffer.cpp
        vk/vk_large_descriptor_sets.cpp
        vk/vk_leak_check.cpp
        vk/vk_line_raster.cpp
        vk/vk_load_store_none.cpp
        vk/vk_mesh_zoo.cpp
        vk/vk_misaligned_dirty.cpp
        vk/vk_multi_entry.cpp
        vk/vk_multi_present.cpp
        vk/vk_multi_thread_windows.cpp
        vk/vk_overlay_test.cpp
        vk/vk_parameter_zoo.cpp
        vk/vk_pixel_history.cpp
        vk/vk_postponed.cpp
        vk/vk_query_pool.cpp
        vk/vk_read_before_overwrite.cpp
        vk/vk_resource_lifetimes.cpp
        vk/vk_robustness2.cpp
        vk/vk_sample_locations.cpp
        vk/vk_secondary_cmdbuf.cpp
        vk/vk_separate_depth_stencil_layouts.cpp
        vk/vk_shader_debug_zoo.cpp
        vk/vk_shader_editing.cpp
        vk/vk_shader_isa.cpp
        vk/vk_shader_printf.cpp
        vk/vk_simple_triangle.cpp
        vk/vk_spec_constants.cpp
        vk/vk_spirv_13_shaders.cpp
        vk/vk_structured_buffer_nested.cpp
        vk/vk_sync2.cpp
        vk/vk_texture_zoo.cpp
        vk/vk_triangle_fan.cpp
        vk/vk_validation_use.cpp
        vk/vk_vertex_attr_zoo.cpp
        vk/vk_video_textures.cpp
        vk/vk_vs_max_desc_set.cpp)

set(OPENGL_SRC
        3rdparty/glad/glad.c
        gl/gl_test.cpp
        gl/gl_test.h
        gl/gl_buffer_resizing.cpp
        gl/gl_buffer_spam.cpp
        gl/gl_buffer_truncation.cpp
        gl/gl_buffer_updates.cpp
        gl/gl_callstacks.cpp
        gl/gl_cbuffer_zoo.cpp
        gl/gl_depthstencil_fbo.cpp
        gl/gl_depth_bounds.cpp
        gl/gl_discard_zoo.cpp
        gl/gl_draw_zoo.cpp
        gl/gl_empty_capture.cpp
        gl/gl_entry_points.cpp
        gl/gl_large_bcn_arrays.cpp
        gl/gl_large_buffer.cpp
        gl/gl_leak_check.cpp
        gl/gl_marker_test.cpp
        gl/gl_map_overrun.cpp
        gl/gl_mesh_zoo.cpp
        gl/gl_midframe_context_create.cpp
        gl/gl_mip_gen_rt.cpp
        gl/gl_multi_window.cpp
        gl/gl_multithread_rendering.cpp
        gl/gl_overlay_test.cpp
        gl/gl_parameter_zoo.cpp
        gl/gl_per_type_tex_units.cpp
        gl/gl_queries_in_use.cpp
        gl/gl_renderbuffer_zoo.cpp
        gl/gl_resource_lifetimes.cpp
        gl/gl_runtime_bind_prog_to_pipe.cpp
        gl/gl_separable_geometry_shader.cpp
        gl/gl_shader_editing.cpp
        gl/gl_shader_isa.cpp
        gl/gl_simple_triangle.cpp
        gl/gl_spirv_shader.cpp
        gl/gl_state_trashing.cpp
        gl/gl_texture_zoo.cpp
        gl/gl_unshared_context.cpp
        gl/gl_unsized_ms_fbo_attachment.cpp
        gl/gl_vao_0.cpp
        gl/gl_vertex_attr_zoo.cpp)

set(SRC main.cpp
        3rdparty/fmt/format.cc
        renderdoc_app.h
        test_common.cpp
        test_common.h
        texture_zoo.cpp)

project(demos)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
    list(APPEND warning_flags -Werror)
    string(REPLACE ";" " " warning_flags "${warning_flags}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${warning_flags}")
endif()

if(APPLE)
    list(APPEND SRC
        apple/nuklear_appkit.mm
        apple/nuklear_appkit.h
        apple/official/metal-cpp.cpp
        apple/official/metal-cpp.h
        apple/apple_platform.cpp
        apple/apple_platform.h
        apple/apple_window.cpp
        apple/apple_window.h)
    add_executable(demos ${SRC} ${VULKAN_SRC})
    set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17")
elseif(UNIX)
    list(APPEND OPENGL_SRC
        3rdparty/glad/glad_glx.c
        gl/gl_test_linux.cpp)
    list(APPEND SRC 
        linux/linux_platform.cpp
        linux/linux_platform.h
        linux/linux_window.cpp
        linux/linux_window.h)
    add_executable(demos ${SRC} ${VULKAN_SRC} ${OPENGL_SRC})
endif()

install(TARGETS demos DESTINATION .)

if("${CMAKE_SIZEOF_VOID_P}" EQUAL "8")
    set(SHADERC_DIR "linux64")
    set(BIN_SUFFIX "x64")
else()
    set(SHADERC_DIR "linux32")
    set(BIN_SUFFIX "x86")
endif()

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/shaderc/${SHADERC_DIR})
    target_compile_definitions(demos PRIVATE -DHAVE_SHADERC=1)
    target_include_directories(demos PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/shaderc/${SHADERC_DIR}/include)
    target_link_libraries(demos PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/shaderc/${SHADERC_DIR}/lib/libshaderc_combined.a)
    message(STATUS "Linking in shaderc")
endif()

find_package(Threads REQUIRED)

target_include_directories(demos
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/vk/official
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

if(APPLE)
    target_include_directories(demos
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/apple/official)
    find_library(COCOA_LIBRARY Cocoa)
    find_library(QUARTZCORE_LIBRARY QuartzCore)
    find_library(METAL_LIBRARY Metal)
    target_compile_definitions(demos PRIVATE
        -DVK_USE_PLATFORM_MACOS_MVK=1)
    set(LIBS ${COCOA_LIBRARY} ${QUARTZCORE_LIBRARY} ${METAL_LIBRARY})
elseif(UNIX)
    target_compile_definitions(demos PRIVATE -DVK_USE_PLATFORM_XCB_KHR=1)
    set(LIBS -lX11 -lxcb -lX11-xcb)
endif()

target_link_libraries(demos PRIVATE ${LIBS} ${CMAKE_THREAD_LIBS_INIT} ${CMAKE_DL_LIBS})
set_target_properties(demos PROPERTIES OUTPUT_NAME demos_${BIN_SUFFIX})

