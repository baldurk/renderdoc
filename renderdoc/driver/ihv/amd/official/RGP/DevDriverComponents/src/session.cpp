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

#include "session.h"
#include "sessionManager.h"
#include "msgChannel.h"
#include "protocolServer.h"
#include "protocolClient.h"
#include <cstring>
#include <cstdio>

#if !defined(NDEBUG) && !defined(DEVDRIVER_SHOW_SYMBOLS)
#define DEVDRIVER_SHOW_SYMBOLS
#endif

using namespace DevDriver::SessionProtocol;
using namespace DevDriver::Platform;

namespace DevDriver
{
#if defined(DEVDRIVER_SHOW_SYMBOLS)
    static_assert(static_cast<uint32>(SessionState::Closed) == 0, "Unexpected SessionState::Closed value.");
    static_assert(static_cast<uint32>(SessionState::Listening) == 1, "Unexpected SessionState::Listening value.");
    static_assert(static_cast<uint32>(SessionState::SynSent) == 2, "Unexpected SessionState::SynSent value.");
    static_assert(static_cast<uint32>(SessionState::SynReceived) == 3, "Unexpected SessionState::SynReceived value.");
    static_assert(static_cast<uint32>(SessionState::Established) == 4, "Unexpected SessionState::Established value.");
    static_assert(static_cast<uint32>(SessionState::FinWait1) == 5, "Unexpected SessionState::FinWait1 value.");
    static_assert(static_cast<uint32>(SessionState::Closing) == 6, "Unexpected SessionState::Closing value.");
    static_assert(static_cast<uint32>(SessionState::FinWait2) == 7, "Unexpected SessionState::FinWait2 value.");
    static_assert(static_cast<uint32>(SessionState::Count) == 8, "Unexpected SessionState::Count value.");

    DD_STATIC_CONST const char* kStateName[static_cast<uint32>(SessionState::Count)] =
    {
        "Closed",           // Closed
        "Listening",        // Listening
        "SynSent",          // SynSent
        "SynReceived",      // SynReceived
        "Established",      // Established
        "FinWait1",         // FinWait1
        "Closing",          // Closing
        "FinWait2",         // FinWait2
    };
#endif

    DD_STATIC_CONST uint32 kMaxRetransmits = 5;
    DD_STATIC_CONST float kMovingAverageWindow = 2;
    DD_STATIC_CONST float kAlpha = 2.0f / (kMovingAverageWindow + 1.0f);
    DD_STATIC_CONST uint32 kFastRetransmitThreshold = 3;
    DD_STATIC_CONST float kMinRetransmitDelay = 100.0f;
    DD_STATIC_CONST float kMaxRetransmitDelay = 2000.0f;
    DD_STATIC_CONST uint32 kMaxUnacknowledgedThreshold = 5;

    Session::Session(IMsgChannel* pMsgChannel) :
        m_pMsgChannel(pMsgChannel),
        m_pProtocolOwner(nullptr),
        m_pSessionUserdata(nullptr),
        m_clientId(pMsgChannel->GetClientId()),
        m_remoteClientId(kBroadcastClientId),
        m_sessionId(kInvalidSessionId),
        m_sessionState(SessionState::Closed),
        m_sessionTerminationReason(Result::Success),
        m_protocolVersion(0),
        m_sessionVersion(kSessionProtocolVersion)
    {
    }

    // Transmits a message and closes the session on error. This helps catches instances where the underlying transport
    // has disconnected.
    bool Session::SendOrClose(const MessageBuffer& messageBuffer)
    {
        bool result = true;
        Result sendResult;
        do
        {
            sendResult = m_pMsgChannel->Forward(messageBuffer);
        } while (sendResult == Result::NotReady);

        if (sendResult != Result::Success)
        {
            Shutdown(Result::Error);
            result = false;
        }
        return result;
    }

    bool Session::SendControlMessage(SessionMessage command, Sequence sequenceNumber)
    {
        MessageBuffer messageBuffer = {};
        messageBuffer.header.dstClientId = m_remoteClientId;
        messageBuffer.header.srcClientId = m_clientId;
        messageBuffer.header.protocolId = Protocol::Session;
        messageBuffer.header.messageId = static_cast<MessageCode>(command);
        messageBuffer.header.sessionId = m_sessionId;
        messageBuffer.header.sequence = sequenceNumber;
        messageBuffer.header.payloadSize = 0;
        messageBuffer.header.windowSize = m_receiveWindow.currentAvailableSize;

        return SendOrClose(messageBuffer);
    }

