/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2018 Baldur Karlsson
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

#include <float.h>
#include "3rdparty/glslang/SPIRV/spirv.hpp"
#include "driver/shaders/spirv/spirv_common.h"
#include "vk_core.h"
#include "vk_debug.h"
#include "vk_shader_cache.h"

inline uint32_t MakeSPIRVOp(spv::Op op, uint32_t WordCount)
{
  return (uint32_t(op) & spv::OpCodeMask) | (WordCount << spv::WordCountShift);
}

static void AddOutputDumping(const ShaderReflection &refl, const SPIRVPatchData &patchData,
                             const char *entryName, uint32_t &descSet, uint32_t vertexIndexOffset,
                             uint32_t instanceIndexOffset, uint32_t numVerts,
                             vector<uint32_t> &modSpirv, uint32_t &bufStride)
{
  uint32_t *spirv = &modSpirv[0];
  size_t spirvLength = modSpirv.size();

  int numOutputs = refl.outputSignature.count();

  RDCASSERT(numOutputs > 0);

  // save the id bound. We use this whenever we need to allocate ourselves
  // a new ID
  uint32_t idBound = spirv[3];

  // we do multiple passes through the SPIR-V to simplify logic, rather than
  // trying to do as few passes as possible.

  // first try to find a few IDs of things we know we'll probably need:
  // * gl_VertexID, gl_InstanceID (identified by a DecorationBuiltIn)
  // * Int32 type, signed and unsigned
  // * Float types, half, float and double
  // * Input Pointer to Int32 (for declaring gl_VertexID)
  // * UInt32 constants from 0 up to however many outputs we have
  // * The entry point we're after
  //
  // At the same time we find the highest descriptor set used and add a
  // new descriptor set binding on the end for our output buffer. This is
  // much easier than trying to add a new bind to an existing descriptor
  // set (which would cascade into a new descriptor set layout, new pipeline
  // layout, etc etc!). However, this might push us over the limit on number
  // of descriptor sets.
  //
  // we also note the index where decorations end, and the index where
  // functions start, for if we need to add new decorations or new
  // types/constants/global variables
  uint32_t vertidxID = 0;
  uint32_t instidxID = 0;
  uint32_t sint32ID = 0;
  uint32_t sint32PtrInID = 0;
  uint32_t uint32ID = 0;
  uint32_t halfID = 0;
  uint32_t floatID = 0;
  uint32_t doubleID = 0;
  uint32_t entryID = 0;

  struct outputIDs
  {
    uint32_t constID;         // constant ID for the index of this output
    uint32_t basetypeID;      // the type ID for this output. Must be present already by definition!
    uint32_t uniformPtrID;    // Uniform Pointer ID for this output. Used to write the output data
    uint32_t outputPtrID;     // Output Pointer ID for this output. Used to read the output data
  };
  outputIDs outs[100] = {};

  RDCASSERT(numOutputs < 100);

  size_t entryInterfaceOffset = 0;
  size_t entryWordCountOffset = 0;
  uint16_t entryWordCount = 0;
  size_t decorateOffset = 0;
  size_t typeVarOffset = 0;

  descSet = 0;

  size_t it = 5;
  while(it < spirvLength)
  {
    uint16_t WordCount = spirv[it] >> spv::WordCountShift;
    spv::Op opcode = spv::Op(spirv[it] & spv::OpCodeMask);

    // we will use the descriptor set immediately after the last set statically used by the shader.
    // This means we don't have to worry about if the descriptor set layout declares more sets which
    // might be invalid and un-bindable, we just trample over the next set that's unused
    if(opcode == spv::OpDecorate && spirv[it + 2] == spv::DecorationDescriptorSet)
      descSet = RDCMAX(descSet, spirv[it + 3] + 1);

    if(opcode == spv::OpDecorate && spirv[it + 2] == spv::DecorationBuiltIn &&
       spirv[it + 3] == spv::BuiltInVertexIndex)
      vertidxID = spirv[it + 1];

    if(opcode == spv::OpDecorate && spirv[it + 2] == spv::DecorationBuiltIn &&
       spirv[it + 3] == spv::BuiltInInstanceIndex)
      instidxID = spirv[it + 1];

    if(opcode == spv::OpTypeInt && spirv[it + 2] == 32 && spirv[it + 3] == 1)
      sint32ID = spirv[it + 1];

    if(opcode == spv::OpTypeInt && spirv[it + 2] == 32 && spirv[it + 3] == 0)
      uint32ID = spirv[it + 1];

    if(opcode == spv::OpTypeFloat && spirv[it + 2] == 16)
      halfID = spirv[it + 1];

    if(opcode == spv::OpTypeFloat && spirv[it + 2] == 32)
      floatID = spirv[it + 1];

    if(opcode == spv::OpTypeFloat && spirv[it + 2] == 64)
      doubleID = spirv[it + 1];

    if(opcode == spv::OpTypePointer && spirv[it + 2] == spv::StorageClassInput &&
       spirv[it + 3] == sint32ID)
      sint32PtrInID = spirv[it + 1];

    for(int i = 0; i < numOutputs; i++)
    {
      if(opcode == spv::OpConstant && spirv[it + 1] == uint32ID && spirv[it + 3] == (uint32_t)i)
      {
        if(outs[i].constID != 0)
          RDCWARN("identical constant declared with two different IDs %u %u!", spirv[it + 2],
                  outs[i].constID);    // not sure if this is valid or not
        outs[i].constID = spirv[it + 2];
      }

      if(outs[i].basetypeID == 0)
      {
        if(refl.outputSignature[i].compCount > 1 && opcode == spv::OpTypeVector)
        {
          uint32_t baseID = 0;

          if(refl.outputSignature[i].compType == CompType::UInt)
            baseID = uint32ID;
          else if(refl.outputSignature[i].compType == CompType::SInt)
            baseID = sint32ID;
          else if(refl.outputSignature[i].compType == CompType::Float)
            baseID = floatID;
          else if(refl.outputSignature[i].compType == CompType::Double)
            baseID = doubleID;
          else
            RDCERR("Unexpected component type for output signature element");

          // if we have the base type, see if this is the right sized vector of that type
          if(baseID != 0 && spirv[it + 2] == baseID &&
             spirv[it + 3] == refl.outputSignature[i].compCount)
            outs[i].basetypeID = spirv[it + 1];
        }

        // handle non-vectors
        if(refl.outputSignature[i].compCount == 1)
        {
          if(refl.outputSignature[i].compType == CompType::UInt)
            outs[i].basetypeID = uint32ID;
          else if(refl.outputSignature[i].compType == CompType::SInt)
            outs[i].basetypeID = sint32ID;
          else if(refl.outputSignature[i].compType == CompType::Float)
            outs[i].basetypeID = floatID;
          else if(refl.outputSignature[i].compType == CompType::Double)
            outs[i].basetypeID = doubleID;
        }
      }

      // if we've found the base type, try and identify pointers to that type
      if(outs[i].basetypeID != 0 && opcode == spv::OpTypePointer &&
         spirv[it + 2] == spv::StorageClassUniform && spirv[it + 3] == outs[i].basetypeID)
      {
        outs[i].uniformPtrID = spirv[it + 1];
      }

      if(outs[i].basetypeID != 0 && opcode == spv::OpTypePointer &&
         spirv[it + 2] == spv::StorageClassOutput && spirv[it + 3] == outs[i].basetypeID)
      {
        outs[i].outputPtrID = spirv[it + 1];
      }
    }

    if(opcode == spv::OpEntryPoint)
    {
      const char *name = (const char *)&spirv[it + 3];

      if(!strcmp(name, entryName))
      {
        if(entryID != 0)
          RDCERR("Same entry point declared twice! %s", entryName);
        entryID = spirv[it + 2];
      }

      // need to update the WordCount when we add IDs, so store this
      entryWordCountOffset = it;
      entryWordCount = WordCount;

      // where to insert new interface IDs if we add them
      entryInterfaceOffset = it + WordCount;
    }

    // when we reach the types, decorations are over
    if(decorateOffset == 0 && opcode >= spv::OpTypeVoid && opcode <= spv::OpTypeForwardPointer)
      decorateOffset = it;

    // stop when we reach the functions, types are over
    if(opcode == spv::OpFunction)
    {
      typeVarOffset = it;
      break;
    }

    it += WordCount;
  }

  RDCASSERT(entryID != 0);

  for(int i = 0; i < numOutputs; i++)
  {
    // must have at least found the base type, or something has gone seriously wrong
    RDCASSERT(outs[i].basetypeID != 0);
  }

  // if needed add new ID for sint32 type
  if(sint32ID == 0)
  {
    sint32ID = idBound++;

    uint32_t typeOp[] = {
        MakeSPIRVOp(spv::OpTypeInt, 4), sint32ID,
        32U,    // 32-bit
        1U,     // signed
    };

    // insert at the end of the types/variables section
    modSpirv.insert(modSpirv.begin() + typeVarOffset, typeOp, typeOp + ARRAY_COUNT(typeOp));

    // update offsets to account for inserted op
    typeVarOffset += ARRAY_COUNT(typeOp);
  }

  // if needed, new ID for input ptr type
  if(sint32PtrInID == 0 && (vertidxID == 0 || instidxID == 0))
  {
    sint32PtrInID = idBound;
    idBound++;

    uint32_t typeOp[] = {
        MakeSPIRVOp(spv::OpTypePointer, 4), sint32PtrInID, spv::StorageClassInput, sint32ID,
    };

    // insert at the end of the types/variables section
    modSpirv.insert(modSpirv.begin() + typeVarOffset, typeOp, typeOp + ARRAY_COUNT(typeOp));

    // update offsets to account for inserted op
    typeVarOffset += ARRAY_COUNT(typeOp);
  }

  if(vertidxID == 0)
  {
    // need to declare our own "in int gl_VertexID;"

    // new ID for vertex index
    vertidxID = idBound;
    idBound++;

    uint32_t varOp[] = {
        MakeSPIRVOp(spv::OpVariable, 4),
        sint32PtrInID,    // type
        vertidxID,        // variable id
        spv::StorageClassInput,
    };

    // insert at the end of the types/variables section
    modSpirv.insert(modSpirv.begin() + typeVarOffset, varOp, varOp + ARRAY_COUNT(varOp));

    // update offsets to account for inserted op
    typeVarOffset += ARRAY_COUNT(varOp);

    uint32_t decorateOp[] = {
        MakeSPIRVOp(spv::OpDecorate, 4), vertidxID, spv::DecorationBuiltIn, spv::BuiltInVertexIndex,
    };

    // insert at the end of the decorations before the types
    modSpirv.insert(modSpirv.begin() + decorateOffset, decorateOp,
                    decorateOp + ARRAY_COUNT(decorateOp));

    // update offsets to account for inserted op
    typeVarOffset += ARRAY_COUNT(decorateOp);
    decorateOffset += ARRAY_COUNT(decorateOp);

    modSpirv[entryWordCountOffset] = MakeSPIRVOp(spv::OpEntryPoint, ++entryWordCount);

    // need to add this input to the declared interface on OpEntryPoint
    modSpirv.insert(modSpirv.begin() + entryInterfaceOffset, vertidxID);

    // update offsets to account for inserted ID
    entryInterfaceOffset++;
    typeVarOffset++;
    decorateOffset++;
  }

  if(instidxID == 0)
  {
    // need to declare our own "in int gl_InstanceID;"

    // new ID for vertex index
    instidxID = idBound;
    idBound++;

    uint32_t varOp[] = {
        MakeSPIRVOp(spv::OpVariable, 4),
        sint32PtrInID,    // type
        instidxID,        // variable id
        spv::StorageClassInput,
    };

    // insert at the end of the types/variables section
    modSpirv.insert(modSpirv.begin() + typeVarOffset, varOp, varOp + ARRAY_COUNT(varOp));

    // update offsets to account for inserted op
    typeVarOffset += ARRAY_COUNT(varOp);

    uint32_t decorateOp[] = {
        MakeSPIRVOp(spv::OpDecorate, 4), instidxID, spv::DecorationBuiltIn, spv::BuiltInInstanceIndex,
    };

    // insert at the end of the decorations before the types
    modSpirv.insert(modSpirv.begin() + decorateOffset, decorateOp,
                    decorateOp + ARRAY_COUNT(decorateOp));

    // update offsets to account for inserted op
    typeVarOffset += ARRAY_COUNT(decorateOp);
    decorateOffset += ARRAY_COUNT(decorateOp);

    modSpirv[entryWordCountOffset] = MakeSPIRVOp(spv::OpEntryPoint, ++entryWordCount);

    // need to add this input to the declared interface on OpEntryPoint
    modSpirv.insert(modSpirv.begin() + entryInterfaceOffset, instidxID);

    // update offsets to account for inserted ID
    entryInterfaceOffset++;
    typeVarOffset++;
    decorateOffset++;
  }

  // if needed add new ID for uint32 type
  if(uint32ID == 0)
  {
    uint32ID = idBound++;

    uint32_t typeOp[] = {
        MakeSPIRVOp(spv::OpTypeInt, 4), uint32ID,
        32U,    // 32-bit
        0U,     // unsigned
    };

    // insert at the end of the types/variables section
    modSpirv.insert(modSpirv.begin() + typeVarOffset, typeOp, typeOp + ARRAY_COUNT(typeOp));

    // update offsets to account for inserted op
    typeVarOffset += ARRAY_COUNT(typeOp);
  }

  // add any constants we're missing
  for(int i = 0; i < numOutputs; i++)
  {
    if(outs[i].constID == 0)
    {
      outs[i].constID = idBound++;

      uint32_t constantOp[] = {
          MakeSPIRVOp(spv::OpConstant, 4), uint32ID, outs[i].constID, (uint32_t)i,
      };

      // insert at the end of the types/variables/constants section
      modSpirv.insert(modSpirv.begin() + typeVarOffset, constantOp,
                      constantOp + ARRAY_COUNT(constantOp));

      // update offsets to account for inserted op
      typeVarOffset += ARRAY_COUNT(constantOp);
    }
  }

  // add any uniform pointer types we're missing. Note that it's quite likely
  // output types will overlap (think - 5 outputs, 3 of which are float4/vec4)
  // so any time we create a new uniform pointer type, we update all subsequent
  // outputs to refer to it.
  for(int i = 0; i < numOutputs; i++)
  {
    if(outs[i].uniformPtrID == 0)
    {
      outs[i].uniformPtrID = idBound++;

      uint32_t typeOp[] = {
          MakeSPIRVOp(spv::OpTypePointer, 4), outs[i].uniformPtrID, spv::StorageClassUniform,
          outs[i].basetypeID,
      };

      // insert at the end of the types/variables/constants section
      modSpirv.insert(modSpirv.begin() + typeVarOffset, typeOp, typeOp + ARRAY_COUNT(typeOp));

      // update offsets to account for inserted op
      typeVarOffset += ARRAY_COUNT(typeOp);

      // update subsequent outputs of identical type
      for(int j = i + 1; j < numOutputs; j++)
      {
        if(outs[i].basetypeID == outs[j].basetypeID)
        {
          RDCASSERT(outs[j].uniformPtrID == 0);
          outs[j].uniformPtrID = outs[i].uniformPtrID;
        }
      }
    }

    // matrices would have been written through an output pointer of matrix type, but we're reading
    // them vector-by-vector so we may need to declare an output pointer of the corresponding
    // vector type.
    // Otherwise, we expect to re-use the original SPIR-V's output pointer.
    if(outs[i].outputPtrID == 0)
    {
      if(!patchData.outputs[i].isMatrix)
      {
        RDCERR("No output pointer ID found for non-matrix output %d: %s (%u %u)", i,
               refl.outputSignature[i].varName.c_str(), refl.outputSignature[i].compType,
               refl.outputSignature[i].compCount);
      }

      outs[i].outputPtrID = idBound++;

      uint32_t typeOp[] = {
          MakeSPIRVOp(spv::OpTypePointer, 4), outs[i].outputPtrID, spv::StorageClassOutput,
          outs[i].basetypeID,
      };

      // insert at the end of the types/variables/constants section
      modSpirv.insert(modSpirv.begin() + typeVarOffset, typeOp, typeOp + ARRAY_COUNT(typeOp));

      // update offsets to account for inserted op
      typeVarOffset += ARRAY_COUNT(typeOp);

      // update subsequent outputs of identical type
      for(int j = i + 1; j < numOutputs; j++)
      {
        if(outs[i].basetypeID == outs[j].basetypeID)
        {
          RDCASSERT(outs[j].outputPtrID == 0);
          outs[j].outputPtrID = outs[i].outputPtrID;
        }
      }
    }
  }

  uint32_t outBufferVarID = 0;
  uint32_t numVertsConstID = 0;
  uint32_t vertexIndexOffsetConstID = 0;
  uint32_t instanceIndexOffsetConstID = 0;

  // now add the structure type etc for our output buffer
  {
    uint32_t vertStructID = idBound++;

    uint32_t vertStructOp[2 + 100] = {
        MakeSPIRVOp(spv::OpTypeStruct, 2 + numOutputs), vertStructID,
    };

    for(int o = 0; o < numOutputs; o++)
      vertStructOp[2 + o] = outs[o].basetypeID;

    // insert at the end of the types/variables section
    modSpirv.insert(modSpirv.begin() + typeVarOffset, vertStructOp, vertStructOp + 2 + numOutputs);

    // update offsets to account for inserted op
    typeVarOffset += 2 + numOutputs;

    uint32_t runtimeArrayID = idBound++;

    uint32_t runtimeArrayOp[] = {
        MakeSPIRVOp(spv::OpTypeRuntimeArray, 3), runtimeArrayID, vertStructID,
    };

    // insert at the end of the types/variables section
    modSpirv.insert(modSpirv.begin() + typeVarOffset, runtimeArrayOp,
                    runtimeArrayOp + ARRAY_COUNT(runtimeArrayOp));

    // update offsets to account for inserted op
    typeVarOffset += ARRAY_COUNT(runtimeArrayOp);

    // add a constant for the number of verts, the 'instance stride' of the array
    numVertsConstID = idBound++;

    uint32_t instanceStrideConstOp[] = {
        MakeSPIRVOp(spv::OpConstant, 4), sint32ID, numVertsConstID, numVerts,
    };

    // insert at the end of the types/variables section
    modSpirv.insert(modSpirv.begin() + typeVarOffset, instanceStrideConstOp,
                    instanceStrideConstOp + ARRAY_COUNT(instanceStrideConstOp));

    // update offsets to account for inserted op
    typeVarOffset += ARRAY_COUNT(instanceStrideConstOp);

    // add a constant for the value that VertexIndex starts at, so we can get a 0-based vertex index
    vertexIndexOffsetConstID = idBound++;

    uint32_t vertexIndexOffsetConstOp[] = {
        MakeSPIRVOp(spv::OpConstant, 4), sint32ID, vertexIndexOffsetConstID, vertexIndexOffset,
    };

    // insert at the end of the types/variables section
    modSpirv.insert(modSpirv.begin() + typeVarOffset, vertexIndexOffsetConstOp,
                    vertexIndexOffsetConstOp + ARRAY_COUNT(vertexIndexOffsetConstOp));

    // update offsets to account for inserted op
    typeVarOffset += ARRAY_COUNT(vertexIndexOffsetConstOp);

    // add a constant for the value that InstanceIndex starts at, so we can get a 0-based instance
    // index
    instanceIndexOffsetConstID = idBound++;

    uint32_t instanceIndexOffsetConstOp[] = {
        MakeSPIRVOp(spv::OpConstant, 4), sint32ID, instanceIndexOffsetConstID, instanceIndexOffset,
    };

    // insert at the end of the types/variables section
    modSpirv.insert(modSpirv.begin() + typeVarOffset, instanceIndexOffsetConstOp,
                    instanceIndexOffsetConstOp + ARRAY_COUNT(instanceIndexOffsetConstOp));

    // update offsets to account for inserted op
    typeVarOffset += ARRAY_COUNT(instanceIndexOffsetConstOp);

    uint32_t outputStructID = idBound++;

    uint32_t outputStructOp[] = {
        MakeSPIRVOp(spv::OpTypeStruct, 3), outputStructID, runtimeArrayID,
    };

    // insert at the end of the types/variables section
    modSpirv.insert(modSpirv.begin() + typeVarOffset, outputStructOp,
                    outputStructOp + ARRAY_COUNT(outputStructOp));

    // update offsets to account for inserted op
    typeVarOffset += ARRAY_COUNT(outputStructOp);

    uint32_t outputStructPtrID = idBound++;

    uint32_t outputStructPtrOp[] = {
        MakeSPIRVOp(spv::OpTypePointer, 4), outputStructPtrID, spv::StorageClassUniform,
        outputStructID,
    };

    // insert at the end of the types/variables section
    modSpirv.insert(modSpirv.begin() + typeVarOffset, outputStructPtrOp,
                    outputStructPtrOp + ARRAY_COUNT(outputStructPtrOp));

    // update offsets to account for inserted op
    typeVarOffset += ARRAY_COUNT(outputStructPtrOp);

    outBufferVarID = idBound++;

    uint32_t outputVarOp[] = {
        MakeSPIRVOp(spv::OpVariable, 4), outputStructPtrID, outBufferVarID, spv::StorageClassUniform,
    };

    // insert at the end of the types/variables section
    modSpirv.insert(modSpirv.begin() + typeVarOffset, outputVarOp,
                    outputVarOp + ARRAY_COUNT(outputVarOp));

    // update offsets to account for inserted op
    typeVarOffset += ARRAY_COUNT(outputVarOp);

    // need to add decorations as appropriate
    vector<uint32_t> decorations;

    // reserve room for 1 member decorate per output, plus
    // other fixed decorations
    decorations.reserve(5 * numOutputs + 20);

    uint32_t memberOffset = 0;
    for(int o = 0; o < numOutputs; o++)
    {
      uint32_t elemSize = 0;
      if(refl.outputSignature[o].compType == CompType::Double)
        elemSize = 8;
      else if(refl.outputSignature[o].compType == CompType::SInt ||
              refl.outputSignature[o].compType == CompType::UInt ||
              refl.outputSignature[o].compType == CompType::Float)
        elemSize = 4;
      else
        RDCERR("Unexpected component type for output signature element");

      uint32_t numComps = refl.outputSignature[o].compCount;

      // ensure member is std430 packed (vec4 alignment for vec3/vec4)
      if(numComps == 2)
        memberOffset = AlignUp(memberOffset, 2U * elemSize);
      else if(numComps > 2)
        memberOffset = AlignUp(memberOffset, 4U * elemSize);

      decorations.push_back(MakeSPIRVOp(spv::OpMemberDecorate, 5));
      decorations.push_back(vertStructID);
      decorations.push_back((uint32_t)o);
      decorations.push_back(spv::DecorationOffset);
      decorations.push_back(memberOffset);

      memberOffset += elemSize * refl.outputSignature[o].compCount;
    }

    // align to 16 bytes (vec4) since we will almost certainly have
    // a vec4 in the struct somewhere, and even in std430 alignment,
    // the base struct alignment is still the largest base alignment
    // of any member
    memberOffset = AlignUp16(memberOffset);

    // the array is the only element in the output struct, so
    // it's at offset 0
    decorations.push_back(MakeSPIRVOp(spv::OpMemberDecorate, 5));
    decorations.push_back(outputStructID);
    decorations.push_back(0);
    decorations.push_back(spv::DecorationOffset);
    decorations.push_back(0);

    // set array stride
    decorations.push_back(MakeSPIRVOp(spv::OpDecorate, 4));
    decorations.push_back(runtimeArrayID);
    decorations.push_back(spv::DecorationArrayStride);
    decorations.push_back(memberOffset);

    bufStride = memberOffset;

    // set object type
    decorations.push_back(MakeSPIRVOp(spv::OpDecorate, 3));
    decorations.push_back(outputStructID);
    decorations.push_back(spv::DecorationBufferBlock);

    // set binding
    decorations.push_back(MakeSPIRVOp(spv::OpDecorate, 4));
    decorations.push_back(outBufferVarID);
    decorations.push_back(spv::DecorationDescriptorSet);
    decorations.push_back(descSet);

    decorations.push_back(MakeSPIRVOp(spv::OpDecorate, 4));
    decorations.push_back(outBufferVarID);
    decorations.push_back(spv::DecorationBinding);
    decorations.push_back(0);

    // insert at the end of the types/variables section
    modSpirv.insert(modSpirv.begin() + decorateOffset, decorations.begin(), decorations.end());

    // update offsets to account for inserted op
    typeVarOffset += decorations.size();
    decorateOffset += decorations.size();
  }

  vector<uint32_t> dumpCode;

  {
    // bit of a conservative resize. Each output if in a struct could have
    // AccessChain on source = 4 uint32s
    // Load source           = 4 uint32s
    // AccessChain on dest   = 7 uint32s
    // Store dest            = 3 uint32s
    //
    // loading the indices, and multiplying to get the destination array
    // slot is constant on top of that
    dumpCode.reserve(numOutputs * (4 + 4 + 7 + 3) + 4 + 4 + 5 + 5);

    uint32_t loadedVtxID = idBound++;
    dumpCode.push_back(MakeSPIRVOp(spv::OpLoad, 4));
    dumpCode.push_back(sint32ID);
    dumpCode.push_back(loadedVtxID);
    dumpCode.push_back(vertidxID);

    uint32_t loadedInstID = idBound++;
    dumpCode.push_back(MakeSPIRVOp(spv::OpLoad, 4));
    dumpCode.push_back(sint32ID);
    dumpCode.push_back(loadedInstID);
    dumpCode.push_back(instidxID);

    uint32_t rebasedInstID = idBound++;
    dumpCode.push_back(MakeSPIRVOp(spv::OpISub, 5));
    dumpCode.push_back(sint32ID);
    dumpCode.push_back(rebasedInstID);                 // rebasedInst =
    dumpCode.push_back(loadedInstID);                  //    gl_InstanceIndex -
    dumpCode.push_back(instanceIndexOffsetConstID);    //    instanceIndexOffset

    uint32_t startVertID = idBound++;
    dumpCode.push_back(MakeSPIRVOp(spv::OpIMul, 5));
    dumpCode.push_back(sint32ID);
    dumpCode.push_back(startVertID);        // startVert =
    dumpCode.push_back(rebasedInstID);      //    rebasedInst *
    dumpCode.push_back(numVertsConstID);    //    numVerts

    uint32_t rebasedVertID = idBound++;
    dumpCode.push_back(MakeSPIRVOp(spv::OpISub, 5));
    dumpCode.push_back(sint32ID);
    dumpCode.push_back(rebasedVertID);               // rebasedVert =
    dumpCode.push_back(loadedVtxID);                 //    gl_VertexIndex -
    dumpCode.push_back(vertexIndexOffsetConstID);    //    vertexIndexOffset

    uint32_t arraySlotID = idBound++;
    dumpCode.push_back(MakeSPIRVOp(spv::OpIAdd, 5));
    dumpCode.push_back(sint32ID);
    dumpCode.push_back(arraySlotID);      // arraySlot =
    dumpCode.push_back(startVertID);      //    startVert +
    dumpCode.push_back(rebasedVertID);    //    rebasedVert

    for(int o = 0; o < numOutputs; o++)
    {
      uint32_t loaded = 0;

      // not a structure member or array child, can load directly
      if(patchData.outputs[o].accessChain.empty())
      {
        loaded = idBound++;

        dumpCode.push_back(MakeSPIRVOp(spv::OpLoad, 4));
        dumpCode.push_back(outs[o].basetypeID);
        dumpCode.push_back(loaded);
        dumpCode.push_back(patchData.outputs[o].ID);
      }
      else
      {
        uint32_t readPtr = idBound++;
        loaded = idBound++;

        // structure member, need to access chain first
        dumpCode.push_back(
            MakeSPIRVOp(spv::OpAccessChain, 4 + (uint32_t)patchData.outputs[o].accessChain.size()));
        dumpCode.push_back(outs[o].outputPtrID);
        dumpCode.push_back(readPtr);                    // readPtr =
        dumpCode.push_back(patchData.outputs[o].ID);    // outStructWhatever

        for(uint32_t idx : patchData.outputs[o].accessChain)
          dumpCode.push_back(outs[idx].constID);

        dumpCode.push_back(MakeSPIRVOp(spv::OpLoad, 4));
        dumpCode.push_back(outs[o].basetypeID);
        dumpCode.push_back(loaded);
        dumpCode.push_back(readPtr);
      }

      // access chain the destination
      uint32_t writePtr = idBound++;
      dumpCode.push_back(MakeSPIRVOp(spv::OpAccessChain, 7));
      dumpCode.push_back(outs[o].uniformPtrID);
      dumpCode.push_back(writePtr);
      dumpCode.push_back(outBufferVarID);     // outBuffer
      dumpCode.push_back(outs[0].constID);    // .verts
      dumpCode.push_back(arraySlotID);        // [arraySlot]
      dumpCode.push_back(outs[o].constID);    // .out_...

      dumpCode.push_back(MakeSPIRVOp(spv::OpStore, 3));
      dumpCode.push_back(writePtr);
      dumpCode.push_back(loaded);
    }
  }

  // update these values, since vector will have resized and/or reallocated above
  spirv = &modSpirv[0];
  spirvLength = modSpirv.size();

  bool infunc = false;

  it = 5;
  while(it < spirvLength)
  {
    uint16_t WordCount = spirv[it] >> spv::WordCountShift;
    spv::Op opcode = spv::Op(spirv[it] & spv::OpCodeMask);

    // find the start of the entry point
    if(opcode == spv::OpFunction && spirv[it + 2] == entryID)
      infunc = true;

    // insert the dumpCode before any spv::OpReturn.
    // we should not have any spv::OpReturnValue since this is
    // the entry point. Neither should we have OpKill etc.
    if(infunc && opcode == spv::OpReturn)
    {
      modSpirv.insert(modSpirv.begin() + it, dumpCode.begin(), dumpCode.end());

      it += dumpCode.size();

      // update these values, since vector will have resized and/or reallocated above
      spirv = &modSpirv[0];
      spirvLength = modSpirv.size();
    }

    // done patching entry point
    if(opcode == spv::OpFunctionEnd && infunc)
      break;

    it += WordCount;
  }

  // patch up the new id bound
  spirv[3] = idBound;
}

