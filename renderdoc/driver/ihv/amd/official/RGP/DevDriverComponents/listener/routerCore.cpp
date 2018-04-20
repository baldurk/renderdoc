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
* @brief Class definition for RouterCore
***********************************************************************************************************************
*/

#include "routerCore.h"
#include "../inc/ddPlatform.h"
#include <cstring>
#include "protocols/systemProtocols.h"

namespace DevDriver
{
    DD_STATIC_CONST ClientInfo kUnknownClient =
    {
        "Unknown",
        "Unknown",
        0,
        0,
        false
    };

    DD_STATIC_CONST ClientContext kNewClientContext = {
        kUnknownClient,
        {},
        0,
        false,
        false
    };

    //@note: The clients mutex must always be owned during this function.
    ClientContext* RouterCore::FindClientById(ClientId clientId)
    {
        ClientContext* pLocalClientInfo = nullptr;
        auto search = m_clientMap.find(clientId);
        if (search != m_clientMap.end())
        {
            pLocalClientInfo = &search->second;
        }
        return pLocalClientInfo;
    }

    //@note: The clients mutex must always be owned during this function.
    ClientContext* RouterCore::FindExternalClientByConnection(const ConnectionInfo &connectionInfo)
    {
        const auto &find = m_transportMap.find(connectionInfo.handle);
        if (find != m_transportMap.end())
        {
            if (find->second.pTransport != nullptr && !find->second.pTransport->ForwardingConnection())
            {
                for (auto& pair : find->second.clientMap)
                {
                    if (pair.second.size == connectionInfo.size &&
                        (memcmp(pair.second.data, connectionInfo.data, connectionInfo.size) == 0))
                    {
                        auto findClient = m_clientMap.find(pair.first);
                        if (findClient != m_clientMap.end())
                        {
                            return &findClient->second;
                        }
                        return nullptr;
                    }
                }
            }
        }
        return nullptr;
    }

    //@note: The client and transport mutex must always be owned during this function.
    void RouterCore::AddClient(const ClientId clientId, const ConnectionInfo &connectionInfo, const bool registeredClient)
    {
        DD_ASSERT(clientId != kBroadcastClientId);

        if (clientId != kBroadcastClientId)
        {
            const auto &find = m_transportMap.find(connectionInfo.handle);
            if (find != m_transportMap.end() && find->second.pTransport != nullptr)
            {
                auto &transport = find->second;
                ClientContext clientData(kNewClientContext);
                clientData.clientInfo.clientId = clientId;
                clientData.pingRetryCount = 0;
                clientData.receivedPong = true;
                clientData.connectionInfo = connectionInfo;
                clientData.registeredClient = registeredClient;

                m_clientMap.emplace(clientId, clientData);

                transport.clientMap.emplace(clientId, connectionInfo);

                DD_PRINT(LogLevel::Info, "[RouterCore] Client %u connected via %s", clientId, transport.pTransport->GetTransportName());

            }
        }
    }

    //@note: The client and transport mutex must always be owned during this function.
    void RouterCore::RemoveClient(ClientId clientId)
    {
        const auto &find = m_clientMap.find(clientId);
        if (find != m_clientMap.end())
        {
            TransportHandle tHandle = find->second.connectionInfo.handle;
            const auto &findTransport = m_transportMap.find(tHandle);
            if (findTransport != m_transportMap.end() && findTransport->second.pTransport != nullptr)
            {
                auto &transport = findTransport->second;
                if (transport.pTransport != nullptr)
                {
                    transport.clientMap.erase(clientId);
                    DD_PRINT(LogLevel::Info, "[RouterCore] Client %u disconnected from %s", clientId, transport.pTransport->GetTransportName());
                }
            }

            if (find->second.registeredClient)
            {
                m_pClientManager->UnregisterClient(find->first);

                MessageBuffer messageBuffer = {};
                messageBuffer.header.dstClientId = kBroadcastClientId;
                messageBuffer.header.srcClientId = clientId;
                messageBuffer.header.protocolId = Protocol::System;
                messageBuffer.header.messageId = static_cast<MessageCode>(SystemProtocol::SystemMessage::ClientDisconnected);
                messageBuffer.header.payloadSize = 0;
                SendBroadcastMessage(messageBuffer, nullptr);
            }
            m_clientMap.erase(find);
        }
    }

