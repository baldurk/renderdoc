/*
***********************************************************************************************************************
*
*  Copyright (c) 2016 Advanced Micro Devices, Inc. All rights reserved.
*
*  Permission is hereby granted, free of charge, to any person obtaining a copy
*  of this software and associated documentation files (the "Software"), to deal
*  in the Software without restriction, including without limitation the rights
*  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
*  copies of the Software, and to permit persons to whom the Software is
*  furnished to do so, subject to the following conditions:
*
*  The above copyright notice and this permission notice shall be included in
*  all copies or substantial portions of the Software.
*
*  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
*  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
*  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
*  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
*  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
*  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
*  THE SOFTWARE.
*
***********************************************************************************************************************
*/
/**
***********************************************************************************************************************
* @file  AmdExtD3D.h
* @brief AMD D3D Exension API factory include file.
***********************************************************************************************************************
*/
#pragma once

#include <unknwn.h>

/*      All AMD extensions contain the standard IUnknown interface:
 * virtual HRESULT STDMETHODCALLTYPE QueryInterface(
 *             REFIID riid,
 *             _COM_Outptr_ void __RPC_FAR *__RPC_FAR *ppvObject) = 0;
 * virtual ULONG STDMETHODCALLTYPE AddRef( void) = 0;
 * virtual ULONG STDMETHODCALLTYPE Release( void) = 0;
 */


// The app must use GetProcAddress, etc. to retrive this exported function
// The associated typedef provides a convenient way to define the function pointer
HRESULT __cdecl AmdExtD3DCreateInterface(
    IUnknown*   pOuter,     ///< [in] object on which to base this new interface; usually a D3D device
    REFIID      riid,       ///< ID of the requested interface
    void**      ppvObject); ///< [out] The result interface object
typedef HRESULT (__cdecl *PFNAmdExtD3DCreateInterface)(IUnknown* pOuter, REFIID riid, void** ppvObject);

/**
***********************************************************************************************************************
* @brief Abstract factory for extension interfaces
*
* Each extension interface (e.g. tessellation) will derive from this class
***********************************************************************************************************************
*/
interface __declspec (uuid("014937EC-9288-446F-A9AC-D75A8E3A984F"))
IAmdExtD3DFactory : public IUnknown
{
public:
    virtual HRESULT CreateInterface(
        IUnknown* pOuter,           ///< [in] An object on which to base this new interface; the required object type
                                    ///< is usually a device object but not always
        REFIID    riid,             ///< The ID of the requested interface
        void**    ppvObject) = 0;   ///< [out] The result interface object
};
