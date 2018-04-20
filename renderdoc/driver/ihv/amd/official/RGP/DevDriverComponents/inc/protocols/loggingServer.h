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
* @file  loggingServer.h
* @brief Class declaration for LoggingServer.
***********************************************************************************************************************
*/

#pragma once
#include "baseProtocolServer.h"
#include "loggingProtocol.h"
#include "util/vector.h"
#include "util/queue.h"
#include "ddPlatform.h"

#ifdef DEVDRIVER_LOG_CATEGORY_MASK
#define LOGGING_CATEGORY_MASK static_cast<DevDriver::LoggingProtocol::LoggingCategory>(DEVDRIVER_LOG_CATEGORY_MASK)
#else
#if defined(NDEBUG)
#define LOGGING_CATEGORY_MASK DevDriver::LoggingProtocol::kAllLoggingCategories
#else
#define LOGGING_CATEGORY_MASK DevDriver::LoggingProtocol::kAllLoggingCategories
#endif
#endif

namespace DevDriver
{
    namespace LoggingProtocol
    {
        enum class SessionState
        {
            ReceivePayload = 0,
            ProcessPayload,
            SendPayload,
            SendCategoriesNumResponse,
            SendCategoriesDataResponse,
            FinishLogging
        };

        struct LoggingSession
        {
            SizedPayloadContainer scratchPayload;
            SharedPointer<ISession> pSession;
            Queue<SizedPayloadContainer, 32> messages;
            uint32 itemIndex;
            uint32 numItems;
            LoggingFilter filter;
            SessionState state;
            bool loggingEnabled;

            explicit LoggingSession(const AllocCb& allocCb, const SharedPointer<ISession>& pSession)
                : scratchPayload()
                , pSession(pSession)
                , messages(allocCb)
                , itemIndex(0)
                , numItems(0)
                , filter()
                , state(SessionState::ReceivePayload)
                , loggingEnabled(false)
            {
                memset(&scratchPayload, 0, sizeof(scratchPayload));
                // Default to all messages enabled.

                filter.priority = LogLevel::Error;
                filter.category = kAllLoggingCategories;
            }

            // Helper functions for working with SizedPayloadContainers and managing back-compat.
            Result SendPayload(const SizedPayloadContainer* pPayload, uint32 timeoutInMs);
        };

        class LoggingServer : public BaseProtocolServer
        {
        public:
            explicit LoggingServer(IMsgChannel* pMsgChannel);
            ~LoggingServer();

            bool AcceptSession(const SharedPointer<ISession>& pSession) override;
            void SessionEstablished(const SharedPointer<ISession>& pSession) override;
            void UpdateSession(const SharedPointer<ISession>& pSession) override;
            void SessionTerminated(const SharedPointer<ISession>& pSession, Result terminationReason) override;

            Result AddCategoryTable(uint32 offset, uint32 count, const char **pCategoryTable);

            void Log(LogLevel priority, LoggingCategory category, const char* pFormat, va_list args);
            void Log(LogLevel priority, LoggingCategory category, const char* pFormat, ...)
            {
                va_list args;
                va_start(args, pFormat);
                Log(priority, category, pFormat, args);
                va_end(args);
            }

        private:
            void LockData();
            void UnlockData();

            NamedLoggingCategory m_categories[kMaxCategoryCount];
            Vector<LoggingSession*, 8> m_activeSessions;
            Platform::Mutex m_mutex;
            uint32 m_numCategories;
        };
    }
}