    void RouterCore::SendBroadcastMessage(const MessageBuffer &message, const std::shared_ptr<IListenerTransport> &pSourceTransport)
    {
        const ClientId &srcClientId = message.header.srcClientId;

        ClientId lastFailedClient = kBroadcastClientId;

        for (auto& pair : m_transportMap)
        {
            TransportContext &context = pair.second;
            if (context.pTransport != nullptr)
            {
                std::shared_ptr<IListenerTransport> &pTransport = context.pTransport;
                if (pTransport->ForwardingConnection())
                {
                    if (pTransport != pSourceTransport)
                    {
                        pTransport->TransmitBroadcastMessage(message);
                    }
                }
                else if (context.clientMap.size() > 0)
                {
                    for (const auto &clientPair : context.clientMap)
                    {
                        if (clientPair.first != srcClientId &&
                            pTransport->TransmitMessage(clientPair.second, message) == Result::Error)
                        {
                            lastFailedClient = clientPair.first;
                        }
                    }
                }
            }
        }

        // this is technically recursive - every time a write fails, it will call SendBroadcastMessage to announce the client disconnect
        // which will then traverse the list of clients and trigger additional failures.
        if (lastFailedClient != kBroadcastClientId)
        {
            RemoveClient(lastFailedClient);
        }

    }

    void RouterCore::ProcessRouterMessage(const MessageContext &messageContext)
    {
        const auto &messageHeader = messageContext.message.header;
        const auto &message = messageContext.message;

        const auto &srcClientId = messageHeader.srcClientId;

        const auto &pTransport = TransportForTransportHandle(messageContext.connectionInfo.handle);

        if (pTransport != nullptr)
        {
        // Process system messages
            if (messageHeader.protocolId == Protocol::System)
            {
                using namespace DevDriver::SystemProtocol;

                DD_ASSERT((messageHeader.dstClientId == m_clientId) || (messageHeader.dstClientId == kBroadcastClientId));

                bool queryClientInfo = false;

                std::unique_lock<std::mutex> lock(m_clientMutex);

                ClientContext* pSrcClientInfo = FindClientById(srcClientId);

                if ((pSrcClientInfo == nullptr) &
                    (static_cast<SystemMessage>(messageHeader.messageId) != SystemMessage::ClientDisconnected))
                {
                    AddClient(srcClientId, messageContext.connectionInfo, false);

                    queryClientInfo = true;
                }

                switch (static_cast<SystemMessage>(messageHeader.messageId))
                {
                case SystemMessage::ClientConnected:
                {
                    if (pSrcClientInfo != nullptr && !pSrcClientInfo->clientInfo.hasBeenIdentified)
                    {
                        queryClientInfo = true;
                    }
                    break;
                }
                case SystemMessage::ClientDisconnected:
                {
                    if (pSrcClientInfo != nullptr)
                    {
                        RemoveClient(srcClientId);
                    }
                    break;
                }
                case SystemMessage::Pong:
                {
                    if (pSrcClientInfo != nullptr)
                    {
                        // Update the last response time if we already know about this client.
                        pSrcClientInfo->receivedPong = true;
                        if (!pSrcClientInfo->clientInfo.hasBeenIdentified)
                        {
                            queryClientInfo = true;
                        }
                    }
                    break;
                }
                case SystemMessage::ClientInfo:
                {

                    // this should literally be impossible.
                    DD_ASSERT(pSrcClientInfo != nullptr);

                    {
                        const ClientInfoStruct* DD_RESTRICT pPayload = reinterpret_cast<const ClientInfoStruct*>(&message.payload[0]);
                        Platform::Strncpy(pSrcClientInfo->clientInfo.clientName, pPayload->clientName, sizeof(pSrcClientInfo->clientInfo.clientName));
                        Platform::Strncpy(pSrcClientInfo->clientInfo.clientDescription, pPayload->clientDescription, sizeof(pSrcClientInfo->clientInfo.clientDescription));
                        pSrcClientInfo->clientInfo.clientPid = pPayload->processId;
                    }

                    pSrcClientInfo->pingRetryCount = 0;
                    pSrcClientInfo->receivedPong = true;

                    pSrcClientInfo->clientInfo.hasBeenIdentified = true;
                    queryClientInfo = false;
                    break;
                }
                default:
                    break;
                }

                lock.unlock();

                if (queryClientInfo)
                {
                    MessageBuffer messageBuffer = {};
                    messageBuffer.header.messageId = static_cast<MessageCode>(SystemMessage::QueryClientInfo);
                    messageBuffer.header.payloadSize = 0;

                    messageBuffer.header.srcClientId = m_clientId;
                    messageBuffer.header.dstClientId = srcClientId;
                    messageBuffer.header.protocolId = Protocol::System;
                    if (pTransport->TransmitMessage(messageContext.connectionInfo, messageBuffer) == Result::Error)
                    {
                        RemoveClient(srcClientId);
                    }
                }
            }
        }
    }

