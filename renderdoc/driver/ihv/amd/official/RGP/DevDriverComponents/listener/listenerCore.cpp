/*
 *******************************************************************************
 *
 * Copyright (c) 2016-2018 Advanced Micro Devices, Inc. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 ******************************************************************************/
/**
***********************************************************************************************************************
* @file  listenerCore.cpp
* @brief Class definition for ListenerCore
***********************************************************************************************************************
*/

#include "listenerCore.h"
#include <iostream>
#include "../inc/ddPlatform.h"

#ifdef _WIN32
#include "transports/winPipeTransport.h"
#endif
#include "transports/socketTransport.h"
#include "transports/hostTransport.h"
#include "clientmanagers/listenerClientManager.h"
#include "../src/messageChannel.h"
#include "hostMsgTransport.h"

#include "listenerServer.h"
#include "../inc/protocols/loggingServer.h"
#include "../inc/protocols/loggingProtocol.h"

#include <cstdarg>

#ifdef RDS_LOG_TO_PANEL
#include "../Common/ToolUtil.h"
#endif

namespace DevDriver
{
    // Static pointer to a logging server for use by the LogMessage function
    static LoggingProtocol::LoggingServer* pLogServer = nullptr;

    // Client manager routing prefix
    DD_STATIC_CONST ClientId kListenerClientManagerPrefix = (0x0000) & kRouterPrefixMask;

    // =====================================================================================================================
    // Logs a message to the console or the logging server if it's available
    void LogMessage(LogLevel logLevel, const char *format, ...)
    {
        char message[1024];
        va_list args;
        va_start(args, format);
#if defined(_WIN32)
        vsprintf_s(message, format, args);
#else
        vsprintf(message, format, args);
#endif
        va_end(args);
        if (pLogServer != nullptr)
        {
            using namespace DevDriver::LoggingProtocol;
            pLogServer->Log(logLevel, kGeneralCategoryMask, message);
        }

#ifdef RDS_LOG_TO_PANEL
        ToolUtil::DbgMsg(message);
#else
        Platform::DebugPrint(logLevel, message);
#endif
    }

    // =====================================================================================================================
    // Constructs a vector of the currently connected clients and returns it
    std::vector<ClientInfo> ListenerCore::GetConnectedClientList()
    {
        return m_routerCore.GetConnectedClientList();
    }

    // =====================================================================================================================
    // Constructor
    ListenerCore::ListenerCore() :
        m_createInfo(),
        m_routerCore(),
        m_pClientManager(nullptr),
        m_started(false),
        m_pMsgChannel(nullptr),
        m_pServer(nullptr)
    {
    }

    // =====================================================================================================================
    // Destructor
    ListenerCore::~ListenerCore()
    {
        Destroy();
    }

    // =====================================================================================================================
    // Initializes the listener core object and binds to all provided addresses
    Result ListenerCore::Initialize(ListenerCreateInfo& createInfo)
    {
        DD_ASSERT(m_pClientManager == nullptr);
        DD_ASSERT(m_pMsgChannel == nullptr);

        Result result = Result::Unavailable;

        std::lock_guard<std::mutex> lock(m_routerMutex);

        if (m_pClientManager == nullptr)
        {
            ListenerClientManagerInfo infoStruct = {};
            infoStruct.routerPrefix = kListenerClientManagerPrefix;
            infoStruct.routerPrefixMask = 0;

            IClientManager *pClientManager = new ListenerClientManager(createInfo.allocCb, infoStruct);
            if (m_routerCore.SetClientManager(pClientManager) == Result::Success)
            {
                m_pClientManager = pClientManager;
            }
            else
            {
                delete pClientManager;
            }
        }

        if (m_pClientManager != nullptr)
        {
#ifdef _WIN32
            auto pPipeTransport = std::make_shared<PipeListenerTransport>(kDefaultNamedPipe.hostname);
#else
            auto pPipeTransport = std::make_shared<SocketListenerTransport>(kDefaultNamedPipe.type,
                                                                            kDefaultNamedPipe.hostname,
                                                                            kDefaultNamedPipe.port);
#endif
            if (m_routerCore.RegisterTransport(pPipeTransport) == Result::Success)
            {
                m_managedTransports.emplace_back(pPipeTransport);
            }

            for (uint32 i = 0; i < createInfo.numAddresses; i++)
            {
                // todo: validate this.
                ListenerBindAddress &address = createInfo.pAddressesToBind[i];

                auto pRemoteTransport = std::make_shared<SocketListenerTransport>(TransportType::Remote,
                                                                                  address.hostAddress,
                                                                                  address.port);
                if (m_routerCore.RegisterTransport(pRemoteTransport) == Result::Success)
                {
                    m_managedTransports.emplace_back(pRemoteTransport);
                }
            }
        }

        DD_ASSERT(m_pClientManager != nullptr);

        if ((m_pClientManager != nullptr) && ((m_managedTransports.size() > 0) || (m_pClientManager->GetHostTransport() != nullptr)))
        {

            if (createInfo.flags.enableServer)
            {
                std::shared_ptr<HostListenerTransport> pLoopbackTransport = std::make_shared<HostListenerTransport>(createInfo);
                if (pLoopbackTransport != nullptr && m_routerCore.RegisterTransport(pLoopbackTransport) == Result::Success)
                {
                    ClientId hostClientId = m_pClientManager->GetHostClientId();

                    MessageChannelCreateInfo channelCreateInfo = {};
                    Platform::Strncpy(channelCreateInfo.clientDescription,
                                      createInfo.description,
                                      sizeof(channelCreateInfo.clientDescription));
                    channelCreateInfo.createUpdateThread = true;
                    channelCreateInfo.componentType = Component::Server;
                    m_pMsgChannel = new MessageChannel<HostMsgTransport>(createInfo.allocCb,
                                                                         channelCreateInfo,
                                                                         pLoopbackTransport,
                                                                         hostClientId);
                    if (m_pMsgChannel != nullptr)
                    {
                        m_pServer = new ListenerServer(createInfo.serverCreateInfo, m_pMsgChannel);
                        m_managedTransports.emplace_back(pLoopbackTransport);
                    }
                }
            }

            DD_PRINT(LogLevel::Info, "[ListenerCore] Using %s client manager", m_pClientManager->GetClientManagerName());
            for (const auto &pTransport : m_managedTransports)
            {
                DD_PRINT(LogLevel::Info, "[ListenerCore] Listening for connections on %s", pTransport->GetTransportName());
            }

            RouterStartInfo startInfo = {};
            Platform::Strncpy(startInfo.description, createInfo.description, sizeof(startInfo.description));

            if (m_routerCore.Start(startInfo) == Result::Success)
            {
                if (m_pServer != nullptr)
                {
                    if (m_pServer->Initialize() == Result::Success)
                    {
                        pLogServer = m_pServer->GetLoggingServer();

                        // Bind the listener core object to the listener URI service.
                        m_listenerURIService.BindListenerCore(this);

                        // Register the listener URI service.
                        m_pMsgChannel->RegisterService(&m_listenerURIService);
                    }
                    else
                    {
                        m_pServer->Destroy();
                        delete m_pServer;
                        m_pServer = nullptr;
                    }
                }

                m_createInfo = createInfo;
                result = Result::Success;
                m_started = true;
            }
        }

        DD_ASSERT(m_started);

        if (!m_started)
        {

            if (m_pServer != nullptr)
            {
                pLogServer = nullptr;
                m_pServer->Destroy();
                delete m_pServer;
                m_pServer = nullptr;
            }

            if (m_pMsgChannel != nullptr)
            {
                m_pMsgChannel->Unregister();
                delete m_pMsgChannel;
                m_pMsgChannel = nullptr;
            }

            for (const auto &pTransport : m_managedTransports)
            {
                m_routerCore.RemoveTransport(pTransport);
            }
            m_managedTransports.clear();
            m_routerCore.Stop();

            if (m_pClientManager != nullptr)
            {
                delete m_pClientManager;
                m_pClientManager = nullptr;
            }
        }
        DD_ASSERT(result == Result::Success);
        return result;
    }

