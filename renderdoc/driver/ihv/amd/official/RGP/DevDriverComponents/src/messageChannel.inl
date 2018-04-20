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

#include "messageChannel.h"
#include "protocolServer.h"
#include "protocolClient.h"

namespace DevDriver
{
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Thread function that calls the message channel update function until the
    template <class MsgTransport>
    void MessageChannel<MsgTransport>::MsgChannelReceiveFunc(void* pThreadParam)
    {
        MessageChannel* pMessageChannel = reinterpret_cast<MessageChannel*>(pThreadParam);

        while ((pMessageChannel->m_msgThreadParams.active) & (pMessageChannel->m_clientId != kBroadcastClientId))
        {
            pMessageChannel->Update();
        }

        // Check to see if the message thread was terminated normally. If active is still set to true we are destroying
        // this thread due to a connection failure, so we need to close all active sessions and exit.
        if (pMessageChannel->m_msgThreadParams.active)
        {
            const Result status = pMessageChannel->m_sessionManager.Destroy();
            DD_ASSERT(status == Result::Success);
            DD_UNUSED(status);
            pMessageChannel->m_msgThreadParams.active = false;
        }
    }

    template <class MsgTransport>
    template <class ...Args>
    MessageChannel<MsgTransport>::MessageChannel(const AllocCb&                  allocCb,
                                                 const MessageChannelCreateInfo& createInfo,
                                                 Args&&...                       args) :
        m_msgTransport(Platform::Forward<Args>(args)...),
        m_receiveQueue(allocCb),
        m_clientId(kBroadcastClientId),
        m_allocCb(allocCb),
        m_createInfo(createInfo),
        m_clientInfoResponse(),
        m_lastActivityTimeMs(0),
        m_lastKeepaliveTransmitted(0),
        m_lastKeepaliveReceived(0),
        m_msgThread(),
        m_msgThreadParams(),
        m_updateSemaphore(1, 1),
        m_sessionManager(allocCb),
        m_transferManager(allocCb),
        m_pURIServer(nullptr),
        m_clientURIService()
    {
    }

    template <class MsgTransport>
    MessageChannel<MsgTransport>::~MessageChannel()
    {
        Unregister();
    }

    template <class MsgTransport>
    void MessageChannel<MsgTransport>::Update(uint32 timeoutInMs)
    {
        MessageBuffer messageBuffer = {};

        // Attempt to read a message from the queue with a timeout.
        if (m_updateSemaphore.Wait(kInfiniteTimeout) == Result::Success)
        {
            Result status = ReadTransportMessage(messageBuffer, timeoutInMs);
            while (status == Result::Success)
            {
                // Read any remaining messages in the queue without waiting on a timeout until the queue is empty.
                if (!HandleMessageReceived(messageBuffer))
                {
                    Platform::LockGuard<Platform::AtomicLock> lock(m_receiveQueue.lock);
                    if (m_receiveQueue.queue.PushBack(messageBuffer))
                    {
                        m_receiveQueue.semaphore.Signal();
                    }
                }
                status = ReadTransportMessage(messageBuffer, kNoWait);
            }

            if (status != Result::NotReady)
            {
                Disconnect();
            }
            else if (MsgTransport::RequiresClientRegistration() & MsgTransport::RequiresKeepAlive())
            {
                // if keep alive is enabled and the last message read wasn't an error
                uint64 currentTime = Platform::GetCurrentTimeInMs();

                // only check the keep alive threshold if we haven't had any network traffic in kKeepAliveTimeout time
                if ((currentTime - m_lastActivityTimeMs) > kKeepAliveTimeout)
                {
                    // if we have gone <kKeepAliveThreshold> heartbeats without reponse we disconnect
                    if ((m_lastKeepaliveTransmitted - m_lastKeepaliveReceived) < kKeepAliveThreshold)
                    {
                        // send a heartbeat and increment the last keepalive transmitted variable
                        using namespace DevDriver::ClientManagementProtocol;
                        MessageBuffer heartbeat = kOutOfBandMessage;
                        heartbeat.header.messageId = static_cast<MessageCode>(ManagementMessage::KeepAlive);
                        heartbeat.header.sessionId = ++m_lastKeepaliveTransmitted;
                        Forward(heartbeat);

                        // we need to update the last activity time to make sure it doesn't immediately timeout again
                        m_lastActivityTimeMs = currentTime;
                    }
                    else
                    {
                        // we have sent too many heartbeats without response, so disconnect
                        Disconnect();
                    }
                }
            }

            // Give the session manager a chance to update its sessions.
            m_sessionManager.UpdateSessions();

            m_updateSemaphore.Signal();

#if defined(DD_LINUX)
            // we yield the thread after processing messages to let other threads grab the lock if the need to
            // this works around an issue where the message processing thread releases the lock then reacquires
            // it before a sleeping thread that is waiting on it can get it.
            Platform::Sleep(0);
#endif
        }
    }