    /////////////////////////////
    // Periodically sends client discovery packets into the network to keep the local client list up to date.
    void RouterCore::UpdateClients()
    {
        // Check if the update interval has been reached.
        uint64 currentTimeInMs = Platform::GetCurrentTimeInMs();
        if ((currentTimeInMs - kClientDiscoveryIntervalInMs) >= m_lastClientPingTimeInMs)
        {
            std::lock_guard<std::mutex> clientLock(m_clientMutex);

            for (auto it = m_clientMap.begin(); it != m_clientMap.end(); )
            {
                const ClientId &clientId = it->first;
                const TransportHandle &tHandle = it->second.connectionInfo.handle;
                if (it->second.receivedPong)
                {
                    it->second.pingRetryCount = 0;
                    it->second.receivedPong = false;
                }
                else
                {
                    it->second.pingRetryCount += 1;
                }

                // Remove the local client from our list if they've timed out.
                if (it->second.pingRetryCount > kClientTimeoutCount)
                {
                    std::lock_guard<std::mutex> transportLock(m_transportMutex);
                    const auto &find = m_transportMap.find(tHandle);
                    if (find != m_transportMap.end())
                    {
                        auto &transport = find->second;
                        if (transport.pTransport != nullptr)
                        {
                            if (transport.clientMap.erase(clientId) > 0)
                            {
                                DD_PRINT(LogLevel::Info,
                                         "[RouterCore] Client %u timed out from %s",
                                         clientId,
                                         transport.pTransport->GetTransportName());
                            }
                        }
                    }
                    if (it->second.registeredClient)
                    {
                        m_pClientManager->UnregisterClient(clientId);
                        MessageBuffer messageBuffer = {};
                        messageBuffer.header.dstClientId = kBroadcastClientId;
                        messageBuffer.header.srcClientId = clientId;
                        messageBuffer.header.protocolId = Protocol::System;
                        messageBuffer.header.messageId = static_cast<MessageCode>(SystemProtocol::SystemMessage::ClientDisconnected);
                        messageBuffer.header.payloadSize = 0;
                        SendBroadcastMessage(messageBuffer, nullptr);
                    }
                    it = m_clientMap.erase(it);
                }
                else
                {
                    ++it;
                }
            }

            m_lastClientPingTimeInMs = currentTimeInMs;

            // Broadcast a client discovery request into the local network.
            MessageBuffer messageBuffer = {};

            messageBuffer.header.srcClientId = m_clientId;
            messageBuffer.header.dstClientId = kBroadcastClientId;
            messageBuffer.header.protocolId = Protocol::System;
            messageBuffer.header.messageId = static_cast<MessageCode>(SystemProtocol::SystemMessage::Ping);
            messageBuffer.header.sessionId = kInvalidSessionId;
            messageBuffer.header.sequence = 0;
            messageBuffer.header.payloadSize = 0;

            std::lock_guard<std::mutex> transportLock(m_transportMutex);
            SendBroadcastMessage(messageBuffer, nullptr);
            // Update the last client discovery time.
        }
    }

