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
* @file  driverControlServer.h
* @brief Class declaration for DriverControlServer.
***********************************************************************************************************************
*/

#pragma once

#include "baseProtocolServer.h"
#include "driverControlProtocol.h"

namespace DevDriver
{
    namespace DriverControlProtocol
    {
        DD_STATIC_CONST uint32 kMaxNumGpus = 16;

        typedef Result(*QueryDeviceClockCallback)(uint32 gpuIndex, float* pGpuClock, float* pMemClock, void* pUserdata);
        typedef Result(*QueryMaxDeviceClockCallback)(uint32 gpuIndex, float* pMaxGpuClock, float* pMaxMemClock, void* pUserdata);
        typedef Result(*SetDeviceClockModeCallback)(uint32 gpuIndex, DeviceClockMode clockMode, void* pUserdata);

        struct DeviceClockCallbackInfo
        {
            QueryDeviceClockCallback    queryClockCallback;
            QueryMaxDeviceClockCallback queryMaxClockCallback;
            SetDeviceClockModeCallback  setCallback;
            void*                       pUserdata;
        };

        class DriverControlServer : public BaseProtocolServer
        {
        public:
            explicit DriverControlServer(IMsgChannel* pMsgChannel);
            ~DriverControlServer();

            void Finalize() override;

            bool AcceptSession(const SharedPointer<ISession>& pSession) override;
            void SessionEstablished(const SharedPointer<ISession>& pSession) override;
            void UpdateSession(const SharedPointer<ISession>& pSession) override;
            void SessionTerminated(const SharedPointer<ISession>& pSession, Result terminationReason) override;

            void WaitForDriverResume();

            bool IsDriverInitialized() const;
            void FinishDriverInitialization();

            DriverStatus QueryDriverStatus();

            void PauseDriver();
            void ResumeDriver();

            void SetNumGpus(uint32 numGpus);
            void SetDeviceClockCallback(const DeviceClockCallbackInfo& deviceClockCallbackInfo);

            uint32 GetNumGpus();
            DeviceClockMode GetDeviceClockMode(uint32 gpuIndex);

        private:
            void LockData();
            void UnlockData();
            void WaitForDriverStart(uint64 timeoutInMs);

            Platform::Mutex m_mutex;
            DriverStatus m_driverStatus;
            Platform::Event m_driverResumedEvent;

            uint32 m_numGpus;
            DeviceClockMode m_deviceClockModes[kMaxNumGpus];
            DeviceClockCallbackInfo m_deviceClockCallbackInfo;
            Platform::Atomic m_numSessions;
            Platform::Atomic m_stepCounter;

            DD_STATIC_CONST uint32 kBroadcastIntervalInMs = 100;
        };
    }
} // DevDriver
