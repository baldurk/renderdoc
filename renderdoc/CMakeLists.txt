set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(RDOC_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(RDOC_DEFINITIONS PRIVATE -DRENDERDOC_EXPORTS)
set(RDOC_INCLUDES
    PRIVATE ${RDOC_SOURCE_DIR}
    PRIVATE ${RDOC_SOURCE_DIR}/3rdparty)
set(RDOC_LIBRARIES)

if(ANDROID)
    list(APPEND RDOC_LIBRARIES
        PRIVATE -lm
        PRIVATE -ldl
        PRIVATE -llog
        PRIVATE -landroid)
elseif(APPLE)
    list(APPEND RDOC_LIBRARIES
        PRIVATE -lm
        PRIVATE -ldl)

    if(ENABLE_VULKAN)
        find_library(COCOA_LIBRARY Cocoa)
        list(APPEND RDOC_LIBRARIES PRIVATE ${COCOA_LIBRARY})

        find_library(QUARTZCORE_LIBRARY QuartzCore)
        list(APPEND RDOC_LIBRARIES PRIVATE ${QUARTZCORE_LIBRARY})
    endif()

    if(ENABLE_GL)
        find_library(OPENGL_LIBRARY OpenGL)
        list(APPEND RDOC_LIBRARIES PRIVATE ${OPENGL_LIBRARY})
    endif()
elseif(UNIX)
    find_package(PkgConfig REQUIRED)
    find_package(Threads REQUIRED)

    list(APPEND RDOC_LIBRARIES
        PRIVATE -lm
        PRIVATE -ldl
        PRIVATE -lrt)

    if(ENABLE_XLIB)
        find_package(X11 REQUIRED)

        list(APPEND RDOC_INCLUDES
            PRIVATE ${X11_X11_INCLUDE_PATH})

        list(APPEND RDOC_LIBRARIES
            PRIVATE ${X11_X11_LIB})
    endif()

    if(ENABLE_XCB)
        # This should be fixed and moved to FindXCB.cmake
        pkg_check_modules(PC_XCB REQUIRED xcb xcb-keysyms)
        set(XCB_INCLUDE_DIRS)
        set(XCB_LIBRARIES xcb xcb-keysyms)

        list(APPEND RDOC_INCLUDES
            PRIVATE ${XCB_INCLUDE_DIRS})

        list(APPEND RDOC_LIBRARIES
            PRIVATE -l${XCB_LIBRARIES})
    endif()
endif()

if(NOT "x${CMAKE_THREAD_LIBS_INIT}" STREQUAL "x")
    list(APPEND RDOC_LIBRARIES PRIVATE ${CMAKE_THREAD_LIBS_INIT})
endif()

set(sources
    api/app/renderdoc_app.h
    api/replay/basic_types.h
    api/replay/stringise.h
    api/replay/structured_data.h
    api/replay/capture_options.h
    api/replay/control_types.h
    api/replay/common_pipestate.h
    api/replay/d3d11_pipestate.h
    api/replay/data_types.h
    api/replay/gl_pipestate.h
    api/replay/renderdoc_replay.h
    api/replay/replay_enums.h
    api/replay/shader_types.h
    api/replay/vk_pipestate.h
    api/replay/version.h
    api/replay/renderdoc_tostr.inl
    api/replay/version.cpp
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
    common/threading_tests.cpp
    core/core.cpp
    core/image_viewer.cpp
    core/core.h
    core/crash_handler.h
    core/target_control.cpp
    core/remote_server.cpp
    core/replay_proxy.cpp
    core/replay_proxy.h
    android/android.cpp
    android/android_patch.cpp
    android/android_tools.cpp
    android/android_utils.cpp
    android/android_manifest.cpp
    android/android.h
    android/android_utils.h
    android/jdwp.h
    android/jdwp.cpp
    android/jdwp_util.cpp
    android/jdwp_connection.cpp
    core/plugins.cpp
    core/plugins.h
    core/resource_manager.cpp
    core/resource_manager.h
    data/hlsl/debugcbuffers.h
    data/glsl/debuguniforms.h
    data/glsl/vk_texsample.h
    data/glsl/gl_texsample.h
    data/glsl/gles_texsample.h
    data/glsl_shaders.cpp
    data/glsl_shaders.h
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
    replay/basic_types_tests.cpp
    replay/capture_options.cpp
    replay/renderdoc_serialise.inl
    replay/capture_file.cpp
    replay/entry_points.cpp
    replay/replay_driver.cpp
    replay/replay_driver.h
    replay/replay_output.cpp
    replay/replay_controller.cpp
    replay/replay_controller.h
    serialise/serialiser.cpp
    serialise/serialiser.h
    serialise/lz4io.cpp
    serialise/lz4io.h
    serialise/zstdio.cpp
    serialise/zstdio.h
    serialise/streamio.cpp
    serialise/streamio.h
    serialise/rdcfile.cpp
    serialise/rdcfile.h
    serialise/codecs/xml_codec.cpp
    serialise/codecs/chrome_json_codec.cpp
    serialise/comp_io_tests.cpp
    serialise/serialiser_tests.cpp
    serialise/streamio_tests.cpp
    strings/grisu2.cpp
    strings/string_utils.cpp
    strings/string_utils.h
    strings/utf8printf.cpp
    3rdparty/jpeg-compressor/jpgd.cpp
    3rdparty/jpeg-compressor/jpgd.h
    3rdparty/jpeg-compressor/jpge.cpp
    3rdparty/jpeg-compressor/jpge.h
    3rdparty/android/android_manifest.h
    3rdparty/catch/catch.cpp
    3rdparty/catch/catch.hpp
    3rdparty/pugixml/pugixml.cpp
    3rdparty/pugixml/pugixml.hpp
    3rdparty/pugixml/pugiconfig.hpp
    3rdparty/lz4/lz4.c
    3rdparty/lz4/lz4.h
    3rdparty/miniz/miniz.c
    3rdparty/miniz/miniz.h
    3rdparty/zstd/bitstream.h
    3rdparty/zstd/compiler.h
    3rdparty/zstd/cpu.h
    3rdparty/zstd/debug.c
    3rdparty/zstd/debug.h
    3rdparty/zstd/entropy_common.c
    3rdparty/zstd/error_private.c
    3rdparty/zstd/error_private.h
    3rdparty/zstd/fse.h
    3rdparty/zstd/fse_compress.c
    3rdparty/zstd/fse_decompress.c
    3rdparty/zstd/hist.c
    3rdparty/zstd/hist.h
    3rdparty/zstd/huf.h
    3rdparty/zstd/huf_compress.c
    3rdparty/zstd/huf_decompress.c
    3rdparty/zstd/LICENSE
    3rdparty/zstd/mem.h
    3rdparty/zstd/pool.c
    3rdparty/zstd/pool.h
    3rdparty/zstd/threading.c
    3rdparty/zstd/threading.h
    3rdparty/zstd/xxhash.c
    3rdparty/zstd/xxhash.h
    3rdparty/zstd/zstd.h
    3rdparty/zstd/zstd_common.c
    3rdparty/zstd/zstd_compress.c
    3rdparty/zstd/zstd_compress_internal.h
    3rdparty/zstd/zstd_decompress.c
    3rdparty/zstd/zstd_double_fast.c
    3rdparty/zstd/zstd_double_fast.h
    3rdparty/zstd/zstd_errors.h
    3rdparty/zstd/zstd_fast.c
    3rdparty/zstd/zstd_fast.h
    3rdparty/zstd/zstd_internal.h
    3rdparty/zstd/zstd_lazy.c
    3rdparty/zstd/zstd_lazy.h
    3rdparty/zstd/zstd_ldm.c
    3rdparty/zstd/zstd_ldm.h
    3rdparty/zstd/zstd_opt.c
    3rdparty/zstd/zstd_opt.h
    3rdparty/zstd/zstdmt_compress.c
    3rdparty/zstd/zstdmt_compress.h
    3rdparty/stb/stb_image.h
    3rdparty/stb/stb_image_write.h
    3rdparty/stb/stb_image_resize.h
    3rdparty/stb/stb_impl.c
    3rdparty/stb/stb_truetype.h
    3rdparty/tinyexr/tinyexr.cpp
    3rdparty/tinyexr/tinyexr.h
    3rdparty/tinyfiledialogs/tinyfiledialogs.c
    3rdparty/tinyfiledialogs/tinyfiledialogs.h)

if(ANDROID)
    list(APPEND sources
        data/embedded_files.h
        os/posix/android/android_stringio.cpp
        os/posix/android/android_callstack.cpp
        os/posix/android/android_process.cpp
        os/posix/android/android_threading.cpp
        os/posix/android/android_hook.cpp
        os/posix/android/android_network.cpp
        os/posix/posix_network.h
        os/posix/posix_network.cpp
        os/posix/posix_process.cpp
        os/posix/posix_stringio.cpp
        os/posix/posix_threading.cpp
        os/posix/posix_specific.h)
elseif(APPLE)
    list(APPEND sources
        data/embedded_files.h
        os/posix/apple/apple_stringio.cpp
        os/posix/apple/apple_callstack.cpp
        os/posix/apple/apple_process.cpp
        os/posix/apple/apple_threading.cpp
        os/posix/apple/apple_hook.cpp
        os/posix/apple/apple_network.cpp
        os/posix/posix_network.h
        os/posix/posix_network.cpp
        os/posix/posix_process.cpp
        os/posix/posix_stringio.cpp
        os/posix/posix_threading.cpp
        os/posix/posix_specific.h)
elseif(UNIX)
    list(APPEND sources
        data/embedded_files.h
        os/posix/linux/linux_stringio.cpp
        os/posix/linux/linux_callstack.cpp
        os/posix/linux/linux_process.cpp
        os/posix/linux/linux_threading.cpp
        os/posix/linux/linux_hook.cpp
        os/posix/linux/linux_network.cpp
        3rdparty/plthook/plthook.h
        3rdparty/plthook/plthook_elf.c
        os/posix/posix_network.h
        os/posix/posix_network.cpp
        os/posix/posix_process.cpp
        os/posix/posix_stringio.cpp
        os/posix/posix_threading.cpp
        os/posix/posix_specific.h)
endif()

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR APPLE)
    set_source_files_properties(3rdparty/tinyexr/tinyexr.cpp
        PROPERTIES COMPILE_FLAGS "-Wno-extra -Wno-unused-function")

    set_source_files_properties(3rdparty/miniz/miniz.c
        PROPERTIES COMPILE_FLAGS "-Wno-attributes")

    # Need to add -Wno-unknown-warning-option since some clang versions don't have
    # -Wno-shift-negative-value available
    set_source_files_properties(3rdparty/jpeg-compressor/jpgd.cpp
        PROPERTIES COMPILE_FLAGS "-Wno-unknown-warning-option -Wno-shift-negative-value -Wno-invalid-noreturn")

    if(CMAKE_COMPILER_IS_GNUCXX)
        set_property(SOURCE 3rdparty/jpeg-compressor/jpgd.cpp
            APPEND_STRING PROPERTY COMPILE_FLAGS " -Wno-unknown-warning -Wno-implicit-fallthrough")

        set_property(SOURCE strings/utf8printf.cpp
            APPEND_STRING PROPERTY COMPILE_FLAGS " -Wno-unknown-warning -Wno-format-truncation")
    endif()

    # Need to add -Wno-unknown-warning-option since only newer clang versions have
    # -Wno-unused-lambda-capture available
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
	       # Only clang has this warning. Fixing it in this file causes a compile error on windows
	       set_source_files_properties(os/os_specific.cpp
	           PROPERTIES COMPILE_FLAGS "-Wno-unknown-warning-option -Wno-unused-lambda-capture")
    endif()