    void RouterCore::ProcessClientManagementMessage(const MessageContext &messageContext)
    {
        using namespace DevDriver::ClientManagementProtocol;
        const auto &message = messageContext.message;
        const auto &messageHeader = message.header;

        std::lock_guard<std::mutex> clientLock(m_clientMutex);
        std::lock_guard<std::mutex> transportLock(m_transportMutex);

        const auto &find = m_transportMap.find(messageContext.connectionInfo.handle);
        if (find != m_transportMap.end() && find->second.pTransport != nullptr)
        {
            const auto &pTransport = find->second.pTransport;

            // if both the source and destination are broadcast, we treat these as special cases for client registration
            // detect messages that are the wrong versions
            if (IsOutOfBandMessage(message) & !IsValidOutOfBandMessage(message))
            {
                MessageBuffer messageBuffer = kOutOfBandMessage;
                // this assumes that these are valid for the protocol
                messageBuffer.header.messageId = static_cast<MessageCode>(ManagementMessage::ConnectResponse);
                messageBuffer.header.payloadSize = sizeof(ConnectResponsePayload);
                // we want to overwrite the protocol id to the original protocol id, that way the client can properly recognize it
                messageBuffer.header.protocolId = messageHeader.protocolId;

                {
                    ConnectResponsePayload* DD_RESTRICT pPayload = reinterpret_cast<ConnectResponsePayload*>(&messageBuffer.payload[0]);
                    pPayload->clientId = kBroadcastClientId;
                    pPayload->result = Result::VersionMismatch;
                }

                // Send the response and return the result.
                do {} while (pTransport->TransmitMessage(messageContext.connectionInfo, messageBuffer) == Result::NotReady);

                return;
            } else if (IsOutOfBandMessage(message) & IsValidOutOfBandMessage(message) &
                (static_cast<ManagementMessage>(messageHeader.messageId) == ManagementMessage::KeepAlive))
            {
                // keepalive packet, discard
                DD_PRINT(LogLevel::Debug, "Received keep alive packet seq %u", messageHeader.sessionId);
                MessageBuffer messageBuffer = kOutOfBandMessage;
                // this assumes that these are valid for the protocol
                messageBuffer.header.messageId = static_cast<MessageCode>(ManagementMessage::KeepAlive);
                messageBuffer.header.payloadSize = 0;
                messageBuffer.header.sessionId = messageHeader.sessionId;
                do {} while (pTransport->TransmitMessage(messageContext.connectionInfo, messageBuffer) == Result::NotReady);
                return;
            }

            const auto &srcClientId = messageHeader.srcClientId;

            switch (static_cast<ManagementMessage>(messageHeader.messageId))
            {
            case ManagementMessage::ConnectRequest:
            {
                Result result = Result::VersionMismatch;
                ClientId externalClientId = kBroadcastClientId;

                if (messageHeader.payloadSize == sizeof(ConnectRequestPayload))
                {
                    result = Result::Error;
                    ClientContext* pExternalClientInfo = FindExternalClientByConnection(messageContext.connectionInfo);

                    // Accept the client if it's a new client.
                    if (pExternalClientInfo == nullptr)
                    {
#if DD_VERSION_SUPPORTS(GPUOPEN_DEPRECATE_LEGACY_KMD_VERSION)
                        result = m_pClientManager->RegisterClient(&externalClientId);
#else
                        {
                            const ConnectRequestPayload *DD_RESTRICT pPayload = reinterpret_cast<const ConnectRequestPayload*>(&message.payload[0]);
                            result = m_pClientManager->RegisterClient(pPayload->componentType, pPayload->initialClientFlags, &externalClientId);
                        }
#endif
                        if (result == Result::Success)
                        {
                            DD_ASSERT(externalClientId != kBroadcastClientId);
                            AddClient(externalClientId, messageContext.connectionInfo, true);
                            MessageBuffer messageBuffer = {};
                            messageBuffer.header.dstClientId = kBroadcastClientId;
                            messageBuffer.header.srcClientId = externalClientId;
                            messageBuffer.header.protocolId = Protocol::System;
                            messageBuffer.header.messageId = static_cast<MessageCode>(SystemProtocol::SystemMessage::ClientConnected);
                            messageBuffer.header.payloadSize = 0;
                            SendBroadcastMessage(messageBuffer, nullptr);
                        }
                    }
                    else
                    {
                        result = Result::Success;
                        externalClientId = pExternalClientInfo->clientInfo.clientId;
                        DD_ASSERT(externalClientId != kBroadcastClientId);
                    }

                }

                MessageBuffer messageBuffer = kOutOfBandMessage;
                // this assumes that these are valid for the protocol
                messageBuffer.header.messageId = static_cast<MessageCode>(ManagementMessage::ConnectResponse);
                messageBuffer.header.payloadSize = sizeof(ConnectResponsePayload);
                // we want to overwrite the protocol id to the original protocol id, that way the client can properly recognize it
                messageBuffer.header.protocolId = messageHeader.protocolId;

                {
                    ConnectResponsePayload *DD_RESTRICT pPayload = reinterpret_cast<ConnectResponsePayload*>(&messageBuffer.payload[0]);
                    pPayload->clientId = externalClientId;
                    pPayload->result = result;
                }

                // Send the response and return the result.
                if (pTransport->TransmitMessage(messageContext.connectionInfo, messageBuffer) == Result::Error)
                {
                    if (externalClientId != kBroadcastClientId)
                        RemoveClient(externalClientId);
                }
            }
            break;
            case ManagementMessage::DisconnectNotification:
            {
                // look up client context to see if the client transmitting the message is managed by the listener
                ClientContext* pExternalClientInfo = FindExternalClientByConnection(messageContext.connectionInfo);

                // make sure the client transmitting is the same as the client we are expecting
                if (pExternalClientInfo != NULL && srcClientId == pExternalClientInfo->clientInfo.clientId)
                {
                    MessageBuffer messageBuffer = {};
                    messageBuffer.header.srcClientId = m_clientId;
                    messageBuffer.header.dstClientId = srcClientId;
                    messageBuffer.header.protocolId = messageHeader.protocolId;
                    messageBuffer.header.messageId = static_cast<MessageCode>(ManagementMessage::DisconnectResponse);
                    messageBuffer.header.payloadSize = 0;

                    // notify the client the connection is terminated
                    pTransport->TransmitMessage(messageContext.connectionInfo, messageBuffer);

                    // remove the client since it was valid
                    RemoveClient(srcClientId);
                }
            }
            break;
            case ManagementMessage::SetClientFlags:
            {
                // look up client context to see if the client transmitting the message is managed by the listener
                ClientContext* pExternalClientInfo = FindExternalClientByConnection(messageContext.connectionInfo);

                // make sure the client transmitting is the same as the client we are expecting
                if (pExternalClientInfo != NULL && srcClientId == pExternalClientInfo->clientInfo.clientId)
                {
                    // set up the response message buffer
                    MessageBuffer messageBuffer = {};
                    messageBuffer.header.srcClientId = m_clientId;
                    messageBuffer.header.dstClientId = srcClientId;
                    messageBuffer.header.protocolId = messageHeader.protocolId;
                    messageBuffer.header.messageId = static_cast<MessageCode>(ManagementMessage::SetClientFlagsResponse);
                    messageBuffer.header.payloadSize = sizeof(SetClientFlagsResponsePayload);
                    {

                        SetClientFlagsResponsePayload* DD_RESTRICT pResponse = reinterpret_cast<SetClientFlagsResponsePayload*>(&messageBuffer.payload[0]);
#if DD_VERSION_SUPPORTS(GPUOPEN_DISTRIBUTED_STATUS_FLAGS_VERSION)
                        // for backwards compatibility we inform the client that the request failed
                        pResponse->result = Result::Error;
#else
                        const SetClientFlagsPayload* DD_RESTRICT pPayload = reinterpret_cast<const SetClientFlagsPayload*>(&message.payload[0]);
                        // call UpdateClientStatus and save result into reponse buffer
                        pResponse->result = m_pClientManager->UpdateClientStatus(srcClientId, pPayload->flags);
#endif
                    }

                    // Send the response and return the result.
                    Result transmitResult;
                    do
                    {
                        transmitResult = pTransport->TransmitMessage(messageContext.connectionInfo, messageBuffer);
                    } while (transmitResult == Result::NotReady);

                    // if we failed to transmit to the client, we need to remove it
                    if (transmitResult == Result::Error)
                    {
                        RemoveClient(srcClientId);
                    }
                }
            }
            break;
            case ManagementMessage::QueryStatus:
            {
                MessageBuffer messageBuffer = kOutOfBandMessage;
                // this assumes that these are valid for the protocol
                messageBuffer.header.messageId = static_cast<MessageCode>(ManagementMessage::QueryStatusResponse);
                messageBuffer.header.payloadSize = sizeof(QueryStatusResponsePayload);
                // we want to overwrite the protocol id to the original protocol id, that way the client can properly recognize it
                messageBuffer.header.protocolId = messageHeader.protocolId;

                {
                    QueryStatusResponsePayload* DD_RESTRICT pResponse = reinterpret_cast<QueryStatusResponsePayload*>(&messageBuffer.payload[0]);
#if DD_VERSION_SUPPORTS(GPUOPEN_DISTRIBUTED_STATUS_FLAGS_VERSION)
                    // for backwards compatibility we inform clients that developer mode is enabled + return success
                    pResponse->flags = static_cast<StatusFlags>(ClientStatusFlags::DeveloperModeEnabled);
                    pResponse->result = Result::Success;
#else
                    pResponse->result = m_pClientManager->QueryStatus(&pResponse->flags);
#endif
                }
                Result transmitResult;

                do
                {
                    transmitResult = pTransport->TransmitMessage(messageContext.connectionInfo, messageBuffer);
                } while (transmitResult == Result::NotReady);
                if (transmitResult == Result::Error)
                {
                    RemoveClient(srcClientId);
                }
            }
            break;
            default:
                break;
            }
        }
        //clientLock.unlock();}
    }

