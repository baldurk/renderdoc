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

#include "protocols/driverControlClient.h"
#include "msgChannel.h"

#define DRIVERCONTROL_CLIENT_MIN_MAJOR_VERSION 1
#define DRIVERCONTROL_CLIENT_MAX_MAJOR_VERSION 2

namespace DevDriver
{
    namespace DriverControlProtocol
    {
        DriverControlClient::DriverControlClient(IMsgChannel* pMsgChannel)
            : BaseProtocolClient(pMsgChannel, Protocol::DriverControl, DRIVERCONTROL_CLIENT_MIN_MAJOR_VERSION, DRIVERCONTROL_CLIENT_MAX_MAJOR_VERSION)
        {
        }

        DriverControlClient::~DriverControlClient()
        {
        }

        Result DriverControlClient::PauseDriver()
        {
            Result result = Result::Error;

            if (IsConnected())
            {
                DriverControlPayload payload = {};
                payload.command = DriverControlMessage::PauseDriverRequest;

                result = SendPayload(payload);
                if (result == Result::Success)
                {
                    result = ReceivePayload(payload);
                    if (result == Result::Success)
                    {
                        if (payload.command == DriverControlMessage::PauseDriverResponse)
                        {
                            result = payload.pauseDriverResponse.result;
                        }
                        else
                        {
                            // Invalid response payload
                            result = Result::Error;
                        }
                    }
                }
            }

            return result;
        }

        Result DriverControlClient::ResumeDriver()
        {
            Result result = Result::Error;

            if (IsConnected())
            {
                DriverControlPayload payload = {};
                payload.command = DriverControlMessage::ResumeDriverRequest;

                result = SendPayload(payload);
                if (result == Result::Success)
                {
                    result = ReceivePayload(payload);
                    if (result == Result::Success)
                    {
                        if (payload.command == DriverControlMessage::ResumeDriverResponse)
                        {
                            result = payload.resumeDriverResponse.result;
                        }
                        else
                        {
                            // Invalid response payload
                            result = Result::Error;
                        }
                    }
                }
            }

            return result;
        }

        Result DriverControlClient::StepDriver(uint32 numSteps)
        {
            Result result = Result::Error;

            if (IsConnected() && numSteps > 0)
            {
                DriverControlPayload payload = {};
                payload.command = DriverControlMessage::StepDriverRequest;
                payload.stepDriverRequest.count = numSteps;
                result = SendPayload(payload);
                if (result == Result::Success)
                {
                    result = ReceivePayload(payload);
                    if (result == Result::Success)
                    {
                        if (payload.command == DriverControlMessage::StepDriverResponse)
                        {
                            result = payload.resumeDriverResponse.result;
                        }
                        else
                        {
                            // Invalid response payload
                            result = Result::Error;
                        }
                    }
                }
            }

            return result;
        }

        Result DriverControlClient::QueryNumGpus(uint32* pNumGpus)
        {
            Result result = Result::Error;

            if (IsConnected() && (pNumGpus != nullptr))
            {
                DriverControlPayload payload = {};
                payload.command = DriverControlMessage::QueryNumGpusRequest;

                result = SendPayload(payload);
                if (result == Result::Success)
                {
                    result = ReceivePayload(payload);
                    if (result == Result::Success)
                    {
                        if (payload.command == DriverControlMessage::QueryNumGpusResponse)
                        {
                            result = payload.queryNumGpusResponse.result;
                            *pNumGpus = payload.queryNumGpusResponse.numGpus;
                        }
                        else
                        {
                            // Invalid response payload
                            result = Result::Error;
                        }
                    }
                }
            }

            return result;
        }

        Result DriverControlClient::QueryDeviceClockMode(uint32 gpuIndex, DeviceClockMode* pClockMode)
        {
            Result result = Result::Error;

            if (IsConnected() && (pClockMode != nullptr))
            {
                DriverControlPayload payload = {};
                payload.command = DriverControlMessage::QueryDeviceClockModeRequest;
                payload.queryDeviceClockModeRequest.gpuIndex = gpuIndex;

                result = SendPayload(payload);
                if (result == Result::Success)
                {
                    result = ReceivePayload(payload);
                    if (result == Result::Success)
                    {
                        if (payload.command == DriverControlMessage::QueryDeviceClockModeResponse)
                        {
                            result = payload.queryDeviceClockModeResponse.result;
                            if (result == Result::Success)
                            {
                                *pClockMode = payload.queryDeviceClockModeResponse.mode;
                            }
                        }
                        else
                        {
                            // Invalid response payload
                            result = Result::Error;
                        }
                    }
                }
            }

            return result;
        }

        Result DriverControlClient::SetDeviceClockMode(uint32 gpuIndex, DeviceClockMode clockMode)
        {
            Result result = Result::Error;

            if (IsConnected())
            {
                DriverControlPayload payload = {};
                payload.command = DriverControlMessage::SetDeviceClockModeRequest;
                payload.setDeviceClockModeRequest.mode = clockMode;
                payload.setDeviceClockModeRequest.gpuIndex = gpuIndex;

                result = SendPayload(payload);
                if (result == Result::Success)
                {
                    result = ReceivePayload(payload);
                    if (result == Result::Success)
                    {
                        if (payload.command == DriverControlMessage::SetDeviceClockModeResponse)
                        {
                            result = payload.setDeviceClockModeResponse.result;
                        }
                        else
                        {
                            // Invalid response payload
                            result = Result::Error;
                        }
                    }
                }
            }

            return result;
        }

