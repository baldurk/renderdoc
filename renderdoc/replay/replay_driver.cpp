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

#include "replay_driver.h"
#include "maths/formatpacking.h"

DrawcallDescription *SetupDrawcallPointers(vector<DrawcallDescription *> *drawcallTable,
                                           rdctype::array<DrawcallDescription> &draws,
                                           DrawcallDescription *parent,
                                           DrawcallDescription *&previous)
{
  DrawcallDescription *ret = NULL;

  for(size_t i = 0; i < draws.size(); i++)
  {
    DrawcallDescription *draw = &draws[i];

    draw->parent = parent ? parent->eventID : 0;

    if(draw->children.count > 0)
    {
      if(drawcallTable)
      {
        RDCASSERT(drawcallTable->empty() || draw->eventID > drawcallTable->back()->eventID);
        drawcallTable->resize(RDCMAX(drawcallTable->size(), size_t(draw->eventID + 1)));
        (*drawcallTable)[draw->eventID] = draw;
      }

      ret = SetupDrawcallPointers(drawcallTable, draw->children, draw, previous);
    }
    else if((draw->flags & (DrawFlags::PushMarker | DrawFlags::SetMarker | DrawFlags::MultiDraw)) &&
            !(draw->flags & DrawFlags::APICalls))
    {
      // don't want to set up previous/next links for markers, but still add them to the table
      // Some markers like Present or API Calls should have previous/next

      if(drawcallTable)
      {
        RDCASSERT(drawcallTable->empty() || draw->eventID > drawcallTable->back()->eventID);
        drawcallTable->resize(RDCMAX(drawcallTable->size(), size_t(draw->eventID + 1)));
        (*drawcallTable)[draw->eventID] = draw;
      }
    }
    else
    {
      if(previous != NULL)
        previous->next = draw->eventID;
      draw->previous = previous ? previous->eventID : 0;

      if(drawcallTable)
      {
        RDCASSERT(drawcallTable->empty() || draw->eventID > drawcallTable->back()->eventID);
        drawcallTable->resize(RDCMAX(drawcallTable->size(), size_t(draw->eventID + 1)));
        (*drawcallTable)[draw->eventID] = draw;
      }

      ret = previous = draw;
    }
  }

  return ret;
}

FloatVector HighlightCache::InterpretVertex(byte *data, uint32_t vert, const MeshDisplay &cfg,
                                            byte *end, bool useidx, bool &valid)
{
  FloatVector ret(0.0f, 0.0f, 0.0f, 1.0f);

  if(useidx && idxData)
  {
    if(vert >= (uint32_t)indices.size())
    {
      valid = false;
      return ret;
    }

    vert = indices[vert];
  }

  return HighlightCache::InterpretVertex(data, vert, cfg, end, valid);
}

FloatVector HighlightCache::InterpretVertex(byte *data, uint32_t vert, const MeshDisplay &cfg,
                                            byte *end, bool &valid)
{
  FloatVector ret(0.0f, 0.0f, 0.0f, 1.0f);

  data += vert * cfg.position.stride;

  float *out = &ret.x;

  ResourceFormat fmt;
  fmt.compByteWidth = cfg.position.compByteWidth;
  fmt.compCount = cfg.position.compCount;
  fmt.compType = cfg.position.compType;

  if(cfg.position.specialFormat == SpecialFormat::R10G10B10A2)
  {
    if(data + 4 >= end)
    {
      valid = false;
      return ret;
    }

    Vec4f v = ConvertFromR10G10B10A2(*(uint32_t *)data);
    ret.x = v.x;
    ret.y = v.y;
    ret.z = v.z;
    ret.w = v.w;
    return ret;
  }
  else if(cfg.position.specialFormat == SpecialFormat::R11G11B10)
  {
    if(data + 4 >= end)
    {
      valid = false;
      return ret;
    }

    Vec3f v = ConvertFromR11G11B10(*(uint32_t *)data);
    ret.x = v.x;
    ret.y = v.y;
    ret.z = v.z;
    return ret;
  }

  if(data + cfg.position.compCount * cfg.position.compByteWidth > end)
  {
    valid = false;
    return ret;
  }

  for(uint32_t i = 0; i < cfg.position.compCount; i++)
  {
    *out = ConvertComponent(fmt, data);

    data += cfg.position.compByteWidth;
    out++;
  }

  if(cfg.position.bgraOrder)
  {
    FloatVector reversed;
    reversed.x = ret.z;
    reversed.y = ret.y;
    reversed.z = ret.x;
    reversed.w = ret.w;
    return reversed;
  }

  return ret;
}

