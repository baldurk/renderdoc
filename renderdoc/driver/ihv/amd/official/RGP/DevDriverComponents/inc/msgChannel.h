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
* @file  msgChannel.h
* @brief Interface declaration for IMsgChannel
***********************************************************************************************************************
*/

#pragma once

#include "gpuopen.h"
#include "protocolClient.h"
#include "protocolServer.h"
#include "msgTransport.h"

namespace DevDriver
{
    class IService;

    namespace TransferProtocol
    {
        class TransferManager;
    }

    DD_STATIC_CONST uint32 kDefaultUpdateTimeoutInMs = 10;
    DD_STATIC_CONST uint32 kFindClientTimeout        = 500;

    // Struct of information required to initialize an IMsgChannel instance
    struct MessageChannelCreateInfo
    {
        StatusFlags initialFlags;                           // Initial client status flags.
        Component   componentType;                          // Type of component the message channel represents.
        bool        createUpdateThread;                     // Create a background processing thread for the message
                                                            // channel. This should only be set to false if the
                                                            // owning object is able to call IMsgChannel::Update()
                                                            // at least once per frame.
        char        clientDescription[kMaxStringLength];    // Description of the client provided to other clients on
                                                            // the message bus.
    };

    class IMsgChannel
    {
    public:
        virtual ~IMsgChannel() {}

        // Register, unregister, or check connected status.
        virtual Result Register(uint32 timeoutInMs = ~(0u)) = 0;
        virtual Result Unregister() = 0;
        virtual bool IsConnected() = 0;

        // Send, receive, and forward messages
        virtual Result Send(ClientId dstClientId,
                            Protocol protocol,
                            MessageCode message,
                            const ClientMetadata& metadata,
                            uint32 payloadSizeInBytes,
                            const void* pPayload) = 0;
        virtual Result Receive(MessageBuffer& message, uint32 timeoutInMs) = 0;
        virtual Result Forward(const MessageBuffer& messageBuffer) = 0;

        // Register, unregister, and retrieve IProtocolServer objects
        virtual Result RegisterProtocolServer(IProtocolServer* pServer) = 0;
        virtual Result UnregisterProtocolServer(IProtocolServer* pServer) = 0;
        virtual IProtocolServer* GetProtocolServer(Protocol protocol) = 0;

        // Initiates a connection using the provided protocol client to the specified destionation client id
        virtual Result ConnectProtocolClient(IProtocolClient* pProtocolClient, ClientId dstClientId) = 0;

        // Register or Unregister an IService object
        virtual Result RegisterService(IService* pService) = 0;
        virtual Result UnregisterService(IService* pService) = 0;

        // Get the allocator used to create this message channel
        virtual const AllocCb& GetAllocCb() const = 0;

        // Returns client information for the first client to respond that matches the specified filter
        virtual Result FindFirstClient(const ClientMetadata& filter,
                                       ClientId*             pClientId,
                                       uint32                timeoutInMs = kFindClientTimeout,
                                       ClientMetadata*       pClientMetadata = nullptr) = 0;

        // Get the client ID, or returns kBroadcastClientId if disconnected.
        virtual ClientId GetClientId() const = 0;

        // Get the client information struct for the message channel.
        virtual const ClientInfoStruct& GetClientInfo() const = 0;

        // Get a human-readable string describing the connection type.
        virtual const char* GetTransportName() const = 0;

        // Set and get all client status flags.
        virtual Result SetStatusFlags(StatusFlags flags) = 0;
        virtual StatusFlags GetStatusFlags() const = 0;

        // Set the specified client status flag.
        template <ClientStatusFlags flag>
        Result SetStatusFlag(bool enable)
        {
            Result toggleResult = Result::Success;
            StatusFlags oldFlags = GetStatusFlags();
            StatusFlags newFlags;

            if (enable)
            {
                // Toggle developer mode
                newFlags = oldFlags | static_cast<DevDriver::StatusFlags>(flag);
            }
            else
            {
                // Toggle developer mode
                newFlags = oldFlags & ~static_cast<DevDriver::StatusFlags>(flag);
            }

            if (newFlags != oldFlags)
            {
                toggleResult = SetStatusFlags(newFlags);
            }
            return toggleResult;
        }

        // Get the specified client status flag.
        template <ClientStatusFlags flag>
        bool GetStatusFlag() const
        {
            return ((GetStatusFlags() & static_cast<StatusFlags>(flag)) != 0);
        }

        // Utility functions that should probably not be publicly exposed.
        // TODO: Refactor surrounding code to eliminate these.
        virtual TransferProtocol::TransferManager& GetTransferManager() = 0;
        virtual void Update(uint32 timeoutInMs = kDefaultUpdateTimeoutInMs) = 0;

        // Backwards compatibility
        Result EstablishSession(ClientId dstClientId, IProtocolClient* pProtocolClient)
        {
            return ConnectProtocolClient(pProtocolClient, dstClientId);
        }
    protected:
        IMsgChannel() {};
    };

} // DevDriver
