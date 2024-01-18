cmake_minimum_required(VERSION 2.8.12)

if(APPLE)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/../bin")
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
endif()

# Configure some stuff that needs to be set really early
if(BUILD_ANDROID)
    if(NOT DEFINED ENV{JAVA_HOME})
        message(FATAL_ERROR "JAVA_HOME environment variable must be defined for Android build")
    endif()
    message(STATUS "Using JAVA_HOME = $ENV{JAVA_HOME}")

    execute_process(COMMAND $ENV{JAVA_HOME}/bin/java -version
        RESULT_VARIABLE _result
        OUTPUT_VARIABLE _output
        ERROR_VARIABLE _output
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_STRIP_TRAILING_WHITESPACE)
    
    if(NOT _result AND _output MATCHES "version \"([0-9]+).([0-9]+)")
        message(STATUS "Java in JAVA_HOME is ${CMAKE_MATCH_1}.${CMAKE_MATCH_2}")
    else()
        message(STATUS "Java in JAVA_HOME is unknown version ${_output} ${_result}")
    endif()

    if(DEFINED ENV{ANDROID_HOME} AND EXISTS "$ENV{ANDROID_HOME}/build-tools")
        set(ANDROID_SDK_ROOT_PATH "$ENV{ANDROID_HOME}")
    elseif(DEFINED ENV{ANDROID_SDK_ROOT} AND EXISTS "$ENV{ANDROID_SDK_ROOT}/build-tools")
        set(ANDROID_SDK_ROOT_PATH "$ENV{ANDROID_SDK_ROOT}")
    elseif(DEFINED ENV{ANDROID_SDK} AND EXISTS "$ENV{ANDROID_SDK}/build-tools")
        set(ANDROID_SDK_ROOT_PATH "$ENV{ANDROID_SDK}")
    else()
        message(FATAL_ERROR "Can't locate Android SDK, set ANDROID_HOME, ANDROID_SDK_ROOT or ANDROID_SDK")
    endif()

    message(STATUS "Using Android SDK found in ${ANDROID_SDK_ROOT_PATH}")

    if(DEFINED ENV{ANDROID_NDK_HOME} AND EXISTS "$ENV{ANDROID_NDK_HOME}/build/cmake/android.toolchain.cmake")
        set(ANDROID_NDK_ROOT_PATH "$ENV{ANDROID_NDK_HOME}")
    elseif(DEFINED ENV{ANDROID_NDK_ROOT} AND EXISTS "$ENV{ANDROID_NDK_ROOT}/build/cmake/android.toolchain.cmake")
        set(ANDROID_NDK_ROOT_PATH "$ENV{ANDROID_NDK_ROOT}")
    elseif(DEFINED ENV{NDK_HOME} AND EXISTS "$ENV{NDK_HOME}/build/cmake/android.toolchain.cmake")
        set(ANDROID_NDK_ROOT_PATH "$ENV{NDK_HOME}")
    elseif(DEFINED ENV{ANDROID_NDK} AND EXISTS "$ENV{ANDROID_NDK}/build/cmake/android.toolchain.cmake")
        set(ANDROID_NDK_ROOT_PATH "$ENV{ANDROID_NDK}")
    else()
        message(FATAL_ERROR "Can't locate Android NDK, set ANDROID_NDK_HOME, ANDROID_NDK_ROOT, NDK_HOME or ANDROID_NDK")
    endif()

    message(STATUS "Using Android NDK found in ${ANDROID_NDK_ROOT_PATH}")

    # Extract the NDK major version
    set(ANDROID_NDK_VERSION "0.0.0")
    if(EXISTS "${ANDROID_NDK_ROOT_PATH}/source.properties")
        file(STRINGS "${ANDROID_NDK_ROOT_PATH}/source.properties" __ndk_props)
        foreach(__line ${__ndk_props})
            string(FIND "${__line}" "Pkg.Revision = " __ndk_rev_found)
            if(__ndk_rev_found EQUAL "0")
                string(SUBSTRING "${__line}" 15 -1 ANDROID_NDK_VERSION)
                message(STATUS "Android NDK version detected as ${ANDROID_NDK_VERSION}")
                break()
            endif()
        endforeach()
    endif()

    set(CMAKE_TOOLCHAIN_FILE
        "${ANDROID_NDK_ROOT_PATH}/build/cmake/android.toolchain.cmake"
        CACHE STRING
        "The Android toolchain file")

    # Set default API level to 21 if not configured explicitly
    if(NOT ANDROID_PLATFORM)
        set(ANDROID_PLATFORM "android-21")
    endif()

    # default to libc++_static as the other options can cause crashes
    if(NOT ANDROID_STL)
        set(ANDROID_STL "c++_static")
    endif()

    # Choose clang if the NDK has both gcc and clang, since gcc sometimes fails
    set(CMAKE_ANDROID_NDK_TOOLCHAIN_VERSION "clang")

    # Default to arm64 if nothing is specified on the command line.
    # Options are {armeabi-v7a,arm64-v8a}
    set(ANDROID_ABI "arm64-v8a" CACHE STRING "The Android ABI to build for")

    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DANDROID=1")
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
        vk/vk_blend.cpp
        vk/vk_buffer_truncation.cpp
        vk/vk_cbuffer_zoo.cpp
        vk/vk_compute_only.cpp
        vk/vk_counters.cpp
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
        vk/vk_mem_bench.cpp
        vk/vk_mesh_zoo.cpp
        vk/vk_misaligned_dirty.cpp
        vk/vk_multi_entry.cpp
        vk/vk_multi_present.cpp
        vk/vk_multi_thread_windows.cpp
        vk/vk_multi_view.cpp
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
        gl/gl_pixel_history.cpp
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
    list(APPEND warning_flags -Werror -Wno-unused-result -Wno-nullability-completeness)
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
    set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17 -Wno-deprecated-declarations")
