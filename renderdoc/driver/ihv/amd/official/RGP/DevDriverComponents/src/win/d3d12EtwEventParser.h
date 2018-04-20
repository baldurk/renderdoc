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

//=============================================================================
/// Copyright (c) 2016 Advanced Micro Devices, Inc. All rights reserved.
/// \author AMD Developer Tools Team
/// \file
/// \brief  Implementation of a realtime D3D12 ETW Event consumer.
//=============================================================================

#pragma once

#include "gpuopen.h"
#include "ddPlatform.h"
#include "protocols/etwProtocol.h"
#include "util/queue.h"
#include <unordered_map>
#include <queue>
#include <codecvt>

#include <windows.h>
#include <evntrace.h>
#include <evntprov.h>
#include <evntcons.h>
#include <objbase.h>
#include <Tdh.h>
#include <tchar.h>
#include <string>

#include "d3d12EtwDxgkParser.h"

namespace DevDriver
{
    class GpuEventComparison
    {
    public:
        bool operator() (const GpuEvent & lhs, const GpuEvent & rhs)
        {
            //your code...
            return lhs.submissionTime < rhs.submissionTime;
        }
        typedef GpuEvent first_argument_type;
        typedef GpuEvent second_argument_type;
        typedef bool result_type;
    };

    using ParsedStorage = std::priority_queue<GpuEvent, std::vector<GpuEvent>, GpuEventComparison>;

    struct TraceStorage
    {
        ParsedStorage parsedEvents;
        DxgkEtwParser::EventStorage dxgkEvents;
        ProcessId processId;
    };

    template<bool is32Bit>
    void PrintPacketData(TraceStorage &traceData, PEVENT_RECORD pEvent)
    {
        const char* pBaseAddress = ((const char*)pEvent->UserData);
        const uint32* pIteratorPointer = reinterpret_cast<const uint32 *>(pBaseAddress);
        const uint32* pEndPointer = reinterpret_cast<const uint32 *>(pBaseAddress + pEvent->UserDataLength);

        while (pIteratorPointer < pEndPointer)
        {
            printf(" %.8X\n", *pIteratorPointer);
            pIteratorPointer++;
        }
        printf("\n");
    }

    namespace DxgkEtwParser
    {

        template<bool is32Bit>
        void ProcessWaitQueuePacket(TraceStorage &traceData, PEVENT_RECORD pEvent)
        {
            using WaitPacket = WaitPacketHeader<is32Bit>;

            const char* pBaseAddress = ((const char*)pEvent->UserData);
            const WaitPacket *pHeader = reinterpret_cast<const WaitPacket*>(pBaseAddress);

            // create event and copy raw time into it
            QueueSyncSubmissionEvent event = {};
            event.type = GpuEventType::QueueWait;

            // TODO: Is this correct? Do we need to modify this at all?
            event.timestamp = static_cast<uint64>(pEvent->EventHeader.TimeStamp.QuadPart);
            event.contextIdentifier = pHeader->hContext;
            event.sequence = pHeader->sequence;
            event.fences.emplace_back(FenceObject({ pHeader->hSyncObject, pHeader->fenceValue }));

            traceData.dxgkEvents.submissionEvents[event.contextIdentifier][event.sequence] = event;
        }

        template<bool is32Bit>
        void ProcessSignalQueuePacket(TraceStorage &traceData, PEVENT_RECORD pEvent)
        {
            using SignalPacket = SignalPacketHeader<is32Bit>;
            const char* pBaseAddress = ((const char*)pEvent->UserData);
            const SignalPacket *pHeader = reinterpret_cast<const SignalPacket*>(pBaseAddress);
            // create event and copy raw time into it
            QueueSyncSubmissionEvent event = {};
            event.type = GpuEventType::QueueSignal;

            // TODO: Is this correct? Do we need to modify this at all?
            event.timestamp = static_cast<uint64>(pEvent->EventHeader.TimeStamp.QuadPart);
            event.contextIdentifier = pHeader->hContext;
            event.sequence = pHeader->sequence;

            const uint64 *fenceValues = (const uint64*)(((const char*)&pHeader->semaphore.values[0]) + sizeof(uint64) * pHeader->semaphore.count);
            for (UINT32 i = 0; i < pHeader->semaphore.count; i++)
            {
                event.fences.emplace_back(FenceObject({ pHeader->semaphore.values[i], fenceValues[i] }));
            }

            traceData.dxgkEvents.submissionEvents[event.contextIdentifier][event.sequence] = event;
        }

        template<bool is32Bit>
        void FinalizeSyncQueuePacket(TraceStorage &traceData, GpuEventType type, uint64 context, uint32 sequence, uint64 timestamp)
        {
            const auto &findContext = traceData.dxgkEvents.submissionEvents.find(context);
            if (findContext != traceData.dxgkEvents.submissionEvents.end())
            {
                const auto& find = findContext->second.find(sequence);
                if (find != findContext->second.end())
                {
                    const QueueSyncSubmissionEvent &submissionEvent = find->second;
                    if (sequence == submissionEvent.sequence &&  type == submissionEvent.type)
                    {
                        GpuEvent event = {};
                        event.type = type;
                        event.submissionTime = submissionEvent.timestamp;
                        event.completionTime = timestamp;
                        event.queue.contextIdentifier = submissionEvent.contextIdentifier;
                        for (auto &fence : submissionEvent.fences)
                        {
                            event.queue.fenceObject = fence.fenceObject;
                            event.queue.fenceValue = fence.fenceValue;
                            traceData.parsedEvents.emplace(event);
                        }
                    }
                }
            }
        }

