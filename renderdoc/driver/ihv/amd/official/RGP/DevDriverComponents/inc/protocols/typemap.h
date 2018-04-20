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
* @file  map.h
* @brief Type mappings between protocol enum and client/server types.
***********************************************************************************************************************
*/

#pragma once

namespace DevDriver
{
    class IProtocolServer;
    class IProtocolClient;

    template <Protocol protocol>
    struct ProtocolServerMap
    {
        typedef IProtocolServer type;
    };

    template <Protocol protocol>
    struct ProtocolClientMap
    {
        typedef IProtocolClient type;
    };

    template <Protocol protocol>
    using ProtocolServerType = typename ProtocolServerMap<protocol>::type;

    template <Protocol protocol>
    using ProtocolClientType = typename ProtocolClientMap<protocol>::type;

    namespace LoggingProtocol
    {
        class LoggingServer;
        class LoggingClient;
    }

    template <>
    struct ProtocolServerMap<Protocol::Logging>
    {
        typedef LoggingProtocol::LoggingServer type;
    };

    template <>
    struct ProtocolClientMap<Protocol::Logging>
    {
        typedef LoggingProtocol::LoggingClient type;
    };

    namespace SettingsProtocol
    {
        class SettingsServer;
        class SettingsClient;
    }

    template <>
    struct ProtocolServerMap<Protocol::Settings>
    {
        typedef SettingsProtocol::SettingsServer type;
    };

    template <>
    struct ProtocolClientMap<Protocol::Settings>
    {
        typedef SettingsProtocol::SettingsClient type;
    };

    namespace DriverControlProtocol
    {
        class DriverControlServer;
        class DriverControlClient;
    }

    template <>
    struct ProtocolServerMap<Protocol::DriverControl>
    {
        typedef DriverControlProtocol::DriverControlServer type;
    };

    template <>
    struct ProtocolClientMap<Protocol::DriverControl>
    {
        typedef DriverControlProtocol::DriverControlClient type;
    };

    namespace RGPProtocol
    {
        class RGPServer;
        class RGPClient;
    }

    template <>
    struct ProtocolServerMap<Protocol::RGP>
    {
        typedef RGPProtocol::RGPServer type;
    };

    template <>
    struct ProtocolClientMap<Protocol::RGP>
    {
        typedef RGPProtocol::RGPClient type;
    };

    namespace ETWProtocol
    {
        class ETWServer;
        class ETWClient;
    }

    template <>
    struct ProtocolServerMap<Protocol::ETW>
    {
        typedef ETWProtocol::ETWServer type;
    };

    template <>
    struct ProtocolClientMap<Protocol::ETW>
    {
        typedef ETWProtocol::ETWClient type;
    };

    namespace GpuCrashDumpProtocol
    {
        class GpuCrashDumpClient;
        class GpuCrashDumpServer;
    }

    template <>
    struct ProtocolServerMap<Protocol::GpuCrashDump>
    {
        typedef GpuCrashDumpProtocol::GpuCrashDumpServer type;
    };

    template <>
    struct ProtocolClientMap<Protocol::GpuCrashDump>
    {
        typedef GpuCrashDumpProtocol::GpuCrashDumpClient type;
    };
    namespace TransferProtocol
    {
        class TransferServer;
        class TransferClient;
    }

    template <>
    struct ProtocolServerMap<Protocol::Transfer>
    {
        typedef TransferProtocol::TransferServer type;
    };

    template <>
    struct ProtocolClientMap<Protocol::Transfer>
    {
        typedef TransferProtocol::TransferClient type;
    };

    namespace URIProtocol
    {
        class URIServer;
        class URIClient;
    }

    template <>
    struct ProtocolServerMap<Protocol::URI>
    {
        typedef URIProtocol::URIServer type;
    };

    template <>
    struct ProtocolClientMap<Protocol::URI>
    {
        typedef URIProtocol::URIClient type;
    };
}
