set(sources renderdoccmd.cpp)
set(includes PRIVATE ${CMAKE_SOURCE_DIR}/renderdoc/api)
set(libraries PRIVATE renderdoc)

if(APPLE)
    list(APPEND sources renderdoccmd_apple.cpp)
elseif(UNIX)
    list(APPEND sources renderdoccmd_linux.cpp)

    find_package(OpenGL REQUIRED)
    list(APPEND includes PRIVATE ${OPENGL_INCLUDE_DIR})
    list(APPEND libraries PRIVATE ${OPENGL_gl_LIBRARY})

    list(APPEND libraries PRIVATE -lxcb)
endif()

add_executable(renderdoccmd ${sources})
target_include_directories(renderdoccmd ${includes})
target_link_libraries(renderdoccmd ${libraries})