        Result DriverControlClient::QueryDeviceClock(uint32 gpuIndex, float* pGpuClock, float* pMemClock)
        {
            Result result = Result::Error;

            if (IsConnected() && (pGpuClock != nullptr) && (pMemClock != nullptr))
            {
                DriverControlPayload payload = {};
                payload.command = DriverControlMessage::QueryDeviceClockRequest;
                payload.queryDeviceClockRequest.gpuIndex = gpuIndex;

                result = SendPayload(payload);
                if (result == Result::Success)
                {
                    result = ReceivePayload(payload);
                    if (result == Result::Success)
                    {
                        if (payload.command == DriverControlMessage::QueryDeviceClockResponse)
                        {
                            result = payload.queryDeviceClockResponse.result;
                            if (result == Result::Success)
                            {
                                *pGpuClock = payload.queryDeviceClockResponse.gpuClock;
                                *pMemClock = payload.queryDeviceClockResponse.memClock;
                            }
                        }
                        else
                        {
                            // Invalid response payload
                            result = Result::Error;
                        }
                    }
                }
            }

            return result;
        }

        Result DriverControlClient::QueryMaxDeviceClock(uint32 gpuIndex, float* pMaxGpuClock, float* pMaxMemClock)
        {
            Result result = Result::Error;

            if (IsConnected() && (pMaxGpuClock != nullptr) && (pMaxMemClock != nullptr))
            {
                DriverControlPayload payload = {};
                payload.command = DriverControlMessage::QueryMaxDeviceClockRequest;
                payload.queryMaxDeviceClockRequest.gpuIndex = gpuIndex;

                result = SendPayload(payload);
                if (result == Result::Success)
                {
                    result = ReceivePayload(payload);
                    if (result == Result::Success)
                    {
                        if (payload.command == DriverControlMessage::QueryMaxDeviceClockResponse)
                        {
                            result = payload.queryMaxDeviceClockResponse.result;
                            if (result == Result::Success)
                            {
                                *pMaxGpuClock = payload.queryMaxDeviceClockResponse.maxGpuClock;
                                *pMaxMemClock = payload.queryMaxDeviceClockResponse.maxMemClock;
                            }
                        }
                        else
                        {
                            // Invalid response payload
                            result = Result::Error;
                        }
                    }
                }
            }

            return result;
        }

        Result DriverControlClient::QueryDriverStatus(DriverStatus* pDriverStatus)
        {
            Result result = Result::Error;

            if (IsConnected() && (pDriverStatus != nullptr))
            {
                DriverControlPayload payload = {};
                payload.command = DriverControlMessage::QueryDriverStatusRequest;

                result = SendPayload(payload);
                if (result == Result::Success)
                {
                    result = ReceivePayload(payload);
                    if (result == Result::Success)
                    {
                        if (payload.command == DriverControlMessage::QueryDriverStatusResponse)
                        {
                            *pDriverStatus = payload.queryDriverStatusResponse.status;
                        }
                        else
                        {
                            // Invalid response payload
                            result = Result::Error;
                        }
                    }
                }
            }

            return result;
        }

        Result DriverControlClient::WaitForDriverInitialization(uint32 timeoutInMs)
        {
            Result result = Result::VersionMismatch;
            if (GetSessionVersion() >= DRIVERCONTROL_INITIALIZATION_STATUS_VERSION)
            {
                result = Result::Error;
                if (IsConnected())
                {
                    const uint64 startTime = Platform::GetCurrentTimeInMs();
                    const uint64 kQueryDelayInMs = 250;
                    uint64 nextQueryTime = startTime;
                    result = Result::Success;

                    while (result == Result::Success)
                    {
                        const uint64 currentTime = Platform::GetCurrentTimeInMs();
                        const uint64 totalElapsedTime = (currentTime - startTime);
                        if (totalElapsedTime >= timeoutInMs)
                        {
                            result = Result::NotReady;
                        }
                        else if (currentTime >= nextQueryTime)
                        {
                            nextQueryTime = currentTime + kQueryDelayInMs;

                            DriverControlPayload payload = {};
                            payload.command = DriverControlMessage::QueryDriverStatusRequest;

                            result = SendPayload(payload);
                            if (result == Result::Success)
                            {
                                result = ReceivePayload(payload);
                                if (result == Result::Success)
                                {
                                    if (payload.command == DriverControlMessage::QueryDriverStatusResponse)
                                    {
                                        if ((payload.queryDriverStatusResponse.status == DriverStatus::Running) ||
                                            (payload.queryDriverStatusResponse.status == DriverStatus::Paused))
                                        {
                                            break;
                                        }
                                    }
                                    else
                                    {
                                        // Invalid response payload
                                        result = Result::Error;
                                    }
                                }
                            }
                        }
                    }
                }
            }

            return result;
        }
}

} // DevDriver
