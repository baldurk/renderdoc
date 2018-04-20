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
* @file  abstractClientManager.h
* @brief Class declaration for IClientManager
***********************************************************************************************************************
*/

#pragma once

#include "gpuopen.h"
#include "abstractClientManager.h"
#if DD_VERSION_SUPPORTS(GPUOPEN_DEPRECATE_LEGACY_KMD_VERSION)
#include "util/hashSet.h"
#else
#include "util/hashMap.h"
#endif
#include "ddPlatform.h"

namespace DevDriver
{
    struct ListenerClientManagerInfo
    {
        ClientId routerPrefix;
        ClientId routerPrefixMask;
    };

    class ListenerClientManager : public IClientManager
    {
    public:
        ListenerClientManager(const AllocCb& allocCb, const ListenerClientManagerInfo& clientManagerInfo);
        ~ListenerClientManager();

        // TODO: explicit initialize and destroy?
        // anything else missing?

        Result RegisterHost(ClientId* pClientId) override;
        std::shared_ptr<IListenerTransport> GetHostTransport() override { return std::shared_ptr<IListenerTransport>(); };
        Result UnregisterHost() override;

#if DD_VERSION_SUPPORTS(GPUOPEN_DEPRECATE_LEGACY_KMD_VERSION)
        Result RegisterClient(ClientId* pClientId) override;
#else
        Result RegisterClient(Component componentType, StatusFlags flags, ClientId* pClientId) override;
#endif
        Result UnregisterClient(ClientId clientId) override;

#if !DD_VERSION_SUPPORTS(GPUOPEN_DISTRIBUTED_STATUS_FLAGS_VERSION)
        Result UpdateHostStatus(StatusFlags flags) override;
        Result UpdateClientStatus(ClientId clientId, StatusFlags flags) override;
        Result QueryStatus(StatusFlags *pFlags) override;
#endif
        const char *GetClientManagerName() const override { return "Internal"; };
        ClientId GetHostClientId() const override { return (m_initialized ? m_hostClientId : kBroadcastClientId); };

    protected:
#if !DD_VERSION_SUPPORTS(GPUOPEN_DEPRECATE_LEGACY_KMD_VERSION)
        struct ClientInfo
        {
            StatusFlags status;
            Component componentType;
        };
#endif

        const ListenerClientManagerInfo m_clientManagerInfo;
        bool                            m_initialized;
        ClientId                        m_hostClientId;
        Platform::Mutex                 m_clientMutex;
#if DD_VERSION_SUPPORTS(GPUOPEN_DEPRECATE_LEGACY_KMD_VERSION)
        HashSet<ClientId>               m_clientInfo;
#else
        HashMap<ClientId, ClientInfo>   m_clientInfo;
        StatusFlags                     m_combinedStatusFlags;
#endif
        Platform::Random                m_rand;

#if !DD_VERSION_SUPPORTS(GPUOPEN_DEPRECATE_LEGACY_KMD_VERSION)
        bool RecalculateClientStatus();
#endif
        ClientId GenerateClientId();
    };

} // DevDriver
