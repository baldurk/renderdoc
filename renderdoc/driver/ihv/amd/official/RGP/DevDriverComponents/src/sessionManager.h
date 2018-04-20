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
* @file  sessionManager.h
* @brief Class declaration for SessionManager
***********************************************************************************************************************
*/

#pragma once

#include "gpuopen.h"
#include "msgChannel.h"
#include "protocolServer.h"
#include "protocolClient.h"
#include "session.h"
#include "util/vector.h"
#include "util/sharedptr.h"
#include "util/hashMap.h"

namespace DevDriver
{
    class SessionManager
    {
    public:
        // Constructor
        explicit SessionManager(const AllocCb& allocCb);

        // Destructor
        ~SessionManager();

        // Initialize the session manager.
        Result Init(IMsgChannel* pMessageChannel);

        // Destroy the session manager, closing all sessions in the process.
        Result Destroy();

        // Creates a session with the specified remote client, using the provided protocol client.
        Result EstablishSessionForClient(IProtocolClient& protocolClient, ClientId dstClientId);

        // Process a session message.
        void HandleReceivedSessionMessage(const MessageBuffer& messageBuffer);

        // Updates all active sessions.
        void UpdateSessions();

        // Registers the protocol server provided.
        Result RegisterProtocolServer(IProtocolServer* pServer);

        // Unregisters the protocol server provided and closes all associated connections.
        Result UnregisterProtocolServer(IProtocolServer* pServer);

        // Get the pointer to the protocol server associated with the provided protocol, or nullptr.
        IProtocolServer* GetProtocolServer(Protocol protocol);

        // Returns true if the protocol associated with the provided protocol exists, false otherwise.
        bool HasProtocolServer(Protocol protocol);

        // Notify the session manager that the destination client has disconnected.
        void HandleClientDisconnection(ClientId dstClientId);

        // Returns the currently associated ClientId, or kBroadcastClientId if not connected.
        ClientId GetClientId() const { return m_clientId; };
    private:
        // Server hash map goes from Protocol -> IProtocolServer*, with 8 buckets
        using ServerHashMap = HashMap<Protocol, IProtocolServer*, 8, DevDriver::NullHashFunc>;
        // Session hash map goes from SessionId -> SharedPointer<Session> with a default of 16 buckets
        using SessionHashMap = HashMap<SessionId, SharedPointer<Session>, 16>;

        // Convenience method to send a command packet (e.g., one with no payload) with the given parameters
        Result SendCommand(
            ClientId    remoteClientId,
            MessageCode command,
            SessionId   sessionId,
            Sequence    sequenceNumber,
            WindowSize  windowSize)
        {
            MessageBuffer messageBuffer = {};
            messageBuffer.header.dstClientId = remoteClientId;
            messageBuffer.header.srcClientId = m_clientId;
            messageBuffer.header.protocolId  = Protocol::Session;
            messageBuffer.header.messageId   = command;
            messageBuffer.header.sessionId   = sessionId;
            messageBuffer.header.sequence    = sequenceNumber;
            messageBuffer.header.payloadSize = 0;
            messageBuffer.header.windowSize  = windowSize;
            return m_pMessageChannel->Forward(messageBuffer);
        }

        // Convenience method to send a reset packet
        Result SendReset(ClientId remoteClientId, uint32 remoteSessionId, Result reason, Version version);

        SessionId GetNewSessionId(SessionId remoteSessionId);
        SharedPointer<Session> FindOpenSession(SessionId sessionId);

        ClientId         m_clientId;        // Client Id associated with the session manager.
        IMsgChannel*     m_pMessageChannel; // Message Channel object.
        Platform::Atomic m_lastSessionId;   // Counter used to generate unique session IDs.
        Platform::Mutex  m_sessionMutex;    // Mutex to synchronize session object access.
        SessionHashMap   m_sessions;        // Hash map containing currently active sessions.

        Platform::Mutex  m_serverMutex;     // Mutex to synchronize access to protocol servers.
        ServerHashMap    m_protocolServers; // Hash map containing protocol servers.

        bool             m_active;          // Flag used to indicate whether the client accepts or rejects
                                            // new sessions.
        AllocCb          m_allocCb;         // Allocator callbacks.
    };

} // DevDriver
