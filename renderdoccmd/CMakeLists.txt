set(sources renderdoccmd.cpp)
set(includes PRIVATE ${CMAKE_SOURCE_DIR}/renderdoc/api)
set(libraries PRIVATE renderdoc)

if(APPLE)
    list(APPEND sources renderdoccmd_apple.cpp)
elseif(ANDROID)
    list(APPEND sources renderdoccmd_android.cpp)
    include_directories(${ANDROID_NDK}/sources/android/native_app_glue)
    list(APPEND libraries PRIVATE -llog -landroid)
    set(LINKER_FLAGS "-Wl,--no-as-needed")
elseif(UNIX)
    list(APPEND sources renderdoccmd_linux.cpp)

    if(ENABLE_GL)
        find_package(OpenGL REQUIRED)
        list(APPEND includes PRIVATE ${OPENGL_INCLUDE_DIR})
        list(APPEND libraries PRIVATE ${OPENGL_gl_LIBRARY})
    endif()

    if(ENABLE_GLES)
        list(APPEND libraries PRIVATE -lEGL)
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
    set(CMAKE_SHARED_LINKER_FLAGS "${LINKER_FLAGS}")
    add_library(renderdoccmd SHARED ${sources})
else()
    set(CMAKE_SKIP_BUILD_RPATH TRUE)
    set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
    set(CMAKE_INSTALL_RPATH "$ORIGIN/:$ORIGIN/../lib/")
    set(CMAKE_EXE_LINKER_FLAGS "${LINKER_FLAGS}")

    add_executable(renderdoccmd ${sources})
endif()

target_include_directories(renderdoccmd ${includes})
target_link_libraries(renderdoccmd ${libraries})

install (TARGETS renderdoccmd DESTINATION bin)

if(ANDROID)
    if(NOT DEFINED ENV{JAVA_HOME})
        message(FATAL_ERROR "JAVA_HOME environment variable must be defined for Android build")
    endif()
    if(NOT DEFINED ENV{ANDROID_SDK})
        message(FATAL_ERROR "ANDROID_SDK environment variable must be defined for Android build")
    endif()

    set(ANDROID_BUILD_TOOLS_VERSION "" CACHE STRING "Version of Android build-tools to use instead of the default")
    if(ANDROID_BUILD_TOOLS_VERSION STREQUAL "")
        set(ANDROID_BUILD_TOOLS_VERSION "26.0.1")
    endif()
    message(STATUS "Using Android build-tools version ${ANDROID_BUILD_TOOLS_VERSION}")

    set(BUILD_TOOLS "$ENV{ANDROID_SDK}/build-tools/${ANDROID_BUILD_TOOLS_VERSION}")
    set(RT_JAR "$ENV{JAVA_HOME}/jre/lib/rt.jar")
    set(JAVA_BIN "$ENV{JAVA_HOME}/bin")

    set(APK_TARGET_ID "android-23" CACHE STRING "The Target ID to build the APK for, use <android list targets> to choose another one.")
    set(ANDROID_JAR "$ENV{ANDROID_SDK}/platforms/${APK_TARGET_ID}/android.jar")
    if(CMAKE_HOST_WIN32)
        set(CLASS_PATH "${ANDROID_JAR}\;obj")
    else()
        set(CLASS_PATH "${ANDROID_JAR}:obj")
    endif()
    set(KEYSTORE ${CMAKE_CURRENT_BINARY_DIR}/debug.keystore)
    add_custom_command(OUTPUT ${KEYSTORE}
                       WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
                       COMMAND ${JAVA_BIN}/keytool -genkey -keystore ${KEYSTORE} -storepass android -alias androiddebugkey -keypass android -keyalg RSA -keysize 2048 -validity 10000 -dname "CN=, OU=, O=, L=, S=, C=")

    # APK_VERSION_CODE corresponds to android:versionCode, an internal integer value that can be queried. Higher numbers indicate more recent versions.
    # APK_VERSION_NAME corresponds to android:versionName, a string value that is displayed to the user.
    set(APK_VERSION_CODE "${RENDERDOC_VERSION_MAJOR}${RENDERDOC_VERSION_MINOR}")
    if(BUILD_VERSION_STABLE)
        set(APK_VERSION_NAME "${RENDERDOC_VERSION}")
    else()
        set(APK_VERSION_NAME ${GIT_COMMIT_HASH})
    endif()
    message(STATUS "Building APK versionCode ${APK_VERSION_CODE}, versionName ${APK_VERSION_NAME}")


    set(APK_FILE ${CMAKE_BINARY_DIR}/bin/RenderDocCmd.apk)
    add_custom_target(apk ALL
                      DEPENDS ${APK_FILE}
                      DEPENDS ${KEYSTORE})

    add_custom_command(OUTPUT ${APK_FILE}
                       DEPENDS renderdoc
                       DEPENDS renderdoccmd
                       WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
                       COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/android ${CMAKE_CURRENT_BINARY_DIR}
                       COMMAND ${CMAKE_COMMAND} -E make_directory libs/lib/${ANDROID_ABI}
                       COMMAND ${CMAKE_COMMAND} -E make_directory obj
                       COMMAND ${CMAKE_COMMAND} -E make_directory bin
                       COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:renderdoc> libs/lib/${ANDROID_ABI}/libVkLayer_GLES_RenderDoc.so
                       COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:renderdoccmd> libs/lib/${ANDROID_ABI}/$<TARGET_FILE_NAME:renderdoccmd>
                       COMMAND ${BUILD_TOOLS}/aapt package -f -m -S res -J src -M AndroidManifest.xml -I ${ANDROID_JAR}
                       COMMAND ${JAVA_BIN}/javac -d ./obj -source 1.7 -target 1.7 -bootclasspath ${RT_JAR} -classpath "${CLASS_PATH}" -sourcepath src src/org/renderdoc/renderdoccmd/*.java
                       COMMAND ${BUILD_TOOLS}/dx --dex --output=bin/classes.dex ./obj
                       COMMAND ${BUILD_TOOLS}/aapt package -f -M AndroidManifest.xml --version-code ${APK_VERSION_CODE} --version-name ${APK_VERSION_NAME} -S res -I ${ANDROID_JAR} -F RenderDocCmd-unaligned.apk bin libs
                       COMMAND ${BUILD_TOOLS}/zipalign -f 4 RenderDocCmd-unaligned.apk RenderDocCmd.apk
                       COMMAND ${BUILD_TOOLS}/apksigner sign --ks ${KEYSTORE} --ks-pass pass:android --key-pass pass:android --ks-key-alias androiddebugkey RenderDocCmd.apk
                       COMMAND ${CMAKE_COMMAND} -E copy RenderDocCmd.apk ${APK_FILE})

endif()