void VulkanDebugManager::ClearPostVSCache()
{
  VkDevice dev = m_Device;

  for(auto it = m_PostVSData.begin(); it != m_PostVSData.end(); ++it)
  {
    m_pDriver->vkDestroyBuffer(dev, it->second.vsout.buf, NULL);
    m_pDriver->vkDestroyBuffer(dev, it->second.vsout.idxBuf, NULL);
    m_pDriver->vkFreeMemory(dev, it->second.vsout.bufmem, NULL);
    m_pDriver->vkFreeMemory(dev, it->second.vsout.idxBufMem, NULL);
  }

  m_PostVSData.clear();
}

void VulkanDebugManager::InitPostVSBuffers(uint32_t eventId)
{
  // go through any aliasing
  if(m_PostVSAlias.find(eventId) != m_PostVSAlias.end())
    eventId = m_PostVSAlias[eventId];

  if(m_PostVSData.find(eventId) != m_PostVSData.end())
    return;

  if(!m_pDriver->GetDeviceFeatures().vertexPipelineStoresAndAtomics)
    return;

  const VulkanRenderState &state = m_pDriver->m_RenderState;
  VulkanCreationInfo &creationInfo = m_pDriver->m_CreationInfo;

  if(state.graphics.pipeline == ResourceId() || state.renderPass == ResourceId())
    return;

  const VulkanCreationInfo::Pipeline &pipeInfo = creationInfo.m_Pipeline[state.graphics.pipeline];

  if(pipeInfo.shaders[0].module == ResourceId())
    return;

  const VulkanCreationInfo::ShaderModule &moduleInfo =
      creationInfo.m_ShaderModule[pipeInfo.shaders[0].module];

  ShaderReflection *refl = pipeInfo.shaders[0].refl;

  // no outputs from this shader? unexpected but theoretically possible (dummy VS before
  // tessellation maybe). Just fill out an empty data set
  if(refl->outputSignature.empty())
  {
    // empty vertex output signature
    m_PostVSData[eventId].vsin.topo = pipeInfo.topology;
    m_PostVSData[eventId].vsout.buf = VK_NULL_HANDLE;
    m_PostVSData[eventId].vsout.instStride = 0;
    m_PostVSData[eventId].vsout.vertStride = 0;
    m_PostVSData[eventId].vsout.nearPlane = 0.0f;
    m_PostVSData[eventId].vsout.farPlane = 0.0f;
    m_PostVSData[eventId].vsout.useIndices = false;
    m_PostVSData[eventId].vsout.hasPosOut = false;
    m_PostVSData[eventId].vsout.idxBuf = VK_NULL_HANDLE;

    m_PostVSData[eventId].vsout.topo = pipeInfo.topology;

    return;
  }

  const DrawcallDescription *drawcall = m_pDriver->GetDrawcall(eventId);

  if(drawcall == NULL || drawcall->numIndices == 0 || drawcall->numInstances == 0)
    return;

  // the SPIR-V patching will determine the next descriptor set to use, after all sets statically
  // used by the shader. This gets around the problem where the shader only uses 0 and 1, but the
  // layout declares 0-4, and 2,3,4 are invalid at bind time and we are unable to bind our new set
  // 5. Instead we'll notice that only 0 and 1 are used and just use 2 ourselves (although it was in
  // the original set layout, we know it's statically unused by the shader so we can safely steal
  // it).
  uint32_t descSet = 0;

  // we go through the driver for all these creations since they need to be properly
  // registered in order to be put in the partial replay state
  VkResult vkr = VK_SUCCESS;
  VkDevice dev = m_Device;

  VkPipelineLayout pipeLayout;

  VkGraphicsPipelineCreateInfo pipeCreateInfo;

  // get pipeline create info
  m_pDriver->GetShaderCache()->MakeGraphicsPipelineInfo(pipeCreateInfo, state.graphics.pipeline);

  // set primitive topology to point list
  VkPipelineInputAssemblyStateCreateInfo *ia =
      (VkPipelineInputAssemblyStateCreateInfo *)pipeCreateInfo.pInputAssemblyState;

  VkPrimitiveTopology topo = ia->topology;

  ia->topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;

  // remove all stages but the vertex shader, we just want to run it and write the data,
  // we don't want to tessellate/geometry shade, nor rasterize (which we disable below)
  uint32_t vertIdx = pipeCreateInfo.stageCount;

  for(uint32_t i = 0; i < pipeCreateInfo.stageCount; i++)
  {
    if(pipeCreateInfo.pStages[i].stage & VK_SHADER_STAGE_VERTEX_BIT)
    {
      vertIdx = i;
      break;
    }
  }

  RDCASSERT(vertIdx < pipeCreateInfo.stageCount);

  if(vertIdx != 0)
    (VkPipelineShaderStageCreateInfo &)pipeCreateInfo.pStages[0] = pipeCreateInfo.pStages[vertIdx];

  pipeCreateInfo.stageCount = 1;

  // enable rasterizer discard
  VkPipelineRasterizationStateCreateInfo *rs =
      (VkPipelineRasterizationStateCreateInfo *)pipeCreateInfo.pRasterizationState;
  rs->rasterizerDiscardEnable = true;

  VkBuffer meshBuffer = VK_NULL_HANDLE, readbackBuffer = VK_NULL_HANDLE;
  VkDeviceMemory meshMem = VK_NULL_HANDLE, readbackMem = VK_NULL_HANDLE;

  VkBuffer idxBuf = VK_NULL_HANDLE, uniqIdxBuf = VK_NULL_HANDLE;
  VkDeviceMemory idxBufMem = VK_NULL_HANDLE, uniqIdxBufMem = VK_NULL_HANDLE;

  uint32_t numVerts = drawcall->numIndices;
  VkDeviceSize bufSize = 0;

  vector<uint32_t> indices;
  uint32_t idxsize = state.ibuffer.bytewidth;
  bool index16 = (idxsize == 2);
  uint32_t numIndices = numVerts;
  bytebuf idxdata;
  uint16_t *idx16 = NULL;
  uint32_t *idx32 = NULL;

  uint32_t minIndex = 0, maxIndex = 0;

  uint32_t vertexIndexOffset = 0;

  if(drawcall->flags & DrawFlags::UseIBuffer)
  {
    // fetch ibuffer
    GetBufferData(state.ibuffer.buf, state.ibuffer.offs + drawcall->indexOffset * idxsize,
                  uint64_t(drawcall->numIndices) * idxsize, idxdata);

    // figure out what the maximum index could be, so we can clamp our index buffer to something
    // sane
    uint32_t maxIdx = 0;

    // if there are no active bindings assume the vertex shader is generating its own data
    // and don't clamp the indices
    if(pipeCreateInfo.pVertexInputState->vertexBindingDescriptionCount == 0)
      maxIdx = ~0U;

    for(uint32_t b = 0; b < pipeCreateInfo.pVertexInputState->vertexBindingDescriptionCount; b++)
    {
      const VkVertexInputBindingDescription &input =
          pipeCreateInfo.pVertexInputState->pVertexBindingDescriptions[b];
      // only vertex inputs (not instance inputs) count
      if(input.inputRate == VK_VERTEX_INPUT_RATE_VERTEX)
      {
        if(b >= state.vbuffers.size())
          continue;

        ResourceId buf = state.vbuffers[b].buf;
        VkDeviceSize offs = state.vbuffers[b].offs;

        VkDeviceSize bufsize = creationInfo.m_Buffer[buf].size;

        // the maximum valid index on this particular input is the one that reaches
        // the end of the buffer. The maximum valid index at all is the one that reads
        // off the end of ALL buffers (so we max it with any other maxindex value
        // calculated).
        if(input.stride > 0)
          maxIdx = RDCMAX(maxIdx, uint32_t((bufsize - offs) / input.stride));
      }
    }

    // in case the vertex buffers were set but had invalid stride (0), max with the number
    // of vertices too. This is fine since the max here is just a conservative limit
    maxIdx = RDCMAX(maxIdx, drawcall->numIndices);

    // do ibuffer rebasing/remapping

    idx16 = (uint16_t *)&idxdata[0];
    idx32 = (uint32_t *)&idxdata[0];

    // only read as many indices as were available in the buffer
    numIndices =
        RDCMIN(uint32_t(index16 ? idxdata.size() / 2 : idxdata.size() / 4), drawcall->numIndices);

    // grab all unique vertex indices referenced
    for(uint32_t i = 0; i < numIndices; i++)
    {
      uint32_t i32 = index16 ? uint32_t(idx16[i]) : idx32[i];

      // we clamp to maxIdx here, to avoid any invalid indices like 0xffffffff
      // from filtering through. Worst case we index to the end of the vertex
      // buffers which is generally much more reasonable
      i32 = RDCMIN(maxIdx, i32);

      auto it = std::lower_bound(indices.begin(), indices.end(), i32);

      if(it != indices.end() && *it == i32)
        continue;

      indices.insert(it, i32);
    }

    // if we read out of bounds, we'll also have a 0 index being referenced
    // (as 0 is read). Don't insert 0 if we already have 0 though
    if(numIndices < drawcall->numIndices && (indices.empty() || indices[0] != 0))
      indices.insert(indices.begin(), 0);

    minIndex = indices[0];
    maxIndex = indices[indices.size() - 1];

    vertexIndexOffset = minIndex + drawcall->baseVertex;

    // set numVerts
    numVerts = maxIndex - minIndex + 1;

    // create buffer with unique 0-based indices
    VkBufferCreateInfo bufInfo = {
        VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        NULL,
        0,
        indices.size() * sizeof(uint32_t),
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
    };

    vkr = m_pDriver->vkCreateBuffer(dev, &bufInfo, NULL, &uniqIdxBuf);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    VkMemoryRequirements mrq = {0};
    m_pDriver->vkGetBufferMemoryRequirements(dev, uniqIdxBuf, &mrq);

    VkMemoryAllocateInfo allocInfo = {
        VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, NULL, mrq.size,
        m_pDriver->GetUploadMemoryIndex(mrq.memoryTypeBits),
    };

    vkr = m_pDriver->vkAllocateMemory(dev, &allocInfo, NULL, &uniqIdxBufMem);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    vkr = m_pDriver->vkBindBufferMemory(dev, uniqIdxBuf, uniqIdxBufMem, 0);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    byte *idxData = NULL;
    vkr = m_pDriver->vkMapMemory(m_Device, uniqIdxBufMem, 0, VK_WHOLE_SIZE, 0, (void **)&idxData);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    memcpy(idxData, &indices[0], indices.size() * sizeof(uint32_t));

    m_pDriver->vkUnmapMemory(m_Device, uniqIdxBufMem);

    bufInfo.size = numIndices * idxsize;

    vkr = m_pDriver->vkCreateBuffer(dev, &bufInfo, NULL, &idxBuf);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    m_pDriver->vkGetBufferMemoryRequirements(dev, idxBuf, &mrq);

    allocInfo.allocationSize = mrq.size;
    allocInfo.memoryTypeIndex = m_pDriver->GetUploadMemoryIndex(mrq.memoryTypeBits);

    vkr = m_pDriver->vkAllocateMemory(dev, &allocInfo, NULL, &idxBufMem);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    vkr = m_pDriver->vkBindBufferMemory(dev, idxBuf, idxBufMem, 0);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);
  }
  else
  {
    // firstVertex
    vertexIndexOffset = drawcall->vertexOffset;
  }

  uint32_t bufStride = 0;
  vector<uint32_t> modSpirv = moduleInfo.spirv.spirv;

  AddOutputDumping(*refl, *pipeInfo.shaders[0].patchData, pipeInfo.shaders[0].entryPoint.c_str(),
                   descSet, vertexIndexOffset, drawcall->instanceOffset, numVerts, modSpirv,
                   bufStride);

  {
    VkDescriptorSetLayout *descSetLayouts;

    // descSet will be the index of our new descriptor set
    descSetLayouts = new VkDescriptorSetLayout[descSet + 1];

    for(uint32_t i = 0; i < descSet; i++)
      descSetLayouts[i] = GetResourceManager()->GetCurrentHandle<VkDescriptorSetLayout>(
          creationInfo.m_PipelineLayout[pipeInfo.layout].descSetLayouts[i]);

    // this layout just says it has one storage buffer
    descSetLayouts[descSet] = m_MeshFetchDescSetLayout;

    const vector<VkPushConstantRange> &push =
        creationInfo.m_PipelineLayout[pipeInfo.layout].pushRanges;

    VkPipelineLayoutCreateInfo pipeLayoutInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        NULL,
        0,
        descSet + 1,
        descSetLayouts,
        (uint32_t)push.size(),
        push.empty() ? NULL : &push[0],
    };

    // create pipeline layout with same descriptor set layouts, plus our mesh output set
    vkr = m_pDriver->vkCreatePipelineLayout(dev, &pipeLayoutInfo, NULL, &pipeLayout);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    SAFE_DELETE_ARRAY(descSetLayouts);

    // repoint pipeline layout
    pipeCreateInfo.layout = pipeLayout;
  }

  // create vertex shader with modified code
  VkShaderModuleCreateInfo moduleCreateInfo = {
      VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, NULL,         0,
      modSpirv.size() * sizeof(uint32_t),          &modSpirv[0],
  };

  VkShaderModule module;
  vkr = m_pDriver->vkCreateShaderModule(dev, &moduleCreateInfo, NULL, &module);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  // change vertex shader to use our modified code
  for(uint32_t i = 0; i < pipeCreateInfo.stageCount; i++)
  {
    VkPipelineShaderStageCreateInfo &sh =
        (VkPipelineShaderStageCreateInfo &)pipeCreateInfo.pStages[i];
    if(sh.stage == VK_SHADER_STAGE_VERTEX_BIT)
    {
      sh.module = module;
      // entry point name remains the same
      break;
    }
  }

  // create new pipeline
  VkPipeline pipe;
  vkr = m_pDriver->vkCreateGraphicsPipelines(m_Device, VK_NULL_HANDLE, 1, &pipeCreateInfo, NULL,
                                             &pipe);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  // make copy of state to draw from
  VulkanRenderState modifiedstate = state;

  // bind created pipeline to partial replay state
  modifiedstate.graphics.pipeline = GetResID(pipe);

  // push back extra descriptor set to partial replay state
  // note that we examined the used pipeline layout above and inserted our descriptor set
  // after any the application used. So there might be more bound, but we want to ensure to
  // bind to the slot we're using
  modifiedstate.graphics.descSets.resize(descSet + 1);
  modifiedstate.graphics.descSets[descSet].descSet = GetResID(m_MeshFetchDescSet);

  if(!(drawcall->flags & DrawFlags::UseIBuffer))
  {
    // create buffer of sufficient size (num indices * bufStride)
    VkBufferCreateInfo bufInfo = {
        VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        NULL,
        0,
        drawcall->numIndices * drawcall->numInstances * bufStride,
        0,
    };

    bufSize = bufInfo.size;

    bufInfo.usage |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bufInfo.usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    bufInfo.usage |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    bufInfo.usage |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;

    vkr = m_pDriver->vkCreateBuffer(dev, &bufInfo, NULL, &meshBuffer);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    bufInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

    vkr = m_pDriver->vkCreateBuffer(dev, &bufInfo, NULL, &readbackBuffer);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    VkMemoryRequirements mrq = {0};
    m_pDriver->vkGetBufferMemoryRequirements(dev, meshBuffer, &mrq);

    VkMemoryAllocateInfo allocInfo = {
        VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, NULL, mrq.size,
        m_pDriver->GetGPULocalMemoryIndex(mrq.memoryTypeBits),
    };

    vkr = m_pDriver->vkAllocateMemory(dev, &allocInfo, NULL, &meshMem);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    vkr = m_pDriver->vkBindBufferMemory(dev, meshBuffer, meshMem, 0);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    m_pDriver->vkGetBufferMemoryRequirements(dev, readbackBuffer, &mrq);

    allocInfo.memoryTypeIndex = m_pDriver->GetReadbackMemoryIndex(mrq.memoryTypeBits);

    vkr = m_pDriver->vkAllocateMemory(dev, &allocInfo, NULL, &readbackMem);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    vkr = m_pDriver->vkBindBufferMemory(dev, readbackBuffer, readbackMem, 0);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    // vkUpdateDescriptorSet desc set to point to buffer
    VkDescriptorBufferInfo fetchdesc = {0};
    fetchdesc.buffer = meshBuffer;
    fetchdesc.offset = 0;
    fetchdesc.range = bufInfo.size;

    VkWriteDescriptorSet write = {
        VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, m_MeshFetchDescSet, 0,   0, 1,
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,      NULL, &fetchdesc,         NULL};
    m_pDriver->vkUpdateDescriptorSets(dev, 1, &write, 0, NULL);

    VkCommandBuffer cmd = m_pDriver->GetNextCmd();

    VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL,
                                          VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};

    vkr = ObjDisp(dev)->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    // do single draw
    modifiedstate.BeginRenderPassAndApplyState(cmd, VulkanRenderState::BindGraphics);
    ObjDisp(cmd)->CmdDraw(Unwrap(cmd), drawcall->numIndices, drawcall->numInstances,
                          drawcall->vertexOffset, drawcall->instanceOffset);
    modifiedstate.EndRenderPass(cmd);

    VkBufferMemoryBarrier meshbufbarrier = {
        VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
        NULL,
        VK_ACCESS_SHADER_WRITE_BIT,
        VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT,
        VK_QUEUE_FAMILY_IGNORED,
        VK_QUEUE_FAMILY_IGNORED,
        Unwrap(meshBuffer),
        0,
        bufInfo.size,
    };

    // wait for writing to finish
    DoPipelineBarrier(cmd, 1, &meshbufbarrier);

    VkBufferCopy bufcopy = {
        0, 0, bufInfo.size,
    };

    // copy to readback buffer
    ObjDisp(dev)->CmdCopyBuffer(Unwrap(cmd), Unwrap(meshBuffer), Unwrap(readbackBuffer), 1, &bufcopy);

    meshbufbarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    meshbufbarrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
    meshbufbarrier.buffer = Unwrap(readbackBuffer);

    // wait for copy to finish
    DoPipelineBarrier(cmd, 1, &meshbufbarrier);

    vkr = ObjDisp(dev)->EndCommandBuffer(Unwrap(cmd));
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    // submit & flush so that we don't have to keep pipeline around for a while
    m_pDriver->SubmitCmds();
    m_pDriver->FlushQ();
  }
  else
  {
    // create buffer of sufficient size
    // this can't just be bufStride * num unique indices per instance, as we don't
    // have a compact 0-based index to index into the buffer. We must use
    // index-minIndex which is 0-based but potentially sparse, so this buffer may
    // be more or less wasteful
    VkBufferCreateInfo bufInfo = {
        VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,          NULL, 0,
        numVerts * drawcall->numInstances * bufStride, 0,
    };

    bufInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bufInfo.usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    bufInfo.usage |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    bufInfo.usage |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;

    vkr = m_pDriver->vkCreateBuffer(dev, &bufInfo, NULL, &meshBuffer);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    bufInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

    vkr = m_pDriver->vkCreateBuffer(dev, &bufInfo, NULL, &readbackBuffer);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    VkMemoryRequirements mrq = {0};
    m_pDriver->vkGetBufferMemoryRequirements(dev, meshBuffer, &mrq);

    VkMemoryAllocateInfo allocInfo = {
        VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, NULL, mrq.size,
        m_pDriver->GetGPULocalMemoryIndex(mrq.memoryTypeBits),
    };

    vkr = m_pDriver->vkAllocateMemory(dev, &allocInfo, NULL, &meshMem);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    vkr = m_pDriver->vkBindBufferMemory(dev, meshBuffer, meshMem, 0);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    m_pDriver->vkGetBufferMemoryRequirements(dev, readbackBuffer, &mrq);

    allocInfo.memoryTypeIndex = m_pDriver->GetReadbackMemoryIndex(mrq.memoryTypeBits);

    vkr = m_pDriver->vkAllocateMemory(dev, &allocInfo, NULL, &readbackMem);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    vkr = m_pDriver->vkBindBufferMemory(dev, readbackBuffer, readbackMem, 0);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    VkBufferMemoryBarrier meshbufbarrier = {
        VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
        NULL,
        VK_ACCESS_HOST_WRITE_BIT,
        VK_ACCESS_INDEX_READ_BIT,
        VK_QUEUE_FAMILY_IGNORED,
        VK_QUEUE_FAMILY_IGNORED,
        Unwrap(uniqIdxBuf),
        0,
        indices.size() * sizeof(uint32_t),
    };

    VkCommandBuffer cmd = m_pDriver->GetNextCmd();

    VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL,
                                          VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};

    vkr = ObjDisp(dev)->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    // wait for upload to finish
    DoPipelineBarrier(cmd, 1, &meshbufbarrier);

    // fill destination buffer with 0s to ensure unwritten vertices have sane data
    ObjDisp(dev)->CmdFillBuffer(Unwrap(cmd), Unwrap(meshBuffer), 0, bufInfo.size, 0);

    // wait to finish
    meshbufbarrier.buffer = Unwrap(meshBuffer);
    meshbufbarrier.size = bufInfo.size;
    DoPipelineBarrier(cmd, 1, &meshbufbarrier);

    // set bufSize
    bufSize = numVerts * drawcall->numInstances * bufStride;

    // bind unique'd ibuffer
    modifiedstate.ibuffer.bytewidth = 4;
    modifiedstate.ibuffer.offs = 0;
    modifiedstate.ibuffer.buf = GetResID(uniqIdxBuf);

    // vkUpdateDescriptorSet desc set to point to buffer
    VkDescriptorBufferInfo fetchdesc = {0};
    fetchdesc.buffer = meshBuffer;
    fetchdesc.offset = 0;
    fetchdesc.range = bufInfo.size;

    VkWriteDescriptorSet write = {
        VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, m_MeshFetchDescSet, 0,   0, 1,
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,      NULL, &fetchdesc,         NULL};
    m_pDriver->vkUpdateDescriptorSets(dev, 1, &write, 0, NULL);

    // do single draw
    modifiedstate.BeginRenderPassAndApplyState(cmd, VulkanRenderState::BindGraphics);
    ObjDisp(cmd)->CmdDrawIndexed(Unwrap(cmd), (uint32_t)indices.size(), drawcall->numInstances, 0,
                                 drawcall->baseVertex, drawcall->instanceOffset);
    modifiedstate.EndRenderPass(cmd);

    // rebase existing index buffer to point to the right elements in our stream-out'd
    // vertex buffer

    // An index buffer could be something like: 500, 520, 518, 553, 554, 556
    // in which case we can't use the existing index buffer without filling 499 slots of vertex
    // data with padding. Instead we rebase the indices based on the smallest index so it becomes
    // 0, 1, 2, 1, 3, 2 and then that matches our stream-out'd buffer.
    //
    // Note that there could also be gaps in the indices as above which must remain as
    // we don't have a 0-based dense 'vertex id' to base our SSBO indexing off, only index value.

    bool stripRestart = pipeCreateInfo.pInputAssemblyState->primitiveRestartEnable == VK_TRUE &&
                        IsStrip(drawcall->topology);

    if(index16)
    {
      for(uint32_t i = 0; i < numIndices; i++)
      {
        if(stripRestart && idx16[i] == 0xffff)
          continue;

        idx16[i] = idx16[i] - uint16_t(minIndex);
      }
    }
    else
    {
      for(uint32_t i = 0; i < numIndices; i++)
      {
        if(stripRestart && idx32[i] == 0xffffffff)
          continue;

        idx32[i] -= minIndex;
      }
    }

    // upload rebased memory
    byte *idxData = NULL;
    vkr = m_pDriver->vkMapMemory(m_Device, idxBufMem, 0, VK_WHOLE_SIZE, 0, (void **)&idxData);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    memcpy(idxData, idx32, numIndices * idxsize);

    m_pDriver->vkUnmapMemory(m_Device, idxBufMem);

    meshbufbarrier.buffer = Unwrap(idxBuf);
    meshbufbarrier.size = numIndices * idxsize;

    // wait for upload to finish
    DoPipelineBarrier(cmd, 1, &meshbufbarrier);

    // wait for mesh output writing to finish
    meshbufbarrier.buffer = Unwrap(meshBuffer);
    meshbufbarrier.size = bufSize;
    meshbufbarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    meshbufbarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

    DoPipelineBarrier(cmd, 1, &meshbufbarrier);

    VkBufferCopy bufcopy = {
        0, 0, bufInfo.size,
    };

    // copy to readback buffer
    ObjDisp(dev)->CmdCopyBuffer(Unwrap(cmd), Unwrap(meshBuffer), Unwrap(readbackBuffer), 1, &bufcopy);

    meshbufbarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    meshbufbarrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
    meshbufbarrier.buffer = Unwrap(readbackBuffer);

    // wait for copy to finish
    DoPipelineBarrier(cmd, 1, &meshbufbarrier);

    vkr = ObjDisp(dev)->EndCommandBuffer(Unwrap(cmd));
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    // submit & flush so that we don't have to keep pipeline around for a while
    m_pDriver->SubmitCmds();
    m_pDriver->FlushQ();
  }

  // readback mesh data
  byte *byteData = NULL;
  vkr = m_pDriver->vkMapMemory(m_Device, readbackMem, 0, VK_WHOLE_SIZE, 0, (void **)&byteData);

  // do near/far calculations

  float nearp = 0.1f;
  float farp = 100.0f;

  Vec4f *pos0 = (Vec4f *)byteData;

  bool found = false;

  // expect position at the start of the buffer, as system values are sorted first
  // and position is the first value

  for(uint32_t i = 1;
      refl->outputSignature[0].systemValue == ShaderBuiltin::Position && i < numVerts; i++)
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

    Vec4f *pos = (Vec4f *)(byteData + i * bufStride);

    // skip invalid vertices (w=0)
    if(pos->w != 0.0f && fabs(pos->w - pos0->w) > 0.01f && fabs(pos->z - pos0->z) > 0.01f)
    {
      Vec2f A(pos0->w, pos0->z);
      Vec2f B(pos->w, pos->z);

      float m = (B.y - A.y) / (B.x - A.x);
      float c = B.y - B.x * m;

      if(m == 1.0f)
        continue;

      if(-c / m <= 0.000001f)
        continue;

      nearp = -c / m;
      farp = c / (1 - m);

      found = true;

      break;
    }
  }

  // if we didn't find anything, all z's and w's were identical.
  // If the z is positive and w greater for the first element then
  // we detect this projection as reversed z with infinite far plane
  if(!found && pos0->z > 0.0f && pos0->w > pos0->z)
  {
    nearp = pos0->z;
    farp = FLT_MAX;
  }

  m_pDriver->vkUnmapMemory(m_Device, readbackMem);

  // clean up temporary memories
  m_pDriver->vkDestroyBuffer(m_Device, readbackBuffer, NULL);
  m_pDriver->vkFreeMemory(m_Device, readbackMem, NULL);

  if(uniqIdxBuf != VK_NULL_HANDLE)
  {
    m_pDriver->vkDestroyBuffer(m_Device, uniqIdxBuf, NULL);
    m_pDriver->vkFreeMemory(m_Device, uniqIdxBufMem, NULL);
  }

  // fill out m_PostVSData
  m_PostVSData[eventId].vsin.topo = topo;
  m_PostVSData[eventId].vsout.topo = topo;
  m_PostVSData[eventId].vsout.buf = meshBuffer;
  m_PostVSData[eventId].vsout.bufmem = meshMem;

  m_PostVSData[eventId].vsout.vertStride = bufStride;
  m_PostVSData[eventId].vsout.nearPlane = nearp;
  m_PostVSData[eventId].vsout.farPlane = farp;

  m_PostVSData[eventId].vsout.useIndices = bool(drawcall->flags & DrawFlags::UseIBuffer);
  m_PostVSData[eventId].vsout.numVerts = drawcall->numIndices;

  m_PostVSData[eventId].vsout.instStride = 0;
  if(drawcall->flags & DrawFlags::Instanced)
    m_PostVSData[eventId].vsout.instStride = uint32_t(bufSize / drawcall->numInstances);

  m_PostVSData[eventId].vsout.idxBuf = VK_NULL_HANDLE;
  if(m_PostVSData[eventId].vsout.useIndices && idxBuf != VK_NULL_HANDLE)
  {
    m_PostVSData[eventId].vsout.idxBuf = idxBuf;
    m_PostVSData[eventId].vsout.idxBufMem = idxBufMem;
    m_PostVSData[eventId].vsout.idxFmt =
        state.ibuffer.bytewidth == 2 ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32;
  }

  m_PostVSData[eventId].vsout.hasPosOut =
      refl->outputSignature[0].systemValue == ShaderBuiltin::Position;

  // delete pipeline layout
  m_pDriver->vkDestroyPipelineLayout(dev, pipeLayout, NULL);

  // delete pipeline
  m_pDriver->vkDestroyPipeline(dev, pipe, NULL);

  // delete shader/shader module
  m_pDriver->vkDestroyShaderModule(dev, module, NULL);
}

