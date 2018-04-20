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

#include "protocols/driverControlServer.h"
#include "msgChannel.h"
#include <cstring>
#include "protocols/systemProtocols.h"

#define DRIVERCONTROL_SERVER_MIN_MAJOR_VERSION 1
#define DRIVERCONTROL_SERVER_MAX_MAJOR_VERSION 2

namespace DevDriver
{
    namespace DriverControlProtocol
    {
        enum class SessionState
        {
            ReceivePayload = 0,
            ProcessPayload,
            SendPayload,
            StepDriver
        };

        struct DriverControlSession
        {
            SessionState state;
            DriverControlPayload payload;
        };

        DriverControlServer::DriverControlServer(IMsgChannel* pMsgChannel)
            : BaseProtocolServer(pMsgChannel, Protocol::DriverControl, DRIVERCONTROL_SERVER_MIN_MAJOR_VERSION, DRIVERCONTROL_SERVER_MAX_MAJOR_VERSION)
            , m_driverStatus(DriverStatus::EarlyInit)
            , m_driverResumedEvent(true)
            , m_numGpus(0)
            , m_deviceClockCallbackInfo()
            , m_numSessions(0)
            , m_stepCounter(0)
        {
            DD_ASSERT(m_pMsgChannel != nullptr);

            for (uint32 gpuIndex = 0; gpuIndex < kMaxNumGpus; ++gpuIndex)
            {
                m_deviceClockModes[gpuIndex] = DeviceClockMode::Default;
            }
        }

        DriverControlServer::~DriverControlServer()
        {
        }

        void DriverControlServer::Finalize()
        {
            DD_STATIC_CONST uint32 kDefaultDriverStartTimeoutMs = 1000;
            WaitForDriverStart(kDefaultDriverStartTimeoutMs);

            LockData();
            BaseProtocolServer::Finalize();
            UnlockData();
        }

        bool DriverControlServer::AcceptSession(const SharedPointer<ISession>& pSession)
        {
            DD_UNUSED(pSession);
            return true;
        }

        void DriverControlServer::SessionEstablished(const SharedPointer<ISession>& pSession)
        {
            DD_UNUSED(pSession);

            // Allocate session data for the newly established session
            DriverControlSession* pSessionData = DD_NEW(DriverControlSession, m_pMsgChannel->GetAllocCb())();
            pSessionData->state = SessionState::ReceivePayload;
            pSessionData->payload = {};
            Platform::AtomicIncrement(&m_numSessions);
            pSession->SetUserData(pSessionData);
        }