elseif(BUILD_ANDROID)
    string(REPLACE "\\" "/" GLUE_SOURCE "${ANDROID_NDK_ROOT_PATH}/sources/android/native_app_glue/android_native_app_glue.c")
    include_directories(${ANDROID_NDK_ROOT_PATH}/sources/android/native_app_glue)

    list(APPEND OPENGL_SRC
        3rdparty/glad/glad_egl.c
        gl/gl_test_android.cpp)
    list(APPEND SRC 
        android/android_platform.cpp
        android/android_platform.h
        android/android_window.cpp
        android/android_window.h
        ${GLUE_SOURCE})
    add_library(demos SHARED ${SRC} ${VULKAN_SRC} ${OPENGL_SRC})
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

if(BUILD_ANDROID)
    set(SHADERC_DIR "dummy")
    set(BIN_SUFFIX "")
    target_link_libraries(demos PRIVATE ${ANDROID_NDK_ROOT_PATH}/sources/third_party/shaderc/obj/local/${ANDROID_ABI}/libshaderc_combined.a)
    target_compile_definitions(demos PRIVATE -DHAVE_SHADERC=1)
    target_include_directories(demos PRIVATE ${ANDROID_NDK_ROOT_PATH}/sources/third_party/shaderc/include)
else()
    if("${CMAKE_SIZEOF_VOID_P}" EQUAL "8")
        set(SHADERC_DIR "linux64")
        set(BIN_SUFFIX "_x64")
    else()
        set(SHADERC_DIR "linux32")
        set(BIN_SUFFIX "_x86")
    endif()
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
elseif(BUILD_ANDROID)
    target_compile_definitions(demos PRIVATE -DVK_USE_PLATFORM_ANDROID_KHR=1)
    list(APPEND LIBS PRIVATE -llog -landroid -lEGL)

    # The version of fmt used will not build due to std::char_traits support of non-std types being
    # deprecated in the version of LLVM used by NDK v26
    if(NOT "${ANDROID_NDK_VERSION}" VERSION_LESS "26.0.0")
        target_compile_options(demos PRIVATE -Wno-deprecated-declarations)
    endif()
elseif(UNIX)
    target_compile_definitions(demos PRIVATE -DVK_USE_PLATFORM_XCB_KHR=1)
    set(LIBS -lX11 -lxcb -lX11-xcb)
endif()

target_link_libraries(demos PRIVATE ${LIBS} ${CMAKE_THREAD_LIBS_INIT} ${CMAKE_DL_LIBS})
set_target_properties(demos PROPERTIES OUTPUT_NAME demos${BIN_SUFFIX})

