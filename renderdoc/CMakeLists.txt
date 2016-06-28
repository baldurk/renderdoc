set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(RDOC_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(RDOC_DEFINITIONS PRIVATE -DRENDERDOC_EXPORTS)
set(RDOC_INCLUDES
    PRIVATE ${RDOC_SOURCE_DIR}
    PRIVATE ${RDOC_SOURCE_DIR}/3rdparty)
set(RDOC_LIBRARIES)

# get git commit hash
execute_process(COMMAND sh ${CMAKE_SOURCE_DIR}/calc_hash.sh
    OUTPUT_VARIABLE GIT_COMMIT_HASH)
string(STRIP ${GIT_COMMIT_HASH} GIT_COMMIT_HASH)
list(APPEND RDOC_DEFINITIONS
    PRIVATE -DGIT_COMMIT_HASH="${GIT_COMMIT_HASH}")

if(ANDROID)
    list(APPEND RDOC_LIBRARIES
        PRIVATE m
        PRIVATE dl
        PRIVATE ${CMAKE_THREAD_LIBS_INIT})
elseif(APPLE)
    list(APPEND RDOC_LIBRARIES
        PRIVATE m
        PRIVATE dl
        PRIVATE log
        PRIVATE ${CMAKE_THREAD_LIBS_INIT})
elseif(UNIX)
    find_package(PkgConfig REQUIRED)
    find_package(Threads REQUIRED)
    find_package(X11 REQUIRED)

    # This should be fixed and moved to FindXCB.cmake
    pkg_check_modules(PC_XCB REQUIRED xcb xcb-keysyms)
    set(XCB_INCLUDE_DIRS)
    set(XCB_LIBRARIES xcb xcb-keysyms)

    list(APPEND RDOC_INCLUDES
        PRIVATE ${X11_X11_INCLUDE_PATH}
        PRIVATE ${XCB_INCLUDE_DIRS})

    list(APPEND RDOC_LIBRARIES
        PRIVATE m
        PRIVATE dl
        PRIVATE ${CMAKE_THREAD_LIBS_INIT}
        PRIVATE ${X11_X11_LIB}
        PRIVATE ${XCB_LIBRARIES})
endif()

set(sources
    api/app/renderdoc_app.h
    api/replay/basic_types.h
    api/replay/capture_options.h
    api/replay/control_types.h
    api/replay/d3d11_pipestate.h
    api/replay/data_types.h
    api/replay/gl_pipestate.h
    api/replay/renderdoc_replay.h
    api/replay/replay_enums.h
    api/replay/shader_types.h
    api/replay/vk_pipestate.h
    common/common.cpp
    common/common.h
    common/custom_assert.h
    common/dds_readwrite.cpp
    common/dds_readwrite.h
    common/globalconfig.h
    common/shader_cache.h
    common/threading.h
    common/timing.h
    common/wrapped_pool.h
    core/core.cpp
    core/core.h
    core/crash_handler.h
    core/remote_access.cpp
    core/remote_replay.cpp
    core/replay_proxy.cpp
    core/replay_proxy.h
    core/resource_manager.cpp
    core/resource_manager.h
    core/socket_helpers.h
    data/glsl/debuguniforms.h
    data/glsl/texsample.h
    data/hlsl/debugcbuffers.h
    data/spv/debuguniforms.h
    data/spv/texsample.h
    data/version.h
    hooks/hooks.cpp
    hooks/hooks.h
    maths/camera.cpp
    maths/camera.h
    maths/formatpacking.h
    maths/half_convert.h
    maths/matrix.cpp
    maths/matrix.h
    maths/quat.h
    maths/vec.h
    os/os_specific.cpp
    os/os_specific.h
    replay/app_api.cpp
    replay/capture_options.cpp
    replay/entry_points.cpp
    replay/replay_driver.h
    replay/replay_output.cpp
    replay/replay_renderer.cpp
    replay/replay_renderer.h
    replay/type_helpers.cpp
    replay/type_helpers.h
    serialise/grisu2.cpp
    serialise/serialiser.cpp
    serialise/serialiser.h
    serialise/string_utils.cpp
    serialise/string_utils.h
    serialise/utf8printf.cpp
    3rdparty/jpeg-compressor/jpgd.cpp
    3rdparty/jpeg-compressor/jpgd.h
    3rdparty/jpeg-compressor/jpge.cpp
    3rdparty/jpeg-compressor/jpge.h
    3rdparty/lz4/lz4.c
    3rdparty/lz4/lz4.h
    3rdparty/stb/stb_image.h
    3rdparty/stb/stb_image_write.h
    3rdparty/stb/stb_impl.c
    3rdparty/stb/stb_truetype.h
    3rdparty/tinyexr/tinyexr.cpp
    3rdparty/tinyexr/tinyexr.h)

if(ANDROID)
    list(APPEND sources
        data/embedded_files.h
        os/posix/android/android_stringio.cpp
        os/posix/android/android_callstack.cpp
        os/posix/android/android_process.cpp
        os/posix/android/android_threading.cpp
        os/posix/posix_hook.cpp
        os/posix/posix_hook.h
        os/posix/posix_network.cpp
        os/posix/posix_process.cpp
        os/posix/posix_stringio.cpp
        os/posix/posix_threading.cpp
        os/posix/posix_specific.h)
    # posix_libentry must be the last so that library_loaded is called after
    # static objects are constructed.
    list(APPEND sources os/posix/posix_libentry.cpp)
elseif(APPLE)
    list(APPEND sources
        data/embedded_files.h
        os/posix/apple/apple_stringio.cpp
        os/posix/apple/apple_callstack.cpp
        os/posix/apple/apple_process.cpp
        os/posix/apple/apple_threading.cpp
        os/posix/posix_hook.cpp
        os/posix/posix_hook.h
        os/posix/posix_network.cpp
        os/posix/posix_process.cpp
        os/posix/posix_stringio.cpp
        os/posix/posix_threading.cpp
        os/posix/posix_specific.h)
    # posix_libentry must be the last so that library_loaded is called after
    # static objects are constructed.
    list(APPEND sources os/posix/posix_libentry.cpp)
elseif(UNIX)
    list(APPEND sources
        data/embedded_files.h
        os/posix/linux/linux_stringio.cpp
        os/posix/linux/linux_callstack.cpp
        os/posix/linux/linux_process.cpp
        os/posix/linux/linux_threading.cpp
        os/posix/posix_hook.cpp
        os/posix/posix_hook.h
        os/posix/posix_network.cpp
        os/posix/posix_process.cpp
        os/posix/posix_stringio.cpp
        os/posix/posix_threading.cpp
        os/posix/posix_specific.h)
    # posix_libentry must be the last so that library_loaded is called after
    # static objects are constructed.
    list(APPEND sources os/posix/posix_libentry.cpp)
endif()

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR APPLE)
    set_source_files_properties(3rdparty/tinyexr/tinyexr.cpp
        PROPERTIES COMPILE_FLAGS "-Wno-extra -Wno-unused-function")

    set_source_files_properties(3rdparty/jpeg-compressor/jpgd.cpp
        PROPERTIES COMPILE_FLAGS "-Wno-shift-negative-value")
