set(sources
    nv_counter_enumerator.cpp
    nv_counter_enumerator.h
    nv_vk_counters.cpp
    nv_vk_counters.h)

set(include_dirs
    ${RDOC_INCLUDES}
    "${CMAKE_CURRENT_SOURCE_DIR}/official/PerfSDK/redist/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/official/PerfSDK/redist/NvPerfUtility/include")

add_library(rdoc_nv OBJECT ${sources})
target_compile_definitions(rdoc_nv ${RDOC_DEFINITIONS})
target_include_directories(rdoc_nv ${include_dirs})
