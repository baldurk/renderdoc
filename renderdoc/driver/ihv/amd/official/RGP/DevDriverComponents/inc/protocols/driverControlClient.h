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
* @file  driverControlClient.h
* @brief Developer Driver Control Client Interface
***********************************************************************************************************************
*/

#pragma once

#include "baseProtocolClient.h"
#include "protocols/driverControlProtocol.h"

namespace DevDriver
{
    class IMsgChannel;

    namespace DriverControlProtocol
    {
        class DriverControlClient : public BaseProtocolClient
        {
        public:
            explicit DriverControlClient(IMsgChannel* pMsgChannel);
            ~DriverControlClient();

            // Pauses execution inside the driver
            Result PauseDriver();

            // Resumes execution inside the driver
            Result ResumeDriver();

            Result StepDriver(uint32 numSteps);

            // Returns the number of gpus the driver is managing.
            Result QueryNumGpus(uint32* pNumGpus);

            // Returns the current device clock mode
            Result QueryDeviceClockMode(uint32 gpuIndex, DeviceClockMode* pClockMode);

            // Sets the current device clock mode
            Result SetDeviceClockMode(uint32 gpuIndex, DeviceClockMode clockMode);

            // Returns the current device clock values in MHz
            Result QueryDeviceClock(uint32 gpuIndex, float* pGpuClock, float* pMemClock);

            // Returns the max device clock values in MHz
            Result QueryMaxDeviceClock(uint32 gpuIndex, float* pMaxGpuClock, float* pMaxMemClock);

            // Returns the current status of the driver.
            Result QueryDriverStatus(DriverStatus* pDriverStatus);

            // Waits until the driver finishes initialization.
            Result WaitForDriverInitialization(uint32 timeoutInMs);

        private:
        };
    }
} // DevDriver
