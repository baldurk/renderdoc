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

#include "listenerClientManager.h"
#include "ddPlatform.h"
#include <cstring>

namespace DevDriver
{
    ListenerClientManager::ListenerClientManager(const AllocCb& allocCb, const ListenerClientManagerInfo& clientManagerInfo) :
        m_clientManagerInfo(clientManagerInfo),
        m_initialized(false),
        m_hostClientId(kBroadcastClientId),
        m_clientMutex(),
        m_clientInfo(allocCb),
#if !DD_VERSION_SUPPORTS(GPUOPEN_DEPRECATE_LEGACY_KMD_VERSION)
        m_combinedStatusFlags(static_cast<StatusFlags>(ClientStatusFlags::None)),
#endif
        m_rand()
    {
        DD_ASSERT((clientManagerInfo.routerPrefix &
                   clientManagerInfo.routerPrefixMask) == clientManagerInfo.routerPrefix);
    }

    ListenerClientManager::~ListenerClientManager()
    {
        if (m_initialized)
            UnregisterHost();
    }

    Result ListenerClientManager::RegisterHost(ClientId* pClientId)
    {
        Result result = Result::Error;
        if (!m_initialized)
        {
            Platform::LockGuard<Platform::Mutex> clientLock(m_clientMutex);

            m_hostClientId = GenerateClientId();
            DD_ASSERT(m_hostClientId != kBroadcastClientId);
            m_initialized = true;
            *pClientId = m_hostClientId;
#if DD_VERSION_SUPPORTS(GPUOPEN_DEPRECATE_LEGACY_KMD_VERSION)
            m_clientInfo.Insert(m_hostClientId);
#else
            m_clientInfo[m_hostClientId].status = 0;
            m_clientInfo[m_hostClientId].componentType = Component::Server;
            RecalculateClientStatus();
#endif
            result = Result::Success;
        }
        return result;
    }

#if !DD_VERSION_SUPPORTS(GPUOPEN_DISTRIBUTED_STATUS_FLAGS_VERSION)
    Result ListenerClientManager::UpdateHostStatus(StatusFlags flags)
    {
        Result result = Result::Error;
        if (m_initialized)
        {
            Platform::LockGuard<Platform::Mutex> clientLock(m_clientMutex);
            m_clientInfo[m_hostClientId].status = flags;
            RecalculateClientStatus();
            result = Result::Success;
        }
        return result;
    }
#endif

    Result ListenerClientManager::UnregisterHost()
    {
        Result result = Result::Error;
        if (m_initialized)
        {
            Platform::LockGuard<Platform::Mutex> clientLock(m_clientMutex);
            m_clientInfo.Clear();
            m_hostClientId = kBroadcastClientId;
#if !DD_VERSION_SUPPORTS(GPUOPEN_DEPRECATE_LEGACY_KMD_VERSION)
            RecalculateClientStatus();
#endif
            m_initialized = false;
            result = Result::Success;
        }
        return result;
    }

#if DD_VERSION_SUPPORTS(GPUOPEN_DEPRECATE_LEGACY_KMD_VERSION)
    Result ListenerClientManager::RegisterClient(ClientId* pClientId)
#else
    Result ListenerClientManager::RegisterClient(Component componentType, StatusFlags flags, ClientId* pClientId)
#endif
    {
        Result result = Result::Error;
        if (m_initialized)
        {
            Platform::LockGuard<Platform::Mutex> clientLock(m_clientMutex);

            const ClientId tempClientId = GenerateClientId();
            if (tempClientId != kBroadcastClientId)
            {
                const ClientId clientMask = ~m_clientManagerInfo.routerPrefixMask;
                DD_ASSERT((tempClientId & clientMask) != kBroadcastClientId);
#if DD_VERSION_SUPPORTS(GPUOPEN_DEPRECATE_LEGACY_KMD_VERSION)
                m_clientInfo.Insert(tempClientId);
#else
                ClientInfo newClientInfo = {};
                newClientInfo.componentType = componentType;
                newClientInfo.status = flags;
                m_clientInfo.Create(tempClientId, newClientInfo);
                RecalculateClientStatus();
#endif
                *pClientId = tempClientId;
                result = Result::Success;
            }
            else
            {
                // This is a critical failure and shouldn't happen under normal conditions
                DD_ALERT_REASON("Client manager was unable to generate a new client ID");
            }
        }
        return result;
    }

    Result ListenerClientManager::UnregisterClient(ClientId clientId)
    {
        Result result = Result::Error;
        if (m_initialized & (clientId != m_hostClientId))
        {
            Platform::LockGuard<Platform::Mutex> clientLock(m_clientMutex);
            // Attempt to erase a client with the specified client ID. If >0 we have successfully removed
            // the client ID from the set.
            if (m_clientInfo.Erase(clientId) == Result::Success)
            {
#if !DD_VERSION_SUPPORTS(GPUOPEN_DEPRECATE_LEGACY_KMD_VERSION)
                RecalculateClientStatus();
#endif
                result = Result::Success;
            }
        }
        return result;
    }

#if !DD_VERSION_SUPPORTS(GPUOPEN_DISTRIBUTED_STATUS_FLAGS_VERSION)
    Result ListenerClientManager::UpdateClientStatus(ClientId clientId, StatusFlags flags)
   {
        Result result = Result::Error;
        if (m_initialized & (clientId != m_hostClientId))
        {
            Platform::LockGuard<Platform::Mutex> clientLock(m_clientMutex);
            auto find = m_clientInfo.Find(clientId);
            if (find != m_clientInfo.End())
            {
                find->value.status = flags;
                result = Result::Success;
                RecalculateClientStatus();
            }
        }
        return result;
    }

    Result ListenerClientManager::QueryStatus(StatusFlags *pFlags)
    {
        Result result = Result::Error;
        if (m_initialized)
        {
            *pFlags = m_combinedStatusFlags;
            result = Result::Success;
        }
        return result;
    }
#endif

#if !DD_VERSION_SUPPORTS(GPUOPEN_DEPRECATE_LEGACY_KMD_VERSION)
    bool ListenerClientManager::RecalculateClientStatus()
    {
        const StatusFlags originalFlags = m_combinedStatusFlags;
        m_combinedStatusFlags = static_cast<StatusFlags>(ClientStatusFlags::None);
        for (auto &client : m_clientInfo)
        {
            m_combinedStatusFlags |= client.value .status;
        }

        return (m_combinedStatusFlags != originalFlags);
    }
#endif

    // Generate a random client ID that has not already been allocated
    ClientId ListenerClientManager::GenerateClientId()
    {
        ClientId tempClientId = kBroadcastClientId;

        const ClientId routerPrefix = m_clientManagerInfo.routerPrefix;
        const ClientId clientMask = ~m_clientManagerInfo.routerPrefixMask;

        // The maximum number of clients we can allocate is equal the client ID mask, less one for the broadcast ID
        DD_STATIC_CONST size_t kMaxNumberOfClients = (kClientIdMask - 1);

        if (m_clientInfo.Size() < kMaxNumberOfClients)
        {
            // Loop until we have found a client ID that is not the broadcast client ID and that we haven't allocated
            do
            {
                // Add one since the range is typically 0 <= x < Max
                const uint32 randVal = m_rand.Generate() + 1;
                tempClientId = static_cast<ClientId>(randVal & clientMask) | routerPrefix;
            } while (((tempClientId & clientMask) == kBroadcastClientId) || m_clientInfo.Contains(tempClientId));
        }

        DD_ASSERT(tempClientId != kBroadcastClientId);
        return tempClientId;
    }
} // DevDriver