void HighlightCache::CacheHighlightingData(uint32_t eventID, const MeshDisplay &cfg)
{
  if(EID != eventID || cfg.type != stage || cfg.position.buf != buf || cfg.position.offset != offs)
  {
    EID = eventID;
    buf = cfg.position.buf;
    offs = cfg.position.offset;
    stage = cfg.type;

    uint32_t bytesize = cfg.position.idxByteWidth;
    uint64_t maxIndex = cfg.position.numVerts;

    if(cfg.position.idxByteWidth == 0 || stage == MeshDataStage::GSOut)
    {
      indices.clear();
      idxData = false;
    }
    else
    {
      idxData = true;

      vector<byte> idxdata;
      if(cfg.position.idxbuf != ResourceId())
        driver->GetBufferData(cfg.position.idxbuf, cfg.position.idxoffs,
                              cfg.position.numVerts * bytesize, idxdata);

      uint8_t *idx8 = (uint8_t *)&idxdata[0];
      uint16_t *idx16 = (uint16_t *)&idxdata[0];
      uint32_t *idx32 = (uint32_t *)&idxdata[0];

      uint32_t numIndices = RDCMIN(cfg.position.numVerts, uint32_t(idxdata.size() / bytesize));

      indices.resize(numIndices);

      if(bytesize == 1)
      {
        for(uint32_t i = 0; i < numIndices; i++)
        {
          indices[i] = uint32_t(idx8[i]);
          maxIndex = RDCMAX(maxIndex, (uint64_t)indices[i]);
        }
      }
      else if(bytesize == 2)
      {
        for(uint32_t i = 0; i < numIndices; i++)
        {
          indices[i] = uint32_t(idx16[i]);
          maxIndex = RDCMAX(maxIndex, (uint64_t)indices[i]);
        }
      }
      else if(bytesize == 4)
      {
        for(uint32_t i = 0; i < numIndices; i++)
        {
          indices[i] = idx32[i];
          maxIndex = RDCMAX(maxIndex, (uint64_t)indices[i]);
        }
      }

      uint32_t sub = uint32_t(-cfg.position.baseVertex);
      uint32_t add = uint32_t(cfg.position.baseVertex);

      if(cfg.position.baseVertex > 0)
        maxIndex += add;

      for(uint32_t i = 0; cfg.position.baseVertex != 0 && i < numIndices; i++)
      {
        if(cfg.position.baseVertex < 0)
        {
          if(indices[i] < sub)
            indices[i] = 0;
          else
            indices[i] -= sub;
        }
        else
        {
          indices[i] += add;
        }
      }
    }

    driver->GetBufferData(cfg.position.buf, cfg.position.offset,
                          (maxIndex + 1) * cfg.position.stride, vertexData);
  }
}