    bool Session::SendAckMessage()
    {
        // transmit an ack based on the current max sequence received
        const Sequence& seq = m_receiveWindow.nextExpectedSequence;
        m_receiveWindow.lastUnacknowledgedSequence = seq;
        m_receiveWindow.currentAvailableSize = CalculateCurrentWindowSize();
        return SendControlMessage(SessionMessage::Ack, (seq - 1));
    }

    Result Session::MarkMessagesAsAcknowledged(Sequence maxSequenceNumber)
    {
        Result result = Result::Error;

        LockGuard<AtomicLock> lock(m_sendWindow.lock);

        Sequence seq = m_sendWindow.nextUnacknowledgedSequence;
        const uint64 currentTime = Platform::GetCurrentTimeInMs();
        float currentAverage = m_sendWindow.roundTripTime;

        // the first pass goes through and invalidates all packets that have been acknowledged
        while (seq <= Min(maxSequenceNumber, m_sendWindow.lastSentSequence))
        {
            const Sequence index = seq % m_sendWindow.GetWindowSize();

            DD_ASSERT(m_sendWindow.valid[index] == true && m_sendWindow.sequence[index] == seq);
            if ((m_sendWindow.valid[index] != true) | (m_sendWindow.sequence[index] != seq))
                break;

            m_sendWindow.valid[index] = false;

            // if we aren't in the middle of retransmit, feel free to use this as part of the round trip time
            if (m_sendWindow.retransmitCount == 0)
            {
                const uint64 elapsedTimeInMs = currentTime - m_sendWindow.initialTransmitTimeInMs[index];
                currentAverage = (kAlpha * elapsedTimeInMs) + ((1.0f - kAlpha) * currentAverage);
            }

            DD_ASSERT((m_sendWindow.nextSequence - seq) <= m_sendWindow.GetWindowSize());

            m_sendWindow.semaphore.Signal();
            seq++;
        }

        // if we acknowledged any new packets this time
        if (m_sendWindow.nextUnacknowledgedSequence < seq)
        {
            result = Result::Success;

            DD_PRINT(LogLevel::Debug, "Current round trip time: %0.2f", currentAverage);
            // housekeeping
            m_sendWindow.roundTripTime = currentAverage;
            m_sendWindow.retransmitCount = 0;
            m_sendWindow.nextUnacknowledgedSequence = seq;
            m_sendWindow.lastAckCount = 0;

        }
        else if (m_sendWindow.nextUnacknowledgedSequence == seq)
        {
            // handle case MarkMessagesAsAcknowledged was called we didn't actually mark any messages as acknowledged
            // This typically means that a packet was dropped and the other host has started retransmitting duplicate
            // ack packets
            m_sendWindow.lastAckCount++;

            // if we've passed the fast retransmit threshold we need to automatically start retransmitting data
            // we start at the first unacknowledged packet, and retransmit one additional packet for every duplicate we
            // receive
            if (m_sendWindow.lastAckCount >= kFastRetransmitThreshold)
            {
                // calculate the nextSequence number for the packet to retransmit
                const Sequence retransSeq = m_sendWindow.nextUnacknowledgedSequence +
                    (m_sendWindow.lastAckCount - kFastRetransmitThreshold);

                DD_PRINT(LogLevel::Debug, "FAST RETRANS session %u seq %u", m_sessionId, retransSeq);

                // Calculate the index for the packet to be retransmitted
                const Sequence index = retransSeq % m_sendWindow.GetWindowSize();

                // Re-write the window size in the retransmitted packet
                m_sendWindow.messages[index].header.windowSize = m_receiveWindow.currentAvailableSize;

                if (SendOrClose(m_sendWindow.messages[index]))
                {
                    // If we successfully transmitted this we want to reset the transmit count so that regular
                    // retransmit doesn't take affect
                    m_sendWindow.retransmitCount = 0;
                }
            }
        }
        return result;
    }

