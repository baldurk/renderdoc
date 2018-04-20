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

#include "protocols/ddGpuCrashDumpClient.h"
#include "ddTransferManager.h"
#include "msgChannel.h"

#define GPUCRASHDUMP_CLIENT_MIN_MAJOR_VERSION 1
#define GPUCRASHDUMP_CLIENT_MAX_MAJOR_VERSION 1

namespace DevDriver
{
    namespace GpuCrashDumpProtocol
    {
        GpuCrashDumpClient::GpuCrashDumpClient(IMsgChannel* pMsgChannel)
            : BaseProtocolClient(pMsgChannel, Protocol::GpuCrashDump, GPUCRASHDUMP_CLIENT_MIN_MAJOR_VERSION, GPUCRASHDUMP_CLIENT_MAX_MAJOR_VERSION)
            , m_pCrashDump(nullptr)
            , m_crashDumpSize(0)
            , m_crashDumpBytesSent(0)
        {
        }

        GpuCrashDumpClient::~GpuCrashDumpClient()
        {
        }

        Result GpuCrashDumpClient::BeginGpuCrashDump(const void* pCrashDump, size_t crashDumpSize)
        {
            Result result = Result::Error;

            // Make sure we're connected and aren't already in the middle of reporting a crash.
            if (IsConnected() && (m_pCrashDump == nullptr))
            {
                // Attempt to acquire memory for the crash data.
                m_pCrashDump = reinterpret_cast<uint8*>(DD_MALLOC(crashDumpSize,
                                                                  DD_DEFAULT_ALIGNMENT,
                                                                  m_pMsgChannel->GetAllocCb()));
                if (m_pCrashDump != nullptr)
                {
                    // Copy the crash data into the memory.
                    memcpy(m_pCrashDump, pCrashDump, crashDumpSize);

                    // Send a notification payload to the server.
                    GpuCrashDumpPayload payload = {};
                    payload.command             = GpuCrashDumpMessage::GpuCrashNotify;
                    payload.notify.sizeInBytes  = static_cast<uint32>(crashDumpSize); // @todo: Support 4GB+ crashes?

                    // Exchange messages. Make sure we get the correct command back.
                    result = Transact(payload, payload);
                    if ((result == Result::Success) && (payload.command == GpuCrashDumpMessage::GpuCrashAcknowledge))
                    {
                        // Check if the server wants the crash dump.
                        if (payload.acknowledge.acceptedCrashDump)
                        {
                            // The notification was successfully acknowledged. Save the crash data size and reset the
                            // bytes sent.
                            m_crashDumpSize      = static_cast<uint32>(crashDumpSize);
                            m_crashDumpBytesSent = 0;

                            result = Result::Success;
                        }
                        else
                        {
                            // The server rejected the crash notification. Release the crash data memory.
                            DD_FREE(m_pCrashDump, m_pMsgChannel->GetAllocCb());
                            m_pCrashDump = nullptr;

                            result = Result::Rejected;
                        }
                    }
                    else
                    {
                        // The server did not acknowledge the crash notification. Release the crash data memory.
                        DD_FREE(m_pCrashDump, m_pMsgChannel->GetAllocCb());
                        m_pCrashDump = nullptr;

                        result = Result::Error;
                    }
                }
                else
                {
                    // Failed to acquire memory for crash data.
                    result = Result::Error;
                }
            }

            return result;
        }

        Result GpuCrashDumpClient::EndGpuCrashDump()
        {
            Result result = Result::Error;

            // We should always have valid crash data memory if we're in the middle of the gpu crash process.
            if (m_pCrashDump != nullptr)
            {
                // We should never have valid crash data memory when we've finished transferring all the crash data.
                DD_ASSERT(m_crashDumpBytesSent < m_crashDumpSize);

                GpuCrashDumpPayload payload = {};
                payload.command = GpuCrashDumpMessage::GpuCrashDataChunk;

                // Send a new crash data chunk.
                // Calculate the number of bytes to send.
                // We need to send a partial chunk if the number of bytes sent + the data chunk size would
                // be larger than the number of total bytes.
                const uint32 bytesToSend =
                    ((m_crashDumpBytesSent + kMaxGpuCrashDumpDataChunkSize > m_crashDumpSize) ?
                     (m_crashDumpSize - m_crashDumpBytesSent)                                 :
                     kMaxGpuCrashDumpDataChunkSize);

                memcpy(payload.dataChunk.data, m_pCrashDump + m_crashDumpBytesSent, bytesToSend);

                result = SendPayload(payload);

                if (result == Result::Success)
                {
                    // We successfully sent a chunk. Update the number of bytes sent.
                    m_crashDumpBytesSent += bytesToSend;

                    // Make sure we haven't somehow sent too much data.
                    DD_ASSERT(m_crashDumpBytesSent <= m_crashDumpSize);

                    // If we've sent all the data, send the sentinel.
                    if (m_crashDumpBytesSent == m_crashDumpSize)
                    {
                        payload.command = GpuCrashDumpMessage::GpuCrashDataSentinel;
                        payload.sentinel.result = Result::Success;

                        result = SendPayload(payload);

                        // If we successfully sent the sentinel, return end of stream.
                        if (result == Result::Success)
                        {
                            result = Result::EndOfStream;
                        }
                        else
                        {
                            // If we fail to send the sentinel, return an error.
                            result = Result::Error;
                        }
                    }
                    else
                    {
                        // We still have more chunks to send but we sent a chunk successfully. Return the existing success result.
                    }
                }
                else
                {
                    // We failed to send a chunk, return an error.
                    result = Result::Error;
                }

                // Release the crash data memory before returning if there are no more chunks.
                if (result != Result::Success)
                {
                    DD_FREE(m_pCrashDump, m_pMsgChannel->GetAllocCb());
                    m_pCrashDump = nullptr;
                }
            }

            return result;
        }
}

} // DevDriver
