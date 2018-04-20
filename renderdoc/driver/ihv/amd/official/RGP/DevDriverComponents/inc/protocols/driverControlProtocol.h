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

#pragma once

#include "gpuopen.h"

#define DRIVERCONTROL_PROTOCOL_MAJOR_VERSION 2
#define DRIVERCONTROL_PROTOCOL_MINOR_VERSION 0

#define DRIVERCONTROL_INTERFACE_VERSION ((DRIVERCONTROL_INTERFACE_MAJOR_VERSION << 16) | DRIVERCONTROL_INTERFACE_MINOR_VERSION)

#define DRIVERCONTROL_PROTOCOL_MINIMUM_MAJOR_VERSION 1

/*
***********************************************************************************************************************
*| Version | Change Description                                                                                       |
*| ------- | ---------------------------------------------------------------------------------------------------------|
*|  2.0    | Added initialization time driver status values and a terminate driver command.                           |
*|  1.0    | Initial version                                                                                          |
***********************************************************************************************************************
*/

#define DRIVERCONTROL_INITIALIZATION_STATUS_VERSION 2
#define DRIVERCONTROL_INITIAL_VERSION 1

namespace DevDriver
{
    namespace DriverControlProtocol
    {
        ///////////////////////
        // DriverControl Protocol
        enum struct DriverControlMessage : MessageCode
        {
            Unknown = 0,
            PauseDriverRequest,
            PauseDriverResponse,
            ResumeDriverRequest,
            ResumeDriverResponse,
            QueryNumGpusRequest,
            QueryNumGpusResponse,
            QueryDeviceClockModeRequest,
            QueryDeviceClockModeResponse,
            SetDeviceClockModeRequest,
            SetDeviceClockModeResponse,
            QueryDeviceClockRequest,
            QueryDeviceClockResponse,
            QueryMaxDeviceClockRequest,
            QueryMaxDeviceClockResponse,
            QueryDriverStatusRequest,
            QueryDriverStatusResponse,
            StepDriverRequest,
            StepDriverResponse,
            Count
        };

        ///////////////////////
        // DriverControl Types
        enum struct DeviceClockMode : uint32
        {
            Unknown = 0,
            Default,
            Profiling,
            MinimumMemory,
            MinimumEngine,
            Peak,
            Count
        };

        enum struct DriverStatus : uint32
        {
            Running = 0,
            Paused,
            HaltedOnStart,
            EarlyInit,
            LateInit,
            Count
        };

        ///////////////////////
        // DriverControl Payloads
        DD_NETWORK_STRUCT(PauseDriverResponsePayload, 4)
        {
            Result result;
        };

        DD_CHECK_SIZE(PauseDriverResponsePayload, 4);

        DD_NETWORK_STRUCT(ResumeDriverResponsePayload, 4)
        {
            Result result;
        };

        DD_CHECK_SIZE(ResumeDriverResponsePayload, 4);

        DD_NETWORK_STRUCT(QueryNumGpusResponsePayload, 4)
        {
            Result result;
            uint32 numGpus;
        };

        DD_CHECK_SIZE(QueryNumGpusResponsePayload, 8);

        DD_NETWORK_STRUCT(QueryDeviceClockModeRequestPayload, 4)
        {
            uint32 gpuIndex;
        };

        DD_CHECK_SIZE(QueryDeviceClockModeRequestPayload, 4);

        DD_NETWORK_STRUCT(QueryDeviceClockModeResponsePayload, 4)
        {
            Result result;
            DeviceClockMode mode;
        };

        DD_CHECK_SIZE(QueryDeviceClockModeResponsePayload, 8);

        DD_NETWORK_STRUCT(SetDeviceClockModeRequestPayload, 4)
        {
            uint32 gpuIndex;
            DeviceClockMode mode;
        };

        DD_CHECK_SIZE(SetDeviceClockModeRequestPayload, 8);

        DD_NETWORK_STRUCT(SetDeviceClockModeResponsePayload, 4)
        {
            Result result;
        };

        DD_CHECK_SIZE(SetDeviceClockModeResponsePayload, 4);

        DD_NETWORK_STRUCT(QueryDeviceClockRequestPayload, 4)
        {
            uint32 gpuIndex;
        };

        DD_CHECK_SIZE(QueryDeviceClockRequestPayload, 4);

        DD_NETWORK_STRUCT(QueryDeviceClockResponsePayload, 4)
        {
            Result result;
            float gpuClock;
            float memClock;
        };

        DD_CHECK_SIZE(QueryDeviceClockResponsePayload, 12);

        DD_NETWORK_STRUCT(QueryMaxDeviceClockRequestPayload, 4)
        {
            uint32 gpuIndex;
        };

        DD_CHECK_SIZE(QueryMaxDeviceClockRequestPayload, 4);

        DD_NETWORK_STRUCT(QueryMaxDeviceClockResponsePayload, 4)
        {
            Result result;
            float maxGpuClock;
            float maxMemClock;
        };

        DD_CHECK_SIZE(QueryMaxDeviceClockResponsePayload, 12);

        DD_NETWORK_STRUCT(QueryDriverStatusResponsePayload, 4)
        {
            DriverStatus status;
        };

        DD_CHECK_SIZE(QueryDriverStatusResponsePayload, 4);

        DD_NETWORK_STRUCT(StepDriverRequestPayload, 4)
        {
            uint32 count;
        };

        DD_CHECK_SIZE(StepDriverRequestPayload, 4);

        DD_NETWORK_STRUCT(StepDriverResponsePayload, 4)
        {
            Result result;
        };

        DD_CHECK_SIZE(StepDriverResponsePayload, 4);

        DD_NETWORK_STRUCT(DriverControlPayload, 4)
        {
            DriverControlMessage command;
            // pad out to 4 bytes for alignment requirements
            char                 padding[3];

            union
            {
                PauseDriverResponsePayload          pauseDriverResponse;
                ResumeDriverResponsePayload         resumeDriverResponse;
                QueryNumGpusResponsePayload         queryNumGpusResponse;
                QueryDeviceClockModeRequestPayload  queryDeviceClockModeRequest;
                QueryDeviceClockModeResponsePayload queryDeviceClockModeResponse;
                SetDeviceClockModeRequestPayload    setDeviceClockModeRequest;
                SetDeviceClockModeResponsePayload   setDeviceClockModeResponse;
                QueryDeviceClockRequestPayload      queryDeviceClockRequest;
                QueryDeviceClockResponsePayload     queryDeviceClockResponse;
                QueryMaxDeviceClockResponsePayload  queryMaxDeviceClockResponse;
                QueryMaxDeviceClockRequestPayload   queryMaxDeviceClockRequest;
                QueryDriverStatusResponsePayload    queryDriverStatusResponse;
                StepDriverRequestPayload            stepDriverRequest;
                StepDriverResponsePayload           stepDriverResponse;
            };
        };

        DD_CHECK_SIZE(DriverControlPayload, 16);
    }
}
