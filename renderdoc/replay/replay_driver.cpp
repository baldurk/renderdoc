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

#include "replay_driver.h"
#include "maths/formatpacking.h"
#include "serialise/serialiser.h"

template <>
rdcstr DoStringise(const RemapTexture &el)
{
  BEGIN_ENUM_STRINGISE(RemapTexture);
  {
    STRINGISE_ENUM_CLASS(NoRemap)
    STRINGISE_ENUM_CLASS(RGBA8)
    STRINGISE_ENUM_CLASS(RGBA16)
    STRINGISE_ENUM_CLASS(RGBA32)
    STRINGISE_ENUM_CLASS(D32S8)
  }
  END_ENUM_STRINGISE();
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, GetTextureDataParams &el)
{
  SERIALISE_MEMBER(forDiskSave);
  SERIALISE_MEMBER(typeHint);
  SERIALISE_MEMBER(resolve);
  SERIALISE_MEMBER(remap);
  SERIALISE_MEMBER(blackPoint);
  SERIALISE_MEMBER(whitePoint);
}

INSTANTIATE_SERIALISE_TYPE(GetTextureDataParams);

static bool PreviousNextExcludedMarker(DrawcallDescription *draw)
{
  return bool(draw->flags & (DrawFlags::PushMarker | DrawFlags::SetMarker | DrawFlags::MultiDraw |
                             DrawFlags::APICalls));
}

static DrawcallDescription *SetupDrawcallPointers(std::vector<DrawcallDescription *> &drawcallTable,
                                                  rdcarray<DrawcallDescription> &draws,
                                                  DrawcallDescription *parent,
                                                  DrawcallDescription *&previous)
{
  DrawcallDescription *ret = NULL;

  for(size_t i = 0; i < draws.size(); i++)
  {
    DrawcallDescription *draw = &draws[i];

    draw->parent = parent;

    if(!draw->children.empty())
    {
      {
        RDCASSERT(drawcallTable.empty() || draw->eventId > drawcallTable.back()->eventId);
        drawcallTable.resize(RDCMAX(drawcallTable.size(), size_t(draw->eventId + 1)));
        drawcallTable[draw->eventId] = draw;
      }

      ret = SetupDrawcallPointers(drawcallTable, draw->children, draw, previous);
    }
    else if(PreviousNextExcludedMarker(draw))
    {
      // don't want to set up previous/next links for markers, but still add them to the table
      // Some markers like Present should have previous/next, but API Calls we also skip

      {
        // we also allow equal EIDs for fake markers that don't have their own EIDs
        RDCASSERT(drawcallTable.empty() || draw->eventId > drawcallTable.back()->eventId ||
                  (draw->eventId == drawcallTable.back()->eventId &&
                   (drawcallTable.back()->flags & DrawFlags::PushMarker)));
        drawcallTable.resize(RDCMAX(drawcallTable.size(), size_t(draw->eventId + 1)));
        drawcallTable[draw->eventId] = draw;
      }
    }
    else
    {
      if(previous)
        previous->next = draw;
      draw->previous = previous;

      {
        // we also allow equal EIDs for fake markers that don't have their own EIDs
        RDCASSERT(drawcallTable.empty() || draw->eventId > drawcallTable.back()->eventId ||
                  (draw->eventId == drawcallTable.back()->eventId &&
                   (drawcallTable.back()->flags & DrawFlags::PushMarker)));
        drawcallTable.resize(RDCMAX(drawcallTable.size(), size_t(draw->eventId + 1)));
        drawcallTable[draw->eventId] = draw;
      }

      ret = previous = draw;
    }
  }

  return ret;
}

