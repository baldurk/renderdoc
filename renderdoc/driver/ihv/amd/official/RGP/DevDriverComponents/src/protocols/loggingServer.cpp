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

#include "protocols/loggingServer.h"
#include "msgChannel.h"
#include <cstring>
#include <cstdio>

#define LOGGING_SERVER_MIN_MAJOR_VERSION 2
#define LOGGING_SERVER_MAX_MAJOR_VERSION 3

namespace DevDriver
{
    namespace LoggingProtocol
    {
        static const NamedLoggingCategory kDefaultLoggingCategories[kReservedCategoryCount] = {
            {kGeneralCategoryMask, "General"},
            {kSystemCategoryMask, "System"},
        };
        static_assert(kGeneralCategoryOffset == 0, "General category offset has changed unexpectedly");
        static_assert(kSystemCategoryOffset == 1, "System category offset has changed unexpectedly");

        LoggingServer::LoggingServer(IMsgChannel* pMsgChannel)
            : BaseProtocolServer(pMsgChannel, Protocol::Logging, LOGGING_SERVER_MIN_MAJOR_VERSION, LOGGING_SERVER_MAX_MAJOR_VERSION)
            , m_categories()
            , m_activeSessions(pMsgChannel->GetAllocCb())
            , m_numCategories(0)
        {
            DD_ASSERT(m_pMsgChannel != nullptr);

            // Initialize the category table
            memset(&m_categories[0], 0, sizeof(m_categories));

            // Initialize default logging categories
            for (uint32 i = 0; i < kReservedCategoryCount; i++)
            {
                // Only initialize valid categorie entries
                if (kDefaultLoggingCategories[i].category != 0 && kDefaultLoggingCategories[i].name[0] != 0)
                {
                    // Validate that there hasn't been a mistake made somewhere
                    DD_ASSERT(kDefaultLoggingCategories[i].category ==
                        ((LoggingCategory)1 << (kDefinableCategoryCount + i)));

                    // Copy the category definition into our table and increment count
                    m_categories[kDefinableCategoryCount + i] = kDefaultLoggingCategories[i];
                    m_numCategories++;
                }
            }
        }

        LoggingServer::~LoggingServer()
        {
        }

        bool LoggingServer::AcceptSession(const SharedPointer<ISession>& pSession)
        {
            DD_UNUSED(pSession);
            return true;
        }

        void LoggingServer::SessionEstablished(const SharedPointer<ISession>& pSession)
        {
            DD_UNUSED(pSession);
            // Allocate session data for the newly established session
            LoggingSession *pSessionData = DD_NEW(LoggingSession, m_pMsgChannel->GetAllocCb())(m_pMsgChannel->GetAllocCb(), pSession);
            pSessionData->state = SessionState::ReceivePayload;
            pSessionData->loggingEnabled = false;
            memset(&pSessionData->scratchPayload, 0, sizeof(pSessionData->scratchPayload));
            // Default to all messages enabled.

            pSessionData->filter.priority = LogLevel::Error;
            pSessionData->filter.category = kAllLoggingCategories;

            LockData();

            m_activeSessions.PushBack(pSessionData);

            UnlockData();

            pSession->SetUserData(pSessionData);
        }

