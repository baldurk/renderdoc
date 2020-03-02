set(sources
    arm_counters.h
)

if(ANDROID)
    list(APPEND sources
        arm_counters.cpp

        official/lizard/include/lizard/lizard_api.h
        official/lizard/lizard_api.cpp

        official/lizard/include/lizard/lizard.hpp
        official/lizard/lizard.cpp
        official/lizard/lizard_counter.cpp
        official/lizard/lizard_communication.hpp
        official/lizard/lizard_communication.cpp
        official/lizard/hwcpipe_communication.hpp
        official/lizard/hwcpipe_communication.cpp
        official/lizard/hwcpipe_api.hpp
        official/lizard/hwcpipe_api.cpp
        official/lizard/gatord_xml_reader.cpp

        official/lizard/gator_api.cpp
        official/lizard/gator_constants.hpp
        official/lizard/gator_message.cpp
        official/lizard/gator_message.hpp
        official/lizard/message_util.cpp
        official/lizard/message_util.hpp
        official/lizard/socket.cpp

        official/lizard/thirdparty/hwcpipe/hwcpipe.h
        official/lizard/thirdparty/hwcpipe/hwcpipe.cpp
        official/lizard/thirdparty/hwcpipe/cpu_profiler.h
        official/lizard/thirdparty/hwcpipe/gpu_profiler.h
        official/lizard/thirdparty/hwcpipe/value.h

        official/lizard/thirdparty/hwcpipe/vendor/arm/mali/hwc.hpp
        official/lizard/thirdparty/hwcpipe/vendor/arm/mali/hwc_names.hpp
        official/lizard/thirdparty/hwcpipe/vendor/arm/mali/mali_profiler.h
        official/lizard/thirdparty/hwcpipe/vendor/arm/mali/mali_profiler.cpp

        official/lizard/thirdparty/hwcpipe/vendor/arm/pmu/pmu_counter.h
        official/lizard/thirdparty/hwcpipe/vendor/arm/pmu/pmu_counter.cpp
        official/lizard/thirdparty/hwcpipe/vendor/arm/pmu/pmu_profiler.h
        official/lizard/thirdparty/hwcpipe/vendor/arm/pmu/pmu_profiler.cpp
    )
else()
    list(APPEND sources
         arm_counters_stub.cpp
    )
endif()

add_library(rdoc_arm OBJECT ${sources})

set(include_dirs
    ${RDOC_INCLUDES}
    official/lizard/
    official/lizard/include/lizard
    official/lizard/thirdparty/hwcpipe/
)
target_include_directories(rdoc_arm ${include_dirs})
target_compile_definitions(rdoc_arm ${RDOC_DEFINITIONS})