endif()

set(data
    data/glsl/blit.vert
    data/glsl/checkerboard.frag
    data/glsl/debuguniforms.h
    data/glsl/fixedcol.frag
    data/glsl/histogram.comp
    data/glsl/mesh.comp
    data/glsl/mesh.frag
    data/glsl/mesh.geom
    data/glsl/mesh.vert
    data/glsl/minmaxresult.comp
    data/glsl/minmaxtile.comp
    data/glsl/outline.frag
    data/glsl/quadresolve.frag
    data/glsl/quadwrite.frag
    data/glsl/texdisplay.frag
    data/glsl/gl_texsample.h
    data/glsl/gles_texsample.h
    data/glsl/vk_texsample.h
    data/glsl/gltext.frag
    data/glsl/gltext.vert
    data/glsl/vktext.frag
    data/glsl/vktext.vert
    data/glsl/array2ms.comp
    data/glsl/ms2array.comp
    data/glsl/trisize.frag
    data/glsl/trisize.geom
    data/glsl/deptharr2ms.frag
    data/glsl/depthms2arr.frag
    data/sourcecodepro.ttf
    driver/vulkan/renderdoc.json)

set(data_objects)

if(UNIX)
    # If we're cross-compiling, include-bin will get built for the target and we
    # then can't execute it. Instead, we force calling c++ (which we can safely
    # assume is present) directly to build the binary

    if(CMAKE_CROSSCOMPILING)
        set(HOST_NATIVE_CPP_COMPILER c++ CACHE STRING "Command to run to compile a .cpp into an executable. Default is just c++")

        add_custom_command(OUTPUT ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/include-bin
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            COMMAND ${HOST_NATIVE_CPP_COMPILER} 3rdparty/include-bin/main.cpp -o ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/include-bin
            DEPENDS 3rdparty/include-bin/main.cpp)
    else()
        add_executable(include-bin 3rdparty/include-bin/main.cpp)
    endif()

    foreach(res ${data})
        set(in ${res})
        set(working_dir ${CMAKE_CURRENT_SOURCE_DIR})
        set(out_src ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_FILES_DIRECTORY}/data.src/${in}.c)
        get_filename_component(out_src_dir ${out_src} DIRECTORY)

        add_custom_command(OUTPUT ${out_src}
            WORKING_DIRECTORY ${working_dir}
            COMMAND ${CMAKE_COMMAND} -E make_directory ${out_src_dir}
            COMMAND ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/include-bin ${in} ${out_src}
            DEPENDS ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/include-bin
            DEPENDS ${res})

        list(APPEND data_objects ${out_src})
    endforeach()
