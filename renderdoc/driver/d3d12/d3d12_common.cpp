/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2016 Baldur Karlsson
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

#include "d3d12_common.h"
#include "driver/dxgi/dxgi_wrapped.h"

template <>
void Serialiser::Serialise(const char *name, D3D12_RESOURCE_DESC &el)
{
  ScopedContext scope(this, name, "D3D12_RESOURCE_DESC", 0, true);

  Serialise("Dimension", el.Dimension);
  Serialise("Alignment", el.Alignment);
  Serialise("Width", el.Width);
  Serialise("Height", el.Height);
  Serialise("DepthOrArraySize", el.DepthOrArraySize);
  Serialise("MipLevels", el.MipLevels);
  Serialise("Format", el.Format);
  Serialise("SampleDesc", el.SampleDesc);
  Serialise("Layout", el.Layout);
  Serialise("Flags", el.Flags);
}

string ToStrHelper<false, D3D12_RESOURCE_DIMENSION>::Get(const D3D12_RESOURCE_DIMENSION &el)
{
  switch(el)
  {
    TOSTR_CASE_STRINGIZE(D3D12_RESOURCE_DIMENSION_UNKNOWN)
    TOSTR_CASE_STRINGIZE(D3D12_RESOURCE_DIMENSION_BUFFER)
    TOSTR_CASE_STRINGIZE(D3D12_RESOURCE_DIMENSION_TEXTURE1D)
    TOSTR_CASE_STRINGIZE(D3D12_RESOURCE_DIMENSION_TEXTURE2D)
    TOSTR_CASE_STRINGIZE(D3D12_RESOURCE_DIMENSION_TEXTURE3D)
    default: break;
  }

  return StringFormat::Fmt("D3D12_RESOURCE_DIMENSION<%d>", el);
}

string ToStrHelper<false, D3D12_TEXTURE_LAYOUT>::Get(const D3D12_TEXTURE_LAYOUT &el)
{
  switch(el)
  {
    TOSTR_CASE_STRINGIZE(D3D12_TEXTURE_LAYOUT_UNKNOWN)
    TOSTR_CASE_STRINGIZE(D3D12_TEXTURE_LAYOUT_ROW_MAJOR)
    TOSTR_CASE_STRINGIZE(D3D12_TEXTURE_LAYOUT_64KB_UNDEFINED_SWIZZLE)
    TOSTR_CASE_STRINGIZE(D3D12_TEXTURE_LAYOUT_64KB_STANDARD_SWIZZLE)
    default: break;
  }

  return StringFormat::Fmt("D3D12_TEXTURE_LAYOUT<%d>", el);
}

string ToStrHelper<false, D3D12_RESOURCE_FLAGS>::Get(const D3D12_RESOURCE_FLAGS &el)
{
  string ret;

  if(el & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET)
    ret += " | D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET";
  if(el & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL)
    ret += " | D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL";
  if(el & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS)
    ret += " | D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS";
  if(el & D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE)
    ret += " | D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE";
  if(el & D3D12_RESOURCE_FLAG_ALLOW_CROSS_ADAPTER)
    ret += " | D3D12_RESOURCE_FLAG_ALLOW_CROSS_ADAPTER";
  if(el & D3D12_RESOURCE_FLAG_ALLOW_SIMULTANEOUS_ACCESS)
    ret += " | D3D12_RESOURCE_FLAG_ALLOW_SIMULTANEOUS_ACCESS";

  if(!ret.empty())
    ret = ret.substr(3);

  return ret;
}