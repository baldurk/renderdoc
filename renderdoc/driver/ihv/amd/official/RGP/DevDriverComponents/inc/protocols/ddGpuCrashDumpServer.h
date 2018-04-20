/*
 *******************************************************************************
 *
 * Copyright (c) 2017-2018 Advanced Micro Devices, Inc. All rights reserved.
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
* @file  ddGpuCrashDumpServer.h
* @brief Class declaration for GpuCrashDumpServer.
***********************************************************************************************************************
*/

#pragma once

#include "baseProtocolServer.h"
#include "ddGpuCrashDumpProtocol.h"
#include "util/vector.h"
#include "util/sharedptr.h"

namespace DevDriver
{
    namespace GpuCrashDumpProtocol
    {
        // Abstract interface for handling crash notifications
        class ICrashDumpHandler
        {
        public:
            virtual ~ICrashDumpHandler() {}

            // Accepts or rejects a crash dump notification from an external client.
            virtual bool AcceptCrashDump(size_t crashDumpSizeInBytes, ClientId clientId, void** ppUserdata) = 0;

            // Handles incoming crash dump data. Returns false if we should abort the transfer.
            virtual void ReceiveCrashDumpData(const uint8* pCrashDumpData, size_t crashDumpDataSize, void* pUserdata) = 0;

            // Performs any work that should be done at the end of the transfer.
            virtual void FinishCrashDumpTransfer(bool transferSuccessful, void* pUserdata) = 0;

        protected:
            ICrashDumpHandler() {}
        };

        class GpuCrashDumpServer : public BaseProtocolServer
        {
        public:
            explicit GpuCrashDumpServer(IMsgChannel* pMsgChannel);
            ~GpuCrashDumpServer();

            bool AcceptSession(const SharedPointer<ISession>& pSession) override;
            void SessionEstablished(const SharedPointer<ISession>& pSession) override;
            void UpdateSession(const SharedPointer<ISession>& pSession) override;
            void SessionTerminated(const SharedPointer<ISession>& pSession, Result terminationReason) override;

            // Sets the current crash dump handler.
            // Returns Error if the crash handler cannot currently be changed due to active sessions.
            Result SetCrashDumpHandler(ICrashDumpHandler* pHandler);

            // Gets the current crash dump handler.
            ICrashDumpHandler* GetCrashDumpHandler();

        private:
            Platform::Mutex    m_mutex;
            ICrashDumpHandler* m_pCrashDumpHandler;
            uint32             m_numSessions;
        };
    }
} // DevDriver
