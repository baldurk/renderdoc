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
* @file  rgpProtcol.h
* @brief Common interface for RGP Protocol.
***********************************************************************************************************************
*/

#pragma once

#pragma pack(push)

#include "gpuopen.h"

#define RGP_PROTOCOL_MAJOR_VERSION 6
#define RGP_PROTOCOL_MINOR_VERSION 0

#define RGP_INTERFACE_VERSION ((RGP_INTERFACE_MAJOR_VERSION << 16) | RGP_INTERFACE_MINOR_VERSION)

#define RGP_PROTOCOL_MINIMUM_MAJOR_VERSION 2

/*
***********************************************************************************************************************
*| Version | Change Description                                                                                       |
*| ------- | ---------------------------------------------------------------------------------------------------------|
*|  6.0    | Added support for trace trigger markers.                                                                 |
*|  5.0    | Added support for allow compute presents trace parameter and removed unused clock mode parameter.        |
*|  4.0    | Added support for reporting trace transfer progress.                                                     |
*|  3.0    | Updated TraceParameters struct to allow for specifying profiling clock mode.                             |
*|  2.0    | Add TraceParameters struct and ExecuteTraceRequestPayload so a client can specify trace options.         |
*|  1.0    | Initial version                                                                                          |
***********************************************************************************************************************
*/

#define RGP_TRIGGER_MARKERS_VERSION 6
#define RGP_COMPUTE_PRESENTS_VERSION 5
#define RGP_TRACE_PROGRESS_VERSION 4
#define RGP_PROFILING_CLOCK_MODES_VERSION 3
#define RGP_TRACE_PARAMETERS_VERSION 2
#define RGP_INITIAL_VERSION 1

namespace DevDriver
{
    namespace RGPProtocol
    {
        ///////////////////////
        // RGP Protocol
        enum struct RGPMessage : MessageCode
        {
            Unknown = 0,
            ExecuteTraceRequest,
            TraceDataChunk,
            TraceDataSentinel,
            QueryProfilingStatusRequest,
            QueryProfilingStatusResponse,
            EnableProfilingRequest,
            EnableProfilingResponse,
            TraceDataHeader,
            AbortTrace,
            Count
        };

        // @note: We currently subtract sizeof(uint32) instead of sizeof(RGPMessage) to work around struct packing issues.
        //        The compiler pads out RGPMessage to 4 bytes when it's included in the payload struct. It also pads out
        //        the TraceDataChunk data field to 1000 bytes. This causes the total payload size to be 1004 bytes which is
        //        4 bytes larger than the maximum size allowed.
        DD_STATIC_CONST Size kMaxTraceDataChunkSize = (kMaxPayloadSizeInBytes - sizeof(uint32) - sizeof(uint32));

        ///////////////////////
        // RGP Constants
        const uint32 kMarkerStringLength = 256;

        ///////////////////////
        // RGP Types
        DD_NETWORK_STRUCT(TraceDataChunk, 4)
        {
            uint32 dataSize;
            uint8  data[kMaxTraceDataChunkSize];
        };

        DD_CHECK_SIZE(TraceDataChunk, kMaxTraceDataChunkSize + sizeof(int32));

        DD_NETWORK_STRUCT(TraceParameters, 4)
        {
            uint32 gpuMemoryLimitInMb;
            uint32 numPreparationFrames;
            union
            {
                struct
                {
                    uint32 enableInstructionTokens : 1;
                    uint32 reserved : 31;
                };
                uint32 u32All;
            } flags;
        };

        DD_CHECK_SIZE(TraceParameters, 12);

        enum struct ProfilingClockMode : uint32
        {
            Stable = 0,
            Max,
            Normal,
            Count
        };

        DD_NETWORK_STRUCT(TraceParametersV2, 4)
        {
            uint32 gpuMemoryLimitInMb;
            uint32 numPreparationFrames;
            ProfilingClockMode clockMode;
            union
            {
                struct
                {
                    uint32 enableInstructionTokens : 1;
                    uint32 reserved : 31;
                };
                uint32 u32All;
            } flags;
        };

        DD_CHECK_SIZE(TraceParametersV2, 16);

        DD_NETWORK_STRUCT(TraceParametersV3, 4)
        {
            uint32 gpuMemoryLimitInMb;
            uint32 numPreparationFrames;
            union
            {
                struct
                {
                    uint32 enableInstructionTokens : 1;
                    uint32 allowComputePresents    : 1;
                    uint32 reserved : 30;
                };
                uint32 u32All;
            } flags;
        };

