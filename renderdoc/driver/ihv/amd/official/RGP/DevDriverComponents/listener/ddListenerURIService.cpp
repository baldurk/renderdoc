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

#include "ddListenerURIService.h"
#include "msgChannel.h"
#include "ddTransferManager.h"
#include "listenerCore.h"

namespace DevDriver
{
    // =====================================================================================================================
    ListenerURIService::ListenerURIService()
        : m_pListenerCore(nullptr)
    {
    }

    // =====================================================================================================================
    ListenerURIService::~ListenerURIService()
    {
    }

    // =====================================================================================================================
    Result ListenerURIService::HandleRequest(URIRequestContext* pContext)
    {
        DD_ASSERT(pContext != nullptr);

        Result result = Result::Error;

        // We can only handle requests if a valid listener core has been bound.
        if (m_pListenerCore != nullptr)
        {
            // We currently only handle the "info" command.
            // All other commands will result in an error.
            if (strcmp(pContext->pRequestArguments, "clients") == 0)
            {
                // Get a list of all currently connected clients.
                const std::vector<DevDriver::ClientInfo> connectedClients = m_pListenerCore->GetConnectedClientList();

                // @todo: Replace with a string builder utility class.
                char textBuffer[256];

                // Write all the info into the response block as plain text.
                auto& pBlock = pContext->pResponseBlock;

                // Write the header
                Platform::Snprintf(textBuffer, sizeof(textBuffer), "--- %u Connected Clients ---", static_cast<uint32>(connectedClients.size()));
                pBlock->Write(reinterpret_cast<const uint8*>(textBuffer), strlen(textBuffer));

                for (uint32 clientIndex = 0; clientIndex < static_cast<uint32>(connectedClients.size()); ++clientIndex)
                {
                    const ClientInfo& clientInfo = connectedClients[clientIndex];

                    // Write the client header
                    Platform::Snprintf(textBuffer, sizeof(textBuffer), "\n\n--- Client %u ---", clientIndex);
                    pBlock->Write(reinterpret_cast<const uint8*>(textBuffer), strlen(textBuffer));

                    // Write the name
                    Platform::Snprintf(textBuffer, sizeof(textBuffer), "\nName: %s", clientInfo.clientName);
                    pBlock->Write(reinterpret_cast<const uint8*>(textBuffer), strlen(textBuffer));

                    // Write the description
                    Platform::Snprintf(textBuffer, sizeof(textBuffer), "\nDescription: %s", clientInfo.clientDescription);
                    pBlock->Write(reinterpret_cast<const uint8*>(textBuffer), strlen(textBuffer));

                    // Write the process id
                    Platform::Snprintf(textBuffer, sizeof(textBuffer), "\nProcess Id: %u", clientInfo.clientPid);
                    pBlock->Write(reinterpret_cast<const uint8*>(textBuffer), strlen(textBuffer));

                    // Write the client id
                    Platform::Snprintf(textBuffer, sizeof(textBuffer), "\nClient Id: %u", static_cast<uint32>(clientInfo.clientId));
                    pBlock->Write(reinterpret_cast<const uint8*>(textBuffer), strlen(textBuffer));

                    // Write has been identified
                    Platform::Snprintf(textBuffer, sizeof(textBuffer), "\nHas Been Identified: %u", static_cast<uint32>(clientInfo.hasBeenIdentified));
                    pBlock->Write(reinterpret_cast<const uint8*>(textBuffer), strlen(textBuffer));
                }

                pContext->responseDataFormat = URIDataFormat::Text;

                result = Result::Success;
            }
            else if (strcmp(pContext->pRequestArguments, "transports") == 0)
            {
                // Get a list of all currently managed transports.
                const std::vector<std::shared_ptr<IListenerTransport>>& managedTransports = m_pListenerCore->GetManagedTransports();

                // @todo: Replace with a string builder utility class.
                char textBuffer[256];

                // Write all the info into the response block as plain text.
                auto& pBlock = pContext->pResponseBlock;

                // Write the header
                Platform::Snprintf(textBuffer, sizeof(textBuffer), "--- %u Transports ---", static_cast<uint32>(managedTransports.size()));
                pBlock->Write(reinterpret_cast<const uint8*>(textBuffer), strlen(textBuffer));

                for (uint32 transportIndex = 0; transportIndex < static_cast<uint32>(managedTransports.size()); ++transportIndex)
                {
                    const std::shared_ptr<IListenerTransport>& transport = managedTransports[transportIndex];

                    // Write the transport header
                    Platform::Snprintf(textBuffer, sizeof(textBuffer), "\n\n--- Transport %u ---", transportIndex);
                    pBlock->Write(reinterpret_cast<const uint8*>(textBuffer), strlen(textBuffer));

                    // Write the name
                    Platform::Snprintf(textBuffer, sizeof(textBuffer), "\nName: %s", transport->GetTransportName());
                    pBlock->Write(reinterpret_cast<const uint8*>(textBuffer), strlen(textBuffer));

                    // Write the handle
                    Platform::Snprintf(textBuffer, sizeof(textBuffer), "\nHandle: %u", transport->GetHandle());
                    pBlock->Write(reinterpret_cast<const uint8*>(textBuffer), strlen(textBuffer));

                    // Write the description
                    Platform::Snprintf(textBuffer, sizeof(textBuffer), "\nIs Forwarding Connection: %u", static_cast<uint32>(transport->ForwardingConnection()));
                    pBlock->Write(reinterpret_cast<const uint8*>(textBuffer), strlen(textBuffer));
                }

                pContext->responseDataFormat = URIDataFormat::Text;

                result = Result::Success;
            }
            else if (strcmp(pContext->pRequestArguments, "info") == 0)
            {
                // @todo: Replace with a string builder utility class.
                char textBuffer[256];

                // Write all the info into the response block as plain text.
                auto& pBlock = pContext->pResponseBlock;

                const IClientManager* pClientManager = m_pListenerCore->GetClientManager();
                const ListenerCreateInfo& createInfo = m_pListenerCore->GetCreateInfo();

                // Write the listener description
                Platform::Snprintf(textBuffer, sizeof(textBuffer), "Listener Description: %s", createInfo.description);
                pBlock->Write(reinterpret_cast<const uint8*>(textBuffer), strlen(textBuffer));

                // Write the listener UWP flag
                Platform::Snprintf(textBuffer, sizeof(textBuffer), "\nListener UWP Support: %u", static_cast<uint32>(createInfo.flags.enableUWP));
                pBlock->Write(reinterpret_cast<const uint8*>(textBuffer), strlen(textBuffer));

                // Write the listener server flag
                Platform::Snprintf(textBuffer, sizeof(textBuffer), "\nListener Server Support: %u", static_cast<uint32>(createInfo.flags.enableServer));
                pBlock->Write(reinterpret_cast<const uint8*>(textBuffer), strlen(textBuffer));

                // Write the name of the client manager
                Platform::Snprintf(textBuffer, sizeof(textBuffer), "\nClient Manager Name: %s", pClientManager->GetClientManagerName());
                pBlock->Write(reinterpret_cast<const uint8*>(textBuffer), strlen(textBuffer));

                // Write the host client id of the client manager
                Platform::Snprintf(textBuffer, sizeof(textBuffer), "\nClient Manager Host Client Id: %u", static_cast<uint32>(pClientManager->GetHostClientId()));
                pBlock->Write(reinterpret_cast<const uint8*>(textBuffer), strlen(textBuffer));

                pContext->responseDataFormat = URIDataFormat::Text;

                result = Result::Success;
            }
        }

        return result;
    }
} // DevDriver