    template <class MsgTransport>
    Result MessageChannel<MsgTransport>::ConnectProtocolClient(IProtocolClient* pClient, ClientId dstClientId)
    {
        return (pClient != nullptr) ? m_sessionManager.EstablishSessionForClient(*pClient, dstClientId) : Result::Error;
    }

    template <class MsgTransport>
    Result MessageChannel<MsgTransport>::RegisterProtocolServer(IProtocolServer* pServer)
    {
        Result result = Result::Error;
        if (pServer != nullptr)
        {
            ProtocolFlags enabledProtocols = {};
            switch (pServer->GetProtocol())
            {
            case Protocol::Logging:
            {
                enabledProtocols.logging = true;
                break;
            }

            case Protocol::Settings:
            {
                enabledProtocols.settings = true;
                break;
            }

            case Protocol::DriverControl:
            {
                enabledProtocols.driverControl = true;
                break;
            }

            case Protocol::RGP:
            {
                enabledProtocols.rgp = true;
                break;
            }

            case Protocol::ETW:
            {
                enabledProtocols.etw = true;
                break;
            }

            case Protocol::GpuCrashDump:
            {
                enabledProtocols.gpuCrashDump = true;
                break;
            }

            default:
            {
                DD_ALERT_REASON("Registered protocol server for unknown protocol");
                break;
            }
            }

            result = m_sessionManager.RegisterProtocolServer(pServer);
            if (result == Result::Success)
            {
                m_clientInfoResponse.metadata.protocols.value |= enabledProtocols.value;
            }
        }
        return result;
    }

    template <class MsgTransport>
    Result MessageChannel<MsgTransport>::UnregisterProtocolServer(IProtocolServer* pServer)
    {
        // @todo: Remove enabled protocol metadata flags related to pServer

        return m_sessionManager.UnregisterProtocolServer(pServer);
    }

    template <class MsgTransport>
    ClientId MessageChannel<MsgTransport>::GetClientId() const
    {
        return m_clientId;
    }

