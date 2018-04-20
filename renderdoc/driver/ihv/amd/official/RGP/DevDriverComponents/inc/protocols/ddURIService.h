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
* @file  ddURIService.h
* @brief Class declaration for URIService.
***********************************************************************************************************************
*/

#pragma once

#include "gpuopen.h"
#include "ddPlatform.h"
#include "ddUriInterface.h"
#include "util/sharedptr.h"
#include "ddTransferManager.h"

namespace DevDriver
{
    namespace URIProtocol
    {
        // We alias these types for backwards compatibility
        using URIRequestContext = DevDriver::URIRequestContext;
        using ResponseDataFormat = DevDriver::URIDataFormat;

        // Base class for URI services
        class URIService : public IService
        {
        public:
            virtual ~URIService() {}

            // Returns the name of the service
            const char* GetName() const override final { return m_name; }

        protected:
            URIService(const char* pName)
            {
                // Copy the service name into a member variable for later use.
                Platform::Strncpy(m_name, pName, sizeof(m_name));
            }

            DD_STATIC_CONST uint32 kServiceNameSize = 64;

            // The name of the service
            char m_name[kServiceNameSize];
        };
    }
} // DevDriver