    bool RouterCore::IsRoutableMessage(const MessageContext &recvMsgContext)
    {

        const ClientId &dstClientId = recvMsgContext.message.header.dstClientId;
        const ClientId &srcClientId = recvMsgContext.message.header.srcClientId;

        bool isClientManagement = ((recvMsgContext.message.header.protocolId == Protocol::ClientManagement)
            | ClientManagementProtocol::IsOutOfBandMessage(recvMsgContext.message));

        if (isClientManagement)
        {
            RouteInternalMessage(recvMsgContext);
        }
        else if (srcClientId != kBroadcastClientId)
        {
            if ((recvMsgContext.message.header.protocolId == Protocol::System) &
                ((dstClientId == kBroadcastClientId) | (dstClientId == m_clientId)))
            {
                // enqueue for the client management thread
                RouteInternalMessage(recvMsgContext);
            }
            return true;
        }
        return false;
    }

    bool RouterCore::ConnectionInfoForClientId(ClientId clientId, ConnectionInfo *pConnectionInfo)
    {
        if (clientId == kBroadcastClientId)
            return false;
        std::lock_guard<std::mutex> clientLock(m_clientMutex);
        ClientContext* pClientInfo = FindClientById(clientId);
        if (pClientInfo != nullptr)
        {
            *pConnectionInfo = pClientInfo->connectionInfo;
            return true;
        }
        return false;
    }