    template <class MsgTransport>
    inline Result MessageChannel<MsgTransport>::FindFirstClient(const ClientMetadata& filter,
                                                                ClientId*             pClientId,
                                                                uint32                timeoutInMs,
                                                                ClientMetadata*       pClientMetadata)
    {
        using namespace DevDriver::SystemProtocol;
        Result result = Result::Error;

        if (pClientId != nullptr)
        {
            result = Result::NotReady;
            // acquire the update semaphore. this prevents the update thread from processing messages
            // as it is possible it could process messages the client was looking for
            if (m_updateSemaphore.Wait(kInfiniteTimeout) == Result::Success)
            {
                const uint64 startTime = Platform::GetCurrentTimeInMs();
                uint64 timeDelta = 0;

                MessageBuffer messageBuffer = {};

                // we're going to loop through sending a ping, receiving/processing any messages, then updating sessions
                do
                {
                    // send a ping every time the outer loop executes
                    result = SendSystem(kBroadcastClientId,
                                        SystemMessage::Ping,
                                        filter);

                    // there are two expected results from SendSystem: Success or NotReady
                    // if it is successful, we need to transition to waiting
                    if (result == Result::Success)
                    {
                        // read any traffic waiting
                        result = ReadTransportMessage(messageBuffer, kDefaultUpdateTimeoutInMs);

                        // we are going to loop until either an error is encountered or there is no data remaining
                        // the expected behavior is that this loop will exit with result == Result::NotReady
                        while (result == Result::Success)
                        {
                            // if the default message handler doesn't care about this message we inspect it
                            if (!HandleMessageReceived(messageBuffer))
                            {
                                // did we receive any pong messages?
                                if ((messageBuffer.header.protocolId == Protocol::System) &
                                    (static_cast<SystemMessage>(messageBuffer.header.messageId) == SystemMessage::Pong))
                                {
                                    const ClientMetadata metadata =
                                        *reinterpret_cast<const ClientMetadata *>(&messageBuffer.header.sequence);
                                    // check to see if it matches with our initial filter
                                    if (filter.Matches(metadata))
                                    {
                                        // if it does, write out the client ID
                                        *pClientId = messageBuffer.header.srcClientId;

                                        // If the pClientMetadata pointer is valid, fill it with the matching client
                                        // metadata struct data.
                                        if (pClientMetadata != nullptr)
                                        {
                                            pClientMetadata->value = metadata.value;
                                        }

                                        // if we found a matching client we break out of the inner loop.
                                        // the outer loop will exit automatically because result is implied to be Success
                                        // inside the execution of this loop
                                        break;
                                    }
                                }
                                else
                                {
                                    // if this message wasn't one we were looking for, we go ahead and enqueue
                                    // the message in the local receive queue
                                    Platform::LockGuard<Platform::AtomicLock> lock(m_receiveQueue.lock);
                                    if (m_receiveQueue.queue.PushBack(messageBuffer))
                                    {
                                        m_receiveQueue.semaphore.Signal();
                                    }
                                }
                            }

                            // read the next message with no timeout
                            // this ensures that this inner loop will exit immediately when no data is remaining
                            result = ReadTransportMessage(messageBuffer, kNoWait);
                        }

                        // Give the session manager a chance to update its sessions.
                        m_sessionManager.UpdateSessions();
                    }
                    else if (result == Result::NotReady)
                    {
                        // the write failed because the transport was busy, so we sleep and will try again
                        Platform::Sleep(Platform::Min(timeoutInMs, kDefaultUpdateTimeoutInMs));
                    }

                    // calculate how much time has elapsed since we started looping
                    timeDelta = Platform::GetCurrentTimeInMs() - startTime;

                    // we repeat this loop so long as:
                    //  1. result is NotReady, which implies that either the write or last read timed out
                    //  2. The loop haven't exceed the specified timeout
                    // we exit the outer loop if:
                    //  1. The inner loop exited with result equal to Success, indicating we found a client
                    //  2. result indicates an unexpected error
                    //  3. We have timed out, in which case result is already NotReady
                } while ((result == Result::NotReady) &
                         (timeDelta < timeoutInMs));

                // release the update lock
                m_updateSemaphore.Signal();
            }
        }
        return result;
    }