        void LoggingServer::UpdateSession(const SharedPointer<ISession>& pSession)
        {
            LoggingSession *pSessionData = reinterpret_cast<LoggingSession*>(pSession->GetUserData());

            switch (pSessionData->state)
            {
                case SessionState::ReceivePayload:
                {
                    const Result result = pSession->ReceivePayload(&pSessionData->scratchPayload, kNoWait);

                    if (result == Result::Success)
                    {
                        pSessionData->state = SessionState::ProcessPayload;
                    }
                    else if (result == Result::NotReady)
                    {
                        // If there's no messages to receive, we should send log messages if logging is enabled.
                        if (pSessionData->loggingEnabled)
                        {
                            LockData();

                            // Send as many log messages from our queue as possible.

                            while (pSessionData->messages.PeekFront() != nullptr)
                            {
                                const SizedPayloadContainer* pPayload = pSessionData->messages.PeekFront();
                                if (pSessionData->SendPayload(pPayload, kNoWait) == Result::Success)
                                {
                                    DD_PRINT(LogLevel::Debug, "Sent Logging Payload To Session %d!", pSession->GetSessionId());
                                    // Pop the message off the queue since it was successfully sent.
                                    pSessionData->messages.PopFront();
                                }
                                else
                                {
                                    break;
                                }
                            }

                            UnlockData();
                        }
                    }

                    break;
                }

                case SessionState::ProcessPayload:
                {
                    SizedPayloadContainer& container = pSessionData->scratchPayload;
                    switch (container.GetPayload<LoggingHeader>().command)
                    {
                        case LoggingMessage::QueryCategoriesRequest:
                        {
                            LockData();
                            const uint32 numCategories = (m_numCategories <= kMaxCategoryCount) ? m_numCategories : 0;
                            UnlockData();

                            container.CreatePayload<QueryCategoriesNumResponsePayload>(numCategories);
                            pSessionData->state = SessionState::SendCategoriesNumResponse;

                            break;
                        }

                        case LoggingMessage::EnableLoggingRequest:
                        {
                            DD_PRINT(LogLevel::Debug, "Starting Logging!");
                            LockData();
                            pSessionData->filter         = container.GetPayload<EnableLoggingRequestPayload>().filter;
                            pSessionData->loggingEnabled = true;
                            UnlockData();

                            container.CreatePayload<EnableLoggingResponsePayload>(Result::Success);
                            pSessionData->state = SessionState::SendPayload;
                            break;
                        }

                        case LoggingMessage::DisableLogging:
                        {
                            DD_PRINT(LogLevel::Debug, "Stopping Logging!");
                            LockData();

                            pSessionData->loggingEnabled = false;
                            pSessionData->state = SessionState::FinishLogging;

                            // We have no additional messages to send so let the client know via the sentinel.
                            SizedPayloadContainer* pPayload = pSessionData->messages.AllocateBack();
                            if (pPayload != nullptr)
                            {
                                pPayload->CreatePayload<LoggingHeader>(LoggingMessage::LogMessageSentinel);
                            }
                            DD_PRINT(LogLevel::Debug, "Inserted logging sentinel");

                            UnlockData();

                            break;
                        }

                        default:
                        {
                            DD_UNREACHABLE();
                            break;
                        }
                    }

                    break;
                }

                case SessionState::FinishLogging:
                {
                    DD_PRINT(LogLevel::Debug, "Finishing Logging!");
                    LockData();

                    // Send as many log messages from our queue as possible.
                    if (pSessionData->messages.Size() > 0)
                    {
                        DD_PRINT(LogLevel::Debug, "Logging messages remaining: %u", pSessionData->messages.Size());
                        const SizedPayloadContainer* pPayload = pSessionData->messages.PeekFront();
                        while (pPayload != nullptr)
                        {
                            if (pSessionData->SendPayload(pPayload, kNoWait) == Result::Success)
                            {
                                // Pop the message off the queue since it was successfully sent.
                                pSessionData->messages.PopFront();

                                // Peek at the next message.
                                pPayload = pSessionData->messages.PeekFront();
                            }
                            else
                            {
                                break;
                            }
                        }
                    }
                    else
                    {
                        pSessionData->state = SessionState::ReceivePayload;
                    }

                    UnlockData();

                    break;
                }

                case SessionState::SendPayload:
                {
                    Result result = pSessionData->SendPayload(&pSessionData->scratchPayload, kNoWait);
                    if (result == Result::Success)
                    {
                        pSessionData->state = SessionState::ReceivePayload;
                    }

                    break;
                }

                case SessionState::SendCategoriesNumResponse:
                {
                    Result result = pSessionData->SendPayload(&pSessionData->scratchPayload, kNoWait);
                    if (result == Result::Success)
                    {
                        const QueryCategoriesNumResponsePayload* pQueryCategoriesResponse =
                            reinterpret_cast<QueryCategoriesNumResponsePayload*>(pSessionData->scratchPayload.payload);
                        if (pQueryCategoriesResponse->numCategories > 0)
                        {
                            pSessionData->itemIndex = 0;
                            pSessionData->numItems  = 0;
                            pSessionData->state     = SessionState::SendCategoriesDataResponse;

                            // Prepare the payload for the first data response
                            if (pSessionData->numItems < m_numCategories)
                            {
                                // If m_numCategories were to get modified and pSessionData->itemIndex is already past
                                // the insertion point this can potentially cause an access violation by iterating past
                                // the array boundary. we prevent this by not allowing modification of this table while
                                // there are clients connected, but this probably needs to be addressed at some point.
                                while (m_categories[pSessionData->itemIndex].category == 0)
                                {
                                    // Find first valid category
                                    pSessionData->itemIndex++;
                                }

                                const Version sessionVersion = pSession->GetVersion();
                                LockData();
                                const NamedLoggingCategory& category = m_categories[pSessionData->itemIndex];
                                const size_t categoryNameSize = strlen(category.name) + 1;
                                QueryCategoriesDataResponsePayload::WritePayload(m_categories[pSessionData->itemIndex],
                                                                                 sessionVersion,
                                                                                 categoryNameSize,
                                                                                 &pSessionData->scratchPayload);
                                UnlockData();
                            }
                        }
                        else
                        {
                            pSessionData->state = SessionState::ReceivePayload;
                        }
                    }
                    break;
                }

                case SessionState::SendCategoriesDataResponse:
                {
                    if (pSessionData->numItems < m_numCategories)
                    {
                        while (pSessionData->SendPayload(&pSessionData->scratchPayload, kNoWait) == Result::Success)
                        {
                            pSessionData->numItems++;
                            // Prepare the payload for the next data response if necessary
                            if (pSessionData->numItems < m_numCategories)
                            {
                                // Seek to the next valid category in the table.
                                // TODO: there is a potential issue here where if the number of categories increases
                                // but we've already passed the insertion point this will cause an access violation by
                                // iterating off the array. we prevent this by not allowing modification of this table while
                                // there are clients connected, but this probably needs to be addressed at some point.
                                while (m_categories[++pSessionData->itemIndex].category == 0)
                                {
                                }

                                const Version sessionVersion = pSession->GetVersion();
                                LockData();
                                const NamedLoggingCategory& category = m_categories[pSessionData->itemIndex];
                                const size_t categoryNameSize = strlen(category.name) + 1;
                                QueryCategoriesDataResponsePayload::WritePayload(m_categories[pSessionData->itemIndex],
                                                                                 sessionVersion,
                                                                                 categoryNameSize,
                                                                                 &pSessionData->scratchPayload);
                                UnlockData();
                            }
                            else
                            {
                                // Break out of the send loop if we've finished sending all of the responses
                                break;
                            }
                        }
                    }
                    else
                    {
                        // We've sent all the responses. Return to normal operation.
                        pSessionData->state = SessionState::ReceivePayload;
                    }

                    break;
                }

                default:
                {
                    DD_UNREACHABLE();
                    break;
                }
            }
        }

