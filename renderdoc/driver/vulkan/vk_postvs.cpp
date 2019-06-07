/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2018-2019 Baldur Karlsson
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
#include "3rdparty/glslang/SPIRV/GLSL.std.450.h"
#include "3rdparty/glslang/SPIRV/spirv.hpp"
#include "driver/shaders/spirv/spirv_editor.h"
#include "vk_core.h"
#include "vk_debug.h"
#include "vk_shader_cache.h"

struct VkXfbQueryResult
{
  uint64_t numPrimitivesWritten;
  uint64_t numPrimitivesGenerated;
};

static const char *PatchedMeshOutputEntryPoint = "rdc";
static const uint32_t MeshOutputDispatchWidth = 128;
static const uint32_t MeshOutputTBufferArraySize = 16;

// 0 = output
// 1 = indices
// 2 = float vbuffers
// 3 = uint vbuffers
// 4 = sint vbuffers
static const uint32_t MeshOutputReservedBindings = 5;

static void ConvertToMeshOutputCompute(const ShaderReflection &refl, const SPIRVPatchData &patchData,
                                       const char *entryName, std::vector<uint32_t> instDivisor,
                                       const DrawcallDescription *draw, uint32_t numVerts,
                                       uint32_t numViews, std::vector<uint32_t> &modSpirv,
                                       uint32_t &bufStride)
{
  SPIRVEditor editor(modSpirv);

  uint32_t numInputs = (uint32_t)refl.inputSignature.size();

  uint32_t numOutputs = (uint32_t)refl.outputSignature.size();
  RDCASSERT(numOutputs > 0);

  for(rdcspv::Iter it = editor.Begin(SPIRVSection::Annotations),
                   end = editor.End(SPIRVSection::Annotations);
      it < end; ++it)
  {
    // we will use descriptor set 0 bindings 0..N for our own purposes.
    //
    // Since bindings are arbitrary, we just increase all user bindings to make room, and we'll
    // redeclare the descriptor set layouts and pipeline layout. This is inevitable in the case
    // where all descriptor sets are already used. In theory we only have to do this with set 0, but
    // that requires knowing which variables are in set 0 and it's simpler to increase all bindings.
    if(it.opcode() == spv::OpDecorate && it.word(2) == spv::DecorationBinding)
    {
      RDCASSERT(it.word(3) < (0xffffffff - MeshOutputReservedBindings));
      it.word(3) += MeshOutputReservedBindings;
    }
  }

  // tbuffer types, the values are the descriptor bindings
  enum tbufferType
  {
    tbuffer_undefined,
    tbuffer_float = 2,
    tbuffer_uint = 3,
    tbuffer_sint = 4,
    tbuffer_count,
  };

  struct inputOutputIDs
  {
    // if this is a builtin value, what builtin value is expected
    ShaderBuiltin builtin = ShaderBuiltin::Undefined;
    // ID of the variable
    rdcspv::Id variableID;
    // constant ID for the index of this attribute
    rdcspv::Id constID;
    // the type ID for this attribute. Must be present already by definition!
    rdcspv::Id basetypeID;
    // tbuffer type for this input
    tbufferType tbuffer;
    // gvec4 type for this input, used as result type when fetching from tbuffer
    rdcspv::Id vec4ID;
    // Uniform Pointer ID for this output. Used only for output data, to write to output SSBO
    rdcspv::Id uniformPtrID;
    // Output Pointer ID for this attribute.
    // For inputs, used to 'write' to the global at the start.
    // For outputs, used to 'read' from the global at the end.
    rdcspv::Id privatePtrID;
  };
  std::vector<inputOutputIDs> ins;
  ins.resize(numInputs);
  std::vector<inputOutputIDs> outs;
  outs.resize(numOutputs);

  std::set<rdcspv::Id> inputs;
  std::set<rdcspv::Id> outputs;

  std::map<rdcspv::Id, rdcspv::Id> typeReplacements;

  // rewrite any inputs and outputs to be private storage class
  for(rdcspv::Iter it = editor.Begin(SPIRVSection::TypesVariablesConstants),
                   end = editor.End(SPIRVSection::TypesVariablesConstants);
      it < end; ++it)
  {
    // rewrite any input/output variables to private, and build up inputs/outputs list
    if(it.opcode() == spv::OpTypePointer)
    {
      rdcspv::Id id;

      if(it.word(2) == spv::StorageClassInput)
      {
        id = rdcspv::Id::fromWord(it.word(1));
        inputs.insert(id);
      }
      else if(it.word(2) == spv::StorageClassOutput)
      {
        id = rdcspv::Id::fromWord(it.word(1));
        outputs.insert(id);

        rdcspv::Id baseId = rdcspv::Id::fromWord(it.word(3));

        rdcspv::Iter baseIt = editor.GetID(baseId);
        if(baseIt && baseIt.opcode() == spv::OpTypeStruct)
          outputs.insert(baseId);
      }

      if(id)
      {
        SPIRVPointer privPtr(rdcspv::Id::fromWord(it.word(3)), spv::StorageClassPrivate);

        rdcspv::Id origId = editor.GetType(privPtr);

        if(origId)
        {
          // if we already had a private pointer for this type, we have to use that type - we can't
          // create a new type by aliasing. Thus we need to replace any uses of 'id' with 'origId'.
          typeReplacements[id] = origId;

          // and remove this type declaration
          editor.Remove(it);
        }
        else
        {
          editor.PreModify(it);

          it.word(2) = spv::StorageClassPrivate;

          // if we didn't already have this pointer, process the modified type declaration
          editor.PostModify(it);
        }
      }
    }
    else if(it.opcode() == spv::OpVariable)
    {
      bool mod = false;

      if(it.word(3) == spv::StorageClassInput)
      {
        mod = true;
        editor.PreModify(it);
        it.word(3) = spv::StorageClassPrivate;

        inputs.insert(rdcspv::Id::fromWord(it.word(2)));
      }
      else if(it.word(3) == spv::StorageClassOutput)
      {
        mod = true;
        editor.PreModify(it);
        it.word(3) = spv::StorageClassPrivate;

        outputs.insert(rdcspv::Id::fromWord(it.word(2)));
      }

      auto replIt = typeReplacements.find(rdcspv::Id::fromWord(it.word(1)));
      if(replIt != typeReplacements.end())
      {
        if(!mod)
          editor.PreModify(it);
        mod = true;
        it.word(1) = typeReplacements[rdcspv::Id::fromWord(it.word(1))].value();
      }

      if(mod)
        editor.PostModify(it);

      // if we repointed this variable to an existing private declaration, we must also move it to
      // the end of the section. The reason being that the private pointer type declared may be
      // declared *after* this variable. There can't be any dependencies on this later in the
      // section because it's a variable not a type, so it's safe to move to the end.
      if(replIt != typeReplacements.end())
      {
        // make a copy of the opcode
        rdcspv::Operation op = rdcspv::Operation::copy(it);
        // remove the old one
        editor.Remove(it);
        // add it anew
        editor.AddVariable(op);
      }
    }
    else if(it.opcode() == spv::OpTypeFunction)
    {
      bool mod = false;

      auto replIt = typeReplacements.find(rdcspv::Id::fromWord(it.word(1)));
      if(replIt != typeReplacements.end())
      {
        editor.PreModify(it);
        mod = true;
        it.word(1) = typeReplacements[rdcspv::Id::fromWord(it.word(1))].value();
      }

      for(size_t i = 4; i < it.size(); it++)
      {
        replIt = typeReplacements.find(rdcspv::Id::fromWord(it.word(i)));
        if(replIt != typeReplacements.end())
        {
          if(!mod)
            editor.PreModify(it);
          mod = true;
          it.word(i) = typeReplacements[rdcspv::Id::fromWord(it.word(i))].value();
        }
      }

      if(mod)
        editor.PostModify(it);
    }
    else if(it.opcode() == spv::OpConstantNull)
    {
      auto replIt = typeReplacements.find(rdcspv::Id::fromWord(it.word(1)));
      if(replIt != typeReplacements.end())
      {
        editor.PreModify(it);
        it.word(1) = typeReplacements[rdcspv::Id::fromWord(it.word(1))].value();
        editor.PostModify(it);
      }
    }
  }

  for(rdcspv::Iter it = editor.Begin(SPIRVSection::Functions); it; ++it)
  {
    // identify functions with result types we might want to replace
    if(it.opcode() == spv::OpFunction || it.opcode() == spv::OpFunctionParameter ||
       it.opcode() == spv::OpVariable || it.opcode() == spv::OpAccessChain ||
       it.opcode() == spv::OpInBoundsAccessChain || it.opcode() == spv::OpBitcast ||
       it.opcode() == spv::OpUndef || it.opcode() == spv::OpExtInst ||
       it.opcode() == spv::OpFunctionCall || it.opcode() == spv::OpPhi)
    {
      editor.PreModify(it);

      uint32_t &id = it.word(1);
      auto replIt = typeReplacements.find(rdcspv::Id::fromWord(id));
      if(replIt != typeReplacements.end())
        id = typeReplacements[rdcspv::Id::fromWord(id)].value();

      editor.PostModify(it);
    }
  }

  // detect builtin inputs or outputs, and remove builtin decorations
  for(rdcspv::Iter it = editor.Begin(SPIRVSection::Annotations),
                   end = editor.End(SPIRVSection::Annotations);
      it < end; ++it)
  {
    // remove any builtin decorations
    if(it.opcode() == spv::OpDecorate && it.word(2) == spv::DecorationBuiltIn)
    {
      // we don't have to do anything, the ID mapping is in the SPIRVPatchData, so just discard the
      // location information
      editor.Remove(it);
    }

    if(it.opcode() == spv::OpMemberDecorate && it.word(3) == spv::DecorationBuiltIn)
      editor.Remove(it);

    // remove block decoration from input or output structs
    if(it.opcode() == spv::OpDecorate && it.word(2) == spv::DecorationBlock)
    {
      rdcspv::Id id = rdcspv::Id::fromWord(it.word(1));

      if(outputs.find(id) != outputs.end() || inputs.find(id) != inputs.end())
        editor.Remove(it);
    }

    // remove all invariant decoreations
    if(it.opcode() == spv::OpDecorate && it.word(2) == spv::DecorationInvariant)
    {
      editor.Remove(it);
    }

    if(it.opcode() == spv::OpDecorate && it.word(2) == spv::DecorationLocation)
    {
      // we don't have to do anything, the ID mapping is in the SPIRVPatchData, so just discard the
      // location information
      editor.Remove(it);
    }
  }

  rdcspv::Id entryID;

  std::set<rdcspv::Id> entries;

  for(const SPIRVEntry &entry : editor.GetEntries())
  {
    if(entry.name == entryName)
      entryID = entry.id;

    entries.insert(entry.id);
  }

  RDCASSERT(entryID);

  for(rdcspv::Iter it = editor.Begin(SPIRVSection::Debug), end2 = editor.End(SPIRVSection::Debug);
      it < end2; ++it)
  {
    if(it.opcode() == spv::OpName &&
       (inputs.find(rdcspv::Id::fromWord(it.word(1))) != inputs.end() ||
        outputs.find(rdcspv::Id::fromWord(it.word(1))) != outputs.end()))
    {
      rdcspv::Id id = rdcspv::Id::fromWord(it.word(1));
      std::string oldName = (const char *)&it.word(2);
      editor.Remove(it);
      if(typeReplacements.find(id) == typeReplacements.end())
        editor.SetName(id, ("emulated_" + oldName).c_str());
    }

    // remove any OpName for the old entry points
    if(it.opcode() == spv::OpName && entries.find(rdcspv::Id::fromWord(it.word(1))) != entries.end())
      editor.Remove(it);
  }

  // declare necessary variables per-output, types and constants. We do this last so that we don't
  // add a private pointer that we later try and deduplicate when collapsing output/input pointers
  // to private
  for(uint32_t i = 0; i < numOutputs; i++)
  {
    inputOutputIDs &io = outs[i];

    io.builtin = refl.outputSignature[i].systemValue;

    // constant for this index
    io.constID = editor.AddConstantImmediate(i);

    io.variableID = rdcspv::Id::fromWord(patchData.outputs[i].ID);

    // base type - either a scalar or a vector, since matrix outputs are decayed to vectors
    {
      SPIRVScalar scalarType = scalar<uint32_t>();

      if(refl.outputSignature[i].compType == CompType::UInt)
        scalarType = scalar<uint32_t>();
      else if(refl.outputSignature[i].compType == CompType::SInt)
        scalarType = scalar<int32_t>();
      else if(refl.outputSignature[i].compType == CompType::Float)
        scalarType = scalar<float>();
      else if(refl.outputSignature[i].compType == CompType::Double)
        scalarType = scalar<double>();

      io.vec4ID = editor.DeclareType(SPIRVVector(scalarType, 4));

      if(refl.outputSignature[i].compCount > 1)
        io.basetypeID =
            editor.DeclareType(SPIRVVector(scalarType, refl.outputSignature[i].compCount));
      else
        io.basetypeID = editor.DeclareType(scalarType);
    }

    io.uniformPtrID = editor.DeclareType(SPIRVPointer(io.basetypeID, spv::StorageClassUniform));
    io.privatePtrID = editor.DeclareType(SPIRVPointer(io.basetypeID, spv::StorageClassPrivate));

    RDCASSERT(io.basetypeID && io.vec4ID && io.constID && io.privatePtrID && io.uniformPtrID,
              io.basetypeID, io.vec4ID, io.constID, io.privatePtrID, io.uniformPtrID);
  }

  // repeat for inputs
  for(uint32_t i = 0; i < numInputs; i++)
  {
    inputOutputIDs &io = ins[i];

    io.builtin = refl.inputSignature[i].systemValue;

    // constant for this index
    io.constID = editor.AddConstantImmediate(i);

    io.variableID = rdcspv::Id::fromWord(patchData.inputs[i].ID);

    SPIRVScalar scalarType = scalar<uint32_t>();

    // base type - either a scalar or a vector, since matrix outputs are decayed to vectors
    if(refl.inputSignature[i].compType == CompType::UInt)
    {
      scalarType = scalar<uint32_t>();
      io.tbuffer = tbuffer_uint;
    }
    else if(refl.inputSignature[i].compType == CompType::SInt)
    {
      scalarType = scalar<int32_t>();
      io.tbuffer = tbuffer_sint;
    }
    else if(refl.inputSignature[i].compType == CompType::Float)
    {
      scalarType = scalar<float>();
      io.tbuffer = tbuffer_float;
    }
    else if(refl.inputSignature[i].compType == CompType::Double)
    {
      scalarType = scalar<double>();
      // doubles are loaded packed from a uint tbuffer
      io.tbuffer = tbuffer_uint;
    }

    // doubles are loaded as uvec4 and then packed in pairs, so we need to declare vec4ID as uvec4
    if(refl.inputSignature[i].compType == CompType::Double)
      io.vec4ID = editor.DeclareType(SPIRVVector(scalar<uint32_t>(), 4));
    else
      io.vec4ID = editor.DeclareType(SPIRVVector(scalarType, 4));

    if(refl.inputSignature[i].compCount > 1)
      io.basetypeID = editor.DeclareType(SPIRVVector(scalarType, refl.inputSignature[i].compCount));
    else
      io.basetypeID = editor.DeclareType(scalarType);

    io.privatePtrID = editor.DeclareType(SPIRVPointer(io.basetypeID, spv::StorageClassPrivate));

    RDCASSERT(io.basetypeID && io.vec4ID && io.constID && io.privatePtrID, io.basetypeID, io.vec4ID,
              io.constID, io.privatePtrID);
  }

  struct tbufferIDs
  {
    rdcspv::Id imageTypeID;
    rdcspv::Id imageSampledTypeID;
    rdcspv::Id pointerTypeID;
    rdcspv::Id variableID;
  } tbuffers[tbuffer_count];

  rdcspv::Id arraySize = editor.AddConstantImmediate<uint32_t>(MeshOutputTBufferArraySize);

  for(tbufferType tb : {tbuffer_float, tbuffer_sint, tbuffer_uint})
  {
    SPIRVScalar scalarType = scalar<float>();
    const char *name = "float_vbuffers";

    if(tb == tbuffer_sint)
    {
      scalarType = scalar<int32_t>();
      name = "int_vbuffers";
    }
    else if(tb == tbuffer_uint)
    {
      scalarType = scalar<uint32_t>();
      name = "uint_vbuffers";
    }

    tbuffers[tb].imageTypeID = editor.DeclareType(
        SPIRVImage(scalarType, spv::DimBuffer, 0, 0, 0, 1, spv::ImageFormatUnknown));
    tbuffers[tb].imageSampledTypeID = editor.DeclareType(SPIRVSampledImage(tbuffers[tb].imageTypeID));

    rdcspv::Id arrayType = editor.MakeId();
    editor.AddType(rdcspv::Operation(
        spv::OpTypeArray,
        {arrayType.value(), tbuffers[tb].imageSampledTypeID.value(), arraySize.value()}));

    rdcspv::Id arrayPtrType =
        editor.DeclareType(SPIRVPointer(arrayType, spv::StorageClassUniformConstant));

    tbuffers[tb].pointerTypeID = editor.DeclareType(
        SPIRVPointer(tbuffers[tb].imageSampledTypeID, spv::StorageClassUniformConstant));

    tbuffers[tb].variableID = editor.MakeId();
    editor.AddVariable(rdcspv::Operation(
        spv::OpVariable,
        {arrayPtrType.value(), tbuffers[tb].variableID.value(), spv::StorageClassUniformConstant}));

    editor.SetName(tbuffers[tb].variableID, name);

    editor.AddDecoration(rdcspv::Operation(
        spv::OpDecorate,
        {tbuffers[tb].variableID.value(), (uint32_t)spv::DecorationDescriptorSet, 0}));
    editor.AddDecoration(rdcspv::Operation(
        spv::OpDecorate,
        {tbuffers[tb].variableID.value(), (uint32_t)spv::DecorationBinding, (uint32_t)tb}));
  }

  rdcspv::Id uint32Vec4ID;
  rdcspv::Id idxImageTypeID;
  rdcspv::Id idxImagePtr;
  rdcspv::Id idxSampledTypeID;

  if(draw->flags & DrawFlags::Indexed)
  {
    uint32Vec4ID = editor.DeclareType(SPIRVVector(scalar<uint32_t>(), 4));

    idxImageTypeID = editor.DeclareType(
        SPIRVImage(scalar<uint32_t>(), spv::DimBuffer, 0, 0, 0, 1, spv::ImageFormatUnknown));
    idxSampledTypeID = editor.DeclareType(SPIRVSampledImage(idxImageTypeID));

    rdcspv::Id idxImagePtrType =
        editor.DeclareType(SPIRVPointer(idxSampledTypeID, spv::StorageClassUniformConstant));

    idxImagePtr = editor.MakeId();
    editor.AddVariable(rdcspv::Operation(
        spv::OpVariable,
        {idxImagePtrType.value(), idxImagePtr.value(), spv::StorageClassUniformConstant}));

    editor.SetName(idxImagePtr, "ibuffer");

    editor.AddDecoration(rdcspv::Operation(
        spv::OpDecorate, {idxImagePtr.value(), (uint32_t)spv::DecorationDescriptorSet, 0}));
    editor.AddDecoration(rdcspv::Operation(
        spv::OpDecorate, {idxImagePtr.value(), (uint32_t)spv::DecorationBinding, 1}));
  }

  if(numInputs > 0)
  {
    editor.AddCapability(spv::CapabilitySampledBuffer);
  }

  rdcspv::Id outBufferVarID;
  rdcspv::Id numVertsConstID = editor.AddConstantImmediate<uint32_t>(numVerts);
  rdcspv::Id numInstConstID = editor.AddConstantImmediate<uint32_t>(draw->numInstances);
  rdcspv::Id numViewsConstID = editor.AddConstantImmediate<uint32_t>(numViews);

  editor.SetName(numVertsConstID, "numVerts");
  editor.SetName(numInstConstID, "numInsts");
  editor.SetName(numViewsConstID, "numViews");

  // declare the output buffer and its type
  {
    std::vector<rdcspv::Id> words;
    for(uint32_t o = 0; o < numOutputs; o++)
      words.push_back(outs[o].basetypeID);

    // struct vertex { ... outputs };
    rdcspv::Id vertStructID = editor.DeclareStructType(words);
    editor.SetName(vertStructID, "vertex_struct");

    // vertex vertArray[];
    rdcspv::Id runtimeArrayID = editor.AddType(rdcspv::Operation(
        spv::OpTypeRuntimeArray, {editor.MakeId().value(), vertStructID.value()}));
    editor.SetName(runtimeArrayID, "vertex_array");

    // struct meshOutput { vertex vertArray[]; };
    rdcspv::Id outputStructID = editor.DeclareStructType({runtimeArrayID});
    editor.SetName(outputStructID, "meshOutput");

    // meshOutput *
    rdcspv::Id outputStructPtrID =
        editor.DeclareType(SPIRVPointer(outputStructID, spv::StorageClassUniform));
    editor.SetName(outputStructPtrID, "meshOutput_ptr");

    // meshOutput *outputData;
    outBufferVarID = editor.AddVariable(rdcspv::Operation(
        spv::OpVariable,
        {outputStructPtrID.value(), editor.MakeId().value(), spv::StorageClassUniform}));
    editor.SetName(outBufferVarID, "outputData");

    uint32_t memberOffset = 0;
    for(uint32_t o = 0; o < numOutputs; o++)
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

      // apply decoration to each member in the struct with its offset in the struct
      editor.AddDecoration(rdcspv::Operation(
          spv::OpMemberDecorate, {vertStructID.value(), o, spv::DecorationOffset, memberOffset}));

      memberOffset += elemSize * refl.outputSignature[o].compCount;
    }

    // align to 16 bytes (vec4) since we will almost certainly have
    // a vec4 in the struct somewhere, and even in std430 alignment,
    // the base struct alignment is still the largest base alignment
    // of any member
    bufStride = AlignUp16(memberOffset);

    // the array is the only element in the output struct, so
    // it's at offset 0
    editor.AddDecoration(rdcspv::Operation(spv::OpMemberDecorate,
                                           {outputStructID.value(), 0, spv::DecorationOffset, 0}));

    // set array stride
    editor.AddDecoration(rdcspv::Operation(
        spv::OpDecorate, {runtimeArrayID.value(), spv::DecorationArrayStride, bufStride}));

    // set object type
    editor.AddDecoration(
        rdcspv::Operation(spv::OpDecorate, {outputStructID.value(), spv::DecorationBufferBlock}));

    // set binding
    editor.AddDecoration(rdcspv::Operation(
        spv::OpDecorate, {outBufferVarID.value(), spv::DecorationDescriptorSet, 0}));
    editor.AddDecoration(
        rdcspv::Operation(spv::OpDecorate, {outBufferVarID.value(), spv::DecorationBinding, 0}));
  }

  rdcspv::Id uint32Vec3ID = editor.DeclareType(SPIRVVector(scalar<uint32_t>(), 3));
  rdcspv::Id invocationPtr = editor.DeclareType(SPIRVPointer(uint32Vec3ID, spv::StorageClassInput));
  rdcspv::Id invocationId = editor.AddVariable(rdcspv::Operation(
      spv::OpVariable, {invocationPtr.value(), editor.MakeId().value(), spv::StorageClassInput}));
  editor.AddDecoration(rdcspv::Operation(
      spv::OpDecorate,
      {invocationId.value(), spv::DecorationBuiltIn, spv::BuiltInGlobalInvocationId}));

  editor.SetName(invocationId, "rdoc_invocation");

  // make a new entry point that will call the old function, then when it returns extract & write
  // the outputs.
  rdcspv::Id wrapperEntry = editor.MakeId();
  // don't set a debug name, as some drivers get confused when this doesn't match the entry point
  // name :(.
  // editor.SetName(wrapperEntry, "RenderDoc_MeshFetch_Wrapper_Entrypoint");

  // we remove all entry points and just create one of our own.
  rdcspv::Iter it = editor.Begin(SPIRVSection::EntryPoints);

  {
    // there should already have been at least one entry point
    RDCASSERT(it.opcode() == spv::OpEntryPoint);
    // and it should have been at least 5 words (if not more) since a vertex shader cannot function
    // without at least one interface ID. We only need one, so there should be plenty space.
    RDCASSERT(it.size() >= 5);

    editor.PreModify(it);

    it.nopRemove(5);

    it.word(1) = spv::ExecutionModelGLCompute;
    it.word(2) = wrapperEntry.value();
    it.word(3) = MAKE_FOURCC('r', 'd', 'c', 0);
    it.word(4) = invocationId.value();

    editor.PostModify(it);

    ++it;
  }

  for(rdcspv::Iter end = editor.End(SPIRVSection::EntryPoints); it < end; ++it)
    editor.Remove(it);

  // Strip away any execution modes from the original shaders
  for(it = editor.Begin(SPIRVSection::ExecutionMode); it < editor.End(SPIRVSection::ExecutionMode);
      ++it)
  {
    if(it.opcode() == spv::OpExecutionMode)
    {
      rdcspv::Id modeEntryID = rdcspv::Id::fromWord(it.word(1));

      // We only need to be cautious about what we are stripping for the entry
      // that we are actually translating, the rest aren't used anyways.
      if(modeEntryID == entryID)
      {
        // Lets check to make sure we don't blindly strip away execution modes that
        // might actually have an impact on the behaviour of the shader.
        spv::ExecutionMode execMode = spv::ExecutionMode(it.word(2));
        switch(execMode)
        {
          case spv::ExecutionModeXfb: break;
          default: RDCERR("Unexpected execution mode");
        }
      }

      editor.Remove(it);
    }
  }

  // Add our compute shader execution mode
  editor.AddExecutionMode(wrapperEntry, spv::ExecutionModeLocalSize, {MeshOutputDispatchWidth, 1, 1});

  rdcspv::Id uint32ID = editor.DeclareType(scalar<uint32_t>());

  // add the wrapper function
  {
    std::vector<rdcspv::Operation> ops;

    rdcspv::Id voidType = editor.DeclareType(scalar<void>());
    rdcspv::Id funcType = editor.DeclareType(SPIRVFunction(voidType, {}));

    ops.push_back(rdcspv::Operation(
        spv::OpFunction,
        {voidType.value(), wrapperEntry.value(), spv::FunctionControlMaskNone, funcType.value()}));

    ops.push_back(rdcspv::Operation(spv::OpLabel, {editor.MakeId().value()}));
    {
      // uint3 invocationVec = gl_GlobalInvocationID;
      rdcspv::Id invocationVector = editor.MakeId();
      ops.push_back(rdcspv::Operation(
          spv::OpLoad, {uint32Vec3ID.value(), invocationVector.value(), invocationId.value()}));

      // uint invocation = invocationVec.x
      rdcspv::Id uintInvocationID = editor.MakeId();
      ops.push_back(rdcspv::Operation(
          spv::OpCompositeExtract,
          {uint32ID.value(), uintInvocationID.value(), invocationVector.value(), 0U}));

      // arraySlotID = uintInvocationID;
      rdcspv::Id arraySlotID = uintInvocationID;

      editor.SetName(uintInvocationID, "arraySlot");

      // uint viewinst = uintInvocationID / numVerts
      rdcspv::Id viewinstID = editor.MakeId();
      ops.push_back(
          rdcspv::Operation(spv::OpUDiv, {uint32ID.value(), viewinstID.value(),
                                          uintInvocationID.value(), numVertsConstID.value()}));

      editor.SetName(viewinstID, "viewInstance");

      rdcspv::Id instID = editor.MakeId();
      ops.push_back(rdcspv::Operation(spv::OpUMod, {uint32ID.value(), instID.value(),
                                                    viewinstID.value(), numInstConstID.value()}));

      editor.SetName(instID, "instanceID");

      rdcspv::Id viewID = editor.MakeId();
      ops.push_back(rdcspv::Operation(spv::OpUDiv, {uint32ID.value(), viewID.value(),
                                                    viewinstID.value(), numInstConstID.value()}));

      editor.SetName(viewID, "viewID");

      // bool inBounds = viewID < numViews;
      rdcspv::Id inBounds = editor.MakeId();
      ops.push_back(rdcspv::Operation(spv::OpULessThan,
                                      {editor.DeclareType(scalar<bool>()).value(), inBounds.value(),
                                       viewID.value(), numViewsConstID.value()}));

      // if(inBounds) goto continueLabel; else goto killLabel;
      rdcspv::Id killLabel = editor.MakeId();
      rdcspv::Id continueLabel = editor.MakeId();
      ops.push_back(rdcspv::Operation(spv::OpSelectionMerge,
                                      {killLabel.value(), spv::SelectionControlMaskNone}));
      ops.push_back(rdcspv::Operation(
          spv::OpBranchConditional, {inBounds.value(), continueLabel.value(), killLabel.value()}));

      // continueLabel:
      ops.push_back(rdcspv::Operation(spv::OpLabel, {continueLabel.value()}));

      // uint vtx = uintInvocationID % numVerts
      rdcspv::Id vtxID = editor.MakeId();
      ops.push_back(rdcspv::Operation(
          spv::OpUMod,
          {uint32ID.value(), vtxID.value(), uintInvocationID.value(), numVertsConstID.value()}));

      editor.SetName(vtxID, "vertexID");

      rdcspv::Id vertexIndexID = vtxID;

      // if we're indexing, look up the index buffer. We don't have to apply vertexOffset - it was
      // already applied when we read back and uniq-ified the index buffer.
      if(draw->flags & DrawFlags::Indexed)
      {
        // sampledimage idximg = *idximgPtr;
        rdcspv::Id loaded = editor.MakeId();
        ops.push_back(rdcspv::Operation(
            spv::OpLoad, {idxSampledTypeID.value(), loaded.value(), idxImagePtr.value()}));

        // image rawimg = imageFromSampled(idximg);
        rdcspv::Id rawimg = editor.MakeId();
        ops.push_back(rdcspv::Operation(spv::OpImage,
                                        {idxImageTypeID.value(), rawimg.value(), loaded.value()}));

        // uvec4 result = texelFetch(rawimg, vtxID);
        rdcspv::Id result = editor.MakeId();
        ops.push_back(rdcspv::Operation(spv::OpImageFetch, {uint32Vec4ID.value(), result.value(),
                                                            rawimg.value(), vertexIndexID.value()}));

        // vertexIndex = result.x;
        vertexIndexID = editor.MakeId();
        ops.push_back(rdcspv::Operation(
            spv::OpCompositeExtract, {uint32ID.value(), vertexIndexID.value(), result.value(), 0}));
      }

      // we use the current value of vertexIndex and use instID, to lookup per-vertex and
      // per-instance attributes. This is because when we fetched the vertex data, we advanced by
      // (in non-indexed draws) vertexOffset, and by instanceOffset. Rather than fetching data
      // that's only used as padding skipped over by these offsets.
      rdcspv::Id vertexLookupID = vertexIndexID;
      rdcspv::Id instanceLookupID = instID;

      if(!(draw->flags & DrawFlags::Indexed))
      {
        // for non-indexed draws, we manually apply the vertex offset, but here after we used the
        // 0-based one to calculate the array slot
        vertexIndexID = editor.MakeId();
        ops.push_back(rdcspv::Operation(
            spv::OpIAdd, {uint32ID.value(), vertexIndexID.value(), vtxID.value(),
                          editor.AddConstantImmediate<uint32_t>(draw->vertexOffset).value()}));
      }
      editor.SetName(vertexIndexID, "vertexIndex");

      // instIndex = inst + instOffset
      rdcspv::Id instIndexID = editor.MakeId();
      ops.push_back(rdcspv::Operation(
          spv::OpIAdd, {uint32ID.value(), instIndexID.value(), instID.value(),
                        editor.AddConstantImmediate<uint32_t>(draw->instanceOffset).value()}));
      editor.SetName(instIndexID, "instanceIndex");

      rdcspv::Id idxs[64] = {};

      for(size_t i = 0; i < refl.inputSignature.size(); i++)
      {
        ShaderBuiltin builtin = refl.inputSignature[i].systemValue;
        if(builtin != ShaderBuiltin::Undefined)
        {
          rdcspv::Id valueID;
          CompType compType = CompType::UInt;

          if(builtin == ShaderBuiltin::VertexIndex)
          {
            valueID = vertexIndexID;
          }
          else if(builtin == ShaderBuiltin::InstanceIndex)
          {
            valueID = instIndexID;
          }
          else if(builtin == ShaderBuiltin::ViewportIndex)
          {
            valueID = viewID;
          }
          else if(builtin == ShaderBuiltin::BaseVertex)
          {
            if(draw->flags & DrawFlags::Indexed)
            {
              valueID = editor.AddConstantImmediate<uint32_t>(draw->vertexOffset);
            }
            else
            {
              valueID = editor.AddConstantImmediate<int32_t>(draw->baseVertex);
              compType = CompType::SInt;
            }
          }
          else if(builtin == ShaderBuiltin::BaseInstance)
          {
            valueID = editor.AddConstantImmediate<uint32_t>(draw->instanceOffset);
          }
          else if(builtin == ShaderBuiltin::DrawIndex)
          {
            valueID = editor.AddConstantImmediate<uint32_t>(draw->drawIndex);
          }

          if(valueID)
          {
            if(refl.inputSignature[i].compType == compType)
            {
              ops.push_back(
                  rdcspv::Operation(spv::OpStore, {ins[i].variableID.value(), valueID.value()}));
            }
            else
            {
              rdcspv::Id castedValue = editor.MakeId();
              // assume we can just bitcast
              ops.push_back(rdcspv::Operation(
                  spv::OpBitcast, {ins[i].basetypeID.value(), castedValue.value(), valueID.value()}));
              ops.push_back(rdcspv::Operation(spv::OpStore,
                                              {ins[i].variableID.value(), castedValue.value()}));
            }
          }
          else
          {
            RDCERR("Unsupported/unsupported built-in input %s", ToStr(builtin).c_str());
          }
        }
        else
        {
          if(idxs[i] == 0)
            idxs[i] = editor.AddConstantImmediate<uint32_t>((uint32_t)i);

          if(idxs[refl.inputSignature[i].regIndex] == 0)
            idxs[refl.inputSignature[i].regIndex] =
                editor.AddConstantImmediate<uint32_t>(refl.inputSignature[i].regIndex);

          tbufferIDs tb = tbuffers[ins[i].tbuffer];

          uint32_t location = refl.inputSignature[i].regIndex;

          rdcspv::Id ptrId = editor.MakeId();
          // sampledimage *imgPtr = xxx_tbuffers[i];
          ops.push_back(rdcspv::Operation(
              spv::OpAccessChain, {tb.pointerTypeID.value(), ptrId.value(), tb.variableID.value(),
                                   idxs[refl.inputSignature[i].regIndex].value()}));

          // sampledimage img = *imgPtr;
          rdcspv::Id loaded = editor.MakeId();
          ops.push_back(rdcspv::Operation(
              spv::OpLoad, {tb.imageSampledTypeID.value(), loaded.value(), ptrId.value()}));

          // image rawimg = imageFromSampled(img);
          rdcspv::Id rawimg = editor.MakeId();
          ops.push_back(rdcspv::Operation(
              spv::OpImage, {tb.imageTypeID.value(), rawimg.value(), loaded.value()}));

          // vec4 result = texelFetch(rawimg, vtxID or instID);
          rdcspv::Id idx = vertexLookupID;

          if(location < instDivisor.size())
          {
            uint32_t divisor = instDivisor[location];

            if(divisor == ~0U)
            {
              // this magic value indicates vertex-rate data
              idx = vertexLookupID;
            }
            else if(divisor == 0)
            {
              // if the divisor is 0, all instances read the first value.
              idx = editor.AddConstantImmediate<uint32_t>(0);
            }
            else if(divisor == 1)
            {
              // if the divisor is 1, it's just regular instancing
              idx = instanceLookupID;
            }
            else
            {
              // otherwise we divide by the divisor
              idx = editor.MakeId();
              rdcspv::Id divisorId = editor.AddConstantImmediate<uint32_t>(divisor);
              ops.push_back(rdcspv::Operation(
                  spv::OpUDiv,
                  {uint32ID.value(), idx.value(), instanceLookupID.value(), divisorId.value()}));
            }
          }

          if(refl.inputSignature[i].compType == CompType::Double)
          {
            // since doubles are packed into two uints, we need to multiply the index by two
            rdcspv::Id doubled = editor.MakeId();
            ops.push_back(
                rdcspv::Operation(spv::OpIMul, {uint32ID.value(), doubled.value(), idx.value(),
                                                editor.AddConstantImmediate<uint32_t>(2).value()}));
            idx = doubled;
          }

          rdcspv::Id result = editor.MakeId();
          ops.push_back(rdcspv::Operation(spv::OpImageFetch, {ins[i].vec4ID.value(), result.value(),
                                                              rawimg.value(), idx.value()}));

          if(refl.inputSignature[i].compType == CompType::Double)
          {
            // since doubles are packed into two uints, we now need to fetch more data and do
            // packing. We can fetch the data unconditionally since it's harmless to read out of the
            // bounds of the buffer

            rdcspv::Id nextidx = editor.MakeId();
            ops.push_back(
                rdcspv::Operation(spv::OpIAdd, {uint32ID.value(), nextidx.value(), idx.value(),
                                                editor.AddConstantImmediate<uint32_t>(1).value()}));

            rdcspv::Id result2 = editor.MakeId();
            ops.push_back(rdcspv::Operation(
                spv::OpImageFetch,
                {ins[i].vec4ID.value(), result2.value(), rawimg.value(), nextidx.value()}));

            rdcspv::Id glsl450 = editor.ImportExtInst("GLSL.std.450");

            rdcspv::Id uvec2Type = editor.DeclareType(SPIRVVector(scalar<uint32_t>(), 2));
            rdcspv::Id comps[4] = {};

            for(uint32_t c = 0; c < refl.inputSignature[i].compCount; c++)
            {
              // first extract the uvec2 we want
              rdcspv::Id packed = editor.MakeId();

              // uvec2 packed = result.[xy/zw] / result2.[xy/zw];
              ops.push_back(rdcspv::Operation(spv::OpVectorShuffle,
                                              {uvec2Type.value(), packed.value(), result.value(),
                                               result2.value(), c * 2 + 0, c * 2 + 1}));

              char swizzle[] = "xyzw";

              editor.SetName(packed, StringFormat::Fmt("packed_%c", swizzle[c]).c_str());

              // double comp = PackDouble2x32(packed);
              comps[c] = editor.MakeId();
              ops.push_back(rdcspv::Operation(
                  spv::OpExtInst, {
                                      editor.DeclareType(scalar<double>()).value(), comps[c].value(),
                                      glsl450.value(), GLSLstd450PackDouble2x32, packed.value(),
                                  }));
            }

            // if there's only one component it's ready, otherwise construct a vector
            if(refl.inputSignature[i].compCount == 1)
            {
              result = comps[0];
            }
            else
            {
              result = editor.MakeId();

              std::vector<uint32_t> words = {ins[i].basetypeID.value(), result.value()};

              for(uint32_t c = 0; c < refl.inputSignature[i].compCount; c++)
                words.push_back(comps[c].value());

              // baseTypeN value = result.xyz;
              ops.push_back(rdcspv::Operation(spv::OpCompositeConstruct, words));
            }
          }
          else if(refl.inputSignature[i].compCount == 1)
          {
            // for one component, extract x

            rdcspv::Id swizzleIn = result;
            result = editor.MakeId();

            // baseType value = result.x;
            ops.push_back(rdcspv::Operation(
                spv::OpCompositeExtract,
                {ins[i].basetypeID.value(), result.value(), swizzleIn.value(), 0}));
          }
          else if(refl.inputSignature[i].compCount != 4)
          {
            // for less than 4 components, extract the sub-vector
            rdcspv::Id swizzleIn = result;
            result = editor.MakeId();

            std::vector<uint32_t> words = {ins[i].basetypeID.value(), result.value(),
                                           swizzleIn.value(), swizzleIn.value()};

            for(uint32_t c = 0; c < refl.inputSignature[i].compCount; c++)
              words.push_back(c);

            // baseTypeN value = result.xyz;
            ops.push_back(rdcspv::Operation(spv::OpVectorShuffle, words));
          }

          // copy the 4 component result directly

          // not a composite type, we can store directly
          if(patchData.inputs[i].accessChain.empty())
          {
            // *global = value
            ops.push_back(
                rdcspv::Operation(spv::OpStore, {ins[i].variableID.value(), result.value()}));
          }
          else
          {
            // for composite types we need to access chain first
            rdcspv::Id subElement = editor.MakeId();
            std::vector<uint32_t> words = {ins[i].privatePtrID.value(), subElement.value(),
                                           patchData.inputs[i].ID};

            for(uint32_t accessIdx : patchData.inputs[i].accessChain)
            {
              if(idxs[accessIdx] == 0)
                idxs[accessIdx] = editor.AddConstantImmediate<uint32_t>(accessIdx);

              words.push_back(idxs[accessIdx].value());
            }

            ops.push_back(rdcspv::Operation(spv::OpAccessChain, words));

            ops.push_back(rdcspv::Operation(spv::OpStore, {subElement.value(), result.value()}));
          }
        }
      }

      // real_main();
      ops.push_back(rdcspv::Operation(
          spv::OpFunctionCall, {voidType.value(), editor.MakeId().value(), entryID.value()}));

      rdcspv::Id zero = editor.AddConstantImmediate<uint32_t>(0);

      for(uint32_t o = 0; o < numOutputs; o++)
      {
        rdcspv::Id loaded;

        // not a structure member or array child, can load directly
        if(patchData.outputs[o].accessChain.empty())
        {
          loaded = editor.MakeId();
          // type loaded = *globalvar;
          ops.push_back(rdcspv::Operation(
              spv::OpLoad, {outs[o].basetypeID.value(), loaded.value(), patchData.outputs[o].ID}));
        }
        else
        {
          rdcspv::Id readPtr = editor.MakeId();
          loaded = editor.MakeId();

          // structure member, need to access chain first
          std::vector<uint32_t> words = {outs[o].privatePtrID.value(), readPtr.value(),
                                         patchData.outputs[o].ID};

          for(uint32_t idx : patchData.outputs[o].accessChain)
          {
            if(idxs[idx] == 0)
              idxs[idx] = editor.AddConstantImmediate<uint32_t>(idx);

            words.push_back(idxs[idx].value());
          }

          // type *readPtr = globalvar.globalsub...;
          ops.push_back(rdcspv::Operation(spv::OpAccessChain, words));
          // type loaded = *readPtr;
          ops.push_back(rdcspv::Operation(
              spv::OpLoad, {outs[o].basetypeID.value(), loaded.value(), readPtr.value()}));
        }

        // access chain the destination
        // type *writePtr = outBuffer.verts[arraySlot].outputN
        rdcspv::Id writePtr = editor.MakeId();
        ops.push_back(rdcspv::Operation(
            spv::OpAccessChain,
            {outs[o].uniformPtrID.value(), writePtr.value(), outBufferVarID.value(), zero.value(),
             arraySlotID.value(), outs[o].constID.value()}));

        // *writePtr = loaded;
        ops.push_back(rdcspv::Operation(spv::OpStore, {writePtr.value(), loaded.value()}));
      }

      // goto killLabel;
      ops.push_back(rdcspv::Operation(spv::OpBranch, {killLabel.value()}));

      // killLabel:
      ops.push_back(rdcspv::Operation(spv::OpLabel, {killLabel.value()}));
    }
    ops.push_back(rdcspv::Operation(spv::OpReturn, {}));

    ops.push_back(rdcspv::Operation(spv::OpFunctionEnd, {}));

    editor.AddFunction(ops.data(), ops.size());
  }
}