endif()

add_library(rdoc OBJECT ${sources})
target_compile_definitions(rdoc ${RDOC_DEFINITIONS})
target_include_directories(rdoc ${RDOC_INCLUDES})

set(data
    data/glsl/arraymscopy.comp
    data/glsl/blit.frag
    data/glsl/blit.vert
    data/glsl/checkerboard.frag
    data/glsl/debuguniforms.h
    data/glsl/generic.frag
    data/glsl/generic.vert
    data/glsl/histogram.comp
    data/glsl/mesh.comp
    data/glsl/mesh.frag
    data/glsl/mesh.geom
    data/glsl/mesh.vert
    data/glsl/outline.frag
    data/glsl/quadoverdraw.frag
    data/glsl/texdisplay.frag
    data/glsl/texsample.h
    data/glsl/text.frag
    data/glsl/text.vert
    data/spv/blit.vert
    data/spv/checkerboard.frag
    data/spv/debuguniforms.h
    data/spv/fixedcol.frag
    data/spv/histogram.comp
    data/spv/mesh.comp
    data/spv/mesh.frag
    data/spv/mesh.geom
    data/spv/mesh.vert
    data/spv/minmaxresult.comp
    data/spv/minmaxtile.comp
    data/spv/outline.frag
    data/spv/quadresolve.frag
    data/spv/quadwrite.frag
    data/spv/texdisplay.frag
    data/spv/texsample.h
    data/spv/text.frag
    data/spv/text.vert
    data/sourcecodepro.ttf)

set(data_objects)

if(UNIX)
    foreach(res ${data})
        set(in ${res})
        set(working_dir ${CMAKE_CURRENT_SOURCE_DIR})
        set(out_src ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_FILES_DIRECTORY}/data.src/${in}.c)
        get_filename_component(out_src_dir ${out_src} DIRECTORY)

        add_custom_command(OUTPUT ${out_src}
            WORKING_DIRECTORY ${working_dir}
            COMMAND ${CMAKE_COMMAND} -E make_directory ${out_src_dir}
            COMMAND xxd -i ${in} ${out_src}
            DEPENDS ${res})

        list(APPEND data_objects ${out_src})
    endforeach()
endif()

set(renderdoc_objects)

if(ENABLE_GL)
    add_subdirectory(driver/gl)
    list(APPEND renderdoc_objects $<TARGET_OBJECTS:rdoc_gl>)
endif()

if(ENABLE_VULKAN)
    add_subdirectory(driver/vulkan)
    list(APPEND renderdoc_objects $<TARGET_OBJECTS:rdoc_vulkan>)
endif()

if(ENABLE_GL OR ENABLE_VULKAN)
    add_subdirectory(driver/shaders/spirv)
    list(APPEND renderdoc_objects $<TARGET_OBJECTS:rdoc_spirv>)
endif()

# rdoc must be after its drivers because of linux_libentry.cpp
list(APPEND renderdoc_objects
    $<TARGET_OBJECTS:rdoc>
    ${data_objects})

add_library(renderdoc SHARED ${renderdoc_objects})
target_compile_definitions(renderdoc ${RDOC_DEFINITIONS})
target_include_directories(renderdoc ${RDOC_INCLUDES})
target_link_libraries(renderdoc ${RDOC_LIBRARIES})
