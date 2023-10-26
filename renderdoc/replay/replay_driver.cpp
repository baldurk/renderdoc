/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2023 Baldur Karlsson
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
#include <float.h>
#include <math.h>
#include "compressonator/CMP_Core.h"
#include "maths/formatpacking.h"
#include "maths/half_convert.h"
#include "serialise/serialiser.h"
#include "strings/string_utils.h"

template <>
rdcstr DoStringise(const RemapTexture &el)
{
  BEGIN_ENUM_STRINGISE(RemapTexture);
  {
    STRINGISE_ENUM_CLASS(NoRemap)
    STRINGISE_ENUM_CLASS(RGBA8)
    STRINGISE_ENUM_CLASS(RGBA16)
    STRINGISE_ENUM_CLASS(RGBA32)
  }
  END_ENUM_STRINGISE();
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, GetTextureDataParams &el)
{
  SERIALISE_MEMBER(forDiskSave);
  SERIALISE_MEMBER(standardLayout);
  SERIALISE_MEMBER(typeCast);
  SERIALISE_MEMBER(resolve);
  SERIALISE_MEMBER(remap);
  SERIALISE_MEMBER(blackPoint);
  SERIALISE_MEMBER(whitePoint);
}

INSTANTIATE_SERIALISE_TYPE(GetTextureDataParams);

static bool PreviousNextExcludedMarker(ActionDescription *action)
{
  return bool(action->flags & (ActionFlags::PushMarker | ActionFlags::PopMarker |
                               ActionFlags::SetMarker | ActionFlags::MultiAction));
}

CompType BaseRemapType(RemapTexture remap, CompType typeCast)
{
  switch(typeCast)
  {
    case CompType::Float:
    case CompType::SNorm:
    case CompType::UNorm: return CompType::Float;
    case CompType::UNormSRGB:
      return remap == RemapTexture::RGBA8 ? CompType::UNormSRGB : CompType::Float;
    case CompType::UInt: return CompType::UInt;
    case CompType::SInt: return CompType::SInt;
    default: return typeCast;
  }
}

static ActionDescription *SetupActionPointers(rdcarray<ActionDescription *> &actionTable,
                                              rdcarray<ActionDescription> &actions,
                                              ActionDescription *parent, ActionDescription *&previous)
{
  ActionDescription *ret = NULL;

  for(size_t i = 0; i < actions.size(); i++)
  {
    ActionDescription *action = &actions[i];

    RDCASSERTMSG("All actions must have their own event as the final event",
                 !action->events.empty() && action->events.back().eventId == action->eventId);

    action->parent = parent;

    if(!action->children.empty())
    {
      {
        RDCASSERT(actionTable.empty() || action->eventId > actionTable.back()->eventId ||
                  actionTable.back()->IsFakeMarker());
        actionTable.resize(RDCMAX(actionTable.size(), size_t(action->eventId) + 1));
        actionTable[action->eventId] = action;
      }

      ret = SetupActionPointers(actionTable, action->children, action, previous);
    }
    else if(PreviousNextExcludedMarker(action))
    {
      // don't want to set up previous/next links for markers, but still add them to the table
      // Some markers like Present should have previous/next, but API Calls we also skip

      {
        // we also allow non-contiguous EIDs for fake markers that have high EIDs
        RDCASSERT(actionTable.empty() || action->eventId > actionTable.back()->eventId ||
                  actionTable.back()->IsFakeMarker());
        actionTable.resize(RDCMAX(actionTable.size(), size_t(action->eventId) + 1));
        actionTable[action->eventId] = action;
      }
    }
    else
    {
      if(previous)
        previous->next = action;
      action->previous = previous;

      {
        // we also allow non-contiguous EIDs for fake markers that have high EIDs
        RDCASSERT(actionTable.empty() || action->eventId > actionTable.back()->eventId ||
                  actionTable.back()->IsFakeMarker());
        actionTable.resize(RDCMAX(actionTable.size(), size_t(action->eventId) + 1));
        actionTable[action->eventId] = action;
      }

      ret = previous = action;
    }
  }

  return ret;
}

void SetupActionPointers(rdcarray<ActionDescription *> &actionTable,
                         rdcarray<ActionDescription> &actions)
{
  ActionDescription *previous = NULL;
  SetupActionPointers(actionTable, actions, NULL, previous);

  // markers don't enter the previous/next chain, but we still want pointers for them that point to
  // the next or previous actual action (skipping any markers). This means that
  // action->next->previous
  // != action sometimes, but it's more useful than action->next being NULL in the middle of the
  // list.
  // This enables searching for a marker string and then being able to navigate from there and
  // joining the 'real' linked list after one step.

  previous = NULL;
  rdcarray<ActionDescription *> markers;

  for(ActionDescription *action : actionTable)
  {
    if(!action)
      continue;

    bool marker = PreviousNextExcludedMarker(action);

    if(marker)
    {
      // point the previous pointer to the last non-marker action we got. If we haven't hit one yet
      // because this is near the start, this will just be NULL.
      action->previous = previous;

      // because there can be multiple markers consecutively we want to point all of their nexts to
      // the next action we encounter. Accumulate this list, though in most cases it will only be 1
      // long as it's uncommon to have multiple markers one after the other
      markers.push_back(action);
    }
    else
    {
      // the next markers we encounter should point their previous to this.
      previous = action;

      // all previous markers point to this one
      for(ActionDescription *m : markers)
        m->next = action;

      markers.clear();
    }
  }
}

