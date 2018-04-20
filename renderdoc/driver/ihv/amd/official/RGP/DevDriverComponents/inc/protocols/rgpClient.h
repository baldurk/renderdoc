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
* @file  rgpClient.h
* @brief Class declaration for RGPClient.
***********************************************************************************************************************
*/

#pragma once

#include "baseProtocolClient.h"

#include "protocols/rgpProtocol.h"

namespace DevDriver
{
    class IMsgChannel;

    namespace RGPProtocol
    {
        typedef void(*TraceDataChunkReceived)(const TraceDataChunk* pChunk, void* pUserdata);

        struct ChunkCallbackInfo
        {
            TraceDataChunkReceived chunkCallback;
            void*                  pUserdata;
        };

        struct ClientTraceParametersInfo
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

        struct BeginTraceInfo
        {
            ClientTraceParametersInfo parameters;   // Parameters for the trace
            ChunkCallbackInfo         callbackInfo; // Callback used to return trace data
        };

        class RGPClient : public BaseProtocolClient
        {
        public:
            explicit RGPClient(IMsgChannel* pMsgChannel);
            ~RGPClient();

            // Requests an RGP trace in the driver. Returns Success if the request was successfully delivered.
            Result BeginTrace(const BeginTraceInfo& traceInfo);

            // Waits until a previously requested trace completes in the driver and returns the result of the trace.
            // If the trace was successful, the number of chunks generated is returned in pNumChunks and the size in
            // bytes of the trace data is returned in pTraceSizeInBytes.
            Result EndTrace(uint32* pNumChunks, uint64* pTraceSizeInBytes);

            // Reads a chunk of trace data from a previous trace that completed successfully. Returns chunk data
            // via the callback provided earlier in BeginTraceInfo.
            Result ReadTraceDataChunk();

            // Aborts a trace in progress.
            Result AbortTrace();

            // Queries the current profiling status of the driver.
            Result QueryProfilingStatus(ProfilingStatus* pStatus);

            // Enables profiling support inside the driver. ExecuteTrace will only succeed if the connected driver
            // has profiling enabled.
            Result EnableProfiling();

        private:
            void ResetState() override;

            enum class TraceState : uint32
            {
                Idle = 0,
                TraceRequested,
                TraceCompleted,
                Error
            };

            struct ClientTraceContext
            {
                TraceState state;
                BeginTraceInfo traceInfo;
                uint32 numChunksReceived;
                uint32 numChunks;
            };

            ClientTraceContext m_traceContext;

            DD_STATIC_CONST uint32 kRGPChunkTimeoutInMs = 3000;
        };
    }
} // DevDriver