    Result Session::WriteMessageIntoReceiveWindow(const MessageBuffer& messageBuffer)
    {
        DD_PRINT(LogLevel::Debug,
                         "Attempting to write message with seq %u into session %u's receive window",
                         messageBuffer.header.sequence,
                         m_sessionId);

        Result result = Result::Error;
        LockGuard<AtomicLock> lock(m_receiveWindow.lock);

        Sequence nextSequence = m_receiveWindow.nextExpectedSequence;

        // check to see if we have any unacknowledged data in the receive window
        const bool pendingAck = (nextSequence > m_receiveWindow.lastUnacknowledgedSequence);

        if ((messageBuffer.header.sequence >= nextSequence)
            & (messageBuffer.header.payloadSize <= kMaxPayloadSizeInBytes))
        {
            const Sequence distance = (messageBuffer.header.sequence - m_receiveWindow.nextUnreadSequence);

            if (distance < m_receiveWindow.GetWindowSize())
            {
                DD_PRINT(LogLevel::Debug,
                         "Session %u received message sq %u",
                         m_sessionId,
                         messageBuffer.header.sequence);

                // Send the request.
                const uint32 index = messageBuffer.header.sequence % m_receiveWindow.GetWindowSize();

                // DD_ASSERT(m_receiveWindow.valid[index] == false);

                // copy data + set associated state
                memcpy(&m_receiveWindow.messages[index],
                       &messageBuffer,
                       sizeof(MessageHeader) + messageBuffer.header.payloadSize);

                m_receiveWindow.sequence[index] = messageBuffer.header.sequence;
                m_receiveWindow.valid[index] = true;

                // Step the sequence number forward until we find an invalid packet or finish scanning the entire window.
                while ((nextSequence - m_receiveWindow.nextUnreadSequence) < m_receiveWindow.GetWindowSize())
                {
                    if (m_receiveWindow.valid[nextSequence % m_receiveWindow.GetWindowSize()])
                    {
                        // Increment the sequence number since this is a valid packet
                        nextSequence++;
                        DD_ASSERT(m_receiveWindow.nextUnreadSequence != nextSequence);
                        m_receiveWindow.semaphore.Signal();
                    } else
                    {
                        // Break out since we found an invalid packet
                        break;
                    }
                }

                m_receiveWindow.nextExpectedSequence = nextSequence;

                // if we already have data waiting we want to ack in two conditions
                //  1) if too many packets have not been acknowledged
                //  2) if we have waited more than half of the a round trip time
                // This is to prevent situations where a client retransmits a bunch of data unnecessarily
                if (pendingAck)
                {
                    const uint64 unackDistance = (nextSequence - m_receiveWindow.lastUnacknowledgedSequence);
                    if (unackDistance >= kMaxUnacknowledgedThreshold)
                    {
                        DD_PRINT(LogLevel::Debug, "Early ack seq %u", (nextSequence - 1));
                        SendAckMessage();
                    }
                }
                result = Result::Success;
            }
        }
        else
        {
            // if this data arrived out of order or was retransmitted
            // we go ahead and send a new acknowledgment now without waiting
            if (!pendingAck)
            {
                DD_PRINT(LogLevel::Debug, "Reack seq %u", (nextSequence - 1));
            }
            SendAckMessage();
            result = Result::Success;
        }
        return result;
    }

    Result Session::WriteMessageIntoSendWindow(
        SessionMessage message,
        uint32 payloadSizeInBytes,
        const void* pPayload, uint32 timeoutInMs)
    {
        Result result = Result::Error;

        if (m_sessionState < SessionState::FinWait2)
        {
            if (payloadSizeInBytes <= kMaxPayloadSizeInBytes)
            {
                result = m_sendWindow.semaphore.Wait(timeoutInMs);

                if (result == Result::Success)
                {
                    LockGuard<AtomicLock> lock(m_sendWindow.lock);

                    const Sequence distance = m_sendWindow.nextSequence - m_sendWindow.nextUnacknowledgedSequence;
                    DD_UNUSED(distance);
                    DD_ASSERT(distance < m_sendWindow.GetWindowSize());

                    const Sequence seq = m_sendWindow.nextSequence;
                    ++m_sendWindow.nextSequence;

                    DD_PRINT(LogLevel::Never, "Sending a message with sequence number %u", seq);
                    DD_PRINT(LogLevel::Never, "Next sequence number %u", m_sendWindow.nextSequence);

                    const Sequence index = seq % m_sendWindow.GetWindowSize();

                    DD_ASSERT(m_sendWindow.valid[index] == false);
                    DD_ASSERT((payloadSizeInBytes > 0 && pPayload != nullptr) || (pPayload == nullptr && payloadSizeInBytes == 0));

                    MessageBuffer& messageBuffer = m_sendWindow.messages[index];
                    // Set up the message header.
                    messageBuffer.header.srcClientId = m_clientId;
                    messageBuffer.header.dstClientId = m_remoteClientId;
                    messageBuffer.header.protocolId = Protocol::Session;
                    messageBuffer.header.messageId = static_cast<MessageCode>(message);
                    messageBuffer.header.sessionId = m_sessionId;
                    messageBuffer.header.windowSize = m_receiveWindow.currentAvailableSize;
                    messageBuffer.header.sequence = seq;

                    if ((pPayload != nullptr) & (payloadSizeInBytes > 0))
                    {
                        memcpy(&messageBuffer.payload[0], pPayload, payloadSizeInBytes);
                        messageBuffer.header.payloadSize = payloadSizeInBytes;
                    }
                    else
                    {
                        messageBuffer.header.payloadSize = 0;
                    }

                    m_sendWindow.sequence[index] = seq;
                    m_sendWindow.valid[index] = true;
                }
            }
            else
            {
                result = Result::InsufficientMemory;
            }
        }
        return result;
    }