        void LoggingServer::SessionTerminated(const SharedPointer<ISession>& pSession, Result terminationReason)
        {
            DD_UNUSED(terminationReason);
            LoggingSession *pLoggingSession = reinterpret_cast<LoggingSession*>(pSession->SetUserData(nullptr));

            // Free the session data
            if (pLoggingSession != nullptr)
            {
                LockData();

                m_activeSessions.Remove(pLoggingSession);

                UnlockData();

                DD_DELETE(pLoggingSession, m_pMsgChannel->GetAllocCb());
            }
        }

        Result LoggingServer::AddCategoryTable(uint32 offset, uint32 count, const char **pCategoryTable)
        {
            Result result = Result::Error;
            LockData();
            // Only allow modification if no sessions are connected.
            // This is explicitly to prevent an issue where the number of categories changes while the server is
            // trying to respond to QueryCategoriesRequest.
            if (m_activeSessions.Size() == 0)
            {
                // Ensure that the offset is valid and that the count is nonzero
                if ((offset < kDefinableCategoryCount) & (count > 0))
                {
                    bool available = true;

                    // Ee need to make sure each index is valid and unused
                    for (uint32 index = 0; index < count; index++)
                    {
                        size_t catIndex = (index + offset);

                        // If either of these cases are true we abort:
                        // 1) the index is greater than the maximum definable allowed
                        // 2) the category table defines a string and it is already taken
                        // note: this allows null pointers to be ignored in the category table
                        if ((catIndex >= kDefinableCategoryCount) ||
                            ((pCategoryTable[index] != nullptr) &
                            (m_categories[catIndex].category != 0)))
                        {
                            available = false;
                            break;
                        }
                    }

                    // If no errors were found
                    if (available)
                    {
                        for (uint32 index = 0; index < count; index++)
                        {
                            // Only do this if the entry in the category table is not null. Allows us to skip entries
                            // by defining null pointers inside the table
                            if (pCategoryTable[index] != nullptr)
                            {
                                const uint32 catIndex = index + offset;
                                const LoggingCategory mask = ((LoggingCategory)1 << catIndex) & kDefinableCategoryMask;
                                if (catIndex < kDefinableCategoryCount && mask != 0)
                                {
                                    // Copy the category name into the local category table and calculate the bitmask
                                    Platform::Strncpy(m_categories[catIndex].name,
                                                      pCategoryTable[index],
                                                      sizeof(NamedLoggingCategory::name));

                                    m_categories[catIndex].category = mask;
                                    m_numCategories++;
                                }
                                else
                                {
                                    DD_UNREACHABLE();
                                }
                            }
                        }
                        result = Result::Success;
                    }
                }
            }
            UnlockData();
            return result;
        }

