set(sources
    amd_counters.cpp
    amd_counters.h
    amd_isa.cpp
    amd_isa.h
    amd_isa_devices.cpp
    amd_isa_devices.h
    amd_isa_posix.cpp
    amd_rgp.cpp
    amd_rgp.h
    official/RGP/DevDriverAPI.h)

set(include_dirs ${RDOC_INCLUDES})

add_library(rdoc_amd OBJECT ${sources})
target_compile_definitions(rdoc_amd ${RDOC_DEFINITIONS})
target_include_directories(rdoc_amd ${include_dirs})