    Result Session::Connect(IProtocolClient& owner,
                            ClientId remoteClientId,
                            SessionId sessionId)
    {
        Result result = Result::Error;

        // A session can only connect to a remote session if:
        // 1) The provided object is indeed a Client object
        // 2) The remote client ID provided is not a broadcast client ID
        // 3) The session ID provided is not invalid
        // 4) The session is in the closed state
        if ((owner.GetType() == SessionType::Client) &&
            (remoteClientId != kBroadcastClientId) &&
            (sessionId != kInvalidSessionId) &&
            (m_sessionState == SessionState::Closed))
        {
            m_pProtocolOwner = &owner;
            m_remoteClientId = remoteClientId;
            m_sessionId = sessionId;

            // Write the payload data for a session request packet.
            SynPayload payload = {};
            payload.protocol = m_pProtocolOwner->GetProtocol();
            payload.minVersion = m_pProtocolOwner->GetMinVersion();
            payload.maxVersion = m_pProtocolOwner->GetMaxVersion();
            payload.sessionVersion = m_sessionVersion;
            result = WriteMessageIntoSendWindow(SessionMessage::Syn, sizeof(SynPayload), &payload, kInfiniteTimeout);

            if (result == Result::Success)
            {
                SetState(SessionState::SynSent);
            }
        }
        return result;
    }

    Result Session::BindToServer(IProtocolServer& owner,
                                ClientId remoteClientId,
                                SessionVersion sessionVersion,
                                Version protocolVersion,
                                SessionId sessionId)
    {
        Result result = Result::Error;

        // We can only bind to a protocol server if:
        // 1) The provided object is indeed a Server object
        // 2) The remote client ID provided is not a broadcast client ID
        // 3) The session ID provided is not invalid
        // 4) The session is in the closed state
        if ((owner.GetType() == SessionType::Server) &&
            (remoteClientId != kBroadcastClientId) &&
            (sessionId != kInvalidSessionId) &&
            (m_sessionState == SessionState::Closed))
        {
            m_pProtocolOwner = &owner;
            m_remoteClientId = remoteClientId;
            m_sessionVersion = Platform::Min(sessionVersion, kSessionProtocolVersion);
            m_protocolVersion = protocolVersion;
            m_sessionId = sessionId;
            SetState(SessionState::Listening);
            result = Result::Success;
        }
        return result;
    }

    void Session::HandleMessage(SharedPointer<Session>& pSession, const MessageBuffer& messageBuffer)
    {
        const SessionState initialState = GetSessionState();
        switch (static_cast<SessionMessage>(messageBuffer.header.messageId))
        {
            case SessionMessage::Syn:
                HandleSynMessage(messageBuffer);
                break;
            case SessionMessage::SynAck:
                HandleSynAckMessage(messageBuffer);
                break;
            case SessionMessage::Fin:
                HandleFinMessage(messageBuffer);
                break;
            case SessionMessage::Data:
                HandleDataMessage(messageBuffer);
                break;
            case SessionMessage::Ack:
                HandleAckMessage(messageBuffer);
                break;
            case SessionMessage::Rst:
                HandleRstMessage(messageBuffer);
                break;
            default:
                DD_UNREACHABLE();
                break;
        }

        // If the message caused a state change, we need to process the state transition. For now, that is only
        // issuing the session established callback.
        if (initialState != m_sessionState)
        {
            switch (m_sessionState)
            {
                case SessionState::Established:
                {
                    if (m_pProtocolOwner != nullptr)
                    {
                        m_pProtocolOwner->SessionEstablished(pSession);
                    }
                    else
                    {
                        Shutdown(Result::Error);
                    }
                }
                break;
                default:
                    break;
            }
        }
    }

