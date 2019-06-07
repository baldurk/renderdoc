/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Baldur Karlsson
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

#include "spirv_reflect.h"
#include "replay/replay_driver.h"
#include "spirv_editor.h"

void FillSpecConstantVariables(const rdcarray<ShaderConstant> &invars,
                               rdcarray<ShaderVariable> &outvars,
                               const std::vector<SpecConstant> &specInfo)
{
  StandardFillCBufferVariables(invars, outvars, bytebuf());

  RDCASSERTEQUAL(invars.size(), outvars.size());

  for(size_t v = 0; v < invars.size() && v < outvars.size(); v++)
  {
    outvars[v].value.uv[0] = (invars[v].defaultValue & 0xFFFFFFFF);
    outvars[v].value.uv[1] = ((invars[v].defaultValue >> 32) & 0xFFFFFFFF);
  }

  // find any actual values specified
  for(size_t i = 0; i < specInfo.size(); i++)
  {
    for(size_t v = 0; v < invars.size() && v < outvars.size(); v++)
    {
      if(specInfo[i].specID == invars[v].byteOffset)
      {
        memcpy(outvars[v].value.uv, specInfo[i].data.data(),
               RDCMIN(specInfo[i].data.size(), sizeof(outvars[v].value.uv)));
      }
    }
  }
}

void AddXFBAnnotations(const ShaderReflection &refl, const SPIRVPatchData &patchData,
                       const char *entryName, std::vector<uint32_t> &modSpirv, uint32_t &xfbStride)
{
  SPIRVEditor editor(modSpirv);

  rdcarray<SigParameter> outsig = refl.outputSignature;
  std::vector<SPIRVPatchData::InterfaceAccess> outpatch = patchData.outputs;

  rdcspv::Id entryid;
  for(const SPIRVEntry &entry : editor.GetEntries())
  {
    if(entry.name == entryName)
    {
      entryid = entry.id;
      break;
    }
  }

  bool hasXFB = false;

  for(rdcspv::Iter it = editor.Begin(SPIRVSection::ExecutionMode);
      it < editor.End(SPIRVSection::ExecutionMode); ++it)
  {
    if(it.opcode() == spv::OpExecutionMode && rdcspv::Id::fromWord(it.word(1)) == entryid &&
       it.word(2) == spv::ExecutionModeXfb)
    {
      hasXFB = true;
      break;
    }
  }

  if(hasXFB)
  {
    for(rdcspv::Iter it = editor.Begin(SPIRVSection::Annotations);
        it < editor.End(SPIRVSection::Annotations); ++it)
    {
      // remove any existing xfb decorations
      if(it.opcode() == spv::OpDecorate &&
         (it.word(2) == spv::DecorationXfbBuffer || it.word(2) == spv::DecorationXfbStride))
      {
        editor.Remove(it);
      }

      // offset is trickier, need to see if it'll match one we want later
      if((it.opcode() == spv::OpDecorate && it.word(2) == spv::DecorationOffset) ||
         (it.opcode() == spv::OpMemberDecorate && it.word(3) == spv::DecorationOffset))
      {
        for(size_t i = 0; i < outsig.size(); i++)
        {
          if(outpatch[i].structID && !outpatch[i].accessChain.empty())
          {
            if(it.opcode() == spv::OpMemberDecorate && it.word(1) == outpatch[i].structID &&
               it.word(2) == outpatch[i].accessChain.back())
            {
              editor.Remove(it);
            }
          }
          else
          {
            if(it.opcode() == spv::OpDecorate && it.word(1) == outpatch[i].ID)
            {
              editor.Remove(it);
            }
          }
        }
      }
    }
  }
  else
  {
    editor.AddExecutionMode(entryid, spv::ExecutionModeXfb);
  }

  editor.AddCapability(spv::CapabilityTransformFeedback);

  // find the position output and move it to the front
  for(size_t i = 0; i < outsig.size(); i++)
  {
    if(outsig[i].systemValue == ShaderBuiltin::Position)
    {
      outsig.insert(0, outsig[i]);
      outsig.erase(i + 1);

      outpatch.insert(outpatch.begin(), outpatch[i]);
      outpatch.erase(outpatch.begin() + i + 1);
      break;
    }
  }

  for(size_t i = 0; i < outsig.size(); i++)
  {
    if(outpatch[i].isArraySubsequentElement)
    {
      // do not patch anything as we only patch the base array, but reserve space in the stride
    }
    else if(outpatch[i].structID && !outpatch[i].accessChain.empty())
    {
      editor.AddDecoration(rdcspv::Operation(
          spv::OpMemberDecorate,
          {outpatch[i].structID, outpatch[i].accessChain.back(), spv::DecorationOffset, xfbStride}));
    }
    else if(outpatch[i].ID)
    {
      editor.AddDecoration(rdcspv::Operation(
          spv::OpDecorate, {outpatch[i].ID, (uint32_t)spv::DecorationOffset, xfbStride}));
    }

    uint32_t compByteSize = 4;

    if(outsig[i].compType == CompType::Double)
      compByteSize = 8;

    xfbStride += outsig[i].compCount * compByteSize;
  }

  std::set<uint32_t> vars;

  for(size_t i = 0; i < outpatch.size(); i++)
  {
    if(outpatch[i].ID && !outpatch[i].isArraySubsequentElement &&
       vars.find(outpatch[i].ID) == vars.end())
    {
      editor.AddDecoration(rdcspv::Operation(
          spv::OpDecorate, {outpatch[i].ID, (uint32_t)spv::DecorationXfbBuffer, 0}));
      editor.AddDecoration(rdcspv::Operation(
          spv::OpDecorate, {outpatch[i].ID, (uint32_t)spv::DecorationXfbStride, xfbStride}));
      vars.insert(outpatch[i].ID);
    }
  }
}
