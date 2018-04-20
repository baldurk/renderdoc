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

//=============================================================================
/// Copyright (c) 2016 Advanced Micro Devices, Inc. All rights reserved.
/// \author AMD Developer Tools Team
/// \file
/// \brief  Implementation of a realtime D3D12 ETW Event consumer.
//=============================================================================

#pragma once

#include "traceSession.h"
#include "gpuopen.h"
#include "protocols/etwProtocol.h"
#include <thread>

#include "util/queue.h"
#include "../win/traceSession.h"
#include "../win/d3d12EtwEventParser.h"

namespace DevDriver
{
    namespace ETWProtocol
    {
        enum class SessionState
        {
            Idle = 0,
            Tracing,
            Streaming,
            TransmitMessage
        };

        class ETWSession : public ETWConsumerBase
        {
        public:
            ETWSession(const SharedPointer<ISession>& pSession,
                       const AllocCb& allocCb) :
                m_pSession(pSession),
                m_allocCb(allocCb),
                m_state(SessionState::Idle),
                m_payload(),
                m_trace(allocCb),
                m_numEvents(0),
                m_traceSession(),
                m_traceParser(),
                m_traceInProgress(false)
            {
            };

            ~ETWSession() {
                if (m_traceInProgress != false)
                {
                    m_traceSession.Stop();
                    m_traceInProgress = false;
                    m_traceThread.join();
                    m_trace.Clear();
                }
            };

            void UpdateSession()
            {
                DD_ASSERT(this == reinterpret_cast<ETWSession*>(m_pSession->GetUserData()));

                switch (m_state)
                {
                    case SessionState::Idle:
                    {
                        ETWPayload payload;
                        uint32 bytesReceived = 0;
                        Result result = m_pSession->Receive(sizeof(payload), &payload, &bytesReceived, kNoWait);
                        if (result == Result::Success)
                        {
                            DD_ASSERT(sizeof(payload) == bytesReceived);
                            m_trace.Clear();
                            m_numEvents = 0;

                            if (payload.command == ETWMessage::BeginTrace)
                            {
                                DD_PRINT(LogLevel::Info, "[ETWSession] Trace request received");
                                m_payload.command = ETWMessage::BeginResponse;
                                m_payload.startTraceResponse.result = BeginTrace(payload.startTrace.processId);
                                DD_ASSERT(m_payload.startTraceResponse.result == Result::Success);
                                TransmitAndChangeState();
                            }
                        }
                        break;
                    }
                    case SessionState::TransmitMessage:
                    {
                        TransmitAndChangeState();
                        break;
                    }
                    case SessionState::Tracing:
                    {
                        ETWPayload payload;
                        uint32 bytesReceived = 0;
                        Result result = m_pSession->Receive(sizeof(payload), &payload, &bytesReceived, kNoWait);
                        if (result == Result::Success)
                        {
                            DD_ASSERT(sizeof(payload) == bytesReceived);
                            if (payload.command == ETWMessage::EndTrace)
                            {
                                m_payload.command = ETWMessage::EndResponse;
                                m_payload.stopTraceResponse.result = EndTrace();

                                if (payload.stopTrace.discard == 0)
                                {
                                    m_payload.stopTraceResponse.numEventsCaptured = static_cast<uint32>(m_numEvents);
                                }
                                else
                                {
                                    m_payload.stopTraceResponse.numEventsCaptured = 0;
                                    m_trace.Clear();
                                }
                                TransmitAndChangeState();
                            }
                        }
                        break;
                    }
                    case SessionState::Streaming:
                    {
                        while (m_trace.Size() > 0)
                        {
                            ETWPayload *pPayload = m_trace.PeekFront();
                            Result sendResult = m_pSession->Send(sizeof(ETWPayload), pPayload, kNoWait);
                            if (sendResult == Result::Success)
                            {
                                m_trace.PopFront();
                            }
                            else if (sendResult == Result::NotReady)
                            {
                                break;
                            }
                        }
                        if ((m_trace.Size() == 0) & (m_numEvents > 0))
                        {
                            m_payload.command = ETWMessage::TraceDataSentinel;
                            m_payload.traceDataSentinel.result = Result::Success;
                            TransmitAndChangeState();
                        }
                        break;
                    }
                    default:
                        DD_UNREACHABLE();
                        break;
                }
            }

