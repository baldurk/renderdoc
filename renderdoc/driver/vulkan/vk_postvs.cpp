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
#include "driver/shaders/spirv/spirv_editor.h"
#include "vk_core.h"
#include "vk_debug.h"
#include "vk_shader_cache.h"

static const char *PatchedMeshOutputEntryPoint = "rdc";
static const uint32_t MeshOutputDispatchWidth = 128;
static const uint32_t MeshOutputTBufferArraySize = 16;

static void ConvertToMeshOutputCompute(const ShaderReflection &refl, const SPIRVPatchData &patchData,
                                       const char *entryName, std::vector<uint32_t> instDivisor,
                                       uint32_t &descSet, const DrawcallDescription *draw,
                                       int32_t indexOffset, uint64_t numFetchVerts, uint32_t numVerts,
                                       std::vector<uint32_t> &modSpirv, uint32_t &bufStride)
{
  SPIRVEditor editor(modSpirv);

  uint32_t numInputs = (uint32_t)refl.inputSignature.size();

  uint32_t numOutputs = (uint32_t)refl.outputSignature.size();
  RDCASSERT(numOutputs > 0);

  descSet = 0;

  for(SPIRVIterator it = editor.BeginDecorations(), end = editor.EndDecorations(); it < end; ++it)
  {
    // we will use the descriptor set immediately after the last set statically used by the shader.
    // This means we don't have to worry about if the descriptor set layout declares more sets which
    // might be invalid and un-bindable, we just trample over the next set that's unused.
    // This is much easier than trying to add a new bind to an existing descriptor set (which would
    // cascade into a new descriptor set layout, new pipeline layout, etc etc!). However, this might
    // push us over the limit on number of descriptor sets.
    if(it.opcode() == spv::OpDecorate && it.word(2) == spv::DecorationDescriptorSet)
      descSet = RDCMAX(descSet, it.word(3) + 1);
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
    SPIRVId variableID;
    // constant ID for the index of this attribute
    SPIRVId constID;
    // the type ID for this attribute. Must be present already by definition!
    SPIRVId basetypeID;
    // tbuffer type for this input
    tbufferType tbuffer;
    // gvec4 type for this input, used as result type when fetching from tbuffer
    uint32_t vec4ID;
    // Uniform Pointer ID for this output. Used only for output data, to write to output SSBO
    SPIRVId uniformPtrID;
    // Output Pointer ID for this attribute.
    // For inputs, used to 'write' to the global at the start.
    // For outputs, used to 'read' from the global at the end.
    SPIRVId privatePtrID;
  };
  std::vector<inputOutputIDs> ins;
  ins.resize(numInputs);
  std::vector<inputOutputIDs> outs;
  outs.resize(numOutputs);

  std::set<SPIRVId> inputs;
  std::set<SPIRVId> outputs;

  std::map<SPIRVId, SPIRVId> typeReplacements;

  // rewrite any inputs and outputs to be private storage class
  for(SPIRVIterator it = editor.BeginTypes(), end = editor.EndTypes(); it < end; ++it)
  {
    // rewrite any input/output variables to private, and build up inputs/outputs list
    if(it.opcode() == spv::OpTypePointer)
    {
      SPIRVId id;

      if(it.word(2) == spv::StorageClassInput)
      {
        id = it.word(1);
        inputs.insert(id);
      }
      else if(it.word(2) == spv::StorageClassOutput)
      {
        id = it.word(1);
        outputs.insert(id);

        SPIRVId baseId = it.word(3);

        SPIRVIterator baseIt = editor.GetID(baseId);
        if(baseIt && baseIt.opcode() == spv::OpTypeStruct)
          outputs.insert(baseId);
      }

      if(id)
      {
        SPIRVPointer privPtr(it.word(3), spv::StorageClassPrivate);

        SPIRVId origId = editor.GetType(privPtr);

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

        inputs.insert(it.word(2));
      }
      else if(it.word(3) == spv::StorageClassOutput)
      {
        mod = true;
        editor.PreModify(it);
        it.word(3) = spv::StorageClassPrivate;

        outputs.insert(it.word(2));
      }

      auto replIt = typeReplacements.find(it.word(1));
      if(replIt != typeReplacements.end())
      {
        if(!mod)
          editor.PreModify(it);
        mod = true;
        it.word(1) = typeReplacements[it.word(1)];
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
        SPIRVOperation op = SPIRVOperation::copy(it);
        // remove the old one
        editor.Remove(it);
        // add it anew
        editor.AddVariable(op);
      }
    }
    else if(it.opcode() == spv::OpTypeFunction)
    {
      bool mod = false;

      auto replIt = typeReplacements.find(it.word(1));
      if(replIt != typeReplacements.end())
      {
        editor.PreModify(it);
        mod = true;
        it.word(1) = typeReplacements[it.word(1)];
      }

      for(size_t i = 4; i < it.size(); it++)
      {
        replIt = typeReplacements.find(it.word(i));
        if(replIt != typeReplacements.end())
        {
          if(!mod)
            editor.PreModify(it);
          mod = true;
          it.word(i) = typeReplacements[it.word(i)];
        }
      }

      if(mod)
        editor.PostModify(it);
    }
    else if(it.opcode() == spv::OpConstantNull)
    {
      auto replIt = typeReplacements.find(it.word(1));
      if(replIt != typeReplacements.end())
      {
        editor.PreModify(it);
        it.word(1) = typeReplacements[it.word(1)];
        editor.PostModify(it);
      }
    }
  }

  for(SPIRVIterator it = editor.BeginFunctions(); it; ++it)
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
      auto replIt = typeReplacements.find(id);
      if(replIt != typeReplacements.end())
        id = typeReplacements[id];

      editor.PostModify(it);
    }
  }

  // detect builtin inputs or outputs, and remove builtin decorations
  for(SPIRVIterator it = editor.BeginDecorations(), end = editor.EndDecorations(); it < end; ++it)
  {
    // remove any builtin decorations
    if(it.opcode() == spv::OpDecorate && it.word(2) == spv::DecorationBuiltIn)
    {
      SPIRVId id = it.word(1);

      if(outputs.find(id) != outputs.end())
      {
        // outputs we don't have to do anything, discard the builtin information
      }
      else if(inputs.find(id) != inputs.end())
      {
        // for inputs, record the variable ID for this builtin
        for(size_t i = 0; i < refl.inputSignature.size(); i++)
        {
          const SigParameter &sig = refl.inputSignature[i];

          if(sig.systemValue ==
             BuiltInToSystemAttribute(ShaderStage::Vertex, (spv::BuiltIn)it.word(3)))
          {
            ins[i].variableID = id;
            break;
          }
        }
      }

      editor.Remove(it);
    }

    if(it.opcode() == spv::OpMemberDecorate && it.word(3) == spv::DecorationBuiltIn)
      editor.Remove(it);

    // remove block decoration from input or output structs
    if(it.opcode() == spv::OpDecorate && it.word(2) == spv::DecorationBlock)
    {
      SPIRVId id = it.word(1);

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
      SPIRVId id = it.word(1);

      if(outputs.find(id) != outputs.end())
      {
        // outputs we don't have to do anything, discard the location information
      }
      else if(inputs.find(id) != inputs.end())
      {
        // for inputs, record the variable ID for this location
        for(size_t i = 0; i < refl.inputSignature.size(); i++)
        {
          const SigParameter &sig = refl.inputSignature[i];

          if(sig.systemValue == ShaderBuiltin::Undefined && sig.regIndex == it.word(3))
          {
            ins[i].variableID = id;
            break;
          }
        }
      }

      editor.Remove(it);
    }
  }

  SPIRVId entryID = 0;

  std::set<SPIRVId> entries;

  for(const SPIRVEntry &entry : editor.GetEntries())
  {
    if(entry.name == entryName)
      entryID = entry.id;

    entries.insert(entry.id);
  }

  RDCASSERT(entryID);

  for(SPIRVIterator it = editor.BeginDebug(), end2 = editor.EndDebug(); it < end2; ++it)
  {
    if(it.opcode() == spv::OpName &&
       (inputs.find(it.word(1)) != inputs.end() || outputs.find(it.word(1)) != outputs.end()))
    {
      SPIRVId id = it.word(1);
      std::string oldName = (const char *)&it.word(2);
      editor.Remove(it);
      editor.SetName(id, ("emulated_" + oldName).c_str());
    }

    // remove any OpName for the old entry points
    if(it.opcode() == spv::OpName && entries.find(it.word(1)) != entries.end())
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

    io.variableID = patchData.outputs[i].ID;

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
      RDCERR("Double inputs are not supported, will be undefined");
      scalarType = scalar<double>();
    }

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
    uint32_t imageTypeID;
    uint32_t imageSampledTypeID;
    uint32_t pointerTypeID;
    uint32_t variableID;
  } tbuffers[tbuffer_count];

  uint32_t arraySize = editor.AddConstantImmediate<uint32_t>(MeshOutputTBufferArraySize);

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

    uint32_t arrayType = editor.MakeId();
    editor.AddType(
        SPIRVOperation(spv::OpTypeArray, {arrayType, tbuffers[tb].imageSampledTypeID, arraySize}));

    uint32_t arrayPtrType =
        editor.DeclareType(SPIRVPointer(arrayType, spv::StorageClassUniformConstant));

    tbuffers[tb].pointerTypeID = editor.DeclareType(
        SPIRVPointer(tbuffers[tb].imageSampledTypeID, spv::StorageClassUniformConstant));

    tbuffers[tb].variableID = editor.MakeId();
    editor.AddVariable(SPIRVOperation(
        spv::OpVariable, {arrayPtrType, tbuffers[tb].variableID, spv::StorageClassUniformConstant}));

    editor.SetName(tbuffers[tb].variableID, name);

    editor.AddDecoration(SPIRVOperation(
        spv::OpDecorate, {tbuffers[tb].variableID, (uint32_t)spv::DecorationDescriptorSet, descSet}));
    editor.AddDecoration(SPIRVOperation(
        spv::OpDecorate, {tbuffers[tb].variableID, (uint32_t)spv::DecorationBinding, (uint32_t)tb}));
  }

  SPIRVId uint32Vec4ID = 0;
  SPIRVId idxImageTypeID = 0;
  SPIRVId idxImagePtr = 0;
  SPIRVId idxSampledTypeID = 0;

  if(draw->flags & DrawFlags::UseIBuffer)
  {
    uint32Vec4ID = editor.DeclareType(SPIRVVector(scalar<uint32_t>(), 4));

    idxImageTypeID = editor.DeclareType(
        SPIRVImage(scalar<uint32_t>(), spv::DimBuffer, 0, 0, 0, 1, spv::ImageFormatUnknown));
    idxSampledTypeID = editor.DeclareType(SPIRVSampledImage(idxImageTypeID));

    uint32_t idxImagePtrType =
        editor.DeclareType(SPIRVPointer(idxSampledTypeID, spv::StorageClassUniformConstant));

    idxImagePtr = editor.MakeId();
    editor.AddVariable(SPIRVOperation(
        spv::OpVariable, {idxImagePtrType, idxImagePtr, spv::StorageClassUniformConstant}));

    editor.SetName(idxImagePtr, "ibuffer");

    editor.AddDecoration(SPIRVOperation(
        spv::OpDecorate, {idxImagePtr, (uint32_t)spv::DecorationDescriptorSet, descSet}));
    editor.AddDecoration(
        SPIRVOperation(spv::OpDecorate, {idxImagePtr, (uint32_t)spv::DecorationBinding, 1}));
  }

  if(numInputs > 0)
  {
    editor.AddCapability(spv::CapabilitySampledBuffer);
  }

  SPIRVId outBufferVarID = 0;
  SPIRVId numFetchVertsConstID = editor.AddConstantImmediate((int32_t)numFetchVerts);
  SPIRVId numVertsConstID = editor.AddConstantImmediate((int32_t)numVerts);
  SPIRVId numInstConstID = editor.AddConstantImmediate((int32_t)draw->numInstances);

  editor.SetName(numFetchVertsConstID, "numFetchVerts");
  editor.SetName(numVertsConstID, "numVerts");
  editor.SetName(numInstConstID, "numInsts");

  // declare the output buffer and its type
  {
    std::vector<uint32_t> words;
    for(uint32_t o = 0; o < numOutputs; o++)
      words.push_back(outs[o].basetypeID);

    // struct vertex { ... outputs };
    SPIRVId vertStructID = editor.DeclareStructType(words);
    editor.SetName(vertStructID, "vertex_struct");

    // vertex vertArray[];
    SPIRVId runtimeArrayID =
        editor.AddType(SPIRVOperation(spv::OpTypeRuntimeArray, {editor.MakeId(), vertStructID}));
    editor.SetName(runtimeArrayID, "vertex_array");

    // struct meshOutput { vertex vertArray[]; };
    SPIRVId outputStructID = editor.DeclareStructType({runtimeArrayID});
    editor.SetName(outputStructID, "meshOutput");

    // meshOutput *
    SPIRVId outputStructPtrID =
        editor.DeclareType(SPIRVPointer(outputStructID, spv::StorageClassUniform));
    editor.SetName(outputStructPtrID, "meshOutput_ptr");

    // meshOutput *outputData;
    outBufferVarID = editor.AddVariable(SPIRVOperation(
        spv::OpVariable, {outputStructPtrID, editor.MakeId(), spv::StorageClassUniform}));
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
      editor.AddDecoration(SPIRVOperation(spv::OpMemberDecorate,
                                          {vertStructID, o, spv::DecorationOffset, memberOffset}));

      memberOffset += elemSize * refl.outputSignature[o].compCount;
    }

    // align to 16 bytes (vec4) since we will almost certainly have
    // a vec4 in the struct somewhere, and even in std430 alignment,
    // the base struct alignment is still the largest base alignment
    // of any member
    bufStride = AlignUp16(memberOffset);

    // the array is the only element in the output struct, so
    // it's at offset 0
    editor.AddDecoration(
        SPIRVOperation(spv::OpMemberDecorate, {outputStructID, 0, spv::DecorationOffset, 0}));

    // set array stride
    editor.AddDecoration(
        SPIRVOperation(spv::OpDecorate, {runtimeArrayID, spv::DecorationArrayStride, bufStride}));

    // set object type
    editor.AddDecoration(
        SPIRVOperation(spv::OpDecorate, {outputStructID, spv::DecorationBufferBlock}));

    // set binding
    editor.AddDecoration(
        SPIRVOperation(spv::OpDecorate, {outBufferVarID, spv::DecorationDescriptorSet, descSet}));
    editor.AddDecoration(SPIRVOperation(spv::OpDecorate, {outBufferVarID, spv::DecorationBinding, 0}));
  }

  SPIRVId uint32Vec3ID = editor.DeclareType(SPIRVVector(scalar<uint32_t>(), 3));
  SPIRVId invocationPtr = editor.DeclareType(SPIRVPointer(uint32Vec3ID, spv::StorageClassInput));
  SPIRVId invocationId = editor.AddVariable(
      SPIRVOperation(spv::OpVariable, {invocationPtr, editor.MakeId(), spv::StorageClassInput}));
  editor.AddDecoration(SPIRVOperation(
      spv::OpDecorate, {invocationId, spv::DecorationBuiltIn, spv::BuiltInGlobalInvocationId}));

  editor.SetName(invocationId, "rdoc_invocation");

  // make a new entry point that will call the old function, then when it returns extract & write
  // the outputs.
  SPIRVId wrapperEntry = editor.MakeId();
  // don't set a debug name, as some drivers get confused when this doesn't match the entry point
  // name :(.
  // editor.SetName(wrapperEntry, "RenderDoc_MeshFetch_Wrapper_Entrypoint");

  // we remove all entry points and just create one of our own.
  SPIRVIterator it = editor.BeginEntries();

  {
    // there should already have been at least one entry point
    RDCASSERT(it.opcode() == spv::OpEntryPoint);
    // and it should have been at least 5 words (if not more) since a vertex shader cannot function
    // without at least one interface ID. We only need one, so there should be plenty space.
    RDCASSERT(it.size() >= 5);

    editor.PreModify(it);

    SPIRVOperation op(it);

    op.nopRemove(5);

    op[1] = spv::ExecutionModelGLCompute;
    op[2] = wrapperEntry;
    op[3] = MAKE_FOURCC('r', 'd', 'c', 0);
    op[4] = invocationId;

    editor.PostModify(it);

    ++it;
  }

  for(SPIRVIterator end = editor.EndEntries(); it < end; ++it)
    editor.Remove(it);

  editor.AddOperation(
      it, SPIRVOperation(spv::OpExecutionMode, {wrapperEntry, spv::ExecutionModeLocalSize,
                                                MeshOutputDispatchWidth, 1, 1}));

  SPIRVId uint32ID = editor.DeclareType(scalar<uint32_t>());
  SPIRVId sint32ID = editor.DeclareType(scalar<int32_t>());

  // add the wrapper function
  {
    std::vector<SPIRVOperation> ops;

    SPIRVId voidType = editor.DeclareType(scalar<void>());
    SPIRVId funcType = editor.DeclareType(SPIRVFunction(voidType, {}));

    ops.push_back(SPIRVOperation(spv::OpFunction,
                                 {voidType, wrapperEntry, spv::FunctionControlMaskNone, funcType}));

    ops.push_back(SPIRVOperation(spv::OpLabel, {editor.MakeId()}));
    {
      // uint3 invocationVec = gl_GlobalInvocationID;
      uint32_t invocationVector = editor.MakeId();
      ops.push_back(SPIRVOperation(spv::OpLoad, {uint32Vec3ID, invocationVector, invocationId}));

      // uint invocation = invocationVec.x
      uint32_t invocationID = editor.MakeId();
      ops.push_back(
          SPIRVOperation(spv::OpCompositeExtract, {uint32ID, invocationID, invocationVector, 0U}));

      // int intInvocationID = int(invocation);
      uint32_t intInvocationID = editor.MakeId();
      ops.push_back(SPIRVOperation(spv::OpBitcast, {sint32ID, intInvocationID, invocationID}));

      editor.SetName(intInvocationID, "invocation");

      // int inst = intInvocationID / numFetchVerts
      uint32_t instID = editor.MakeId();
      ops.push_back(
          SPIRVOperation(spv::OpSDiv, {sint32ID, instID, intInvocationID, numFetchVertsConstID}));

      editor.SetName(instID, "instanceID");

      // bool inBounds = inst < numInstances;
      uint32_t inBounds = editor.MakeId();
      ops.push_back(SPIRVOperation(
          spv::OpULessThan, {editor.DeclareType(scalar<bool>()), inBounds, instID, numInstConstID}));

      // if(inBounds) goto continueLabel; else goto killLabel;
      uint32_t killLabel = editor.MakeId();
      uint32_t continueLabel = editor.MakeId();
      ops.push_back(SPIRVOperation(spv::OpSelectionMerge, {killLabel, spv::SelectionControlMaskNone}));
      ops.push_back(SPIRVOperation(spv::OpBranchConditional, {inBounds, continueLabel, killLabel}));

      // continueLabel:
      ops.push_back(SPIRVOperation(spv::OpLabel, {continueLabel}));

      // int vtx = intInvocationID % numVerts
      uint32_t vtx = editor.MakeId();
      ops.push_back(SPIRVOperation(spv::OpSMod, {sint32ID, vtx, intInvocationID, numVertsConstID}));

      editor.SetName(vtx, "vertexID");

      uint32_t vertexIndex = vtx;

      // if we're indexing, look up the index buffer. We don't have to apply vertexOffset - it was
      // already applied when we read back and uniq-ified the index buffer.
      if(draw->flags & DrawFlags::UseIBuffer)
      {
        // sampledimage idximg = *idximgPtr;
        uint32_t loaded = editor.MakeId();
        ops.push_back(SPIRVOperation(spv::OpLoad, {idxSampledTypeID, loaded, idxImagePtr}));

        // image rawimg = imageFromSampled(idximg);
        uint32_t rawimg = editor.MakeId();
        ops.push_back(SPIRVOperation(spv::OpImage, {idxImageTypeID, rawimg, loaded}));

        // uvec4 result = texelFetch(rawimg, vtxID);
        uint32_t result = editor.MakeId();
        ops.push_back(SPIRVOperation(spv::OpImageFetch, {uint32Vec4ID, result, rawimg, vertexIndex}));

        // uint vtxID = result.x;
        uint32_t uintIndex = editor.MakeId();
        ops.push_back(SPIRVOperation(spv::OpCompositeExtract, {uint32ID, uintIndex, result, 0}));

        vertexIndex = editor.MakeId();
        ops.push_back(SPIRVOperation(spv::OpBitcast, {sint32ID, vertexIndex, uintIndex}));
      }

      // int arraySlotID = inst * numVerts;
      uint32_t arraySlotTempID = editor.MakeId();
      ops.push_back(SPIRVOperation(spv::OpIMul, {sint32ID, arraySlotTempID, instID, numVertsConstID}));

      // arraySlotID = arraySlotID + vertexIndex;
      uint32_t arraySlotTemp2ID = editor.MakeId();
      ops.push_back(
          SPIRVOperation(spv::OpIAdd, {sint32ID, arraySlotTemp2ID, arraySlotTempID, vertexIndex}));

      // arraySlotID = arraySlotID + indexOffset;
      uint32_t arraySlotID = editor.MakeId();
      ops.push_back(SPIRVOperation(spv::OpIAdd, {sint32ID, arraySlotID, arraySlotTemp2ID,
                                                 editor.AddConstantImmediate(indexOffset)}));

      editor.SetName(arraySlotID, "arraySlot");

      // we use the current value of vertexIndex and use instID, to lookup per-vertex and
      // per-instance attributes. This is because when we fetched the vertex data, we advanced by
      // (in non-indexed draws) vertexOffset, and by instanceOffset. Rather than fetching data
      // that's only used as padding skipped over by these offsets.
      uint32_t vertexLookup = vertexIndex;
      uint32_t instanceLookup = instID;

      if(!(draw->flags & DrawFlags::UseIBuffer))
      {
        // for non-indexed draws, we manually apply the vertex offset, but here after we used the
        // 0-based one to calculate the array slot
        vertexIndex = editor.MakeId();
        ops.push_back(SPIRVOperation(
            spv::OpIAdd, {sint32ID, vertexIndex, vtx,
                          editor.AddConstantImmediate(int32_t(draw->vertexOffset & 0x7fffffff))}));
      }
      editor.SetName(vertexIndex, "vertexIndex");

      // instIndex = inst + instOffset
      uint32_t instIndex = editor.MakeId();
      ops.push_back(SPIRVOperation(
          spv::OpIAdd, {sint32ID, instIndex, instID,
                        editor.AddConstantImmediate(int32_t(draw->instanceOffset & 0x7fffffff))}));
      editor.SetName(instIndex, "instanceIndex");

      uint32_t idxs[64] = {};

      for(size_t i = 0; i < refl.inputSignature.size(); i++)
      {
        ShaderBuiltin builtin = refl.inputSignature[i].systemValue;

        if(builtin == ShaderBuiltin::VertexIndex)
        {
          ops.push_back(SPIRVOperation(spv::OpStore, {ins[i].variableID, vertexIndex}));
        }
        else if(builtin == ShaderBuiltin::InstanceIndex)
        {
          ops.push_back(SPIRVOperation(spv::OpStore, {ins[i].variableID, instIndex}));
        }
        else if(builtin != ShaderBuiltin::Undefined)
        {
          RDCERR("Unsupported/unsupported built-in input %s", ToStr(builtin).c_str());
        }
        else
        {
          if(idxs[i] == 0)
            idxs[i] = editor.AddConstantImmediate<uint32_t>((uint32_t)i);

          if(idxs[refl.inputSignature[i].regIndex] == 0)
            idxs[refl.inputSignature[i].regIndex] =
                editor.AddConstantImmediate<uint32_t>((uint32_t)refl.inputSignature[i].regIndex);

          tbufferIDs tb = tbuffers[ins[i].tbuffer];

          uint32_t location = refl.inputSignature[i].regIndex;

          uint32_t ptrId = editor.MakeId();
          // sampledimage *imgPtr = xxx_tbuffers[i];
          ops.push_back(SPIRVOperation(spv::OpAccessChain, {tb.pointerTypeID, ptrId, tb.variableID,
                                                            idxs[refl.inputSignature[i].regIndex]}));

          // sampledimage img = *imgPtr;
          uint32_t loaded = editor.MakeId();
          ops.push_back(SPIRVOperation(spv::OpLoad, {tb.imageSampledTypeID, loaded, ptrId}));

          // image rawimg = imageFromSampled(img);
          uint32_t rawimg = editor.MakeId();
          ops.push_back(SPIRVOperation(spv::OpImage, {tb.imageTypeID, rawimg, loaded}));

          // vec4 result = texelFetch(rawimg, vtxID or instID);
          uint32_t idx = vertexLookup;

          if(location < instDivisor.size())
          {
            uint32_t divisor = instDivisor[location];

            if(divisor == ~0U)
            {
              // this magic value indicates vertex-rate data
              idx = vertexLookup;
            }
            else if(divisor == 0)
            {
              // if the divisor is 0, all instances read the first value.
              idx = editor.AddConstantImmediate<int32_t>(0);
            }
            else if(divisor == 1)
            {
              // if the divisor is 1, it's just regular instancing
              idx = instanceLookup;
            }
            else
            {
              // otherwise we divide by the divisor
              idx = editor.MakeId();
              divisor = editor.AddConstantImmediate(int32_t(divisor & 0x7fffffff));
              ops.push_back(SPIRVOperation(spv::OpSDiv, {sint32ID, idx, instanceLookup, divisor}));
            }
          }

          uint32_t result = editor.MakeId();
          ops.push_back(SPIRVOperation(spv::OpImageFetch, {ins[i].vec4ID, result, rawimg, idx}));

          // for one component, extract x, for less than 4, extract the sub-vector, otherwise
          // leave alone (4 components)
          if(refl.inputSignature[i].compCount == 1)
          {
            uint32_t swizzleIn = result;
            result = editor.MakeId();

            // baseType value = result.x;
            ops.push_back(
                SPIRVOperation(spv::OpCompositeExtract, {ins[i].basetypeID, result, swizzleIn, 0}));
          }
          else if(refl.inputSignature[i].compCount != 4)
          {
            uint32_t swizzleIn = result;
            result = editor.MakeId();

            std::vector<uint32_t> words = {ins[i].basetypeID, result, swizzleIn, swizzleIn};

            for(uint32_t c = 0; c < refl.inputSignature[i].compCount; c++)
              words.push_back(c);

            // baseTypeN value = result.xyz;
            ops.push_back(SPIRVOperation(spv::OpVectorShuffle, words));
          }

          // not a composite type, we can store directly
          if(patchData.inputs[i].accessChain.empty())
          {
            // *global = value
            ops.push_back(SPIRVOperation(spv::OpStore, {ins[i].variableID, result}));
          }
          else
          {
            // for composite types we need to access chain first
            uint32_t subElement = editor.MakeId();
            std::vector<uint32_t> words = {ins[i].privatePtrID, subElement, patchData.inputs[i].ID};

            for(uint32_t accessIdx : patchData.inputs[i].accessChain)
            {
              if(idxs[accessIdx] == 0)
                idxs[accessIdx] = editor.AddConstantImmediate<uint32_t>((uint32_t)accessIdx);

              words.push_back(idxs[accessIdx]);
            }

            ops.push_back(SPIRVOperation(spv::OpAccessChain, words));

            ops.push_back(SPIRVOperation(spv::OpStore, {subElement, result}));
          }
        }
      }

      // real_main();
      ops.push_back(SPIRVOperation(spv::OpFunctionCall, {voidType, editor.MakeId(), entryID}));

      SPIRVId zero = editor.AddConstantImmediate<uint32_t>(0);

      for(uint32_t o = 0; o < numOutputs; o++)
      {
        uint32_t loaded = 0;

        // not a structure member or array child, can load directly
        if(patchData.outputs[o].accessChain.empty())
        {
          loaded = editor.MakeId();
          // type loaded = *globalvar;
          ops.push_back(
              SPIRVOperation(spv::OpLoad, {outs[o].basetypeID, loaded, patchData.outputs[o].ID}));
        }
        else
        {
          uint32_t readPtr = editor.MakeId();
          loaded = editor.MakeId();

          // structure member, need to access chain first
          std::vector<uint32_t> words = {outs[o].privatePtrID, readPtr, patchData.outputs[o].ID};

          for(uint32_t idx : patchData.outputs[o].accessChain)
          {
            if(idxs[idx] == 0)
              idxs[idx] = editor.AddConstantImmediate<uint32_t>(idx);

            words.push_back(idxs[idx]);
          }

          // type *readPtr = globalvar.globalsub...;
          ops.push_back(SPIRVOperation(spv::OpAccessChain, words));
          // type loaded = *readPtr;
          ops.push_back(SPIRVOperation(spv::OpLoad, {outs[o].basetypeID, loaded, readPtr}));
        }

        // access chain the destination
        // type *writePtr = outBuffer.verts[arraySlot].outputN
        uint32_t writePtr = editor.MakeId();
        ops.push_back(SPIRVOperation(
            spv::OpAccessChain,
            {outs[o].uniformPtrID, writePtr, outBufferVarID, zero, arraySlotID, outs[o].constID}));

        // *writePtr = loaded;
        ops.push_back(SPIRVOperation(spv::OpStore, {writePtr, loaded}));
      }

      // goto killLabel;
      ops.push_back(SPIRVOperation(spv::OpBranch, {killLabel}));

      // killLabel:
      ops.push_back(SPIRVOperation(spv::OpLabel, {killLabel}));
    }
    ops.push_back(SPIRVOperation(spv::OpReturn, {}));

    ops.push_back(SPIRVOperation(spv::OpFunctionEnd, {}));

    editor.AddFunction(ops.data(), ops.size());
  }

  editor.StripNops();
}

