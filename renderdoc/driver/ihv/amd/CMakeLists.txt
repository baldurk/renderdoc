set(sources
    amd_isa.cpp
    amd_isa.h
    amd_isa_devices.cpp
    amd_isa_devices.h
    amd_isa_posix.cpp)

add_library(rdoc_amd OBJECT ${sources})
target_compile_definitions(rdoc_amd ${RDOC_DEFINITIONS})
target_include_directories(rdoc_amd ${RDOC_INCLUDES})