    template <class MsgTransport>
    bool MessageChannel<MsgTransport>::HandleMessageReceived(const MessageBuffer& messageBuffer)
    {
        using namespace DevDriver::SystemProtocol;
        using namespace DevDriver::ClientManagementProtocol;

        bool handled = false;
        bool forThisHost = false;

        // todo: move this out into message reading loop so that it isn't getting done for every message
        if (MsgTransport::RequiresClientRegistration() & MsgTransport::RequiresKeepAlive())
        {
            m_lastActivityTimeMs = Platform::GetCurrentTimeInMs();
        }

        if ((messageBuffer.header.protocolId == Protocol::Session) & (messageBuffer.header.dstClientId == m_clientId))
        {
            m_sessionManager.HandleReceivedSessionMessage(messageBuffer);
            handled = true;
        }
        else if (IsOutOfBandMessage(messageBuffer))
        {
            handled = true; // always filter these out from the client
            if (IsValidOutOfBandMessage(messageBuffer)
                & (static_cast<ManagementMessage>(messageBuffer.header.messageId) == ManagementMessage::KeepAlive))
            {
                DD_PRINT(LogLevel::Debug, "Received keep alive response seq %u", messageBuffer.header.sessionId);
                m_lastKeepaliveReceived = messageBuffer.header.sessionId;
            }
        }
        else
        {
            using namespace DevDriver::SystemProtocol;
            const ClientId &dstClientId = messageBuffer.header.dstClientId;
            const ClientMetadata *pMetadata = reinterpret_cast<const ClientMetadata *>(&messageBuffer.header.sequence);
            forThisHost = !!((((dstClientId == kBroadcastClientId)
                               & (pMetadata->Matches(m_clientInfoResponse.metadata)))
                              | ((m_clientId != kBroadcastClientId) & (dstClientId == m_clientId))));
            if ((forThisHost) & (messageBuffer.header.protocolId == Protocol::System))
            {
                const ClientId &srcClientId = messageBuffer.header.srcClientId;

                const SystemMessage& message = static_cast<SystemMessage>(messageBuffer.header.messageId);

                switch (message)
                {
                case SystemMessage::Ping:
                {
                    SendSystem(srcClientId,
                               SystemMessage::Pong,
                               m_clientInfoResponse.metadata);

                    handled = true;

                    break;
                }
                case SystemMessage::QueryClientInfo:
                {
                    Send(srcClientId,
                         Protocol::System,
                         static_cast<MessageCode>(SystemMessage::ClientInfo),
                         m_clientInfoResponse.metadata,
                         sizeof(m_clientInfoResponse),
                         &m_clientInfoResponse);

                    handled = true;

                    break;
                }
                case SystemMessage::ClientDisconnected:
                {
                    m_sessionManager.HandleClientDisconnection(srcClientId);
                    break;
                }
                default:
                {
                    // Unhandled system message
                    break;
                }
                }
            }
        }
        return (handled | (!forThisHost));
    }

    template <class MsgTransport>
    Result MessageChannel<MsgTransport>::Send(ClientId dstClientId, Protocol protocol, MessageCode message, const ClientMetadata &metadata, uint32 payloadSizeInBytes, const void* pPayload)
    {
        MessageBuffer messageBuffer = {};
        messageBuffer.header.dstClientId = dstClientId;
        messageBuffer.header.srcClientId = m_clientId;
        messageBuffer.header.protocolId = protocol;
        messageBuffer.header.messageId = message;
        messageBuffer.header.payloadSize = payloadSizeInBytes;
        messageBuffer.header.sequence = metadata.value;

        if ((pPayload != nullptr) & (payloadSizeInBytes != 0))
        {
            memcpy(messageBuffer.payload, pPayload, Platform::Min(payloadSizeInBytes, static_cast<uint32>(sizeof(messageBuffer.payload))));
        }
        return Forward(messageBuffer);
    }

    template <class MsgTransport>
    Result MessageChannel<MsgTransport>::SendSystem(ClientId dstClientId, SystemProtocol::SystemMessage message, const ClientMetadata &metadata)
    {
        MessageBuffer messageBuffer = {};
        messageBuffer.header.dstClientId = dstClientId;
        messageBuffer.header.srcClientId = m_clientId;
        messageBuffer.header.protocolId = Protocol::System;
        messageBuffer.header.messageId = static_cast<MessageCode>(message);
        messageBuffer.header.sequence = metadata.value;
        return Forward(messageBuffer);
    }