void VulkanReplay::ClearPostVSCache()
{
  VkDevice dev = m_Device;

  for(auto it = m_PostVS.Data.begin(); it != m_PostVS.Data.end(); ++it)
  {
    if(it->second.vsout.idxbuf != VK_NULL_HANDLE)
    {
      m_pDriver->vkDestroyBuffer(dev, it->second.vsout.idxbuf, NULL);
      m_pDriver->vkFreeMemory(dev, it->second.vsout.idxbufmem, NULL);
    }
    m_pDriver->vkDestroyBuffer(dev, it->second.vsout.buf, NULL);
    m_pDriver->vkFreeMemory(dev, it->second.vsout.bufmem, NULL);

    if(it->second.gsout.buf != VK_NULL_HANDLE)
    {
      m_pDriver->vkDestroyBuffer(dev, it->second.gsout.buf, NULL);
      m_pDriver->vkFreeMemory(dev, it->second.gsout.bufmem, NULL);
    }
  }

  m_PostVS.Data.clear();
}

void VulkanReplay::PatchReservedDescriptors(const VulkanStatePipeline &pipe,
                                            VkDescriptorPool &descpool,
                                            std::vector<VkDescriptorSetLayout> &setLayouts,
                                            std::vector<VkDescriptorSet> &descSets,
                                            VkShaderStageFlagBits patchedBindingStage,
                                            const VkDescriptorSetLayoutBinding *newBindings,
                                            size_t newBindingsCount)
{
  VkDevice dev = m_Device;
  VulkanCreationInfo &creationInfo = m_pDriver->m_CreationInfo;

  const VulkanCreationInfo::Pipeline &pipeInfo = creationInfo.m_Pipeline[pipe.pipeline];

  VkResult vkr = VK_SUCCESS;

  {
    std::vector<VkWriteDescriptorSet> descWrites;
    std::vector<VkDescriptorImageInfo *> allocImgWrites;
    std::vector<VkDescriptorBufferInfo *> allocBufWrites;
    std::vector<VkBufferView *> allocBufViewWrites;

    // one for each descriptor type. 1 of each to start with, we then increment for each descriptor
    // we need to allocate
    VkDescriptorPoolSize poolSizes[11] = {
        {VK_DESCRIPTOR_TYPE_SAMPLER, 1},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1},
        {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1},
        {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1},
        {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1},
        {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1},
    };

    // count up our own
    for(size_t i = 0; i < newBindingsCount; i++)
      poolSizes[newBindings[i].descriptorType].descriptorCount += newBindings[i].descriptorCount;

    const std::vector<ResourceId> &descSetLayoutIds =
        creationInfo.m_PipelineLayout[pipeInfo.layout].descSetLayouts;

    // need to add our added bindings to the first descriptor set
    std::vector<VkDescriptorSetLayoutBinding> bindings(newBindings, newBindings + newBindingsCount);

    // if there are fewer sets bound than were declared in the pipeline layout, only process the
    // bound sets (as otherwise we'd fail to copy from them). Assume the application knew what it
    // was doing and the other sets are statically unused.
    setLayouts.resize(RDCMIN(pipe.descSets.size(), descSetLayoutIds.size()));

    // need at least one set, if the shader isn't using any we'll just make our own
    if(setLayouts.empty())
      setLayouts.resize(1);

    for(size_t i = 0; i < setLayouts.size(); i++)
    {
      bool hasImmutableSamplers = false;

      // except for the first layout we need to start from scratch
      if(i > 0)
        bindings.clear();

      // if the shader had no descriptor sets at all, i will be invalid, so just skip and add a set
      // with only our own bindings.
      if(i < descSetLayoutIds.size())
      {
        const DescSetLayout &origLayout = creationInfo.m_DescSetLayout[descSetLayoutIds[i]];

        for(size_t b = 0; b < origLayout.bindings.size(); b++)
        {
          const DescSetLayout::Binding &bind = origLayout.bindings[b];

          // skip empty bindings
          if(bind.descriptorCount == 0 || bind.stageFlags == 0)
            continue;

          // make room in the pool
          poolSizes[bind.descriptorType].descriptorCount += bind.descriptorCount;

          VkDescriptorSetLayoutBinding newBind;
          // offset the binding. We offset all sets to make it easier for patching - don't need to
          // conditionally patch shader bindings depending on which set they're in.
          newBind.binding = uint32_t(b + newBindingsCount);
          newBind.descriptorCount = bind.descriptorCount;
          newBind.descriptorType = bind.descriptorType;

          // we only need it available for compute, just make all bindings visible otherwise dynamic
          // buffer offsets could be indexed wrongly. Consider the case where we have binding 0 as a
          // fragment UBO, and binding 1 as a vertex UBO. Then there are two dynamic offsets, and
          // the second is the one we want to use with ours. If we only add the compute visibility
          // bit to the second UBO, then suddenly it's the *first* offset that we must provide.
          // Instead of trying to remap offsets to match, we simply make every binding compute
          // visible so the ordering is still the same. Since compute and graphics are disjoint this
          // is safe.
          if(patchedBindingStage)
            newBind.stageFlags = patchedBindingStage;
          else
            newBind.stageFlags = bind.stageFlags;

          if(bind.immutableSampler)
          {
            hasImmutableSamplers = true;
            VkSampler *samplers = new VkSampler[bind.descriptorCount];
            newBind.pImmutableSamplers = samplers;
            for(uint32_t s = 0; s < bind.descriptorCount; s++)
              samplers[s] =
                  GetResourceManager()->GetCurrentHandle<VkSampler>(bind.immutableSampler[s]);
          }
          else
          {
            newBind.pImmutableSamplers = NULL;
          }

          bindings.push_back(newBind);
        }
      }

      VkDescriptorSetLayoutCreateInfo descsetLayoutInfo = {
          VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
          NULL,
          0,
          (uint32_t)bindings.size(),
          bindings.data(),
      };

      // create new offseted descriptor layout
      vkr = m_pDriver->vkCreateDescriptorSetLayout(dev, &descsetLayoutInfo, NULL, &setLayouts[i]);
      RDCASSERTEQUAL(vkr, VK_SUCCESS);

      if(hasImmutableSamplers)
      {
        for(const VkDescriptorSetLayoutBinding &bind : bindings)
          delete[] bind.pImmutableSamplers;
      }
    }

    VkDescriptorPoolCreateInfo poolCreateInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    // 1 set for each layout
    poolCreateInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    poolCreateInfo.maxSets = (uint32_t)setLayouts.size();
    poolCreateInfo.poolSizeCount = ARRAY_COUNT(poolSizes);
    poolCreateInfo.pPoolSizes = poolSizes;

    // create descriptor pool with enough space for our descriptors
    vkr = m_pDriver->vkCreateDescriptorPool(dev, &poolCreateInfo, NULL, &descpool);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    // allocate all the descriptors
    VkDescriptorSetAllocateInfo descSetAllocInfo = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        NULL,
        descpool,
        (uint32_t)setLayouts.size(),
        setLayouts.data(),
    };

    descSets.resize(setLayouts.size());
    m_pDriver->vkAllocateDescriptorSets(dev, &descSetAllocInfo, descSets.data());

    // copy the data across from the real descriptors into our adjusted bindings
    for(size_t i = 0; i < descSetLayoutIds.size(); i++)
    {
      const DescSetLayout &origLayout = creationInfo.m_DescSetLayout[descSetLayoutIds[i]];

      if(i >= pipe.descSets.size())
        continue;

      if(pipe.descSets[i].descSet == ResourceId())
        continue;

      WrappedVulkan::DescriptorSetInfo &setInfo =
          m_pDriver->m_DescriptorSetState[pipe.descSets[i].descSet];

      {
        // push descriptors don't have a source to copy from, we need to add writes
        VkWriteDescriptorSet write = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        write.dstSet = descSets[i];

        for(size_t b = 0; b < origLayout.bindings.size(); b++)
        {
          const DescSetLayout::Binding &bind = origLayout.bindings[b];

          // skip empty bindings
          if(bind.descriptorCount == 0 || bind.stageFlags == 0)
            continue;

          DescriptorSetBindingElement *slot = setInfo.currentBindings[b];

          write.dstBinding = uint32_t(b + newBindingsCount);
          write.dstArrayElement = 0;
          write.descriptorCount = bind.descriptorCount;
          write.descriptorType = bind.descriptorType;

          switch(write.descriptorType)
          {
            case VK_DESCRIPTOR_TYPE_SAMPLER:
            case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
            case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
            case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
            case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
            {
              VkDescriptorImageInfo *out = new VkDescriptorImageInfo[write.descriptorCount];
              for(uint32_t w = 0; w < write.descriptorCount; w++)
                out[w] = slot[w].imageInfo;
              write.pImageInfo = out;
              allocImgWrites.push_back(out);
              break;
            }
            case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
            case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
            {
              VkBufferView *out = new VkBufferView[write.descriptorCount];
              for(uint32_t w = 0; w < write.descriptorCount; w++)
                out[w] = slot[w].texelBufferView;
              write.pTexelBufferView = out;
              allocBufViewWrites.push_back(out);
              break;
            }
            case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
            case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
            case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
            case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
            {
              VkDescriptorBufferInfo *out = new VkDescriptorBufferInfo[write.descriptorCount];
              for(uint32_t w = 0; w < write.descriptorCount; w++)
                out[w] = slot[w].bufferInfo;
              write.pBufferInfo = out;
              allocBufWrites.push_back(out);
              break;
            }
            default: RDCERR("Unexpected descriptor type %d", write.descriptorType);
          }

          // start with no descriptors
          write.descriptorCount = 0;

          for(uint32_t w = 0; w < bind.descriptorCount; w++)
          {
            // if this write is valid, we increment the descriptor count and continue
            if(IsValid(write, w - write.dstArrayElement))
            {
              write.descriptorCount++;
            }
            else
            {
              // if this write isn't valid, then we first check to see if we had any previous
              // pending writes in the array we were going to batch together, if so we add them.
              if(write.descriptorCount > 0)
                descWrites.push_back(write);

              // skip past any previous descriptors we just wrote, as well as the current invalid
              // one
              if(write.pBufferInfo)
                write.pBufferInfo += write.descriptorCount + 1;
              if(write.pImageInfo)
                write.pImageInfo += write.descriptorCount + 1;
              if(write.pTexelBufferView)
                write.pTexelBufferView += write.descriptorCount + 1;

              // now start again from 0 descriptors, at the next array element
              write.dstArrayElement += write.descriptorCount + 1;
              write.descriptorCount = 0;
            }
          }

          // if there are any left, add them here
          if(write.descriptorCount > 0)
            descWrites.push_back(write);

          // don't leak the arrays and cause double deletes, NULL them after each time
          write.pImageInfo = NULL;
          write.pBufferInfo = NULL;
          write.pTexelBufferView = NULL;
        }
      }
    }

    m_pDriver->vkUpdateDescriptorSets(dev, (uint32_t)descWrites.size(), descWrites.data(), 0, NULL);

    // delete allocated arrays for descriptor writes
    for(VkDescriptorBufferInfo *a : allocBufWrites)
      delete[] a;
    for(VkDescriptorImageInfo *a : allocImgWrites)
      delete[] a;
    for(VkBufferView *a : allocBufViewWrites)
      delete[] a;
  }
}

