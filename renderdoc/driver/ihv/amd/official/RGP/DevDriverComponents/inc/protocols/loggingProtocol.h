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

#pragma once

#include "gpuopen.h"
#include <cstring>

#define LOGGING_PROTOCOL_MAJOR_VERSION 3
#define LOGGING_PROTOCOL_MINOR_VERSION 0

#define LOGGING_INTERFACE_VERSION ((LOGGING_INTERFACE_MAJOR_VERSION << 16) | LOGGING_INTERFACE_MINOR_VERSION)

#define LOGGING_PROTOCOL_MINIMUM_MAJOR_VERSION 1

/*
***********************************************************************************************************************
*| Version | Change Description                                                                                       |
*| ------- | ---------------------------------------------------------------------------------------------------------|
*|  3.0    | Variably sized log message support                                                                       |
*|  2.0    | Refactor to simplify protocol + API semantics                                                            |
*|  1.0    | Initial version                                                                                          |
***********************************************************************************************************************
*/

#define LOGGING_LARGE_MESSAGES_VERSION 3
#define LOGGING_REFACTOR_VERSION 2
#define LOGGING_INITIAL_VERSION 1

namespace DevDriver
{

    namespace LoggingProtocol
    {
        ///////////////////////
        // Logging Constants
        DD_STATIC_CONST uint32 kLegacyLoggingPayloadSize = 152;

        // Subtract the logging header size from the max payload size to get the max size for any logging payload.
        DD_STATIC_CONST uint32 kLoggingHeaderSize = sizeof(uint64);
        DD_STATIC_CONST uint32 kMaxLoggingPayloadSize = (kMaxPayloadSizeInBytes - kLoggingHeaderSize);

        ///////////////////////
        // Logging Protocol
        enum struct LoggingMessage : uint32
        {
            Unknown = 0,
            EnableLoggingRequest,
            EnableLoggingResponse,
            DisableLogging,
            QueryCategoriesRequest,
            QueryCategoriesNumResponse,
            QueryCategoriesDataResponse,
            LogMessage,
            LogMessageSentinel,
            Count
        };

        ///////////////////////
        // Logging Types
        typedef uint64 LoggingCategory;

        // WARNING: Do not increase this without also changing the payload size.
        DD_STATIC_CONST uint32 kMaxCategoryCount = 64;
        DD_STATIC_CONST uint32 kMaxCategoryIndex = (kMaxCategoryCount - 1);
        DD_STATIC_CONST LoggingCategory kAllLoggingCategories = static_cast<LoggingCategory>(-1);

        // offset definition for the default categories.
        // we are reserving a total of four, giving us two we can use in the future
        enum DefaultCategories : LoggingCategory
        {
            kGeneralCategoryOffset,
            kSystemCategoryOffset,
            kReservedOffset1,
            kReservedOffset2,
            kReservedCategoryCount
        };

        // define categories that are available to client applications
        DD_STATIC_CONST uint32 kDefinableCategoryCount = kMaxCategoryCount - kReservedCategoryCount;
        DD_STATIC_CONST LoggingCategory kDefinableCategoryMask = ((LoggingCategory)1 << kDefinableCategoryCount) - 1;

        static_assert(kDefinableCategoryCount <= kMaxCategoryCount, "Invalid kReservedCategoryCount");
        // ensure that the available logging category mask is wholly contained inside the all category mask
        static_assert((kDefinableCategoryMask & kAllLoggingCategories) == kDefinableCategoryMask,
                      "Invalid category masks defined");

        // define the default category masks start so that the first mask is outside of the kDefinableCategoryMask
        enum BaseCategoryMasks : LoggingCategory
        {
            kGeneralCategoryMask = ((LoggingCategory)1 << (kDefinableCategoryCount + kGeneralCategoryOffset)),
            kSystemCategoryMask = ((LoggingCategory)1 << (kDefinableCategoryCount + kSystemCategoryOffset))
        };

        // test to make sure that the base logging category bitmasks are contained inside the all logging category mask
        static_assert((kAllLoggingCategories & kGeneralCategoryMask) == kGeneralCategoryMask,
                      "Invalid category masks defined");
        static_assert((kAllLoggingCategories & kSystemCategoryMask) == kSystemCategoryMask,
                      "Invalid category masks defined");

        // ensure that the base logging categories do not overlap with the available logging category mask
        static_assert((kDefinableCategoryMask & kGeneralCategoryMask) == 0, "Invalid category masks defined");
        static_assert((kDefinableCategoryMask & kSystemCategoryMask) == 0, "Invalid category masks defined");

        // A logging category is defined as both a bitmask + a name
        DD_NETWORK_STRUCT(NamedLoggingCategory, 8)
        {
            LoggingCategory category;
            char            name[kMaxLoggingPayloadSize - sizeof(LoggingCategory)];
        };

        DD_CHECK_SIZE(NamedLoggingCategory, kMaxLoggingPayloadSize);

        // ensure that we cannot define more categories than we have bits more
        static_assert(kMaxCategoryCount <= 64, "kMaxCategoryCount is too big to fit inside the payload.");

        // logging filter definition
        // TODO: consider replacing this with manual bitshifting
        DD_NETWORK_STRUCT(LoggingFilter, 8)
        {
            LoggingCategory        category;
            uint8                  reserved[7];
            LogLevel               priority;
        };

        DD_CHECK_SIZE(LoggingFilter, 16);