    template <class MsgTransport>
    Result MessageChannel<MsgTransport>::Forward(const MessageBuffer& messageBuffer)
    {
        Result result = Result::Error;
        if (m_clientId != kBroadcastClientId)
        {
            result = WriteTransportMessage(messageBuffer);
            if ((result != Result::Success) & (result != Result::NotReady))
            {
                Disconnect();
            }
        }
        return result;
    }

    template <class MsgTransport>
    Result MessageChannel<MsgTransport>::Receive(MessageBuffer& message, uint32 timeoutInMs)
    {
        Result result = Result::Unavailable;
        if ((m_receiveQueue.queue.Size() > 0) | (m_clientId != kBroadcastClientId))
        {
            result = m_receiveQueue.semaphore.Wait(timeoutInMs);
            if (result == Result::Success)
            {
                Platform::LockGuard<Platform::AtomicLock> lock(m_receiveQueue.lock);
                m_receiveQueue.queue.PopFront(message);
            }
        }
        return result;
    }

    template <class MsgTransport>
    Result MessageChannel<MsgTransport>::Register(uint32 timeoutInMs)
    {
        Result status = Result::Error;

        if (m_clientId == kBroadcastClientId)
        {
            status = m_msgTransport.Connect(&m_clientId, timeoutInMs);
        }

        if (MsgTransport::RequiresClientRegistration())
        {
            if ((status == Result::Success) & (m_clientId == kBroadcastClientId))
            {
                using namespace DevDriver::ClientManagementProtocol;
                MessageBuffer recvBuffer = {};
                MessageBuffer messageBuffer = kOutOfBandMessage;
                messageBuffer.header.messageId = static_cast<MessageCode>(ManagementMessage::ConnectRequest);
                messageBuffer.header.payloadSize = sizeof(ConnectRequestPayload);

                {
                    ConnectRequestPayload* DD_RESTRICT pConnectionRequest =
                        reinterpret_cast<ConnectRequestPayload*>(&messageBuffer.payload[0]);
                    pConnectionRequest->componentType = m_createInfo.componentType;
                    pConnectionRequest->initialClientFlags = m_createInfo.initialFlags;
                }

                uint64 sendTime = Platform::GetCurrentTimeInMs();
                uint64 currentTime = sendTime;

                Result registerResult;
                do
                {
                    registerResult = WriteTransportMessage(messageBuffer);

                    if (registerResult == Result::Success)
                    {
                        registerResult = ReadTransportMessage(recvBuffer, kRetransmitTimeoutInMs);
                        if (registerResult == Result::Success)
                        {
                            registerResult = Result::NotReady;
                            if (recvBuffer.header.protocolId == Protocol::ClientManagement)
                            {
                                registerResult = Result::VersionMismatch;
                                if (IsOutOfBandMessage(recvBuffer) &
                                    IsValidOutOfBandMessage(recvBuffer) &
                                    (static_cast<ManagementMessage>(recvBuffer.header.messageId) == ManagementMessage::ConnectResponse))
                                {
                                    ConnectResponsePayload* DD_RESTRICT pConnectionResponse =
                                        reinterpret_cast<ConnectResponsePayload*>(&recvBuffer.payload[0]);
                                    registerResult = pConnectionResponse->result;
                                    m_clientId = pConnectionResponse->clientId;
                                }
                            }
                        }
                    }

                    currentTime = Platform::GetCurrentTimeInMs();
                } while ((registerResult == Result::NotReady) & ((currentTime - sendTime) < timeoutInMs));

                status = registerResult;
            }
        }
        if (status == Result::Success)
        {
            memset(&m_clientInfoResponse, 0, sizeof(m_clientInfoResponse));
            Platform::Strncpy(m_clientInfoResponse.clientDescription, m_createInfo.clientDescription, sizeof(m_clientInfoResponse.clientDescription));
            Platform::GetProcessName(m_clientInfoResponse.clientName, sizeof(m_clientInfoResponse.clientName));
            m_clientInfoResponse.processId = Platform::GetProcessId();
            m_clientInfoResponse.metadata.clientType = m_createInfo.componentType;
            m_clientInfoResponse.metadata.status = m_createInfo.initialFlags;

            status = ((m_sessionManager.Init(this) == Result::Success) ? Result::Success : Result::Error);

            // Initialize the transfer manager
            if (status == Result::Success)
            {
                status = ((m_transferManager.Init(this, &m_sessionManager) == Result::Success) ? Result::Success : Result::Error);
            }

            // Initialize the URI server
            if (status == Result::Success)
            {
                m_pURIServer = DD_NEW(URIProtocol::URIServer, m_allocCb)(this);
                status = (m_pURIServer != nullptr) ? Result::Success : Result::Error;
            }

            // Register the URI server
            if (status == Result::Success)
            {
                m_sessionManager.RegisterProtocolServer(m_pURIServer);
            }

            // Set up internal URI services
            if (status == Result::Success)
            {
                m_clientURIService.BindMessageChannel(this);
                m_pURIServer->RegisterService(&m_clientURIService);
            }

            if ((status == Result::Success) & m_createInfo.createUpdateThread)
            {
                status = CreateMsgThread();
            }
        }
        return status;
    }