    void Session::HandleSynMessage(const MessageBuffer& messageBuffer)
    {
        DD_ASSERT(m_pProtocolOwner->GetType() == SessionType::Server);

        const SessionId& remoteSessionId = messageBuffer.header.sessionId;
        const Sequence& receiveSequence = messageBuffer.header.sequence;

        // Write the payload data for a session request packet.
        SynAckPayload payload = {};
        payload.initialSessionId = remoteSessionId;
        payload.sequence = receiveSequence;
        payload.version = m_protocolVersion;
        payload.sessionVersion = m_sessionVersion;
        const Result result = WriteMessageIntoSendWindow(SessionMessage::SynAck,
                                                         sizeof(SynAckPayload),
                                                         &payload,
                                                         kInfiniteTimeout);
        if (result == Result::Success)
        {
            SetState(SessionState::SynReceived);
            m_receiveWindow.nextUnreadSequence = receiveSequence + 1;
            m_receiveWindow.nextExpectedSequence = receiveSequence + 1;
            m_receiveWindow.lastUnacknowledgedSequence = receiveSequence + 1;
            m_receiveWindow.currentAvailableSize = m_receiveWindow.MaxAdvertizedSize();
        }
        else
        {
            Shutdown(Result::Error);
        }
    }

    void Session::HandleSynAckMessage(const MessageBuffer& messageBuffer)
    {
        switch (m_sessionState)
        {
            // These should not happen during normal operation, but in if they do we need to handle it
        case SessionState::FinWait1:
        case SessionState::FinWait2:
        case SessionState::Closing:
            // Established is the expected case here
        case SessionState::Established:
            MarkMessagesAsAcknowledged(messageBuffer.header.sequence);
            break;
        case SessionState::SynSent:
        {
            // A client has sent a response back from an earlier session request.
            // Find the local session data.
            DD_PRINT(LogLevel::Debug, "Received SYNACK");
            // The local session data's remote client id should always match the sender's id.
            const SynAckPayload* DD_RESTRICT pPayload = reinterpret_cast<const SynAckPayload*>(&messageBuffer.payload[0]);

            MarkMessagesAsAcknowledged(pPayload->sequence);

            m_sessionVersion = pPayload->sessionVersion;
            DD_PRINT(LogLevel::Debug, "Established session with session version %u\n", m_sessionVersion);
            DD_PRINT(LogLevel::Debug, "Acknowledging SYNACK packet %u", messageBuffer.header.sequence);

            SetState(SessionState::Established);

            // Update our remote session id in the local session data.
            m_sessionId = messageBuffer.header.sessionId;
            if (pPayload->version != 0)
            {
                m_protocolVersion = pPayload->version;
            }
            else if (m_pProtocolOwner != nullptr)
            {
                m_protocolVersion = m_pProtocolOwner->GetMinVersion();
            }

            m_receiveWindow.nextUnreadSequence = messageBuffer.header.sequence + 1;
            m_receiveWindow.nextExpectedSequence = messageBuffer.header.sequence + 1;
            m_receiveWindow.lastUnacknowledgedSequence = messageBuffer.header.sequence + 1;
            m_receiveWindow.currentAvailableSize = m_receiveWindow.MaxAdvertizedSize();
            SendAckMessage();
            break;
        }
        default:
            break;
        }

        // Update the send window size
        UpdateSendWindowSize(messageBuffer);
    }

    void Session::HandleFinMessage(const MessageBuffer& messageBuffer)
    {
        if (m_sessionState < SessionState::Closing)
        {
            WriteMessageIntoReceiveWindow(messageBuffer);
            // Mark the session as terminated;
            SetState(SessionState::Closing);
            m_sessionTerminationReason = Result::Success;
        }
        else if (m_sessionState == SessionState::FinWait2)
        {
            // if we've hit this point we received a Fin message while waiting on an ack for a Fin message.
            // Best thing we can do is send an acknowledgement and close the session immediately
            WriteMessageIntoReceiveWindow(messageBuffer);
            SendAckMessage();
            SetState(SessionState::Closed);
            m_sessionTerminationReason = Result::Success;
        }
        // Update the send window size
        UpdateSendWindowSize(messageBuffer);
    }