        void DriverControlServer::UpdateSession(const SharedPointer<ISession>& pSession)
        {
            DriverControlSession* pSessionData = reinterpret_cast<DriverControlSession*>(pSession->GetUserData());

            switch (pSessionData->state)
            {
                case SessionState::ReceivePayload:
                {
                    uint32 bytesReceived = 0;
                    Result result = pSession->Receive(sizeof(pSessionData->payload), &pSessionData->payload, &bytesReceived, kNoWait);

                    if (result == Result::Success)
                    {
                        DD_ASSERT(sizeof(pSessionData->payload) == bytesReceived);
                        pSessionData->state = SessionState::ProcessPayload;
                    }

                    break;
                }

                case SessionState::ProcessPayload:
                {
                    switch (pSessionData->payload.command)
                    {
                        case DriverControlMessage::PauseDriverRequest:
                        {
                            Result result = Result::Error;

                            // Only allow pausing if we're already in the running state.
                            if (m_driverStatus == DriverStatus::Running)
                            {
                                m_driverStatus = DriverStatus::Paused;
                                m_driverResumedEvent.Clear();
                                result = Result::Success;
                            }

                            pSessionData->payload.command = DriverControlMessage::PauseDriverResponse;
                            pSessionData->payload.pauseDriverResponse.result = result;

                            pSessionData->state = SessionState::SendPayload;

                            break;
                        }

                        case DriverControlMessage::ResumeDriverRequest:
                        {
                            Result result = Result::Error;

                            // Allow resuming the driver from the initial "halted on start" state and from the regular paused state.
                            if ((m_driverStatus == DriverStatus::HaltedOnStart) || (m_driverStatus == DriverStatus::Paused))
                            {
                                // If we're resuming from the paused state, move to the running state, otherwise we're moving from
                                // halt on start to the late initialization phase.
                                m_driverStatus = (m_driverStatus == DriverStatus::Paused) ? DriverStatus::Running : DriverStatus::LateInit;
                                m_driverResumedEvent.Signal();
                                result = Result::Success;
                            }

                            pSessionData->payload.command = DriverControlMessage::ResumeDriverResponse;
                            pSessionData->payload.resumeDriverResponse.result = result;

                            pSessionData->state = SessionState::SendPayload;

                            break;
                        }

                        case DriverControlMessage::QueryDeviceClockModeRequest:
                        {
                            Result result = Result::Error;

                            LockData();

                            const uint32 gpuIndex = pSessionData->payload.queryDeviceClockModeRequest.gpuIndex;
                            if (gpuIndex < m_numGpus)
                            {
                                pSessionData->payload.queryDeviceClockModeResponse.mode = m_deviceClockModes[gpuIndex];
                                result = Result::Success;
                            }

                            UnlockData();

                            pSessionData->payload.command = DriverControlMessage::QueryDeviceClockModeResponse;
                            pSessionData->payload.queryDeviceClockModeResponse.result = result;

                            pSessionData->state = SessionState::SendPayload;

                            break;
                        }

                        case DriverControlMessage::SetDeviceClockModeRequest:
                        {
                            Result result = Result::Error;

                            LockData();

                            const uint32 gpuIndex = pSessionData->payload.setDeviceClockModeRequest.gpuIndex;
                            if (gpuIndex < m_numGpus)
                            {
                                if (m_deviceClockCallbackInfo.setCallback != nullptr)
                                {
                                    const DeviceClockMode clockMode = pSessionData->payload.setDeviceClockModeRequest.mode;

                                    result = m_deviceClockCallbackInfo.setCallback(gpuIndex,
                                                                                   clockMode,
                                                                                   m_deviceClockCallbackInfo.pUserdata);
                                    if (result == Result::Success)
                                    {
                                        // Update the current clock mode
                                        m_deviceClockModes[gpuIndex] = clockMode;
                                    }
                                }
                            }

                            UnlockData();

                            pSessionData->payload.command = DriverControlMessage::SetDeviceClockModeResponse;
                            pSessionData->payload.setDeviceClockModeResponse.result = result;

                            pSessionData->state = SessionState::SendPayload;

                            break;
                        }

                        case DriverControlMessage::QueryDeviceClockRequest:
                        {
                            Result result = Result::Error;

                            float gpuClock = 0.0f;
                            float memClock = 0.0f;

                            LockData();

                            const uint32 gpuIndex = pSessionData->payload.queryDeviceClockRequest.gpuIndex;
                            if (gpuIndex < m_numGpus)
                            {
                                if (m_deviceClockCallbackInfo.queryClockCallback != nullptr)
                                {
                                    result = m_deviceClockCallbackInfo.queryClockCallback(gpuIndex,
                                                                                          &gpuClock,
                                                                                          &memClock,
                                                                                          m_deviceClockCallbackInfo.pUserdata);
                                }
                            }

                            UnlockData();

                            pSessionData->payload.command = DriverControlMessage::QueryDeviceClockResponse;
                            pSessionData->payload.queryDeviceClockResponse.result = result;
                            pSessionData->payload.queryDeviceClockResponse.gpuClock = gpuClock;
                            pSessionData->payload.queryDeviceClockResponse.memClock = memClock;

                            pSessionData->state = SessionState::SendPayload;

                            break;
                        }

                        case DriverControlMessage::QueryMaxDeviceClockRequest:
                        {
                            Result result = Result::Error;

                            float maxGpuClock = 0.0f;
                            float maxMemClock = 0.0f;

                            LockData();

                            const uint32 gpuIndex = pSessionData->payload.queryMaxDeviceClockRequest.gpuIndex;
                            if (gpuIndex < m_numGpus)
                            {
                                if (m_deviceClockCallbackInfo.queryMaxClockCallback != nullptr)
                                {
                                    result = m_deviceClockCallbackInfo.queryMaxClockCallback(gpuIndex,
                                                                                             &maxGpuClock,
                                                                                             &maxMemClock,
                                                                                             m_deviceClockCallbackInfo.pUserdata);
                                }
                            }

                            UnlockData();

                            pSessionData->payload.command = DriverControlMessage::QueryMaxDeviceClockResponse;
                            pSessionData->payload.queryMaxDeviceClockResponse.result = result;
                            pSessionData->payload.queryMaxDeviceClockResponse.maxGpuClock = maxGpuClock;
                            pSessionData->payload.queryMaxDeviceClockResponse.maxMemClock = maxMemClock;

                            pSessionData->state = SessionState::SendPayload;

                            break;
                        }

                        case DriverControlMessage::QueryNumGpusRequest:
                        {
                            Result result = Result::Success;

                            LockData();

                            const uint32 numGpus = m_numGpus;

                            UnlockData();

                            pSessionData->payload.command = DriverControlMessage::QueryNumGpusResponse;
                            pSessionData->payload.queryNumGpusResponse.result = result;
                            pSessionData->payload.queryNumGpusResponse.numGpus = numGpus;

                            pSessionData->state = SessionState::SendPayload;

                            break;
                        }

                        case DriverControlMessage::QueryDriverStatusRequest:
                        {
                            LockData();

                            const DriverStatus driverStatus = m_driverStatus;

                            UnlockData();

                            pSessionData->payload.command = DriverControlMessage::QueryDriverStatusResponse;

                            // On older versions, override EarlyInit and LateInit to the running state to maintain backwards compatibility.
                            if ((pSession->GetVersion() < DRIVERCONTROL_INITIALIZATION_STATUS_VERSION) &&
                                ((driverStatus == DriverStatus::EarlyInit) || (driverStatus == DriverStatus::LateInit)))
                            {
                                pSessionData->payload.queryDriverStatusResponse.status = DriverStatus::Running;
                            }
                            else
                            {
                                pSessionData->payload.queryDriverStatusResponse.status = driverStatus;
                            }

                            pSessionData->state = SessionState::SendPayload;

                            break;
                        }
                        case DriverControlMessage::StepDriverRequest:
                        {
                            if (m_driverStatus == DriverStatus::Paused && m_stepCounter == 0)
                            {
                                int32 count = Platform::Max((int32) pSessionData->payload.stepDriverRequest.count, 1);
                                Platform::AtomicAdd(&m_stepCounter, count);
                                DD_PRINT(LogLevel::Verbose, "[DriverControlServer] Stepping driver %i frames", m_stepCounter);
                                pSessionData->state = SessionState::StepDriver;
                                ResumeDriver();
                            }
                            else
                            {
                                pSessionData->payload.command = DriverControlMessage::StepDriverResponse;
                                pSessionData->payload.stepDriverResponse.result = Result::Error;
                                pSessionData->state = SessionState::SendPayload;
                            }
                            break;
                        }
                        default:
                        {
                            DD_UNREACHABLE();
                            break;
                        }
                    }

                    break;
                }

                case SessionState::SendPayload:
                {
                    Result result = pSession->Send(sizeof(pSessionData->payload), &pSessionData->payload, kNoWait);
                    if (result == Result::Success)
                    {
                        pSessionData->state = SessionState::ReceivePayload;
                    }

                    break;
                }

                case SessionState::StepDriver:
                {
                    if (m_driverStatus == DriverStatus::Paused && m_stepCounter == 0)
                    {
                        pSessionData->payload.command = DriverControlMessage::StepDriverResponse;
                        pSessionData->payload.stepDriverResponse.result = Result::Success;
                        pSessionData->state = SessionState::SendPayload;
                    }
                    break;
                }

                default:
                {
                    DD_UNREACHABLE();
                    break;
                }
            }
        }