        template<bool is32Bit>
        void ProcessSyncEndQueuePacket(TraceStorage &traceData, PEVENT_RECORD pEvent)
        {
            using SyncQueuePacket = SyncQueuePacketHeader<is32Bit>;
            const SyncQueuePacket* pHeader = reinterpret_cast<const SyncQueuePacket*>(pEvent->UserData);
            switch (pHeader->packetType)
            {
                case CommandBufferType::Signal:
                {
                    FinalizeSyncQueuePacket<is32Bit>(traceData,
                                                     GpuEventType::QueueSignal,
                                                     pHeader->hContext,
                                                     pHeader->sequence,
                                                     static_cast<uint64>(pEvent->EventHeader.TimeStamp.QuadPart));
                    break;
                }
                case CommandBufferType::Wait:
                {
                    FinalizeSyncQueuePacket<is32Bit>(traceData,
                                                     GpuEventType::QueueWait,
                                                     pHeader->hContext,
                                                     pHeader->sequence,
                                                     static_cast<uint64>(pEvent->EventHeader.TimeStamp.QuadPart));
                    break;
                }
                default:
                    break;
            }
        }

        template<bool is32Bit>
        void ParseQueuePacket(TraceStorage &traceData, PEVENT_RECORD pEvent)
        {
            switch (static_cast<QueuePacketId>(pEvent->EventHeader.EventDescriptor.Id))
            {
                case QueuePacketId::End:
                    ProcessSyncEndQueuePacket<is32Bit>(traceData, pEvent);
                    break;
                case QueuePacketId::Signal:
                    if (pEvent->EventHeader.ProcessId == traceData.processId)
                        ProcessSignalQueuePacket<is32Bit>(traceData, pEvent);
                    break;
                case QueuePacketId::Wait:
                    if (pEvent->EventHeader.ProcessId == traceData.processId)
                        ProcessWaitQueuePacket<is32Bit>(traceData, pEvent);
                    break;
                default:
                    break;
            }
        }

        template<bool is32Bit>
        void ParsePacket(TraceStorage &traceData, PEVENT_RECORD pEvent)
        {
            char buffer[4096];
            PTRACE_EVENT_INFO pInfo = reinterpret_cast<PTRACE_EVENT_INFO>(&buffer[0]);
            DWORD BufferSize = sizeof(buffer);

            // Retrieve the required buffer size for the event metadata.
            DWORD result = TdhGetEventInformation(pEvent, 0, nullptr, pInfo, &BufferSize);
            if (result == ERROR_SUCCESS)
            {
                LPWSTR eventString = (LPWSTR)((PBYTE)(pInfo)+pInfo->TaskNameOffset);

                std::wstring strName = std::wstring(eventString);
                const Event type = kObjectTypeMap[strName];
                switch (type)
                {
                    case Event::QueuePacket:
                    {
                        DxgkEtwParser::ParseQueuePacket<is32Bit>(traceData, pEvent);
                        break;
                    }
                    default:
                        break;
                }
            }
        }
    }

    class EtwParser
    {
    public:
        EtwParser()
            : m_traceData()
            , m_dxgKernelProviderGUID()
        {
            HRESULT result = CLSIDFromString(kDxgKernelProviderGuid, &m_dxgKernelProviderGUID);
            (void)result;
        }

        bool Start(ProcessId pid)
        {
            DD_ASSERT(m_traceData.processId == 0);
            if (m_traceData.processId == 0)
            {
                m_traceData.processId = pid;
                return true;
            }
            return false;
        }

        void ParseEvent(PEVENT_RECORD pEvent)
        {
            bool is32Bit = (pEvent->EventHeader.Flags & EVENT_HEADER_FLAG_32_BIT_HEADER) != 0;
            if (is32Bit)
            {
                ParseEventInternal<true>(pEvent);
            }
            else
            {
                ParseEventInternal<false>(pEvent);
            }
        }

        void ClearEvents()
        {

        }

        size_t FinishTrace(Queue<ETWProtocol::ETWPayload>& msgQueue)
        {
            size_t result = 0;
            if (m_traceData.processId != 0)
            {
                using namespace DevDriver::ETWProtocol;

                result = m_traceData.parsedEvents.size();

                while (m_traceData.parsedEvents.size() > 0)
                {
                    ETWPayload *pPayload = msgQueue.AllocateBack();
                    if (pPayload != nullptr)
                    {
                        pPayload->command = ETWMessage::TraceDataChunk;
                        pPayload->traceDataChunk.numEvents = 0;
                        for (uint32 count = 0; count < kMaxEventsPerChunk && m_traceData.parsedEvents.size() > 0; count++)
                        {
                            pPayload->traceDataChunk.events[count] = m_traceData.parsedEvents.top();
                            m_traceData.parsedEvents.pop();
                            pPayload->traceDataChunk.numEvents++;
                        }
                    }
                }
                m_traceData.processId = 0;
            }
            return result;
        }
    private:
        TraceStorage m_traceData;
        GUID m_dxgKernelProviderGUID;

        template<bool is32Bit>
        void ParseEventInternal(PEVENT_RECORD pEvent)
        {
            int isDxgKernelProviderEvent = IsEqualGUID(pEvent->EventHeader.ProviderId, m_dxgKernelProviderGUID);
            if (isDxgKernelProviderEvent != 0)
            {
                DxgkEtwParser::ParsePacket<is32Bit>(m_traceData, pEvent);
            }
        }
    };
}