MeshFormat VulkanDebugManager::GetPostVSBuffers(uint32_t eventId, uint32_t instID, MeshDataStage stage)
{
  // go through any aliasing
  if(m_PostVSAlias.find(eventId) != m_PostVSAlias.end())
    eventId = m_PostVSAlias[eventId];

  VulkanPostVSData postvs;
  RDCEraseEl(postvs);

  if(m_PostVSData.find(eventId) != m_PostVSData.end())
    postvs = m_PostVSData[eventId];

  VulkanPostVSData::StageData s = postvs.GetStage(stage);

  MeshFormat ret;

  if(s.useIndices && s.idxBuf != VK_NULL_HANDLE)
  {
    ret.indexResourceId = GetResID(s.idxBuf);
    ret.indexByteStride = s.idxFmt == VK_INDEX_TYPE_UINT16 ? 2 : 4;
  }
  else
  {
    ret.indexResourceId = ResourceId();
    ret.indexByteStride = 0;
  }
  ret.indexByteOffset = 0;
  ret.baseVertex = 0;

  if(s.buf != VK_NULL_HANDLE)
    ret.vertexResourceId = GetResID(s.buf);
  else
    ret.vertexResourceId = ResourceId();

  ret.vertexByteOffset = s.instStride * instID;
  ret.vertexByteStride = s.vertStride;

  ret.format.compCount = 4;
  ret.format.compByteWidth = 4;
  ret.format.compType = CompType::Float;
  ret.format.type = ResourceFormatType::Regular;
  ret.format.bgraOrder = false;

  ret.showAlpha = false;

  ret.topology = MakePrimitiveTopology(s.topo, 1);
  ret.numIndices = s.numVerts;

  ret.unproject = s.hasPosOut;
  ret.nearPlane = s.nearPlane;
  ret.farPlane = s.farPlane;

  return ret;
}
