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
* @file  baseProtocolServer.h
* @brief Class declaration for BaseProtocolServer.
***********************************************************************************************************************
*/

#pragma once

#include "protocolServer.h"

namespace DevDriver
{
    class IMsgChannel;

    class BaseProtocolServer : public IProtocolServer
    {
    public:
        virtual ~BaseProtocolServer();

        Protocol GetProtocol() const override final { return m_protocol; };
        SessionType GetType() const override final { return SessionType::Server; };
        Version GetMinVersion() const override final { return m_minVersion; };
        Version GetMaxVersion() const override final { return m_maxVersion; };

        bool GetSupportedVersion(Version minVersion, Version maxVersion, Version * version) const override final;

        virtual void Finalize() override;
    protected:
        BaseProtocolServer(IMsgChannel* pMsgChannel, Protocol protocol, Version minVersion, Version maxVersion);

        // Helper functions for working with SizedPayloadContainers
        Result SendPayload(ISession* pSession, const SizedPayloadContainer* pPayload, uint32 timeoutInMs);
        Result ReceivePayload(ISession* pSession, SizedPayloadContainer* pPayload, uint32 timeoutInMs);

        IMsgChannel* const m_pMsgChannel;
        const Protocol m_protocol;
        const Version m_minVersion;
        const Version m_maxVersion;

        bool m_isFinalized;
    };

} // DevDriver
