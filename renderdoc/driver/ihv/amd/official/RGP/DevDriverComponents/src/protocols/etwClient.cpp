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

#include "protocols/etwClient.h"
#include "msgChannel.h"
#include <cstring>

namespace DevDriver
{
    namespace ETWProtocol
    {
        ETWClient::ETWClient(IMsgChannel* pMsgChannel)
            : BaseProtocolClient(pMsgChannel, Protocol::ETW, kVersion, kVersion)
            , m_sessionState(SessionState::Idle)
        {
        }

        ETWClient::~ETWClient()
        {
        }

        Result ETWClient::EnableTracing(ProcessId processId)
        {
            Result result = Result::Error;

            if (IsConnected() && m_sessionState == SessionState::Idle)
            {
                ETWPayload payload = {};
                payload.command = ETWMessage::BeginTrace;
                payload.startTrace.processId = processId;
                result = SendPayload(payload);
                if (result == Result::Success)
                {
                    // Receive chunk data until we reach a trace data sentinel.
                    result = ReceivePayload(payload);
                    if (result == Result::Success)
                    {
                        if (payload.command == ETWMessage::BeginResponse)
                        {
                            result = payload.startTraceResponse.result;
                        }
                        else
                        {
                            result = Result::Error;
                        }
                    }
                }
            }

            if (result == Result::Success)
            {
                m_sessionState = SessionState::Tracing;
            }
            return result;
        }

        Result ETWClient::DisableTracing(size_t *pNumEvents)
        {
            Result result = Result::Error;
            if (IsConnected() && m_sessionState == SessionState::Tracing)
            {
                ETWPayload payload = {};
                payload.command = ETWMessage::EndTrace;
                payload.stopTrace.discard = (pNumEvents == nullptr);
                result = SendPayload(payload);
                if (result == Result::Success)
                {
                    // Receive chunk data until we reach a trace data sentinel.
                    result = ReceivePayload(payload);
                    if (result == Result::Success)
                    {
                        if (payload.command == ETWMessage::EndResponse)
                        {
                            if (pNumEvents != nullptr)
                            {
                                *pNumEvents = payload.stopTraceResponse.numEventsCaptured;
                            }

                            result = payload.stopTraceResponse.result;
                            if (result == Result::Success && payload.stopTraceResponse.numEventsCaptured != 0)
                            {
                                m_sessionState = SessionState::Waiting;
                            }
                            else
                            {
                                m_sessionState = SessionState::Idle;
                            }
                        }
                        else
                        {
                            result = Result::Error;
                        }
                    }
                }
            }
            return result;
        }

        Result ETWClient::GetTraceData(GpuEvent *buffer, size_t numEvents)
        {
            Result result = Result::Error;
            if (IsConnected() && m_sessionState == SessionState::Waiting)
            {
                m_sessionState = SessionState::Receiving;
                bool traceCompleted = false;
                size_t numEventsCopied = 0;
                ETWPayload payload = {};
                // Receive chunk data until we reach a trace data sentinel.
                Result readResult = ReceivePayload(payload);
                while (readResult == Result::Success && !traceCompleted)
                {
                    switch (payload.command)
                    {
                        case ETWMessage::TraceDataChunk:
                        {
                            if ((numEventsCopied + payload.traceDataChunk.numEvents) <= numEvents && buffer != nullptr)
                            {
                                memcpy(&buffer[numEventsCopied], &payload.traceDataChunk.events[0], payload.traceDataChunk.numEvents * sizeof(GpuEvent));
                                numEventsCopied += payload.traceDataChunk.numEvents;
                            }
                            break;
                        }
                        case ETWMessage::TraceDataSentinel:
                        {
                            if (numEventsCopied <= numEvents)
                            {
                                result = payload.traceDataSentinel.result;
                            }
                            traceCompleted = true;
                            break;
                        }
                        default:
                            result = Result::Error;
                            traceCompleted = true;
                            break;
                    }
                    if (!traceCompleted)
                        readResult = ReceivePayload(payload);
                }
                m_sessionState = SessionState::Idle;
            }
            return result;
        }
    }
} // DevDriver
