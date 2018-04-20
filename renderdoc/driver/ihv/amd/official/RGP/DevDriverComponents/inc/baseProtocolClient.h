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
* @file  baseProtocolClient.h
* @brief Class declaration for BaseProtocolClient.
***********************************************************************************************************************
*/

#pragma once

#include "protocolClient.h"

namespace DevDriver
{
    class IMsgChannel;

    enum class ClientState : uint32
    {
        Disconnected = 0,
        Connecting,
        Connected
    };

    class BaseProtocolClient : public IProtocolClient
    {
    public:
        virtual ~BaseProtocolClient();

        // query constant properties of the protocol client
        Protocol GetProtocol() const override final { return m_protocol; };
        SessionType GetType() const override final { return SessionType::Client; };
        Version GetMinVersion() const override final { return m_minVersion; };
        Version GetMaxVersion() const override final { return m_maxVersion; };

        // connection management/tracking
        Result Connect(ClientId clientId) override final;
        void Disconnect() override final;
        bool IsConnected() const override final;

#if !DD_VERSION_SUPPORTS(GPUOPEN_SESSION_INTERFACE_CLEANUP_VERSION)
        // Orphans the current session associated with the client object and moves to the disconnected state
        void Orphan() override final;
#endif

        // properties that are only valid in a connected session
        ClientId GetRemoteClientId() const override final;
        Version GetSessionVersion() const override final;

        // asychronous callbacks used by the SessionManager - should never be called directly
        virtual void SessionEstablished(const SharedPointer<ISession>& pSession) override;
        virtual void UpdateSession(const SharedPointer<ISession>& pSession) override;
        virtual void SessionTerminated(const SharedPointer<ISession>& pSession, Result terminationReason) override;

    protected:
        BaseProtocolClient(IMsgChannel* pMsgChannel, Protocol protocol, Version minVersion, Version maxVersion);

        // Default implementation of ResetState that does nothing
        virtual void ResetState() {};

        IMsgChannel* const m_pMsgChannel;
        const Protocol m_protocol;
        const Version m_minVersion;
        const Version m_maxVersion;

        SharedPointer<ISession> m_pSession;

        DD_STATIC_CONST uint32 kDefaultRetryTimeoutInMs = 50;
        DD_STATIC_CONST uint32 kDefaultCommunicationTimeoutInMs = 5000;

        // Attempts to receive a payload into a fixed size buffer.
        // Returns the result and the size of the payload if it was received successfully.
        Result ReceiveSizedPayload(void*   pPayloadBuffer,
                                   uint32  payloadBufferSize,
                                   uint32* pBytesReceived,
                                   uint32  timeoutInMs = kDefaultCommunicationTimeoutInMs,
                                   uint32  retryInMs   = kDefaultRetryTimeoutInMs)
        {
            uint32 timeElapsed = 0;
            // Blocking wait on the message.
            Result result = Result::Error;

            SharedPointer<ISession> pSession = m_pSession;
            if (!pSession.IsNull())
            {
                do
                {
                    result = pSession->Receive(payloadBufferSize, pPayloadBuffer, pBytesReceived, retryInMs);
                    timeElapsed += retryInMs;
                }
                while ((result == Result::NotReady) & (timeElapsed <= timeoutInMs));
            }

            return result;
        }

        // Templated wrapper around ReceiveSizedPayload
        template <typename T>
        Result ReceivePayload(T&     payload,
                              uint32 timeoutInMs = kDefaultCommunicationTimeoutInMs,
                              uint32 retryInMs   = kDefaultRetryTimeoutInMs)
        {
            uint32 bytesReceived = 0;
            Result result = ReceiveSizedPayload(&payload, sizeof(T), &bytesReceived, timeoutInMs, retryInMs);

            // Return an error if we don't get back the size we were expecting.
            if ((result == Result::Success) & (bytesReceived != sizeof(T)))
            {
                result = Result::Error;
            }

            return result;
        }

        // Attempts to send a payload
        Result SendSizedPayload(const void* pPayload,
                                uint32      payloadSize,
                                uint32      timeoutInMs = kDefaultCommunicationTimeoutInMs,
                                uint32      retryInMs   = kDefaultRetryTimeoutInMs)
        {
            Result result = Result::Error;
            uint32 timeElapsed = 0;
            SharedPointer<ISession> pSession = m_pSession;
            if (!pSession.IsNull())
            {
                do
                {
                    result = pSession->Send(payloadSize, pPayload, retryInMs);
                    timeElapsed += retryInMs;
                } while ((result == Result::NotReady) & (timeElapsed <= timeoutInMs));
            }
            return result;
        }

        // Templated wrapper around SendSizedPayload
        template <typename T>
        Result SendPayload(T&     payload,
                           uint32 timeoutInMs = kDefaultCommunicationTimeoutInMs,
                           uint32 retryInMs   = kDefaultRetryTimeoutInMs)
        {
            return SendSizedPayload(&payload, sizeof(T), timeoutInMs, retryInMs);
        }

        // Attempts to perform a payload "transaction" which is defined as sending one payload out, and receiving one in response.
        Result TransactSized(const void* pSendPayload,
                             uint32      sendSize,
                             void*       pReceivePayload,
                             uint32      receiveSize,
                             uint32*     pBytesReceived,
                             uint32      timeoutInMs = kDefaultCommunicationTimeoutInMs,
                             uint32      retryInMs   = kDefaultRetryTimeoutInMs)
        {
            Result result = Result::Error;
            if (IsConnected())
            {
                result = SendSizedPayload(pSendPayload, sendSize, timeoutInMs, retryInMs);
                if (result == Result::Success)
                {
                    result = ReceiveSizedPayload(pReceivePayload, receiveSize, pBytesReceived, timeoutInMs, retryInMs);
                }
            }
            return result;
        }

        // Templated wrapper around TransactSized
        template <typename T, typename U>
        Result Transact(const T& sendPayload,
                        U&       receivePayload,
                        uint32   timeoutInMs = kDefaultCommunicationTimeoutInMs,
                        uint32   retryInMs   = kDefaultRetryTimeoutInMs)
        {
            Result result = Result::Error;
            if (IsConnected())
            {
                result = SendPayload(sendPayload, timeoutInMs, retryInMs);
                if (result == Result::Success)
                {
                    result = ReceivePayload(receivePayload, timeoutInMs, retryInMs);
                }
            }
            return result;
        }

    private:
        Platform::Event m_pendingOperationEvent;
        Result m_connectResult;
        ClientState m_state;
    };

} // DevDriver
