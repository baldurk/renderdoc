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
#include <map>
#include <unordered_map>
#include <queue>
#include <set>
#include <codecvt>

namespace DevDriver
{
    static const wchar_t* kDxgKernelProviderGuid = L"{802ec45a-1e99-4b83-9920-87c98277ba9d}";   ///< The DXGKernel ETW provider's GUID.

    namespace DxgkEtwParser
    {
        struct CommonQueueEvent
        {
            uint64 timestamp;
            uint64 contextIdentifier;
            uint32 sequence;
            GpuEventType type;
        };

        class EventLess
        {
        public:
            bool operator() (const CommonQueueEvent &left, const CommonQueueEvent&right)
            {
                return left.timestamp < right.timestamp;
            }
        };

        struct FenceObject
        {
            uint64 fenceObject;
            uint64 fenceValue;
        };

        struct QueueSyncSubmissionEvent : public CommonQueueEvent
        {
            std::vector<FenceObject> fences;
        };

        struct QueueSyncCompletionEvent : public CommonQueueEvent
        {
        };

        struct EventStorage
        {
            std::unordered_map<uint64, std::unordered_map<uint32, QueueSyncSubmissionEvent>> submissionEvents;
        };

#pragma warning(push)
        // enable MSVC specific extension that allows zero sized arrays in structs
#pragma warning(disable : 4200)
#pragma pack(push)
#pragma pack(1)
        template<typename T>
        struct ArrayHeader
        {
            UINT32 count;
            T values[];
        };

        template<bool is32Bit>
        struct PointerSize
        {
        };

        template<>
        struct PointerSize<false>
        {
            typedef ULONGLONG type;
        };

        template<>
        struct PointerSize<true>
        {
            typedef ULONG type;
        };

        template<bool is32Bit>
        using Pointer = typename PointerSize<is32Bit>::type;

        enum struct CommandBufferType : UINT32
        {
            Render = 0,
            MmioFlip = 3,
            Wait = 4,
            Signal = 5,
            Device = 6,
            Software = 7,
            Paging = 8,
        };

        template<bool is32Bit>
        struct QueueFenceHeader
        {
            Pointer<is32Bit> hContext;
            UINT32 sequence;
            UINT32 flags;
        };

        template<bool is32Bit>
        struct WaitPacketHeader : QueueFenceHeader<is32Bit>
        {
            Pointer<is32Bit> hSyncObject;
            UINT64 fenceValue;
        };

        template<bool is32Bit>
        struct SignalPacketHeader : QueueFenceHeader<is32Bit>
        {
            ArrayHeader<Pointer<is32Bit>> semaphore;
        };

        template<bool is32Bit>
        struct SyncQueuePacketHeader
        {
            Pointer<is32Bit> hContext;
            CommandBufferType packetType;
            UINT32 sequence;
        };

        // technically, the end packets have extra values after the common header. We don't actually use them though.
        //struct EndQueuePacket : SyncQueuePacketHeader
        //{
        //    UINT32 preempted;
        //    UINT32 timeout;
        //};
#pragma pack(pop)
#pragma warning(pop)
    }

    enum struct Event
    {
        Unknown,
        QueuePacket,
    };

#pragma warning(push)
    // MSVC has a known issue where it triggers warnings for initializing static objects with literals.
#pragma warning(disable : 4592)
    // we define a static object to force the compiler to handle string hashing for us.
    // this should be const, but there are MSVC compiler bugs that prevent that.
    static std::unordered_map<std::wstring, Event> kObjectTypeMap = {
        { L"QueuePacket", Event::QueuePacket },
    };
#pragma warning(pop)

    enum struct QueuePacketId : UINT32
    {
        Unknown = 0,
        Info = 0x00b3,
        End = 0x00b4,
        Wait = 0x00f4,
        Signal = 0x00f5,
    };
}