endif()

set(renderdoc_objects)

if(ENABLE_GL OR ENABLE_GLES)
    add_subdirectory(driver/gl)
    list(APPEND renderdoc_objects $<TARGET_OBJECTS:rdoc_gl>)
endif()

if(ENABLE_VULKAN)
    add_subdirectory(driver/vulkan)
    list(APPEND renderdoc_objects $<TARGET_OBJECTS:rdoc_vulkan>)
endif()

if(ENABLE_GL OR ENABLE_GLES OR ENABLE_VULKAN)
    add_subdirectory(driver/shaders/spirv)
    list(APPEND renderdoc_objects $<TARGET_OBJECTS:rdoc_spirv>)
endif()

option(USE_INTERCEPTOR_LIB OFF)

# on Android, pull in interceptor-lib only if we have LLVM available
if(ANDROID AND USE_INTERCEPTOR_LIB )
    find_package(LLVM CONFIG)

    if(NOT LLVM_FOUND)
        message(FATAL_ERROR "LLVM not found - interceptor-lib requires LLVM 4.0 available.")
    elseif(${LLVM_PACKAGE_VERSION} VERSION_LESS "4.0.0" OR ${LLVM_PACKAGE_VERSION} VERSION_GREATER "4.0.1")
        message(FATAL_ERROR "LLVM ${LLVM_PACKAGE_VERSION} found - we require precisely 4.0.")
    else()
        message(STATUS "LLVM ${LLVM_PACKAGE_VERSION} found - enabling android hooking using interceptor-lib.")
        add_subdirectory(3rdparty/interceptor-lib)
        list(APPEND renderdoc_objects $<TARGET_OBJECTS:interceptor_lib>)
        link_directories(${LLVM_DIR}/../..)
        set(RDOC_DEFINITIONS ${RDOC_DEFINITIONS} PRIVATE -DRENDERDOC_HAVE_INTERCEPTOR_LIB)

        foreach(lib ${interceptor_libs})
            list(APPEND RDOC_LIBRARIES PRIVATE -l${lib})
        endforeach()
    endif()