void VulkanReplay::FetchVSOut(uint32_t eventId)
{
  const VulkanRenderState &state = m_pDriver->m_RenderState;
  VulkanCreationInfo &creationInfo = m_pDriver->m_CreationInfo;

  const VulkanCreationInfo::Pipeline &pipeInfo = creationInfo.m_Pipeline[state.graphics.pipeline];

  const DrawcallDescription *drawcall = m_pDriver->GetDrawcall(eventId);

  const VulkanCreationInfo::ShaderModule &moduleInfo =
      creationInfo.m_ShaderModule[pipeInfo.shaders[0].module];

  ShaderReflection *refl = pipeInfo.shaders[0].refl;

  // set defaults so that we don't try to fetch this output again if something goes wrong and the
  // same event is selected again
  {
    m_PostVS.Data[eventId].vsin.topo = pipeInfo.topology;
    m_PostVS.Data[eventId].vsout.buf = VK_NULL_HANDLE;
    m_PostVS.Data[eventId].vsout.bufmem = VK_NULL_HANDLE;
    m_PostVS.Data[eventId].vsout.instStride = 0;
    m_PostVS.Data[eventId].vsout.vertStride = 0;
    m_PostVS.Data[eventId].vsout.numViews = 1;
    m_PostVS.Data[eventId].vsout.nearPlane = 0.0f;
    m_PostVS.Data[eventId].vsout.farPlane = 0.0f;
    m_PostVS.Data[eventId].vsout.useIndices = false;
    m_PostVS.Data[eventId].vsout.hasPosOut = false;
    m_PostVS.Data[eventId].vsout.idxbuf = VK_NULL_HANDLE;
    m_PostVS.Data[eventId].vsout.idxbufmem = VK_NULL_HANDLE;

    m_PostVS.Data[eventId].vsout.topo = pipeInfo.topology;
  }

  // no outputs from this shader? unexpected but theoretically possible (dummy VS before
  // tessellation maybe). Just fill out an empty data set
  if(refl->outputSignature.empty())
    return;

  // we go through the driver for all these creations since they need to be properly
  // registered in order to be put in the partial replay state
  VkResult vkr = VK_SUCCESS;
  VkDevice dev = m_Device;

  VkDescriptorPool descpool;
  std::vector<VkDescriptorSetLayout> setLayouts;
  std::vector<VkDescriptorSet> descSets;

  VkPipelineLayout pipeLayout;

  VkGraphicsPipelineCreateInfo pipeCreateInfo;

  // get pipeline create info
  m_pDriver->GetShaderCache()->MakeGraphicsPipelineInfo(pipeCreateInfo, state.graphics.pipeline);

  VkDescriptorSetLayoutBinding newBindings[] = {
      // output buffer
      {
          0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, NULL,
      },    // index buffer (if needed)
      {
          1, VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, NULL,
      },    // vertex buffers (float type)
      {
          2, VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, MeshOutputTBufferArraySize,
          VK_SHADER_STAGE_COMPUTE_BIT, NULL,
      },    // vertex buffers (uint32_t type)
      {
          3, VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, MeshOutputTBufferArraySize,
          VK_SHADER_STAGE_COMPUTE_BIT, NULL,
      },    // vertex buffers (int32_t type)
      {
          4, VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, MeshOutputTBufferArraySize,
          VK_SHADER_STAGE_COMPUTE_BIT, NULL,
      },
  };
  RDCCOMPILE_ASSERT(ARRAY_COUNT(newBindings) == MeshOutputReservedBindings,
                    "MeshOutputReservedBindings is wrong");

  // create a duplicate set of descriptor sets, all visible to compute, with bindings shifted to
  // account for new ones we need. This also copies the existing bindings into the new sets
  PatchReservedDescriptors(m_pDriver->m_RenderState.graphics, descpool, setLayouts, descSets,
                           VK_SHADER_STAGE_COMPUTE_BIT, newBindings, ARRAY_COUNT(newBindings));

  // create pipeline layout with new descriptor set layouts
  {
    std::vector<VkPushConstantRange> push = creationInfo.m_PipelineLayout[pipeInfo.layout].pushRanges;

    // ensure the push range is visible to the compute shader
    for(VkPushConstantRange &range : push)
      range.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkPipelineLayoutCreateInfo pipeLayoutInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        NULL,
        0,
        (uint32_t)setLayouts.size(),
        setLayouts.data(),
        (uint32_t)push.size(),
        push.data(),
    };

    vkr = m_pDriver->vkCreatePipelineLayout(dev, &pipeLayoutInfo, NULL, &pipeLayout);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);
  }

  VkBuffer meshBuffer = VK_NULL_HANDLE, readbackBuffer = VK_NULL_HANDLE;
  VkDeviceMemory meshMem = VK_NULL_HANDLE, readbackMem = VK_NULL_HANDLE;

  VkBuffer uniqIdxBuf = VK_NULL_HANDLE;
  VkDeviceMemory uniqIdxBufMem = VK_NULL_HANDLE;
  VkBufferView uniqIdxBufView = VK_NULL_HANDLE;

  VkBuffer rebasedIdxBuf = VK_NULL_HANDLE;
  VkDeviceMemory rebasedIdxBufMem = VK_NULL_HANDLE;

  uint32_t numVerts = drawcall->numIndices;
  VkDeviceSize bufSize = 0;

  uint32_t numViews = 1;

  {
    const VulkanCreationInfo::RenderPass &rp = creationInfo.m_RenderPass[state.renderPass];

    if(state.subpass < rp.subpasses.size())
    {
      numViews = RDCMAX(numViews, (uint32_t)rp.subpasses[state.subpass].multiviews.size());
    }
    else
    {
      RDCERR("Subpass is out of bounds to renderpass creation info");
    }
  }

  uint32_t idxsize = state.ibuffer.bytewidth;

  uint32_t maxIndex = RDCMAX(drawcall->baseVertex, 0) + numVerts - 1;

  uint32_t maxInstance = drawcall->instanceOffset + drawcall->numInstances - 1;

  if(drawcall->flags & DrawFlags::Indexed)
  {
    bool index16 = (idxsize == 2);
    bytebuf idxdata;
    std::vector<uint32_t> indices;
    uint16_t *idx16 = NULL;
    uint32_t *idx32 = NULL;

    // fetch ibuffer
    if(state.ibuffer.buf != ResourceId())
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
    uint32_t numIndices =
        RDCMIN(uint32_t(index16 ? idxdata.size() / 2 : idxdata.size() / 4), drawcall->numIndices);

    uint32_t idxclamp = 0;
    if(drawcall->baseVertex < 0)
      idxclamp = uint32_t(-drawcall->baseVertex);

    // grab all unique vertex indices referenced
    for(uint32_t i = 0; i < numIndices; i++)
    {
      uint32_t i32 = index16 ? uint32_t(idx16[i]) : idx32[i];

      // apply baseVertex but clamp to 0 (don't allow index to become negative)
      if(i32 < idxclamp)
        i32 = 0;
      else if(drawcall->baseVertex < 0)
        i32 -= idxclamp;
      else if(drawcall->baseVertex > 0)
        i32 += drawcall->baseVertex;

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

    maxIndex = indices.back();

    // set numVerts
    numVerts = (uint32_t)indices.size();

    // An index buffer could be something like: 500, 501, 502, 501, 503, 502
    // in which case we can't use the existing index buffer without filling 499 slots of vertex
    // data with padding. Instead we rebase the indices based on the smallest vertex so it becomes
    // 0, 1, 2, 1, 3, 2 and then that matches our stream-out'd buffer.
    //
    // Note that there could also be gaps, like: 500, 501, 502, 510, 511, 512
    // which would become 0, 1, 2, 3, 4, 5 and so the old index buffer would no longer be valid.
    // We just stream-out a tightly packed list of unique indices, and then remap the index buffer
    // so that what did point to 500 points to 0 (accounting for rebasing), and what did point
    // to 510 now points to 3 (accounting for the unique sort).

    // we use a map here since the indices may be sparse. Especially considering if an index
    // is 'invalid' like 0xcccccccc then we don't want an array of 3.4 billion entries.
    std::map<uint32_t, size_t> indexRemap;
    for(size_t i = 0; i < indices.size(); i++)
    {
      // by definition, this index will only appear once in indices[]
      indexRemap[indices[i]] = i;
    }

    // create buffer with unique 0-based indices
    VkBufferCreateInfo bufInfo = {
        VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        NULL,
        0,
        indices.size() * sizeof(uint32_t),
        VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
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

    if(vkr == VK_ERROR_OUT_OF_DEVICE_MEMORY || vkr == VK_ERROR_OUT_OF_HOST_MEMORY)
    {
      RDCWARN("Failed to allocate %llu bytes for unique index buffer", mrq.size);
      return;
    }

    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    vkr = m_pDriver->vkBindBufferMemory(dev, uniqIdxBuf, uniqIdxBufMem, 0);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    VkBufferViewCreateInfo viewInfo = {
        VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO,
        NULL,
        0,
        uniqIdxBuf,
        VK_FORMAT_R32_UINT,
        0,
        VK_WHOLE_SIZE,
    };

    vkr = m_pDriver->vkCreateBufferView(dev, &viewInfo, NULL, &uniqIdxBufView);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    byte *idxData = NULL;
    vkr = m_pDriver->vkMapMemory(m_Device, uniqIdxBufMem, 0, VK_WHOLE_SIZE, 0, (void **)&idxData);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    memcpy(idxData, &indices[0], indices.size() * sizeof(uint32_t));

    VkMappedMemoryRange range = {
        VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE, NULL, uniqIdxBufMem, 0, VK_WHOLE_SIZE,
    };

    vkr = m_pDriver->vkFlushMappedMemoryRanges(m_Device, 1, &range);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    m_pDriver->vkUnmapMemory(m_Device, uniqIdxBufMem);

    // rebase existing index buffer to point to the right elements in our stream-out'd
    // vertex buffer
    for(uint32_t i = 0; i < numIndices; i++)
    {
      uint32_t i32 = index16 ? uint32_t(idx16[i]) : idx32[i];

      // preserve primitive restart indices
      if(i32 == (index16 ? 0xffff : 0xffffffff))
        continue;

      // apply baseVertex but clamp to 0 (don't allow index to become negative)
      if(i32 < idxclamp)
        i32 = 0;
      else if(drawcall->baseVertex < 0)
        i32 -= idxclamp;
      else if(drawcall->baseVertex > 0)
        i32 += drawcall->baseVertex;

      if(index16)
        idx16[i] = uint16_t(indexRemap[i32]);
      else
        idx32[i] = uint32_t(indexRemap[i32]);
    }

    bufInfo.size = RDCMAX((VkDeviceSize)64, (VkDeviceSize)idxdata.size());
    bufInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

    vkr = m_pDriver->vkCreateBuffer(dev, &bufInfo, NULL, &rebasedIdxBuf);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    m_pDriver->vkGetBufferMemoryRequirements(dev, rebasedIdxBuf, &mrq);

    allocInfo.allocationSize = mrq.size;
    allocInfo.memoryTypeIndex = m_pDriver->GetUploadMemoryIndex(mrq.memoryTypeBits);

    vkr = m_pDriver->vkAllocateMemory(dev, &allocInfo, NULL, &rebasedIdxBufMem);

    if(vkr == VK_ERROR_OUT_OF_DEVICE_MEMORY || vkr == VK_ERROR_OUT_OF_HOST_MEMORY)
    {
      RDCWARN("Failed to allocate %llu bytes for rebased index buffer", mrq.size);
      return;
    }

    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    vkr = m_pDriver->vkBindBufferMemory(dev, rebasedIdxBuf, rebasedIdxBufMem, 0);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    vkr = m_pDriver->vkMapMemory(m_Device, rebasedIdxBufMem, 0, VK_WHOLE_SIZE, 0, (void **)&idxData);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    memcpy(idxData, idxdata.data(), idxdata.size());

    VkMappedMemoryRange rebasedRange = {
        VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE, NULL, rebasedIdxBufMem, 0, VK_WHOLE_SIZE,
    };

    vkr = m_pDriver->vkFlushMappedMemoryRanges(m_Device, 1, &rebasedRange);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    m_pDriver->vkUnmapMemory(m_Device, rebasedIdxBufMem);
  }

  uint32_t bufStride = 0;
  std::vector<uint32_t> modSpirv = moduleInfo.spirv.spirv;

  struct CompactedAttrBuffer
  {
    VkDeviceMemory mem;
    VkBuffer buf;
    VkBufferView view;
  };

  std::vector<uint32_t> attrInstDivisor;
  CompactedAttrBuffer vbuffers[64];
  RDCEraseEl(vbuffers);

  {
    VkWriteDescriptorSet descWrites[64];
    uint32_t numWrites = 0;

    RDCEraseEl(descWrites);

    const VkPipelineVertexInputStateCreateInfo *vi = pipeCreateInfo.pVertexInputState;

    RDCASSERT(vi->vertexAttributeDescriptionCount <= MeshOutputTBufferArraySize);

    // we fetch the vertex buffer data up front here since there's a very high chance of either
    // overlap due to interleaved attributes, or no overlap and no wastage due to separate compact
    // attributes.
    std::vector<bytebuf> origVBs;
    origVBs.reserve(16);

    for(uint32_t vb = 0; vb < vi->vertexBindingDescriptionCount; vb++)
    {
      uint32_t binding = vi->pVertexBindingDescriptions[vb].binding;
      if(binding >= state.vbuffers.size())
      {
        origVBs.push_back(bytebuf());
        continue;
      }

      VkDeviceSize offs = state.vbuffers[binding].offs;
      uint64_t len = 0;

      if(vi->pVertexBindingDescriptions[vb].inputRate == VK_VERTEX_INPUT_RATE_INSTANCE)
      {
        len = uint64_t(maxInstance + 1) * vi->pVertexBindingDescriptions[vb].stride;

        offs += drawcall->instanceOffset * vi->pVertexBindingDescriptions[vb].stride;
      }
      else
      {
        len = uint64_t(maxIndex + 1) * vi->pVertexBindingDescriptions[vb].stride;

        offs += drawcall->vertexOffset * vi->pVertexBindingDescriptions[vb].stride;
      }

      if(state.vbuffers[binding].buf != ResourceId())
      {
        origVBs.push_back(bytebuf());
        GetBufferData(state.vbuffers[binding].buf, offs, len, origVBs.back());
      }
    }

    for(uint32_t i = 0; i < vi->vertexAttributeDescriptionCount; i++)
    {
      const VkVertexInputAttributeDescription &attrDesc = vi->pVertexAttributeDescriptions[i];
      uint32_t attr = attrDesc.location;

      RDCASSERT(attr < 64);
      if(attr >= ARRAY_COUNT(vbuffers))
      {
        RDCERR("Attribute index too high! Resize array.");
        continue;
      }

      uint32_t instDivisor = ~0U;
      size_t stride = 1;

      const byte *origVBBegin = NULL;
      const byte *origVBEnd = NULL;

      for(uint32_t vb = 0; vb < vi->vertexBindingDescriptionCount; vb++)
      {
        const VkVertexInputBindingDescription &vbDesc = vi->pVertexBindingDescriptions[vb];
        if(vbDesc.binding == attrDesc.binding)
        {
          origVBBegin = origVBs[vb].data() + attrDesc.offset;
          origVBEnd = origVBs[vb].data() + origVBs[vb].size();
          stride = vbDesc.stride;
          if(vbDesc.inputRate == VK_VERTEX_INPUT_RATE_INSTANCE)
            instDivisor = pipeInfo.vertexBindings[vbDesc.binding].instanceDivisor;
          else
            instDivisor = ~0U;
          break;
        }
      }

      RDCASSERT(origVBEnd);

      // in some limited cases, provided we added the UNIFORM_TEXEL_BUFFER usage bit, we could use
      // the original buffers here as-is and read out of them. However it is likely that the offset
      // is not a multiple of the minimum texel buffer offset for at least some of the buffers if
      // not all of them, so we simplify the code here by *always* reading back the vertex buffer
      // data and uploading a compacted version.

      // we also need to handle the case where the format is not natively supported as a texel
      // buffer, which requires us to then pick a supported format that's wider (so contains the
      // same precision) but does support texel buffers, and expand to that.
      VkFormat origFormat = attrDesc.format;
      VkFormat expandedFormat = attrDesc.format;

      if((m_pDriver->GetFormatProperties(attrDesc.format).bufferFeatures &
          VK_FORMAT_FEATURE_UNIFORM_TEXEL_BUFFER_BIT) == 0)
      {
        // Our selection is simple. For integer formats, the 4-component version is spec-required to
        // be supported, so we can expand to that and just pad/upcast the data directly.
        // Likewise for float formats, the 4-component 32-bit float version is required to be
        // supported, and can represent any other float format (e.g. R16_SNORM can't be represented
        // by R16_SFLOAT but can be represented by R32_SFLOAT. Same for R16_*SCALED. Fortunately
        // there is no R32_SNORM or R32_*SCALED).
        // So we pick one of three formats depending on the base type of the original format.
        //
        // Note: This does not handle double format inputs, which must have special handling.

        if(IsDoubleFormat(origFormat))
          expandedFormat = VK_FORMAT_R32G32B32A32_UINT;
        else if(IsUIntFormat(origFormat))
          expandedFormat = VK_FORMAT_R32G32B32A32_UINT;
        else if(IsSIntFormat(origFormat))
          expandedFormat = VK_FORMAT_R32G32B32A32_SINT;
        else
          expandedFormat = VK_FORMAT_R32G32B32A32_SFLOAT;
      }

      uint32_t elemSize = GetByteSize(1, 1, 1, expandedFormat, 0);

      // doubles are packed as uvec2
      if(IsDoubleFormat(origFormat))
        elemSize *= 2;

      // used for interpreting the original data, if we're upcasting
      ResourceFormat fmt = MakeResourceFormat(origFormat);

      {
        VkBufferCreateInfo bufInfo = {
            VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            NULL,
            0,
            elemSize * (maxIndex + 1),
            VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        };

        if(instDivisor != ~0U)
          bufInfo.size = elemSize * (maxInstance + 1);

        vkr = m_pDriver->vkCreateBuffer(dev, &bufInfo, NULL, &vbuffers[attr].buf);
        RDCASSERTEQUAL(vkr, VK_SUCCESS);

        VkMemoryRequirements mrq = {0};
        m_pDriver->vkGetBufferMemoryRequirements(dev, vbuffers[attr].buf, &mrq);

        VkMemoryAllocateInfo allocInfo = {
            VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, NULL, mrq.size,
            m_pDriver->GetUploadMemoryIndex(mrq.memoryTypeBits),
        };

        vkr = m_pDriver->vkAllocateMemory(dev, &allocInfo, NULL, &vbuffers[attr].mem);

        if(vkr == VK_ERROR_OUT_OF_DEVICE_MEMORY || vkr == VK_ERROR_OUT_OF_HOST_MEMORY)
        {
          RDCWARN("Failed to allocate %llu bytes for patched vertex buffer", mrq.size);
          return;
        }

        RDCASSERTEQUAL(vkr, VK_SUCCESS);

        vkr = m_pDriver->vkBindBufferMemory(dev, vbuffers[attr].buf, vbuffers[attr].mem, 0);
        RDCASSERTEQUAL(vkr, VK_SUCCESS);

        byte *compactedData = NULL;
        vkr = m_pDriver->vkMapMemory(m_Device, vbuffers[attr].mem, 0, VK_WHOLE_SIZE, 0,
                                     (void **)&compactedData);
        RDCASSERTEQUAL(vkr, VK_SUCCESS);

        if(compactedData && origVBEnd)
        {
          const byte *src = origVBBegin;
          byte *dst = compactedData;
          const byte *dstEnd = dst + bufInfo.size;

          // fast memcpy compaction case for natively supported texel buffer formats
          if(origFormat == expandedFormat)
          {
            while(src < origVBEnd && dst < dstEnd)
            {
              memcpy(dst, src, elemSize);
              dst += elemSize;
              src += stride;
            }
          }
          else
          {
            uint32_t zero = 0;

            // upcasting path
            if(IsDoubleFormat(origFormat))
            {
              while(src < origVBEnd && dst < dstEnd)
              {
                // the double is already in "packed uvec2" order, with least significant 32-bits
                // first, so we can copy directly
                memcpy(dst, src, sizeof(double) * fmt.compCount);
                dst += sizeof(double) * fmt.compCount;

                // fill up to *8* zeros not 4, since we're filling two for every component
                for(uint8_t c = fmt.compCount * 2; c < 8; c++)
                {
                  memcpy(dst, &zero, sizeof(uint32_t));
                  dst += sizeof(uint32_t);
                }

                src += stride;
              }
            }
            else if(IsUIntFormat(expandedFormat))
            {
              while(src < origVBEnd && dst < dstEnd)
              {
                uint32_t val = 0;

                const byte *s = src;

                uint8_t c = 0;
                for(; c < fmt.compCount; c++)
                {
                  if(fmt.compByteWidth == 1)
                    val = *s;
                  else if(fmt.compByteWidth == 2)
                    val = *(uint16_t *)s;
                  else if(fmt.compByteWidth == 4)
                    val = *(uint32_t *)s;

                  memcpy(dst, &val, sizeof(uint32_t));
                  dst += sizeof(uint32_t);
                  s += fmt.compByteWidth;
                }

                for(; c < 4; c++)
                {
                  memcpy(dst, &zero, sizeof(uint32_t));
                  dst += sizeof(uint32_t);
                }

                src += stride;
              }
            }
            else if(IsSIntFormat(expandedFormat))
            {
              while(src < origVBEnd && dst < dstEnd)
              {
                int32_t val = 0;

                const byte *s = src;

                uint8_t c = 0;
                for(; c < fmt.compCount; c++)
                {
                  if(fmt.compByteWidth == 1)
                    val = *(int8_t *)s;
                  else if(fmt.compByteWidth == 2)
                    val = *(int16_t *)s;
                  else if(fmt.compByteWidth == 4)
                    val = *(int32_t *)s;

                  memcpy(dst, &val, sizeof(int32_t));
                  dst += sizeof(int32_t);
                  s += fmt.compByteWidth;
                }

                for(; c < 4; c++)
                {
                  memcpy(dst, &zero, sizeof(uint32_t));
                  dst += sizeof(uint32_t);
                }

                src += stride;
              }
            }
            else
            {
              while(src < origVBEnd && dst < dstEnd)
              {
                bool valid = false;
                FloatVector vec = HighlightCache::InterpretVertex(src, 0, 0, fmt, origVBEnd, valid);

                memcpy(dst, &vec, sizeof(FloatVector));
                dst += sizeof(FloatVector);
                src += stride;
              }
            }
          }
        }

        VkMappedMemoryRange range = {
            VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE, NULL, vbuffers[attr].mem, 0, VK_WHOLE_SIZE,
        };

        vkr = m_pDriver->vkFlushMappedMemoryRanges(m_Device, 1, &range);
        RDCASSERTEQUAL(vkr, VK_SUCCESS);

        m_pDriver->vkUnmapMemory(m_Device, vbuffers[attr].mem);
      }

      VkBufferViewCreateInfo info = {
          VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO,
          NULL,
          0,
          vbuffers[attr].buf,
          expandedFormat,
          0,
          VK_WHOLE_SIZE,
      };

      if((m_pDriver->GetFormatProperties(expandedFormat).bufferFeatures &
          VK_FORMAT_FEATURE_UNIFORM_TEXEL_BUFFER_BIT) == 0)
      {
        RDCERR(
            "Format %s doesn't support texel buffers, and no suitable upcasting format was found! "
            "Replacing with safe but broken format to avoid crashes, but vertex data will be "
            "wrong.",
            ToStr(origFormat).c_str());
        info.format = VK_FORMAT_R8G8B8A8_UNORM;
      }

      m_pDriver->vkCreateBufferView(dev, &info, NULL, &vbuffers[attr].view);

      attrInstDivisor.resize(RDCMAX(attrInstDivisor.size(), size_t(attr + 1)));
      attrInstDivisor[attr] = instDivisor;

      descWrites[numWrites].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      descWrites[numWrites].dstSet = descSets[0];
      if(IsSIntFormat(attrDesc.format))
        descWrites[numWrites].dstBinding = 4;
      else if(IsUIntFormat(attrDesc.format) || IsDoubleFormat(attrDesc.format))
        descWrites[numWrites].dstBinding = 3;
      else
        descWrites[numWrites].dstBinding = 2;
      descWrites[numWrites].dstArrayElement = attr;
      descWrites[numWrites].descriptorCount = 1;
      descWrites[numWrites].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
      descWrites[numWrites].pTexelBufferView = &vbuffers[attr].view;
      numWrites++;
    }

    // add a write of the index buffer
    if(uniqIdxBufView != VK_NULL_HANDLE)
    {
      descWrites[numWrites].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      descWrites[numWrites].dstSet = descSets[0];
      descWrites[numWrites].dstBinding = 1;
      descWrites[numWrites].dstArrayElement = 0;
      descWrites[numWrites].descriptorCount = 1;
      descWrites[numWrites].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
      descWrites[numWrites].pTexelBufferView = &uniqIdxBufView;
      numWrites++;
    }

    m_pDriver->vkUpdateDescriptorSets(dev, numWrites, descWrites, 0, NULL);
  }

  ConvertToMeshOutputCompute(*refl, *pipeInfo.shaders[0].patchData,
                             pipeInfo.shaders[0].entryPoint.c_str(), attrInstDivisor, drawcall,
                             numVerts, numViews, modSpirv, bufStride);

  VkComputePipelineCreateInfo compPipeInfo = {VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};

  // repoint pipeline layout
  compPipeInfo.layout = pipeLayout;

  // create vertex shader with modified code
  VkShaderModuleCreateInfo moduleCreateInfo = {
      VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, NULL,         0,
      modSpirv.size() * sizeof(uint32_t),          &modSpirv[0],
  };

  VkShaderModule module;
  vkr = m_pDriver->vkCreateShaderModule(dev, &moduleCreateInfo, NULL, &module);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  compPipeInfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  compPipeInfo.stage.module = module;
  compPipeInfo.stage.pName = PatchedMeshOutputEntryPoint;
  compPipeInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;

  // copy over specialization info
  for(uint32_t s = 0; s < pipeCreateInfo.stageCount; s++)
  {
    if(pipeCreateInfo.pStages[s].stage == VK_SHADER_STAGE_VERTEX_BIT)
    {
      compPipeInfo.stage.pSpecializationInfo = pipeCreateInfo.pStages[s].pSpecializationInfo;
      break;
    }
  }

  // create new pipeline
  VkPipeline pipe;
  vkr = m_pDriver->vkCreateComputePipelines(m_Device, VK_NULL_HANDLE, 1, &compPipeInfo, NULL, &pipe);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  // make copy of state to draw from
  VulkanRenderState modifiedstate = state;

  // bind created pipeline to partial replay state
  modifiedstate.compute.pipeline = GetResID(pipe);

  // move graphics descriptor sets onto the compute pipe.
  modifiedstate.compute.descSets = modifiedstate.graphics.descSets;

  // replace descriptor set IDs with our temporary sets. The offsets we keep the same. If the
  // original draw had no sets, we ensure there's room (with no offsets needed)
  if(modifiedstate.compute.descSets.empty())
    modifiedstate.compute.descSets.resize(1);

  for(size_t i = 0; i < descSets.size(); i++)
    modifiedstate.compute.descSets[i].descSet = GetResID(descSets[i]);

  {
    // create buffer of sufficient size
    // this can't just be bufStride * num unique indices per instance, as we don't
    // have a compact 0-based index to index into the buffer. We must use
    // index-minIndex which is 0-based but potentially sparse, so this buffer may
    // be more or less wasteful
    VkBufferCreateInfo bufInfo = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};

    // set bufSize
    bufSize = bufInfo.size = uint64_t(numVerts) * uint64_t(drawcall->numInstances) *
                             uint64_t(bufStride) * uint64_t(numViews);

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

    if(vkr == VK_ERROR_OUT_OF_DEVICE_MEMORY || vkr == VK_ERROR_OUT_OF_HOST_MEMORY)
    {
      RDCWARN("Failed to allocate %llu bytes for output vertex SSBO", mrq.size);
      return;
    }

    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    vkr = m_pDriver->vkBindBufferMemory(dev, meshBuffer, meshMem, 0);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    m_pDriver->vkGetBufferMemoryRequirements(dev, readbackBuffer, &mrq);

    allocInfo.memoryTypeIndex = m_pDriver->GetReadbackMemoryIndex(mrq.memoryTypeBits);

    vkr = m_pDriver->vkAllocateMemory(dev, &allocInfo, NULL, &readbackMem);

    if(vkr == VK_ERROR_OUT_OF_DEVICE_MEMORY || vkr == VK_ERROR_OUT_OF_HOST_MEMORY)
    {
      RDCWARN("Failed to allocate %llu bytes for readback memory", mrq.size);
      return;
    }

    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    vkr = m_pDriver->vkBindBufferMemory(dev, readbackBuffer, readbackMem, 0);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    VkCommandBuffer cmd = m_pDriver->GetNextCmd();

    VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL,
                                          VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};

    vkr = ObjDisp(dev)->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    // fill destination buffer with 0s to ensure unwritten vertices have sane data
    ObjDisp(dev)->CmdFillBuffer(Unwrap(cmd), Unwrap(meshBuffer), 0, bufInfo.size, 0);

    VkBufferMemoryBarrier meshbufbarrier = {
        VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
        NULL,
        VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_HOST_WRITE_BIT,
        VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
        VK_QUEUE_FAMILY_IGNORED,
        VK_QUEUE_FAMILY_IGNORED,
    };

    meshbufbarrier.size = VK_WHOLE_SIZE;

    VkMemoryBarrier globalbarrier = {
        VK_STRUCTURE_TYPE_MEMORY_BARRIER, NULL,
        VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_HOST_WRITE_BIT,
        VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
    };

    // wait for uploads of index buffer (if used), compacted vertex buffers, and the above fill to
    // finish.
    DoPipelineBarrier(cmd, 1, &globalbarrier);

    // vkUpdateDescriptorSet desc set to point to buffer
    VkDescriptorBufferInfo fetchdesc = {0};
    fetchdesc.buffer = meshBuffer;
    fetchdesc.offset = 0;
    fetchdesc.range = bufInfo.size;

    VkWriteDescriptorSet write = {
        VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, descSets[0], 0,   0, 1,
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,      NULL, &fetchdesc,  NULL};
    m_pDriver->vkUpdateDescriptorSets(dev, 1, &write, 0, NULL);

    // do single draw
    modifiedstate.BindPipeline(cmd, VulkanRenderState::BindCompute, true);
    uint64_t totalVerts = numVerts * uint64_t(drawcall->numInstances) * uint64_t(numViews);

    // the validation layers will probably complain about this dispatch saying some arrays aren't
    // fully updated. That's because they don't statically analyse that only fixed indices are
    // referred to. It's safe to leave unused array indices as invalid descriptors.
    ObjDisp(cmd)->CmdDispatch(Unwrap(cmd), uint32_t(totalVerts / MeshOutputDispatchWidth) + 1, 1, 1);

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

  for(CompactedAttrBuffer attrBuf : vbuffers)
  {
    m_pDriver->vkDestroyBufferView(dev, attrBuf.view, NULL);
    m_pDriver->vkDestroyBuffer(dev, attrBuf.buf, NULL);
    m_pDriver->vkFreeMemory(dev, attrBuf.mem, NULL);
  }

  // readback mesh data
  byte *byteData = NULL;
  vkr = m_pDriver->vkMapMemory(m_Device, readbackMem, 0, VK_WHOLE_SIZE, 0, (void **)&byteData);

  VkMappedMemoryRange range = {
      VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE, NULL, readbackMem, 0, VK_WHOLE_SIZE,
  };

  vkr = m_pDriver->vkInvalidateMappedMemoryRanges(m_Device, 1, &range);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

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
    m_pDriver->vkDestroyBufferView(m_Device, uniqIdxBufView, NULL);
  }

  // fill out m_PostVS.Data
  m_PostVS.Data[eventId].vsin.topo = pipeCreateInfo.pInputAssemblyState->topology;
  m_PostVS.Data[eventId].vsout.topo = pipeCreateInfo.pInputAssemblyState->topology;
  m_PostVS.Data[eventId].vsout.buf = meshBuffer;
  m_PostVS.Data[eventId].vsout.bufmem = meshMem;

  m_PostVS.Data[eventId].vsout.baseVertex = 0;

  m_PostVS.Data[eventId].vsout.numViews = numViews;

  m_PostVS.Data[eventId].vsout.vertStride = bufStride;
  m_PostVS.Data[eventId].vsout.nearPlane = nearp;
  m_PostVS.Data[eventId].vsout.farPlane = farp;

  m_PostVS.Data[eventId].vsout.useIndices = bool(drawcall->flags & DrawFlags::Indexed);
  m_PostVS.Data[eventId].vsout.numVerts = drawcall->numIndices;

  m_PostVS.Data[eventId].vsout.instStride = 0;
  if(drawcall->flags & DrawFlags::Instanced)
    m_PostVS.Data[eventId].vsout.instStride = uint32_t(bufSize / (drawcall->numInstances * numViews));

  m_PostVS.Data[eventId].vsout.idxbuf = VK_NULL_HANDLE;
  if(m_PostVS.Data[eventId].vsout.useIndices && state.ibuffer.buf != ResourceId())
  {
    m_PostVS.Data[eventId].vsout.idxbuf = rebasedIdxBuf;
    m_PostVS.Data[eventId].vsout.idxbufmem = rebasedIdxBufMem;
    m_PostVS.Data[eventId].vsout.idxFmt = idxsize == 2 ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32;
  }

  m_PostVS.Data[eventId].vsout.hasPosOut =
      refl->outputSignature[0].systemValue == ShaderBuiltin::Position;

  // delete descriptors. Technically we don't have to free the descriptor sets, but our tracking on
  // replay doesn't handle destroying children of pooled objects so we do it explicitly anyway.
  m_pDriver->vkFreeDescriptorSets(dev, descpool, (uint32_t)descSets.size(), descSets.data());

  m_pDriver->vkDestroyDescriptorPool(dev, descpool, NULL);

  for(VkDescriptorSetLayout layout : setLayouts)
    m_pDriver->vkDestroyDescriptorSetLayout(dev, layout, NULL);

  // delete pipeline layout
  m_pDriver->vkDestroyPipelineLayout(dev, pipeLayout, NULL);

  // delete pipeline
  m_pDriver->vkDestroyPipeline(dev, pipe, NULL);

  // delete shader/shader module
  m_pDriver->vkDestroyShaderModule(dev, module, NULL);
}

