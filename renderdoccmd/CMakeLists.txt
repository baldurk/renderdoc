set(sources renderdoccmd.cpp)
set(definitions)
set(includes PRIVATE ${CMAKE_SOURCE_DIR}/renderdoc/api)
set(libraries PRIVATE renderdoc)

if(WIN32)
    list(APPEND sources renderdoccmd_win32.cpp)
    list(APPEND definitions PRIVATE -DWIN32)
elseif(UNIX)
    list(APPEND sources renderdoccmd_linux.cpp)
    list(APPEND definitions PRIVATE -DLINUX)

    list(APPEND libraries PRIVATE -lxcb)

    find_package(OpenGL REQUIRED)
    list(APPEND includes PRIVATE ${OPENGL_INCLUDE_DIR})
    list(APPEND libraries PRIVATE ${OPENGL_gl_LIBRARY})
endif()

add_executable(renderdoccmd ${sources})
target_compile_definitions(renderdoccmd ${definitions})
target_include_directories(renderdoccmd ${includes})
target_link_libraries(renderdoccmd ${libraries})
