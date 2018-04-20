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

#include "protocols/ddURIClient.h"
#include "protocols/ddURIProtocol.h"
#include "msgChannel.h"
#include "ddTransferManager.h"

#define URI_CLIENT_MIN_MAJOR_VERSION URI_INITIAL_VERSION
#define URI_CLIENT_MAX_MAJOR_VERSION URI_RESPONSE_FORMATS_VERSION

namespace DevDriver
{
    namespace URIProtocol
    {
        static constexpr URIDataFormat ResponseFormatToUriFormat(ResponseDataFormat format)
        {
            static_assert(static_cast<uint32>(ResponseDataFormat::Unknown) == static_cast<uint32>(URIDataFormat::Unknown),
                          "ResponseDataFormat and URIDataFormat no longer match");
            static_assert(static_cast<uint32>(ResponseDataFormat::Text) == static_cast<uint32>(URIDataFormat::Text),
                          "ResponseDataFormat and URIDataFormat no longer match");
            static_assert(static_cast<uint32>(ResponseDataFormat::Binary) == static_cast<uint32>(URIDataFormat::Binary),
                          "ResponseDataFormat and URIDataFormat no longer match");
            static_assert(static_cast<uint32>(ResponseDataFormat::Count) == static_cast<uint32>(URIDataFormat::Count),
                          "ResponseDataFormat and URIDataFormat no longer match");
            return static_cast<URIDataFormat>(format);
        }

        // =====================================================================================================================
        URIClient::URIClient(IMsgChannel* pMsgChannel)
            : BaseProtocolClient(pMsgChannel, Protocol::URI, URI_CLIENT_MIN_MAJOR_VERSION, URI_CLIENT_MAX_MAJOR_VERSION)
        {
            memset(&m_context, 0, sizeof(m_context));
        }

        // =====================================================================================================================
        URIClient::~URIClient()
        {
        }

        // =====================================================================================================================
        Result URIClient::RequestURI(
            const char*         pRequestString,
            ResponseHeader*     pResponseHeader)
        {
            Result result = Result::Error;

            if ((m_context.state == State::Idle) &&
                (pRequestString != nullptr))
            {
                // Set up the request payload.
                URIPayload payload = {};
                payload.command = URIMessage::URIRequest;
                Platform::Strncpy(payload.uriRequest.uriString, pRequestString, sizeof(payload.uriRequest.uriString));

                // Issue a transaction.
                result = Transact(payload, payload);
                if ((result == Result::Success) && (payload.command == URIMessage::URIResponse))
                {
                    // Set up some defaults for the response fields.
                    URIDataFormat responseDataFormat = URIDataFormat::Text;

                    // We've successfully received the response. Extract the relevant fields from the response.
                    const URIResponsePayload& responsePayload = payload.uriResponse;
                    const TransferProtocol::BlockId remoteBlockId = responsePayload.blockId;
                    result = responsePayload.result;

                    if (m_pSession->GetVersion() >= URI_RESPONSE_FORMATS_VERSION)
                    {
                        responseDataFormat = ResponseFormatToUriFormat(responsePayload.format);
                    }

                    if (result == Result::Success)
                    {
                        // Attempt to open the pull block containing the response data.
                        // @Todo: Detect if the service returns the invalid block ID and treat that as a success.
                        //        It will require a new protocol version because existing clients will fail if
                        //        the invalid block ID is returned in lieu of a block of size 0.
                        TransferProtocol::PullBlock* pPullBlock =
                            m_pMsgChannel->GetTransferManager().OpenPullBlock(GetRemoteClientId(), remoteBlockId);

                        if (pPullBlock != nullptr)
                        {
                            m_context.pBlock = pPullBlock;
                            const size_t blockSize = m_context.pBlock->GetBlockDataSize();

                            // We successfully opened the block. Return the block data size and format via the header.
                            // The header is optional so check for nullptr first.
                            if (pResponseHeader != nullptr)
                            {
                                pResponseHeader->responseDataSizeInBytes = blockSize;
                                pResponseHeader->responseDataFormat      = responseDataFormat;
                            }

                            // If the block size is non-zero we move to the read state
                            if (blockSize > 0)
                            {
                                // Set up internal state.
                                m_context.state = State::ReadResponse;
                            }
                            else // If the block size is zero we automatically close it and move back to idle
                            {
                                // @Todo: Pass the invalid block ID back instead of a zero sized block.
                                m_context.state = State::Idle;
                                m_pMsgChannel->GetTransferManager().ClosePullBlock(&m_context.pBlock);
                            }
                        }
                        else
                        {
                            // Failed to open the response block.
                            result = Result::Error;
                        }
                    }
                    else if (result == Result::Unavailable)
                    {
                        // The request failed because the requested service was not available on the remote server.
                    }
                    else
                    {
                        // The request failed on the remote server for an unknown reason.
                        result = Result::Error;
                    }
                }
                else
                {
                    // If we fail the transaction or it's an unexpected response, fail the request.
                    result = Result::Error;
                }
            }

            return result;
        }

#if !DD_VERSION_SUPPORTS(GPUOPEN_URI_RESPONSE_FORMATS_VERSION)
        // =====================================================================================================================
        Result URIClient::RequestURI(
            const char*         pRequestString,
            size_t*             pResponseSizeInBytes)
        {
            Result result = Result::Error;

            if ((pRequestString != nullptr) &&
                (pResponseSizeInBytes != nullptr))
            {
                // Pass a header into the request function so we can get the response size.
                ResponseHeader header = {};
                result = RequestURI(pRequestString, &header);

                // If the request was successful, extract the response size and return it.
                if (result == Result::Success)
                {
                    *pResponseSizeInBytes = header.responseDataSizeInBytes;
                }
            }

            return result;
        }
#endif

        // =====================================================================================================================
        Result URIClient::ReadResponse(uint8* pDstBuffer, size_t bufferSize, size_t* pBytesRead)
        {
            Result result = Result::Error;

            if (m_context.state == State::ReadResponse)
            {
                result = m_context.pBlock->Read(pDstBuffer, bufferSize, pBytesRead);

                // If we reach the end of the stream or we encounter an error, we should transition back to the idle state.
                if ((result == Result::EndOfStream) ||
                    (result == Result::Error))
                {
                    m_context.state = State::Idle;
                    m_pMsgChannel->GetTransferManager().ClosePullBlock(&m_context.pBlock);
                }
            }

            return result;
        }

        // =====================================================================================================================
        Result URIClient::AbortRequest()
        {
            Result result = Result::Error;

            if (m_context.state == State::ReadResponse)
            {
                m_context.state = State::Idle;
                m_pMsgChannel->GetTransferManager().ClosePullBlock(&m_context.pBlock);

                result = Result::Success;
            }

            return result;
        }

        // =====================================================================================================================
        void URIClient::ResetState()
        {
            // Close the pull block if it's still valid.
            if (m_context.pBlock != nullptr)
            {
                m_pMsgChannel->GetTransferManager().ClosePullBlock(&m_context.pBlock);
            }

            memset(&m_context, 0, sizeof(m_context));
        }
    }

} // DevDriver