void VulkanReplay::FetchTessGSOut(uint32_t eventId)
{
  VulkanRenderState state = m_pDriver->m_RenderState;
  VulkanCreationInfo &creationInfo = m_pDriver->m_CreationInfo;

  const VulkanCreationInfo::Pipeline &pipeInfo = creationInfo.m_Pipeline[state.graphics.pipeline];

  const DrawcallDescription *drawcall = m_pDriver->GetDrawcall(eventId);

  // set defaults so that we don't try to fetch this output again if something goes wrong and the
  // same event is selected again
  {
    m_PostVS.Data[eventId].gsout.buf = VK_NULL_HANDLE;
    m_PostVS.Data[eventId].gsout.bufmem = VK_NULL_HANDLE;
    m_PostVS.Data[eventId].gsout.instStride = 0;
    m_PostVS.Data[eventId].gsout.vertStride = 0;
    m_PostVS.Data[eventId].gsout.numViews = 1;
    m_PostVS.Data[eventId].gsout.nearPlane = 0.0f;
    m_PostVS.Data[eventId].gsout.farPlane = 0.0f;
    m_PostVS.Data[eventId].gsout.useIndices = false;
    m_PostVS.Data[eventId].gsout.hasPosOut = false;
    m_PostVS.Data[eventId].gsout.idxbuf = VK_NULL_HANDLE;
    m_PostVS.Data[eventId].gsout.idxbufmem = VK_NULL_HANDLE;
  }

  if(!creationInfo.m_RenderPass[state.renderPass].subpasses[state.subpass].multiviews.empty())
  {
    RDCWARN("Multipass is active for this draw, no GS/Tess mesh output is available");
    return;
  }

  // first try geometry stage
  int stageIndex = 3;

  // if there is no such shader bound, try tessellation
  if(!pipeInfo.shaders[stageIndex].refl)
    stageIndex = 2;

  // if still nothing, do vertex
  if(!pipeInfo.shaders[stageIndex].refl)
    stageIndex = 0;

  ShaderReflection *lastRefl = pipeInfo.shaders[stageIndex].refl;

  RDCASSERT(lastRefl);

  uint32_t primitiveMultiplier = 1;

  // transform feedback expands strips to lists
  switch(pipeInfo.shaders[stageIndex].patchData->outTopo)
  {
    case Topology::PointList:
      m_PostVS.Data[eventId].gsout.topo = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
      break;
    case Topology::LineList:
    case Topology::LineStrip:
      m_PostVS.Data[eventId].gsout.topo = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
      primitiveMultiplier = 2;
      break;
    default:
      RDCERR("Unexpected output topology %s",
             ToStr(pipeInfo.shaders[stageIndex].patchData->outTopo).c_str());
    // deliberate fallthrough
    case Topology::TriangleList:
    case Topology::TriangleStrip:
      m_PostVS.Data[eventId].gsout.topo = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
      primitiveMultiplier = 3;
      break;
  }

  if(lastRefl->outputSignature.empty())
  {
    // empty vertex output signature
    m_PostVS.Data[eventId].gsout.buf = VK_NULL_HANDLE;
    m_PostVS.Data[eventId].gsout.bufmem = VK_NULL_HANDLE;
    m_PostVS.Data[eventId].gsout.instStride = 0;
    m_PostVS.Data[eventId].gsout.vertStride = 0;
    m_PostVS.Data[eventId].gsout.numViews = 1;
    m_PostVS.Data[eventId].gsout.nearPlane = 0.0f;
    m_PostVS.Data[eventId].gsout.farPlane = 0.0f;
    m_PostVS.Data[eventId].gsout.useIndices = false;
    m_PostVS.Data[eventId].gsout.hasPosOut = false;
    m_PostVS.Data[eventId].gsout.idxbuf = VK_NULL_HANDLE;
    m_PostVS.Data[eventId].gsout.idxbufmem = VK_NULL_HANDLE;
    return;
  }

  if(!ObjDisp(m_Device)->CmdBeginTransformFeedbackEXT)
  {
    RDCLOG(
        "VK_EXT_transform_feedback extension not available, can't fetch tessellation/geometry "
        "output");
    return;
  }

  const VulkanCreationInfo::ShaderModule &moduleInfo =
      creationInfo.m_ShaderModule[pipeInfo.shaders[stageIndex].module];

  std::vector<uint32_t> modSpirv = moduleInfo.spirv.spirv;

  uint32_t xfbStride = 0;

  // adds XFB annotations in order of the output signature (with the position first)
  AddXFBAnnotations(*lastRefl, *pipeInfo.shaders[stageIndex].patchData,
                    pipeInfo.shaders[stageIndex].entryPoint.c_str(), modSpirv, xfbStride);

  // create vertex shader with modified code
  VkShaderModuleCreateInfo moduleCreateInfo = {
      VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, NULL,         0,
      modSpirv.size() * sizeof(uint32_t),          &modSpirv[0],
  };

  VkResult vkr = VK_SUCCESS;
  VkDevice dev = m_Device;

  VkShaderModule module;
  vkr = m_pDriver->vkCreateShaderModule(dev, &moduleCreateInfo, NULL, &module);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  VkGraphicsPipelineCreateInfo pipeCreateInfo;

  // get pipeline create info
  m_pDriver->GetShaderCache()->MakeGraphicsPipelineInfo(pipeCreateInfo, state.graphics.pipeline);

  VkPipelineRasterizationStateCreateInfo *rs =
      (VkPipelineRasterizationStateCreateInfo *)pipeCreateInfo.pRasterizationState;
  rs->rasterizerDiscardEnable = true;

  for(uint32_t i = 0; i < pipeCreateInfo.stageCount; i++)
  {
    VkPipelineShaderStageCreateInfo &stage =
        (VkPipelineShaderStageCreateInfo &)pipeCreateInfo.pStages[i];

    if(StageIndex(stage.stage) == stageIndex)
    {
      stage.module = module;
      break;
    }
  }

  // create a empty renderpass and framebuffer so we can draw
  VkFramebuffer fb = VK_NULL_HANDLE;
  VkRenderPass rp = VK_NULL_HANDLE;

  VkSubpassDescription sub = {0, VK_PIPELINE_BIND_POINT_GRAPHICS};
  VkRenderPassCreateInfo rpinfo = {
      VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO, NULL, 0, 0, NULL, 1, &sub,
  };

  vkr = m_pDriver->vkCreateRenderPass(m_Device, &rpinfo, NULL, &rp);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  VkFramebufferCreateInfo fbinfo = {
      VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO, NULL, 0, rp, 0, NULL, 16U, 16U, 1,
  };

  vkr = m_pDriver->vkCreateFramebuffer(m_Device, &fbinfo, NULL, &fb);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  pipeCreateInfo.renderPass = rp;
  pipeCreateInfo.subpass = 0;

  VkPipeline pipe = VK_NULL_HANDLE;
  vkr = m_pDriver->vkCreateGraphicsPipelines(m_Device, VK_NULL_HANDLE, 1, &pipeCreateInfo, NULL,
                                             &pipe);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  state.graphics.pipeline = GetResID(pipe);
  state.framebuffer = GetResID(fb);
  state.renderPass = GetResID(rp);
  state.subpass = 0;
  state.renderArea.offset.x = 0;
  state.renderArea.offset.y = 0;
  state.renderArea.extent.width = 16;
  state.renderArea.extent.height = 16;

  // disable any existing XFB
  state.xfbbuffers.clear();
  state.xfbcounters.clear();

  if(m_PostVS.XFBQueryPoolSize < drawcall->numInstances)
  {
    if(m_PostVS.XFBQueryPoolSize != VK_NULL_HANDLE)
      m_pDriver->vkDestroyQueryPool(m_Device, m_PostVS.XFBQueryPool, NULL);

    VkQueryPoolCreateInfo info = {
        VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
        NULL,
        0,
        VK_QUERY_TYPE_TRANSFORM_FEEDBACK_STREAM_EXT,
        drawcall->numInstances,
        0,
    };

    vkr = m_pDriver->vkCreateQueryPool(m_Device, &info, NULL, &m_PostVS.XFBQueryPool);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    m_PostVS.XFBQueryPoolSize = drawcall->numInstances;
  }

  VkBuffer meshBuffer = VK_NULL_HANDLE;
  VkDeviceMemory meshMem = VK_NULL_HANDLE;

  // start with bare minimum size, which might be enough if no expansion happens
  VkDeviceSize bufferSize = 0;
  VkDeviceSize dataSize =
      uint64_t(drawcall->numIndices) * uint64_t(drawcall->numInstances) * uint64_t(xfbStride);

  VkXfbQueryResult queryResult = {};

  while(bufferSize < dataSize)
  {
    bufferSize = dataSize;

    if(meshBuffer != VK_NULL_HANDLE)
    {
      m_pDriver->vkDestroyBuffer(dev, meshBuffer, NULL);
      m_pDriver->vkFreeMemory(dev, meshMem, NULL);

      meshBuffer = VK_NULL_HANDLE;
      meshMem = VK_NULL_HANDLE;
    }

    VkBufferCreateInfo bufInfo = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};

    bufInfo.size = bufferSize;

    bufInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bufInfo.usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    bufInfo.usage |= VK_BUFFER_USAGE_TRANSFORM_FEEDBACK_BUFFER_BIT_EXT;
    bufInfo.usage |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;

    vkr = m_pDriver->vkCreateBuffer(dev, &bufInfo, NULL, &meshBuffer);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    VkMemoryRequirements mrq = {0};
    m_pDriver->vkGetBufferMemoryRequirements(dev, meshBuffer, &mrq);

    VkMemoryAllocateInfo allocInfo = {
        VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, NULL, mrq.size,
        m_pDriver->GetGPULocalMemoryIndex(mrq.memoryTypeBits),
    };

    vkr = m_pDriver->vkAllocateMemory(dev, &allocInfo, NULL, &meshMem);

    if(vkr == VK_ERROR_OUT_OF_DEVICE_MEMORY || vkr == VK_ERROR_OUT_OF_HOST_MEMORY)
    {
      RDCWARN("Output allocation for %llu bytes failed fetching tessellation/geometry output.",
              mrq.size);

      m_pDriver->vkDestroyBuffer(dev, meshBuffer, NULL);

      // delete framebuffer and renderpass
      m_pDriver->vkDestroyFramebuffer(dev, fb, NULL);
      m_pDriver->vkDestroyRenderPass(dev, rp, NULL);

      // delete pipeline
      m_pDriver->vkDestroyPipeline(dev, pipe, NULL);

      // delete shader/shader module
      m_pDriver->vkDestroyShaderModule(dev, module, NULL);
      return;
    }

    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    vkr = m_pDriver->vkBindBufferMemory(dev, meshBuffer, meshMem, 0);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    VkCommandBuffer cmd = m_pDriver->GetNextCmd();

    VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL,
                                          VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};

    vkr = ObjDisp(dev)->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    ObjDisp(dev)->CmdResetQueryPool(Unwrap(cmd), Unwrap(m_PostVS.XFBQueryPool), 0, 1);

    // fill destination buffer with 0s to ensure unwritten vertices have sane data
    ObjDisp(dev)->CmdFillBuffer(Unwrap(cmd), Unwrap(meshBuffer), 0, bufInfo.size, 0);

    VkBufferMemoryBarrier meshbufbarrier = {
        VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
        NULL,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_ACCESS_TRANSFORM_FEEDBACK_WRITE_BIT_EXT,
        VK_QUEUE_FAMILY_IGNORED,
        VK_QUEUE_FAMILY_IGNORED,
        Unwrap(meshBuffer),
        0,
        bufInfo.size,
    };

    // wait for the above fill to finish.
    DoPipelineBarrier(cmd, 1, &meshbufbarrier);

    state.BeginRenderPassAndApplyState(cmd, VulkanRenderState::BindGraphics);

    ObjDisp(cmd)->CmdBeginQuery(Unwrap(cmd), Unwrap(m_PostVS.XFBQueryPool), 0, 0);

    ObjDisp(cmd)->CmdBindTransformFeedbackBuffersEXT(Unwrap(cmd), 0, 1, UnwrapPtr(meshBuffer),
                                                     &meshbufbarrier.offset, &meshbufbarrier.size);

    ObjDisp(cmd)->CmdBeginTransformFeedbackEXT(Unwrap(cmd), 0, 1, NULL, NULL);

    if(drawcall->flags & DrawFlags::Indexed)
    {
      ObjDisp(cmd)->CmdDrawIndexed(Unwrap(cmd), drawcall->numIndices, drawcall->numInstances,
                                   drawcall->indexOffset, drawcall->baseVertex,
                                   drawcall->instanceOffset);
    }
    else
    {
      ObjDisp(cmd)->CmdDraw(Unwrap(cmd), drawcall->numIndices, drawcall->numInstances,
                            drawcall->vertexOffset, drawcall->instanceOffset);
    }

    ObjDisp(cmd)->CmdEndTransformFeedbackEXT(Unwrap(cmd), 0, 1, NULL, NULL);

    ObjDisp(cmd)->CmdEndQuery(Unwrap(cmd), Unwrap(m_PostVS.XFBQueryPool), 0);

    state.EndRenderPass(cmd);

    vkr = ObjDisp(dev)->EndCommandBuffer(Unwrap(cmd));
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    m_pDriver->SubmitCmds();
    m_pDriver->FlushQ();

    vkr = ObjDisp(dev)->GetQueryPoolResults(
        Unwrap(dev), Unwrap(m_PostVS.XFBQueryPool), 0, 1, sizeof(VkXfbQueryResult), &queryResult,
        sizeof(VkXfbQueryResult), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    VkDeviceSize generatedSize = queryResult.numPrimitivesGenerated * 3 * xfbStride;

    // output buffer isn't big enough, delete it and re-run so we recreate it larger
    if(generatedSize > dataSize)
      dataSize = generatedSize;
  }

  std::vector<VulkanPostVSData::InstData> instData;

  // instanced draws must be replayed one at a time so we can record the number of primitives from
  // each drawcall, as due to expansion this can vary per-instance.
  if(drawcall->flags & DrawFlags::Instanced && drawcall->numInstances > 1)
  {
    VkCommandBuffer cmd = m_pDriver->GetNextCmd();

    VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL,
                                          VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};

    vkr = ObjDisp(dev)->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    ObjDisp(dev)->CmdResetQueryPool(Unwrap(cmd), Unwrap(m_PostVS.XFBQueryPool), 0,
                                    drawcall->numInstances);

    state.BeginRenderPassAndApplyState(cmd, VulkanRenderState::BindGraphics);

    // do incremental draws to get the output size. We have to do this O(N^2) style because
    // there's no way to replay only a single instance. We have to replay 1, 2, 3, ... N
    // instances and count the total number of verts each time, then we can see from the
    // difference how much each instance wrote.
    for(uint32_t inst = 1; inst <= drawcall->numInstances; inst++)
    {
      ObjDisp(cmd)->CmdBeginQuery(Unwrap(cmd), Unwrap(m_PostVS.XFBQueryPool), inst - 1, 0);

      VkDeviceSize offset = 0;
      ObjDisp(cmd)->CmdBindTransformFeedbackBuffersEXT(Unwrap(cmd), 0, 1, UnwrapPtr(meshBuffer),
                                                       &offset, &bufferSize);

      ObjDisp(cmd)->CmdBeginTransformFeedbackEXT(Unwrap(cmd), 0, 1, NULL, NULL);

      if(drawcall->flags & DrawFlags::Indexed)
      {
        ObjDisp(cmd)->CmdDrawIndexed(Unwrap(cmd), drawcall->numIndices, inst, drawcall->indexOffset,
                                     drawcall->baseVertex, drawcall->instanceOffset);
      }
      else
      {
        ObjDisp(cmd)->CmdDraw(Unwrap(cmd), drawcall->numIndices, inst, drawcall->vertexOffset,
                              drawcall->instanceOffset);
      }

      ObjDisp(cmd)->CmdEndTransformFeedbackEXT(Unwrap(cmd), 0, 1, NULL, NULL);

      ObjDisp(cmd)->CmdEndQuery(Unwrap(cmd), Unwrap(m_PostVS.XFBQueryPool), inst - 1);
    }

    state.EndRenderPass(cmd);

    vkr = ObjDisp(dev)->EndCommandBuffer(Unwrap(cmd));
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    m_pDriver->SubmitCmds();
    m_pDriver->FlushQ();

    std::vector<VkXfbQueryResult> queryResults;
    queryResults.resize(drawcall->numInstances);
    vkr = ObjDisp(dev)->GetQueryPoolResults(
        Unwrap(dev), Unwrap(m_PostVS.XFBQueryPool), 0, drawcall->numInstances,
        sizeof(VkXfbQueryResult) * drawcall->numInstances, queryResults.data(),
        sizeof(VkXfbQueryResult), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    uint64_t prevVertCount = 0;

    for(uint32_t inst = 0; inst < drawcall->numInstances; inst++)
    {
      uint64_t vertCount = queryResults[inst].numPrimitivesWritten * primitiveMultiplier;

      VulkanPostVSData::InstData d;
      d.numVerts = uint32_t(vertCount - prevVertCount);
      d.bufOffset = uint32_t(xfbStride * prevVertCount);
      prevVertCount = vertCount;

      instData.push_back(d);
    }
  }

  float nearp = 0.1f;
  float farp = 100.0f;

  Vec4f pos0;

  bool found = false;

  // we read back the buffer in chunks, since we're likely to find a match in the first few
  // vertices.

  VkDeviceSize readbackoffset = 0;
  const VkDeviceSize readbacksize = 1024 * 1024;

  while(readbackoffset < bufferSize)
  {
    bytebuf data;
    GetBufferData(GetResID(meshBuffer), readbackoffset, readbacksize, data);

    if(data.empty())
      break;

    if(readbackoffset == 0)
      memcpy(&pos0, data.data(), sizeof(pos0));

    for(uint32_t i = 0; i < data.size() / xfbStride; i++)
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

      Vec4f *pos = (Vec4f *)(data.data() + xfbStride * i);

      // skip invalid vertices (w=0)
      if(pos->w != 0.0f && fabs(pos->w - pos0.w) > 0.01f && fabs(pos->z - pos0.z) > 0.01f)
      {
        Vec2f A(pos0.w, pos0.z);
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

    if(found)
      break;

    // read the next segment
    readbackoffset += readbacksize;
  }

  // if we didn't find anything, all z's and w's were identical.
  // If the z is positive and w greater for the first element then
  // we detect this projection as reversed z with infinite far plane
  if(!found && pos0.z > 0.0f && pos0.w > pos0.z)
  {
    nearp = pos0.z;
    farp = FLT_MAX;
  }

  // fill out m_PostVS.Data
  m_PostVS.Data[eventId].gsout.buf = meshBuffer;
  m_PostVS.Data[eventId].gsout.bufmem = meshMem;

  m_PostVS.Data[eventId].gsout.baseVertex = 0;

  m_PostVS.Data[eventId].gsout.numViews = 1;

  m_PostVS.Data[eventId].gsout.vertStride = xfbStride;
  m_PostVS.Data[eventId].gsout.nearPlane = nearp;
  m_PostVS.Data[eventId].gsout.farPlane = farp;

  m_PostVS.Data[eventId].gsout.useIndices = false;

  m_PostVS.Data[eventId].gsout.numVerts =
      uint32_t(queryResult.numPrimitivesWritten) * primitiveMultiplier;

  // set instance stride to 0. If there's any stride needed, it will be calculated using instData
  m_PostVS.Data[eventId].gsout.instStride = 0;
  m_PostVS.Data[eventId].gsout.instData = instData;

  m_PostVS.Data[eventId].gsout.idxbuf = VK_NULL_HANDLE;
  m_PostVS.Data[eventId].gsout.idxbufmem = VK_NULL_HANDLE;

  m_PostVS.Data[eventId].gsout.hasPosOut = true;

  // delete framebuffer and renderpass
  m_pDriver->vkDestroyFramebuffer(dev, fb, NULL);
  m_pDriver->vkDestroyRenderPass(dev, rp, NULL);

  // delete pipeline
  m_pDriver->vkDestroyPipeline(dev, pipe, NULL);

  // delete shader/shader module
  m_pDriver->vkDestroyShaderModule(dev, module, NULL);
}

