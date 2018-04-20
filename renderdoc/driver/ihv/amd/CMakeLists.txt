set(sources
    amd_counters.cpp
    amd_counters.h
    amd_isa.cpp
    amd_isa.h
    amd_isa_devices.cpp
    amd_isa_devices.h
    amd_isa_posix.cpp
    amd_rgp.cpp
    amd_rgp.h)

set(rgp_sources
    official/RGP/DevDriverAPI/RGPClientInProcessModel.cpp
    official/RGP/DevDriverAPI/RGPClientInProcessModel.h
    official/RGP/DevDriverAPI/DevDriverAPI.cpp
    official/RGP/DevDriverAPI/DevDriverAPI.h)

set(dev_driver_sources
    official/RGP/DevDriverComponents/listener/ddListenerURIService.cpp
    official/RGP/DevDriverComponents/listener/ddListenerURIService.h
    official/RGP/DevDriverComponents/listener/hostMsgTransport.h
    official/RGP/DevDriverComponents/listener/hostMsgTransport.cpp
    official/RGP/DevDriverComponents/listener/listenerServer.h
    official/RGP/DevDriverComponents/listener/listenerServer.cpp
    official/RGP/DevDriverComponents/listener/transports/hostTransport.h
    official/RGP/DevDriverComponents/listener/transports/hostTransport.cpp
    official/RGP/DevDriverComponents/listener/routerCore.h
    official/RGP/DevDriverComponents/listener/routerCore.cpp
    official/RGP/DevDriverComponents/listener/listenerCore.h
    official/RGP/DevDriverComponents/listener/listenerCore.cpp
    official/RGP/DevDriverComponents/listener/transportThread.h
    official/RGP/DevDriverComponents/listener/transportThread.cpp
    official/RGP/DevDriverComponents/listener/transports/abstractListenerTransport.h
    official/RGP/DevDriverComponents/listener/transports/socketTransport.h
    official/RGP/DevDriverComponents/listener/transports/socketTransport.cpp
    official/RGP/DevDriverComponents/listener/clientmanagers/abstractClientManager.h
    official/RGP/DevDriverComponents/listener/clientmanagers/listenerClientManager.h
    official/RGP/DevDriverComponents/listener/clientmanagers/listenerClientManager.cpp
    official/RGP/DevDriverComponents/src/baseProtocolClient.cpp
    official/RGP/DevDriverComponents/src/baseProtocolServer.cpp
    official/RGP/DevDriverComponents/src/ddClientURIService.cpp
    official/RGP/DevDriverComponents/src/ddClientURIService.h
    official/RGP/DevDriverComponents/src/ddSocket.h
    official/RGP/DevDriverComponents/src/ddTransferManager.cpp
    official/RGP/DevDriverComponents/src/devDriverClient.cpp
    official/RGP/DevDriverComponents/src/devDriverServer.cpp
    official/RGP/DevDriverComponents/src/messageChannel.h
    official/RGP/DevDriverComponents/src/messageChannel.inl
    official/RGP/DevDriverComponents/src/session.h
    official/RGP/DevDriverComponents/src/session.cpp
    official/RGP/DevDriverComponents/src/sessionManager.h
    official/RGP/DevDriverComponents/src/sessionManager.cpp
    official/RGP/DevDriverComponents/src/socketMsgTransport.h
    official/RGP/DevDriverComponents/src/socketMsgTransport.cpp
    official/RGP/DevDriverComponents/src/protocols/ddGpuCrashDumpClient.cpp
    official/RGP/DevDriverComponents/src/protocols/ddGpuCrashDumpServer.cpp
    official/RGP/DevDriverComponents/src/protocols/ddTransferClient.cpp
    official/RGP/DevDriverComponents/src/protocols/ddTransferServer.cpp
    official/RGP/DevDriverComponents/src/protocols/ddURIClient.cpp
    official/RGP/DevDriverComponents/src/protocols/ddURIServer.cpp
    official/RGP/DevDriverComponents/src/protocols/ddURIServer.h
    official/RGP/DevDriverComponents/src/protocols/driverControlClient.cpp
    official/RGP/DevDriverComponents/src/protocols/driverControlServer.cpp
    official/RGP/DevDriverComponents/src/protocols/loggingClient.cpp
    official/RGP/DevDriverComponents/src/protocols/loggingServer.cpp
    official/RGP/DevDriverComponents/src/protocols/rgpClient.cpp
    official/RGP/DevDriverComponents/src/protocols/rgpServer.cpp
    official/RGP/DevDriverComponents/src/protocols/settingsClient.cpp
    official/RGP/DevDriverComponents/src/protocols/settingsServer.cpp

    official/RGP//DevDriverComponents/src/posix/ddPosixPlatform.cpp
    official/RGP//DevDriverComponents/src/posix/ddPosixSocket.cpp
)

set(include_dirs ${RDOC_INCLUDES})

if(UNIX AND NOT ANDROID AND NOT APPLE)
    add_compile_options(-D_LINUX -DGPUOPEN_CLIENT_INTERFACE_MAJOR_VERSION=30)

    list(APPEND sources ${dev_driver_sources} ${rgp_sources})
    list(APPEND include_dirs official/RGP/DevDriverComponents/inc)
endif()

add_library(rdoc_amd OBJECT ${sources})
target_compile_definitions(rdoc_amd ${RDOC_DEFINITIONS})
target_include_directories(rdoc_amd ${include_dirs})