    void Session::HandleDataMessage(const MessageBuffer& messageBuffer)
    {
        switch (m_sessionState)
        {
        case SessionState::FinWait1: // we are closing, but received data
        case SessionState::FinWait2: // we are waiting for an ack but received data
        case SessionState::Established:
        {
            WriteMessageIntoReceiveWindow(messageBuffer);
            break;
        }
        default:
            break;
        }

        // Update the send window size
        UpdateSendWindowSize(messageBuffer);
    }

    void Session::HandleAckMessage(const MessageBuffer& messageBuffer)
    {
        switch (m_sessionState)
        {
        case SessionState::SynReceived: // SynAck was transmitted, waiting on ack
            DD_PRINT(LogLevel::Debug, "Received ACK while in SYN_RECEIVED");
            SetState(SessionState::Established);
            MarkMessagesAsAcknowledged(messageBuffer.header.sequence);
            break;
        case SessionState::Established: // Session has been established
        case SessionState::FinWait1: // Session has requested a disconnect
        case SessionState::FinWait2: // Session sent Fin and is waiting on ack
        case SessionState::Closing: // Session has received a Fin packet and sending data
            MarkMessagesAsAcknowledged(messageBuffer.header.sequence);
            break;
        default:
            break;
        }

        // Update the send window size
        UpdateSendWindowSize(messageBuffer);
    }

    void Session::HandleRstMessage(const MessageBuffer& messageBuffer)
    {
        const Result reason = static_cast<Result>(messageBuffer.header.sequence);
        Shutdown(reason);

        // TODO: figure out how to propagate this back
        //if (reason == Result::VersionMismatch)
        //{
        //    m_protocolVersion = static_cast<Version>(messageBuffer.header.windowSize);
        //}

        UpdateSendWindowSize(messageBuffer);
    }

    WindowSize Session::CalculateCurrentWindowSize()
    {
        // Make sure we don't get a negative number
        DD_ASSERT(m_receiveWindow.nextExpectedSequence >= m_receiveWindow.nextUnreadSequence);

        int32 windowSize = static_cast<int32>(m_receiveWindow.MaxAdvertizedSize()) -
                           static_cast<int32>(m_receiveWindow.nextExpectedSequence - m_receiveWindow.nextUnreadSequence);

        // Always make sure we return atleast 1 for the window size or the sender will stop sending messages entirely.
        windowSize = Platform::Max(windowSize, 1);

        return static_cast<WindowSize>(windowSize);
    }

    void Session::UpdateSendWindowSize(const MessageBuffer& messageBuffer)
    {
        // Update the window size based on the packet we received
        LockGuard<AtomicLock> lock(m_sendWindow.lock);
        m_sendWindow.lastAvailableSize = messageBuffer.header.windowSize;
    }

    bool Session::IsSendWindowEmpty()
    {
        LockGuard<AtomicLock> lock(m_sendWindow.lock);

        // check to see if any packets we have sent have not been acknowledged
        bool isEmpty = (m_sendWindow.nextUnacknowledgedSequence > m_sendWindow.lastSentSequence);

        // we also need to check to make sure there are no unsent packets
        // this is necessary because on close a client will often write the Fin packet into the
        // transmit window and then try to close the session before it has been able to transmit the
        // data. This test enforces proper behavior, however due to separate bugs that could cause
        // sessions to get stuck while closing we cannot actually enable it unless we know the
        // server has the appropriate fixes.
        if (m_sessionVersion >= kSessionProtocolVersionSynAckVersion)
        {
            isEmpty &= ((m_sendWindow.lastSentSequence + 1) == m_sendWindow.nextSequence);
        }
        return isEmpty;
    }

    void Session::UpdateReceiveWindow()
    {
        LockGuard<AtomicLock> lock(m_receiveWindow.lock);
        {
            const Sequence& seq = m_receiveWindow.nextExpectedSequence;
            if (seq > m_receiveWindow.lastUnacknowledgedSequence)
            {
                // if there is unacknowledged data in the receive window we need to acknowledge it
                DD_PRINT(LogLevel::Never, "Acknowledging packets %u-%u", m_receiveWindow.lastUnacknowledgedSequence, (seq - 1));
                SendAckMessage();
            }
        }
    }