void SetupDrawcallPointers(std::vector<DrawcallDescription *> &drawcallTable,
                           rdcarray<DrawcallDescription> &draws)
{
  DrawcallDescription *previous = NULL;
  SetupDrawcallPointers(drawcallTable, draws, NULL, previous);

  // markers don't enter the previous/next chain, but we still want pointers for them that point to
  // the next or previous actual draw (skipping any markers). This means that draw->next->previous
  // != draw sometimes, but it's more useful than draw->next being NULL in the middle of the list.
  // This enables searching for a marker string and then being able to navigate from there and
  // joining the 'real' linked list after one step.

  previous = NULL;
  std::vector<DrawcallDescription *> markers;

  for(DrawcallDescription *draw : drawcallTable)
  {
    if(!draw)
      continue;

    bool marker = PreviousNextExcludedMarker(draw);

    if(marker)
    {
      // point the previous pointer to the last non-marker draw we got. If we haven't hit one yet
      // because this is near the start, this will just be NULL.
      draw->previous = previous;

      // because there can be multiple markers consecutively we want to point all of their nexts to
      // the next draw we encounter. Accumulate this list, though in most cases it will only be 1
      // long as it's uncommon to have multiple markers one after the other
      markers.push_back(draw);
    }
    else
    {
      // the next markers we encounter should point their previous to this.
      previous = draw;

      // all previous markers point to this one
      for(DrawcallDescription *m : markers)
        m->next = draw;

      markers.clear();
    }
  }
}

void PatchLineStripIndexBuffer(const DrawcallDescription *draw, uint8_t *idx8, uint16_t *idx16,
                               uint32_t *idx32, std::vector<uint32_t> &patchedIndices)
{
  const uint32_t restart = 0xffffffff;

#define IDX_VALUE(offs)        \
  (idx16 ? idx16[index + offs] \
         : (idx32 ? idx32[index + offs] : (idx8 ? idx8[index + offs] : index + offs)))

  switch(draw->topology)
  {
    case Topology::TriangleList:
    {
      for(uint32_t index = 0; index + 3 <= draw->numIndices; index += 3)
      {
        patchedIndices.push_back(IDX_VALUE(0));
        patchedIndices.push_back(IDX_VALUE(1));
        patchedIndices.push_back(IDX_VALUE(2));
        patchedIndices.push_back(IDX_VALUE(0));
        patchedIndices.push_back(restart);
      }
      break;
    }
    case Topology::TriangleStrip:
    {
      // we decompose into individual triangles. This will mean the shared lines will be overwritten
      // twice but it's a simple algorithm and otherwise decomposing a tristrip into a line strip
      // would need some more complex handling (you could two pairs of triangles in a single strip
      // by changing the winding, but then you'd need to restart and jump back, and handle a
      // trailing single triangle, etc).
      for(uint32_t index = 0; index + 3 <= draw->numIndices; index++)
      {
        patchedIndices.push_back(IDX_VALUE(0));
        patchedIndices.push_back(IDX_VALUE(1));
        patchedIndices.push_back(IDX_VALUE(2));
        patchedIndices.push_back(IDX_VALUE(0));
        patchedIndices.push_back(restart);
      }
      break;
    }
    case Topology::TriangleFan:
    {
      uint32_t index = 0;
      uint32_t base = IDX_VALUE(0);
      index++;

      // this would be easier to do as a line list and just do base -> 1, 1 -> 2 lines for each
      // triangle then a base -> 2 for the last one. However I would be amazed if this code ever
      // runs except in an artificial test, so let's go with the simple and easy to understand
      // solution.
      for(; index + 2 <= draw->numIndices; index++)
      {
        patchedIndices.push_back(base);
        patchedIndices.push_back(IDX_VALUE(0));
        patchedIndices.push_back(IDX_VALUE(1));
        patchedIndices.push_back(base);
        patchedIndices.push_back(restart);
      }
      break;
    }
    case Topology::TriangleList_Adj:
    {
      // skip the adjacency values
      for(uint32_t index = 0; index + 6 <= draw->numIndices; index += 6)
      {
        patchedIndices.push_back(IDX_VALUE(0));
        patchedIndices.push_back(IDX_VALUE(2));
        patchedIndices.push_back(IDX_VALUE(4));
        patchedIndices.push_back(IDX_VALUE(0));
        patchedIndices.push_back(restart);
      }
      break;
    }
    case Topology::TriangleStrip_Adj:
    {
      // skip the adjacency values
      for(uint32_t index = 0; index + 6 <= draw->numIndices; index += 2)
      {
        patchedIndices.push_back(IDX_VALUE(0));
        patchedIndices.push_back(IDX_VALUE(2));
        patchedIndices.push_back(IDX_VALUE(4));
        patchedIndices.push_back(IDX_VALUE(0));
        patchedIndices.push_back(restart);
      }
      break;
    }
    default:
      RDCERR("Unsupported topology %s for line-list patching", ToStr(draw->topology).c_str());
      return;
  }

#undef IDX_VALUE
}

