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

#include "baseProtocolServer.h"
#include "msgChannel.h"

namespace DevDriver
{
    BaseProtocolServer::BaseProtocolServer(IMsgChannel* pMsgChannel, Protocol protocol, Version minVersion, Version maxVersion)
        : m_pMsgChannel(pMsgChannel)
        , m_protocol(protocol)
        , m_minVersion(minVersion)
        , m_maxVersion(maxVersion)
        , m_isFinalized(false)
    {
        DD_ASSERT(m_pMsgChannel != nullptr);
    }

    Result BaseProtocolServer::SendPayload(ISession* pSession, const SizedPayloadContainer* pPayload, uint32 timeoutInMs)
    {
        DD_ASSERT(pSession != nullptr);
        DD_ASSERT(pPayload != nullptr);

        return pSession->Send(pPayload->payloadSize, pPayload->payload, timeoutInMs);
    }

    Result BaseProtocolServer::ReceivePayload(ISession* pSession, SizedPayloadContainer* pPayload, uint32 timeoutInMs)
    {
        DD_ASSERT(pSession != nullptr);
        DD_ASSERT(pPayload != nullptr);

        return pSession->Receive(sizeof(pPayload->payload), pPayload->payload, &pPayload->payloadSize, timeoutInMs);
    }

    BaseProtocolServer::~BaseProtocolServer()
    {
    }

    bool BaseProtocolServer::GetSupportedVersion(Version minVersion, Version maxVersion, Version *version) const
    {
        DD_ASSERT(minVersion <= maxVersion);
        DD_ASSERT(version != nullptr);
        bool foundVersion = false;

        const bool minLessThanEqMax = (minVersion <= m_maxVersion);
        const bool maxLessThanEqMax = (maxVersion <= m_maxVersion);
        const bool maxGreaterThanMax = !maxLessThanEqMax;
        const bool maxGreaterThanEqMin = (maxVersion >= m_minVersion);

        // We need to find the highest value intersection between the input range and our range. To do this, we are
        // concerned primarily with two cases: if the upper bound of the range provided falls inside our range, or if
        // the upper bound of our range falls within the range it provided. To determine this we perform two tests:
        // * Check to see if (iMax <= Max) and (iMax >= Min). If this is true, than we use their maximum value.
        // * Check to see if (iMax > Max) and (iMin <= Max). If this is true, we use our maximum value.
        // We additionally attempt to report the closest bound (e.g., minimum or maximum) in the case where the they
        // don't overlap. This (hopefully) will allow them to determine whether their version was too low or too high.

        // If the max version requested is within the range supported by the server, we pick it by default
        if (maxLessThanEqMax & maxGreaterThanEqMin)
        {
            *version = maxVersion;
            foundVersion = true;
        }
        // Otherwise, we check to ensure that our max version is within the range provided to us.
        else if (maxGreaterThanMax & minLessThanEqMax)
        {
            *version = m_maxVersion;
            foundVersion = true;
        }
        else
        {
            // If the min version requested is not less than or equal to our max, we return the max version supported.
            // Otherwise we assume that the max requested is less than our min version
            *version = (!minLessThanEqMax) ? m_maxVersion : m_minVersion;
        }

        return foundVersion;
    }

    void BaseProtocolServer::Finalize()
    {
        // Finalize should only be called once.
        DD_ASSERT(m_isFinalized == false);

        m_isFinalized = true;
    }
} // DevDriver