    void Session::UpdateSendWindow()
    {
        LockGuard<AtomicLock> lock(m_sendWindow.lock);

        // check to see if we have any data we sent that hasn't been acknowledged yet
        if (m_sendWindow.nextUnacknowledgedSequence <= m_sendWindow.lastSentSequence)
        {
            // only do this if we haven't hit the retransmit limit yet
            if (m_sendWindow.retransmitCount <= kMaxRetransmits)
            {
                static_assert(kMaxRetransmits <= 14, "Error, retransmitMultiplier doesn't have enough precision for requested retransmit count");
                // calculate the current timeout
                // equivalent to 2 ^ (retransmitCount + 1)
                const uint16 retransmitMultiplier = 2 << m_sendWindow.retransmitCount;
                const float retransmitTimeout = Max(m_sendWindow.roundTripTime, kMinRetransmitDelay);
                const uint64 currentTimeout = (uint64)Min((retransmitTimeout * retransmitMultiplier), kMaxRetransmitDelay);
                const uint64 currentTime = Platform::GetCurrentTimeInMs();

                uint8 count = 0;

                // for every unacknowledged packet
                for (Sequence seq = m_sendWindow.nextUnacknowledgedSequence;
                     seq <= m_sendWindow.lastSentSequence;
                     seq++)
                {
                    const Sequence index = seq % m_sendWindow.GetWindowSize();
                    const uint64 currentDifference = (currentTime - m_sendWindow.initialTransmitTimeInMs[index]);

                    // if it hasn't timed out yet we abort
                    if (currentDifference <= currentTimeout)
                    {
                        break;
                    }

                    DD_ASSERT(m_sendWindow.valid[index] == true);
                    DD_ASSERT(m_sendWindow.sequence[index] == seq);

                    m_sendWindow.messages[index].header.windowSize = m_receiveWindow.currentAvailableSize;

                    // if we couldn't retransmit the message we abort
                    if (!SendOrClose(m_sendWindow.messages[index]))
                    {
                        break;
                    }
                    count++;
                    DD_PRINT(LogLevel::Debug, "RETRANSMIT: rtt: %0.2f retransmit timeout: %llu diff: %llu", m_sendWindow.roundTripTime, currentTimeout, currentDifference);
                    DD_PRINT(LogLevel::Debug, "RETRANSMIT: session %u seq %u count %u", m_sessionId, seq, m_sendWindow.retransmitCount);
                }

                // if we successfully retransmitted any packets we increment the retrans count
                if (count > 0)
                {
                    DD_PRINT(LogLevel::Debug, "RETRANSMIT: retransmitted %u packets", count);
                    m_sendWindow.retransmitCount += 1;
                }
            }
            else
            {
                Shutdown(Result::NotReady);
            }
        }

        // proceed to transmit any data we haven't sent yet
        const WindowSize& windowSize = m_sendWindow.GetWindowSize();

        Sequence seq = m_sendWindow.lastSentSequence + 1;
        while ((seq < m_sendWindow.nextSequence) & (m_sendWindow.lastAvailableSize > 0))
        {
            const uint32 index = seq % windowSize;
            if (m_sendWindow.valid[index] & (seq == m_sendWindow.sequence[index]))
            {
                MessageBuffer& messageBuffer = m_sendWindow.messages[index];
                messageBuffer.header.windowSize = m_receiveWindow.currentAvailableSize;

                const Result sendResult = m_pMsgChannel->Forward(messageBuffer);
                if (sendResult == Result::Success)
                {
                    const uint64 currentTime = Platform::GetCurrentTimeInMs();
                    m_sendWindow.initialTransmitTimeInMs[index] = currentTime;
                    m_sendWindow.lastSentSequence = messageBuffer.header.sequence;
                    m_sendWindow.lastAvailableSize -= 1;
                }
                else
                {
                    if (sendResult != Result::NotReady)
                    {
                        Shutdown(Result::Error);
                    }
                    // packet dropped, abort transmitting
                    break;
                }
                seq++;
            }
            else
            {
                DD_ASSERT_REASON("Transmit window data corruption detected");
            }
        }
    }

    void Session::UpdateTimeout()
    {
        if (m_sessionState == SessionState::FinWait1)
        {
            if (WriteMessageIntoSendWindow(SessionMessage::Fin, 0, nullptr, kInfiniteTimeout) == Result::Success)
            {
                SetState(SessionState::FinWait2);
            }
        }

        if (m_sessionState == SessionState::FinWait2 && IsSendWindowEmpty())
        {
            SetState(SessionState::Closed);
        }

        if (m_sessionState == SessionState::Closing)
        {
            LockGuard<AtomicLock> lock(m_receiveWindow.lock);

            // if the only message we have left is the fin message, we are safe to transition to closed
            if (m_receiveWindow.nextUnreadSequence < m_receiveWindow.nextExpectedSequence)
            {
                const Sequence index = m_receiveWindow.nextUnreadSequence % m_receiveWindow.GetWindowSize();
                MessageBuffer& message = m_receiveWindow.messages[index];
                if (static_cast<SessionMessage>(message.header.messageId) == SessionMessage::Fin)
                {
                    SetState(SessionState::Closed);
                }
            }
        }
    }