        // logging message definition. Filter is included so the client can identify the message.
        DD_NETWORK_STRUCT(LogMessage, 8)
        {
            LoggingFilter filter;
            char message[kMaxLoggingPayloadSize - sizeof(LoggingFilter)];
        };

        DD_CHECK_SIZE(LogMessage, kMaxLoggingPayloadSize);

        ///////////////////////
        // Logging Payloads
        DD_NETWORK_STRUCT(LoggingHeader, 4)
        {
            LoggingMessage command;
            // Padding for backwards compatibility. Initial protocol defined struct as 8 byte aligned, so first 8 bytes
            // were always used for the header. We mark this as char to prevent the compiler from initializing this.
            char _padding[4];

            constexpr LoggingHeader(LoggingMessage message)
                : command(message)
                , _padding()
            {
            }
        };

        DD_CHECK_SIZE(LoggingHeader, 8);

        static_assert(sizeof(LoggingHeader) == kLoggingHeaderSize, "Logging header size mismatch!");

        DD_NETWORK_STRUCT(EnableLoggingRequestPayload, 8)
        {
            LoggingHeader header;
            LoggingFilter filter;

            constexpr EnableLoggingRequestPayload(const LoggingFilter& initialFilter)
                : header(LoggingMessage::EnableLoggingRequest)
                , filter(initialFilter)
            {
            }
        };

        DD_CHECK_SIZE(EnableLoggingRequestPayload, sizeof(LoggingHeader) + sizeof(LoggingFilter));

        DD_NETWORK_STRUCT(EnableLoggingResponsePayload, 4)
        {
            LoggingHeader header;
            Result result;
            // Padding for backwards compatibility. Should remove on version bump.
            char _padding[4];

            constexpr EnableLoggingResponsePayload(Result response)
                : header(LoggingMessage::EnableLoggingResponse)
                , result(response)
                , _padding()
            {
            }
        };

        DD_CHECK_SIZE(EnableLoggingResponsePayload, sizeof(LoggingHeader) + 8);

        DD_NETWORK_STRUCT(QueryCategoriesNumResponsePayload, 4)
        {
            LoggingHeader header;
            uint32 numCategories;
            // Padding for backwards compatibility. Should remove on version bump.
            char _padding[4];

            constexpr QueryCategoriesNumResponsePayload(uint32 categories)
                : header(LoggingMessage::QueryCategoriesNumResponse)
                , numCategories(categories)
                , _padding()
            {
            }
        };

        DD_CHECK_SIZE(QueryCategoriesNumResponsePayload, sizeof(LoggingHeader) + 8);

        DD_NETWORK_STRUCT(QueryCategoriesDataResponsePayload, 8)
        {
            LoggingHeader header;
            NamedLoggingCategory category;

            static void WritePayload(
                const NamedLoggingCategory& category,
                Version sessionVersion,
                size_t categoryNameSize,
                SizedPayloadContainer* pContainer)
            {
                DD_ASSERT(pContainer != nullptr);
                const size_t maxNameSize =
                    (sessionVersion >= LOGGING_LARGE_MESSAGES_VERSION) ? sizeof(category.name) : kMaxStringLength;
                const size_t finalNameSize = Platform::Min(categoryNameSize, maxNameSize);
                const uint32 payloadSize =
                    static_cast<uint32>(offsetof(QueryCategoriesDataResponsePayload, category.name) + finalNameSize);

                pContainer->payloadSize = payloadSize;

                QueryCategoriesDataResponsePayload& payload =
                    pContainer->GetPayload<QueryCategoriesDataResponsePayload>();
                payload.header = LoggingHeader(LoggingMessage::QueryCategoriesDataResponse);

                memcpy(&payload.category,
                    &category,
                    payloadSize);

                // If we had to truncate the string to fit in the payload, we want to overwrite the final character
                // with null.
                if (categoryNameSize > finalNameSize)
                {
                    payload.category.name[finalNameSize - 1] = '\0';
                }
            }
        };

        DD_CHECK_SIZE(QueryCategoriesDataResponsePayload, sizeof(LoggingHeader) + sizeof(NamedLoggingCategory));

        DD_NETWORK_STRUCT(LogMessagePayload, 8)
        {
            LoggingHeader header;
            LogMessage message;

            static void WritePayload(
                const LogMessage& message,
                Version sessionVersion,
                size_t messageSize,
                SizedPayloadContainer* pContainer)
            {
                DD_ASSERT(pContainer != nullptr);
                const size_t maxStringSize =
                    (sessionVersion >= LOGGING_LARGE_MESSAGES_VERSION) ? sizeof(message.message) : kMaxStringLength;
                const size_t finalMessageSize = Platform::Min(messageSize, maxStringSize);
                const uint32 payloadSize =
                    static_cast<uint32>(offsetof(LogMessagePayload, message.message) + finalMessageSize);

                pContainer->payloadSize = payloadSize;

                LogMessagePayload& payload = pContainer->GetPayload<LogMessagePayload>();
                payload.header = LoggingHeader(LoggingMessage::LogMessage);

                memcpy(&payload.message,
                       &message,
                       payloadSize);

                // If we had to truncate the string to fit in the payload, we want to overwrite the final character
                // with null.
                if (messageSize > finalMessageSize)
                {
                    payload.message.message[finalMessageSize - 1] = '\0';
                }
            }
        };

        DD_CHECK_SIZE(LogMessagePayload, sizeof(LoggingHeader) + sizeof(LogMessage));
    }
}
