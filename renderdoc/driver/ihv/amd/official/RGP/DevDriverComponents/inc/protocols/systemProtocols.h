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
* @file  systemProtocols.h
* @brief Protocol header for all system protocols
***********************************************************************************************************************
*/

#pragma once

#include "gpuopen.h"

namespace DevDriver
{
    namespace SystemProtocol
    {
        ///////////////////////
        // GPU Open System Protocol
        enum struct SystemMessage : MessageCode
        {
            Unknown = 0,
            ClientConnected,
            ClientDisconnected,
            Ping,
            Pong,
            QueryClientInfo,
            ClientInfo,
            Halted,
            Count,
        };
    }

    namespace SessionProtocol
    {
        ///////////////////////
        // GPU Open Session Protocol
        enum struct SessionMessage : MessageCode
        {
            Unknown = 0,
            Syn,
            SynAck,
            Fin,
            Data,
            Ack,
            Rst,
            Count
        };

        typedef uint8 SessionVersion;
        // Session protocol 2 lets session servers return session version as part of the synack
        DD_STATIC_CONST SessionVersion kSessionProtocolVersionSynAckVersion = 2;
        // Session protocol 1 lets session clients specify a max range supported as part of the syn
        DD_STATIC_CONST SessionVersion kSessionProtocolRangeVersion = 1;
        // current version is 2
        DD_STATIC_CONST SessionVersion kSessionProtocolVersion = kSessionProtocolVersionSynAckVersion;
        // not mentioned is session version 0. It only supported min version in SynAck, servers reporting it cannot
        // cleanly terminate in response to a Fin packet.

        // tripwire - this intentionally will break if the message version changes. Since that implies a breaking change, we need to address
        // to re-baseline this as version 0 and update the SynPayload struct at the same time
        static_assert(kMessageVersion == 1011, "Session packets need to be cleaned up as part of the next protocol version");

        DD_NETWORK_STRUCT(SynPayload, 4)
        {
            Version         minVersion;
            Protocol        protocol;
            // pad out to 4 bytes
            SessionVersion  sessionVersion;

            // New fields read if sessionVersion != 0
            Version         maxVersion;
            // pad out to 8 bytes
            uint8           reserved[2];
        };

        DD_CHECK_SIZE(SynPayload, 8);

        //
        // SynPayloadV2 is here so that we can use it with the next breaking message bus change.
        //
        //DD_NETWORK_STRUCT(SynPayloadV2, 4)
        //{
        //    Protocol        protocol;
        //    SessionVersion  sessionVersion;
        //    Version         minVersion;
        //    Version         maxVersion;
        //    // pad out to 8 bytes
        //    uint8           reserved[2];
        //};

        //DD_CHECK_SIZE(SynPayloadV2, 8);

        DD_NETWORK_STRUCT(SynAckPayload, 8)
        {
            Sequence            sequence;
            SessionId           initialSessionId;
            Version             version;
            SessionVersion      sessionVersion;
            uint8               reserved[1];
        };

        DD_CHECK_SIZE(SynAckPayload, 16);
    }

    namespace ClientManagementProtocol
    {

        ///////////////////////
        // GPU Open ClientManagement Protocol
        enum struct ManagementMessage : MessageCode
        {
            Unknown = 0,
            ConnectRequest,
            ConnectResponse,
            DisconnectNotification,
            DisconnectResponse,
            SetClientFlags,
            SetClientFlagsResponse,
            QueryStatus,
            QueryStatusResponse,
            KeepAlive,
            Count
        };

        DD_STATIC_CONST MessageBuffer kOutOfBandMessage =
        {
            { // header
                kBroadcastClientId,             //srcClientId
                kBroadcastClientId,             //dstClientId
                Protocol::ClientManagement,     //protocolId
                0,                              //messageId
                0,                              //windowSize
                0,                              //payloadSize
                0,                              //sessionId
                kMessageVersion                 //sequence
            },
            {} // payload
        };

        inline bool IsOutOfBandMessage(const MessageBuffer &message)
        {
            // an out of band message is denoted by both the dstClientId and srcClientId
            // being initialized to kBroadcastClientId.
            static_assert(kBroadcastClientId == 0, "Error, kBroadcastClientId is non-zero. IsOutOfBandMessage needs to be fixed");
            return ((message.header.dstClientId | message.header.srcClientId) == kBroadcastClientId);
        }

        inline bool IsValidOutOfBandMessage(const MessageBuffer &message)
        {
            // an out of band message is only valid if the sequence field is initialized with the correct version
            // and the protocolId is equal to the receiving client's Protocol::ClientManagement value
            return ((message.header.sequence == kMessageVersion) &
                    (message.header.protocolId == Protocol::ClientManagement));
        }

        DD_NETWORK_STRUCT(ConnectRequestPayload, 4)
        {
            StatusFlags initialClientFlags;
            uint8       padding[2];
            Component   componentType;
            uint8       reserved[3];
        };

        DD_CHECK_SIZE(ConnectRequestPayload, 8);

        DD_NETWORK_STRUCT(ConnectResponsePayload, 4)
        {
            Result      result;
            ClientId    clientId;
            // pad this out to 8 bytes for future expansion
            uint8       padding[2];
        };

        DD_CHECK_SIZE(ConnectResponsePayload, 8);

        DD_NETWORK_STRUCT(SetClientFlagsPayload, 4)
        {
            StatusFlags flags;
            uint8       padding[2];
        };

        DD_CHECK_SIZE(SetClientFlagsPayload, 4);

        DD_NETWORK_STRUCT(SetClientFlagsResponsePayload, 4)
        {
            Result      result;
        };

        DD_CHECK_SIZE(SetClientFlagsResponsePayload, 4);

        DD_NETWORK_STRUCT(QueryStatusResponsePayload, 4)
        {
            Result      result;
            StatusFlags flags;
            uint8       reserved[2];
        };

        DD_CHECK_SIZE(QueryStatusResponsePayload, 8);
    }
}