    Result Session::Send(uint32 payloadSizeInBytes, const void* pPayload, uint32 timeoutInMs)
    {
        Result result = Result::Error;
        if (m_sessionState != SessionState::Closed)
        {
            result = WriteMessageIntoSendWindow(SessionMessage::Data, payloadSizeInBytes, pPayload, timeoutInMs);
        }
        return result;
    }

    Result Session::Receive(uint32 payloadSizeInBytes, void* pPayload, uint32* pBytesReceived, uint32 timeoutInMs)
    {
        Result result = Result::Error;

        if (m_sessionState >= SessionState::Established)
        {
            // Messages cannot be received after a session has entered the closing state.
            result = m_receiveWindow.semaphore.Wait(timeoutInMs);

            if (result == Result::Success)
            {
                LockGuard<AtomicLock> lock(m_receiveWindow.lock);
                DD_ASSERT(m_receiveWindow.nextUnreadSequence < m_receiveWindow.nextExpectedSequence);

                const Sequence index = m_receiveWindow.nextUnreadSequence % m_receiveWindow.GetWindowSize();
                MessageBuffer& message = m_receiveWindow.messages[index];

                if (payloadSizeInBytes >= message.header.payloadSize)
                {
                    if (static_cast<SessionMessage>(message.header.messageId) == SessionMessage::Data)
                    {
                        uint32 payloadSize = Platform::Min(message.header.payloadSize, payloadSizeInBytes);

                        DD_PRINT(LogLevel::Never, "Reading message number %u", m_receiveWindow.nextUnreadSequence);
                        DD_ASSERT(m_receiveWindow.valid[index] && m_receiveWindow.sequence[index] == m_receiveWindow.nextUnreadSequence);
                        memcpy(pPayload, &message.payload[0], payloadSize);
                        *pBytesReceived = payloadSize;
                    }
                    else
                    {
                        DD_ASSERT(static_cast<SessionMessage>(message.header.messageId) == SessionMessage::Fin);
                        DD_ASSERT(m_sessionState == SessionState::Closing);
                        SetState(SessionState::Closed);
                        result = Result::EndOfStream;
                    }
                    m_receiveWindow.valid[index] = false;
                    m_receiveWindow.nextUnreadSequence++;
                    m_receiveWindow.currentAvailableSize = CalculateCurrentWindowSize();
                }
                else
                {
                    // Re-signal the semaphore so a future read can read it.
                    m_receiveWindow.semaphore.Signal();
                    result = Result::InsufficientMemory;
                }
            }
        }
        return result;
    }

    void Session::Shutdown(Result reason)
    {
	    m_sessionTerminationReason = reason;

        switch (m_sessionState)
        {
        case SessionState::Closed:
        case SessionState::FinWait1:
        case SessionState::FinWait2:
        case SessionState::Closing:
            if (reason != Result::Success)
            {
                SetState(SessionState::Closed);
            }
            break;
        case SessionState::Established:
            if (reason == Result::Success)
            {
                // Send the request.
                SetState(SessionState::FinWait1);
            } else
            {
                SetState(SessionState::Closed);
            }
            break;
        default:
            SetState(SessionState::Closed);
            break;
        }
    }

    void Session::Close(Result reason)
    {
        Orphan();
        Shutdown(reason);
    }

    void Session::Orphan()
    {
        // WARNING - this can leak memory as this will lead to SessionTerminate not being called
        DD_ASSERT(m_pSessionUserdata == nullptr);
        m_pProtocolOwner = nullptr;
        m_pSessionUserdata = nullptr;
    }

    inline void DevDriver::Session::SetState(SessionState newState)
    {
        if (m_sessionState != newState)
        {
#if defined(DEVDRIVER_SHOW_SYMBOLS)
            DD_PRINT(LogLevel::Debug, "[Session] Session %u transitioned states: %s -> %s",
                     GetSessionId(), kStateName[static_cast<uint32>(m_sessionState)],
                     kStateName[static_cast<uint32>(newState)]);
#endif
            m_sessionState = newState;
        }
    }
} // DevDriver