        DD_CHECK_SIZE(TraceParametersV3, 12);

        DD_NETWORK_STRUCT(TraceParametersV4, 4)
        {
            uint32 gpuMemoryLimitInMb;
            uint32 numPreparationFrames;
            union
            {
                struct
                {
                    uint32 enableInstructionTokens : 1;
                    uint32 allowComputePresents    : 1;
                    uint32 reserved : 30;
                };
                uint32 u32All;
            } flags;

            // Begin Tag
            uint32 beginTagHigh;
            uint32 beginTagLow;

            // End Tag
            uint32 endTagHigh;
            uint32 endTagLow;

            // Begin/End Marker Strings
            char beginMarker[kMarkerStringLength];
            char endMarker[kMarkerStringLength];
        };

        DD_CHECK_SIZE(TraceParametersV4, 540);

        enum struct ProfilingStatus : uint32
        {
            NotAvailable = 0,
            Available,
            Enabled,
            Count
        };

        ///////////////////////
        // RGP Payloads

        DD_NETWORK_STRUCT(ExecuteTraceRequestPayload, 4)
        {
            TraceParameters parameters;
        };

        DD_CHECK_SIZE(ExecuteTraceRequestPayload, 12);

        DD_NETWORK_STRUCT(ExecuteTraceRequestPayloadV2, 4)
        {
            TraceParametersV2 parameters;
        };

        DD_CHECK_SIZE(ExecuteTraceRequestPayloadV2, 16);

        DD_NETWORK_STRUCT(ExecuteTraceRequestPayloadV3, 4)
        {
            TraceParametersV3 parameters;
        };

        DD_CHECK_SIZE(ExecuteTraceRequestPayloadV3, 12);

        DD_NETWORK_STRUCT(ExecuteTraceRequestPayloadV4, 4)
        {
            TraceParametersV4 parameters;
        };

        DD_CHECK_SIZE(ExecuteTraceRequestPayloadV4, 540);

        DD_NETWORK_STRUCT(ExecuteTraceResponsePayload, 4)
        {
            Result result;
        };

        DD_CHECK_SIZE(ExecuteTraceResponsePayload, 4);

        DD_NETWORK_STRUCT(TraceDataChunkPayload, 4)
        {
            TraceDataChunk chunk;
        };

        DD_CHECK_SIZE(TraceDataChunkPayload, kMaxTraceDataChunkSize + sizeof(int32));

        DD_NETWORK_STRUCT(TraceDataSentinelPayload, 4)
        {
            Result result;
        };

        DD_CHECK_SIZE(TraceDataSentinelPayload, 4);

        DD_NETWORK_STRUCT(TraceDataHeaderPayload, 4)
        {
            Result result;
            uint32 numChunks;
            uint32 sizeInBytes;
        };

        DD_CHECK_SIZE(TraceDataHeaderPayload, 12);

        DD_NETWORK_STRUCT(QueryProfilingStatusResponsePayload, 4)
        {
            ProfilingStatus status;
        };

        DD_CHECK_SIZE(QueryProfilingStatusResponsePayload, 4);

        DD_NETWORK_STRUCT(EnableProfilingResponsePayload, 4)
        {
            Result result;
        };

        DD_CHECK_SIZE(EnableProfilingResponsePayload, 4);

        DD_NETWORK_STRUCT(RGPPayload, 4)
        {
            RGPMessage  command;
            // pad out to 4 bytes for alignment requirements
            char        padding[3];
            union
            {
                ExecuteTraceRequestPayload          executeTraceRequest;
                ExecuteTraceRequestPayloadV2        executeTraceRequestV2;
                ExecuteTraceRequestPayloadV3        executeTraceRequestV3;
                ExecuteTraceRequestPayloadV4        executeTraceRequestV4;
                ExecuteTraceResponsePayload         executeTraceResponse;
                TraceDataChunkPayload               traceDataChunk;
                TraceDataSentinelPayload            traceDataSentinel;
                TraceDataHeaderPayload              traceDataHeader;
                QueryProfilingStatusResponsePayload queryProfilingStatusResponse;
                EnableProfilingResponsePayload      enableProfilingStatusResponse;
            };
        };

        DD_CHECK_SIZE(RGPPayload, kMaxPayloadSizeInBytes);
    }
}

#pragma pack(pop)
