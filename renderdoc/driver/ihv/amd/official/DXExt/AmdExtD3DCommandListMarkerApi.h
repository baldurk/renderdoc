/*
***********************************************************************************************************************
*
*  Copyright (c) 2017 Advanced Micro Devices, Inc. All rights reserved.
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
* @file  AmdExtD3DCommandListMarkerApi.h
* @brief
*    AMD D3D Command List Marker API include file.
***********************************************************************************************************************
*/

#pragma once

#include <unknwn.h>

/**
***********************************************************************************************************************
* @brief D3D Command List Marker extension API object
***********************************************************************************************************************
*/
interface __declspec(uuid("735F1F3A-555D-4F70-AB92-7DB4A3AB1D28"))
IAmdExtD3DCommandListMarker : public IUnknown
{
public:
    /// Set a command list marker to indicate the beginning of a rendering pass
    virtual VOID PushMarker(const char* pMarker) = 0;
    /// Set a command list marker to indicate the end of the current rendering pass
    virtual VOID PopMarker() = 0;
    /// Set a command list marker to indicate a rendering activity
    virtual VOID SetMarker(const char* pMarker) = 0;
};