set(sources renderdoccmd.cpp)
set(includes PRIVATE ${CMAKE_SOURCE_DIR}/renderdoc/api)
set(libraries PRIVATE renderdoc)

if(APPLE)
    list(APPEND sources renderdoccmd_apple.cpp)
elseif(ANDROID)
    list(APPEND sources renderdoccmd_android.cpp)
    include_directories(${ANDROID_NDK}/sources/android/native_app_glue)
    list(APPEND libraries PRIVATE -llog -landroid)
    set(LINKER_FLAGS "-Wl,--no-as-needed")
elseif(UNIX)
    list(APPEND sources renderdoccmd_linux.cpp)

    if(ENABLE_GL)
        find_package(OpenGL REQUIRED)
        list(APPEND includes PRIVATE ${OPENGL_INCLUDE_DIR})
        list(APPEND libraries PRIVATE ${OPENGL_gl_LIBRARY})
    endif()

    if(ENABLE_GLES)
        list(APPEND libraries PRIVATE -lEGL)
    endif()

    if(ENABLE_XLIB)
        list(APPEND libraries PRIVATE -lX11)
    endif()

    if(ENABLE_XCB)
        list(APPEND libraries PRIVATE -lxcb)
    endif()

    if(ENABLE_XLIB AND ENABLE_XCB)
        list(APPEND libraries PRIVATE -lX11-xcb)
    endif()

    # Make sure that for the target executable we don't throw away
    # any shared libraries.
    set(LINKER_FLAGS "-Wl,--no-as-needed")
endif()

if(ANDROID)
    set(CMAKE_SHARED_LINKER_FLAGS "${LINKER_FLAGS}")
    add_library(renderdoccmd SHARED ${sources})
else()
    set(CMAKE_SKIP_BUILD_RPATH TRUE)
    set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
    set(CMAKE_INSTALL_RPATH "$ORIGIN/:$ORIGIN/../lib/")
    set(CMAKE_EXE_LINKER_FLAGS "${LINKER_FLAGS}")

    add_executable(renderdoccmd ${sources})
endif()

target_include_directories(renderdoccmd ${includes})
target_link_libraries(renderdoccmd ${libraries})

install (TARGETS renderdoccmd DESTINATION bin)

if(ANDROID)
    set(APK_TARGET_ID "android-23" CACHE STRING "The Target ID to build the APK for, use <android list targets> to choose another one.")

    set(APK_FILE ${CMAKE_BINARY_DIR}/bin/RenderDocCmd.apk)
    add_custom_target(apk ALL
                      DEPENDS ${APK_FILE})
    add_custom_command(OUTPUT ${APK_FILE}
                       DEPENDS renderdoc
                       DEPENDS renderdoccmd
                       WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
                       COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/android ${CMAKE_CURRENT_BINARY_DIR}
                       COMMAND ${CMAKE_COMMAND} -E make_directory libs/${ANDROID_ABI}
                       COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:renderdoc> libs/${ANDROID_ABI}/$<TARGET_FILE_NAME:renderdoc>
                       COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:renderdoccmd> libs/${ANDROID_ABI}/$<TARGET_FILE_NAME:renderdoccmd>
                       COMMAND android update project --path . --name RenderDocCmd --target ${APK_TARGET_ID}
                       COMMAND ant debug
                       COMMAND ${CMAKE_COMMAND} -E copy bin/RenderDocCmd-debug.apk ${APK_FILE})
endif()