    template <class MsgTransport>
    Result MessageChannel<MsgTransport>::Unregister()
    {
        if (m_createInfo.createUpdateThread)
        {
            Result status = DestroyMsgThread();
            DD_ASSERT(status == Result::Success);
            DD_UNUSED(status);
        }

        if (m_pURIServer != nullptr)
        {
            m_sessionManager.UnregisterProtocolServer(m_pURIServer);

            DD_DELETE(m_pURIServer, m_allocCb);
            m_pURIServer = nullptr;
        }

        // Destroy the transfer manager
        m_transferManager.Destroy();

        Result status = m_sessionManager.Destroy();
        DD_ASSERT(status == Result::Success);
        DD_UNUSED(status);

        if (MsgTransport::RequiresClientRegistration())
        {
            if (m_clientId != kBroadcastClientId)
            {
                using namespace DevDriver::ClientManagementProtocol;
                MessageBuffer disconnectMsgBuffer = {};
                disconnectMsgBuffer.header.protocolId = Protocol::ClientManagement;
                disconnectMsgBuffer.header.messageId =
                    static_cast<MessageCode>(ManagementMessage::DisconnectNotification);
                disconnectMsgBuffer.header.srcClientId = m_clientId;
                disconnectMsgBuffer.header.dstClientId = kBroadcastClientId;
                disconnectMsgBuffer.header.payloadSize = 0;

                WriteTransportMessage(disconnectMsgBuffer);
            }
        }
        return Disconnect();
    }

    template <class MsgTransport>
    inline bool MessageChannel<MsgTransport>::IsConnected()
    {
        return (m_clientId != kBroadcastClientId);
    }

