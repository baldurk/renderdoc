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

#include "protocols/rgpServer.h"
#include "msgChannel.h"
#include <cstring>

#include "util/queue.h"

#define RGP_SERVER_MIN_MAJOR_VERSION 2
#define RGP_SERVER_MAX_MAJOR_VERSION 6

namespace DevDriver
{
    namespace RGPProtocol
    {
        enum class SessionState
        {
            ReceivePayload = 0,
            ProcessPayload,
            SendPayload,
            TransferTraceData
        };

        struct RGPSession
        {
            SessionState state;
            Version version;
            uint64 traceSizeInBytes;
            Queue<RGPPayload, 32> chunkPayloads;
            RGPPayload payload;
            bool abortRequestedByClient;

            explicit RGPSession(const AllocCb& allocCb)
                : state(SessionState::ReceivePayload)
                , version(0)
                , traceSizeInBytes(0)
                , chunkPayloads(allocCb)
                , payload()
                , abortRequestedByClient(false)
            {
            }
        };

        RGPServer::RGPServer(IMsgChannel* pMsgChannel)
            : BaseProtocolServer(pMsgChannel, Protocol::RGP, RGP_SERVER_MIN_MAJOR_VERSION, RGP_SERVER_MAX_MAJOR_VERSION)
            , m_traceStatus(TraceStatus::Idle)
            , m_pCurrentSessionData(nullptr)
            , m_profilingStatus(ProfilingStatus::NotAvailable)
        {
            DD_ASSERT(m_pMsgChannel != nullptr);
            memset(&m_traceParameters, 0, sizeof(m_traceParameters));
        }

        RGPServer::~RGPServer()
        {
        }

        void RGPServer::Finalize()
        {
            Platform::LockGuard<Platform::Mutex> lock(m_mutex);
            BaseProtocolServer::Finalize();
        }

        bool RGPServer::AcceptSession(const SharedPointer<ISession>& pSession)
        {
            DD_UNUSED(pSession);
            return true;
        }

        void RGPServer::SessionEstablished(const SharedPointer<ISession>& pSession)
        {
            DD_UNUSED(pSession);

            // Allocate session data for the newly established session
            RGPSession* pSessionData = DD_NEW(RGPSession, m_pMsgChannel->GetAllocCb())(m_pMsgChannel->GetAllocCb());
            pSessionData->state = SessionState::ReceivePayload;
            memset(&pSessionData->payload, 0, sizeof(RGPPayload));

            pSession->SetUserData(pSessionData);
        }