if(ANDROID)
    # Android sets this to off becuase Android is always terrible forever.
    # It breaks finding java in the path, so enable it again
    set(CMAKE_FIND_USE_SYSTEM_ENVIRONMENT_PATH ON)

    #############################
    # We need to check that 'java' in PATH is new enough. Temporarily unset the JAVA_HOME env,
    # then invoke FindJava.cmake which will search just the PATH, then re-set it.
    set(SAVE_JAVA_HOME $ENV{JAVA_HOME})

    set(ENV{JAVA_HOME} "")
    find_package(Java)
    set(ENV{JAVA_HOME} ${SAVE_JAVA_HOME})

    if(NOT ${Java_FOUND})
        message(FATAL_ERROR "Building Android requires the 'java' program in your PATH. It must be at least Java 8 (1.8)")
    endif()

    if(${Java_VERSION} VERSION_LESS 1.8)
        message(FATAL_ERROR "Building Android requires the 'java' program in your PATH to be at least Java 8 (1.8)")
    endif()
    message(STATUS "Using Java of version ${Java_VERSION}")

    set(ANDROID_BUILD_TOOLS_VERSION "" CACHE STRING "Version of Android build-tools to use instead of the default")
    if(ANDROID_BUILD_TOOLS_VERSION STREQUAL "")
        # Enumerate the build tools versions available, and pick the most recent
        file(GLOB __buildTools RELATIVE "${ANDROID_SDK_ROOT_PATH}/build-tools" "${ANDROID_SDK_ROOT_PATH}/build-tools/*")
        list(SORT __buildTools)

        list(GET __buildTools -1 ANDROID_BUILD_TOOLS_VERSION)

        unset(__buildTools)
    endif()
    message(STATUS "Using Android build-tools version ${ANDROID_BUILD_TOOLS_VERSION}")

    set(APK_TARGET_ID "" CACHE STRING "The Target ID to build the APK for like 'android-99', use <android list targets> to choose another one.")
    if(APK_TARGET_ID STREQUAL "")
        # This seems different from the platform we're targetting,
        # default to the latest available that's greater or equal to our target platform
        file(GLOB __platforms RELATIVE "${ANDROID_SDK_ROOT_PATH}/platforms" "${ANDROID_SDK_ROOT_PATH}/platforms/*")
        list(SORT __platforms)

        # In case we don't find one, target the latest platform
        list(GET __platforms -1 APK_TARGET_ID)

        string(REPLACE "android-" "" __targetPlat "${ANDROID_PLATFORM}")

        # We require at least android 23 for Activity.requestPermissions
        if(__targetPlat LESS 23)
            set(__targetPlat 23)
        endif()

        foreach( __plat ${__platforms})
            string(REPLACE "android-" "" __curPlat "${__plat}")

            if(NOT (__curPlat LESS __targetPlat) )
                set(APK_TARGET_ID "android-${__curPlat}")
                break()
            endif()
        endforeach()

        unset(__platforms)
        unset(__targetPlat)
        unset(__curPlat)
    endif()
    message(STATUS "Using android.jar from platform ${APK_TARGET_ID}")

    # Suffix for scripts rather than binaries, which is needed explicitly on windows
    set(TOOL_SCRIPT_EXTENSION "")
    if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Windows")
        set(TOOL_SCRIPT_EXTENSION ".bat")
    endif()

    set(BUILD_TOOLS "${ANDROID_SDK_ROOT_PATH}/build-tools/${ANDROID_BUILD_TOOLS_VERSION}")
    set(RT_JAR "$ENV{JAVA_HOME}/jre/lib/rt.jar")
    set(JAVA_BIN "$ENV{JAVA_HOME}/bin")

    string(REPLACE "\\" "/" ANDROID_JAR "${ANDROID_SDK_ROOT_PATH}/platforms/${APK_TARGET_ID}/android.jar")
    if(CMAKE_HOST_WIN32)
        set(CLASS_PATH "${ANDROID_JAR}\;obj")
    else()
        set(CLASS_PATH "${ANDROID_JAR}:obj")
    endif()
    set(KEYSTORE ${CMAKE_CURRENT_BINARY_DIR}/debug.keystore)
    add_custom_command(OUTPUT ${KEYSTORE}
                       WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
                       COMMAND ${JAVA_BIN}/keytool -genkey -keystore ${KEYSTORE} -storepass android -alias rdocandroidkey -keypass android -keyalg RSA -keysize 2048 -validity 10000 -dname "CN=, OU=, O=, L=, S=, C=")

    set(APK_VERSION_CODE "10")
    set(APK_VERSION_NAME "demos_build")

    # Set the package name based on the ABI
    if(ANDROID_ABI STREQUAL "armeabi-v7a")
        set(ABI_EXTENSION_NAME "arm32")
    elseif(ANDROID_ABI STREQUAL "arm64-v8a")
        set(ABI_EXTENSION_NAME "arm64")
    elseif(ANDROID_ABI STREQUAL "x86")
        set(ABI_EXTENSION_NAME "x86")
    elseif(ANDROID_ABI STREQUAL "x86_64")
        set(ABI_EXTENSION_NAME "x64")
    else()
        message(FATAL_ERROR "ABI ${ANDROID_ABI} is not supported.")
    endif()

    set(RENDERDOC_ANDROID_PACKAGE_NAME "renderdoc.org.demos.${ABI_EXTENSION_NAME}")

    set(APK_FILE ${CMAKE_BINARY_DIR}/bin/${RENDERDOC_ANDROID_PACKAGE_NAME}.apk)
    add_custom_target(apk ALL
                      DEPENDS ${APK_FILE})

    # Copy in android package files, replacing the package name with the architecture-specific package name
    configure_file(android/Loader.java ${CMAKE_CURRENT_BINARY_DIR}/src/renderdoc/org/demos/Loader.java)
    configure_file(android/AndroidManifest.xml ${CMAKE_CURRENT_BINARY_DIR}/AndroidManifest.xml)
    configure_file(android/icon.png ${CMAKE_CURRENT_BINARY_DIR}/res/drawable/icon.png COPYONLY)

    add_custom_command(OUTPUT ${APK_FILE}
                       DEPENDS demos
                       DEPENDS ${KEYSTORE}
                       WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
                       COMMAND ${CMAKE_COMMAND} -E make_directory libs/lib/${ANDROID_ABI}
                       COMMAND ${CMAKE_COMMAND} -E make_directory obj
                       COMMAND ${CMAKE_COMMAND} -E make_directory bin
                       COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:demos> libs/lib/${ANDROID_ABI}/$<TARGET_FILE_NAME:demos>
                       )

    set(D8_SCRIPT "${BUILD_TOOLS}/d8${TOOL_SCRIPT_EXTENSION}")
    if(NOT EXISTS ${D8_SCRIPT})
        set(DEX_COMMAND ${BUILD_TOOLS}/dx${TOOL_SCRIPT_EXTENSION} --dex --output=bin/classes.dex ./obj)
    else()
        set(DEX_COMMAND ${D8_SCRIPT} --output ./bin/ ./obj/renderdoc/org/demos/${ABI_EXTENSION_NAME}/*.class)
    endif()

    add_custom_command(OUTPUT ${APK_FILE} APPEND
                       COMMAND ${CMAKE_COMMAND} -E remove ${APK_FILE} # Don't package any existing artifact into the new one
                       COMMAND ${BUILD_TOOLS}/aapt package -f -m -S res -J src -M AndroidManifest.xml -I ${ANDROID_JAR}
                       COMMAND ${JAVA_BIN}/javac -d ./obj -source 1.7 -target 1.7 -bootclasspath ${RT_JAR} -classpath "${CLASS_PATH}" -sourcepath src src/renderdoc/org/demos/*.java
                       COMMAND ${DEX_COMMAND}
                       COMMAND ${BUILD_TOOLS}/aapt package -f -M AndroidManifest.xml --version-code ${APK_VERSION_CODE} --version-name ${APK_VERSION_NAME} -S res -I ${ANDROID_JAR} -F demos-unaligned.apk bin libs
                       COMMAND ${BUILD_TOOLS}/zipalign -f 4 demos-unaligned.apk demos.apk
                       COMMAND ${BUILD_TOOLS}/apksigner${TOOL_SCRIPT_EXTENSION} sign --ks ${KEYSTORE} --ks-pass pass:android --key-pass pass:android --ks-key-alias rdocandroidkey demos.apk
                       COMMAND ${CMAKE_COMMAND} -E copy demos.apk ${APK_FILE})

endif()
