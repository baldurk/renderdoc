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
* @file  session.h
* @brief Class declaration for session
***********************************************************************************************************************
*/

#pragma once

#include "gpuopen.h"
#include "ddPlatform.h"
#include "util/sharedptr.h"
#include "protocols/systemProtocols.h"
#include "protocolSession.h"
#include "protocolClient.h"
#include "protocolServer.h"

namespace DevDriver
{
    class IMsgChannel;

    enum struct SessionState
    {
        Closed = 0,
        Listening,
        SynSent,
        SynReceived,
        Established,
        FinWait1,
        Closing,
        FinWait2,
        Count
    };

    DD_STATIC_CONST WindowSize kDefaultWindowSize = 128;
    DD_STATIC_CONST float kInitialRoundTripTimeInMs = 50.0f;

    class Session : public ISession
    {
    public:
        ///
        /// Internal interface for SessionManager
        ///
        Session(IMsgChannel* pMsgChannel);

        Result Connect(IProtocolClient& owner,
                       ClientId remoteClientId,
                       SessionId sessionId);

        Result BindToServer(IProtocolServer& owner,
                           ClientId remoteClientId,
                           SessionProtocol::SessionVersion sessionVersion,
                           Version protocolVersion,
                           SessionId sessionId);

        void HandleMessage(SharedPointer<Session>& pSession, const MessageBuffer& messageBuffer);

        SessionState GetSessionState() const
        {
            return m_sessionState;
        }

        void CloseIfOwnedBy(SharedPointer<Session>& pSession, IProtocolSession* pOwner)
        {
            if (pOwner == m_pProtocolOwner)
            {
                if (m_pProtocolOwner != nullptr)
                {
                    m_pProtocolOwner->SessionTerminated(pSession, Result::EndOfStream);
                    Orphan();
                }
                Shutdown(Result::Success);
            }
        }

        bool IsSessionOpenAndMatches(ClientId remoteClientId, SessionId sessionId)
        {
            return ((m_sessionId == sessionId) &
                (m_remoteClientId == remoteClientId) &
                    (m_sessionState != SessionState::Closed));
        }

        void Update(const SharedPointer<Session>& pSession)
        {
            DD_ASSERT(pSession.Get() == this);

            UpdateReceiveWindow();
            UpdateSendWindow();
            UpdateTimeout();

            // Update active sessions for non-clients
            if (m_sessionState >= SessionState::Established)
            {
                if (m_pProtocolOwner != nullptr)
                {
                    m_pProtocolOwner->UpdateSession(pSession);
                }
                else
                {
                    Shutdown(Result::Error);
                }
            }

            if (m_sessionState == SessionState::Closed)
            {
                DD_PRINT(LogLevel::Debug, "[Session] Session %u terminated - reason %u", m_sessionId, m_sessionTerminationReason);

                if (m_pProtocolOwner != nullptr)
                {
                    m_pProtocolOwner->SessionTerminated(pSession, m_sessionTerminationReason);
                    Orphan();
                }
            }
        }

        ///
        /// Public ISession interface implementation
        ///

        Result Send(uint32 payloadSizeInBytes, const void* pPayload, uint32 timeoutInMs) override final;
        Result Receive(uint32 payloadSizeInBytes, void* pPayload, uint32* pBytesReceived, uint32 timeoutInMs) override final;
        void   Shutdown(Result reason) override final;
        void   Close(Result reason) override final;

#if !DD_VERSION_SUPPORTS(GPUOPEN_SESSION_INTERFACE_CLEANUP_VERSION)
        void OrphanSession() override final
        {
            Orphan();
        };

        void CloseSession(Result reason = Result::Error) override final
        {
            Shutdown(reason);
        };
#endif

        void* SetUserData(void* pUserData) override final
        {
            return Platform::Exchange(m_pSessionUserdata, pUserData);
        }

        void *GetUserData() const override final
        {
            return m_pSessionUserdata;
        }

        SessionId GetSessionId() const override final
        {
            return m_sessionId;
        }

        ClientId GetDestinationClientId() const override final
        {
            return m_remoteClientId;
        }

