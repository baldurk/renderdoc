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
* @file  rgpServer.h
* @brief Class declaration for RGPServer.
***********************************************************************************************************************
*/

#pragma once

#include "baseProtocolServer.h"

#include "rgpProtocol.h"

namespace DevDriver
{
    namespace RGPProtocol
    {
        enum class TraceStatus : uint32
        {
            Idle = 0,
            Pending,
            Running,
            Finishing,
            Aborting
        };

        struct ServerTraceParametersInfo
        {
            uint32 gpuMemoryLimitInMb;
            uint32 numPreparationFrames;

            union
            {
                struct
                {
                    uint32 enableInstructionTokens : 1;
                    uint32 allowComputePresents : 1;
                    uint32 reserved : 30;
                };
                uint32 u32All;
            } flags;

            uint64 beginTag;
            uint64 endTag;

            char beginMarker[kMarkerStringLength];
            char endMarker[kMarkerStringLength];
        };

        struct RGPSession;

        class RGPServer : public BaseProtocolServer
        {
        public:
            explicit RGPServer(IMsgChannel* pMsgChannel);
            ~RGPServer();

            void Finalize() override;

            bool AcceptSession(const SharedPointer<ISession>& pSession) override;
            void SessionEstablished(const SharedPointer<ISession>& pSession) override;
            void UpdateSession(const SharedPointer<ISession>& pSession) override;
            void SessionTerminated(const SharedPointer<ISession>& pSession, Result terminationReason) override;

            // Returns true if traces are currently enabled.
            bool TracesEnabled();

            // Allows remote clients to request traces.
            Result EnableTraces();

            // Disable support for traces.
            Result DisableTraces();

            // Returns true if a client has requested a trace and it has not been started yet.
            bool IsTracePending();

            // Returns true if a client has requested a trace and it is currently running.
            bool IsTraceRunning();

            // Starts a new trace. This will only succeed if a trace was previously pending.
            Result BeginTrace();

            // Ends a trace. This will only succeed if a trace was previously in progress.
            Result EndTrace();

            // Aborts a trace. This will only succeed if a trace was previously in progress.
            Result AbortTrace();

            // Writes data into the current trace. This can only be performed when there is a trace in progress.
            Result WriteTraceData(const uint8* pTraceData, size_t traceDataSize);

            // Returns the current profiling status on the rgp server.
            ProfilingStatus QueryProfilingStatus();

            // Returns the current trace parameters on the rgp server.
            ServerTraceParametersInfo QueryTraceParameters();

        private:
            void LockData();
            void UnlockData();
            void ClearCurrentSession();

            Platform::Mutex m_mutex;
            TraceStatus m_traceStatus;
            RGPSession* m_pCurrentSessionData;
            ProfilingStatus m_profilingStatus;
            ServerTraceParametersInfo m_traceParameters;
        };
    }
} // DevDriver
