set(sources
    renderdoccmd.cpp
    renderdoccmd.h
    3rdparty/cmdline/cmdline.h
    )
set(includes PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty ${CMAKE_SOURCE_DIR}/renderdoc/api)
set(libraries PRIVATE renderdoc)

if(PythonLibs_FOUND)
    add_definitions(-DPYTHON_VERSION_MINOR=${PYTHON_VERSION_MINOR})
else()
    add_definitions(-DPYTHON_VERSION_MINOR=0)
endif()

if(APPLE)
    list(APPEND sources renderdoccmd_apple.cpp apple/cocoa_window.mm)

    set_property(SOURCE renderdoccmd_apple.cpp APPEND_STRING PROPERTY COMPILE_FLAGS " -Wno-shadow")

    find_library(COCOA_LIBRARY Cocoa)
    list(APPEND libraries PRIVATE ${COCOA_LIBRARY})

    find_library(QUARTZCORE_LIBRARY QuartzCore)
    list(APPEND libraries PRIVATE ${QUARTZCORE_LIBRARY})

elseif(ANDROID)
    string(REPLACE "\\" "/" GLUE_SOURCE "${ANDROID_NDK_ROOT_PATH}/sources/android/native_app_glue/android_native_app_glue.c")
    list(APPEND sources renderdoccmd_android.cpp "${GLUE_SOURCE}")
    include_directories(${ANDROID_NDK_ROOT_PATH}/sources/android/native_app_glue)
    list(APPEND libraries PRIVATE -llog -landroid)
    set(LINKER_FLAGS "-Wl,--no-as-needed")

    if(ENABLE_ASAN)
        set(LINKER_FLAGS "${LINKER_FLAGS} -fsanitize=address")
    endif()

    set_source_files_properties(renderdoccmd.cpp PROPERTIES COMPILE_FLAGS "-fexceptions -frtti")
    set_source_files_properties(renderdoccmd_android.cpp PROPERTIES COMPILE_FLAGS "-fexceptions")

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set_property(SOURCE renderdoccmd_android.cpp APPEND_STRING PROPERTY COMPILE_FLAGS " -Wno-shadow")
endif()

elseif(ENABLE_GGP)
    list(APPEND sources renderdoccmd_ggp.cpp)
    find_library(GGP_LIBRARY ggp)
    list(APPEND libraries PRIVATE ${GGP_LIBRARY})
    set(LINKER_FLAGS "-Wl,--no-as-needed -Wl,--hash-style=gnu -Wl,--build-id=sha1")
    set_property(SOURCE renderdoccmd_ggp.cpp APPEND_STRING PROPERTY COMPILE_FLAGS " -Wno-shadow")
elseif(UNIX)
    list(APPEND sources renderdoccmd_linux.cpp)

    if(FREEBSD)
        list(APPEND libraries PRIVATE -L/usr/local/lib)
        list(APPEND includes /usr/local/include)
    endif()

    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        set_property(SOURCE renderdoccmd_linux.cpp APPEND_STRING PROPERTY COMPILE_FLAGS " -Wno-shadow")
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
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${LINKER_FLAGS}")
    add_library(renderdoccmd SHARED ${sources})
else()
    set(CMAKE_SKIP_BUILD_RPATH TRUE)
    set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
    if(APPLE)
        set(CMAKE_MACOSX_RPATH TRUE)
        set(CMAKE_INSTALL_RPATH "@executable_path/../lib")
    else()
        set(CMAKE_INSTALL_RPATH "$ORIGIN/:$ORIGIN/../lib${LIB_SUFFIX}/${LIB_SUBFOLDER_TRAIL_SLASH}")
    endif()
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${LINKER_FLAGS}")

    add_executable(renderdoccmd ${sources})
    
    if(INTERNAL_SELF_CAPTURE)
        set_target_properties(renderdoccmd PROPERTIES OUTPUT_NAME "rdocselfcmd")
    endif()
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set_property(SOURCE renderdoccmd.cpp APPEND_STRING PROPERTY COMPILE_FLAGS " -Wno-shadow")
endif()

target_include_directories(renderdoccmd ${includes})
target_link_libraries(renderdoccmd ${libraries})

install (TARGETS renderdoccmd DESTINATION bin)