bool HighlightCache::FetchHighlightPositions(const MeshDisplay &cfg, FloatVector &activeVertex,
                                             vector<FloatVector> &activePrim,
                                             vector<FloatVector> &adjacentPrimVertices,
                                             vector<FloatVector> &inactiveVertices)
{
  bool valid = true;

  byte *data = &vertexData[0];
  byte *dataEnd = data + vertexData.size();

  uint32_t idx = cfg.highlightVert;
  Topology meshtopo = cfg.position.topo;

  activeVertex = InterpretVertex(data, idx, cfg, dataEnd, true, valid);

  uint32_t primRestart = 0;
  if(IsStrip(meshtopo))
  {
    if(cfg.position.idxByteWidth == 1)
      primRestart = 0xff;
    else if(cfg.position.idxByteWidth == 2)
      primRestart = 0xffff;
    else
      primRestart = 0xffffffff;
  }

  // Reference for how primitive topologies are laid out:
  // http://msdn.microsoft.com/en-us/library/windows/desktop/bb205124(v=vs.85).aspx
  // Section 19.1 of the Vulkan 1.0.48 spec
  // Section 10.1 of the OpenGL 4.5 spec
  if(meshtopo == Topology::LineList)
  {
    uint32_t v = uint32_t(idx / 2) * 2;    // find first vert in primitive

    activePrim.push_back(InterpretVertex(data, v + 0, cfg, dataEnd, true, valid));
    activePrim.push_back(InterpretVertex(data, v + 1, cfg, dataEnd, true, valid));
  }
  else if(meshtopo == Topology::TriangleList)
  {
    uint32_t v = uint32_t(idx / 3) * 3;    // find first vert in primitive

    activePrim.push_back(InterpretVertex(data, v + 0, cfg, dataEnd, true, valid));
    activePrim.push_back(InterpretVertex(data, v + 1, cfg, dataEnd, true, valid));
    activePrim.push_back(InterpretVertex(data, v + 2, cfg, dataEnd, true, valid));
  }
  else if(meshtopo == Topology::LineList_Adj)
  {
    uint32_t v = uint32_t(idx / 4) * 4;    // find first vert in primitive

    FloatVector vs[] = {
        InterpretVertex(data, v + 0, cfg, dataEnd, true, valid),
        InterpretVertex(data, v + 1, cfg, dataEnd, true, valid),
        InterpretVertex(data, v + 2, cfg, dataEnd, true, valid),
        InterpretVertex(data, v + 3, cfg, dataEnd, true, valid),
    };

    adjacentPrimVertices.push_back(vs[0]);
    adjacentPrimVertices.push_back(vs[1]);

    adjacentPrimVertices.push_back(vs[2]);
    adjacentPrimVertices.push_back(vs[3]);

    activePrim.push_back(vs[1]);
    activePrim.push_back(vs[2]);
  }
  else if(meshtopo == Topology::TriangleList_Adj)
  {
    uint32_t v = uint32_t(idx / 6) * 6;    // find first vert in primitive

    FloatVector vs[] = {
        InterpretVertex(data, v + 0, cfg, dataEnd, true, valid),
        InterpretVertex(data, v + 1, cfg, dataEnd, true, valid),
        InterpretVertex(data, v + 2, cfg, dataEnd, true, valid),
        InterpretVertex(data, v + 3, cfg, dataEnd, true, valid),
        InterpretVertex(data, v + 4, cfg, dataEnd, true, valid),
        InterpretVertex(data, v + 5, cfg, dataEnd, true, valid),
    };

    adjacentPrimVertices.push_back(vs[0]);
    adjacentPrimVertices.push_back(vs[1]);
    adjacentPrimVertices.push_back(vs[2]);

    adjacentPrimVertices.push_back(vs[2]);
    adjacentPrimVertices.push_back(vs[3]);
    adjacentPrimVertices.push_back(vs[4]);

    adjacentPrimVertices.push_back(vs[4]);
    adjacentPrimVertices.push_back(vs[5]);
    adjacentPrimVertices.push_back(vs[0]);

    activePrim.push_back(vs[0]);
    activePrim.push_back(vs[2]);
    activePrim.push_back(vs[4]);
  }
  else if(meshtopo == Topology::LineStrip)
  {
    // find first vert in primitive. In strips a vert isn't
    // in only one primitive, so we pick the first primitive
    // it's in. This means the first N points are in the first
    // primitive, and thereafter each point is in the next primitive
    uint32_t v = RDCMAX(idx, 1U) - 1;

    // skip past any primitive restart indices
    if(idxData && primRestart)
    {
      while(v < (uint32_t)indices.size() && indices[v] == primRestart)
        v++;
    }

    activePrim.push_back(InterpretVertex(data, v + 0, cfg, dataEnd, true, valid));
    activePrim.push_back(InterpretVertex(data, v + 1, cfg, dataEnd, true, valid));
  }
  else if(meshtopo == Topology::TriangleStrip)
  {
    // find first vert in primitive. In strips a vert isn't
    // in only one primitive, so we pick the first primitive
    // it's in. This means the first N points are in the first
    // primitive, and thereafter each point is in the next primitive
    uint32_t v = RDCMAX(idx, 2U) - 2;

    // skip past any primitive restart indices
    if(idxData && primRestart)
    {
      while(v < (uint32_t)indices.size() &&
            (indices[v + 0] == primRestart || indices[v + 1] == primRestart))
        v++;
    }

    activePrim.push_back(InterpretVertex(data, v + 0, cfg, dataEnd, true, valid));
    activePrim.push_back(InterpretVertex(data, v + 1, cfg, dataEnd, true, valid));
    activePrim.push_back(InterpretVertex(data, v + 2, cfg, dataEnd, true, valid));
  }
  else if(meshtopo == Topology::LineStrip_Adj)
  {
    // find first vert in primitive. In strips a vert isn't
    // in only one primitive, so we pick the first primitive
    // it's in. This means the first N points are in the first
    // primitive, and thereafter each point is in the next primitive
    uint32_t v = RDCMAX(idx, 3U) - 3;

    // skip past any primitive restart indices
    if(idxData && primRestart)
    {
      while(v < (uint32_t)indices.size() &&
            (indices[v + 0] == primRestart || indices[v + 1] == primRestart ||
             indices[v + 2] == primRestart))
        v++;
    }

    FloatVector vs[] = {
        InterpretVertex(data, v + 0, cfg, dataEnd, true, valid),
        InterpretVertex(data, v + 1, cfg, dataEnd, true, valid),
        InterpretVertex(data, v + 2, cfg, dataEnd, true, valid),
        InterpretVertex(data, v + 3, cfg, dataEnd, true, valid),
    };

    adjacentPrimVertices.push_back(vs[0]);
    adjacentPrimVertices.push_back(vs[1]);

    adjacentPrimVertices.push_back(vs[2]);
    adjacentPrimVertices.push_back(vs[3]);

    activePrim.push_back(vs[1]);
    activePrim.push_back(vs[2]);
  }
  else if(meshtopo == Topology::TriangleStrip_Adj)
  {
    // Triangle strip with adjacency is the most complex topology, as
    // we need to handle the ends separately where the pattern breaks.

    uint32_t numidx = cfg.position.numVerts;

    if(numidx < 6)
    {
      // not enough indices provided, bail to make sure logic below doesn't
      // need to have tons of edge case detection
      valid = false;
    }
    else if(idx <= 4 || numidx <= 7)
    {
      FloatVector vs[] = {
          InterpretVertex(data, 0, cfg, dataEnd, true, valid),
          InterpretVertex(data, 1, cfg, dataEnd, true, valid),
          InterpretVertex(data, 2, cfg, dataEnd, true, valid),
          InterpretVertex(data, 3, cfg, dataEnd, true, valid),
          InterpretVertex(data, 4, cfg, dataEnd, true, valid),

          // note this one isn't used as it's adjacency for the next triangle
          InterpretVertex(data, 5, cfg, dataEnd, true, valid),

          // min() with number of indices in case this is a tiny strip
          // that is basically just a list
          InterpretVertex(data, RDCMIN(6U, numidx - 1), cfg, dataEnd, true, valid),
      };

      // these are the triangles on the far left of the MSDN diagram above
      adjacentPrimVertices.push_back(vs[0]);
      adjacentPrimVertices.push_back(vs[1]);
      adjacentPrimVertices.push_back(vs[2]);

      adjacentPrimVertices.push_back(vs[4]);
      adjacentPrimVertices.push_back(vs[3]);
      adjacentPrimVertices.push_back(vs[0]);

      adjacentPrimVertices.push_back(vs[4]);
      adjacentPrimVertices.push_back(vs[2]);
      adjacentPrimVertices.push_back(vs[6]);

      activePrim.push_back(vs[0]);
      activePrim.push_back(vs[2]);
      activePrim.push_back(vs[4]);
    }
    else if(idx > numidx - 4)
    {
      // in diagram, numidx == 14

      FloatVector vs[] = {
          /*[0]=*/InterpretVertex(data, numidx - 8, cfg, dataEnd, true, valid),    // 6 in diagram

          // as above, unused since this is adjacency for 2-previous triangle
          /*[1]=*/InterpretVertex(data, numidx - 7, cfg, dataEnd, true, valid),    // 7 in diagram
          /*[2]=*/InterpretVertex(data, numidx - 6, cfg, dataEnd, true, valid),    // 8 in diagram

          // as above, unused since this is adjacency for previous triangle
          /*[3]=*/InterpretVertex(data, numidx - 5, cfg, dataEnd, true, valid),    // 9 in diagram
          /*[4]=*/InterpretVertex(data, numidx - 4, cfg, dataEnd, true,
                                  valid),    // 10 in diagram
          /*[5]=*/InterpretVertex(data, numidx - 3, cfg, dataEnd, true,
                                  valid),    // 11 in diagram
          /*[6]=*/InterpretVertex(data, numidx - 2, cfg, dataEnd, true,
                                  valid),    // 12 in diagram
          /*[7]=*/InterpretVertex(data, numidx - 1, cfg, dataEnd, true,
                                  valid),    // 13 in diagram
      };

      // these are the triangles on the far right of the MSDN diagram above
      adjacentPrimVertices.push_back(vs[2]);    // 8 in diagram
      adjacentPrimVertices.push_back(vs[0]);    // 6 in diagram
      adjacentPrimVertices.push_back(vs[4]);    // 10 in diagram

      adjacentPrimVertices.push_back(vs[4]);    // 10 in diagram
      adjacentPrimVertices.push_back(vs[7]);    // 13 in diagram
      adjacentPrimVertices.push_back(vs[6]);    // 12 in diagram

      adjacentPrimVertices.push_back(vs[6]);    // 12 in diagram
      adjacentPrimVertices.push_back(vs[5]);    // 11 in diagram
      adjacentPrimVertices.push_back(vs[2]);    // 8 in diagram

      activePrim.push_back(vs[2]);    // 8 in diagram
      activePrim.push_back(vs[4]);    // 10 in diagram
      activePrim.push_back(vs[6]);    // 12 in diagram
    }
    else
    {
      // we're in the middle somewhere. Each primitive has two vertices for it
      // so our step rate is 2. The first 'middle' primitive starts at indices 5&6
      // and uses indices all the way back to 0
      uint32_t v = RDCMAX(((idx + 1) / 2) * 2, 6U) - 6;

      // skip past any primitive restart indices
      if(idxData && primRestart)
      {
        while(v < (uint32_t)indices.size() &&
              (indices[v + 0] == primRestart || indices[v + 1] == primRestart ||
               indices[v + 2] == primRestart || indices[v + 3] == primRestart ||
               indices[v + 4] == primRestart || indices[v + 5] == primRestart))
          v++;
      }

      // these correspond to the indices in the MSDN diagram, with {2,4,6} as the
      // main triangle
      FloatVector vs[] = {
          InterpretVertex(data, v + 0, cfg, dataEnd, true, valid),

          // this one is adjacency for 2-previous triangle
          InterpretVertex(data, v + 1, cfg, dataEnd, true, valid),
          InterpretVertex(data, v + 2, cfg, dataEnd, true, valid),

          // this one is adjacency for previous triangle
          InterpretVertex(data, v + 3, cfg, dataEnd, true, valid),
          InterpretVertex(data, v + 4, cfg, dataEnd, true, valid),
          InterpretVertex(data, v + 5, cfg, dataEnd, true, valid),
          InterpretVertex(data, v + 6, cfg, dataEnd, true, valid),
          InterpretVertex(data, v + 7, cfg, dataEnd, true, valid),
          InterpretVertex(data, v + 8, cfg, dataEnd, true, valid),
      };

      // these are the triangles around {2,4,6} in the MSDN diagram above
      adjacentPrimVertices.push_back(vs[0]);
      adjacentPrimVertices.push_back(vs[2]);
      adjacentPrimVertices.push_back(vs[4]);

      adjacentPrimVertices.push_back(vs[2]);
      adjacentPrimVertices.push_back(vs[5]);
      adjacentPrimVertices.push_back(vs[6]);

      adjacentPrimVertices.push_back(vs[6]);
      adjacentPrimVertices.push_back(vs[8]);
      adjacentPrimVertices.push_back(vs[4]);

      activePrim.push_back(vs[2]);
      activePrim.push_back(vs[4]);
      activePrim.push_back(vs[6]);
    }
  }
  else if(meshtopo >= Topology::PatchList)
  {
    uint32_t dim = PatchList_Count(cfg.position.topo);

    uint32_t v0 = uint32_t(idx / dim) * dim;

    for(uint32_t v = v0; v < v0 + dim; v++)
    {
      if(v != idx && valid)
        inactiveVertices.push_back(InterpretVertex(data, v, cfg, dataEnd, true, valid));
    }
  }
  else    // if(meshtopo == Topology::PointList) point list, or unknown/unhandled type
  {
    // no adjacency, inactive verts or active primitive
  }

  return valid;
}