endif()

if(ANDROID AND NOT USE_INTERCEPTOR_LIB)
    message(STATUS "Interceptor-lib not enabled (USE_INTERCEPTOR_LIB) - android hooking will use sometimes less reliable PLT-interception method. ")
endif()

# always pull in the amd folder
add_subdirectory(driver/ihv/amd)
list(APPEND renderdoc_objects $<TARGET_OBJECTS:rdoc_amd>)

# pull in the intel folder for GL perf queries
add_subdirectory(driver/ihv/intel)
list(APPEND renderdoc_objects $<TARGET_OBJECTS:rdoc_intel>)

add_library(rdoc OBJECT ${sources})
target_compile_definitions(rdoc ${RDOC_DEFINITIONS})
target_include_directories(rdoc ${RDOC_INCLUDES})

list(APPEND renderdoc_objects
    $<TARGET_OBJECTS:rdoc>
    ${data_objects})

# posix_libentry must be the last so that library_loaded is called after
# static objects are constructed. We guarantee this happens after even
# any other static libraries that we link by making it its own static
# library and appending it to the end of the link list.
add_library(renderdoc_libentry STATIC os/posix/posix_libentry.cpp)
target_compile_definitions(renderdoc_libentry ${RDOC_DEFINITIONS})
target_include_directories(renderdoc_libentry ${RDOC_INCLUDES})
link_directories(${CMAKE_CURRENT_BINARY_DIR})

