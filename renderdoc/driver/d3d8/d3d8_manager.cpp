/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2017 Baldur Karlsson
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

#include "d3d8_manager.h"
#include "d3d8_resources.h"

bool D3D8ResourceManager::SerialisableResource(ResourceId id, D3D8ResourceRecord *record)
{
  return true;
}

ResourceId D3D8ResourceManager::GetID(IUnknown *res)
{
  return GetResID(res);
}

bool D3D8ResourceManager::ResourceTypeRelease(IUnknown *res)
{
  if(res)
    res->Release();

  return true;
}

bool D3D8ResourceManager::Force_InitialState(IUnknown *res, bool prepare)
{
  return false;
}

bool D3D8ResourceManager::Need_InitialStateChunk(IUnknown *res)
{
  return true;
}

bool D3D8ResourceManager::Prepare_InitialState(IUnknown *res)
{
  // TODO
  return false;
}

bool D3D8ResourceManager::Serialise_InitialState(ResourceId id, IUnknown *res)
{
  // TODO
  return false;
}

void D3D8ResourceManager::Create_InitialState(ResourceId id, IUnknown *live, bool hasData)
{
}

void D3D8ResourceManager::Apply_InitialState(IUnknown *live, InitialContentData data)
{
  // TODO
}
