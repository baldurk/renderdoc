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

#include "protocols/ddGpuCrashDumpServer.h"
#include "msgChannel.h"
#include "ddTransferManager.h"

#define GPUCRASHDUMP_SERVER_MIN_MAJOR_VERSION 1
#define GPUCRASHDUMP_SERVER_MAX_MAJOR_VERSION 1

namespace DevDriver
{
    namespace GpuCrashDumpProtocol
    {
        // Session state enumeration
        enum class SessionState
        {
            WaitForCrashDump = 0,
            SendAcknowledgement,
            TransferCrashDump
        };

        // Session object
        struct GpuCrashDumpSession
        {
            SessionState state;
            GpuCrashDumpPayload payload;
            uint32 crashDataTotalSizeInBytes;
            uint32 crashDataBytesReceived;
            void* pUserdata;
        };

        GpuCrashDumpServer::GpuCrashDumpServer(IMsgChannel* pMsgChannel)
            : BaseProtocolServer(pMsgChannel, Protocol::GpuCrashDump, GPUCRASHDUMP_SERVER_MIN_MAJOR_VERSION, GPUCRASHDUMP_SERVER_MAX_MAJOR_VERSION)
            , m_pCrashDumpHandler(nullptr)
            , m_numSessions(0)
        {
        }

        GpuCrashDumpServer::~GpuCrashDumpServer()
        {
        }

        bool GpuCrashDumpServer::AcceptSession(const SharedPointer<ISession>& pSession)
        {
            DD_UNUSED(pSession);

            bool accept = false;

            m_mutex.Lock();

           // Only accept sessions if we have a crash handler installed.
            if (m_pCrashDumpHandler != nullptr)
            {
                // Keep track of the number of sessions.
                m_numSessions++;

                accept = true;
            }

            m_mutex.Unlock();

            return accept;
        }

        void GpuCrashDumpServer::SessionEstablished(const SharedPointer<ISession>& pSession)
        {
            // Allocate session data for the newly established session
            GpuCrashDumpSession* pSessionData = DD_NEW(GpuCrashDumpSession, m_pMsgChannel->GetAllocCb())();
            pSessionData->state               = SessionState::WaitForCrashDump;
            pSessionData->payload             = {};
            pSessionData->pUserdata           = nullptr;

            pSession->SetUserData(pSessionData);
        }