if(ANDROID)
    # Android sets this to off becuase Android is always terrible forever.
    # It breaks finding java in the path, so enable it again
    set(CMAKE_FIND_USE_SYSTEM_ENVIRONMENT_PATH ON)

    #############################
    # We need to check that 'java' in PATH is new enough. Temporarily unset the JAVA_HOME env,
    # then invoke FindJava.cmake which will search just the PATH, then re-set it.
    set(SAVE_JAVA_HOME $ENV{JAVA_HOME})

    set(ENV{JAVA_HOME} "")
    find_package(Java)
    set(ENV{JAVA_HOME} ${SAVE_JAVA_HOME})

    if(NOT ${Java_FOUND})
        message(FATAL_ERROR "Building Android requires the 'java' program in your PATH. It must be at least Java 8 (1.8)")
    endif()

    if(${Java_VERSION} VERSION_LESS 1.8)
        message(FATAL_ERROR "Building Android requires the 'java' program in your PATH to be at least Java 8 (1.8)")
    endif()
    message(STATUS "Using Java of version ${Java_VERSION}")

    if(STRIP_ANDROID_LIBRARY AND ANDROID_STRIP_TOOL AND RELEASE_MODE)
        add_custom_command(TARGET renderdoccmd POST_BUILD
            COMMAND echo Stripping $<TARGET_FILE:renderdoccmd>
            COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:renderdoccmd> $<TARGET_FILE:renderdoccmd>.dbg
            COMMAND ${ANDROID_STRIP_TOOL} --strip-unneeded $<TARGET_FILE:renderdoccmd>)
    endif()

    set(ANDROID_BUILD_TOOLS_VERSION "" CACHE STRING "Version of Android build-tools to use instead of the default")
    if(ANDROID_BUILD_TOOLS_VERSION STREQUAL "")
        # Enumerate the build tools versions available, and pick the most recent
        file(GLOB __buildTools RELATIVE "${ANDROID_SDK_ROOT_PATH}/build-tools" "${ANDROID_SDK_ROOT_PATH}/build-tools/*")
        list(SORT __buildTools)

        list(GET __buildTools -1 ANDROID_BUILD_TOOLS_VERSION)

        unset(__buildTools)
    endif()
    message(STATUS "Using Android build-tools version ${ANDROID_BUILD_TOOLS_VERSION}")

    set(APK_TARGET_ID "" CACHE STRING "The Target ID to build the APK for like 'android-99', use <android list targets> to choose another one.")
    if(APK_TARGET_ID STREQUAL "")
        # This seems different from the platform we're targetting,
        # default to the latest available that's greater or equal to our target platform
        file(GLOB __platforms RELATIVE "${ANDROID_SDK_ROOT_PATH}/platforms" "${ANDROID_SDK_ROOT_PATH}/platforms/android-*")
        list(SORT __platforms)

        # In case we don't find one, target the latest platform
        list(GET __platforms -1 APK_TARGET_ID)

        string(REPLACE "android-" "" __targetPlat "${ANDROID_PLATFORM}")

        # We require at least android 23 for Activity.requestPermissions
        if(__targetPlat LESS 23)
            set(__targetPlat 23)
        endif()

        foreach( __plat ${__platforms})
            string(REPLACE "android-" "" __curPlat "${__plat}")

            if(NOT (__curPlat LESS __targetPlat) )
                set(APK_TARGET_ID "android-${__curPlat}")
                break()
            endif()
        endforeach()

        unset(__platforms)
        unset(__targetPlat)
        unset(__curPlat)
    endif()
    message(STATUS "Using android.jar from platform ${APK_TARGET_ID}")

    # Suffix for scripts rather than binaries, which is needed explicitly on windows
    set(TOOL_SCRIPT_EXTENSION "")
    if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Windows")
        set(TOOL_SCRIPT_EXTENSION ".bat")
    endif()

    set(BUILD_TOOLS "${ANDROID_SDK_ROOT_PATH}/build-tools/${ANDROID_BUILD_TOOLS_VERSION}")
    set(RT_JAR "$ENV{JAVA_HOME}/jre/lib/rt.jar")
    set(JAVA_BIN "$ENV{JAVA_HOME}/bin")

    string(REPLACE "\\" "/" ANDROID_JAR "${ANDROID_SDK_ROOT_PATH}/platforms/${APK_TARGET_ID}/android.jar")
    if(CMAKE_HOST_WIN32)
        set(CLASS_PATH "${ANDROID_JAR}\;obj")
    else()
        set(CLASS_PATH "${ANDROID_JAR}:obj")
    endif()
    set(KEYSTORE ${CMAKE_CURRENT_BINARY_DIR}/debug.keystore)
    add_custom_command(OUTPUT ${KEYSTORE}
                       WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
                       COMMAND ${JAVA_BIN}/keytool -genkey -keystore ${KEYSTORE} -storepass android -alias rdocandroidkey -keypass android -keyalg RSA -keysize 2048 -validity 10000 -dname "CN=, OU=, O=, L=, S=, C=")

    # APK_VERSION_CODE corresponds to android:versionCode, an internal integer value that can be queried. Higher numbers indicate more recent versions.
    # APK_VERSION_NAME corresponds to android:versionName, a string value that is displayed to the user.
    set(APK_VERSION_CODE "${RENDERDOC_VERSION_MAJOR}${RENDERDOC_VERSION_MINOR}")
    set(APK_VERSION_NAME ${GIT_COMMIT_HASH})
    message(STATUS "Building APK versionCode ${APK_VERSION_CODE}, versionName ${APK_VERSION_NAME}")

    # Set the package name based on the ABI
    if(ANDROID_ABI STREQUAL "armeabi-v7a")
        set(ABI_EXTENSION_NAME "arm32")
    elseif(ANDROID_ABI STREQUAL "arm64-v8a")
        set(ABI_EXTENSION_NAME "arm64")
    elseif(ANDROID_ABI STREQUAL "x86")
        set(ABI_EXTENSION_NAME "x86")
    elseif(ANDROID_ABI STREQUAL "x86_64")
        set(ABI_EXTENSION_NAME "x64")
    else()
        message(FATAL_ERROR "ABI ${ANDROID_ABI} is not supported.")
    endif()

    set(RENDERDOC_ANDROID_PACKAGE_NAME "org.renderdoc.renderdoccmd.${ABI_EXTENSION_NAME}")

    set(APK_FILE ${CMAKE_BINARY_DIR}/bin/${RENDERDOC_ANDROID_PACKAGE_NAME}.apk)
    add_custom_target(apk ALL
                      DEPENDS ${APK_FILE})
                  
    option(ENABLE_CUSTOM_WRAP_SCRIPT "Enable custom wrap.sh on Android to workaround Android bug" ON)

    # Copy in android package files, replacing the package name with the architecture-specific package name
    configure_file(android/Loader.java ${CMAKE_CURRENT_BINARY_DIR}/src/org/renderdoc/renderdoccmd/Loader.java)
    configure_file(android/AndroidManifest.xml ${CMAKE_CURRENT_BINARY_DIR}/AndroidManifest.xml)
    configure_file(android/icon.png ${CMAKE_CURRENT_BINARY_DIR}/res/drawable/icon.png COPYONLY)

    add_custom_command(OUTPUT ${APK_FILE}
                       DEPENDS renderdoc
                       DEPENDS renderdoccmd
                       DEPENDS ${KEYSTORE}
                       WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
                       COMMAND ${CMAKE_COMMAND} -E make_directory libs/lib/${ANDROID_ABI}
                       COMMAND ${CMAKE_COMMAND} -E make_directory obj
                       COMMAND ${CMAKE_COMMAND} -E make_directory bin
                       COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:renderdoc> libs/lib/${ANDROID_ABI}/$<TARGET_FILE_NAME:renderdoc>
                       COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:renderdoccmd> libs/lib/${ANDROID_ABI}/$<TARGET_FILE_NAME:renderdoccmd>
                       )
    
    if(ENABLE_CUSTOM_WRAP_SCRIPT)
        # use configure_file to ensure unix newlines regardless of how it is in the repository (e.g. CRLF on windows)
        configure_file(android/wrap.sh ${CMAKE_CURRENT_BINARY_DIR}/libs/lib/${ANDROID_ABI}/wrap.sh @ONLY NEWLINE_STYLE UNIX)

        message(STATUS "Using custom wrap script for Android bug workaround")
        add_custom_command(OUTPUT ${APK_FILE} APPEND
            COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/libs/lib/${ANDROID_ABI}/wrap.sh libs/lib/${ANDROID_ABI}/wrap.sh
            )
    else()
        message(WARNING "Without custom wrap script, some Android versions will break due to Android bug")
    endif()

    if(ENABLE_ASAN)
        set(ASAN_ABI_TAG "")
        if(ANDROID_ABI STREQUAL "armeabi-v7a")
            set(ASAN_ABI_TAG "arm")
        elseif(ANDROID_ABI STREQUAL "arm64-v8a")
            set(ASAN_ABI_TAG "aarch64")
        else()
            message(WARNING "Unknown ABI ${ANDROID_ABI}, libasan tag unknown")
        endif()

        file(GLOB ASAN_LIBRARIES "${ANDROID_TOOLCHAIN_ROOT}/lib64/clang/*/lib/linux/libclang_rt.asan-${ASAN_ABI_TAG}-android.so")
        list(SORT ASAN_LIBRARIES)

        list(GET ASAN_LIBRARIES -1 ASAN_LIBRARY)

        set(WRAP_SCRIPT "${ANDROID_NDK_ROOT_PATH}/wrap.sh/asan.sh")

        string(REPLACE "\\" "/" WRAP_SCRIPT "${WRAP_SCRIPT}")
        string(REPLACE "\\" "/" ASAN_LIBRARY "${ASAN_LIBRARY}")

        # Copy in the wrap script and libasan library
        if(${WRAP_SCRIPT} STREQUAL "" OR NOT EXISTS ${WRAP_SCRIPT})
            message(WARNING "Wrap script couldn't be found in NDK, you will need to manually create one and re-generate apk")
        elseif(${ASAN_LIBRARY} STREQUAL "" OR NOT EXISTS ${ASAN_LIBRARY})
            message(WARNING "libasan library couldn't be found in NDK, you will need to manually copy it in and re-generate apk")
        else()
            if(ENABLE_CUSTOM_WRAP_SCRIPT)
                message(STATUS "Chaining to wrap script ${WRAP_SCRIPT} and libasan library ${ASAN_LIBRARY}")
                add_custom_command(OUTPUT ${APK_FILE} APPEND
                    COMMAND ${CMAKE_COMMAND} -E copy ${WRAP_SCRIPT} libs/lib/${ANDROID_ABI}/asan.sh
                    COMMAND ${CMAKE_COMMAND} -E copy ${ASAN_LIBRARY} libs/lib/${ANDROID_ABI}/
                    )
            else()
                message(STATUS "Directly using wrap script ${WRAP_SCRIPT} and libasan library ${ASAN_LIBRARY}")
                add_custom_command(OUTPUT ${APK_FILE} APPEND
                    COMMAND ${CMAKE_COMMAND} -E copy ${WRAP_SCRIPT} libs/lib/${ANDROID_ABI}/wrap.sh
                    COMMAND ${CMAKE_COMMAND} -E copy ${ASAN_LIBRARY} libs/lib/${ANDROID_ABI}/
                    )
            endif()
        endif()
    endif()

    set(D8_SCRIPT "${BUILD_TOOLS}/d8${TOOL_SCRIPT_EXTENSION}")
    if(NOT EXISTS ${D8_SCRIPT})
        set(DEX_COMMAND ${BUILD_TOOLS}/dx${TOOL_SCRIPT_EXTENSION} --dex --output=bin/classes.dex ./obj)
    else()
        set(DEX_COMMAND ${D8_SCRIPT} --output ./bin/ ./obj/org/renderdoc/renderdoccmd/${ABI_EXTENSION_NAME}/*.class)
    endif()

    add_custom_command(OUTPUT ${APK_FILE} APPEND
                       COMMAND ${BUILD_TOOLS}/aapt package -f -m -S res -J src -M AndroidManifest.xml -I ${ANDROID_JAR}
                       COMMAND ${JAVA_BIN}/javac -d ./obj -source 1.7 -target 1.7 -bootclasspath ${RT_JAR} -classpath "${CLASS_PATH}" -sourcepath src src/org/renderdoc/renderdoccmd/*.java
                       COMMAND ${DEX_COMMAND}
                       COMMAND ${BUILD_TOOLS}/aapt package -f -M AndroidManifest.xml --version-code ${APK_VERSION_CODE} --version-name ${APK_VERSION_NAME} -S res -I ${ANDROID_JAR} -F RenderDocCmd-unaligned.apk bin libs
                       COMMAND ${BUILD_TOOLS}/zipalign -f 4 RenderDocCmd-unaligned.apk RenderDocCmd.apk
                       COMMAND ${BUILD_TOOLS}/apksigner${TOOL_SCRIPT_EXTENSION} sign --ks ${KEYSTORE} --ks-pass pass:android --key-pass pass:android --ks-key-alias rdocandroidkey RenderDocCmd.apk
                       COMMAND ${CMAKE_COMMAND} -E copy RenderDocCmd.apk ${APK_FILE})

endif()
