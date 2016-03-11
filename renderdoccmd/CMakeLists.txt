set(sources renderdoccmd.cpp)
set(definitions)
set(includes PRIVATE ${CMAKE_SOURCE_DIR}/renderdoc/api)
set(libraries PRIVATE renderdoc)

if(UNIX)
    list(APPEND sources renderdoccmd_linux.cpp)
    list(APPEND definitions PRIVATE -DLINUX)

    find_package(OpenGL REQUIRED)
    list(APPEND includes PRIVATE ${OPENGL_INCLUDE_DIR})
    list(APPEND libraries PRIVATE ${OPENGL_gl_LIBRARY})

    list(APPEND libraries PRIVATE -lxcb)
endif()

add_executable(renderdoccmd ${sources})
target_compile_definitions(renderdoccmd ${definitions})
target_include_directories(renderdoccmd ${includes})
target_link_libraries(renderdoccmd ${libraries})
