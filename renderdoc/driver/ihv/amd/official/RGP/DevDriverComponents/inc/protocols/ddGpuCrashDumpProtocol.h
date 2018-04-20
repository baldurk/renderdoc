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
* @file  ddGpuCrashDumpProtocol.h
* @brief Header file for the gpu crash dump protocol
***********************************************************************************************************************
*/

#pragma once

#include "gpuopen.h"
#include "systemProtocols.h"

#define GPUCRASHDUMP_PROTOCOL_MAJOR_VERSION 1
#define GPUCRASHDUMP_PROTOCOL_MINOR_VERSION 0

#define GPUCRASHDUMP_INTERFACE_VERSION ((GPUCRASHDUMP_INTERFACE_MAJOR_VERSION << 16) | GPUCRASHDUMP_INTERFACE_MINOR_VERSION)

#define GPUCRASHDUMP_PROTOCOL_MINIMUM_MAJOR_VERSION 1

/*
***********************************************************************************************************************
*| Version | Change Description                                                                                       |
*| ------- | ---------------------------------------------------------------------------------------------------------|
*|  1.0    | Initial version                                                                                          |
***********************************************************************************************************************
*/

#define GPUCRASHDUMP_INITIAL_VERSION 1

namespace DevDriver
{
    namespace GpuCrashDumpProtocol
    {
        ///////////////////////
        // GpuCrashDump Protocol
        enum struct GpuCrashDumpMessage : MessageCode
        {
            Unknown = 0,
            GpuCrashNotify,
            GpuCrashAcknowledge,
            GpuCrashDataChunk,
            GpuCrashDataSentinel,
            Count
        };

        ///////////////////////
        // GpuCrashDump Constants
        // @note: We currently subtract sizeof(uint32) instead of sizeof(GpuCrashDumpMessage) to work around struct packing issues.
        //        The compiler pads out GpuCrashDumpMessage to 4 bytes when it's included in the payload struct.
        DD_STATIC_CONST Size kMaxGpuCrashDumpDataChunkSize = (kMaxPayloadSizeInBytes - sizeof(uint32));

        ///////////////////////
        // GpuCrashDump Payloads
        DD_NETWORK_STRUCT(GpuCrashNotifyPayload, 4)
        {
            uint32 sizeInBytes;
        };

        DD_CHECK_SIZE(GpuCrashNotifyPayload, 4);

        DD_NETWORK_STRUCT(GpuCrashAcknowledgePayload, 4)
        {
            bool acceptedCrashDump;
            char padding[3];
        };

        DD_CHECK_SIZE(GpuCrashAcknowledgePayload, 4);

        DD_NETWORK_STRUCT(GpuCrashDataChunkPayload, 4)
        {
            uint8 data[kMaxGpuCrashDumpDataChunkSize];
        };

        DD_CHECK_SIZE(GpuCrashDataChunkPayload, kMaxGpuCrashDumpDataChunkSize);

        DD_NETWORK_STRUCT(GpuCrashDataSentinelPayload, 4)
        {
            Result result;
        };

        DD_CHECK_SIZE(GpuCrashDataSentinelPayload, 4);

        DD_NETWORK_STRUCT(GpuCrashDumpPayload, 4)
        {
            GpuCrashDumpMessage command;
            // pad out to 4 bytes for alignment requirements
            char                 padding[3];

            union
            {
                GpuCrashNotifyPayload       notify;
                GpuCrashAcknowledgePayload  acknowledge;
                GpuCrashDataChunkPayload    dataChunk;
                GpuCrashDataSentinelPayload sentinel;
            };
        };

        DD_CHECK_SIZE(GpuCrashDumpPayload, kMaxPayloadSizeInBytes);
    }
}