        void RGPServer::UpdateSession(const SharedPointer<ISession>& pSession)
        {
            RGPSession* pSessionData = reinterpret_cast<RGPSession*>(pSession->GetUserData());

            Platform::LockGuard<Platform::Mutex> lock(m_mutex);

            switch (m_traceStatus)
            {
                case TraceStatus::Idle:
                {
                    // Process messages in when we're not executing a trace.
                    switch (pSessionData->state)
                    {
                        case SessionState::ReceivePayload:
                        {
                            uint32 bytesReceived = 0;
                            Result result = pSession->Receive(sizeof(pSessionData->payload), &pSessionData->payload, &bytesReceived, kNoWait);

                            if (result == Result::Success)
                            {
                                DD_ASSERT(sizeof(pSessionData->payload) == bytesReceived);
                                pSessionData->state = SessionState::ProcessPayload;
                            }

                            break;
                        }

                        case SessionState::ProcessPayload:
                        {
                            switch (pSessionData->payload.command)
                            {
                                case RGPMessage::ExecuteTraceRequest:
                                {
                                    // We should always have a null session data pointer when in the idle trace state.
                                    DD_ASSERT(m_pCurrentSessionData == nullptr);

                                    if (m_profilingStatus == ProfilingStatus::Enabled)
                                    {
                                        if (pSession->GetVersion() < RGP_PROFILING_CLOCK_MODES_VERSION)
                                        {
                                            const TraceParameters& traceParameters = pSessionData->payload.executeTraceRequest.parameters;
                                            m_traceParameters.gpuMemoryLimitInMb = traceParameters.gpuMemoryLimitInMb;
                                            m_traceParameters.numPreparationFrames = traceParameters.numPreparationFrames;
                                            m_traceParameters.flags.u32All = traceParameters.flags.u32All;
                                        }
                                        else if ((pSession->GetVersion() == RGP_PROFILING_CLOCK_MODES_VERSION) ||
                                                 (pSession->GetVersion() == RGP_TRACE_PROGRESS_VERSION))
                                        {
                                            const TraceParametersV2& traceParameters = pSessionData->payload.executeTraceRequestV2.parameters;
                                            m_traceParameters.gpuMemoryLimitInMb = traceParameters.gpuMemoryLimitInMb;
                                            m_traceParameters.numPreparationFrames = traceParameters.numPreparationFrames;
                                            m_traceParameters.flags.u32All = traceParameters.flags.u32All;
                                        }
                                        else if (pSession->GetVersion() == RGP_COMPUTE_PRESENTS_VERSION)
                                        {
                                            const TraceParametersV3& traceParameters = pSessionData->payload.executeTraceRequestV3.parameters;
                                            m_traceParameters.gpuMemoryLimitInMb = traceParameters.gpuMemoryLimitInMb;
                                            m_traceParameters.numPreparationFrames = traceParameters.numPreparationFrames;
                                            m_traceParameters.flags.u32All = traceParameters.flags.u32All;
                                        }
                                        else if (pSession->GetVersion() == RGP_TRIGGER_MARKERS_VERSION)
                                        {
                                            const TraceParametersV4& traceParameters = pSessionData->payload.executeTraceRequestV4.parameters;
                                            m_traceParameters.gpuMemoryLimitInMb = traceParameters.gpuMemoryLimitInMb;
                                            m_traceParameters.numPreparationFrames = traceParameters.numPreparationFrames;
                                            m_traceParameters.flags.u32All = traceParameters.flags.u32All;

                                            m_traceParameters.beginTag =
                                                ((static_cast<uint64>(traceParameters.beginTagHigh) << 32) | traceParameters.beginTagLow);
                                            m_traceParameters.endTag =
                                                ((static_cast<uint64>(traceParameters.endTagHigh) << 32) | traceParameters.endTagLow);

                                            Platform::Strncpy(m_traceParameters.beginMarker,
                                                              traceParameters.beginMarker,
                                                              sizeof(m_traceParameters.beginMarker));

                                            Platform::Strncpy(m_traceParameters.endMarker,
                                                              traceParameters.endMarker,
                                                              sizeof(m_traceParameters.endMarker));
                                        }
                                        else
                                        {
                                            // Unhandled protocol version
                                            DD_UNREACHABLE();
                                        }

                                        m_traceStatus = TraceStatus::Pending;
                                        m_pCurrentSessionData = pSessionData;
                                        m_pCurrentSessionData->state = SessionState::TransferTraceData;
                                        m_pCurrentSessionData->version = pSession->GetVersion();
                                        m_pCurrentSessionData->traceSizeInBytes = 0;
                                    }
                                    else
                                    {
                                        if (pSessionData->version >= RGP_TRACE_PROGRESS_VERSION)
                                        {
                                            pSessionData->payload.command = RGPMessage::TraceDataHeader;
                                            pSessionData->payload.traceDataHeader.numChunks = 0;
                                            pSessionData->payload.traceDataHeader.sizeInBytes = 0;
                                            pSessionData->payload.traceDataHeader.result = Result::Error;
                                        }
                                        else
                                        {
                                            pSessionData->payload.command = RGPMessage::TraceDataSentinel;
                                            pSessionData->payload.traceDataSentinel.result = Result::Error;
                                        }

                                        pSessionData->state = SessionState::SendPayload;
                                    }

                                    break;
                                }

                                case RGPMessage::QueryProfilingStatusRequest:
                                {
                                    pSessionData->payload.command = RGPMessage::QueryProfilingStatusResponse;

                                    const ProfilingStatus profilingStatus = m_profilingStatus;

                                    pSessionData->payload.queryProfilingStatusResponse.status = profilingStatus;

                                    pSessionData->state = SessionState::SendPayload;

                                    break;
                                }

                                case RGPMessage::EnableProfilingRequest:
                                {
                                    pSessionData->payload.command = RGPMessage::EnableProfilingResponse;

                                    Result result = Result::Error;

                                    // Profiling can only be enabled before the server is finalized
                                    if ((m_isFinalized == false) & (m_profilingStatus == ProfilingStatus::Available))
                                    {
                                        m_profilingStatus = ProfilingStatus::Enabled;
                                        result = Result::Success;
                                    }

                                    pSessionData->payload.enableProfilingStatusResponse.result = result;

                                    pSessionData->state = SessionState::SendPayload;

                                    break;
                                }
                                default:
                                {
                                    // Invalid command
                                    DD_UNREACHABLE();
                                    break;
                                }
                            }

                            break;
                        }

                        case SessionState::SendPayload:
                        {
                            Result result = pSession->Send(sizeof(pSessionData->payload), &pSessionData->payload, kNoWait);
                            if (result == Result::Success)
                            {
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

                    break;
                }

                case TraceStatus::Running:
                case TraceStatus::Finishing:
                {
                    // We should never enter this state with a null session data pointer.
                    // The termination callback should prevent this from happening.
                    DD_ASSERT(m_pCurrentSessionData != nullptr);

                    // Make sure we only attempt to talk to the session that requested the trace.
                    if (m_pCurrentSessionData == pSessionData)
                    {
                        // The session should always be ready to transfer data in this state.
                        DD_ASSERT(m_pCurrentSessionData->state == SessionState::TransferTraceData);

                        // Look for an abort request if necessary.
                        if ((pSession->GetVersion() >= RGP_TRACE_PROGRESS_VERSION) && (!pSessionData->abortRequestedByClient))
                        {
                            uint32 bytesReceived = 0;
                            Result result = pSession->Receive(sizeof(pSessionData->payload), &pSessionData->payload, &bytesReceived, kNoWait);

                            if (result == Result::Success)
                            {
                                DD_ASSERT(sizeof(pSessionData->payload) == bytesReceived);

                                if (pSessionData->payload.command == RGPMessage::AbortTrace)
                                {
                                    pSessionData->abortRequestedByClient = true;
                                }
                                else
                                {
                                    // We should only ever receive abort requests in this state.
                                    DD_ALERT_ALWAYS();
                                }
                            }
                        }

                        // If the client requested an abort, then send the trace sentinel.
                        if (pSessionData->abortRequestedByClient)
                        {
                            m_pCurrentSessionData->payload.command = RGPMessage::TraceDataSentinel;
                            m_pCurrentSessionData->payload.traceDataSentinel.result = Result::Aborted;

                            Result result = pSession->Send(sizeof(pSessionData->payload), &pSessionData->payload, kNoWait);
                            if (result == Result::Success)
                            {
                                // The trace was aborted. Move back to idle and reset all state.
                                ClearCurrentSession();
                            }
                        }
                        else
                        {
                            // When trace progress is supported, we only send data if we're in the finishing state.
                            const bool sendTraceData = (pSession->GetVersion() >= RGP_TRACE_PROGRESS_VERSION) ? (m_traceStatus == TraceStatus::Finishing)
                                                                                                              : true;
                            if (sendTraceData)
                            {
                                Result result = Result::Success;

                                while (m_pCurrentSessionData->chunkPayloads.IsEmpty() == false)
                                {
                                    result = pSession->Send(sizeof(RGPPayload), m_pCurrentSessionData->chunkPayloads.PeekFront(), kNoWait);

                                    if (result == Result::Success)
                                    {
                                        m_pCurrentSessionData->chunkPayloads.PopFront();
                                    }
                                    else
                                    {
                                        break;
                                    }
                                }

                                if ((result == Result::Success) &
                                    (m_traceStatus == TraceStatus::Finishing))
                                {
                                    // If we make it this far with a success result in the Finishing state, we've sent all the chunk data.
                                    ClearCurrentSession();
                                }
                            }
                        }
                    }

                    break;
                }

                case TraceStatus::Aborting:
                {
                    if (m_pCurrentSessionData->version >= RGP_TRACE_PROGRESS_VERSION)
                    {
                        m_pCurrentSessionData->payload.command = RGPMessage::TraceDataHeader;
                        m_pCurrentSessionData->payload.traceDataHeader.numChunks = 0;
                        m_pCurrentSessionData->payload.traceDataHeader.sizeInBytes = 0;
                        m_pCurrentSessionData->payload.traceDataHeader.result = Result::Error;
                    }
                    else
                    {
                        m_pCurrentSessionData->payload.command = RGPMessage::TraceDataSentinel;
                        m_pCurrentSessionData->payload.traceDataSentinel.result = Result::Error;
                    }

                    Result result = pSession->Send(sizeof(pSessionData->payload), &pSessionData->payload, kNoWait);
                    if (result == Result::Success)
                    {
                        // The trace aborted. Move back to idle and reset all state.
                        ClearCurrentSession();
                    }

                    break;
                }

                default:
                {
                    // Do nothing
                    break;
                }
            }
        }

        void RGPServer::SessionTerminated(const SharedPointer<ISession>& pSession, Result terminationReason)
        {
            DD_UNUSED(terminationReason);
            RGPSession *pRGPSession = reinterpret_cast<RGPSession*>(pSession->SetUserData(nullptr));

            // Free the session data
            if (pRGPSession != nullptr)
            {
                Platform::LockGuard<Platform::Mutex> lock(m_mutex);

                if (m_pCurrentSessionData == pRGPSession)
                {
                    m_traceStatus = TraceStatus::Idle;

                    m_pCurrentSessionData = nullptr;
                }
                DD_DELETE(pRGPSession, m_pMsgChannel->GetAllocCb());
            }
        }

        bool RGPServer::TracesEnabled()
        {
            Platform::LockGuard<Platform::Mutex> lock(m_mutex);

            const bool tracesEnabled = (m_profilingStatus == ProfilingStatus::Enabled);

            return tracesEnabled;
        }

        Result RGPServer::EnableTraces()
        {
            Result result = Result::Error;

            Platform::LockGuard<Platform::Mutex> lock(m_mutex);

            // Make sure we're not currently running a trace and that traces are currently disabled.
            if ((m_traceStatus == TraceStatus::Idle) & (m_profilingStatus == ProfilingStatus::NotAvailable))
            {
                m_profilingStatus = ProfilingStatus::Available;
                result = Result::Success;
            }

            return result;
        }

        Result RGPServer::DisableTraces()
        {
            Result result = Result::Error;

            Platform::LockGuard<Platform::Mutex> lock(m_mutex);

            // Make sure we're not currently running a trace.
            if (m_traceStatus == TraceStatus::Idle)
            {
                m_profilingStatus = ProfilingStatus::NotAvailable;
                result = Result::Success;
            }

            return result;
        }

        bool RGPServer::IsTracePending()
        {
            // Platform::LockGuard<Platform::Mutex> lock(m_mutex);

            const bool isTracePending = (m_traceStatus == TraceStatus::Pending);

            return isTracePending;
        }

        bool RGPServer::IsTraceRunning()
        {
            Platform::LockGuard<Platform::Mutex> lock(m_mutex);

            const bool isTraceRunning = (m_traceStatus == TraceStatus::Running);

            return isTraceRunning;
        }

        Result RGPServer::BeginTrace()
        {
            Result result = Result::Error;

            Platform::LockGuard<Platform::Mutex> lock(m_mutex);

            // A trace can only begin if a client requests one.
            if (m_traceStatus == TraceStatus::Pending)
            {
                m_traceStatus = TraceStatus::Running;
                result = Result::Success;
            }

            return result;
        }

        Result RGPServer::EndTrace()
        {
            Result result = Result::Error;

            Platform::LockGuard<Platform::Mutex> lock(m_mutex);

            // Make sure there was a trace running before.
            if (m_traceStatus == TraceStatus::Running)
            {
                if (m_pCurrentSessionData != nullptr)
                {
                    if (m_pCurrentSessionData->version >= RGP_TRACE_PROGRESS_VERSION)
                    {
                        // Inject the trace header
                        RGPPayload *pPayload = m_pCurrentSessionData->chunkPayloads.AllocateFront();
                        result = (pPayload != nullptr) ? Result::Success : Result::Error;

                        if (result == Result::Success)
                        {
                            // todo: Support trace sizes larger than 4GB
                            DD_ASSERT(m_pCurrentSessionData->traceSizeInBytes < (~0u));

                            pPayload->command = RGPMessage::TraceDataHeader;
                            pPayload->traceDataHeader.result = Result::Success;
                            pPayload->traceDataHeader.numChunks = static_cast<uint32>(m_pCurrentSessionData->chunkPayloads.Size() - 1);
                            pPayload->traceDataHeader.sizeInBytes = static_cast<uint32>(m_pCurrentSessionData->traceSizeInBytes);

                            pPayload = m_pCurrentSessionData->chunkPayloads.AllocateBack();
                            result = (pPayload != nullptr) ? Result::Success : Result::Error;
                        }

                        if (result == Result::Success)
                        {
                            pPayload->command = RGPMessage::TraceDataSentinel;
                            pPayload->traceDataSentinel.result = Result::Success;

                            m_traceStatus = TraceStatus::Finishing;
                        }
                    }
                    else
                    {
                        RGPPayload *pPayload = m_pCurrentSessionData->chunkPayloads.AllocateBack();
                        if (pPayload != nullptr)
                        {
                            m_traceStatus = TraceStatus::Finishing;
                            pPayload->command = RGPMessage::TraceDataSentinel;
                            pPayload->traceDataSentinel.result = Result::Success;
                            result = Result::Success;
                        }
                    }
                }
                else
                {
                    // The client that requested the trace has disconnected. Discard the trace.
                    m_traceStatus = TraceStatus::Idle;
                    result = Result::Success;
                }

            }

            DD_ASSERT(result == Result::Success);
            return result;
        }

        Result RGPServer::AbortTrace()
        {
            Result result = Result::Error;

            Platform::LockGuard<Platform::Mutex> lock(m_mutex);

            // Make sure there was a trace running before.
            if (m_traceStatus == TraceStatus::Running)
            {
                if (m_pCurrentSessionData != nullptr)
                {
                    m_traceStatus = TraceStatus::Aborting;
                }
                else
                {
                    // The client that requested the trace has disconnected. Discard the trace.
                    m_traceStatus = TraceStatus::Idle;
                }

                result = Result::Success;
            }

            return result;
        }

        Result RGPServer::WriteTraceData(const uint8* pTraceData, size_t traceDataSize)
        {
            Result result = Result::Error;

            Platform::LockGuard<Platform::Mutex> lock(m_mutex);

            // Make sure there is a trace running.
            if (m_traceStatus == TraceStatus::Running)
            {
                size_t traceDataRemaining = traceDataSize;

                if (m_pCurrentSessionData != nullptr)
                {
                    m_pCurrentSessionData->traceSizeInBytes += traceDataSize;
                    while (traceDataRemaining > 0)
                    {
                        RGPPayload *pPayload = m_pCurrentSessionData->chunkPayloads.AllocateBack();
                        if (pPayload != nullptr)
                        {
                            const size_t dataSize = ((traceDataRemaining < kMaxTraceDataChunkSize) ? traceDataRemaining
                                                     : kMaxTraceDataChunkSize);

                            memcpy(pPayload->traceDataChunk.chunk.data, pTraceData, dataSize);
                            pPayload->traceDataChunk.chunk.dataSize = static_cast<uint32>(dataSize);
                            pPayload->command = RGPMessage::TraceDataChunk;

                            pTraceData += dataSize;
                            traceDataRemaining -= dataSize;
                        }
                        else
                        {
                            break;
                        }
                    }
                }

                if (traceDataRemaining == 0)
                    result = Result::Success;
            }

            DD_ASSERT(result == Result::Success);
            return result;
        }

        ProfilingStatus RGPServer::QueryProfilingStatus()
        {
            Platform::LockGuard<Platform::Mutex> lock(m_mutex);

            const ProfilingStatus profilingStatus = m_profilingStatus;

            return profilingStatus;
        }

        ServerTraceParametersInfo RGPServer::QueryTraceParameters()
        {
            Platform::LockGuard<Platform::Mutex> lock(m_mutex);

            const ServerTraceParametersInfo traceParameters = m_traceParameters;

            return traceParameters;
        }

        void RGPServer::LockData()
        {
            m_mutex.Lock();
        }

        void RGPServer::UnlockData()
        {
            m_mutex.Unlock();
        }

        void RGPServer::ClearCurrentSession()
        {
            if (m_pCurrentSessionData != nullptr)
            {
                // Move back to idle state and reset all state if we have a valid session.
                m_traceStatus = TraceStatus::Idle;
                m_pCurrentSessionData->state = SessionState::ReceivePayload;
                m_pCurrentSessionData->version = 0;
                m_pCurrentSessionData->traceSizeInBytes = 0;
                m_pCurrentSessionData->chunkPayloads.Clear();
                m_pCurrentSessionData->abortRequestedByClient = false;
                m_pCurrentSessionData = nullptr;
            }
        }
    }
} // DevDriver