        Version GetVersion() const override final
        {
            return m_protocolVersion;
        }

    private:
        Result MarkMessagesAsAcknowledged(Sequence maxSequenceNumber);
        Result WriteMessageIntoReceiveWindow(const MessageBuffer& messageBuffer);
        Result WriteMessageIntoSendWindow(SessionProtocol::SessionMessage message, uint32 payloadSizeInBytes, const void* pPayload, uint32 timeoutInMs);

        bool SendOrClose(const MessageBuffer& messageBuffer);
        bool SendControlMessage(SessionProtocol::SessionMessage command, Sequence sequenceNumber);
        bool SendAckMessage();

        void HandleSynMessage(const MessageBuffer& messageBuffer);
        void HandleSynAckMessage(const MessageBuffer& messageBuffer);
        void HandleFinMessage(const MessageBuffer& messageBuffer);
        void HandleDataMessage(const MessageBuffer& messageBuffer);
        void HandleAckMessage(const MessageBuffer& messageBuffer);
        void HandleRstMessage(const MessageBuffer& messageBuffer);

        void UpdateReceiveWindow();
        void UpdateSendWindow();
        void UpdateTimeout();

        WindowSize CalculateCurrentWindowSize();
        bool IsSendWindowEmpty();
        void UpdateSendWindowSize(const MessageBuffer& messageBuffer);

        void Orphan();

        inline void SetState(SessionState newState);

        template <WindowSize size = kDefaultWindowSize>
        struct TransmitWindow
        {
            MessageBuffer           messages[size];
            Sequence                sequence[size];
            uint64                  initialTransmitTimeInMs[size];
            volatile bool           valid[size];

            Platform::AtomicLock    lock;
            Platform::Semaphore     semaphore;

            Sequence                nextSequence;
            Sequence                nextUnacknowledgedSequence;
            Sequence                lastSentSequence;
            uint32                  lastAckCount;
            float                   roundTripTime;
            uint8                   retransmitCount;

            WindowSize              lastAvailableSize;

            constexpr WindowSize GetWindowSize() const { return size; };

            TransmitWindow() :
                messages(),
                sequence(),
                initialTransmitTimeInMs(),
                valid(),
                lock(),
                semaphore(size, size),
                nextSequence(1),
                nextUnacknowledgedSequence(1),
                lastSentSequence(0),
                lastAckCount(),
                roundTripTime(kInitialRoundTripTimeInMs),
                retransmitCount(0),
                lastAvailableSize(1)
            {

            };
        };

        template <WindowSize size = kDefaultWindowSize>
        struct ReceiveWindow
        {
            MessageBuffer           messages[size];
            Sequence                sequence[size];
            volatile bool           valid[size];

            Platform::AtomicLock    lock;
            Platform::Semaphore     semaphore;

            Sequence                nextUnreadSequence;
            Sequence                nextExpectedSequence;
            Sequence                lastUnacknowledgedSequence;
            WindowSize              currentAvailableSize;

            constexpr WindowSize MaxAdvertizedSize() const { return size - (size >> 1); };
            constexpr WindowSize GetWindowSize() const { return size; };

            ReceiveWindow() :
                messages(),
                sequence(),
                valid(),
                lock(),
                semaphore(0, size),
                nextUnreadSequence(1),
                nextExpectedSequence(1),
                lastUnacknowledgedSequence(1),
                currentAvailableSize(size - (size >> 1))
            {
            }
        };

        TransmitWindow<kDefaultWindowSize>  m_sendWindow;
        ReceiveWindow<kDefaultWindowSize>   m_receiveWindow;
        IMsgChannel* const                  m_pMsgChannel;
        IProtocolSession*                   m_pProtocolOwner;
        void*                               m_pSessionUserdata;
        const ClientId                      m_clientId;
        ClientId                            m_remoteClientId;
        SessionId                           m_sessionId;
        SessionState                        m_sessionState;
        Result                              m_sessionTerminationReason;
        Version                             m_protocolVersion;
        SessionProtocol::SessionVersion     m_sessionVersion;
    };
} // DevDriver
