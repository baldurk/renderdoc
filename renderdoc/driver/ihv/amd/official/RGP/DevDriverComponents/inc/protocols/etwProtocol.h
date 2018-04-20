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
* @file  etwProtcol.h
* @brief Common interface for ETW Protocol.
***********************************************************************************************************************
*/

#pragma once

#include "gpuopen.h"

namespace DevDriver
{
    enum struct GpuEventType : uint16
    {
        Unknown = 0,
        QueueSignal,
        QueueWait,
        Count
    };

    DD_NETWORK_STRUCT(GpuEvent, 8)
    {
        uint64       submissionTime;
        uint64       completionTime;
        GpuEventType type;
        uint8        reserved[6];
        union
        {
            struct
            {
                uint64 contextIdentifier;
                uint64 fenceObject;
                uint64 fenceValue;
            } queue;
        };
    };

    DD_CHECK_SIZE(GpuEvent, 48);

    namespace ETWProtocol
    {
        ///////////////////////
        // ETW Protocol
        enum struct ETWMessage : MessageCode
        {
            Unknown = 0,
            BeginTrace,
            BeginResponse,
            EndTrace,
            EndResponse,
            TraceDataChunk,
            TraceDataSentinel,
            Count
        };

        ///////////////////////
        // ETW Constants
        DD_STATIC_CONST Version kVersion = 3;

        // @note: We currently subtract sizeof(uint32) instead of sizeof(ETWMessage) to work around struct packing issues.
        //        The compiler pads out ETWMessage to 4 bytes when it's included in the payload struct. It also pads out
        //        the TraceDataChunk data field to 1000 bytes. This causes the total payload size to be 1004 bytes which is
        //        4 bytes larger than the maximum size allowed.
        static const Size kMaxTraceDataChunkSize = (kMaxPayloadSizeInBytes - sizeof(uint32) - sizeof(uint32));

        ///////////////////////
        // ETW Types
        DD_NETWORK_STRUCT(StartTraceRequestPayload, 4)
        {
            ProcessId processId;
        };

        DD_CHECK_SIZE(StartTraceRequestPayload, 4);

        DD_NETWORK_STRUCT(StartTraceResponsePayload, 4)
        {
            Result result;
        };

        DD_CHECK_SIZE(StartTraceResponsePayload, 4);

        DD_NETWORK_STRUCT(StopTraceRequestPayload, 4)
        {
            uint8 discard;
            uint8 reserved[3];
        };

        DD_CHECK_SIZE(StopTraceRequestPayload, 4);

        DD_NETWORK_STRUCT(StopTraceResponsePayload, 4)
        {
            Result result;
            uint32 numEventsCaptured;
        };

        DD_CHECK_SIZE(StopTraceResponsePayload, 8);

        DD_NETWORK_STRUCT(TraceDataChunk, 4)
        {
            uint32 dataSize;
            uint8 data[kMaxTraceDataChunkSize];
        };

        DD_STATIC_CONST uint32 kTraceChunkAlignmentSize = 8;
        DD_STATIC_CONST uint32 kMaxEventsPerChunk = (kMaxPayloadSizeInBytes - kTraceChunkAlignmentSize * 2)/sizeof(GpuEvent);

        DD_NETWORK_STRUCT(TraceDataChunkPayload, kTraceChunkAlignmentSize)
        {
            uint16 numEvents;
            uint8  reserved[kTraceChunkAlignmentSize - sizeof(uint16)];
            GpuEvent events[kMaxEventsPerChunk];
        };

        DD_CHECK_SIZE(TraceDataChunkPayload, (sizeof(GpuEvent) * kMaxEventsPerChunk) + offsetof(TraceDataChunkPayload, events));

        DD_NETWORK_STRUCT(TraceDataSentinelPayload, 4)
        {
            Result result;
        };

        DD_CHECK_SIZE(TraceDataSentinelPayload, 4);

        enum struct ProfilingStatus : uint32
        {
            NotAvailable = 0,
            Available,
            Enabled,
            Count
        };

        ///////////////////////
        // ETW Payloads
        DD_NETWORK_STRUCT(ETWPayload, 8)
        {
            ETWMessage command;
            // pad out to 4 bytes for alignment requirements
            char        padding[7];
            union
            {
                StartTraceRequestPayload            startTrace;
                StartTraceResponsePayload           startTraceResponse;
                StopTraceRequestPayload             stopTrace;
                StopTraceResponsePayload            stopTraceResponse;
                TraceDataChunkPayload               traceDataChunk;
                TraceDataSentinelPayload            traceDataSentinel;
            };
        };

        DD_CHECK_SIZE(ETWPayload, 1360);
    }
}
