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

#include "ddURIServer.h"
#include "msgChannel.h"
#include "ddTransferManager.h"
#include "protocols/ddURIProtocol.h"

#define URI_SERVER_MIN_MAJOR_VERSION URI_INITIAL_VERSION
#define URI_SERVER_MAX_MAJOR_VERSION URI_RESPONSE_FORMATS_VERSION

namespace DevDriver
{
    namespace URIProtocol
    {
        static constexpr ResponseDataFormat UriFormatToResponseFormat(URIDataFormat format)
        {
            static_assert(static_cast<uint32>(ResponseDataFormat::Unknown) == static_cast<uint32>(URIDataFormat::Unknown),
                          "ResponseDataFormat and URIDataFormat no longer match");
            static_assert(static_cast<uint32>(ResponseDataFormat::Text) == static_cast<uint32>(URIDataFormat::Text),
                          "ResponseDataFormat and URIDataFormat no longer match");
            static_assert(static_cast<uint32>(ResponseDataFormat::Binary) == static_cast<uint32>(URIDataFormat::Binary),
                          "ResponseDataFormat and URIDataFormat no longer match");
            static_assert(static_cast<uint32>(ResponseDataFormat::Count) == static_cast<uint32>(URIDataFormat::Count),
                          "ResponseDataFormat and URIDataFormat no longer match");
            return static_cast<ResponseDataFormat>(format);
        }

        struct URISession
        {
            Version version;
            SharedPointer<TransferProtocol::ServerBlock> pBlock;
            URIPayload payload;
            bool hasQueuedPayload;

            explicit URISession()
                : version(0)
                , hasQueuedPayload(false)
            {
                memset(&payload, 0, sizeof(payload));
            }
        };

        // =====================================================================================================================
        // Parses out the parameters from a request string. (Ex. service://service-args)
        bool ExtractRequestParameters(char* pRequestString, char** ppServiceName, char** ppServiceArguments)
        {
            DD_ASSERT(pRequestString != nullptr);
            DD_ASSERT(ppServiceName != nullptr);
            DD_ASSERT(ppServiceArguments != nullptr);

            bool result = false;

            // Iterate through the null terminated string until we find the ":" character or the end of the string.
            char* pCurrentChar = pRequestString;
            while ((*pCurrentChar != ':') && (*pCurrentChar != 0))
            {
                ++pCurrentChar;
            }

            // If we haven't reached the end of the string then we've found the ":" character.
            // Otherwise this string isn't formatted correctly.
            if (*pCurrentChar != 0)
            {
                // Overwrite the ":" character in memory with a null byte to allow us to divide up the request string
                // in place.
                *pCurrentChar = 0;

                // Return the service name and arguments out of the modified request string.
                *ppServiceName = pRequestString;
                *ppServiceArguments = pCurrentChar + 3;

                result = true;
            }

            return result;
        }

        // =====================================================================================================================
        URIServer::URIServer(IMsgChannel* pMsgChannel)
            : BaseProtocolServer(pMsgChannel, Protocol::URI, URI_SERVER_MIN_MAJOR_VERSION, URI_SERVER_MAX_MAJOR_VERSION)
            , m_registeredServices(pMsgChannel->GetAllocCb())
        {
            DD_ASSERT(m_pMsgChannel != nullptr);
        }

        // =====================================================================================================================
        URIServer::~URIServer()
        {
        }

        // =====================================================================================================================
        void URIServer::Finalize()
        {
            Platform::LockGuard<Platform::Mutex> lock(m_mutex);
            BaseProtocolServer::Finalize();
        }

        // =====================================================================================================================
        bool URIServer::AcceptSession(const SharedPointer<ISession>& pSession)
        {
            DD_UNUSED(pSession);
            return true;
        }

        // =====================================================================================================================
        void URIServer::SessionEstablished(const SharedPointer<ISession>& pSession)
        {
            DD_UNUSED(pSession);

            // Allocate session data for the newly established session
            URISession* pSessionData = DD_NEW(URISession, m_pMsgChannel->GetAllocCb())();

            // Allocate a server block for use by the session.
            pSessionData->pBlock = m_pMsgChannel->GetTransferManager().OpenServerBlock();

            pSession->SetUserData(pSessionData);
        }