void VulkanReplay::InitPostVSBuffers(uint32_t eventId)
{
  // go through any aliasing
  if(m_PostVS.Alias.find(eventId) != m_PostVS.Alias.end())
    eventId = m_PostVS.Alias[eventId];

  if(m_PostVS.Data.find(eventId) != m_PostVS.Data.end())
    return;

  const VulkanRenderState &state = m_pDriver->m_RenderState;
  VulkanCreationInfo &creationInfo = m_pDriver->m_CreationInfo;

  if(state.graphics.pipeline == ResourceId() || state.renderPass == ResourceId())
    return;

  const VulkanCreationInfo::Pipeline &pipeInfo = creationInfo.m_Pipeline[state.graphics.pipeline];

  if(pipeInfo.shaders[0].module == ResourceId())
    return;

  const DrawcallDescription *drawcall = m_pDriver->GetDrawcall(eventId);

  if(drawcall == NULL || drawcall->numIndices == 0 || drawcall->numInstances == 0)
    return;

  VkMarkerRegion::Begin(StringFormat::Fmt("FetchVSOut for %u", eventId));

  FetchVSOut(eventId);

  VkMarkerRegion::End();

  // if there's no tessellation or geometry shader active, bail out now
  if(pipeInfo.shaders[2].module == ResourceId() && pipeInfo.shaders[3].module == ResourceId())
    return;

  VkMarkerRegion::Begin(StringFormat::Fmt("FetchTessGSOut for %u", eventId));

  FetchTessGSOut(eventId);

  VkMarkerRegion::End();
}

