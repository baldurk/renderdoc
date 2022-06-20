/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2022 Baldur Karlsson
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
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 ******************************************************************************/

#pragma once

#include "metal_types.h"

namespace ObjC
{
void Get_defaultLibraryData(bytebuf &buffer);
MTL::Texture *Get_Texture(MTL::Drawable *drawable);
CA::MetalLayer *Get_Layer(MTL::Drawable *drawable);
void CALayer_GetSize(void *layerHandle, int &width, int &height);
void CAMetalLayer_Set_drawableSize(void *layerHandle, int w, int h);
void CAMetalLayer_Set_device(void *layerHandle, MTL::Device *device);
void CAMetalLayer_Set_framebufferOnly(void *layerHandle, bool enable);
void CAMetalLayer_Set_pixelFormat(void *layerHandle, MTL::PixelFormat format);
CA::MetalDrawable *CAMetalLayer_nextDrawable(void *layerHandle);
};