void StandardFillCBufferVariable(uint32_t dataOffset, const bytebuf &data, ShaderVariable &outvar,
                                 uint32_t matStride)
{
  const VarType type = outvar.type;
  const uint32_t rows = outvar.rows;
  const uint32_t cols = outvar.columns;

  size_t elemByteSize = 4;
  if(type == VarType::Double || type == VarType::ULong || type == VarType::SLong)
    elemByteSize = 8;
  else if(type == VarType::Half || type == VarType::UShort || type == VarType::SShort)
    elemByteSize = 2;
  else if(type == VarType::UByte || type == VarType::SByte)
    elemByteSize = 1;

  // primary is the 'major' direction
  // so a matrix is a secondaryDim number of primaryDim-sized vectors
  uint32_t primaryDim = cols;
  uint32_t secondaryDim = rows;
  if(rows > 1 && !outvar.rowMajor)
  {
    primaryDim = rows;
    secondaryDim = cols;
  }

  if(dataOffset < data.size())
  {
    const byte *srcData = data.data() + dataOffset;
    const size_t avail = data.size() - dataOffset;

    byte *dstData = elemByteSize == 8 ? (byte *)outvar.value.u64v : (byte *)outvar.value.uv;
    const size_t dstStride = elemByteSize == 8 ? 8 : 4;

    // each secondaryDim element (row or column) is stored in a primaryDim-vector.
    // We copy each vector member individually to account for smaller than uint32 sized types.
    for(uint32_t s = 0; s < secondaryDim; s++)
    {
      for(uint32_t p = 0; p < primaryDim; p++)
      {
        const size_t srcOffset = matStride * s + p * elemByteSize;
        const size_t dstOffset = (primaryDim * s + p) * dstStride;

        if(srcOffset + elemByteSize <= avail)
          memcpy(dstData + dstOffset, srcData + srcOffset, elemByteSize);
      }
    }

    // if it's a matrix and not row major, transpose
    if(primaryDim > 1 && secondaryDim > 1 && !outvar.rowMajor)
    {
      ShaderVariable tmp = outvar;

      if(elemByteSize == 8)
      {
        for(size_t ri = 0; ri < rows; ri++)
          for(size_t ci = 0; ci < cols; ci++)
            outvar.value.u64v[ri * cols + ci] = tmp.value.u64v[ci * rows + ri];
      }
      else
      {
        for(size_t ri = 0; ri < rows; ri++)
          for(size_t ci = 0; ci < cols; ci++)
            outvar.value.uv[ri * cols + ci] = tmp.value.uv[ci * rows + ri];
      }
    }

    // special case - decode halfs in-place, sign extend signed < 4 byte integers
    if(type == VarType::Half)
    {
      for(size_t ri = 0; ri < rows; ri++)
      {
        for(size_t ci = 0; ci < cols; ci++)
        {
          outvar.value.fv[ri * cols + ci] =
              ConvertFromHalf((uint16_t)outvar.value.uv[ri * cols + ci]);
        }
      }
    }
    else if(type == VarType::SShort || type == VarType::SByte)
    {
      const uint32_t testMask = (type == VarType::SShort ? 0x8000 : 0x80);
      const uint32_t extendMask = (type == VarType::SShort ? 0xffff0000 : 0xffffff00);

      for(size_t ri = 0; ri < rows; ri++)
      {
        for(size_t ci = 0; ci < cols; ci++)
        {
          uint32_t &u = outvar.value.uv[ri * cols + ci];

          if(u & testMask)
            u |= extendMask;
        }
      }
    }
  }
}

