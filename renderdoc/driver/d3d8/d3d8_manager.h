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

#include "api/replay/renderdoc_replay.h"
#include "common/wrapped_pool.h"
#include "core/core.h"
#include "core/resource_manager.h"
#include "serialise/serialiser.h"
#include "d3d8_common.h"

enum D3D8ResourceType
{
  Resource_Unknown = 0,
  Resource_VertexBuffer,
  Resource_IndexBuffer,
};

struct D3D8ResourceRecord : public ResourceRecord
{
  enum
  {
    NullResource = NULL
  };

  D3D8ResourceRecord(ResourceId id) : ResourceRecord(id, true) {}
};

class D3D8ResourceManager : public ResourceManager<IUnknown *, IUnknown *, D3D8ResourceRecord>
{
public:
  D3D8ResourceManager(LogState state, Serialiser *ser, WrappedD3DDevice8 *dev)
      : ResourceManager(state, ser), m_Device(dev)
  {
  }

private:
  bool SerialisableResource(ResourceId id, D3D8ResourceRecord *record);
  ResourceId GetID(IUnknown *res);

  bool ResourceTypeRelease(IUnknown *res);

  bool Force_InitialState(IUnknown *res, bool prepare);
  bool Need_InitialStateChunk(IUnknown *res);
  bool Prepare_InitialState(IUnknown *res);
  bool Serialise_InitialState(ResourceId resid, IUnknown *res);
  void Create_InitialState(ResourceId id, IUnknown *live, bool hasData);
  void Apply_InitialState(IUnknown *live, InitialContentData data);

  WrappedD3DDevice8 *m_Device;
};