        // =====================================================================================================================
        void URIServer::UpdateSession(const SharedPointer<ISession>& pSession)
        {
            URISession* pSessionData = reinterpret_cast<URISession*>(pSession->GetUserData());

            // Attempt to send the session's queued payload if it has one.
            if (pSessionData->hasQueuedPayload)
            {
                Result result = pSession->Send(sizeof(pSessionData->payload), &pSessionData->payload, kNoWait);
                if (result == Result::Success)
                {
                    // We successfully sent the payload. The session can now handle new requests.
                    pSessionData->hasQueuedPayload = false;
                }
            }

            // We can only receive new messages if we don't currently have a queued payload.
            if (!pSessionData->hasQueuedPayload)
            {
                // Receive and handle any new requests.
                uint32 bytesReceived = 0;
                Result result = pSession->Receive(sizeof(pSessionData->payload), &pSessionData->payload, &bytesReceived, kNoWait);

                if (result == Result::Success)
                {
                    // Make sure we receive a correctly sized payload.
                    DD_ASSERT(sizeof(pSessionData->payload) == bytesReceived);

                    // Make sure the payload is a uri request since it's the only payload type we should ever receive.
                    DD_ASSERT(pSessionData->payload.command == URIMessage::URIRequest);

                    // Reset the block associated with the session so we can write new data into it.
                    pSessionData->pBlock->Reset();

                    // Attempt to extract the request string.
                    char* pRequestString = pSessionData->payload.uriRequest.uriString;
                    char* pServiceName = nullptr;
                    char* pServiceArguments = nullptr;
                    result = ExtractRequestParameters(pRequestString, &pServiceName, &pServiceArguments) ? Result::Success : Result::Error;

                    if (result == Result::Success)
                    {
                        // We've successfully extracted the request parameters.
                        // Lock the mutex and look up the requested service if it's available.
                        m_mutex.Lock();

                        IService* pService = FindService(pServiceName);

                        m_mutex.Unlock();

                        // Check if the requested service was successfully located.
                        if (pService != nullptr)
                        {
                            // Handle the request using the appropriate service.
                            URIRequestContext context = {};
                            context.pRequestArguments = pServiceArguments;
                            context.pResponseBlock = pSessionData->pBlock;
                            context.responseDataFormat = URIDataFormat::Unknown;

                            result = pService->HandleRequest(&context);

                            // Close the response block.
                            pSessionData->pBlock->Close();

                            // Assemble the response payload.
                            pSessionData->payload.command = URIMessage::URIResponse;
                            // Return the block id and associate the block with the session if we successfully handled the request.
                            pSessionData->payload.uriResponse.result = result;
                            pSessionData->payload.uriResponse.blockId = ((result == Result::Success) ? pSessionData->pBlock->GetBlockId()
                                                                         : TransferProtocol::kInvalidBlockId);
                            // We send this data back regardless of protocol version, but it will only be read
                            // in a v2 session.
                            pSessionData->payload.uriResponse.format = UriFormatToResponseFormat(context.responseDataFormat);

                            // Mark the session as having a queued payload if we fail to send the response.
                            if (pSession->Send(sizeof(pSessionData->payload), &pSessionData->payload, kNoWait) != Result::Success)
                            {
                                pSessionData->hasQueuedPayload = true;
                            }
                        }
                        else
                        {
                            // Failed to locate appropriate service.

                            // Assemble the response payload.
                            pSessionData->payload.command = URIMessage::URIResponse;
                            pSessionData->payload.uriResponse.result = Result::Unavailable;
                            pSessionData->payload.uriResponse.blockId = TransferProtocol::kInvalidBlockId;
                            pSessionData->payload.uriResponse.format = ResponseDataFormat::Unknown;

                            // Mark the session as having a queued payload if we fail to send the response.
                            if (pSession->Send(sizeof(pSessionData->payload), &pSessionData->payload, kNoWait) != Result::Success)
                            {
                                pSessionData->hasQueuedPayload = true;
                            }
                        }
                    }
                    else
                    {
                        // Failed to parse request parameters.

                        // Assemble the response payload.
                        pSessionData->payload.command = URIMessage::URIResponse;
                        pSessionData->payload.uriResponse.result = Result::Error;
                        pSessionData->payload.uriResponse.blockId = TransferProtocol::kInvalidBlockId;
                        pSessionData->payload.uriResponse.format = ResponseDataFormat::Unknown;

                        // Mark the session as having a queued payload if we fail to send the response.
                        if (pSession->Send(sizeof(pSessionData->payload), &pSessionData->payload, kNoWait) != Result::Success)
                        {
                            pSessionData->hasQueuedPayload = true;
                        }
                    }
                }
            }
        }

        // =====================================================================================================================
        void URIServer::SessionTerminated(const SharedPointer<ISession>& pSession, Result terminationReason)
        {
            DD_UNUSED(terminationReason);
            URISession *pURISession = reinterpret_cast<URISession*>(pSession->SetUserData(nullptr));

            // Free the session data
            if (pURISession != nullptr)
            {
                // Release the session's server block before destroying it.
                if (!pURISession->pBlock.IsNull())
                {
                    m_pMsgChannel->GetTransferManager().CloseServerBlock(pURISession->pBlock);
                }

                DD_DELETE(pURISession, m_pMsgChannel->GetAllocCb());
            }
        }

        // =====================================================================================================================
        Result URIServer::RegisterService(IService* pService)
        {
            Platform::LockGuard<Platform::Mutex> lock(m_mutex);

            const Result result = m_registeredServices.PushBack(pService) ? Result::Success : Result::Error;

            return result;
        }

        // =====================================================================================================================
        Result URIServer::UnregisterService(IService* pService)
        {
            Platform::LockGuard<Platform::Mutex> lock(m_mutex);

            const Result result = m_registeredServices.Remove(pService) ? Result::Success : Result::Error;

            return result;
        }

        // =====================================================================================================================
        IService* URIServer::FindService(const char* pServiceName)
        {
            IService* pService = nullptr;

            for (size_t serviceIndex = 0; serviceIndex < m_registeredServices.Size(); ++serviceIndex)
            {
                if (strcmp(m_registeredServices[serviceIndex]->GetName(), pServiceName) == 0)
                {
                    pService = m_registeredServices[serviceIndex];
                    break;
                }
            }

            return pService;
        }
    }
} // DevDriver