    std::shared_ptr<IListenerTransport> RouterCore::TransportForTransportHandle(TransportHandle handle)
    {
        if (handle != 0)
        {
            std::lock_guard<std::mutex> transportLock(m_transportMutex);
            const auto &find = m_transportMap.find(handle);
            if (find != m_transportMap.end())
            {
                return find->second.pTransport;
            }
        }
        return std::shared_ptr<IListenerTransport>();
    }

    void RouterCore::RouteBroadcastMessage(const MessageContext& msgContext)
    {
        std::lock_guard<std::mutex> clientLock(m_clientMutex);
        std::lock_guard<std::mutex> transportLock(m_transportMutex);

        std::shared_ptr<IListenerTransport> pSourceTransport;
        const auto &find = m_transportMap.find(msgContext.connectionInfo.handle);
        if (find != m_transportMap.end())
        {
            pSourceTransport = find->second.pTransport;
        }
        SendBroadcastMessage(msgContext.message, pSourceTransport);
    }

    void RouterCore::RouteInternalMessage(const MessageContext & recvMsgContext)
    {
        // Process the broadcast message locally before rebroadcasting it.
        std::lock_guard<std::mutex> lock(m_clientThread.mutex);
        m_clientThread.queue.emplace_back(recvMsgContext);
        m_clientThread.signal.notify_one();
    }

    void RouterCore::RouterThreadFunc(ProcessingQueue &queueState)
    {
        std::deque<MessageContext> messageBuffer;
        const auto waitTime = std::chrono::milliseconds((int64)kThreadWaitTimeoutInMs);

        while (queueState.active)
        {
            bool swapped = false;

            std::unique_lock<std::mutex> lock(queueState.mutex);
            if (queueState.signal.wait_for(lock,waitTime, [&] { return !queueState.queue.empty(); }))
            {
                messageBuffer.swap(queueState.queue);
                swapped = true;
            }

            lock.unlock();

            if (swapped)
            {
                for (const auto& messageContext : messageBuffer)
                {
                    bool isClientManagement = ((messageContext.message.header.protocolId == Protocol::ClientManagement)
                        | ClientManagementProtocol::IsOutOfBandMessage(messageContext.message));

                    if (isClientManagement)
                    {
                        ProcessClientManagementMessage(messageContext);
                    }
                    else
                    {
                        ProcessRouterMessage(messageContext);
                    }
                }
                messageBuffer.clear();
            }
            UpdateClients();
        }
    }

    std::vector<ClientInfo> RouterCore::GetConnectedClientList()
    {
        std::vector<ClientInfo> result = std::vector<ClientInfo>();
        std::lock_guard<std::mutex> lock(m_clientMutex);

        result.reserve(m_clientMap.size());
        for (const auto& pair : m_clientMap)
        {
            if (pair.first != m_clientId)
            {
                result.emplace_back(pair.second.clientInfo);
            }
        }

        //clientLock.unlock();
        return result;
    }