        void DriverControlServer::SessionTerminated(const SharedPointer<ISession>& pSession, Result terminationReason)
        {
            DD_UNUSED(terminationReason);
            DriverControlSession *pDriverControlSession = reinterpret_cast<DriverControlSession*>(pSession->SetUserData(nullptr));

            // Free the session data
            if (pDriverControlSession != nullptr)
            {
                Platform::AtomicDecrement(&m_numSessions);
                DD_DELETE(pDriverControlSession, m_pMsgChannel->GetAllocCb());
            }
        }

        void DriverControlServer::WaitForDriverResume()
        {
            if ((m_driverStatus == DriverStatus::Running) & (m_stepCounter > 0))
            {
                long stepsRemaining = Platform::AtomicDecrement(&m_stepCounter);
                DD_PRINT(LogLevel::Verbose, "[DriverControlServer] %i frames remaining", stepsRemaining);
                if (stepsRemaining == 0)
                {
                    PauseDriver();
                }
            }

            if (m_driverStatus == DriverStatus::Paused)
            {
                Result waitResult = Result::NotReady;
                while (waitResult == Result::NotReady)
                {
                    if (m_numSessions == 0)
                    {
                        const ClientInfoStruct &clientInfo = m_pMsgChannel->GetClientInfo();
                        ClientMetadata filter = {};

                        m_pMsgChannel->Send(kBroadcastClientId,
                                            Protocol::System,
                                            static_cast<MessageCode>(SystemProtocol::SystemMessage::Halted),
                                            filter,
                                            sizeof(ClientInfoStruct),
                                            &clientInfo);
                        DD_PRINT(LogLevel::Verbose, "[DriverControlServer] Sent system halted message");
                    }
                    waitResult = m_driverResumedEvent.Wait(kBroadcastIntervalInMs);
                }
                DD_ASSERT(m_driverStatus == DriverStatus::Running);
            }
        }

        bool DriverControlServer::IsDriverInitialized() const
        {
            // The running and paused states can only be reached after the driver has fully initialized.
            return ((m_driverStatus == DriverStatus::Running) || (m_driverStatus == DriverStatus::Paused));
        }