void VulkanReplay::ClearPostVSCache()
{
  VkDevice dev = m_Device;

  for(auto it = m_PostVSData.begin(); it != m_PostVSData.end(); ++it)
  {
    m_pDriver->vkDestroyBuffer(dev, it->second.vsout.buf, NULL);
    m_pDriver->vkFreeMemory(dev, it->second.vsout.bufmem, NULL);
  }

  m_PostVSData.clear();
}

void VulkanReplay::InitPostVSBuffers(uint32_t eventId)
{
  // go through any aliasing
  if(m_PostVSAlias.find(eventId) != m_PostVSAlias.end())
    eventId = m_PostVSAlias[eventId];

  if(m_PostVSData.find(eventId) != m_PostVSData.end())
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
    m_PostVSData[eventId].vsout.idxBuf = ResourceId();

    m_PostVSData[eventId].vsout.topo = pipeInfo.topology;

    return;
  }

  const DrawcallDescription *drawcall = m_pDriver->GetDrawcall(eventId);

  if(drawcall == NULL || drawcall->numIndices == 0 || drawcall->numInstances == 0)
    return;

  // the SPIR-V patching will determine the next descriptor set to use, after all sets statically
  // used by the shader. This gets around the problem where the shader only uses 0 and 1, but the
  // layout declares 0-4, and 2,3,4 are invalid at bind time and we are unable to bind our new set
  // 5. Instead we'll notice that only 0 and 1 are used and just use 2 ourselves (although it was
  // in
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

  VkBuffer meshBuffer = VK_NULL_HANDLE, readbackBuffer = VK_NULL_HANDLE;
  VkDeviceMemory meshMem = VK_NULL_HANDLE, readbackMem = VK_NULL_HANDLE;

  VkBuffer uniqIdxBuf = VK_NULL_HANDLE;
  VkDeviceMemory uniqIdxBufMem = VK_NULL_HANDLE;
  VkBufferView uniqIdxBufView = VK_NULL_HANDLE;

  uint32_t numVerts = drawcall->numIndices;
  uint64_t numFetchVerts = drawcall->numIndices;
  VkDeviceSize bufSize = 0;

  uint32_t idxsize = state.ibuffer.bytewidth;

  int32_t baseVertex = 0;

  uint32_t minIndex = 0, maxIndex = RDCMAX(drawcall->baseVertex, 0) + numVerts - 1;

  uint32_t maxInstance = drawcall->instanceOffset + drawcall->numInstances - 1;

  if(drawcall->flags & DrawFlags::UseIBuffer)
  {
    bool index16 = (idxsize == 2);
    bytebuf idxdata;
    std::vector<uint32_t> indices;
    uint16_t *idx16 = NULL;
    uint32_t *idx32 = NULL;

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

    minIndex = indices[0];
    maxIndex = indices[indices.size() - 1];

    // set numVerts
    numVerts = maxIndex - minIndex + 1;
    numFetchVerts = (uint64_t)indices.size();

    // An index buffer could be something like: 500, 520, 518, 553, 554, 556
    // but in our vertex buffer that will be: 0, 20, 18, 53, 54, 56
    // so we add -minIndex as the baseVertex when rendering. The existing baseVertex was 'applied'
    // when we fetched the mesh output so it can be discarded.
    baseVertex = -(int32_t)minIndex;

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

    m_pDriver->vkUnmapMemory(m_Device, uniqIdxBufMem);
  }

  uint32_t bufStride = 0;
  vector<uint32_t> modSpirv = moduleInfo.spirv.spirv;

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
    bytebuf origVBs[16];

    for(uint32_t vb = 0; vb < vi->vertexBindingDescriptionCount; vb++)
    {
      VkDeviceSize offs = state.vbuffers[vb].offs;
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

      GetBufferData(state.vbuffers[vb].buf, offs, len, origVBs[vb]);
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

        if(IsUIntFormat(origFormat))
          expandedFormat = VK_FORMAT_R32G32B32A32_UINT;
        else if(IsSIntFormat(origFormat))
          expandedFormat = VK_FORMAT_R32G32B32A32_SINT;
        else
          expandedFormat = VK_FORMAT_R32G32B32A32_SFLOAT;
      }

      uint32_t elemSize = GetByteSize(1, 1, 1, expandedFormat, 0);

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
            if(IsUIntFormat(expandedFormat))
            {
              while(src < origVBEnd && dst < dstEnd)
              {
                uint32_t val = 0;

                uint8_t c = 0;
                for(; c < fmt.compCount; c++)
                {
                  if(fmt.compByteWidth == 1)
                    val = *src;
                  else if(fmt.compByteWidth == 2)
                    val = *(uint16_t *)src;
                  else if(fmt.compByteWidth == 4)
                    val = *(uint32_t *)src;

                  memcpy(dst, &val, sizeof(uint32_t));
                  dst += sizeof(uint32_t);
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

                uint8_t c = 0;
                for(; c < fmt.compCount; c++)
                {
                  if(fmt.compByteWidth == 1)
                    val = *(int8_t *)src;
                  else if(fmt.compByteWidth == 2)
                    val = *(int16_t *)src;
                  else if(fmt.compByteWidth == 4)
                    val = *(int32_t *)src;

                  memcpy(dst, &val, sizeof(int32_t));
                  dst += sizeof(int32_t);
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
      descWrites[numWrites].dstSet = m_MeshFetchDescSet;
      if(IsSIntFormat(attrDesc.format))
        descWrites[numWrites].dstBinding = 4;
      else if(IsUIntFormat(attrDesc.format))
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
      descWrites[numWrites].dstSet = m_MeshFetchDescSet;
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
                             pipeInfo.shaders[0].entryPoint.c_str(), attrInstDivisor, descSet,
                             drawcall, baseVertex, numFetchVerts, numVerts, modSpirv, bufStride);

  VkComputePipelineCreateInfo compPipeInfo = {VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};

  {
    VkDescriptorSetLayout *descSetLayouts;

    // descSet will be the index of our new descriptor set
    descSetLayouts = new VkDescriptorSetLayout[descSet + 1];

    for(uint32_t i = 0; i < descSet; i++)
      descSetLayouts[i] = m_pDriver->GetResourceManager()->GetCurrentHandle<VkDescriptorSetLayout>(
          creationInfo.m_PipelineLayout[pipeInfo.layout].descSetLayouts[i]);

    // this layout just says it has one storage buffer
    descSetLayouts[descSet] = m_MeshFetchDescSetLayout;

    std::vector<VkPushConstantRange> push = creationInfo.m_PipelineLayout[pipeInfo.layout].pushRanges;

    // ensure the push range is visible to the compute shader
    for(VkPushConstantRange &range : push)
      range.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

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
    compPipeInfo.layout = pipeLayout;
  }

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

  // push back extra descriptor set to partial replay state
  // note that we examined the used pipeline layout above and inserted our descriptor set
  // after any the application used. So there might be more bound, but we want to ensure to
  // bind to the slot we're using
  modifiedstate.compute.descSets.resize(descSet + 1);
  modifiedstate.compute.descSets[descSet].descSet = GetResID(m_MeshFetchDescSet);

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

    VkCommandBuffer cmd = m_pDriver->GetNextCmd();

    VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL,
                                          VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};

    vkr = ObjDisp(dev)->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    // fill destination buffer with 0s to ensure unwritten vertices have sane data
    ObjDisp(dev)->CmdFillBuffer(Unwrap(cmd), Unwrap(meshBuffer), 0, bufInfo.size, 0xbaadf00d);

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

    // set bufSize
    bufSize = numVerts * drawcall->numInstances * bufStride;

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
    modifiedstate.BindPipeline(cmd, VulkanRenderState::BindCompute, true);
    uint64_t totalVerts = numFetchVerts * uint64_t(drawcall->numInstances);

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

  // fill out m_PostVSData
  m_PostVSData[eventId].vsin.topo = pipeCreateInfo.pInputAssemblyState->topology;
  m_PostVSData[eventId].vsout.topo = pipeCreateInfo.pInputAssemblyState->topology;
  m_PostVSData[eventId].vsout.buf = meshBuffer;
  m_PostVSData[eventId].vsout.bufmem = meshMem;

  m_PostVSData[eventId].vsout.baseVertex = baseVertex + drawcall->baseVertex;

  m_PostVSData[eventId].vsout.vertStride = bufStride;
  m_PostVSData[eventId].vsout.nearPlane = nearp;
  m_PostVSData[eventId].vsout.farPlane = farp;

  m_PostVSData[eventId].vsout.useIndices = bool(drawcall->flags & DrawFlags::UseIBuffer);
  m_PostVSData[eventId].vsout.numVerts = drawcall->numIndices;

  m_PostVSData[eventId].vsout.instStride = 0;
  if(drawcall->flags & DrawFlags::Instanced)
    m_PostVSData[eventId].vsout.instStride = uint32_t(bufSize / drawcall->numInstances);

  m_PostVSData[eventId].vsout.idxBuf = ResourceId();
  if(m_PostVSData[eventId].vsout.useIndices && state.ibuffer.buf != ResourceId())
  {
    m_PostVSData[eventId].vsout.idxBuf = GetResourceManager()->GetOriginalID(state.ibuffer.buf);
    m_PostVSData[eventId].vsout.idxOffset = state.ibuffer.offs + drawcall->indexOffset * idxsize;
    m_PostVSData[eventId].vsout.idxFmt = idxsize == 2 ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32;
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
struct VulkanInitPostVSCallback : public VulkanDrawcallCallback
{
  VulkanInitPostVSCallback(WrappedVulkan *vk, const vector<uint32_t> &events)
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

void VulkanReplay::InitPostVSBuffers(const vector<uint32_t> &events)
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

MeshFormat VulkanReplay::GetPostVSBuffers(uint32_t eventId, uint32_t instID, MeshDataStage stage)
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

  if(s.useIndices && s.idxBuf != ResourceId())
  {
    ret.indexResourceId = s.idxBuf;
    ret.indexByteStride = s.idxFmt == VK_INDEX_TYPE_UINT16 ? 2 : 4;
  }
  else
  {
    ret.indexResourceId = ResourceId();
    ret.indexByteStride = 0;
  }
  ret.indexByteOffset = s.idxOffset;
  ret.baseVertex = s.baseVertex;

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
