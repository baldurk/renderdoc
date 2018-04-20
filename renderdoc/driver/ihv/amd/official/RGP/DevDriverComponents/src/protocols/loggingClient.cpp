/*
 *******************************************************************************
 *
 * Copyright (c) 2018 Advanced Micro Devices, Inc. All rights reserved.
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

#include "protocols/loggingClient.h"
#include "msgChannel.h"

#define LOGGING_CLIENT_MIN_MAJOR_VERSION 2
#define LOGGING_CLIENT_MAX_MAJOR_VERSION 3

namespace DevDriver
{
    namespace LoggingProtocol
    {
        LoggingClient::LoggingClient(IMsgChannel* pMsgChannel)
            : BaseProtocolClient(pMsgChannel, Protocol::Logging, LOGGING_CLIENT_MIN_MAJOR_VERSION, LOGGING_CLIENT_MAX_MAJOR_VERSION)
            , m_loggingState(LoggingClientState::Idle)
            , m_logMessages(pMsgChannel->GetAllocCb())
            , m_loggingFinishedEvent(true)
        {
        }

        LoggingClient::~LoggingClient()
        {
        }

        void LoggingClient::UpdateSession(const SharedPointer<ISession>& pSession)
        {
            BaseProtocolClient::UpdateSession(pSession);

            if (IsLogging())
            {
                // Receive all logging messages.
                SizedPayloadContainer container = {};

                Platform::LockGuard<Platform::Mutex> lock(m_mutex);

                // Attempt to receive messages until we either get all of them, or encounter an error.
                while (IsLogging() && (ReceiveLoggingPayload(&container, kNoWait) == Result::Success))
                {
                    const LoggingHeader* pHeader = reinterpret_cast<LoggingHeader*>(container.payload);
                    if (pHeader->command == LoggingMessage::LogMessage)
                    {
                        DD_PRINT(LogLevel::Debug, "Received Logging Payload From Session %d!", pSession->GetSessionId());
                        m_logMessages.PushBack(container);
                    }
                    else if (pHeader->command == LoggingMessage::LogMessageSentinel)
                    {
                        DD_PRINT(LogLevel::Debug, "Received Logging Sentinel From Session %d!", pSession->GetSessionId());

                        // Update our state since we've received all log messages.
                        m_loggingState = LoggingClientState::LoggingFinished;

                        // Trigger the logging finished event once we get the sentinel.
                        // This allows the DisableLogging function to complete.
                        m_loggingFinishedEvent.Signal();

                        break;
                    }
                    else
                    {
                        // This should never happen. This means this is an unexpected packet type.
                        DD_UNREACHABLE();
                    }
                }
            }
        }

        void LoggingClient::SessionTerminated(const SharedPointer<ISession>& pSession, Result terminationReason)
        {
            BaseProtocolClient::SessionTerminated(pSession, terminationReason);
            // ResetState() dumps all pending log messages. If the server disconnects and the session dies, any
            // unread log messages will be lost.
            // @todo: Separate log messages from internal state and allow preservation past session termination.
            ResetState();
        }

        Result LoggingClient::EnableLogging(LogLevel priority, LoggingCategory categoryMask)
        {
            Result result = Result::Error;

            if (IsConnected() && IsIdle())
            {
                LoggingFilter filter = {};
                filter.priority = priority;
                filter.category = categoryMask;

                SizedPayloadContainer container = {};
                container.CreatePayload<EnableLoggingRequestPayload>(filter);

                result = TransactLoggingPayload(&container);
                if (result == Result::Success)
                {
                    const EnableLoggingResponsePayload& response = container.GetPayload<EnableLoggingResponsePayload>();
                    if (response.header.command == LoggingProtocol::LoggingMessage::EnableLoggingResponse)
                    {
                        result = response.result;

                        if (result == Result::Success)
                        {
                            m_loggingState = LoggingClientState::Logging;

                            // Reset the logging finished event since we're starting a new logging session.
                            m_loggingFinishedEvent.Clear();
                        }
                    }
                    else
                    {
                        // Invalid response payload
                        result = Result::Error;
                    }
                }
            }

            return result;
        }

        Result LoggingClient::DisableLogging()
        {
            Result result = Result::Error;
            if (IsConnected() && IsLogging())
            {
                SizedPayloadContainer container = {};
                LoggingHeader* pHeader = reinterpret_cast<LoggingHeader*>(container.payload);
                pHeader->command = LoggingMessage::DisableLogging;
                container.payloadSize = sizeof(LoggingHeader);

                if (SendLoggingPayload(container) == Result::Success)
                {
                    // Wait until the session update thread receives the logging sentinel before we continue.
                    if (m_loggingFinishedEvent.Wait(kInfiniteTimeout) == Result::Success)
                    {
                        // If we've successfully finished logging, then set the result to success.
                        // (We may end up here in the Idle state if the session is disconnected during logging)
                        if (m_loggingState == LoggingClientState::LoggingFinished)
                        {
                            // Set the state back to idle.
                            m_loggingState = LoggingClientState::Idle;

                            result = Result::Success;
                        }
                        else
                        {
                            // The ResetState function should always put us in the Idle state in the event of a session disconnect.
                            DD_ASSERT(m_loggingState == LoggingClientState::Idle);
                        }
                    }
                    else
                    {
                        // We should always successfully wait.
                        DD_UNREACHABLE();
                    }
                }
            }

            return result;
        }

        Result LoggingClient::QueryCategories(Vector<NamedLoggingCategory>& categories)
        {
            Result result = Result::Error;

            if (IsConnected() && IsIdle())
            {
                SizedPayloadContainer container = {};
                LoggingHeader* pHeader = reinterpret_cast<LoggingHeader*>(container.payload);
                pHeader->command = LoggingMessage::QueryCategoriesRequest;
                container.payloadSize = sizeof(LoggingHeader);

                result = TransactLoggingPayload(&container);
                if (result == Result::Success)
                {
                    if (pHeader->command == LoggingMessage::QueryCategoriesNumResponse)
                    {
                        const QueryCategoriesNumResponsePayload* pNumResponse =
                            reinterpret_cast<QueryCategoriesNumResponsePayload*>(container.payload);
                        const uint32 categoriesSent = pNumResponse->numCategories;

                        // TODO: actually validate this instead of just asserting
                        DD_ASSERT(categoriesSent < kMaxCategoryCount);

                        for (uint32 categoryIndex = 0; categoryIndex < categoriesSent; ++categoryIndex)
                        {
                            result = ReceiveLoggingPayload(&container);
                            if (result == Result::Success)
                            {
                                if (pHeader->command == LoggingMessage::QueryCategoriesDataResponse)
                                {
                                    const QueryCategoriesDataResponsePayload* pDataResponse =
                                        reinterpret_cast<QueryCategoriesDataResponsePayload*>(container.payload);
                                    categories.PushBack(pDataResponse->category);
                                }
                                else
                                {
                                    // Invalid response payload
                                    result = Result::Error;
                                    break;
                                }
                            }
                            else
                            {
                                break;
                            }
                        }
                    }
                    else
                    {
                        // Invalid response payload
                        result = Result::Error;
                    }
                }
            }

            return result;
        }

        Result LoggingClient::ReadLogMessages(Vector<LogMessage>& logMessages)
        {
            Result result = (IsConnected() && IsLogging()) ? Result::NotReady : Result::Error;
            Platform::LockGuard<Platform::Mutex> lock(m_mutex);

            while (m_logMessages.IsEmpty() == false)
            {
                const LogMessagePayload* pPayload = reinterpret_cast<const LogMessagePayload*>(m_logMessages.PeekFront()->payload);
                logMessages.PushBack(Platform::Move(pPayload->message));
                m_logMessages.PopFront();
            }
            if (logMessages.Size() > 0)
            {
                result = Result::Success;
            }

            return result;
        }

        bool LoggingClient::HasLogMessages()
        {
            Platform::LockGuard<Platform::Mutex> lock(m_mutex);
            return (m_logMessages.IsEmpty() == false);
        }

        void LoggingClient::ResetState()
        {
            Platform::LockGuard<Platform::Mutex> lock(m_mutex);
            m_loggingState = LoggingClientState::Idle;
            m_logMessages.Clear();
            m_loggingFinishedEvent.Signal();
        }

        bool LoggingClient::IsIdle() const
        {
            return (m_loggingState == LoggingClientState::Idle);
        }

        bool LoggingClient::IsLogging() const
        {
            return (m_loggingState == LoggingClientState::Logging);
        }

        Result LoggingClient::SendLoggingPayload(
            const SizedPayloadContainer& container,
            uint32 timeoutInMs,
            uint32 retryInMs)
        {
            // Use the legacy size for the payload if we're connected to an older client, otherwise use the real size.
            const Version sessionVersion = (m_pSession.IsNull() == false) ? m_pSession->GetVersion() : 0;
            const uint32 payloadSize = (sessionVersion >= LOGGING_LARGE_MESSAGES_VERSION) ? container.payloadSize : kLegacyLoggingPayloadSize;

            return SendSizedPayload(container.payload, payloadSize, timeoutInMs, retryInMs);
        }

        Result LoggingClient::ReceiveLoggingPayload(
            SizedPayloadContainer* pContainer,
            uint32 timeoutInMs,
            uint32 retryInMs)
        {
            return ReceiveSizedPayload(pContainer->payload, sizeof(pContainer->payload), &pContainer->payloadSize, timeoutInMs, retryInMs);
        }

        Result LoggingClient::TransactLoggingPayload(
            SizedPayloadContainer* pContainer,
            uint32 timeoutInMs,
            uint32 retryInMs)
        {
            Result result = SendLoggingPayload(*pContainer, timeoutInMs, retryInMs);
            if (result == Result::Success)
            {
                result = ReceiveLoggingPayload(pContainer, timeoutInMs, retryInMs);
            }

            return result;
        }
    }
}