static void StandardFillCBufferVariables(const rdcarray<ShaderConstant> &invars,
                                         rdcarray<ShaderVariable> &outvars, const bytebuf &data,
                                         uint32_t baseOffset)
{
  for(size_t v = 0; v < invars.size(); v++)
  {
    std::string basename = invars[v].name;

    uint32_t rows = invars[v].type.descriptor.rows;
    uint32_t cols = invars[v].type.descriptor.columns;
    uint32_t elems = RDCMAX(1U, invars[v].type.descriptor.elements);
    const bool rowMajor = invars[v].type.descriptor.rowMajorStorage != 0;
    const bool isArray = elems > 1;

    const uint32_t matStride = invars[v].type.descriptor.matrixByteStride;

    uint32_t dataOffset = baseOffset + invars[v].byteOffset;

    if(!invars[v].type.members.empty() || (rows == 0 && cols == 0))
    {
      ShaderVariable var;
      var.name = basename;
      var.rows = var.columns = 0;
      var.type = VarType::Float;
      var.rowMajor = rowMajor;

      std::vector<ShaderVariable> varmembers;

      if(isArray)
      {
        var.members.resize(elems);
        for(uint32_t i = 0; i < elems; i++)
        {
          ShaderVariable &vr = var.members[i];
          vr.name = StringFormat::Fmt("%s[%u]", basename.c_str(), i);
          vr.rows = vr.columns = 0;
          vr.type = VarType::Float;
          vr.rowMajor = rowMajor;

          StandardFillCBufferVariables(invars[v].type.members, vr.members, data, dataOffset);

          dataOffset += invars[v].type.descriptor.arrayByteStride;

          vr.isStruct = true;
        }

        var.isStruct = false;
      }
      else
      {
        var.isStruct = true;

        StandardFillCBufferVariables(invars[v].type.members, var.members, data, dataOffset);
      }

      outvars.push_back(var);

      continue;
    }

    size_t outIdx = outvars.size();
    outvars.resize(outvars.size() + 1);

    {
      const VarType type = invars[v].type.descriptor.type;

      outvars[outIdx].name = basename;
      outvars[outIdx].rows = 1;
      outvars[outIdx].type = type;
      outvars[outIdx].isStruct = false;
      outvars[outIdx].columns = cols;
      outvars[outIdx].rowMajor = rowMajor;

      ShaderVariable &var = outvars[outIdx];

      if(!isArray)
      {
        outvars[outIdx].rows = rows;

        StandardFillCBufferVariable(dataOffset, data, outvars[outIdx], matStride);
      }
      else
      {
        var.name = outvars[outIdx].name;
        var.rows = 0;
        var.columns = 0;

        std::vector<ShaderVariable> varmembers;
        varmembers.resize(elems);

        std::string base = outvars[outIdx].name;

        for(uint32_t e = 0; e < elems; e++)
        {
          varmembers[e].name = StringFormat::Fmt("%s[%u]", base.c_str(), e);
          varmembers[e].rows = rows;
          varmembers[e].type = type;
          varmembers[e].isStruct = false;
          varmembers[e].columns = cols;
          varmembers[e].rowMajor = rowMajor;

          uint32_t rowDataOffset = dataOffset;

          dataOffset += invars[v].type.descriptor.arrayByteStride;

          StandardFillCBufferVariable(rowDataOffset, data, varmembers[e], matStride);
        }

        {
          var.isStruct = false;
          var.members = varmembers;
        }
      }
    }
  }
}

void StandardFillCBufferVariables(const rdcarray<ShaderConstant> &invars,
                                  rdcarray<ShaderVariable> &outvars, const bytebuf &data)
{
  // start with offset 0
  StandardFillCBufferVariables(invars, outvars, data, 0);
}

uint64_t CalcMeshOutputSize(uint64_t curSize, uint64_t requiredOutput)
{
  // resize exponentially up to 256MB to avoid repeated resizes
  while(curSize < requiredOutput && curSize < 0x10000000ULL)
    curSize *= 2;

  // after that, just align the required size up to 16MB and allocate that. Otherwise we can
  // vastly-overallocate at large sizes.
  if(curSize < requiredOutput)
    curSize = AlignUp(requiredOutput, (uint64_t)0x1000000ULL);

  return curSize;
}