# On apple we need to pass the force_load parameter here - if we set it with LINK_FLAGS below
# it gets applied too early (even if the -lrenderdoc_libentry is later)
if(APPLE)
    list(APPEND RDOC_LIBRARIES PRIVATE "-Wl,-force_load,librenderdoc_libentry.a -lrenderdoc_libentry")
else()
    list(APPEND RDOC_LIBRARIES PRIVATE -lrenderdoc_libentry)
endif()

add_library(renderdoc SHARED ${renderdoc_objects})
target_compile_definitions(renderdoc ${RDOC_DEFINITIONS})
target_include_directories(renderdoc ${RDOC_INCLUDES})
target_link_libraries(renderdoc ${RDOC_LIBRARIES})

add_dependencies(renderdoc renderdoc_libentry)

if(UNIX AND NOT ANDROID AND NOT APPLE)
    set_target_properties(renderdoc PROPERTIES LINK_FLAGS "-Wl,--undefined,force_include_libentry -Wl,--version-script,${CMAKE_CURRENT_SOURCE_DIR}/renderdoc.version -Wl,--no-undefined")
endif()

# On macOS set the rpath so that linked libraries are relative to the executable, not absolute
if(APPLE)
    set_target_properties(renderdoc PROPERTIES MACOSX_RPATH 1)
    set_target_properties(renderdoc PROPERTIES INSTALL_RPATH "@executable_path/../lib")
    set_target_properties(renderdoc PROPERTIES INSTALL_NAME_DIR "@executable_path/../lib")
    set_target_properties(renderdoc PROPERTIES BUILD_WITH_INSTALL_RPATH 1)
endif()

if(ANDROID)
    set_target_properties(renderdoc PROPERTIES LINK_FLAGS "-Wl,--undefined,force_include_libentry -Wl,--build-id")
    # rename output library
    set_target_properties(renderdoc PROPERTIES OUTPUT_NAME "VkLayer_GLES_RenderDoc")

    if(STRIP_ANDROID_LIBRARY AND CMAKE_STRIP AND CMAKE_BUILD_TYPE STREQUAL "Release")
        add_custom_command(TARGET renderdoc POST_BUILD
            COMMAND echo Stripping $<TARGET_FILE:renderdoc>
            COMMAND ${CMAKE_STRIP} --strip-unneeded $<TARGET_FILE:renderdoc>)
    endif()
endif()

install (TARGETS renderdoc DESTINATION lib${LIB_SUFFIX}/${LIB_SUBFOLDER})

# Copy in application API header to include
install (FILES api/app/renderdoc_app.h DESTINATION include RENAME renderdoc.h)

