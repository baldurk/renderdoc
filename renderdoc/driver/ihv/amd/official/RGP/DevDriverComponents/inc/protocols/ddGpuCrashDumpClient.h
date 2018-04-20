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
* @file  ddGpuCrashDumpClient.h
* @brief Class declaration for the gpu crash dump client
***********************************************************************************************************************
*/

#pragma once

#include "baseProtocolClient.h"
#include "protocols/ddGpuCrashDumpProtocol.h"

namespace DevDriver
{
    class IMsgChannel;

    namespace TransferProtocol
    {
        class ServerBlock;
    }

    namespace GpuCrashDumpProtocol
    {
        class GpuCrashDumpClient : public BaseProtocolClient
        {
        public:
            explicit GpuCrashDumpClient(IMsgChannel* pMsgChannel);
            ~GpuCrashDumpClient();

            // Begins the gpu crash dumping process. Sends a notification to the server and starts
            // transferring crash data.
            // Returns Success if the process starts successfully, Rejected if the server rejects
            // the crash dump and Error otherwise.
            Result BeginGpuCrashDump(const void* pCrashDump, size_t crashDumpSize);

            // Ends the gpu crash dumping process. Waits for the crash dump transfer to finish.
            // Returns Success if the transfer completes successfully, and Error otherwise.
            Result EndGpuCrashDump();

        private:
            uint8* m_pCrashDump;
            uint32 m_crashDumpSize;
            uint32 m_crashDumpBytesSent;
        };
    }
} // DevDriver