FloatVector HighlightCache::InterpretVertex(const byte *data, uint32_t vert, const MeshDisplay &cfg,
                                            const byte *end, bool useidx, bool &valid)
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

    if(IsStrip(cfg.position.topology))
    {
      if((cfg.position.indexByteStride == 1 && vert == 0xff) ||
         (cfg.position.indexByteStride == 2 && vert == 0xffff) ||
         (cfg.position.indexByteStride == 4 && vert == 0xffffffff))
      {
        valid = false;
        return ret;
      }
    }
  }

  return HighlightCache::InterpretVertex(data, vert, cfg.position.vertexByteStride,
                                         cfg.position.format, end, valid);
}

FloatVector HighlightCache::InterpretVertex(const byte *data, uint32_t vert,
                                            uint32_t vertexByteStride, const ResourceFormat &fmt,
                                            const byte *end, bool &valid)
{
  FloatVector ret(0.0f, 0.0f, 0.0f, 1.0f);

  data += vert * vertexByteStride;

  float *out = &ret.x;

  if(fmt.type == ResourceFormatType::R10G10B10A2)
  {
    if(data + 4 >= end)
    {
      valid = false;
      return ret;
    }

    Vec4f v;
    if(fmt.compType == CompType::SNorm)
      v = ConvertFromR10G10B10A2SNorm(*(const uint32_t *)data);
    else
      v = ConvertFromR10G10B10A2(*(const uint32_t *)data);
    ret.x = v.x;
    ret.y = v.y;
    ret.z = v.z;
    ret.w = v.w;
    return ret;
  }
  else if(fmt.type == ResourceFormatType::R11G11B10)
  {
    if(data + 4 >= end)
    {
      valid = false;
      return ret;
    }

    Vec3f v = ConvertFromR11G11B10(*(const uint32_t *)data);
    ret.x = v.x;
    ret.y = v.y;
    ret.z = v.z;
    return ret;
  }

  if(data + fmt.compCount * fmt.compByteWidth > end)
  {
    valid = false;
    return ret;
  }

  for(uint32_t i = 0; i < fmt.compCount; i++)
  {
    *out = ConvertComponent(fmt, data);

    data += fmt.compByteWidth;
    out++;
  }

  if(fmt.BGRAOrder())
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

uint64_t inthash(uint64_t val, uint64_t seed)
{
  return (seed << 5) + seed + val; /* hash * 33 + c */
}

uint64_t inthash(ResourceId id, uint64_t seed)
{
  uint64_t val = 0;
  memcpy(&val, &id, sizeof(val));
  return (seed << 5) + seed + val; /* hash * 33 + c */
}

void HighlightCache::CacheHighlightingData(uint32_t eventId, const MeshDisplay &cfg)
{
  std::string ident;

  uint64_t newKey = 5381;

  // hash all the properties of cfg that we use
  newKey = inthash(eventId, newKey);
  newKey = inthash(cfg.position.indexByteStride, newKey);
  newKey = inthash(cfg.position.numIndices, newKey);
  newKey = inthash((uint64_t)cfg.type, newKey);
  newKey = inthash((uint64_t)cfg.position.baseVertex, newKey);
  newKey = inthash((uint64_t)cfg.position.topology, newKey);
  newKey = inthash(cfg.position.vertexByteOffset, newKey);
  newKey = inthash(cfg.position.vertexByteStride, newKey);
  newKey = inthash(cfg.position.indexResourceId, newKey);
  newKey = inthash(cfg.position.vertexResourceId, newKey);

  if(cacheKey != newKey)
  {
    cacheKey = newKey;

    uint32_t bytesize = cfg.position.indexByteStride;
    uint64_t maxIndex = cfg.position.numIndices - 1;

    if(cfg.position.indexByteStride == 0 || cfg.type == MeshDataStage::GSOut)
    {
      indices.clear();
      idxData = false;
    }
    else
    {
      idxData = true;

      bytebuf idxdata;
      if(cfg.position.indexResourceId != ResourceId())
        driver->GetBufferData(cfg.position.indexResourceId, cfg.position.indexByteOffset,
                              cfg.position.numIndices * bytesize, idxdata);

      uint8_t *idx8 = (uint8_t *)&idxdata[0];
      uint16_t *idx16 = (uint16_t *)&idxdata[0];
      uint32_t *idx32 = (uint32_t *)&idxdata[0];

      uint32_t numIndices = RDCMIN(cfg.position.numIndices, uint32_t(idxdata.size() / bytesize));

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

      uint32_t primRestart = 0;
      if(IsStrip(cfg.position.topology))
      {
        if(cfg.position.indexByteStride == 1)
          primRestart = 0xff;
        else if(cfg.position.indexByteStride == 2)
          primRestart = 0xffff;
        else
          primRestart = 0xffffffff;
      }

      for(uint32_t i = 0; cfg.position.baseVertex != 0 && i < numIndices; i++)
      {
        // don't modify primitive restart indices
        if(primRestart && indices[i] == primRestart)
          continue;

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

    driver->GetBufferData(cfg.position.vertexResourceId, cfg.position.vertexByteOffset,
                          (maxIndex + 1) * cfg.position.vertexByteStride, vertexData);
  }
}

bool HighlightCache::FetchHighlightPositions(const MeshDisplay &cfg, FloatVector &activeVertex,
                                             std::vector<FloatVector> &activePrim,
                                             std::vector<FloatVector> &adjacentPrimVertices,
                                             std::vector<FloatVector> &inactiveVertices)
{
  bool valid = true;

  byte *data = &vertexData[0];
  byte *dataEnd = data + vertexData.size();

  uint32_t idx = cfg.highlightVert;
  Topology meshtopo = cfg.position.topology;

  activeVertex = InterpretVertex(data, idx, cfg, dataEnd, true, valid);

  uint32_t primRestart = 0;
  if(IsStrip(meshtopo))
  {
    if(cfg.position.indexByteStride == 1)
      primRestart = 0xff;
    else if(cfg.position.indexByteStride == 2)
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

    uint32_t numidx = cfg.position.numIndices;

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
    uint32_t dim = PatchList_Count(cfg.position.topology);

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

// colour ramp from http://www.ncl.ucar.edu/Document/Graphics/ColorTables/GMT_wysiwyg.shtml
const Vec4f colorRamp[22] = {
    Vec4f(0.000000f, 0.000000f, 0.000000f, 0.0f), Vec4f(0.250980f, 0.000000f, 0.250980f, 1.0f),
    Vec4f(0.250980f, 0.000000f, 0.752941f, 1.0f), Vec4f(0.000000f, 0.250980f, 1.000000f, 1.0f),
    Vec4f(0.000000f, 0.501961f, 1.000000f, 1.0f), Vec4f(0.000000f, 0.627451f, 1.000000f, 1.0f),
    Vec4f(0.250980f, 0.752941f, 1.000000f, 1.0f), Vec4f(0.250980f, 0.878431f, 1.000000f, 1.0f),
    Vec4f(0.250980f, 1.000000f, 1.000000f, 1.0f), Vec4f(0.250980f, 1.000000f, 0.752941f, 1.0f),
    Vec4f(0.250980f, 1.000000f, 0.250980f, 1.0f), Vec4f(0.501961f, 1.000000f, 0.250980f, 1.0f),
    Vec4f(0.752941f, 1.000000f, 0.250980f, 1.0f), Vec4f(1.000000f, 1.000000f, 0.250980f, 1.0f),
    Vec4f(1.000000f, 0.878431f, 0.250980f, 1.0f), Vec4f(1.000000f, 0.627451f, 0.250980f, 1.0f),
    Vec4f(1.000000f, 0.376471f, 0.250980f, 1.0f), Vec4f(1.000000f, 0.125490f, 0.250980f, 1.0f),
    Vec4f(1.000000f, 0.376471f, 0.752941f, 1.0f), Vec4f(1.000000f, 0.627451f, 1.000000f, 1.0f),
    Vec4f(1.000000f, 0.878431f, 1.000000f, 1.0f), Vec4f(1.000000f, 1.000000f, 1.000000f, 1.0f),
};
