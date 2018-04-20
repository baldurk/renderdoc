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

#include "ddClientURIService.h"
#include "msgChannel.h"
#include "ddTransferManager.h"

namespace DevDriver
{
    // =====================================================================================================================
    ClientURIService::ClientURIService()
        : m_pMsgChannel(nullptr)
    {
    }

    // =====================================================================================================================
    ClientURIService::~ClientURIService()
    {
    }

    // =====================================================================================================================
    Result ClientURIService::HandleRequest(URIRequestContext* pContext)
    {
        DD_ASSERT(pContext != nullptr);

        Result result = Result::Error;

        // We can only handle requests if a valid message channel has been bound.
        if (m_pMsgChannel != nullptr)
        {
            // We currently only handle the "info" command.
            // All other commands will result in an error.
            if (strcmp(pContext->pRequestArguments, "info") == 0)
            {
                // Fetch the desired information about the client.
                const ClientId clientId = m_pMsgChannel->GetClientId();
                const ClientInfoStruct& clientInfo = m_pMsgChannel->GetClientInfo();

                // @todo: Replace with a string builder utility class.
                char textBuffer[256];

                // Write all the info into the response block as plain text.
                auto& pBlock = pContext->pResponseBlock;

                // Write the header
                Platform::Snprintf(textBuffer, sizeof(textBuffer), "--- Client Information ---");
                pBlock->Write(reinterpret_cast<const uint8*>(textBuffer), strlen(textBuffer));

                // Write the gpuopen library interface version
                Platform::Snprintf(textBuffer, sizeof(textBuffer), "\nClient Available Interface Version: %u.%u", GPUOPEN_INTERFACE_MAJOR_VERSION, GPUOPEN_INTERFACE_MINOR_VERSION);
                pBlock->Write(reinterpret_cast<const uint8*>(textBuffer), strlen(textBuffer));

                // Write the gpuopen client interface version
                Platform::Snprintf(textBuffer, sizeof(textBuffer), "\nClient Supported Interface Major Version: %u", GPUOPEN_CLIENT_INTERFACE_MAJOR_VERSION);
                pBlock->Write(reinterpret_cast<const uint8*>(textBuffer), strlen(textBuffer));

                // Write the client message bus version
                Platform::Snprintf(textBuffer, sizeof(textBuffer), "\nClient Supported Message Bus Version: %u", kMessageVersion);
                pBlock->Write(reinterpret_cast<const uint8*>(textBuffer), strlen(textBuffer));

                // Write the client transport type
                Platform::Snprintf(textBuffer, sizeof(textBuffer), "\nClient Transport: %s", m_pMsgChannel->GetTransportName());
                pBlock->Write(reinterpret_cast<const uint8*>(textBuffer), strlen(textBuffer));

                // Write the client id
                Platform::Snprintf(textBuffer, sizeof(textBuffer), "\nClient Id: %u", static_cast<uint32>(clientId));
                pBlock->Write(reinterpret_cast<const uint8*>(textBuffer), strlen(textBuffer));

                // Write the client type
                const char* pClientTypeString = "Unknown";
                switch (clientInfo.metadata.clientType)
                {
                    case Component::Server: pClientTypeString = "Server"; break;
                    case Component::Tool: pClientTypeString = "Tool"; break;
                    case Component::Driver: pClientTypeString = "Driver"; break;
                    default: DD_ALERT_ALWAYS(); break;
                }
                Platform::Snprintf(textBuffer, sizeof(textBuffer), "\nClient Type: %s", pClientTypeString);
                pBlock->Write(reinterpret_cast<const uint8*>(textBuffer), strlen(textBuffer));

                // Write the client name
                Platform::Snprintf(textBuffer, sizeof(textBuffer), "\nClient Name: %s", clientInfo.clientName);
                pBlock->Write(reinterpret_cast<const uint8*>(textBuffer), strlen(textBuffer));

                // Write the client description
                Platform::Snprintf(textBuffer, sizeof(textBuffer), "\nClient Description: %s", clientInfo.clientDescription);
                pBlock->Write(reinterpret_cast<const uint8*>(textBuffer), strlen(textBuffer));

                // Write the client operating system
                Platform::Snprintf(textBuffer, sizeof(textBuffer), "\nClient Operating System: %s", DD_OS_STRING " " DD_ARCH_STRING);
                pBlock->Write(reinterpret_cast<const uint8*>(textBuffer), strlen(textBuffer));

                // Only print protocols + status flags in debug builds for now.
#if !defined(NDEBUG)
                {
                    IProtocolServer* pServer = m_pMsgChannel->GetProtocolServer(Protocol::Transfer);
                    if (pServer != nullptr)
                    {
                        Platform::Snprintf(textBuffer, sizeof(textBuffer), "\nClient Transfer Protocol Supported Versions: (%u -> %u)", pServer->GetMinVersion(), pServer->GetMaxVersion());
                        pBlock->Write(reinterpret_cast<const uint8*>(textBuffer), strlen(textBuffer));
                    }
                }

                {
                    IProtocolServer* pServer = m_pMsgChannel->GetProtocolServer(Protocol::URI);
                    if (pServer != nullptr)
                    {
                        Platform::Snprintf(textBuffer, sizeof(textBuffer), "\nClient URI Protocol Supported Versions: (%u -> %u)", pServer->GetMinVersion(), pServer->GetMaxVersion());
                        pBlock->Write(reinterpret_cast<const uint8*>(textBuffer), strlen(textBuffer));
                    }
                }

                // Write the protocols
                Platform::Snprintf(textBuffer, sizeof(textBuffer), "\nClient Logging Protocol Support: %u", clientInfo.metadata.protocols.logging);
                pBlock->Write(reinterpret_cast<const uint8*>(textBuffer), strlen(textBuffer));

                if (clientInfo.metadata.protocols.logging)
                {
                    IProtocolServer* pServer = m_pMsgChannel->GetProtocolServer(Protocol::Logging);
                    if (pServer != nullptr)
                    {
                        Platform::Snprintf(textBuffer, sizeof(textBuffer), "\nClient Logging Protocol Supported Versions: (%u -> %u)", pServer->GetMinVersion(), pServer->GetMaxVersion());
                        pBlock->Write(reinterpret_cast<const uint8*>(textBuffer), strlen(textBuffer));
                    }
                }

                Platform::Snprintf(textBuffer, sizeof(textBuffer), "\nClient Settings Protocol Support: %u", clientInfo.metadata.protocols.settings);
                pBlock->Write(reinterpret_cast<const uint8*>(textBuffer), strlen(textBuffer));

                if (clientInfo.metadata.protocols.settings)
                {
                    IProtocolServer* pServer = m_pMsgChannel->GetProtocolServer(Protocol::Settings);
                    if (pServer != nullptr)
                    {
                        Platform::Snprintf(textBuffer, sizeof(textBuffer), "\nClient Settings Protocol Supported Versions: (%u -> %u)", pServer->GetMinVersion(), pServer->GetMaxVersion());
                        pBlock->Write(reinterpret_cast<const uint8*>(textBuffer), strlen(textBuffer));
                    }
                }

                Platform::Snprintf(textBuffer, sizeof(textBuffer), "\nClient Driver Control Protocol Support: %u", clientInfo.metadata.protocols.driverControl);
                pBlock->Write(reinterpret_cast<const uint8*>(textBuffer), strlen(textBuffer));

                if (clientInfo.metadata.protocols.driverControl)
                {
                    IProtocolServer* pServer = m_pMsgChannel->GetProtocolServer(Protocol::DriverControl);
                    if (pServer != nullptr)
                    {
                        Platform::Snprintf(textBuffer, sizeof(textBuffer), "\nClient Driver Control Protocol Supported Versions: (%u -> %u)", pServer->GetMinVersion(), pServer->GetMaxVersion());
                        pBlock->Write(reinterpret_cast<const uint8*>(textBuffer), strlen(textBuffer));
                    }
                }

                Platform::Snprintf(textBuffer, sizeof(textBuffer), "\nClient RGP Protocol Support: %u", clientInfo.metadata.protocols.rgp);
                pBlock->Write(reinterpret_cast<const uint8*>(textBuffer), strlen(textBuffer));

                if (clientInfo.metadata.protocols.rgp)
                {
                    IProtocolServer* pServer = m_pMsgChannel->GetProtocolServer(Protocol::RGP);
                    if (pServer != nullptr)
                    {
                        Platform::Snprintf(textBuffer, sizeof(textBuffer), "\nClient RGP Protocol Supported Versions: (%u -> %u)", pServer->GetMinVersion(), pServer->GetMaxVersion());
                        pBlock->Write(reinterpret_cast<const uint8*>(textBuffer), strlen(textBuffer));
                    }
                }

                Platform::Snprintf(textBuffer, sizeof(textBuffer), "\nClient ETW Protocol Support: %u", clientInfo.metadata.protocols.etw);
                pBlock->Write(reinterpret_cast<const uint8*>(textBuffer), strlen(textBuffer));

                if (clientInfo.metadata.protocols.etw)
                {
                    IProtocolServer* pServer = m_pMsgChannel->GetProtocolServer(Protocol::ETW);
                    if (pServer != nullptr)
                    {
                        Platform::Snprintf(textBuffer, sizeof(textBuffer), "\nClient ETW Protocol Supported Versions: (%u -> %u)", pServer->GetMinVersion(), pServer->GetMaxVersion());
                        pBlock->Write(reinterpret_cast<const uint8*>(textBuffer), strlen(textBuffer));
                    }
                }

                // Write the status flags
                const uint32 developerModeEnabled = ((clientInfo.metadata.status & static_cast<uint32>(ClientStatusFlags::DeveloperModeEnabled)) != 0) ? 1 : 0;
                Platform::Snprintf(textBuffer, sizeof(textBuffer), "\nClient Developer Mode Status Flag: %u", developerModeEnabled);
                pBlock->Write(reinterpret_cast<const uint8*>(textBuffer), strlen(textBuffer));

                const uint32 haltOnConnect = ((clientInfo.metadata.status & static_cast<uint32>(ClientStatusFlags::HaltOnConnect)) != 0) ? 1 : 0;
                Platform::Snprintf(textBuffer, sizeof(textBuffer), "\nClient Halt On Connect Status Flag: %u", haltOnConnect);
                pBlock->Write(reinterpret_cast<const uint8*>(textBuffer), strlen(textBuffer));

                const uint32 gpuCrashDumpsEnabled = ((clientInfo.metadata.status & static_cast<uint32>(ClientStatusFlags::GpuCrashDumpsEnabled)) != 0) ? 1 : 0;
                Platform::Snprintf(textBuffer, sizeof(textBuffer), "\nClient Gpu Crash Dumps Enabled Status Flag: %u", gpuCrashDumpsEnabled);
                pBlock->Write(reinterpret_cast<const uint8*>(textBuffer), strlen(textBuffer));

                const uint32 pipelineDumpsEnabled = ((clientInfo.metadata.status & static_cast<uint32>(ClientStatusFlags::PipelineDumpsEnabled)) != 0) ? 1 : 0;
                Platform::Snprintf(textBuffer, sizeof(textBuffer), "\nClient Pipeline Dumps Enabled Status Flag: %u", pipelineDumpsEnabled);
                pBlock->Write(reinterpret_cast<const uint8*>(textBuffer), strlen(textBuffer));
#endif

                // Write the process id
                Platform::Snprintf(textBuffer, sizeof(textBuffer), "\nClient Process Id: %u", clientInfo.processId);
                pBlock->Write(reinterpret_cast<const uint8*>(textBuffer), strlen(textBuffer));

                pContext->responseDataFormat = URIDataFormat::Text;

                result = Result::Success;
            }
        }

        return result;
    }
} // DevDriver
