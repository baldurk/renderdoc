/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2017-2019 Baldur Karlsson
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

#include "d3d8_common.h"
#include "d3d8_device.h"

unsigned int RefCounter8::SoftRef(WrappedD3DDevice8 *device)
{
  unsigned int ret = AddRef();
  if(device)
    device->SoftRef();
  else
    RDCWARN("No device pointer, is a deleted resource being AddRef()d?");
  return ret;
}

unsigned int RefCounter8::SoftRelease(WrappedD3DDevice8 *device)
{
  unsigned int ret = Release();
  if(device)
    device->SoftRelease();
  else
    RDCWARN("No device pointer, is a deleted resource being Release()d?");
  return ret;
}

void RefCounter8::AddDeviceSoftref(WrappedD3DDevice8 *device)
{
  if(device)
    device->SoftRef();
}

void RefCounter8::ReleaseDeviceSoftref(WrappedD3DDevice8 *device)
{
  if(device)
    device->SoftRelease();
}