        void GpuCrashDumpServer::UpdateSession(const SharedPointer<ISession>& pSession)
        {
            GpuCrashDumpSession* pSessionData = reinterpret_cast<GpuCrashDumpSession*>(pSession->GetUserData());

            // We should always have a valid crash handler when any sessions are active.
            DD_ASSERT(m_pCrashDumpHandler != nullptr);

            switch (pSessionData->state)
            {
                case SessionState::WaitForCrashDump:
                {
                    uint32 bytesReceived = 0;
                    const Result result = pSession->Receive(sizeof(pSessionData->payload), &pSessionData->payload, &bytesReceived, kNoWait);
                    if (result == Result::Success)
                    {
                        DD_ASSERT(sizeof(pSessionData->payload) == bytesReceived);

                        if (pSessionData->payload.command == GpuCrashDumpMessage::GpuCrashNotify)
                        {
                            // Extract the relevant info from the notification.
                            const uint32 sizeInBytes = pSessionData->payload.notify.sizeInBytes;
                            const ClientId clientId  = pSession->GetDestinationClientId();

                            // Send an acknowledgement back.
                            pSessionData->state           = SessionState::SendAcknowledgement;
                            pSessionData->payload.command = GpuCrashDumpMessage::GpuCrashAcknowledge;

                            // Call the crash available callback and check if we want this crash dump.
                            const bool acceptCrash = m_pCrashDumpHandler->AcceptCrashDump(static_cast<size_t>(sizeInBytes), clientId, &pSessionData->pUserdata);

                            // Set payload value for ack based on callback.
                            pSessionData->payload.acknowledge.acceptedCrashDump = acceptCrash;

                            // Save the size of the crash data and reset the bytes received.
                            pSessionData->crashDataTotalSizeInBytes = sizeInBytes;
                            pSessionData->crashDataBytesReceived    = 0;
                        }
                        else
                        {
                            // We should only ever receive crash notifications in this state.
                            DD_ALERT_REASON("Invalid message received while waiting on crash dump acknowledgement");

                            // Ignore the message.
                        }
                    }
                    else if (result != Result::NotReady)
                    {
                        // We've encountered an error while attempting to read from the session.
                        // Do nothing since the session will close itself automatically.
                    }

                    break;
                }

                case SessionState::SendAcknowledgement:
                {
                    // Make sure we have the correct command.
                    DD_ASSERT(pSessionData->payload.command == GpuCrashDumpMessage::GpuCrashAcknowledge);

                    // Attempt to send the acknowledgement.
                    const Result result = pSession->Send(sizeof(pSessionData->payload), &pSessionData->payload, kNoWait);
                    if (result == Result::Success)
                    {
                        // Transition to the transfer state to begin transferring the crash dump if we accepted the crash.
                        if (pSessionData->payload.acknowledge.acceptedCrashDump)
                        {
                            pSessionData->state = SessionState::TransferCrashDump;
                        }
                        else
                        {
                            // We successfully rejected the crash, transition back to the waiting state.
                            pSessionData->state = SessionState::WaitForCrashDump;
                        }
                    }
                    else if (result != Result::NotReady)
                    {
                        // We've encountered an error while trying to acknowledge the crash.

                        // Let the crash handler know that the transfer is ending if we previously accepted the crash.
                        if (pSessionData->payload.acknowledge.acceptedCrashDump)
                        {
                            m_pCrashDumpHandler->FinishCrashDumpTransfer(false, pSessionData->pUserdata);
                        }

                        // Transition back to the waiting state.
                        pSessionData->state = SessionState::WaitForCrashDump;
                    }

                    break;
                }

                case SessionState::TransferCrashDump:
                {
                    uint32 bytesReceived = 0;
                    const Result result = pSession->Receive(sizeof(pSessionData->payload), &pSessionData->payload, &bytesReceived, kNoWait);
                    if (result == Result::Success)
                    {
                        DD_ASSERT(sizeof(pSessionData->payload) == bytesReceived);

                        if (pSessionData->payload.command == GpuCrashDumpMessage::GpuCrashDataChunk)
                        {
                            // Only receive the chunk if we're still looking for data.
                            if (pSessionData->crashDataBytesReceived < pSessionData->crashDataTotalSizeInBytes)
                            {
                                // Determine the number of bytes in the chunk.
                                // If the current size + the max chunk size is greater than the total, that means this chunk is only partially full
                                // since we can't have more bytes than the total.
                                const uint32 numBytesInChunk =
                                    ((pSessionData->crashDataBytesReceived + kMaxGpuCrashDumpDataChunkSize > pSessionData->crashDataTotalSizeInBytes) ?
                                     (pSessionData->crashDataTotalSizeInBytes - pSessionData->crashDataBytesReceived)                                 :
                                     kMaxGpuCrashDumpDataChunkSize);

                                // We successfully read crash dump data from the remote client. Call the client's handler.
                                m_pCrashDumpHandler->ReceiveCrashDumpData(pSessionData->payload.dataChunk.data,
                                                                          numBytesInChunk,
                                                                          pSessionData->pUserdata);

                                // Update the number of bytes received.
                                pSessionData->crashDataBytesReceived += numBytesInChunk;
                            }
                            else
                            {
                                // End the transfer and transition back to the waiting state.
                                m_pCrashDumpHandler->FinishCrashDumpTransfer(false, pSessionData->pUserdata);
                                pSessionData->state = SessionState::WaitForCrashDump;

                                // This should never happen. If we hit this, the remote client is sending more chunks than necessary.
                                DD_ALERT_REASON("Server received more chunks than expected");
                            }
                        }
                        else if (pSessionData->payload.command == GpuCrashDumpMessage::GpuCrashDataSentinel)
                        {
                            // Let the crash handler know that we're finished with the transfer.
                            m_pCrashDumpHandler->FinishCrashDumpTransfer((pSessionData->payload.sentinel.result == Result::Success),
                                                                         pSessionData->pUserdata);

                            // Transition back to the waiting state.
                            pSessionData->state = SessionState::WaitForCrashDump;
                        }
                        else
                        {
                            // End the transfer and transition back to the waiting state.
                            m_pCrashDumpHandler->FinishCrashDumpTransfer(false, pSessionData->pUserdata);
                            pSessionData->state = SessionState::WaitForCrashDump;

                            // Invalid command?
                            DD_ALERT_REASON("Invalid command received");
                        }
                    }
                    else if (result != Result::NotReady)
                    {
                        // We've encountered some sort of error during the transfer. Let the crash handler know and end the transfer.
                        m_pCrashDumpHandler->FinishCrashDumpTransfer(false, pSessionData->pUserdata);

                        // Transition back to the waiting state.
                        pSessionData->state = SessionState::WaitForCrashDump;
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

        void GpuCrashDumpServer::SessionTerminated(const SharedPointer<ISession>& pSession, Result terminationReason)
        {
            DD_UNUSED(terminationReason);

            m_mutex.Lock();

            // Update the session counter.
            m_numSessions--;

            m_mutex.Unlock();

            GpuCrashDumpSession* pGpuCrashDumpSession = reinterpret_cast<GpuCrashDumpSession*>(pSession->SetUserData(nullptr));

            // Free the session data
            if (pGpuCrashDumpSession != nullptr)
            {
                DD_DELETE(pGpuCrashDumpSession, m_pMsgChannel->GetAllocCb());
            }
        }

        Result GpuCrashDumpServer::SetCrashDumpHandler(ICrashDumpHandler* pHandler)
        {
            Result result = Result::Error;

            m_mutex.Lock();

            // Only allow the caller to change the handler if there are no active sessions.
            if (m_numSessions == 0)
            {
                m_pCrashDumpHandler = pHandler;
                result = Result::Success;
            }

            m_mutex.Unlock();

            return result;
        }

        ICrashDumpHandler* GpuCrashDumpServer::GetCrashDumpHandler()
        {
            m_mutex.Lock();

            ICrashDumpHandler* pHandler = m_pCrashDumpHandler;

            m_mutex.Unlock();

            return pHandler;
        }
    }
} // DevDriver
