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
* @file  protocolSession.h
* @brief Interface declaration for IProtocolSession.
***********************************************************************************************************************
*/

#pragma once

#include "gpuopen.h"
#include "ddPlatform.h"
#include "util/template.h"
#include "util/sharedptr.h"

namespace DevDriver
{
    // A container struct that can hold any protocol's payload and keep track of its size.
    // Not intended for network transport. This struct is intended to help simplify code that works with variably sized payloads.
    // The struct is 8 byte aligned because the internal payload field requires 8 byte alignment.
    DD_ALIGNED_STRUCT(SizedPayloadContainer, 8)
    {
        uint32 payloadSize;
        uint32 _padding;
        char payload[kMaxPayloadSizeInBytes];

        // For safety purposes we limit the CreatePayload and GetPayload methods to types that:
        //
        // 1. Have a standard layout, to ensure that the contents are safe to transmit across the network
        // 2. Are trivially destructible, to ensure that a user doesn't construct an object and then overwrite it
        //    without destroying it
        // 3. Small enough to fit inside the payload field of this struct
        template<typename T>
        struct CanUseAsPayload
        {
            static_assert(Platform::IsStandardLayout<T>::Value, "Type provided does not meet standard layout requirements");
            static_assert(Platform::IsTriviallyDestructible<T>::Value, "Type provided is not trivially destructible");
            static_assert((sizeof(T) <= kMaxPayloadSizeInBytes), "Type provided is too large to fit in the container");

            DD_STATIC_CONST bool Value = Platform::IsStandardLayout<T>::Value &&
                                            Platform::IsTriviallyDestructible<T>::Value &&
                                            (sizeof(T) <= kMaxPayloadSizeInBytes);
        };

        // We additionally only allow creation of a payload if the type is constructible using the arguments specified
        template<typename T, typename... Args>
        struct CanCreatePayload
        {
            static_assert(CanUseAsPayload<T>::Value, "Type specified cannot be used as a payload");
            static_assert(Platform::IsConstructible<T, Args...>::Value, "Type provided cannot be constructed with the provided arguments");

            DD_STATIC_CONST bool Value = CanUseAsPayload<T>::Value &&
                                            Platform::IsConstructible<T, Args...>::Value;
        };

        // Convenience function to allow in-place construction of a payload object using placement new.
        template<typename T,
                 typename... Args,
                 typename = typename Platform::EnableIf<CanCreatePayload<T, Args...>::Value>::Type>
            void CreatePayload(Args&&... args)
        {
            // This is tremendously unsafe, but we use placement new to construct an object inside the buffer.
            // Why do we do this? The big benefit is that it lets us skip having to create a temporary object
            // and then copy it into this buffer.
            //
            // There are a couple of other ancillary benefits that are useful. The biggest is that if an object has
            // a constexpr constructor it can initialize the memory using a memcpy/move instead of having to actually
            // call the constructor. The other benefit is that if the constructor omits initializing memory (e.g.,
            // a giant data buffer) it will also skip re-initializing the memory here. This is not the case with
            // when you create another instance of the object and copy it - the temporary object is almost certainly
            // zero initialized, and the copy/move will result in the entire struct being copied.

            static_assert(alignof(T) <= alignof(SizedPayloadContainer), "Type provided cannot be aligned in the container");

            new(reinterpret_cast<T*>(&payload[0])) T(Platform::Forward<Args>(args)...);
            payloadSize = sizeof(T);
        }

        // Convenience function to allow accessing the payload as if it was the specified type.
        template<typename T,
                 typename = typename Platform::EnableIf<CanUseAsPayload<T>::Value>::Type>
            T& GetPayload()
        {
            return *GetPayloadPointer<T>(&payload[0]);
        }

    private:
        // Convenience function to allow accessing the payload as if it was the specified type.
        template<typename T,
                 typename = typename Platform::EnableIf<CanUseAsPayload<T>::Value>::Type>
            static constexpr T* GetPayloadPointer(char* DD_RESTRICT pPointer)
        {
            static_assert(alignof(T) <= alignof(SizedPayloadContainer), "Type provided cannot be aligned in the container");
            return (T*)(pPointer);
        }
    };

    DD_CHECK_SIZE(SizedPayloadContainer, 8 + kMaxPayloadSizeInBytes);

    class IMsgChannel;
    class Session;

    enum struct SessionType
    {
        Unknown = 0,
        Client,
        Server
    };

    class ISession
    {
    public:
        virtual ~ISession() {};

        virtual Result Send(uint32 payloadSizeInBytes, const void* pPayload, uint32 timeoutInMs) = 0;
        virtual Result Receive(uint32 payloadSizeInBytes, void *pPayload, uint32 *pBytesReceived, uint32 timeoutInMs) = 0;
        virtual void Shutdown(Result reason) = 0;
        virtual void Close(Result reason) = 0;

#if !DD_VERSION_SUPPORTS(GPUOPEN_SESSION_INTERFACE_CLEANUP_VERSION)
        virtual void CloseSession(Result reason = Result::Error) = 0;
        virtual void OrphanSession() = 0;
#endif

        virtual void* SetUserData(void *) = 0;
        virtual void* GetUserData() const = 0;
        virtual SessionId GetSessionId() const = 0;
        virtual ClientId GetDestinationClientId() const = 0;
        virtual Version GetVersion() const = 0;

        // Helper functions for working with SizedPayloadContainers and managing back-compat.
        Result SendPayload(const SizedPayloadContainer& payload, uint32 timeoutInMs)
        {
            return Send(payload.payloadSize, payload.payload, timeoutInMs);
        }

        Result ReceivePayload(SizedPayloadContainer* pPayload, uint32 timeoutInMs)
        {
            DD_ASSERT(pPayload != nullptr);
            return Receive(sizeof(pPayload->payload), pPayload->payload, &pPayload->payloadSize, timeoutInMs);
        }

    protected:
        ISession() {}
    };

    class IProtocolSession
    {
    public:
        virtual ~IProtocolSession() {}

        virtual Protocol GetProtocol() const = 0;
        virtual SessionType GetType() const = 0;
        virtual Version GetMinVersion() const = 0;
        virtual Version GetMaxVersion() const = 0;

        virtual void SessionEstablished(const SharedPointer<ISession> &pSession) = 0;
        virtual void UpdateSession(const SharedPointer<ISession> &pSession) = 0;
        virtual void SessionTerminated(const SharedPointer<ISession> &pSession, Result terminationReason) = 0;
    protected:
        IProtocolSession() {}
    };
} // DevDriver