    RouterCore::RouterCore() :
        m_pClientManager(nullptr),
        m_lastTransportId(0),
        m_lastClientPingTimeInMs(0),
        m_clientThread(),
        m_clientInfoResponse()
    {

    }

    RouterCore::~RouterCore()
    {
        if (m_pClientManager != nullptr)
            Stop();
    }

    Result RouterCore::Start(RouterStartInfo &startInfo)
    {
        Result result = Result::Error;

        // only do this if we have both a client manager and something to talk to
        std::lock_guard<std::mutex> transportLock(m_transportMutex);
        if ((m_pClientManager != nullptr) & (m_transportMap.size() > 0))
        {
            // Initialize the last discovery time to the current time - some offset.
            m_lastClientPingTimeInMs = 0;

            m_clientThread.active = true;
            m_clientThread.thread = std::thread(&DevDriver::RouterCore::RouterThreadFunc, this, std::ref(m_clientThread));

            DD_ASSERT(m_clientThread.thread.joinable());

            DD_PRINT(LogLevel::Verbose, "[RouterCore] Started client management thread!");

            // initialize the default query client info response
            ClientInfoStruct* DD_RESTRICT pPayload = reinterpret_cast<ClientInfoStruct *>(&m_clientInfoResponse.payload[0]);
            memset(pPayload, 0, sizeof(ClientInfoStruct));
            m_clientInfoResponse.header.messageId = static_cast<MessageCode>(SystemProtocol::SystemMessage::ClientInfo);
            m_clientInfoResponse.header.payloadSize = sizeof(ClientInfoStruct);
            m_clientInfoResponse.header.protocolId = Protocol::System;
            Platform::Strncpy(pPayload->clientDescription, startInfo.description, sizeof(pPayload->clientDescription));
            Platform::GetProcessName(&pPayload->clientName[0], sizeof(pPayload->clientName));
            pPayload->processId = Platform::GetProcessId();

            result = Result::Success;
        }
        return result;
    }

    Result RouterCore::SetClientManager(IClientManager *pClientManager)
    {
        Result result = Result::Error;
        if (m_pClientManager == nullptr)
        {
            result = pClientManager->RegisterHost(&m_clientId);
            if ((result == Result::Success) & (m_clientId != kBroadcastClientId))
            {
                m_pClientManager = pClientManager;

                DD_PRINT(LogLevel::Verbose, "[RouterCore] Registered client manager: %s", pClientManager->GetClientManagerName());

                const std::shared_ptr<IListenerTransport> &pHostTransport = m_pClientManager->GetHostTransport();
                if (pHostTransport != nullptr)
                {
                    result = RegisterTransport(pHostTransport);
                }
            }
        }
        return result;
    }

    Result RouterCore::RegisterTransport(const std::shared_ptr<IListenerTransport> &pTransport)
    {
        std::lock_guard<std::mutex> transportLock(m_transportMutex);
        TransportHandle handle = ++m_lastTransportId;
        Result result = pTransport->Enable(this, handle);

        if (result == DevDriver::Result::Success)
        {
            DD_PRINT(LogLevel::Verbose, "[RouterCore] Registered transport: %s", pTransport->GetTransportName());
            TransportContext newContext;
            newContext.pTransport = pTransport;
            m_transportMap[handle] = newContext;
        }

        return result;
    }

    Result RouterCore::RemoveTransport(const std::shared_ptr<IListenerTransport> &pTransport)
    {
        DD_ASSERT(pTransport != nullptr);

        Result result = Result::Error;

        TransportHandle tHandle = pTransport->GetHandle();
        std::unique_lock<std::mutex> transportLock(m_transportMutex);

        const auto &find = m_transportMap.find(tHandle);
        if (find != m_transportMap.end())
        {
            const auto clientMap = find->second.clientMap;
            for (const auto &pair : clientMap)
            {
                RemoveClient(pair.first);
            }
            m_transportMap.erase(tHandle);
            DD_PRINT(LogLevel::Verbose, "[RouterCore] Removing transport: %s", pTransport->GetTransportName());
            result = Result::Success;
        }

        transportLock.unlock();

        if (result == Result::Success)
        {
            pTransport->Disable();
        }
        return result;
    }