struct VulkanInitPostVSCallback : public VulkanDrawcallCallback
{
  VulkanInitPostVSCallback(WrappedVulkan *vk, const std::vector<uint32_t> &events)
      : m_pDriver(vk), m_Events(events)
  {
    m_pDriver->SetDrawcallCB(this);
  }
  ~VulkanInitPostVSCallback() { m_pDriver->SetDrawcallCB(NULL); }
  void PreDraw(uint32_t eid, VkCommandBuffer cmd)
  {
    if(std::find(m_Events.begin(), m_Events.end(), eid) != m_Events.end())
      m_pDriver->GetReplay()->InitPostVSBuffers(eid);
  }

  bool PostDraw(uint32_t eid, VkCommandBuffer cmd) { return false; }
  void PostRedraw(uint32_t eid, VkCommandBuffer cmd) {}
  // Dispatches don't rasterize, so do nothing
  void PreDispatch(uint32_t eid, VkCommandBuffer cmd) {}
  bool PostDispatch(uint32_t eid, VkCommandBuffer cmd) { return false; }
  void PostRedispatch(uint32_t eid, VkCommandBuffer cmd) {}
  // Ditto copy/etc
  void PreMisc(uint32_t eid, DrawFlags flags, VkCommandBuffer cmd) {}
  bool PostMisc(uint32_t eid, DrawFlags flags, VkCommandBuffer cmd) { return false; }
  void PostRemisc(uint32_t eid, DrawFlags flags, VkCommandBuffer cmd) {}
  void PreEndCommandBuffer(VkCommandBuffer cmd) {}
  void AliasEvent(uint32_t primary, uint32_t alias)
  {
    if(std::find(m_Events.begin(), m_Events.end(), primary) != m_Events.end())
      m_pDriver->GetReplay()->AliasPostVSBuffers(primary, alias);
  }