        void LoggingServer::Log(LogLevel priority, LoggingCategory category, const char* pFormat, va_list args)
        {
            // todo: figure out a way to make this threadsafe. right now Log() will cause serialization
            // of separate threads
            LockData();

            // We only need to do work if there are active sessions to send messages to.
            if (m_activeSessions.Size() > 0)
            {
                // define the filter out here so we just copy it into destination messages
                LogMessage message = {};
                message.filter.priority = priority;
                message.filter.category = category;
                Platform::Vsnprintf(message.message,
                                    sizeof(LogMessage::message),
                                    pFormat,
                                    args);
                // Calculate the message size (including the null terminator).
                const size_t messageSize = strlen(message.message) + 1;

                for (auto& pSessionData : m_activeSessions)
                {
                    const LoggingFilter& currentFilter = pSessionData->filter;
                    const bool sendMessage = (currentFilter.priority <= priority) &
                                             ((currentFilter.category & category) != 0);

                    // if the session has logging enabled and the message satisfies the filter of the session
                    if ((pSessionData->loggingEnabled) & sendMessage)
                    {
                        const Version sessionVersion = pSessionData->pSession->GetVersion();

                        // Allocate a message and copy the message into the buffer of all sessions
                        SizedPayloadContainer* pPayloadContainer = pSessionData->messages.AllocateBack();
                        if (pPayloadContainer != nullptr)
                        {
                            LogMessagePayload::WritePayload(message,
                                                            sessionVersion,
                                                            messageSize,
                                                            pPayloadContainer);
                        }
                    }
                }
            }

            UnlockData();
        }

        void LoggingServer::LockData()
        {
            m_mutex.Lock();
        }

        void LoggingServer::UnlockData()
        {
            m_mutex.Unlock();
        }

        Result LoggingSession::SendPayload(const SizedPayloadContainer* pPayload, uint32 timeoutInMs)
        {
            // If we're running an older logging version, always write the fixed payload size. Otherwise, write the real size.
            const uint32 payloadSize =
                (pSession->GetVersion() >= LOGGING_LARGE_MESSAGES_VERSION) ? pPayload->payloadSize : kLegacyLoggingPayloadSize;

            return pSession->Send(payloadSize, pPayload->payload, timeoutInMs);
        }
    }
} // DevDriver
