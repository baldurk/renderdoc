set(sources
    nv_counter_enumerator.cpp
    nv_counter_enumerator.h
    nv_gl_counters.h
    nv_vk_counters.h)

if(ENABLE_GL OR ENABLE_GLES)
    set(sources_gl nv_gl_counters.cpp)
endif()

if(ENABLE_VULKAN)
    set(sources_vulkan nv_vk_counters.cpp)
endif()

set(include_dirs
    ${RDOC_INCLUDES}
    "${CMAKE_CURRENT_SOURCE_DIR}/official/PerfSDK/redist/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/official/PerfSDK/redist/NvPerfUtility/include")

add_library(rdoc_nv OBJECT ${sources} ${sources_gl} ${sources_vulkan})
target_compile_definitions(rdoc_nv ${RDOC_DEFINITIONS})
target_include_directories(rdoc_nv ${include_dirs})