        void DriverControlServer::FinishDriverInitialization()
        {
            if (m_driverStatus == DriverStatus::LateInit)
            {
                DD_PRINT(LogLevel::Verbose, "[DriverControlServer] Driver initialization finished\n");
                m_driverStatus = DriverStatus::Running;
            }
        }

        DriverStatus DriverControlServer::QueryDriverStatus()
        {
            Platform::LockGuard<Platform::Mutex> lock(m_mutex);

            const DriverStatus driverStatus = m_driverStatus;

            return driverStatus;
        }

        void DriverControlServer::PauseDriver()
        {
            if ((m_driverStatus == DriverStatus::Running) || (m_driverStatus == DriverStatus::EarlyInit))
            {
                m_driverStatus = (m_driverStatus == DriverStatus::Running) ? DriverStatus::Paused : DriverStatus::HaltedOnStart;
                DD_PRINT(LogLevel::Verbose, "[DriverControlServer] Paused driver\n");
                m_driverResumedEvent.Clear();
            }
        }

        void DriverControlServer::ResumeDriver()
        {
            if ((m_driverStatus == DriverStatus::Paused) || (m_driverStatus == DriverStatus::HaltedOnStart))
            {
                m_driverStatus = (m_driverStatus == DriverStatus::Paused) ? DriverStatus::Running : DriverStatus::LateInit;
                DD_PRINT(LogLevel::Verbose, "[DriverControlServer] Resumed driver\n");
                m_driverResumedEvent.Signal();
            }
        }

        void DriverControlServer::SetDeviceClockCallback(const DeviceClockCallbackInfo& deviceClockCallbackInfo)
        {
            LockData();

            m_deviceClockCallbackInfo = deviceClockCallbackInfo;

            UnlockData();
        }

        void DriverControlServer::SetNumGpus(uint32 numGpus)
        {
            // Make sure the new number is less than or equal to the max.
            DD_ASSERT(numGpus <= kMaxNumGpus);

            LockData();

            m_numGpus = numGpus;

            UnlockData();
        }

        uint32 DriverControlServer::GetNumGpus()
        {
            LockData();

            uint32 numGpus = m_numGpus;

            UnlockData();

            return numGpus;
        }

        DeviceClockMode DriverControlServer::GetDeviceClockMode(uint32 gpuIndex)
        {
            LockData();

            DD_ASSERT(gpuIndex < m_numGpus);

            DeviceClockMode clockMode = m_deviceClockModes[gpuIndex];

            UnlockData();

            return clockMode;
        }

        void DriverControlServer::LockData()
        {
            m_mutex.Lock();
        }

        void DriverControlServer::UnlockData()
        {
            m_mutex.Unlock();
        }

        void DriverControlServer::WaitForDriverStart(uint64 timeoutInMs)
        {
            ClientId clientId = kBroadcastClientId;

            if (m_driverStatus == DriverStatus::EarlyInit)
            {
                ClientMetadata filter = {};
                filter.status |= static_cast<StatusFlags>(ClientStatusFlags::HaltOnConnect);
                if (m_pMsgChannel->FindFirstClient(filter, &clientId, kBroadcastIntervalInMs) == Result::Success)
                {
                    DD_ASSERT(clientId != kBroadcastClientId);
                    DD_PRINT(LogLevel::Verbose, "[DriverControlServer] Found client requesting driver halt: %u", clientId);
                    PauseDriver();
                }
            }

            if (m_driverStatus == DriverStatus::HaltedOnStart)
            {
                Result waitResult = Result::NotReady;
                uint64 startTime = Platform::GetCurrentTimeInMs();
                uint64 currentTime = startTime;

                while (waitResult == Result::NotReady)
                {
                    if (m_numSessions == 0)
                    {
                        if ((currentTime - startTime) > timeoutInMs)
                        {
                            ResumeDriver();
                            break;
                        }

                        const ClientInfoStruct &clientInfo = m_pMsgChannel->GetClientInfo();
                        ClientMetadata filter = {};

                        m_pMsgChannel->Send(clientId,
                                            Protocol::System,
                                            static_cast<MessageCode>(SystemProtocol::SystemMessage::Halted),
                                            filter,
                                            sizeof(ClientInfoStruct),
                                            &clientInfo);

                        DD_PRINT(LogLevel::Verbose, "[DriverControlServer] Sent system halted message");
                    }
                    waitResult = m_driverResumedEvent.Wait(kBroadcastIntervalInMs);
                    currentTime = Platform::GetCurrentTimeInMs();
                }
            }
            else
            {
                // If we don't need to halt on start, just skip straight to the late init phase.
                m_driverStatus = DriverStatus::LateInit;
            }
        }
    }
} // DevDriver