    template <class MsgTransport>
    Result MessageChannel<MsgTransport>::SetStatusFlags(StatusFlags flags)
    {
        Result status = Result::Error;
        if (m_clientId != kBroadcastClientId)
        {
#if DD_VERSION_SUPPORTS(GPUOPEN_DISTRIBUTED_STATUS_FLAGS_VERSION)
            m_clientInfoResponse.metadata.status = flags;
            status = Result::Success;
#else
            status = m_msgTransport.UpdateClientStatus(m_clientId, flags);

            if (status == Result::Unavailable)
            {
                status = m_updateSemaphore.Wait(kInfiniteTimeout);
                if (status == Result::Success)
                {
                    using namespace DevDriver::ClientManagementProtocol;
                    // @todo: Implement support for updating client status in SocketMsgTransport
                    MessageBuffer updateMsgBuffer = {};
                    MessageBuffer recvBuffer = {};

                    updateMsgBuffer.header.protocolId = Protocol::ClientManagement;
                    updateMsgBuffer.header.messageId = static_cast<MessageCode>(ManagementMessage::SetClientFlags);
                    updateMsgBuffer.header.srcClientId = m_clientId;
                    updateMsgBuffer.header.dstClientId = kBroadcastClientId;
                    updateMsgBuffer.header.payloadSize = sizeof(SetClientFlagsPayload);

                    {
                        SetClientFlagsPayload* DD_RESTRICT pRequest = reinterpret_cast<SetClientFlagsPayload*>(updateMsgBuffer.payload);
                        pRequest->flags = flags;
                    }

                    do
                    {
                        status = Forward(updateMsgBuffer);
                        if (status == Result::Success)
                        {
                            status = ReadTransportMessage(recvBuffer, kDefaultUpdateTimeoutInMs);
                            while (status == Result::Success)
                            {
                                if (!HandleMessageReceived(recvBuffer))
                                {
                                    if ((recvBuffer.header.protocolId == Protocol::ClientManagement) &
                                        (static_cast<ManagementMessage>(recvBuffer.header.messageId) == ManagementMessage::SetClientFlagsResponse))
                                    {
                                        SetClientFlagsResponsePayload* DD_RESTRICT pResponse =
                                            reinterpret_cast<SetClientFlagsResponsePayload*>(recvBuffer.payload);
                                        DD_ASSERT(pResponse->result != Result::NotReady);
                                        status = pResponse->result;
                                        break; // break out of inner while loop
                                    }
                                    else
                                    {
                                        Platform::LockGuard<Platform::AtomicLock> lock(m_receiveQueue.lock);
                                        if (m_receiveQueue.queue.PushBack(recvBuffer))
                                        {
                                            m_receiveQueue.semaphore.Signal();
                                        }
                                    }
                                    status = ReadTransportMessage(recvBuffer, 0);
                                }

                                // Give the session manager a chance to update its sessions.
                                m_sessionManager.UpdateSessions();
                            }
                        }
                    } while (status == Result::NotReady);
                    m_updateSemaphore.Signal();
                }
            }
            if (status == Result::Success)
            {
                m_clientInfoResponse.metadata.status = flags;
            }
#endif
        }
        return status;
    }

    template <class MsgTransport>
    StatusFlags MessageChannel<MsgTransport>::GetStatusFlags() const
    {
        return m_clientInfoResponse.metadata.status;
    }

    template <class MsgTransport>
    Result MessageChannel<MsgTransport>::CreateMsgThread()
    {
        memset(&m_msgThreadParams, 0, sizeof(m_msgThreadParams));

        m_msgThreadParams.active = true;

        Result result = m_msgThread.Start(MsgChannelReceiveFunc, this);

        if (result != Result::Success)
        {
            m_msgThreadParams.active = false;
            DD_ALERT_REASON("Thread creation failed");
        }

        return DD_SANITIZE_RESULT(result);
    }

    template <class MsgTransport>
    Result MessageChannel<MsgTransport>::DestroyMsgThread()
    {
        Result result = Result::Success;
        if (m_msgThread.IsJoinable())
        {
            m_msgThreadParams.active = false;
            result = m_msgThread.Join();
        }
        return DD_SANITIZE_RESULT(result);
    }

    template <class MsgTransport>
    inline Result MessageChannel<MsgTransport>::Disconnect()
    {
        Result result = Result::Success;
        if (m_clientId != kBroadcastClientId)
        {
            m_clientId = kBroadcastClientId;
            m_msgTransport.Disconnect();
            result = Result::Success;
        }
        return result;
    }

    template <class MsgTransport>
    IProtocolServer *MessageChannel<MsgTransport>::GetProtocolServer(Protocol protocol)
    {
        return m_sessionManager.GetProtocolServer(protocol);
    }

} // DevDriver