    // =====================================================================================================================
    // Destroys the listener core object and shuts down all communications
    void ListenerCore::Destroy()
    {
        std::lock_guard<std::mutex> lock(m_routerMutex);

        if (m_started)
        {
            if (m_pServer != nullptr)
            {
                pLogServer = nullptr;
                m_pServer->Destroy();
                delete m_pServer;
                m_pServer = nullptr;
            }

            if (m_pMsgChannel != nullptr)
            {
                delete m_pMsgChannel;
                m_pMsgChannel = nullptr;
            }

            for (const auto &pTransport : m_managedTransports)
            {
                m_routerCore.RemoveTransport(pTransport);
            }
            m_managedTransports.clear();
            m_routerCore.Stop();
            m_started = false;

            if (m_pClientManager != nullptr)
            {
                delete m_pClientManager;
                m_pClientManager = nullptr;
            }
        }
    }

#if !DD_VERSION_SUPPORTS(GPUOPEN_DISTRIBUTED_STATUS_FLAGS_VERSION)
    // =====================================================================================================================
    // Sets the developer mode enabled client flag to the specified value
    bool ListenerCore::SetDeveloperModeEnabled(bool developerModeEnabled)
    {
        bool result = false;
        std::lock_guard<std::mutex> lock(m_routerMutex);
        if (m_pMsgChannel != nullptr)
        {
            result = (m_pMsgChannel->SetStatusFlag<ClientStatusFlags::DeveloperModeEnabled>(developerModeEnabled) == Result::Success);
        }
        return result;
    }

    // =====================================================================================================================
    // Returns the developer mode enabled client flag value
    bool ListenerCore::GetDeveloperModeEnabled()
    {
        bool result = false;
        std::lock_guard<std::mutex> lock(m_routerMutex);
        if (m_pMsgChannel != nullptr)
        {
            result = m_pMsgChannel->GetStatusFlag<ClientStatusFlags::DeveloperModeEnabled>();
        }
        return result;
    }

    // =====================================================================================================================
    // Sets the halt on connect client flag to the specified value
    bool ListenerCore::SetHaltOnConnect(bool haltOnConnect)
    {
        bool result = false;
        std::lock_guard<std::mutex> lock(m_routerMutex);
        if (m_pMsgChannel != nullptr)
        {
            result = (m_pMsgChannel->SetStatusFlag<ClientStatusFlags::HaltOnConnect>(haltOnConnect) == Result::Success);
        }
        return result;
    }

    // =====================================================================================================================
    // Returns the halt on connect client flag value
    bool ListenerCore::GetHaltOnConnect()
    {
        bool result = false;
        std::lock_guard<std::mutex> lock(m_routerMutex);
        if (m_pMsgChannel != nullptr)
        {
            result = m_pMsgChannel->GetStatusFlag<ClientStatusFlags::HaltOnConnect>();
        }
        return result;
    }
#endif
} // DevDriver
