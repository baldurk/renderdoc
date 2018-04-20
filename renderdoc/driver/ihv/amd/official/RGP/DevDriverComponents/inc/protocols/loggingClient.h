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

#pragma once

#include "baseProtocolClient.h"
#include "loggingProtocol.h"
#include "util/queue.h"
#include "util/vector.h"

namespace DevDriver
{
    class IMsgChannel;

    namespace LoggingProtocol
    {
        enum class LoggingClientState : uint32
        {
            Idle = 0,
            Logging,
            LoggingFinished
        };

        class LoggingClient : public BaseProtocolClient
        {
        public:
            explicit LoggingClient(IMsgChannel* pMsgChannel);
            ~LoggingClient();

            // Overrides the base client's update session functionality
            void UpdateSession(const SharedPointer<ISession>& pSession) override final;

            // Overrides the base client's session termination callback to ensure state gets reset
            void SessionTerminated(const SharedPointer<ISession>& pSession, Result terminationReason) override final;

            // Sends an enable logging message to the UMD
            Result EnableLogging(LogLevel priority, LoggingCategory categoryMask);

            // Sends a disable logging message to the UMD
            Result DisableLogging();

            // Queries available categories and populates the provided vector
            Result QueryCategories(Vector<NamedLoggingCategory> &categories);

            // Reads the log messages stored on the remote server into the provided vector
            Result ReadLogMessages(Vector<LogMessage> &logMessages);

            // Returns true if the client currently contains log messages that have not been received.
            bool HasLogMessages();

        private:
            void ResetState() override;

            bool IsIdle() const;
            bool IsLogging() const;

            Result SendLoggingPayload(const SizedPayloadContainer& container,
                                      uint32                         timeoutInMs = kDefaultCommunicationTimeoutInMs,
                                      uint32                         retryInMs   = kDefaultRetryTimeoutInMs);
            Result ReceiveLoggingPayload(SizedPayloadContainer* pContainer,
                                         uint32                   timeoutInMs = kDefaultCommunicationTimeoutInMs,
                                         uint32                   retryInMs   = kDefaultRetryTimeoutInMs);
            Result TransactLoggingPayload(SizedPayloadContainer* pContainer,
                                         uint32                    timeoutInMs = kDefaultCommunicationTimeoutInMs,
                                         uint32                    retryInMs   = kDefaultRetryTimeoutInMs);

            LoggingClientState m_loggingState;
            Queue<SizedPayloadContainer, 32> m_logMessages;
            Platform::Mutex m_mutex;
            Platform::Event m_loggingFinishedEvent;
        };
    }
}
