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
* @file  devDriverServer.h
* @brief Class declaration for DevDriverServer.
***********************************************************************************************************************
*/

#pragma once

#include "gpuopen.h"
#include "msgChannel.h"
#include "msgTransport.h"
#include "protocols/systemProtocols.h"
#include "protocols/typemap.h"

namespace DevDriver
{
    class IProtocolServer;

    // Server Creation Info
    // This struct extends the MessageChannelCreateInfo struct and adds information about the destination host
    // the client will connect to. It additionally allows specifying protocol servers to enable during initialization.
    // See msgChannel.h for a full list of members.
    struct ServerCreateInfo : public MessageChannelCreateInfo
    {
        HostInfo                 connectionInfo;    // Connection information describing how the Server should connect
                                                    // to the message bus.
        ProtocolFlags            servers;           // Set of boolean values indicating which servers should be created
                                                    // during initialization.
    };

#if !DD_VERSION_SUPPORTS(GPUOPEN_CREATE_INFO_CLEANUP_VERSION)
    struct DevDriverServerCreateInfo
    {
        struct TransportCreateInfo : public MessageChannelCreateInfo
        {
            AllocCb       allocCb;
            HostInfo      hostInfo;
            TransportType type;
        } transportCreateInfo;
        ProtocolFlags     enabledProtocols;
    };
#endif

#if !DD_VERSION_SUPPORTS(GPUOPEN_DISTRIBUTED_STATUS_FLAGS_VERSION)
    Result QueryDevDriverStatus(const TransportType type, StatusFlags* pFlags, HostInfo *pHostInfo = nullptr);
#endif

    DD_STATIC_CONST uint32 kQueryStatusTimeoutInMs = 50;

    class DevDriverServer
    {
    public:
        static bool IsConnectionAvailable(const HostInfo& hostInfo, uint32 timeout = kQueryStatusTimeoutInMs);

        explicit DevDriverServer(const AllocCb& allocCb, const ServerCreateInfo& createInfo);
        ~DevDriverServer();

        Result Initialize();
        void Finalize();
        void Destroy();

        bool IsConnected() const;
        IMsgChannel* GetMessageChannel() const;

        LoggingProtocol::LoggingServer* GetLoggingServer();
        SettingsProtocol::SettingsServer* GetSettingsServer();
        DriverControlProtocol::DriverControlServer* GetDriverControlServer();
        RGPProtocol::RGPServer* GetRGPServer();

#if !DD_VERSION_SUPPORTS(GPUOPEN_CREATE_INFO_CLEANUP_VERSION)
        static bool IsConnectionAvailable(const TransportType type, uint32 timeout = kQueryStatusTimeoutInMs);
        explicit DevDriverServer(const DevDriverServerCreateInfo& createInfo);
#endif
    private:
        Result InitializeProtocols();
        void DestroyProtocols();

        Result RegisterProtocol(Protocol protocol);
        void UnregisterProtocol(Protocol protocol);
        void FinalizeProtocol(Protocol protocol);

        IMsgChannel*     m_pMsgChannel;
        AllocCb          m_allocCb;
        ServerCreateInfo m_createInfo;

        template <Protocol protocol, class ...Args>
        inline Result RegisterProtocol(Args... args);

        template <Protocol protocol>
        inline ProtocolServerType<protocol>* GetServer();
    };

} // DevDriver