            Result BeginTrace(ProcessId processId)
            {
                if (m_traceInProgress == false)
                {
                    DD_PRINT(LogLevel::Info, "[ETWSession::BeginTrace] Beginning trace");

                    bool started = m_traceSession.Start();
                    if (started)
                    {
                        DD_PRINT(LogLevel::Info, "[ETWSession::BeginTrace] Trace session started");

                        if (m_traceParser.Start(processId))
                        {
                            DD_PRINT(LogLevel::Info, "[ETWSession::BeginTrace] Trace parser started for process %u", processId);
                            if (m_traceSession.Open(this))
                            {
                                DD_PRINT(LogLevel::Info, "[ETWSession::BeginTrace] Trace session opened");
                                //PCWSTR d3dRuntimeProviderGUID = L"{5d8087dd-3a9b-4f56-90df-49196cdc4f11}";
                                PCWSTR dxgKernelProviderGUID = L"{802ec45a-1e99-4b83-9920-87c98277ba9d}";

                                //bool bRuntimeProviderEnabled = m_traceSession.EnableProviderByGUID(d3dRuntimeProviderGUID, 0);
                                //if (!bRuntimeProviderEnabled)
                                //{
                                //    wprintf(L"Failed to enable provider with GUID '%s'\n", d3dRuntimeProviderGUID);
                                //}

                                bool bDxgKernelProviderEnabled = m_traceSession.EnableProviderByGUID(dxgKernelProviderGUID, 0);
                                if (bDxgKernelProviderEnabled)
                                {
                                    DD_PRINT(LogLevel::Info, "[ETWSession::BeginTrace] DXGK provider enabled");
                                    m_traceThread = std::thread(&TraceSession::Process, &m_traceSession);
                                    if (m_traceThread.joinable())
                                    {
                                        DD_PRINT(LogLevel::Info, "[ETWSession::BeginTrace] Trace thread started");
                                        m_traceInProgress = true;
                                        return Result::Success;
                                    }
                                }
                            }
                        }
                    }
                }
                DD_PRINT(LogLevel::Info, "[ETWSession::BeginTrace] Begin failed");
                return Result::Error;
            }

            Result EndTrace()
            {
                if (m_traceInProgress != false)
                {
                    DD_PRINT(LogLevel::Info, "[ETWSession::EndTrace] Ending trace");
                    if (m_traceSession.Close())
                    {
                        DD_PRINT(LogLevel::Info, "[ETWSession::EndTrace] Trace session closed");
                        if (m_traceSession.Stop())
                        {
                            DD_PRINT(LogLevel::Info, "[ETWSession::EndTrace] Trace session stopped");
                            m_traceInProgress = false;
                            m_traceThread.join();
                            m_trace.Clear();
                            m_numEvents = m_traceParser.FinishTrace(m_trace);
                            DD_PRINT(LogLevel::Info, "[ETWSession::EndTrace] Finished parsing %u events", m_numEvents);
                            return Result::Success;
                        }
                    }
                }
                DD_PRINT(LogLevel::Info, "[ETWSession::EndTrace] End failed");
                return Result::Error;
            }

            void OnEventRecord(PEVENT_RECORD pEvent) override
            {
                m_traceParser.ParseEvent(pEvent);
            }

        private:
            SharedPointer<ISession> m_pSession;
            AllocCb m_allocCb;
            SessionState m_state;
            ETWPayload m_payload;
            Queue<ETWPayload> m_trace;
            size_t m_numEvents;
            TraceSession m_traceSession;
            std::thread m_traceThread;
            EtwParser m_traceParser;
            bool m_traceInProgress;

            void TransmitAndChangeState()
            {
                if (m_pSession->Send(sizeof(m_payload), &m_payload, kNoWait) == Result::Success)
                {
                    switch (m_payload.command)
                    {
                        case ETWMessage::BeginResponse:
                        {
                            if (m_payload.startTraceResponse.result == Result::Success)
                            {
                                m_state = SessionState::Tracing;
                            }
                            else
                            {
                                m_state = SessionState::Idle;
                            }
                            break;
                        }
                        case ETWMessage::EndResponse:
                        {
                            if (m_payload.stopTraceResponse.result == Result::Success &&
                                m_payload.stopTraceResponse.numEventsCaptured != 0)
                            {
                                m_state = SessionState::Streaming;
                            }
                            else
                            {
                                m_state = SessionState::Idle;
                            }
                            break;
                        }
                        case ETWMessage::TraceDataSentinel:
                        {
                            m_state = SessionState::Idle;
                            break;
                        }
                        default:
                        {
                            m_state = SessionState::Idle;
                            DD_UNREACHABLE();
                            break;
                        }
                    }
                }
            }
        };
    }
}