void PatchLineStripIndexBuffer(const ActionDescription *action, Topology topology, uint8_t *idx8,
                               uint16_t *idx16, uint32_t *idx32, rdcarray<uint32_t> &patchedIndices)
{
  const uint32_t restart = 0xffffffff;

#define IDX_VALUE(offs)                                                                        \
  (idx16 ? idx16[index + (offs)]                                                               \
         : (idx32 ? idx32[index + (offs)] : (idx8 ? idx8[index + (offs)] : index + (offs)))) + \
      action->baseVertex

  switch(topology)
  {
    case Topology::TriangleList:
    {
      for(uint32_t index = 0; index + 3 <= action->numIndices; index += 3)
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
      for(uint32_t index = 0; index + 3 <= action->numIndices; index++)
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
      for(; index + 2 <= action->numIndices; index++)
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
      for(uint32_t index = 0; index + 6 <= action->numIndices; index += 6)
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
      for(uint32_t index = 0; index + 6 <= action->numIndices; index += 2)
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
      RDCERR("Unsupported topology %s for line-list patching", ToStr(topology).c_str());
      return;
  }

#undef IDX_VALUE
}

void PatchTriangleFanRestartIndexBufer(rdcarray<uint32_t> &patchedIndices, uint32_t restartIndex)
{
  if(patchedIndices.empty())
    return;

  rdcarray<uint32_t> newIndices;

  uint32_t firstIndex = patchedIndices[0];

  size_t i = 1;
  // while we have at least two indices left
  while(i + 1 < patchedIndices.size())
  {
    uint32_t a = patchedIndices[i];
    uint32_t b = patchedIndices[i + 1];

    if(a != restartIndex && b != restartIndex)
    {
      // no restart, add primitive
      newIndices.push_back(firstIndex);
      newIndices.push_back(a);
      newIndices.push_back(b);

      i++;
      continue;
    }
    else if(b == restartIndex)
    {
      // we've already added the last triangle before the restart in the previous iteration, just
      // continue so we hit the a == restartIndex case below
      i++;
    }
    else if(a == restartIndex)
    {
      // new first index is b
      firstIndex = b;
      // skip both the restartIndex value and the first index, and begin at the next real index (if
      // it exists)
      i += 2;

      uint32_t next[2] = {b, b};

      // if this is the last vertex, the triangle will be degenerate
      if(i < patchedIndices.size())
        next[0] = patchedIndices[i];
      if(i + 1 < patchedIndices.size())
        next[1] = patchedIndices[i + 1];

      // output 3 dummy degenerate triangles so vertex ID mapping is easy
      // we rotate the triangles so the important vertex is last in each.
      for(size_t dummy = 0; dummy < 3; dummy++)
      {
        newIndices.push_back(restartIndex);
        newIndices.push_back(restartIndex);
        newIndices.push_back(restartIndex);
      }
    }
  }

  newIndices.swap(patchedIndices);
}

void StandardFillCBufferVariable(ResourceId shader, const ShaderConstantType &desc,
                                 uint32_t dataOffset, const bytebuf &data, ShaderVariable &outvar,
                                 uint32_t matStride)
{
  const VarType type = outvar.type;
  const uint32_t rows = outvar.rows;
  const uint32_t cols = outvar.columns;

  size_t elemByteSize = 4;
  if(type == VarType::Double || type == VarType::ULong || type == VarType::SLong ||
     type == VarType::GPUPointer)
    elemByteSize = 8;
  else if(type == VarType::Half || type == VarType::UShort || type == VarType::SShort)
    elemByteSize = 2;
  else if(type == VarType::UByte || type == VarType::SByte)
    elemByteSize = 1;

  // primary is the 'major' direction
  // so a matrix is a secondaryDim number of primaryDim-sized vectors
  uint32_t primaryDim = cols;
  uint32_t secondaryDim = rows;
  if(rows > 1 && outvar.ColMajor())
  {
    primaryDim = rows;
    secondaryDim = cols;
  }

  if(dataOffset < data.size())
  {
    const byte *srcData = data.data() + dataOffset;
    const size_t avail = data.size() - dataOffset;

    byte *dstData = outvar.value.u8v.data();

    // each secondaryDim element (row or column) is stored in a primaryDim-vector.
    // We copy each vector member individually to account for smaller than uint32 sized types.
    for(uint32_t s = 0; s < secondaryDim; s++)
    {
      for(uint32_t p = 0; p < primaryDim; p++)
      {
        const size_t srcOffset = matStride * s + p * elemByteSize;
        const size_t dstOffset = (primaryDim * s + p) * elemByteSize;

        if(srcOffset + elemByteSize <= avail)
          memcpy(dstData + dstOffset, srcData + srcOffset, elemByteSize);
      }
    }

    // if it's a matrix and not row major, transpose
    if(primaryDim > 1 && secondaryDim > 1 && outvar.ColMajor())
    {
      ShaderVariable tmp = outvar;

      if(elemByteSize == 8)
      {
        for(size_t ri = 0; ri < rows; ri++)
          for(size_t ci = 0; ci < cols; ci++)
            outvar.value.u64v[ri * cols + ci] = tmp.value.u64v[ci * rows + ri];
      }
      else if(elemByteSize == 4)
      {
        for(size_t ri = 0; ri < rows; ri++)
          for(size_t ci = 0; ci < cols; ci++)
            outvar.value.u32v[ri * cols + ci] = tmp.value.u32v[ci * rows + ri];
      }
      else if(elemByteSize == 2)
      {
        for(size_t ri = 0; ri < rows; ri++)
          for(size_t ci = 0; ci < cols; ci++)
            outvar.value.u16v[ri * cols + ci] = tmp.value.u16v[ci * rows + ri];
      }
      else if(elemByteSize == 1)
      {
        for(size_t ri = 0; ri < rows; ri++)
          for(size_t ci = 0; ci < cols; ci++)
            outvar.value.u8v[ri * cols + ci] = tmp.value.u8v[ci * rows + ri];
      }
    }
  }

  if(desc.pointerTypeID != ~0U)
    outvar.SetTypedPointer(outvar.value.u64v[0], shader, desc.pointerTypeID);
}

static void StandardFillCBufferVariables(ResourceId shader, const rdcarray<ShaderConstant> &invars,
                                         rdcarray<ShaderVariable> &outvars, const bytebuf &data,
                                         uint32_t baseOffset)
{
  for(size_t v = 0; v < invars.size(); v++)
  {
    rdcstr basename = invars[v].name;

    uint8_t rows = invars[v].type.rows;
    uint8_t cols = invars[v].type.columns;
    uint32_t elems = RDCMAX(1U, invars[v].type.elements);
    const bool rowMajor = invars[v].type.RowMajor();
    const bool isArray = elems > 1;

    const uint32_t matStride = invars[v].type.matrixByteStride;

    uint32_t dataOffset = baseOffset + invars[v].byteOffset;

    if(!invars[v].type.members.empty() || (rows == 0 && cols == 0))
    {
      ShaderVariable var;
      var.name = basename;
      var.rows = var.columns = 0;
      var.type = VarType::Struct;
      if(rowMajor)
        var.flags |= ShaderVariableFlags::RowMajorMatrix;

      rdcarray<ShaderVariable> varmembers;

      if(isArray)
      {
        var.members.resize(elems);
        for(uint32_t i = 0; i < elems; i++)
        {
          ShaderVariable &vr = var.members[i];
          vr.name = StringFormat::Fmt("%s[%u]", basename.c_str(), i);
          vr.rows = vr.columns = 0;
          vr.type = VarType::Struct;
          if(rowMajor)
            vr.flags |= ShaderVariableFlags::RowMajorMatrix;

          StandardFillCBufferVariables(shader, invars[v].type.members, vr.members, data, dataOffset);

          dataOffset += invars[v].type.arrayByteStride;
        }
      }
      else
      {
        var.type = VarType::Struct;

        StandardFillCBufferVariables(shader, invars[v].type.members, var.members, data, dataOffset);
      }

      outvars.push_back(var);

      continue;
    }

    size_t outIdx = outvars.size();
    outvars.push_back({});

    {
      const VarType type = invars[v].type.baseType;

      outvars[outIdx].name = basename;
      outvars[outIdx].rows = 1;
      outvars[outIdx].type = type;
      outvars[outIdx].columns = cols;
      if(rowMajor)
        outvars[outIdx].flags |= ShaderVariableFlags::RowMajorMatrix;

      ShaderVariable &var = outvars[outIdx];

      if(!isArray)
      {
        outvars[outIdx].rows = rows;

        StandardFillCBufferVariable(shader, invars[v].type, dataOffset, data, outvars[outIdx],
                                    matStride);
      }
      else
      {
        var.name = outvars[outIdx].name;
        var.rows = 0;
        var.columns = 0;

        rdcarray<ShaderVariable> varmembers;
        varmembers.resize(elems);

        rdcstr base = outvars[outIdx].name;

        for(uint32_t e = 0; e < elems; e++)
        {
          varmembers[e].name = StringFormat::Fmt("%s[%u]", base.c_str(), e);
          varmembers[e].rows = rows;
          varmembers[e].type = type;
          varmembers[e].columns = cols;
          if(rowMajor)
            varmembers[e].flags |= ShaderVariableFlags::RowMajorMatrix;

          uint32_t rowDataOffset = dataOffset;

          dataOffset += invars[v].type.arrayByteStride;

          StandardFillCBufferVariable(shader, invars[v].type, rowDataOffset, data, varmembers[e],
                                      matStride);
        }

        var.members = varmembers;
      }
    }
  }
}

void PreprocessLineDirectives(rdcarray<ShaderSourceFile> &sourceFiles)
{
  struct SplitFile
  {
    rdcstr filename;
    rdcarray<rdcstr> lines;
    bool modified = false;
  };

  rdcarray<SplitFile> splitFiles;

  splitFiles.resize(sourceFiles.size());

  for(size_t i = 0; i < sourceFiles.size(); i++)
    splitFiles[i].filename = sourceFiles[i].filename;

  for(size_t i = 0; i < sourceFiles.size(); i++)
  {
    rdcarray<rdcstr> srclines;

    // start off writing to the corresponding output file.
    SplitFile *dstFile = &splitFiles[i];
    bool changedFile = false;

    size_t dstLine = 0;

    // skip this file if it doesn't contain #line
    if(!sourceFiles[i].contents.contains("#line"))
      continue;

    split(sourceFiles[i].contents, srclines, '\n');
    srclines.push_back("");

    // handle #line directives by inserting empty lines or erasing as necessary
    bool seenLine = false;

    for(size_t srcLine = 0; srcLine < srclines.size(); srcLine++)
    {
      if(srclines[srcLine].empty())
      {
        dstLine++;
        continue;
      }

      char *c = &srclines[srcLine][0];
      char *end = c + srclines[srcLine].size();

      while(*c == '\t' || *c == ' ' || *c == '\r')
        c++;

      if(c == end)
      {
        // blank line, just advance line counter
        dstLine++;
        continue;
      }

      if(c + 5 > end || strncmp(c, "#line", 5) != 0)
      {
        // only actually insert the line if we've seen a #line statement. Otherwise we're just
        // doing an identity copy. This can lead to problems e.g. if there are a few statements in
        // a file before the #line we then create a truncated list of lines with only those and
        // then start spitting the #line directives into other files. We still want to have the
        // original file as-is.
        if(seenLine)
        {
          // resize up to account for the current line, if necessary
          dstFile->lines.resize(RDCMAX(dstLine + 1, dstFile->lines.size()));

          // if non-empty, append this line (to allow multiple lines on the same line
          // number to be concatenated). To avoid screwing up line numbers we have to append with
          // a comment and not a newline.
          if(dstFile->lines[dstLine].empty())
            dstFile->lines[dstLine] = srclines[srcLine];
          else
            dstFile->lines[dstLine] += " /* multiple #lines overlapping */ " + srclines[srcLine];

          dstFile->modified = true;
        }

        // advance line counter
        dstLine++;

        continue;
      }

      seenLine = true;

      // we have a #line directive
      c += 5;

      if(c >= end)
      {
        // invalid #line, just advance line counter
        dstLine++;
        continue;
      }

      while(*c == '\t' || *c == ' ')
        c++;

      if(c >= end)
      {
        // invalid #line, just advance line counter
        dstLine++;
        continue;
      }

      // invalid #line, no line number. Skip/ignore and just advance line counter
      if(*c < '0' || *c > '9')
      {
        dstLine++;
        continue;
      }

      size_t newLineNum = 0;
      while(*c >= '0' && *c <= '9')
      {
        newLineNum *= 10;
        newLineNum += int((*c) - '0');
        c++;
      }

      // convert to 0-indexed line number
      if(newLineNum > 0)
        newLineNum--;

      while(*c == '\t' || *c == ' ')
        c++;

      if(*c == '"')
      {
        // filename
        c++;

        char *filename = c;

        // parse out filename
        while(*c != '"' && *c != 0)
        {
          if(*c == '\\')
          {
            // skip escaped characters
            c += 2;
          }
          else
          {
            c++;
          }
        }

        // parsed filename successfully
        if(*c == '"')
        {
          *c = 0;

          rdcstr fname = filename;
          if(fname.empty())
            fname = "unnamed_shader";

          // find the new destination file
          bool found = false;
          size_t dstFileIdx = 0;

          for(size_t f = 0; f < splitFiles.size(); f++)
          {
            if(splitFiles[f].filename == fname)
            {
              found = true;
              dstFileIdx = f;
              break;
            }
          }

          if(found)
          {
            changedFile = (dstFile != &splitFiles[dstFileIdx]);
            dstFile = &splitFiles[dstFileIdx];
          }
          else
          {
            RDCWARN("Couldn't find filename '%s' in #line directive in debug info", fname.c_str());

            // make a dummy file to write into that won't be used.
            splitFiles.push_back(SplitFile());
            splitFiles.back().filename = fname;
            splitFiles.back().modified = true;

            changedFile = true;
            dstFile = &splitFiles.back();
          }

          // set the next line number, and continue processing
          dstLine = newLineNum;

          continue;
        }
        else
        {
          // invalid #line, ignore
          RDCERR("Couldn't parse #line directive: '%s'", srclines[srcLine].c_str());
          continue;
        }
      }
      else
      {
        // No filename. Set the next line number, and continue processing
        dstLine = newLineNum;
        continue;
      }
    }
  }

  for(size_t i = 0; i < sourceFiles.size(); i++)
  {
    if(sourceFiles[i].contents.empty() || splitFiles[i].modified)
    {
      merge(splitFiles[i].lines, sourceFiles[i].contents, '\n');
    }
  }
}

void StandardFillCBufferVariables(ResourceId shader, const rdcarray<ShaderConstant> &invars,
                                  rdcarray<ShaderVariable> &outvars, const bytebuf &data)
{
  // start with offset 0
  StandardFillCBufferVariables(shader, invars, outvars, data, 0);
}

uint64_t CalcMeshOutputSize(uint64_t curSize, uint64_t requiredOutput)
{
  if(curSize == 0)
    curSize = 32 * 1024 * 1024;

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

  if(cfg.position.format.compType == CompType::UInt ||
     cfg.position.format.compType == CompType::SInt || cfg.position.format.compCount == 4)
    ret.w = 0.0f;

  if(useidx && idxData)
  {
    if(vert >= (uint32_t)indices.size())
    {
      valid = false;
      return ret;
    }

    vert = indices[vert];

    if(cfg.position.topology != Topology::TriangleFan && cfg.position.allowRestart)
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
  data += vert * vertexByteStride;

  if(data + fmt.ElementSize() > end)
  {
    valid = false;

    if(fmt.compType == CompType::UInt || fmt.compType == CompType::SInt || fmt.compCount == 4)
      return FloatVector(0.0f, 0.0f, 0.0f, 0.0f);

    return FloatVector(0.0f, 0.0f, 0.0f, 1.0f);
  }

  return DecodeFormattedComponents(fmt, data);
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
  rdcstr ident;

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
  newKey = inthash((uint64_t)cfg.position.allowRestart, newKey);
  newKey = inthash((uint64_t)cfg.position.restartIndex, newKey);

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
      if(cfg.position.allowRestart)
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

    // if it's a fan AND it uses primitive restart, decompose it into a triangle list because the
    // restart changes the central vertex
    if(cfg.position.topology == Topology::TriangleFan && cfg.position.allowRestart)
    {
      PatchTriangleFanRestartIndexBufer(indices, cfg.position.restartIndex);
    }
  }
}

bool HighlightCache::FetchHighlightPositions(const MeshDisplay &cfg, FloatVector &activeVertex,
                                             rdcarray<FloatVector> &activePrim,
                                             rdcarray<FloatVector> &adjacentPrimVertices,
                                             rdcarray<FloatVector> &inactiveVertices)
{
  bool valid = true;

  byte *data = &vertexData[0];
  byte *dataEnd = data + vertexData.size();

  uint32_t idx = cfg.highlightVert;
  Topology meshtopo = cfg.position.topology;

  // if it's a fan AND it uses primitive restart, it was decomposed into a triangle list
  if(meshtopo == Topology::TriangleFan && cfg.position.allowRestart)
  {
    meshtopo = Topology::TriangleList;

    // due to triangle fan expansion the index we need to look up is adjusted
    if(idx > 2)
      idx = (idx - 1) * 3 - 1;
  }

  activeVertex = InterpretVertex(data, idx, cfg, dataEnd, true, valid);

  uint32_t primRestart = 0;
  if(cfg.position.allowRestart)
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
  else if(meshtopo == Topology::TriangleFan)
  {
    // find first vert in primitive. In fans a vert isn't
    // in only one primitive, so we pick the first primitive
    // it's in. This means the first N points are in the first
    // primitive, and thereafter each point is in the next primitive
    uint32_t v = RDCMAX(idx, 2U) - 1;

    // first vert in the whole fan
    activePrim.push_back(InterpretVertex(data, 0, cfg, dataEnd, true, valid));
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

          /*[4]=*/    // 10 in diagram
          InterpretVertex(data, numidx - 4, cfg, dataEnd, true, valid),

          /*[5]=*/    // 11 in diagram
          InterpretVertex(data, numidx - 3, cfg, dataEnd, true, valid),

          /*[6]=*/    // 12 in diagram
          InterpretVertex(data, numidx - 2, cfg, dataEnd, true, valid),

          /*[7]=*/    // 13 in diagram
          InterpretVertex(data, numidx - 1, cfg, dataEnd, true, valid),
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
    uint32_t dim = PatchList_Count(meshtopo);

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

// unique colors generated from https://mokole.com/palette.html
const uint32_t uniqueColors[48] = {
    0xff00008b, 0xff32cd32, 0xff8fbc8f, 0xff8b008b, 0xffb03060, 0xffd2b48c, 0xff9932cc, 0xffff0000,
    0xffff8c00, 0xffffd700, 0xff00ff00, 0xff00ff7f, 0xff4169e1, 0xffe9967a, 0xffdc143c, 0xff00ffff,
    0xff00bfff, 0xff0000ff, 0xffa020f0, 0xffadff2f, 0xffff6347, 0xffda70d6, 0xffff00ff, 0xfff0e68c,
    0xffffff54, 0xff6495ed, 0xffdda0dd, 0xff90ee90, 0xff87ceeb, 0xffff1493, 0xff7fffd4, 0xffff69b4,
    0xff808080, 0xffc0c0c0, 0xff2f4f4f, 0xff556b2f, 0xff8b4513, 0xff6b8e23, 0xff2e8b57, 0xff8b0000,
    0xff483d8b, 0xff008000, 0xffb8860b, 0xff008b8b, 0xff4682b4, 0xffd2691e, 0xff9acd32, 0xffcd5c5c,

};

bytebuf GetDiscardPattern(DiscardType type, const ResourceFormat &fmt, uint32_t rowPitch, bool invert)
{
  static const rdcliteral patterns[] = {
      // DiscardType::RenderPassLoad
      "..#.....##...##..##....##....##..#..#.####..###..##..###..####.."
      "..#....#..#.#..#.#.#...#.#..#..#.##.#..#...#....#..#.#..#.#....."
      "..#....#..#.#..#.#..#..#..#.#..#.##.#..#...#....#..#.#..#.###..."
      "..#....#..#.####.#..#..#..#.#..#.#.##..#...#....####.###..#....."
      "..#....#..#.#..#.#.#...#.#..#..#.#.##..#...#....#..#.#..#.#....."
      "..####..##..#..#.##....##....##..#..#..#....###.#..#.#..#.####.."
      "................................................................"
      "................................................................"_lit,

      // DiscardType::RenderPassStore
      "...###.####..##..###...##....##..#..#.####..###..##..###..####.."
      "..#.....#...#..#.#..#..#.#..#..#.##.#..#...#....#..#.#..#.#....."
      "...#....#...#..#.#..#..#..#.#..#.##.#..#...#....#..#.#..#.###..."
      "....#...#...#..#.###...#..#.#..#.#.##..#...#....####.###..#....."
      ".....#..#...#..#.#..#..#.#..#..#.#.##..#...#....#..#.#..#.#....."
      "..###...#....##..#..#..##....##..#..#..#....###.#..#.#..#.####.."
      "................................................................"
      "................................................................"_lit,

      // DiscardType::UndefinedTransition
      "..#..#.#..#.##...####.####.####.#..#.####.##....####.#..#..###.."
      "..#..#.##.#.#.#..#....#.....#...##.#.#....#.#....#...####.#....."
      "..#..#.##.#.#..#.###..###...#...##.#.###..#..#...#...##.#.#....."
      "..#..#.#.##.#..#.#....#.....#...#.##.#....#..#...#...#..#.#.##.."
      "..#..#.#.##.#.#..#....#.....#...#.##.#....#.#....#...#..#.#..#.."
      "...##..#..#.##...####.#....####.#..#.####.##....####.#..#..##..."
      "................................................................"
      "................................................................"_lit,

      // DiscardType::DiscardCall
      "..##...####..###..###...#...###..##...####.##...####.#..#..###.."
      "..#.#...#...#....#.....#.#..#..#.#.#..#....#.#...#...####.#....."
      "..#..#..#....#...#.....#.#..#..#.#..#.###..#..#..#...##.#.#....."
      "..#..#..#.....#..#....#####.###..#..#.#....#..#..#...#..#.#.##.."
      "..#.#...#......#.#....#...#.#..#.#.#..#....#.#...#...#..#.#..#.."
      "..##...####.###...###.#...#.#..#.##...####.##...####.#..#..##..."
      "................................................................"
      "................................................................"_lit,
      // DiscardType::InvalidateCall
      "...####.#..#.#...#...#...#....####.##.....#...#####.####.##....."
      "....#...##.#.#...#..#.#..#.....#...#.#...#.#....#...#....#.#...."
      "....#...##.#.#...#..#.#..#.....#...#..#..#.#....#...###..#..#..."
      "....#...#.##..#.#..#####.#.....#...#..#.#####...#...#....#..#..."
      "....#...#.##..#.#..#...#.#.....#...#.#..#...#...#...#....#.#...."
      "...####.#..#...#...#...#.####.####.##...#...#...#...####.##....."
      "................................................................"
      "................................................................"_lit,
  };

  const rdcliteral &pattern = patterns[(int)type];

  RDCASSERT(pattern.length() == DiscardPatternWidth * DiscardPatternHeight);

  bytebuf ret;

  if(fmt.type == ResourceFormatType::Regular || fmt.type == ResourceFormatType::A8 ||
     fmt.type == ResourceFormatType::S8)
  {
    byte black[8] = {};
    byte white[8] = {};

    if(fmt.compType == CompType::Float)
    {
      if(fmt.compByteWidth == 8)
      {
        double b = 0.0;
        double w = 1000.0;
        memcpy(black, &b, sizeof(b));
        memcpy(white, &w, sizeof(w));
      }
      else if(fmt.compByteWidth == 4)
      {
        float b = 0.0f;
        float w = 1000.0f;
        memcpy(black, &b, sizeof(b));
        memcpy(white, &w, sizeof(w));
      }
      else
      {
        uint16_t b = ConvertToHalf(0.0f);
        uint16_t w = ConvertToHalf(1000.0f);
        memcpy(black, &b, sizeof(b));
        memcpy(white, &w, sizeof(w));
      }
    }
    else if(fmt.compType == CompType::Depth)
    {
      if(fmt.compByteWidth == 4)
      {
        float b = 0.0f;
        float w = 1.0f;
        memcpy(black, &b, sizeof(b));
        memcpy(white, &w, sizeof(w));
      }
      else
      {
        // other depth formats are normalised
        memset(black, 0, sizeof(black));
        memset(white, 0xff, sizeof(white));
      }
    }
    else if(fmt.compType == CompType::UInt || fmt.compType == CompType::SInt)
    {
      // ints we use 0 and 127 so it's the same for every signed type and byte width
      white[0] = 127;
    }
    else
    {
      // all other types are normalised, so we just set white to 0xff
      memset(black, 0, sizeof(black));
      memset(white, 0xff, sizeof(white));
    }

    uint32_t tightPitch = DiscardPatternWidth * fmt.compByteWidth * fmt.compCount;
    rowPitch = RDCMAX(rowPitch, tightPitch);

    ret.resize(rowPitch * DiscardPatternHeight);
    byte *out = ret.data();

    for(int yi = 0; yi < (int)DiscardPatternHeight; yi++)
    {
      int y = invert ? DiscardPatternHeight - 1 - yi : yi;
      for(int x = 0; x < (int)DiscardPatternWidth; x++)
      {
        char c = pattern.c_str()[y * DiscardPatternWidth + x];
        for(uint8_t i = 0; i < fmt.compCount; i++)
        {
          if(c == '#')
            memcpy(out, white, fmt.compByteWidth);
          else
            memcpy(out, black, fmt.compByteWidth);
          out += fmt.compByteWidth;
        }
      }
      out += (rowPitch - tightPitch);
    }
  }
  else if(fmt.type == ResourceFormatType::R10G10B10A2)
  {
    uint32_t tightPitch = DiscardPatternWidth * sizeof(uint32_t);
    rowPitch = RDCMAX(rowPitch, tightPitch);

    ret.resize(rowPitch * DiscardPatternHeight);
    uint32_t *out = (uint32_t *)ret.data();

    uint32_t minVal = 0;
    uint32_t maxVal = 0xffffffff;

    if(fmt.compType == CompType::UInt)
      maxVal = (127u << 0) | (127u << 10) | (127u << 20) | (3u << 30);

    for(int yi = 0; yi < (int)DiscardPatternHeight; yi++)
    {
      int y = invert ? DiscardPatternHeight - 1 - yi : yi;
      for(int x = 0; x < (int)DiscardPatternWidth; x++)
      {
        char c = pattern.c_str()[y * DiscardPatternWidth + x];
        *(out++) = (c == '#') ? maxVal : minVal;
      }
      out += (rowPitch - tightPitch);
    }
  }
  else if(fmt.type == ResourceFormatType::R5G6B5 || fmt.type == ResourceFormatType::R5G5B5A1 ||
          fmt.type == ResourceFormatType::R4G4B4A4)
  {
    uint32_t tightPitch = DiscardPatternWidth * sizeof(uint16_t);
    rowPitch = RDCMAX(rowPitch, tightPitch);

    ret.resize(rowPitch * DiscardPatternHeight);
    uint16_t *out = (uint16_t *)ret.data();

    for(int yi = 0; yi < (int)DiscardPatternHeight; yi++)
    {
      int y = invert ? DiscardPatternHeight - 1 - yi : yi;
      for(int x = 0; x < (int)DiscardPatternWidth; x++)
      {
        char c = pattern.c_str()[y * DiscardPatternWidth + x];
        *(out++) = (c == '#') ? 0xffff : 0x0000;
      }
      out += (rowPitch - tightPitch);
    }
  }
  else if(fmt.type == ResourceFormatType::R4G4)
  {
    uint32_t tightPitch = DiscardPatternWidth * sizeof(uint8_t);
    rowPitch = RDCMAX(rowPitch, tightPitch);

    ret.resize(rowPitch * DiscardPatternHeight);
    byte *out = ret.data();

    for(int yi = 0; yi < (int)DiscardPatternHeight; yi++)
    {
      int y = invert ? DiscardPatternHeight - 1 - yi : yi;
      for(int x = 0; x < (int)DiscardPatternWidth; x++)
      {
        char c = pattern.c_str()[y * DiscardPatternWidth + x];
        *(out++) = (c == '#') ? 0xff : 0x00;
      }
      out += (rowPitch - tightPitch);
    }
  }
  else if(fmt.type == ResourceFormatType::R11G11B10)
  {
    const uint32_t black = ConvertToR11G11B10(Vec3f(0.0f, 0.0f, 0.0f));
    const uint32_t white = ConvertToR11G11B10(Vec3f(1000.0f, 1000.0f, 1000.0f));

    uint32_t tightPitch = DiscardPatternWidth * sizeof(uint32_t);
    rowPitch = RDCMAX(rowPitch, tightPitch);

    ret.resize(rowPitch * DiscardPatternHeight);
    uint32_t *out = (uint32_t *)ret.data();

    for(int yi = 0; yi < (int)DiscardPatternHeight; yi++)
    {
      int y = invert ? DiscardPatternHeight - 1 - yi : yi;
      for(int x = 0; x < (int)DiscardPatternWidth; x++)
      {
        char c = pattern.c_str()[y * DiscardPatternWidth + x];
        *(out++) = (c == '#') ? white : black;
      }
      out += (rowPitch - tightPitch);
    }
  }
  else if(fmt.type == ResourceFormatType::R9G9B9E5)
  {
    const uint32_t black = ConvertToR9G9B9E5(Vec3f(0.0f, 0.0f, 0.0f));
    const uint32_t white = ConvertToR9G9B9E5(Vec3f(1000.0f, 1000.0f, 1000.0f));

    uint32_t tightPitch = DiscardPatternWidth * sizeof(uint32_t);
    rowPitch = RDCMAX(rowPitch, tightPitch);

    ret.resize(rowPitch * DiscardPatternHeight);
    uint32_t *out = (uint32_t *)ret.data();

    for(int yi = 0; yi < (int)DiscardPatternHeight; yi++)
    {
      int y = invert ? DiscardPatternHeight - 1 - yi : yi;
      for(int x = 0; x < (int)DiscardPatternWidth; x++)
      {
        char c = pattern.c_str()[y * DiscardPatternWidth + x];
        *(out++) = (c == '#') ? white : black;
      }
      out += (rowPitch - tightPitch);
    }
  }
  else if(fmt.type == ResourceFormatType::D16S8 || fmt.type == ResourceFormatType::D24S8 ||
          fmt.type == ResourceFormatType::D32S8)
  {
    uint32_t white = 0xffffffff;
    uint32_t black = 0;

    uint32_t depthStride = 0;

    if(fmt.type == ResourceFormatType::D16S8)
    {
      depthStride = 2;
    }
    else if(fmt.type == ResourceFormatType::D24S8)
    {
      depthStride = 4;
    }
    else if(fmt.type == ResourceFormatType::D32S8)
    {
      depthStride = 4;
      float maxDepth = 1.0f;
      memcpy(&white, &maxDepth, sizeof(float));
    }

    uint32_t tightPitch = DiscardPatternWidth * depthStride;
    uint32_t tightStencilPitch = DiscardPatternWidth * sizeof(byte);
    rowPitch = RDCMAX(rowPitch, RDCMAX(tightPitch, tightStencilPitch));

    ret.resize(rowPitch * DiscardPatternHeight * 2);
    byte *depthOut = ret.data();
    byte *stencilOut = depthOut + rowPitch * DiscardPatternHeight;

    for(int yi = 0; yi < (int)DiscardPatternHeight; yi++)
    {
      int y = invert ? DiscardPatternHeight - 1 - yi : yi;
      for(int x = 0; x < (int)DiscardPatternWidth; x++)
      {
        char c = pattern.c_str()[y * DiscardPatternWidth + x];
        if(c == '#')
        {
          memcpy(depthOut, &white, depthStride);
          *(stencilOut++) = 0xff;
        }
        else
        {
          memcpy(depthOut, &black, depthStride);
          *(stencilOut++) = 0x00;
        }

        depthOut += depthStride;
      }

      depthOut += (rowPitch - tightPitch);
      stencilOut += (rowPitch - tightStencilPitch);
    }
  }
  else if(fmt.type == ResourceFormatType::BC1 || fmt.type == ResourceFormatType::BC2 ||
          fmt.type == ResourceFormatType::BC3 || fmt.type == ResourceFormatType::BC4 ||
          fmt.type == ResourceFormatType::BC5 || fmt.type == ResourceFormatType::BC6 ||
          fmt.type == ResourceFormatType::BC7)
  {
#if ENABLED(RDOC_ANDROID)
    RDCERR("Format %s not supported on android", fmt.Name().c_str());
#else
    const uint16_t whalf = ConvertToHalf(1000.0f);

    byte block[16];

    uint32_t blockSize =
        fmt.type == ResourceFormatType::BC1 || fmt.type == ResourceFormatType::BC4 ? 8 : 16;
    uint32_t tightPitch = (DiscardPatternWidth / 4) * blockSize;
    rowPitch = RDCMAX(rowPitch, tightPitch);

    ret.reserve(rowPitch * (DiscardPatternHeight / 4));

    bytebuf inblock;

    void *bc6opts = NULL;

    CreateOptionsBC6(&bc6opts);

    SetQualityBC6(bc6opts, 0.1f);

    for(uint32_t yi = 0; yi < DiscardPatternHeight; yi += 4)
    {
      uint32_t baseY = invert ? DiscardPatternHeight - 1 - yi : yi;

      for(uint32_t baseX = 0; baseX < DiscardPatternWidth; baseX += 4)
      {
        inblock.clear();

        // inblock is 4x4 RGBA8_UNORM
        if(fmt.type == ResourceFormatType::BC1 || fmt.type == ResourceFormatType::BC2 ||
           fmt.type == ResourceFormatType::BC3 || fmt.type == ResourceFormatType::BC7)
        {
          for(uint32_t y = baseY; y < baseY + 4; invert ? y-- : y++)
          {
            for(uint32_t x = baseX; x < baseX + 4; x++)
            {
              char c = pattern.c_str()[y * DiscardPatternWidth + x];

              inblock.push_back(c == '#' ? 0xff : 0x00);
              inblock.push_back(c == '#' ? 0xff : 0x00);
              inblock.push_back(c == '#' ? 0xff : 0x00);
              inblock.push_back(c == '#' ? 0xff : 0x00);
            }
          }
        }
        // inblock is 4x4 R8_UNORM
        else if(fmt.type == ResourceFormatType::BC4 || fmt.type == ResourceFormatType::BC5)
        {
          for(uint32_t y = baseY; y < baseY + 4; invert ? y-- : y++)
          {
            for(uint32_t x = baseX; x < baseX + 4; x++)
            {
              char c = pattern.c_str()[y * DiscardPatternWidth + x];

              inblock.push_back(c == '#' ? 0xff : 0x00);
            }
          }
        }
        // inblock is 4x4 RGB16_FLOAT
        else if(fmt.type == ResourceFormatType::BC6)
        {
          for(uint32_t y = baseY; y < baseY + 4; invert ? y-- : y++)
          {
            for(uint32_t x = baseX; x < baseX + 4; x++)
            {
              char c = pattern.c_str()[y * DiscardPatternWidth + x];

              inblock.push_back(c == '#' ? (whalf & 0xff) : 0x00);
              inblock.push_back(c == '#' ? ((whalf >> 8) & 0xff) : 0x00);

              inblock.push_back(c == '#' ? (whalf & 0xff) : 0x00);
              inblock.push_back(c == '#' ? ((whalf >> 8) & 0xff) : 0x00);

              inblock.push_back(c == '#' ? (whalf & 0xff) : 0x00);
              inblock.push_back(c == '#' ? ((whalf >> 8) & 0xff) : 0x00);
            }
          }
        }

        if(fmt.type == ResourceFormatType::BC1)
          CompressBlockBC1(inblock.data(), 4 * sizeof(uint32_t), block, NULL);
        else if(fmt.type == ResourceFormatType::BC2)
          CompressBlockBC2(inblock.data(), 4 * sizeof(uint32_t), block, NULL);
        else if(fmt.type == ResourceFormatType::BC3)
          CompressBlockBC3(inblock.data(), 4 * sizeof(uint32_t), block, NULL);
        else if(fmt.type == ResourceFormatType::BC4)
          CompressBlockBC4(inblock.data(), 4, block, NULL);
        else if(fmt.type == ResourceFormatType::BC5)
          CompressBlockBC5(inblock.data(), 4, inblock.data(), 4, block, NULL);
        else if(fmt.type == ResourceFormatType::BC6)
          CompressBlockBC6((uint16_t *)inblock.data(), 4 * 3, block, bc6opts);
        else if(fmt.type == ResourceFormatType::BC7)
          CompressBlockBC7(inblock.data(), 4 * sizeof(uint32_t), block, NULL);

        ret.append(block, blockSize);
      }

      ret.resize(ret.size() + (rowPitch - tightPitch));
    }

    DestroyOptionsBC6(bc6opts);
#endif
  }
  else if(fmt.type == ResourceFormatType::ETC2 || fmt.type == ResourceFormatType::EAC ||
          fmt.type == ResourceFormatType::ASTC || fmt.type == ResourceFormatType::PVRTC ||
          fmt.type == ResourceFormatType::YUV8 || fmt.type == ResourceFormatType::YUV10 ||
          fmt.type == ResourceFormatType::YUV12 || fmt.type == ResourceFormatType::YUV16)
  {
    RDCERR("Format %s not supported for proper discard pattern", fmt.Name().c_str());
  }
  else
  {
    RDCERR("Unhandled format %s needing discard pattern", fmt.Name().c_str());
  }

  // if we didn't get a proper pattern, try at least to do some kind of checkerboard (not knowing if
  // this will align with the format or not)
  if(ret.empty())
  {
    uint32_t tightPitch = DiscardPatternWidth * 16;
    rowPitch = RDCMAX(rowPitch, tightPitch);

    ret.resize(rowPitch * DiscardPatternHeight);

    byte *out = ret.data();
    int val = 0;
    for(uint32_t y = 0; y < DiscardPatternHeight; y++)
    {
      byte *rowout = out;

      for(uint32_t i = 0; i < DiscardPatternWidth; i++)
      {
        memset(rowout, val, 16);
        rowout += 16;

        // toggle between memset(0) and memset(0xff)
        if(val)
          val = 0;
        else
          val = 0xff;
      }

      out += rowPitch;
    }
  }

  return ret;
}

void DeriveNearFar(Vec4f pos, Vec4f pos0, float &nearp, float &farp, bool &found)
{
  //////////////////////////////////////////////////////////////////////////////////
  // derive near/far, assuming a standard perspective matrix
  //
  // the transformation from from pre-projection {Z,W} to post-projection {Z,W}
  // is linear. So we can say Zpost = Zpre*m + c . Here we assume Wpre = 1
  // and we know Wpost = Zpre from the perspective matrix.
  // we can then see from the perspective matrix that
  // m = F/(F-N)
  // c = -(F*N)/(F-N)
  //
  // with re-arranging and substitution, we then get:
  // N = -c/m
  // F = c/(1-m)
  //
  // so if we can derive m and c then we can determine N and F. We can do this with
  // two points, and we pick them reasonably distinct on z to reduce floating-point
  // error

  // skip invalid vertices (w=0)
  if(pos.w != 0.0f && fabsf(pos.w - pos0.w) > 0.01f && fabsf(pos.z - pos0.z) > 0.01f)
  {
    Vec2f A(pos0.w, pos0.z);
    Vec2f B(pos.w, pos.z);

    float m = (B.y - A.y) / (B.x - A.x);
    float c = B.y - B.x * m;

    if(m == 1.0f || c == 0.0f)
      return;

    if(-c / m <= 0.000001f)
      return;

    nearp = -c / m;
    farp = c / (1 - m);

    found = true;
  }
}