  WrappedVulkan *m_pDriver;
  const std::vector<uint32_t> &m_Events;
};

void VulkanReplay::InitPostVSBuffers(const std::vector<uint32_t> &events)
{
  // first we must replay up to the first event without replaying it. This ensures any
  // non-command buffer calls like memory unmaps etc all happen correctly before this
  // command buffer
  m_pDriver->ReplayLog(0, events.front(), eReplay_WithoutDraw);

  VulkanInitPostVSCallback cb(m_pDriver, events);

  // now we replay the events, which are guaranteed (because we generated them in
  // GetPassEvents above) to come from the same command buffer, so the event IDs are
  // still locally continuous, even if we jump into replaying.
  m_pDriver->ReplayLog(events.front(), events.back(), eReplay_Full);
}

MeshFormat VulkanReplay::GetPostVSBuffers(uint32_t eventId, uint32_t instID, uint32_t viewID,
                                          MeshDataStage stage)
{
  // go through any aliasing
  if(m_PostVS.Alias.find(eventId) != m_PostVS.Alias.end())
    eventId = m_PostVS.Alias[eventId];

  VulkanPostVSData postvs;
  RDCEraseEl(postvs);

  if(m_PostVS.Data.find(eventId) != m_PostVS.Data.end())
    postvs = m_PostVS.Data[eventId];

  const DrawcallDescription *drawcall = m_pDriver->GetDrawcall(eventId);

  uint32_t numInstances = 1;
  if(drawcall && (drawcall->flags & DrawFlags::Instanced))
    numInstances = drawcall->numInstances;

  VulkanPostVSData::StageData s = postvs.GetStage(stage);

  // clamp viewID
  if(s.numViews > 1)
    viewID = RDCMIN(viewID, s.numViews - 1);
  else
    viewID = 0;

  MeshFormat ret;

  if(s.useIndices && s.idxbuf != VK_NULL_HANDLE)
  {
    ret.indexResourceId = GetResID(s.idxbuf);
    ret.indexByteStride = s.idxFmt == VK_INDEX_TYPE_UINT16 ? 2 : 4;
  }
  else
  {
    ret.indexResourceId = ResourceId();
    ret.indexByteStride = 0;
  }
  ret.indexByteOffset = 0;
  ret.baseVertex = s.baseVertex;

  if(s.buf != VK_NULL_HANDLE)
    ret.vertexResourceId = GetResID(s.buf);
  else
    ret.vertexResourceId = ResourceId();

  ret.vertexByteOffset = s.instStride * (instID + viewID * numInstances);
  ret.vertexByteStride = s.vertStride;

  ret.format.compCount = 4;
  ret.format.compByteWidth = 4;
  ret.format.compType = CompType::Float;
  ret.format.type = ResourceFormatType::Regular;

  ret.showAlpha = false;

  ret.topology = MakePrimitiveTopology(s.topo, 1);
  ret.numIndices = s.numVerts;

  ret.unproject = s.hasPosOut;
  ret.nearPlane = s.nearPlane;
  ret.farPlane = s.farPlane;

  if(instID < s.instData.size())
  {
    VulkanPostVSData::InstData inst = s.instData[instID];

    ret.vertexByteOffset = inst.bufOffset;
    ret.numIndices = inst.numVerts;
  }

  return ret;
}