    void RouterCore::Stop()
    {
        DD_ASSERT(m_pClientManager != nullptr);

        DD_PRINT(LogLevel::Verbose, "[RouterCore] Shutting down transport threads...");

        std::unique_lock<std::mutex> transportLock(m_transportMutex);
        std::unordered_set<std::shared_ptr<IListenerTransport>> transports;

        for (auto &it : m_transportMap)
        {
            if (it.second.pTransport != nullptr)
                transports.emplace(it.second.pTransport);
        }
        transportLock.unlock();

        for (auto &pTransport : transports)
        {
            RemoveTransport(pTransport);
        }

        DD_PRINT(LogLevel::Verbose, "[RouterCore] Shutting down client management thread...");

        if (m_clientThread.active)
        {
            m_clientThread.active = false;

            if (m_clientThread.thread.joinable())
                m_clientThread.thread.join();

            DD_PRINT(LogLevel::Verbose, "[RouterCore] Client management thread successfully shut down!");
        }

        if (m_pClientManager != nullptr)
        {
            DevDriver::Result result = m_pClientManager->UnregisterHost();
            DD_ASSERT(result == DevDriver::Result::Success);
            DD_UNUSED(result);
            m_pClientManager = nullptr;
        }
    }

    Result RoutingCache::RouteMessage(const MessageContext & messageContext)
    {
        Result result = Result::Unavailable;
        const ClientId &dstClientId = messageContext.message.header.dstClientId;
        DD_ASSERT(messageContext.connectionInfo.handle != 0);
        if (m_pRouter->IsRoutableMessage(messageContext))
        {
            // If it's a broadcast message, we punt this over to the RouterCore since it has the current list of
            // all transports. This needs to be updated sometime to improve performance and minimize locking, but
            // for the time being this is acceptable.
            if (dstClientId == kBroadcastClientId)
            {
                m_pRouter->RouteBroadcastMessage(messageContext);
                result = Result::Success;
            }
            else
            {
                // If it is a directed message, we check to see if it's the same as the last client we talked to. That
                // lets us skip the overhead of looking up the connection info for every packet during burst traffic
                // situations
                if (m_currentClientId != dstClientId)
                {
                    // If it isn't, we first invalidate previous state
                    m_pCurrentClientContext = nullptr;
                    m_currentClientId = dstClientId;

                    // First things first, we perform a lookup in the cache.
                    const auto find = m_routingCache.find(dstClientId);
                    if (find != m_routingCache.end())
                    {
                        // If it existed in the cache then we can guarantee that there was - at one point - a valid
                        // transport associated with it.
                        m_pCurrentClientContext = &find->second;
                    }
                    else
                    {
                        // If it doesn't exist in the cache we need to look up both the connection info and the
                        // transport information from the main router.
                        CacheClientContext newClientContext = {};
                        ConnectionInfo& connectionInfo = newClientContext.connectionInfo;
                        std::shared_ptr<IListenerTransport>& pTransport = newClientContext.pTransport;

                        // First lookup the client ID to see if the router knows about it
                        if (m_pRouter->ConnectionInfoForClientId(dstClientId, &connectionInfo))
                        {
                            DD_ASSERT(connectionInfo.handle != 0);
                            // Then look up the transport associated with its transport handle. This lookup should
                            // never fail. If it fails, very bad things have happened and we are in an undefined state.
                            pTransport = m_pRouter->TransportForTransportHandle(connectionInfo.handle);
                            DD_ASSERT(pTransport != nullptr);

                            // Place the client context inside the routing cache and set the pointer to it.
                            const auto res = m_routingCache.emplace(dstClientId, newClientContext);
                            m_pCurrentClientContext = &res.first->second;
                        }
                    }
                }

                // If we have a valid client context then we send the message
                if (m_pCurrentClientContext != nullptr)
                {
                    const ConnectionInfo& connectionInfo = m_pCurrentClientContext->connectionInfo;
                    const std::shared_ptr<IListenerTransport>& pTransport = m_pCurrentClientContext->pTransport;

                    result = pTransport->TransmitMessage(connectionInfo, messageContext.message);

                    // If the transport failed (not timed out), erase the client from the routing cache, null out
                    // the current state, and remove the client from the Router.
                    if (result == Result::Error)
                    {
                        m_routingCache.erase(dstClientId);
                        m_pCurrentClientContext = nullptr;
                        m_currentClientId = kBroadcastClientId;

                        std::lock_guard<std::mutex> clientLock(m_pRouter->m_clientMutex);
                        std::lock_guard<std::mutex> transportLock(m_pRouter->m_transportMutex);
                        m_pRouter->RemoveClient(dstClientId);
                    }
                }
            }
        }
        return result;
    }
} // DevDriver
