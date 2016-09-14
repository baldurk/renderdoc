set(sources renderdoccmd.cpp)
set(includes PRIVATE ${CMAKE_SOURCE_DIR}/renderdoc/api)
set(libraries PRIVATE renderdoc)

if(APPLE)
    list(APPEND sources renderdoccmd_apple.cpp)
elseif(ANDROID)
    list(APPEND sources renderdoccmd_android.cpp)
    include_directories(${ANDROID_NDK}/sources/android/native_app_glue)
    list(APPEND libraries PRIVATE -llog -landroid)
elseif(UNIX)
    list(APPEND sources renderdoccmd_linux.cpp)

    if(ENABLE_GL)
        find_package(OpenGL REQUIRED)
        list(APPEND includes PRIVATE ${OPENGL_INCLUDE_DIR})
        list(APPEND libraries PRIVATE ${OPENGL_gl_LIBRARY})
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
endif()

if(ANDROID)
    add_library(renderdoccmd SHARED ${sources})
else()
    set(CMAKE_SKIP_BUILD_RPATH TRUE)
    set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
    set(CMAKE_INSTALL_RPATH "$ORIGIN/")

    add_executable(renderdoccmd ${sources})
endif()

target_include_directories(renderdoccmd ${includes})
target_link_libraries(renderdoccmd ${libraries})

install (TARGETS renderdoccmd DESTINATION bin)

add_custom_command(TARGET renderdoccmd POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_SOURCE_DIR}/bin/)
add_custom_command(TARGET renderdoccmd POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:renderdoccmd> ${CMAKE_SOURCE_DIR}/bin/)
