/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2024 Baldur Karlsson
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
#include <math.h>
#include <algorithm>
#include "core/settings.h"
#include "driver/shaders/spirv/spirv_editor.h"
#include "driver/shaders/spirv/spirv_op_helpers.h"
#include "replay/replay_driver.h"
#include "vk_core.h"
#include "vk_debug.h"
#include "vk_replay.h"
#include "vk_shader_cache.h"

RDOC_CONFIG(rdcstr, Vulkan_Debug_PostVSDumpDirPath, "",
            "Path to dump gnerated SPIR-V compute shaders for fetching post-vs.");
RDOC_EXTERN_CONFIG(bool, Vulkan_Debug_DisableBufferDeviceAddress);

#undef None

struct ScopedOOMHandleVk
{
  ScopedOOMHandleVk(WrappedVulkan *vk)
  {
    m_pDriver = vk;
    m_pDriver->HandleOOM(true);
  }

  ~ScopedOOMHandleVk() { m_pDriver->HandleOOM(false); }
  WrappedVulkan *m_pDriver;
};

struct VkXfbQueryResult
{
  uint64_t numPrimitivesWritten;
  uint64_t numPrimitivesGenerated;
};

static const char *PatchedMeshOutputEntryPoint = "rdc";
static const uint32_t MeshOutputDispatchWidth = 128;
static uint32_t MeshOutputBufferArraySize = 64;

// 0 = output
// 1 = indices
// 2 = vbuffers
static const uint32_t MeshOutputReservedBindings = 3;

enum StorageMode
{
  Binding,
  EXT_bda,
  KHR_bda,
};

static void ConvertToMeshOutputCompute(const ShaderReflection &refl,
                                       const SPIRVPatchData &patchData, const rdcstr &entryName,
                                       StorageMode storageMode, rdcarray<uint32_t> instDivisor,
                                       const ActionDescription *action, uint32_t numVerts,
                                       uint32_t numViews, uint32_t baseSpecConstant,
                                       rdcarray<uint32_t> &modSpirv, uint32_t &bufStride)
{
  rdcspv::Editor editor(modSpirv);

  editor.Prepare();

  uint32_t numInputs = (uint32_t)refl.inputSignature.size();

  uint32_t numOutputs = (uint32_t)refl.outputSignature.size();
  RDCASSERT(numOutputs > 0);

  if(storageMode == Binding)
  {
    for(rdcspv::Iter it = editor.Begin(rdcspv::Section::Annotations),
                     end = editor.End(rdcspv::Section::Annotations);
        it < end; ++it)
    {
      // we will use descriptor set 0 bindings 0..N for our own purposes when not using buffer
      // device address.
      //
      // Since bindings are arbitrary, we just increase all user bindings to make room, and we'll
      // redeclare the descriptor set layouts and pipeline layout. This is inevitable in the case
      // where all descriptor sets are already used. In theory we only have to do this with set 0,
      // but that requires knowing which variables are in set 0 and it's simpler to increase all
      // bindings.
      if(it.opcode() == rdcspv::Op::Decorate)
      {
        rdcspv::OpDecorate dec(it);
        if(dec.decoration == rdcspv::Decoration::Binding)
        {
          RDCASSERT(dec.decoration.binding < (0xffffffff - MeshOutputReservedBindings));
          dec.decoration.binding += MeshOutputReservedBindings;
          it = dec;
        }
      }
    }
  }

  struct inputOutputIDs
  {
    // if this is a builtin value, what builtin value is expected
    ShaderBuiltin builtin = ShaderBuiltin::Undefined;
    // ID of the variable itself. This is the original Input/Output pointer variable that we convert
    // to a private pointer
    rdcspv::Id variable;
    // constant ID for the index of this attribute
    rdcspv::Id indexConst;
    // base gvec4 type for this input. We always fetch uvec4 from the buffer but then bitcast to
    // vec4 or ivec4 if needed
    rdcspv::Id fetchVec4Type;
    // the actual gvec4 type for the input, possibly needed to convert to from the above if it's
    // declared as a 16-bit type since we always fetch 32-bit.
    rdcspv::Id vec4Type;
    // the base type for this attribute. Must be present already by definition! This is the same
    // scalar type as vec4Type but with the correct number of components.
    rdcspv::Id baseType;
    // Uniform Pointer type ID for this output. Used only for output data, to write to output SSBO
    rdcspv::Id ssboPtrType;
    // Output Pointer type ID for this attribute.
    // For inputs, used to 'write' to the global at the start.
    // For outputs, used to 'read' from the global at the end.
    rdcspv::Id privatePtrType;
  };

  rdcarray<inputOutputIDs> ins;
  ins.resize(numInputs);
  rdcarray<inputOutputIDs> outs;
  outs.resize(numOutputs);

  std::set<rdcspv::Id> inputs;
  std::set<rdcspv::Id> outputs;

  std::map<rdcspv::Id, rdcspv::Id> typeReplacements;

  // keep track of any builtins we're preserving
  std::set<rdcspv::Id> builtinKeeps;

  // detect builtin inputs or outputs, and remove builtin decorations
  for(rdcspv::Iter it = editor.Begin(rdcspv::Section::Annotations),
                   end = editor.End(rdcspv::Section::Annotations);
      it < end; ++it)
  {
    if(it.opcode() == rdcspv::Op::Decorate)
    {
      rdcspv::OpDecorate decorate(it);
      // remove any builtin decorations
      if(decorate.decoration == rdcspv::Decoration::BuiltIn)
      {
        // subgroup builtins can be allowed to stay
        if(decorate.decoration.builtIn == rdcspv::BuiltIn::SubgroupEqMask ||
           decorate.decoration.builtIn == rdcspv::BuiltIn::SubgroupGtMask ||
           decorate.decoration.builtIn == rdcspv::BuiltIn::SubgroupGeMask ||
           decorate.decoration.builtIn == rdcspv::BuiltIn::SubgroupLtMask ||
           decorate.decoration.builtIn == rdcspv::BuiltIn::SubgroupLeMask ||
           decorate.decoration.builtIn == rdcspv::BuiltIn::SubgroupLocalInvocationId ||
           decorate.decoration.builtIn == rdcspv::BuiltIn::SubgroupSize)
        {
          builtinKeeps.insert(decorate.target);
          continue;
        }

        // we don't have to do anything, the ID mapping is in the rdcspv::PatchData, so just discard
        // the location information
        editor.Remove(it);
      }
      // remove all invariant decorations
      else if(decorate.decoration == rdcspv::Decoration::Invariant)
      {
        editor.Remove(it);
      }
      // remove all index decorations
      else if(decorate.decoration == rdcspv::Decoration::Index)
      {
        editor.Remove(it);
      }
      // same with flat/noperspective
      else if(decorate.decoration == rdcspv::Decoration::Flat ||
              decorate.decoration == rdcspv::Decoration::NoPerspective)
      {
        editor.Remove(it);
      }
      else if(decorate.decoration == rdcspv::Decoration::Location ||
              decorate.decoration == rdcspv::Decoration::Component)
      {
        // we don't have to do anything, the ID mapping is in the rdcspv::PatchData, so just discard
        // the location information
        editor.Remove(it);
      }
    }

    if(it.opcode() == rdcspv::Op::MemberDecorate)
    {
      rdcspv::OpMemberDecorate memberDecorate(it);
      if(memberDecorate.decoration == rdcspv::Decoration::BuiltIn)
        editor.Remove(it);
    }
  }

  // rewrite any inputs and outputs to be private storage class
  for(rdcspv::Iter it = editor.Begin(rdcspv::Section::TypesVariablesConstants),
                   end = editor.End(rdcspv::Section::TypesVariablesConstants);
      it < end; ++it)
  {
    // rewrite any input/output variables to private, and build up inputs/outputs list
    if(it.opcode() == rdcspv::Op::TypePointer)
    {
      rdcspv::OpTypePointer ptr(it);

      rdcspv::Id id;

      if(ptr.storageClass == rdcspv::StorageClass::Input)
      {
        id = ptr.result;
        inputs.insert(id);
      }
      else if(ptr.storageClass == rdcspv::StorageClass::Output)
      {
        id = ptr.result;
        outputs.insert(id);

        rdcspv::Iter baseIt = editor.GetID(ptr.type);
        if(baseIt && baseIt.opcode() == rdcspv::Op::TypeStruct)
          outputs.insert(ptr.type);
      }
      else if(ptr.storageClass == rdcspv::StorageClass::Private ||
              ptr.storageClass == rdcspv::StorageClass::Function)
      {
        // with variable pointers, we could have a private/function pointer into one of the pointer
        // types we've replaced (e.g. Input and Output where one is patched to be private and the
        // other is replaced since we deduplicate pointer types)
        //
        // we don't have to re-order the declaration, since we're iterating the types in order so
        // the replacement is always earlier than the type it was replacing

        if(typeReplacements.find(ptr.type) != typeReplacements.end())
        {
          editor.PreModify(it);

          ptr.type = typeReplacements[ptr.type];
          it = ptr;

          // if we didn't already have this pointer, process the modified type declaration
          editor.PostModify(it);
        }
      }

      if(id)
      {
        rdcspv::Pointer privPtr(ptr.type, rdcspv::StorageClass::Private);

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

          ptr.storageClass = rdcspv::StorageClass::Private;
          it = ptr;

          // if we didn't already have this pointer, process the modified type declaration
          editor.PostModify(it);
        }
      }
    }
    else if(it.opcode() == rdcspv::Op::Variable)
    {
      rdcspv::OpVariable var(it);

      bool mod = false;

      if(builtinKeeps.find(var.result) != builtinKeeps.end())
      {
        // if this variable is one we're keeping as a builtin, we need to do something different.
        // We don't change its storage class, but we might need to redeclare the pointer as the
        // right matching storage class (because it's been patched to private). This might be
        editor.PreModify(it);

        rdcspv::Id ptrId = var.resultType;
        // if this is in typeReplacements the id is no longer valid and was removed
        auto replIt = typeReplacements.find(ptrId);
        if(replIt != typeReplacements.end())
          ptrId = replIt->second;

        rdcspv::OpTypePointer ptr(editor.GetID(ptrId));

        // declare if necessary the right pointer again, and use that as our type
        var.resultType = editor.DeclareType(rdcspv::Pointer(ptr.type, var.storageClass));

        it = var;
        editor.PostModify(it);

        // copy this variable declaration to the end of the section, after our potentially 'new'
        // recreated pointer type

        rdcspv::Operation op = rdcspv::Operation::copy(it);
        editor.Remove(it);
        editor.AddVariable(op);
        // don't need to add this to the globals because if it needed to be in there it already was

        // don't do any of the rest of the processing
        continue;
      }
      else if(var.storageClass == rdcspv::StorageClass::Input)
      {
        mod = true;
        editor.PreModify(it);

        var.storageClass = rdcspv::StorageClass::Private;

        inputs.insert(var.result);
      }
      else if(var.storageClass == rdcspv::StorageClass::Output)
      {
        mod = true;
        editor.PreModify(it);

        var.storageClass = rdcspv::StorageClass::Private;

        outputs.insert(var.result);
      }

      auto replIt = typeReplacements.find(var.resultType);
      if(replIt != typeReplacements.end())
      {
        if(!mod)
          editor.PreModify(it);
        mod = true;
        var.resultType = replIt->second;
      }

      if(mod)
      {
        it = var;
        editor.PostModify(it);
      }

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
        // don't need to add this to the globals because if it needed to be in there it already was
      }
    }
    else if(it.opcode() == rdcspv::Op::TypeFunction)
    {
      rdcspv::OpTypeFunction func(it);

      bool mod = false;

      auto replIt = typeReplacements.find(func.result);
      if(replIt != typeReplacements.end())
      {
        editor.PreModify(it);
        mod = true;
        func.result = replIt->second;
      }

      for(size_t i = 0; i < func.parameters.size(); i++)
      {
        replIt = typeReplacements.find(func.parameters[i]);
        if(replIt != typeReplacements.end())
        {
          if(!mod)
            editor.PreModify(it);
          mod = true;
          func.parameters[i] = replIt->second;
        }
      }

      if(mod)
      {
        it = func;
        editor.PostModify(it);
      }
    }
    else if(it.opcode() == rdcspv::Op::ConstantNull)
    {
      rdcspv::OpConstantNull nullconst(it);

      auto replIt = typeReplacements.find(nullconst.resultType);
      if(replIt != typeReplacements.end())
      {
        editor.PreModify(it);
        nullconst.resultType = replIt->second;
        it = nullconst;
        editor.PostModify(it);
      }
    }
    else if(it.opcode() == rdcspv::Op::Undef)
    {
      rdcspv::OpUndef undef(it);

      auto replIt = typeReplacements.find(undef.resultType);
      if(replIt != typeReplacements.end())
      {
        editor.PreModify(it);
        undef.resultType = replIt->second;
        it = undef;
        editor.PostModify(it);
      }
    }
  }

  for(rdcspv::Iter it = editor.Begin(rdcspv::Section::Functions); it; ++it)
  {
    // identify functions with result types we might want to replace
    if(it.opcode() == rdcspv::Op::Function || it.opcode() == rdcspv::Op::FunctionParameter ||
       it.opcode() == rdcspv::Op::Variable || it.opcode() == rdcspv::Op::AccessChain ||
       it.opcode() == rdcspv::Op::InBoundsAccessChain || it.opcode() == rdcspv::Op::Bitcast ||
       it.opcode() == rdcspv::Op::Undef || it.opcode() == rdcspv::Op::ExtInst ||
       it.opcode() == rdcspv::Op::ExtInstWithForwardRefsKHR ||
       it.opcode() == rdcspv::Op::FunctionCall || it.opcode() == rdcspv::Op::Phi ||
       it.opcode() == rdcspv::Op::Select)
    {
      editor.PreModify(it);

      rdcspv::Id id = rdcspv::Id::fromWord(it.word(1));
      auto replIt = typeReplacements.find(id);
      if(replIt != typeReplacements.end())
        id = replIt->second;
      it.word(1) = id.value();

      editor.PostModify(it);
    }
  }

  rdcspv::Id entryID;

  std::set<rdcspv::Id> entries;

  for(const rdcspv::EntryPoint &entry : editor.GetEntries())
  {
    if(entry.name == entryName && entry.executionModel == rdcspv::ExecutionModel::Vertex)
      entryID = entry.id;

    entries.insert(entry.id);
  }

  RDCASSERT(entryID);

  // tracks all the global variables we use, for compliance with SPIR-V 1.4.
  rdcarray<rdcspv::Id> globals;

  // we remove all entry points, we'll create one of our own.
  for(rdcspv::Iter it = editor.Begin(rdcspv::Section::EntryPoints),
                   end = editor.End(rdcspv::Section::EntryPoints);
      it < end; ++it)
  {
    rdcspv::OpEntryPoint entry(it);

    // when we find the entry point we're patching, grab it's interface for the set of globals. We
    // will be patching and Input/Output variables to private, but from SPIR-V 1.4 the interface
    // needs to include privates as well.
    if(entry.entryPoint == entryID)
      globals = entry.iface;

    editor.Remove(it);
  }

  for(rdcspv::Iter it = editor.Begin(rdcspv::Section::ExecutionMode),
                   end = editor.End(rdcspv::Section::ExecutionMode);
      it < end; ++it)
  {
    // this can also handle ExecutionModeId and we don't care about the difference
    rdcspv::OpExecutionMode execMode(it);

    if(execMode.entryPoint != entryID)
      editor.Remove(it);
  }

  for(rdcspv::Iter it = editor.Begin(rdcspv::Section::DebugNames),
                   end2 = editor.End(rdcspv::Section::DebugNames);
      it < end2; ++it)
  {
    if(it.opcode() == rdcspv::Op::Name)
    {
      rdcspv::OpName name(it);

      if(inputs.find(name.target) != inputs.end() || outputs.find(name.target) != outputs.end())
      {
        editor.Remove(it);
        if(typeReplacements.find(name.target) == typeReplacements.end())
          editor.SetName(name.target, "emulated_" + name.name);
      }

      // remove any OpName for the old entry points
      if(entries.find(name.target) != entries.end())
        editor.Remove(it);

      // remove any OpName for deleted types
      if(typeReplacements.find(name.target) != typeReplacements.end())
        editor.Remove(it);
    }
  }

  rdcspv::StorageClass bufferClass;
  if(storageMode == Binding)
    bufferClass = editor.StorageBufferClass();
  else
    bufferClass = rdcspv::StorageClass::PhysicalStorageBuffer;

  // declare necessary variables per-output, types and constants. We do this last so that we don't
  // add a private pointer that we later try and deduplicate when collapsing output/input pointers
  // to private
  for(uint32_t i = 0; i < numOutputs; i++)
  {
    inputOutputIDs &io = outs[i];

    io.builtin = refl.outputSignature[i].systemValue;

    // constant for this index
    io.indexConst = editor.AddConstantImmediate(i);

    io.variable = patchData.outputs[i].ID;

    // base type - either a scalar or a vector, since matrix outputs are decayed to vectors
    {
      rdcspv::Scalar scalarType = rdcspv::scalar(refl.outputSignature[i].varType);

      io.vec4Type = editor.DeclareType(rdcspv::Vector(scalarType, 4));

      if(refl.outputSignature[i].compCount > 1)
        io.baseType =
            editor.DeclareType(rdcspv::Vector(scalarType, refl.outputSignature[i].compCount));
      else
        io.baseType = editor.DeclareType(scalarType);
    }

    io.ssboPtrType = editor.DeclareType(rdcspv::Pointer(io.baseType, bufferClass));
    io.privatePtrType =
        editor.DeclareType(rdcspv::Pointer(io.baseType, rdcspv::StorageClass::Private));

    RDCASSERT(io.baseType && io.vec4Type && io.indexConst && io.privatePtrType && io.ssboPtrType,
              io.baseType, io.vec4Type, io.indexConst, io.privatePtrType, io.ssboPtrType);
  }

  // repeat for inputs
  for(uint32_t i = 0; i < numInputs; i++)
  {
    inputOutputIDs &io = ins[i];

    io.builtin = refl.inputSignature[i].systemValue;

    // constant for this index
    io.indexConst = editor.AddConstantImmediate(i);

    io.variable = patchData.inputs[i].ID;

    VarType vType = refl.inputSignature[i].varType;

    rdcspv::Scalar scalarType = rdcspv::scalar(vType);

    // 64-bit values are loaded as uvec4 and then packed in pairs, so we need to declare vec4ID as
    // uvec4
    if(vType == VarType::Double || vType == VarType::ULong || vType == VarType::SLong)
    {
      io.fetchVec4Type = io.vec4Type =
          editor.DeclareType(rdcspv::Vector(rdcspv::scalar<uint32_t>(), 4));
    }
    else
    {
      io.vec4Type = editor.DeclareType(rdcspv::Vector(scalarType, 4));

      // if the underlying scalar is actually
      switch(vType)
      {
        case VarType::Half:
          io.fetchVec4Type = editor.DeclareType(rdcspv::Vector(rdcspv::scalar<float>(), 4));
          break;
        case VarType::SShort:
        case VarType::SByte:
          io.fetchVec4Type = editor.DeclareType(rdcspv::Vector(rdcspv::scalar<int32_t>(), 4));
          break;
        case VarType::UShort:
        case VarType::UByte:
          io.fetchVec4Type = editor.DeclareType(rdcspv::Vector(rdcspv::scalar<uint32_t>(), 4));
          break;
        default: io.fetchVec4Type = io.vec4Type; break;
      }
    }

    if(refl.inputSignature[i].compCount > 1)
      io.baseType = editor.DeclareType(rdcspv::Vector(scalarType, refl.inputSignature[i].compCount));
    else
      io.baseType = editor.DeclareType(scalarType);

    io.privatePtrType =
        editor.DeclareType(rdcspv::Pointer(io.baseType, rdcspv::StorageClass::Private));

    RDCASSERT(io.baseType && io.vec4Type && io.indexConst && io.privatePtrType, io.baseType,
              io.vec4Type, io.indexConst, io.privatePtrType);
  }

  rdcspv::Id u32Type = editor.DeclareType(rdcspv::scalar<uint32_t>());
  rdcspv::Id uvec4Type = editor.DeclareType(rdcspv::Vector(rdcspv::scalar<uint32_t>(), 4));

  rdcspv::Id uvec4StructPtrType;
  rdcspv::Id uintStructPtrType;

  rdcspv::Id arraySize = editor.AddConstantImmediate<uint32_t>(MeshOutputBufferArraySize);

  rdcspv::Id vbuffersVariable, ibufferVariable;

  rdcarray<rdcspv::Id> vbufferSpecConsts;
  rdcarray<rdcspv::Id> vbufferVariables;
  vbufferSpecConsts.resize(MeshOutputBufferArraySize);
  vbufferVariables.resize(MeshOutputBufferArraySize);
  rdcspv::Id ibufferSpecConst;
  rdcspv::Id outputSpecConst;

  {
    rdcspv::Id runtimeArrayID =
        editor.AddType(rdcspv::OpTypeRuntimeArray(editor.MakeId(), uvec4Type));
    editor.AddDecoration(rdcspv::OpDecorate(
        runtimeArrayID,
        rdcspv::DecorationParam<rdcspv::Decoration::ArrayStride>(sizeof(uint32_t) * 4)));

    rdcspv::Id uvec4StructType =
        editor.AddType(rdcspv::OpTypeStruct(editor.MakeId(), {runtimeArrayID}));
    editor.SetName(uvec4StructType, "__rd_uvec4Struct");

    editor.AddDecoration(rdcspv::OpMemberDecorate(
        uvec4StructType, 0, rdcspv::DecorationParam<rdcspv::Decoration::Offset>(0)));

    uvec4StructPtrType = editor.DeclareType(rdcspv::Pointer(uvec4StructType, bufferClass));
    editor.SetName(uvec4StructPtrType, "__rd_uvec4Struct_ptr");

    runtimeArrayID = editor.AddType(rdcspv::OpTypeRuntimeArray(editor.MakeId(), u32Type));
    editor.AddDecoration(rdcspv::OpDecorate(
        runtimeArrayID, rdcspv::DecorationParam<rdcspv::Decoration::ArrayStride>(sizeof(uint32_t))));

    rdcspv::Id uintStructType =
        editor.AddType(rdcspv::OpTypeStruct(editor.MakeId(), {runtimeArrayID}));

    editor.SetName(uintStructType, "__rd_uintStruct");

    editor.AddDecoration(rdcspv::OpMemberDecorate(
        uintStructType, 0, rdcspv::DecorationParam<rdcspv::Decoration::Offset>(0)));

    uintStructPtrType = editor.DeclareType(rdcspv::Pointer(uintStructType, bufferClass));
    editor.SetName(uintStructPtrType, "__rd_uintStruct_ptr");

    if(storageMode == Binding)
    {
      editor.DecorateStorageBufferStruct(uvec4StructType);
      editor.DecorateStorageBufferStruct(uintStructType);

      rdcspv::Id structArrayType = editor.AddType(
          rdcspv::OpTypeArray(editor.MakeId(), uvec4StructType,
                              editor.AddConstantImmediate<uint32_t>(MeshOutputBufferArraySize)));
      rdcspv::Id vbuffersType = editor.DeclareType(rdcspv::Pointer(structArrayType, bufferClass));

      vbuffersVariable = editor.MakeId();
      editor.AddVariable(rdcspv::OpVariable(vbuffersType, vbuffersVariable, bufferClass));
      editor.AddDecoration(rdcspv::OpDecorate(
          vbuffersVariable, rdcspv::DecorationParam<rdcspv::Decoration::DescriptorSet>(0)));
      editor.AddDecoration(rdcspv::OpDecorate(
          vbuffersVariable, rdcspv::DecorationParam<rdcspv::Decoration::Binding>(2)));

      globals.push_back(vbuffersVariable);

      editor.SetName(vbuffersVariable, "__rd_vbuffers");

      if(action->flags & ActionFlags::Indexed)
      {
        rdcspv::Id ibufferType = editor.DeclareType(rdcspv::Pointer(uintStructType, bufferClass));

        ibufferVariable = editor.MakeId();
        editor.AddVariable(rdcspv::OpVariable(ibufferType, ibufferVariable, bufferClass));
        editor.AddDecoration(rdcspv::OpDecorate(
            ibufferVariable, rdcspv::DecorationParam<rdcspv::Decoration::DescriptorSet>(0)));
        editor.AddDecoration(rdcspv::OpDecorate(
            ibufferVariable, rdcspv::DecorationParam<rdcspv::Decoration::Binding>(1)));

        globals.push_back(ibufferVariable);

        editor.SetName(ibufferVariable, "__rd_ibuffer");
      }
    }
    else
    {
      editor.AddDecoration(rdcspv::OpDecorate(uvec4StructType, rdcspv::Decoration::Block));
      editor.AddDecoration(rdcspv::OpDecorate(uintStructType, rdcspv::Decoration::Block));

      // add the extension
      editor.AddExtension(storageMode == KHR_bda ? "SPV_KHR_physical_storage_buffer"
                                                 : "SPV_EXT_physical_storage_buffer");

      // change the memory model to physical storage buffer 64
      rdcspv::Iter it = editor.Begin(rdcspv::Section::MemoryModel);
      rdcspv::OpMemoryModel model(it);
      model.addressingModel = rdcspv::AddressingModel::PhysicalStorageBuffer64;
      it = model;

      // add capabilities
      editor.AddCapability(rdcspv::Capability::PhysicalStorageBufferAddresses);

      if(storageMode == EXT_bda)
        editor.AddCapability(rdcspv::Capability::Int64);

      for(uint32_t i = 0; i <= MeshOutputBufferArraySize + 1; i++)
      {
        rdcspv::Id *dstId = NULL;
        if(i < MeshOutputBufferArraySize)
          dstId = &vbufferSpecConsts[i];
        else if(i == MeshOutputBufferArraySize)
          dstId = &ibufferSpecConst;
        else if(i == MeshOutputBufferArraySize + 1)
          dstId = &outputSpecConst;

        if(!dstId)
          break;

        if(storageMode == KHR_bda)
        {
          rdcspv::Id addressConstantLSB =
              editor.AddSpecConstantImmediate<uint32_t>(0U, baseSpecConstant + i * 2 + 0);
          rdcspv::Id addressConstantMSB =
              editor.AddSpecConstantImmediate<uint32_t>(0U, baseSpecConstant + i * 2 + 1);

          rdcspv::Id uint2 = editor.DeclareType(rdcspv::Vector(rdcspv::scalar<uint32_t>(), 2));

          *dstId = editor.AddConstant(rdcspv::OpSpecConstantComposite(
              uint2, editor.MakeId(), {addressConstantLSB, addressConstantMSB}));
        }
        else
        {
          *dstId = editor.AddSpecConstantImmediate<uint64_t>(0ULL, baseSpecConstant + i * 2);
        }

        if(i == MeshOutputBufferArraySize)
          editor.SetName(*dstId, "__rd_ibufferConst");
        else
          editor.SetName(*dstId, StringFormat::Fmt("__rd_vbufferConst%u", i));
      }
    }
  }

  rdcspv::Id uvec4PtrType = editor.DeclareType(rdcspv::Pointer(uvec4Type, bufferClass));
  rdcspv::Id uintPtrType = editor.DeclareType(rdcspv::Pointer(u32Type, bufferClass));

  if(numInputs > 0)
  {
    editor.AddCapability(rdcspv::Capability::SampledBuffer);
  }

  rdcspv::Id outBufferVarID;
  rdcspv::Id outputStructPtrType;
  rdcspv::Id numVertsConstID = editor.AddConstantImmediate<uint32_t>(numVerts);
  rdcspv::Id numInstConstID = editor.AddConstantImmediate<uint32_t>(action->numInstances);
  rdcspv::Id numViewsConstID = editor.AddConstantImmediate<uint32_t>(numViews);

  editor.SetName(numVertsConstID, "numVerts");
  editor.SetName(numInstConstID, "numInsts");
  editor.SetName(numViewsConstID, "numViews");

  // declare the output buffer and its type
  {
    rdcarray<rdcspv::Id> members;
    for(uint32_t o = 0; o < numOutputs; o++)
      members.push_back(outs[o].baseType);

    // struct vertex { ... outputs };
    rdcspv::Id vertStructID = editor.DeclareStructType(members);
    editor.SetName(vertStructID, "vertex_struct");

    // vertex vertArray[];
    rdcspv::Id runtimeArrayID =
        editor.AddType(rdcspv::OpTypeRuntimeArray(editor.MakeId(), vertStructID));
    editor.SetName(runtimeArrayID, "vertex_array");

    uint32_t memberOffset = 0;
    for(uint32_t o = 0; o < numOutputs; o++)
    {
      uint32_t elemSize = RDCMAX(4U, VarTypeByteSize(refl.outputSignature[o].varType));

      uint32_t numComps = refl.outputSignature[o].compCount;

      // ensure member is std430 packed (vec4 alignment for vec3/vec4)
      if(numComps == 2)
        memberOffset = AlignUp(memberOffset, 2U * elemSize);
      else if(numComps > 2)
        memberOffset = AlignUp(memberOffset, 4U * elemSize);

      // apply decoration to each member in the struct with its offset in the struct
      editor.AddDecoration(rdcspv::OpMemberDecorate(
          vertStructID, o, rdcspv::DecorationParam<rdcspv::Decoration::Offset>(memberOffset)));

      memberOffset += elemSize * refl.outputSignature[o].compCount;
    }

    // align to 16 bytes (vec4) since we will almost certainly have
    // a vec4 in the struct somewhere, and even in std430 alignment,
    // the base struct alignment is still the largest base alignment
    // of any member
    bufStride = AlignUp16(memberOffset);

    // struct meshOutput { vertex vertArray[]; };
    rdcspv::Id outputStructID = editor.DeclareStructType({runtimeArrayID});
    editor.SetName(outputStructID, "meshOutput");

    // meshOutput *
    outputStructPtrType = editor.DeclareType(rdcspv::Pointer(outputStructID, bufferClass));
    editor.SetName(outputStructPtrType, "meshOutput_ptr");

    // the array is the only element in the output struct, so
    // it's at offset 0
    editor.AddDecoration(rdcspv::OpMemberDecorate(
        outputStructID, 0, rdcspv::DecorationParam<rdcspv::Decoration::Offset>(0)));

    // set array stride
    editor.AddDecoration(rdcspv::OpDecorate(
        runtimeArrayID, rdcspv::DecorationParam<rdcspv::Decoration::ArrayStride>(bufStride)));

    if(storageMode == Binding)
    {
      // meshOutput *outputData;
      outBufferVarID =
          editor.AddVariable(rdcspv::OpVariable(outputStructPtrType, editor.MakeId(), bufferClass));
      editor.SetName(outBufferVarID, "outputData");

      globals.push_back(outBufferVarID);

      editor.DecorateStorageBufferStruct(outputStructID);

      // set binding
      editor.AddDecoration(rdcspv::OpDecorate(
          outBufferVarID, rdcspv::DecorationParam<rdcspv::Decoration::DescriptorSet>(0)));
      editor.AddDecoration(rdcspv::OpDecorate(
          outBufferVarID, rdcspv::DecorationParam<rdcspv::Decoration::Binding>(0)));
    }
    else
    {
      editor.AddDecoration(rdcspv::OpDecorate(outputStructID, rdcspv::Decoration::Block));
    }
  }

  rdcspv::Id uint32Vec3ID = editor.DeclareType(rdcspv::Vector(rdcspv::scalar<uint32_t>(), 3));
  rdcspv::Id invocationPtr =
      editor.DeclareType(rdcspv::Pointer(uint32Vec3ID, rdcspv::StorageClass::Input));
  rdcspv::Id invocationId = editor.AddVariable(
      rdcspv::OpVariable(invocationPtr, editor.MakeId(), rdcspv::StorageClass::Input));
  editor.AddDecoration(rdcspv::OpDecorate(
      invocationId,
      rdcspv::DecorationParam<rdcspv::Decoration::BuiltIn>(rdcspv::BuiltIn::GlobalInvocationId)));

  globals.push_back(invocationId);

  editor.SetName(invocationId, "rdoc_invocation");

  // make a new entry point that will call the old function, then when it returns extract & write
  // the outputs.
  rdcspv::Id wrapperEntry = editor.MakeId();
  // don't set a debug name, as some drivers get confused when this doesn't match the entry point
  // name :(.
  // editor.SetName(wrapperEntry, "RenderDoc_MeshFetch_Wrapper_Entrypoint");

  // if we're not using all globals, this is only Input variables so only our invocation Id and any
  // builtins we kept
  if(!editor.EntryPointAllGlobals())
  {
    globals = {invocationId};
    for(rdcspv::Id id : builtinKeeps)
      globals.push_back(id);
  }

  // insert the new patched entry point with the globals
  editor.AddOperation(editor.Begin(rdcspv::Section::EntryPoints),
                      rdcspv::OpEntryPoint(rdcspv::ExecutionModel::GLCompute, wrapperEntry,
                                           PatchedMeshOutputEntryPoint, globals));

  // Strip away any execution modes from the original shaders
  for(rdcspv::Iter it = editor.Begin(rdcspv::Section::ExecutionMode);
      it < editor.End(rdcspv::Section::ExecutionMode); ++it)
  {
    if(it.opcode() == rdcspv::Op::ExecutionMode)
    {
      rdcspv::OpExecutionMode execMode(it);

      // We only need to be cautious about what we are stripping for the entry
      // that we are actually translating, the rest aren't used anyways.
      if(execMode.entryPoint == entryID)
      {
        // Lets check to make sure we don't blindly strip away execution modes that
        // might actually have an impact on the behaviour of the shader.
        switch(execMode.mode)
        {
          // these execution modes should be applied to our entry point
          case rdcspv::ExecutionMode::DenormPreserve:
          case rdcspv::ExecutionMode::DenormFlushToZero:
          case rdcspv::ExecutionMode::SignedZeroInfNanPreserve:
          case rdcspv::ExecutionMode::RoundingModeRTE:
          case rdcspv::ExecutionMode::RoundingModeRTZ:
          case rdcspv::ExecutionMode::SubgroupUniformControlFlowKHR:
            editor.AddExecutionMode(rdcspv::OpExecutionMode(
                wrapperEntry, rdcspv::ExecutionModeAndParamData(execMode.mode)));
            break;
          case rdcspv::ExecutionMode::Xfb: break;
          default: RDCERR("Unexpected execution mode");
        }
      }

      editor.Remove(it);
    }
  }

  // Add our compute shader execution mode
  editor.AddExecutionMode(rdcspv::OpExecutionMode(
      wrapperEntry,
      rdcspv::ExecutionModeParam<rdcspv::ExecutionMode::LocalSize>(MeshOutputDispatchWidth, 1, 1)));

  rdcspv::Id zero = editor.AddConstantImmediate<uint32_t>(0);

  rdcspv::MemoryAccessAndParamDatas memoryAccess;

  // add the wrapper function
  {
    rdcspv::OperationList ops;

    rdcspv::Id voidType = editor.DeclareType(rdcspv::scalar<void>());
    rdcspv::Id funcType = editor.DeclareType(rdcspv::FunctionType(voidType, {}));

    ops.add(rdcspv::OpFunction(voidType, wrapperEntry, rdcspv::FunctionControl::None, funcType));

    ops.add(rdcspv::OpLabel(editor.MakeId()));
    {
      // convert the pointers here
      if(storageMode != Binding)
      {
        memoryAccess.setAligned(sizeof(uint32_t));

        if(ibufferSpecConst != rdcspv::Id())
        {
          // if we don't have the struct as a bind, we need to cast it from the pointer. In
          // KHR_buffer_device_address we bitcast since we store it as a uint2
          if(storageMode == KHR_bda)
            ibufferVariable =
                ops.add(rdcspv::OpBitcast(uintStructPtrType, editor.MakeId(), ibufferSpecConst));
          else
            ibufferVariable = ops.add(
                rdcspv::OpConvertUToPtr(uintStructPtrType, editor.MakeId(), ibufferSpecConst));

          editor.SetName(ibufferVariable, "__rd_ibuffer");
        }

        for(size_t s = 0; s < refl.inputSignature.size(); s++)
        {
          uint32_t idx = refl.inputSignature[s].regIndex;

          if(vbufferSpecConsts[idx] != rdcspv::Id() && vbufferVariables[idx] == rdcspv::Id())
          {
            if(storageMode == KHR_bda)
              vbufferVariables[idx] = ops.add(
                  rdcspv::OpBitcast(uvec4StructPtrType, editor.MakeId(), vbufferSpecConsts[idx]));
            else
              vbufferVariables[idx] = ops.add(rdcspv::OpConvertUToPtr(
                  uvec4StructPtrType, editor.MakeId(), vbufferSpecConsts[idx]));

            editor.SetName(vbufferVariables[idx], StringFormat::Fmt("__rd_vbuffers[%u]", idx));
          }
        }

        {
          if(storageMode == KHR_bda)
            outBufferVarID =
                ops.add(rdcspv::OpBitcast(outputStructPtrType, editor.MakeId(), outputSpecConst));
          else
            outBufferVarID = ops.add(
                rdcspv::OpConvertUToPtr(outputStructPtrType, editor.MakeId(), outputSpecConst));

          editor.SetName(outBufferVarID, "__rd_outbuf");
        }
      }

      // uint3 invocationVec = gl_GlobalInvocationID;
      rdcspv::Id invocationVector =
          ops.add(rdcspv::OpLoad(uint32Vec3ID, editor.MakeId(), invocationId));

      // uint invocation = invocationVec.x
      rdcspv::Id uintInvocationID =
          ops.add(rdcspv::OpCompositeExtract(u32Type, editor.MakeId(), invocationVector, {0U}));

      // arraySlotID = uintInvocationID;
      rdcspv::Id arraySlotID = uintInvocationID;

      editor.SetName(uintInvocationID, "arraySlot");

      // uint viewinst = uintInvocationID / numVerts
      rdcspv::Id viewinstID =
          ops.add(rdcspv::OpUDiv(u32Type, editor.MakeId(), uintInvocationID, numVertsConstID));

      editor.SetName(viewinstID, "viewInstance");

      rdcspv::Id instID =
          ops.add(rdcspv::OpUMod(u32Type, editor.MakeId(), viewinstID, numInstConstID));

      editor.SetName(instID, "instanceID");

      rdcspv::Id viewID =
          ops.add(rdcspv::OpUDiv(u32Type, editor.MakeId(), viewinstID, numInstConstID));

      editor.SetName(viewID, "viewID");

      // bool inBounds = viewID < numViews;
      rdcspv::Id inBounds = ops.add(rdcspv::OpULessThan(editor.DeclareType(rdcspv::scalar<bool>()),
                                                        editor.MakeId(), viewID, numViewsConstID));

      // if(inBounds) goto continueLabel; else goto killLabel;
      rdcspv::Id killLabel = editor.MakeId();
      rdcspv::Id continueLabel = editor.MakeId();
      ops.add(rdcspv::OpSelectionMerge(killLabel, rdcspv::SelectionControl::None));
      ops.add(rdcspv::OpBranchConditional(inBounds, continueLabel, killLabel));

      // continueLabel:
      ops.add(rdcspv::OpLabel(continueLabel));

      // uint vtx = uintInvocationID % numVerts
      rdcspv::Id vtxID =
          ops.add(rdcspv::OpUMod(u32Type, editor.MakeId(), uintInvocationID, numVertsConstID));
      editor.SetName(vtxID, "vertexID");

      rdcspv::Id vertexIndexID = vtxID;

      // if we're indexing, look up the index buffer. We don't have to apply vertexOffset - it was
      // already applied when we read back and uniq-ified the index buffer.
      if(action->flags & ActionFlags::Indexed)
      {
        rdcspv::Id idxPtr;

        // idxptr = &ibuffer.member0[vertexIndex]
        idxPtr = ops.add(rdcspv::OpAccessChain(uintPtrType, editor.MakeId(), ibufferVariable,
                                               {zero, vertexIndexID}));

        // vertexIndex = *idxptr
        vertexIndexID = ops.add(rdcspv::OpLoad(u32Type, editor.MakeId(), idxPtr, memoryAccess));
      }

      // we use the current value of vertexIndex and use instID, to lookup per-vertex and
      // per-instance attributes. This is because when we fetched the vertex data, we advanced by
      // (in non-indexed draws) vertexOffset, and by instanceOffset. Rather than fetching data
      // that's only used as padding skipped over by these offsets.
      rdcspv::Id vertexLookupID = vertexIndexID;
      rdcspv::Id instanceLookupID = instID;

      if(!(action->flags & ActionFlags::Indexed))
      {
        // for non-indexed draws, we manually apply the vertex offset, but here after we used the
        // 0-based one to calculate the array slot
        vertexIndexID =
            ops.add(rdcspv::OpIAdd(u32Type, editor.MakeId(), vtxID,
                                   editor.AddConstantImmediate<uint32_t>(action->vertexOffset)));
      }
      editor.SetName(vertexIndexID, "vertexIndex");

      // instIndex = inst + instOffset
      rdcspv::Id instIndexID =
          ops.add(rdcspv::OpIAdd(u32Type, editor.MakeId(), instID,
                                 editor.AddConstantImmediate<uint32_t>(action->instanceOffset)));
      editor.SetName(instIndexID, "instanceIndex");

      rdcspv::Id idxs[64] = {};

      for(size_t i = 0; i < refl.inputSignature.size(); i++)
      {
        VarType vType = refl.inputSignature[i].varType;

        ShaderBuiltin builtin = refl.inputSignature[i].systemValue;
        if(builtin != ShaderBuiltin::Undefined)
        {
          rdcspv::Id valueID;
          CompType compType = CompType::UInt;

          if(builtin == ShaderBuiltin::VertexIndex)
          {
            valueID = vertexIndexID;
            // although for indexed draws we accounted for vertexOffset when looking up fixed
            // function vertex inputs, we still need to apply it to the VertexIndex builtin here.
            if(action->flags & ActionFlags::Indexed)
            {
              valueID = ops.add(
                  rdcspv::OpIAdd(u32Type, editor.MakeId(), valueID,
                                 editor.AddConstantImmediate<uint32_t>(action->vertexOffset)));
            }
          }
          else if(builtin == ShaderBuiltin::InstanceIndex)
          {
            valueID = instIndexID;
          }
          else if(builtin == ShaderBuiltin::MultiViewIndex)
          {
            valueID = viewID;
          }
          else if(builtin == ShaderBuiltin::ViewportIndex)
          {
            valueID = viewID;
          }
          else if(builtin == ShaderBuiltin::BaseVertex)
          {
            if(action->flags & ActionFlags::Indexed)
            {
              valueID = editor.AddConstantImmediate<uint32_t>(action->vertexOffset);
            }
            else
            {
              valueID = editor.AddConstantImmediate<int32_t>(action->baseVertex);
              compType = CompType::SInt;
            }
          }
          else if(builtin == ShaderBuiltin::BaseInstance)
          {
            valueID = editor.AddConstantImmediate<uint32_t>(action->instanceOffset);
          }
          else if(builtin == ShaderBuiltin::DrawIndex)
          {
            valueID = editor.AddConstantImmediate<uint32_t>(action->drawIndex);
          }
          else if(builtin == ShaderBuiltin::SubgroupEqualMask ||
                  builtin == ShaderBuiltin::SubgroupGreaterMask ||
                  builtin == ShaderBuiltin::SubgroupGreaterEqualMask ||
                  builtin == ShaderBuiltin::SubgroupLessMask ||
                  builtin == ShaderBuiltin::SubgroupLessEqualMask ||
                  builtin == ShaderBuiltin::IndexInSubgroup || builtin == ShaderBuiltin::SubgroupSize)
          {
            // subgroup builtins we left alone, these are still builtins
            continue;
          }

          if(valueID)
          {
            rdcspv::Id ptr = ins[i].variable;

            if(!patchData.inputs[i].accessChain.empty())
            {
              // for composite types we need to access chain first
              rdcarray<rdcspv::Id> chain;

              for(uint32_t accessIdx : patchData.inputs[i].accessChain)
              {
                if(idxs[accessIdx] == 0)
                  idxs[accessIdx] = editor.AddConstantImmediate<uint32_t>(accessIdx);

                chain.push_back(idxs[accessIdx]);
              }

              ptr = ops.add(rdcspv::OpAccessChain(ins[i].privatePtrType, editor.MakeId(),
                                                  patchData.inputs[i].ID, chain));
            }

            if(VarTypeCompType(vType) == compType)
            {
              ops.add(rdcspv::OpStore(ptr, valueID));
            }
            else
            {
              // assume we can just bitcast
              rdcspv::Id castedValue =
                  ops.add(rdcspv::OpBitcast(ins[i].baseType, editor.MakeId(), valueID));
              ops.add(rdcspv::OpStore(ptr, castedValue));
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

          uint32_t location = refl.inputSignature[i].regIndex;

          // idx = vertexIndex
          rdcspv::Id idx = vertexLookupID;

          // maybe idx = instanceIndex / someDivisor
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
              rdcspv::Id divisorId = editor.AddConstantImmediate<uint32_t>(divisor);
              idx = ops.add(rdcspv::OpUDiv(u32Type, editor.MakeId(), instanceLookupID, divisorId));
            }
          }

          if(vType == VarType::Double || vType == VarType::ULong || vType == VarType::SLong)
          {
            // since 64-bit vlaues are packed into two uints, we need to multiply the index by two
            idx = ops.add(rdcspv::OpIMul(u32Type, editor.MakeId(), idx,
                                         editor.AddConstantImmediate<uint32_t>(2)));
          }

          rdcspv::Id ptrId;

          // when we're loading from bindings, the vbuffers variable is an array of N structs each
          // containing uvec4[],
          // when we're using buffer device address we have one variable per vbuffer and it's a
          // plain uvec4*

          // uvec4 *vertex = &vbuffers[reg].member0[idx]
          if(storageMode == Binding)
            ptrId =
                ops.add(rdcspv::OpAccessChain(uvec4PtrType, editor.MakeId(), vbuffersVariable,
                                              {idxs[refl.inputSignature[i].regIndex], zero, idx}));
          else
            // uvec4 *vertex = &vbufferN.member0[idx]
            ptrId = ops.add(rdcspv::OpAccessChain(uvec4PtrType, editor.MakeId(),
                                                  vbufferVariables[refl.inputSignature[i].regIndex],
                                                  {zero, idx}));

          // uvec4 result = *vertex
          rdcspv::Id result =
              ops.add(rdcspv::OpLoad(uvec4Type, editor.MakeId(), ptrId, memoryAccess));

          // if we want this as ivec4 or vec4, bitcast now
          if(ins[i].fetchVec4Type != uvec4Type)
            result = ops.add(rdcspv::OpBitcast(ins[i].fetchVec4Type, editor.MakeId(), result));

          // we always fetch as full 32-bit values, but if the input was declared as a different
          // size (typically ushort or half) then convert here
          if(ins[i].fetchVec4Type != ins[i].vec4Type)
          {
            if(VarTypeCompType(vType) == CompType::Float)
              result = ops.add(rdcspv::OpFConvert(ins[i].vec4Type, editor.MakeId(), result));
            else if(VarTypeCompType(vType) == CompType::UInt)
              result = ops.add(rdcspv::OpUConvert(ins[i].vec4Type, editor.MakeId(), result));
            else
              result = ops.add(rdcspv::OpSConvert(ins[i].vec4Type, editor.MakeId(), result));
          }

          uint32_t comp = Bits::CountTrailingZeroes(uint32_t(refl.inputSignature[i].regChannelMask));

          if(vType == VarType::Double || vType == VarType::ULong || vType == VarType::SLong)
          {
            // since 64-bit values are packed into two uints, we now need to fetch more data and do
            // packing. We can fetch the data unconditionally since it's harmless to read out of the
            // bounds of the buffer

            rdcspv::Id nextidx = ops.add(rdcspv::OpIAdd(u32Type, editor.MakeId(), idx,
                                                        editor.AddConstantImmediate<uint32_t>(1)));

            // uvec4 *vertex = &vbuffers[reg].member0[nextidx]
            if(storageMode == Binding)
              ptrId = ops.add(
                  rdcspv::OpAccessChain(uvec4PtrType, editor.MakeId(), vbuffersVariable,
                                        {idxs[refl.inputSignature[i].regIndex], zero, nextidx}));
            else
              // uvec4 *vertex = &vbufferN.member0[nextidx]
              ptrId = ops.add(rdcspv::OpAccessChain(
                  uvec4PtrType, editor.MakeId(), vbufferVariables[refl.inputSignature[i].regIndex],
                  {zero, nextidx}));
            rdcspv::Id result2 =
                ops.add(rdcspv::OpLoad(uvec4Type, editor.MakeId(), ptrId, memoryAccess));

            rdcspv::Id glsl450 = editor.ImportExtInst("GLSL.std.450");

            rdcspv::Id uvec2Type = editor.DeclareType(rdcspv::Vector(rdcspv::scalar<uint32_t>(), 2));
            rdcspv::Id comps[4] = {};

            for(uint32_t c = 0; c < refl.inputSignature[i].compCount; c++)
            {
              // first extract the uvec2 we want

              // uvec2 packed = result.[xy/zw] / result2.[xy/zw];
              rdcspv::Id packed = ops.add(rdcspv::OpVectorShuffle(
                  uvec2Type, editor.MakeId(), result, result2, {c * 2 + 0, c * 2 + 1}));

              char swizzle[] = "xyzw";

              editor.SetName(packed, StringFormat::Fmt("packed_%c", swizzle[c]));

              if(vType == VarType::Double)
              {
                // double comp = PackDouble2x32(packed);
                comps[c] = ops.add(rdcspv::OpGLSL450(editor.DeclareType(rdcspv::scalar<double>()),
                                                     editor.MakeId(), glsl450,
                                                     rdcspv::GLSLstd450::PackDouble2x32, {packed}));
              }
              else
              {
                rdcspv::Scalar s = (vType == VarType::ULong) ? rdcspv::scalar<uint64_t>()
                                                             : rdcspv::scalar<int64_t>();

                // [u]int64 comp = Bitcast(packed);
                comps[c] = ops.add(rdcspv::OpBitcast(editor.DeclareType(s), editor.MakeId(), packed));
              }
            }

            // if there's only one component it's ready, otherwise construct a vector
            if(refl.inputSignature[i].compCount == 1)
            {
              result = comps[0];
            }
            else
            {
              rdcarray<rdcspv::Id> ids;

              for(uint32_t c = 0; c < refl.inputSignature[i].compCount; c++)
                ids.push_back(comps[c]);

              // baseTypeN value = result.xyz;
              result = ops.add(rdcspv::OpCompositeConstruct(ins[i].baseType, editor.MakeId(), ids));
            }
          }
          else if(refl.inputSignature[i].compCount == 1)
          {
            // for one component, extract x

            // baseType value = result.x;
            result =
                ops.add(rdcspv::OpCompositeExtract(ins[i].baseType, editor.MakeId(), result, {comp}));
          }
          else if(refl.inputSignature[i].compCount != 4)
          {
            // for less than 4 components, extract the sub-vector

            rdcarray<uint32_t> swizzle;

            for(uint32_t c = 0; c < refl.inputSignature[i].compCount; c++)
              swizzle.push_back(c + comp);

            // baseTypeN value = result.xyz;
            result = ops.add(
                rdcspv::OpVectorShuffle(ins[i].baseType, editor.MakeId(), result, result, swizzle));
          }

          // copy the 4 component result directly

          // not a composite type, we can store directly
          if(patchData.inputs[i].accessChain.empty())
          {
            // *global = value
            ops.add(rdcspv::OpStore(ins[i].variable, result));
          }
          else
          {
            // for composite types we need to access chain first
            rdcarray<rdcspv::Id> chain;

            for(uint32_t accessIdx : patchData.inputs[i].accessChain)
            {
              if(idxs[accessIdx] == 0)
                idxs[accessIdx] = editor.AddConstantImmediate<uint32_t>(accessIdx);

              chain.push_back(idxs[accessIdx]);
            }

            rdcspv::Id subElement = ops.add(rdcspv::OpAccessChain(
                ins[i].privatePtrType, editor.MakeId(), patchData.inputs[i].ID, chain));

            ops.add(rdcspv::OpStore(subElement, result));
          }
        }
      }

      // real_main();
      ops.add(rdcspv::OpFunctionCall(voidType, editor.MakeId(), entryID));

      for(uint32_t o = 0; o < numOutputs; o++)
      {
        rdcspv::Id loaded;

        // not a structure member or array child, can load directly
        if(patchData.outputs[o].accessChain.empty())
        {
          // type loaded = *globalvar;
          loaded =
              ops.add(rdcspv::OpLoad(outs[o].baseType, editor.MakeId(), patchData.outputs[o].ID));
        }
        else
        {
          // structure member, need to access chain first
          rdcarray<rdcspv::Id> chain;

          for(uint32_t idx : patchData.outputs[o].accessChain)
          {
            if(idxs[idx] == 0)
              idxs[idx] = editor.AddConstantImmediate<uint32_t>(idx);

            chain.push_back(idxs[idx]);
          }

          // type *readPtr = globalvar.globalsub...;
          rdcspv::Id readPtr = ops.add(rdcspv::OpAccessChain(
              outs[o].privatePtrType, editor.MakeId(), patchData.outputs[o].ID, chain));
          // type loaded = *readPtr;
          loaded = ops.add(rdcspv::OpLoad(outs[o].baseType, editor.MakeId(), readPtr));
        }

        // access chain the destination
        rdcspv::Id writePtr;

        // type *writePtr = &outBuffer.verts[arraySlot].outputN
        writePtr = ops.add(rdcspv::OpAccessChain(outs[o].ssboPtrType, editor.MakeId(), outBufferVarID,
                                                 {zero, arraySlotID, outs[o].indexConst}));

        // *writePtr = loaded;
        ops.add(rdcspv::OpStore(writePtr, loaded, memoryAccess));
      }

      // goto killLabel;
      ops.add(rdcspv::OpBranch(killLabel));

      // killLabel:
      ops.add(rdcspv::OpLabel(killLabel));
    }
    ops.add(rdcspv::OpReturn());

    ops.add(rdcspv::OpFunctionEnd());

    editor.AddFunction(ops);
  }
}

struct OutSigLocation
{
  uint32_t offset;
  uint32_t stride;
};

struct OutMeshletLayout
{
  rdcarray<OutSigLocation> sigLocations;
  uint32_t meshletByteSize;
  uint32_t indexCountPerPrim;
  uint32_t vertArrayLength;
  uint32_t primArrayLength;
};

static void LayOutStorageStruct(rdcspv::Editor &editor, const rdcarray<SpecConstant> &specInfo,
                                rdcspv::SparseIdMap<rdcspv::Id> &outputTypeReplacements,
                                const rdcspv::DataType &type, rdcspv::Id &structType,
                                uint32_t &byteSize)
{
  rdcarray<rdcspv::Id> members;

  structType = editor.MakeId();
  outputTypeReplacements[type.id] = structType;
  editor.SetName(structType, StringFormat::Fmt("layoutStruct%d", type.id.value()));

  uint32_t offset = 0;
  rdcarray<uint32_t> offsets;

  for(uint32_t i = 0; i < type.children.size(); i++)
  {
    rdcspv::Id memberTypeId = type.children[i].type;

    // port across any decorations that should remain from the existing struct
    if(type.children[i].decorations.others.contains(rdcspv::Decoration::Invariant))
      editor.AddDecoration(rdcspv::OpMemberDecorate(structType, i, rdcspv::Decoration::Invariant));

    uint32_t size = 1;
    const rdcspv::DataType &childType = editor.GetDataType(type.children[i].type);

    if(childType.type == rdcspv::DataType::ArrayType)
      memberTypeId = childType.InnerType();

    if(childType.type == rdcspv::DataType::StructType)
    {
      offset = AlignUp16(offset);
      LayOutStorageStruct(editor, specInfo, outputTypeReplacements, childType, memberTypeId, size);
    }
    else if(childType.type == rdcspv::DataType::ArrayType &&
            editor.GetDataType(childType.InnerType()).type == rdcspv::DataType::StructType)
    {
      offset = AlignUp16(offset);
      LayOutStorageStruct(editor, specInfo, outputTypeReplacements,
                          editor.GetDataType(childType.InnerType()), memberTypeId, size);
    }
    else if(childType.type == rdcspv::DataType::ArrayType)
    {
      const rdcspv::DataType &arrayInnerType = editor.GetDataType(childType.InnerType());
      size = VarTypeByteSize(arrayInnerType.scalar().Type());
      offset = AlignUp(offset, size);
      if(arrayInnerType.type == rdcspv::DataType::VectorType)
        size *= arrayInnerType.vector().count;
    }
    else
    {
      size = VarTypeByteSize(childType.scalar().Type());
      offset = AlignUp(offset, size);
      if(childType.type == rdcspv::DataType::VectorType)
        size *= childType.vector().count;

      if(childType.scalar().type == rdcspv::Op::TypeBool)
        memberTypeId = editor.GetType(rdcspv::scalar<uint32_t>());
    }

    offsets.push_back(offset);

    if(childType.type == rdcspv::DataType::ArrayType)
    {
      // make a new array type so we can decorate it with a stride
      memberTypeId =
          editor.AddType(rdcspv::OpTypeArray(editor.MakeId(), memberTypeId, childType.length));
      outputTypeReplacements[type.children[i].type] = memberTypeId;
      editor.SetName(memberTypeId,
                     StringFormat::Fmt("stridedArray%d", type.children[i].type.value()));

      editor.AddDecoration(rdcspv::OpDecorate(
          memberTypeId, rdcspv::DecorationParam<rdcspv::Decoration::ArrayStride>(size)));

      offset += size * editor.EvaluateConstant(childType.length, specInfo).value.u32v[0];
    }
    else
    {
      offset += size;
    }

    members.push_back(memberTypeId);
  }

  editor.AddType(rdcspv::OpTypeStruct(structType, members));

  for(uint32_t i = 0; i < offsets.size(); i++)
    editor.AddDecoration(rdcspv::OpMemberDecorate(
        structType, i, rdcspv::DecorationParam<rdcspv::Decoration::Offset>(offsets[i])));

  byteSize = AlignUp16(offset);
}

static void AddTaskShaderPayloadStores(const rdcarray<SpecConstant> &specInfo,
                                       const rdcstr &entryName, uint32_t outSpecConstant,
                                       rdcarray<uint32_t> &modSpirv, uint32_t &payloadSize)
{
  rdcspv::Editor editor(modSpirv);

  editor.Prepare();

  rdcspv::Id boolType = editor.DeclareType(rdcspv::scalar<bool>());
  rdcspv::Id uint32Type = editor.DeclareType(rdcspv::scalar<uint32_t>());
  rdcspv::Id uvec3Type = editor.DeclareType(rdcspv::Vector(rdcspv::scalar<uint32_t>(), 3));
  rdcspv::Id uvec3PtrType =
      editor.DeclareType(rdcspv::Pointer(uvec3Type, rdcspv::StorageClass::PhysicalStorageBuffer));
  rdcspv::Id uint64Type = editor.DeclareType(rdcspv::scalar<uint64_t>());

  rdcspv::Id entryID;

  for(const rdcspv::EntryPoint &entry : editor.GetEntries())
  {
    if(entry.name == entryName && entry.executionModel == rdcspv::ExecutionModel::TaskEXT)
      entryID = entry.id;
  }

  RDCASSERT(entryID);

  rdcspv::Id payloadId;
  rdcspv::Id payloadTaskStructType;
  rdcspv::Id payloadBlockStructType;

  rdcspv::Id baseAddrId;
  rdcspv::Id outSlotAddr;

  {
    rdcspv::Id uint64ptrtype =
        editor.DeclareType(rdcspv::Pointer(uint64Type, rdcspv::StorageClass::Private));

    outSlotAddr = editor.AddVariable(
        rdcspv::OpVariable(uint64ptrtype, editor.MakeId(), rdcspv::StorageClass::Private));
    editor.SetName(outSlotAddr, "outSlot");
  }

  // set up BDA if it's not already used
  {
    editor.AddExtension("SPV_KHR_physical_storage_buffer");

    rdcspv::Iter it = editor.Begin(rdcspv::Section::MemoryModel);
    rdcspv::OpMemoryModel model(it);
    model.addressingModel = rdcspv::AddressingModel::PhysicalStorageBuffer64;
    it = model;

    editor.AddCapability(rdcspv::Capability::PhysicalStorageBufferAddresses);
    editor.AddCapability(rdcspv::Capability::Int64);

    baseAddrId = editor.AddSpecConstantImmediate<uint64_t>(0U, outSpecConstant);
    editor.SetName(baseAddrId, "baseAddr");
  }

  {
    rdcspv::Iter it = editor.GetEntry(entryID);

    RDCASSERT(it.opcode() == rdcspv::Op::EntryPoint);

    rdcspv::OpEntryPoint entry(it);

    for(rdcspv::Id id : entry.iface)
    {
      const rdcspv::DataType &type = editor.GetDataType(editor.GetIDType(id));

      if(type.type == rdcspv::DataType::PointerType &&
         type.pointerType.storage == rdcspv::StorageClass::TaskPayloadWorkgroupEXT)
      {
        payloadId = id;

        payloadBlockStructType = payloadTaskStructType = type.InnerType();
        rdcspv::SparseIdMap<rdcspv::Id> outputTypeReplacements;
        LayOutStorageStruct(editor, specInfo, outputTypeReplacements,
                            editor.GetDataType(payloadBlockStructType), payloadBlockStructType,
                            payloadSize);
        break;
      }
    }
  }

  rdcspv::Id payloadBDAPtrType;

  if(payloadBlockStructType != rdcspv::Id())
  {
    payloadBDAPtrType = editor.DeclareType(
        rdcspv::Pointer(payloadBlockStructType, rdcspv::StorageClass::PhysicalStorageBuffer));
  }

  rdcarray<rdcspv::Id> newGlobals;

  newGlobals.push_back(outSlotAddr);

  // ensure the local index variable is declared
  {
    rdcspv::OperationList ops;
    rdcspv::Id threadIndex, newGlobal;
    rdctie(threadIndex, newGlobal) = editor.AddBuiltinInputLoad(
        ops, ShaderStage::Mesh, rdcspv::BuiltIn::LocalInvocationIndex, uint32Type);
    if(newGlobal != rdcspv::Id())
      newGlobals.push_back(newGlobal);
  }

  // calculate base address for our task group's data
  {
    rdcspv::OperationList locationCalculate;

    {
      rdcspv::Id uint3Type = editor.DeclareType(rdcspv::Vector(rdcspv::scalar<uint32_t>(), 3));
      rdcspv::Id groupIdx, dispatchSize, newGlobal;

      rdctie(groupIdx, newGlobal) = editor.AddBuiltinInputLoad(
          locationCalculate, ShaderStage::Mesh, rdcspv::BuiltIn::WorkgroupId, uint3Type);
      if(newGlobal != rdcspv::Id())
        newGlobals.push_back(newGlobal);
      rdctie(dispatchSize, newGlobal) = editor.AddBuiltinInputLoad(
          locationCalculate, ShaderStage::Mesh, rdcspv::BuiltIn::NumWorkgroups, uint3Type);
      if(newGlobal != rdcspv::Id())
        newGlobals.push_back(newGlobal);

      // x + y * xsize + z * xsize * ysize

      rdcspv::Id xsize = locationCalculate.add(
          rdcspv::OpCompositeExtract(uint32Type, editor.MakeId(), dispatchSize, {0}));
      rdcspv::Id ysize = locationCalculate.add(
          rdcspv::OpCompositeExtract(uint32Type, editor.MakeId(), dispatchSize, {1}));

      rdcspv::Id xflat = locationCalculate.add(
          rdcspv::OpCompositeExtract(uint32Type, editor.MakeId(), groupIdx, {0}));
      rdcspv::Id yflat = locationCalculate.add(
          rdcspv::OpCompositeExtract(uint32Type, editor.MakeId(), groupIdx, {1}));
      rdcspv::Id zflat = locationCalculate.add(
          rdcspv::OpCompositeExtract(uint32Type, editor.MakeId(), groupIdx, {2}));

      rdcspv::Id xysize =
          locationCalculate.add(rdcspv::OpIMul(uint32Type, editor.MakeId(), xsize, ysize));

      yflat = locationCalculate.add(rdcspv::OpIMul(uint32Type, editor.MakeId(), yflat, xsize));
      zflat = locationCalculate.add(rdcspv::OpIMul(uint32Type, editor.MakeId(), zflat, xysize));

      rdcspv::Id flatIndex =
          locationCalculate.add(rdcspv::OpIAdd(uint32Type, editor.MakeId(), xflat, yflat));
      flatIndex =
          locationCalculate.add(rdcspv::OpIAdd(uint32Type, editor.MakeId(), flatIndex, zflat));

      rdcspv::Id total_stride = editor.AddConstantImmediate<uint64_t>(payloadSize + sizeof(Vec4u));

      rdcspv::Id idx64 =
          locationCalculate.add(rdcspv::OpUConvert(uint64Type, editor.MakeId(), flatIndex));

      rdcspv::Id offset =
          locationCalculate.add(rdcspv::OpIMul(uint64Type, editor.MakeId(), total_stride, idx64));

      rdcspv::Id addr =
          locationCalculate.add(rdcspv::OpIAdd(uint64Type, editor.MakeId(), baseAddrId, offset));

      locationCalculate.add(rdcspv::OpStore(outSlotAddr, addr));
    }

    rdcspv::Iter it = editor.GetID(entryID);
    RDCASSERT(it.opcode() == rdcspv::Op::Function);
    ++it;

    // continue to the first label so we can insert things at the start of the entry point
    for(; it; ++it)
    {
      if(it.opcode() == rdcspv::Op::Label)
      {
        ++it;
        break;
      }
    }

    // skip past any local variables
    while(it.opcode() == rdcspv::Op::Variable || it.opcode() == rdcspv::Op::Line ||
          it.opcode() == rdcspv::Op::NoLine)
      ++it;

    editor.AddOperations(it, locationCalculate);
  }

  // add the globals we registered
  {
    rdcspv::Iter it = editor.GetEntry(entryID);

    RDCASSERT(it.opcode() == rdcspv::Op::EntryPoint);

    rdcspv::OpEntryPoint entry(it);

    editor.Remove(it);

    entry.iface.append(newGlobals);

    editor.AddOperation(it, entry);
  }

  rdcspv::Id zeroU32 = editor.AddConstantImmediate<uint32_t>(0U);
  rdcspv::Id workgroupScope =
      editor.AddConstantImmediate<uint32_t>((uint32_t)rdcspv::Scope::Workgroup);
  rdcspv::Id acqRelWorkgroupSem = editor.AddConstantImmediate<uint32_t>(
      (uint32_t)(rdcspv::MemorySemantics::WorkgroupMemory | rdcspv::MemorySemantics::AcquireRelease));
  rdcspv::Id sixteenU64 = editor.AddConstantImmediate<uint64_t>(16);

  for(rdcspv::Iter it = editor.Begin(rdcspv::Section::Functions);
      it < editor.End(rdcspv::Section::Functions); ++it)
  {
    if(it.opcode() == rdcspv::Op::EmitMeshTasksEXT)
    {
      rdcspv::OpEmitMeshTasksEXT emit(it);
      // only patch emits to our payload. Other shaders may reference other payloads
      if(emit.payload == payloadId)
      {
        rdcspv::OperationList ops;

        // insert a barrier first before the emit
        ops.add(rdcspv::OpControlBarrier(workgroupScope, workgroupScope, acqRelWorkgroupSem));

        rdcspv::Id threadIndex, newGlobal;
        rdctie(threadIndex, newGlobal) = editor.AddBuiltinInputLoad(
            ops, ShaderStage::Mesh, rdcspv::BuiltIn::LocalInvocationIndex, uint32Type);

        rdcspv::Id threadIndexIsZero =
            ops.add(rdcspv::OpIEqual(boolType, editor.MakeId(), threadIndex, zeroU32));

        rdcspv::Id mergeLabel = editor.MakeId();
        rdcspv::Id writeCase = editor.MakeId();
        ops.add(rdcspv::OpSelectionMerge(mergeLabel, rdcspv::SelectionControl::None));
        ops.add(rdcspv::OpBranchConditional(threadIndexIsZero, writeCase, mergeLabel));
        ops.add(rdcspv::OpLabel(writeCase));

        rdcspv::Id sizeAddr = ops.add(rdcspv::OpLoad(uint64Type, editor.MakeId(), outSlotAddr));

        rdcspv::Id ptr = ops.add(rdcspv::OpConvertUToPtr(uvec3PtrType, editor.MakeId(), sizeAddr));

        rdcspv::MemoryAccessAndParamDatas memoryAccess;
        memoryAccess.setAligned(sizeof(uint32_t));

        rdcspv::Id vals = ops.add(rdcspv::OpCompositeConstruct(
            uvec3Type, editor.MakeId(), {emit.groupCountX, emit.groupCountY, emit.groupCountZ}));
        ops.add(rdcspv::OpStore(ptr, vals, memoryAccess));

        if(emit.HasPayload())
        {
          rdcspv::Id payloadAddr =
              ops.add(rdcspv::OpIAdd(uint64Type, editor.MakeId(), sizeAddr, sixteenU64));

          ptr = ops.add(rdcspv::OpConvertUToPtr(payloadBDAPtrType, editor.MakeId(), payloadAddr));

          rdcspv::Id payloadStruct =
              ops.add(rdcspv::OpLoad(payloadTaskStructType, editor.MakeId(), emit.payload));
          rdcspv::Id logicalledPayload =
              ops.add(rdcspv::OpCopyLogical(payloadBlockStructType, editor.MakeId(), payloadStruct));
          ops.add(rdcspv::OpStore(ptr, logicalledPayload, memoryAccess));
        }

        ops.add(rdcspv::OpBranch(mergeLabel));
        ops.add(rdcspv::OpLabel(mergeLabel));

        it = editor.AddOperations(it, ops);

        // don't do any actual emitting
        emit.groupCountX = zeroU32;
        emit.groupCountY = zeroU32;
        emit.groupCountZ = zeroU32;

        editor.PreModify(it);
        it = emit;
        editor.PostModify(it);
      }
    }
  }
}

static void ConvertToFixedTaskFeeder(const rdcarray<SpecConstant> &specInfo,
                                     const rdcstr &entryName, uint32_t inSpecConstant,
                                     uint32_t payloadSize, rdcarray<uint32_t> &modSpirv)
{
  rdcspv::Editor editor(modSpirv);

  editor.Prepare();

  // remove all debug names that exist currently as they may name instructions we're going to remove
  for(rdcspv::Iter it = editor.Begin(rdcspv::Section::DebugNames),
                   end2 = editor.End(rdcspv::Section::DebugNames);
      it < end2; ++it)
  {
    editor.Remove(it);
  }

  rdcspv::Id uint32Type = editor.DeclareType(rdcspv::scalar<uint32_t>());
  rdcspv::Id uvec4Type = editor.DeclareType(rdcspv::Vector(rdcspv::scalar<uint32_t>(), 4));
  rdcspv::Id uvec4PtrType =
      editor.DeclareType(rdcspv::Pointer(uvec4Type, rdcspv::StorageClass::PhysicalStorageBuffer));
  rdcspv::Id uint64Type = editor.DeclareType(rdcspv::scalar<uint64_t>());

  rdcspv::Id baseAddrId;

  // set up BDA if it's not already used
  {
    editor.AddExtension("SPV_KHR_physical_storage_buffer");

    rdcspv::Iter it = editor.Begin(rdcspv::Section::MemoryModel);
    rdcspv::OpMemoryModel model(it);
    model.addressingModel = rdcspv::AddressingModel::PhysicalStorageBuffer64;
    it = model;

    editor.AddCapability(rdcspv::Capability::PhysicalStorageBufferAddresses);
    editor.AddCapability(rdcspv::Capability::Int64);

    baseAddrId = editor.AddSpecConstantImmediate<uint64_t>(0U, inSpecConstant);
    editor.SetName(baseAddrId, "baseAddr");
  }

  rdcarray<rdcspv::Id> newGlobals;

  rdcspv::Id entryID;

  for(const rdcspv::EntryPoint &entry : editor.GetEntries())
  {
    if(entry.name == entryName && entry.executionModel == rdcspv::ExecutionModel::TaskEXT)
      entryID = entry.id;
  }

  RDCASSERT(entryID);

  rdcspv::Id payloadId;
  rdcspv::Id payloadTaskStructType;
  rdcspv::Id payloadBlockStructType;
  uint32_t taskOffsetIndex = 0;

  rdcspv::Id func;

  {
    rdcspv::Iter it = editor.GetEntry(entryID);

    RDCASSERT(it.opcode() == rdcspv::Op::EntryPoint);

    rdcspv::OpEntryPoint entry(it);

    func = entry.entryPoint;

    for(rdcspv::Id id : entry.iface)
    {
      const rdcspv::DataType &type = editor.GetDataType(editor.GetIDType(id));

      if(type.type == rdcspv::DataType::PointerType &&
         type.pointerType.storage == rdcspv::StorageClass::TaskPayloadWorkgroupEXT)
      {
        payloadId = id;

        payloadBlockStructType = payloadTaskStructType = type.InnerType();

        // append the uint offset to the payload struct type. This should not interfere with any
        // other definitions used anywhere else
        {
          it = editor.GetID(payloadTaskStructType);

          rdcspv::OpTypeStruct structType(it);
          taskOffsetIndex = (uint32_t)structType.members.size();
          structType.members.push_back(uint32Type);

          // this is a bit of a hack, we use AddOperation to ensure the struct is in the same order
          // rather than AddType which adds it at the end of the types
          editor.Remove(it);
          editor.AddOperation(it, structType);
          editor.PostModify(it);
        }

        uint32_t byteSize = 0;
        rdcspv::SparseIdMap<rdcspv::Id> outputTypeReplacements;
        LayOutStorageStruct(editor, specInfo, outputTypeReplacements,
                            editor.GetDataType(payloadBlockStructType), payloadBlockStructType,
                            byteSize);

        break;
      }
    }
  }

  // if there was no payload, create our own with just the offset
  if(payloadSize == 0)
  {
    payloadTaskStructType = editor.AddType(rdcspv::OpTypeStruct(editor.MakeId(), {uint32Type}));
    payloadBlockStructType = editor.AddType(rdcspv::OpTypeStruct(editor.MakeId(), {uint32Type}));
    editor.AddDecoration(rdcspv::OpMemberDecorate(
        payloadBlockStructType, 0, rdcspv::DecorationParam<rdcspv::Decoration::Offset>(0)));

    rdcspv::Id taskPtrType = editor.DeclareType(
        rdcspv::Pointer(payloadTaskStructType, rdcspv::StorageClass::TaskPayloadWorkgroupEXT));

    payloadId = editor.AddVariable(rdcspv::OpVariable(
        taskPtrType, editor.MakeId(), rdcspv::StorageClass::TaskPayloadWorkgroupEXT));

    newGlobals.push_back(payloadId);
  }

  rdcspv::Id payloadBDAPtrType = editor.DeclareType(
      rdcspv::Pointer(payloadBlockStructType, rdcspv::StorageClass::PhysicalStorageBuffer));

  // find the group size execution mode and remove it, we'll insert our own that's 1,1,1.
  // we remove this in case it's an ExecutionModeId, in which case it would need to expand to be a
  // plain ExecutionMode
  for(rdcspv::Iter it = editor.Begin(rdcspv::Section::ExecutionMode),
                   end = editor.End(rdcspv::Section::ExecutionMode);
      it < end; ++it)
  {
    // this can also handle ExecutionModeId and we don't care about the difference
    rdcspv::OpExecutionMode execMode(it);

    if(execMode.entryPoint == entryID && (execMode.mode == rdcspv::ExecutionMode::LocalSize ||
                                          execMode.mode == rdcspv::ExecutionMode::LocalSizeId))
    {
      editor.Remove(it);
      break;
    }
  }

  // Add our own localsize execution mode
  editor.AddExecutionMode(rdcspv::OpExecutionMode(
      entryID, rdcspv::ExecutionModeParam<rdcspv::ExecutionMode::LocalSize>(1, 1, 1)));

  rdcspv::Id sixteenU64 = editor.AddConstantImmediate<uint64_t>(16);

  rdcspv::OperationList ops;

  rdcspv::MemoryAccessAndParamDatas memoryAccess;
  memoryAccess.setAligned(sizeof(uint32_t));

  // create our new function to read the payload, count, and offset, and emit mesh tasks for it
  {
    rdcspv::Id uint3Type = editor.DeclareType(rdcspv::Vector(rdcspv::scalar<uint32_t>(), 3));
    rdcspv::Id groupIdx, dispatchSize, newGlobal;

    rdctie(groupIdx, newGlobal) =
        editor.AddBuiltinInputLoad(ops, ShaderStage::Mesh, rdcspv::BuiltIn::WorkgroupId, uint3Type);
    if(newGlobal != rdcspv::Id())
      newGlobals.push_back(newGlobal);
    rdctie(dispatchSize, newGlobal) = editor.AddBuiltinInputLoad(
        ops, ShaderStage::Mesh, rdcspv::BuiltIn::NumWorkgroups, uint3Type);
    if(newGlobal != rdcspv::Id())
      newGlobals.push_back(newGlobal);

    // x + y * xsize + z * xsize * ysize

    rdcspv::Id xsize =
        ops.add(rdcspv::OpCompositeExtract(uint32Type, editor.MakeId(), dispatchSize, {0}));
    rdcspv::Id ysize =
        ops.add(rdcspv::OpCompositeExtract(uint32Type, editor.MakeId(), dispatchSize, {1}));

    rdcspv::Id xflat =
        ops.add(rdcspv::OpCompositeExtract(uint32Type, editor.MakeId(), groupIdx, {0}));
    rdcspv::Id yflat =
        ops.add(rdcspv::OpCompositeExtract(uint32Type, editor.MakeId(), groupIdx, {1}));
    rdcspv::Id zflat =
        ops.add(rdcspv::OpCompositeExtract(uint32Type, editor.MakeId(), groupIdx, {2}));

    rdcspv::Id xysize = ops.add(rdcspv::OpIMul(uint32Type, editor.MakeId(), xsize, ysize));

    yflat = ops.add(rdcspv::OpIMul(uint32Type, editor.MakeId(), yflat, xsize));
    zflat = ops.add(rdcspv::OpIMul(uint32Type, editor.MakeId(), zflat, xysize));

    rdcspv::Id flatIndex = ops.add(rdcspv::OpIAdd(uint32Type, editor.MakeId(), xflat, yflat));
    flatIndex = ops.add(rdcspv::OpIAdd(uint32Type, editor.MakeId(), flatIndex, zflat));

    rdcspv::Id total_stride = editor.AddConstantImmediate<uint64_t>(payloadSize + sizeof(Vec4u));

    rdcspv::Id idx64 = ops.add(rdcspv::OpUConvert(uint64Type, editor.MakeId(), flatIndex));

    rdcspv::Id offset = ops.add(rdcspv::OpIMul(uint64Type, editor.MakeId(), total_stride, idx64));

    rdcspv::Id addr = ops.add(rdcspv::OpIAdd(uint64Type, editor.MakeId(), baseAddrId, offset));

    rdcspv::Id ptr = ops.add(rdcspv::OpConvertUToPtr(uvec4PtrType, editor.MakeId(), addr));

    rdcspv::Id sizeOffset = ops.add(rdcspv::OpLoad(uvec4Type, editor.MakeId(), ptr, memoryAccess));

    rdcspv::Id meshDispatchSizeX =
        ops.add(rdcspv::OpCompositeExtract(uint32Type, editor.MakeId(), sizeOffset, {0}));
    rdcspv::Id meshDispatchSizeY =
        ops.add(rdcspv::OpCompositeExtract(uint32Type, editor.MakeId(), sizeOffset, {1}));
    rdcspv::Id meshDispatchSizeZ =
        ops.add(rdcspv::OpCompositeExtract(uint32Type, editor.MakeId(), sizeOffset, {2}));
    offset = ops.add(rdcspv::OpCompositeExtract(uint32Type, editor.MakeId(), sizeOffset, {3}));

    rdcspv::Id patchedPayload;
    if(payloadSize)
    {
      rdcspv::Id payloadAddr = ops.add(rdcspv::OpIAdd(uint64Type, editor.MakeId(), addr, sixteenU64));

      ptr = ops.add(rdcspv::OpConvertUToPtr(payloadBDAPtrType, editor.MakeId(), payloadAddr));

      rdcspv::Id payloadStruct =
          ops.add(rdcspv::OpLoad(payloadBlockStructType, editor.MakeId(), ptr, memoryAccess));
      rdcspv::Id logicalledPayload =
          ops.add(rdcspv::OpCopyLogical(payloadTaskStructType, editor.MakeId(), payloadStruct));
      patchedPayload = ops.add(rdcspv::OpCompositeInsert(
          payloadTaskStructType, editor.MakeId(), offset, logicalledPayload, {taskOffsetIndex}));
    }
    else
    {
      patchedPayload =
          ops.add(rdcspv::OpCompositeConstruct(payloadTaskStructType, editor.MakeId(), {offset}));
    }
    ops.add(rdcspv::OpStore(payloadId, patchedPayload));
    ops.add(rdcspv::OpEmitMeshTasksEXT(meshDispatchSizeX, meshDispatchSizeY, meshDispatchSizeZ,
                                       payloadId));
  }

  {
    rdcspv::Iter it = editor.GetID(func);
    RDCASSERT(it.opcode() == rdcspv::Op::Function);
    ++it;

    // continue to the first label so we can remove and replace the function
    for(; it; ++it)
    {
      if(it.opcode() == rdcspv::Op::Label)
      {
        ++it;
        break;
      }
    }

    // erase the rest of the function
    while(it.opcode() != rdcspv::Op::FunctionEnd)
    {
      editor.Remove(it);
      ++it;
    }

    it = editor.AddOperations(it, ops);
  }

  // remove all decorations that no longer refer to valid IDs (e.g. instructions in functions we deleted).
  for(rdcspv::Iter it = editor.Begin(rdcspv::Section::Annotations),
                   end2 = editor.End(rdcspv::Section::Annotations);
      it < end2; ++it)
  {
    if(it.opcode() == rdcspv::Op::Decorate)
    {
      rdcspv::OpDecorate dec(it);

      if(!editor.GetID(dec.target))
      {
        editor.Remove(it);
      }
    }
    if(it.opcode() == rdcspv::Op::DecorateId)
    {
      rdcspv::OpDecorateId dec(it);

      if(!editor.GetID(dec.target))
      {
        editor.Remove(it);
      }
    }
  }

  // add the globals we registered
  {
    rdcspv::Iter it = editor.GetEntry(entryID);

    RDCASSERT(it.opcode() == rdcspv::Op::EntryPoint);

    rdcspv::OpEntryPoint entry(it);

    editor.Remove(it);

    entry.iface.append(newGlobals);

    editor.AddOperation(it, entry);
  }
}

static void AddMeshShaderOutputStores(const ShaderReflection &refl,
                                      const rdcarray<SpecConstant> &specInfo,
                                      const SPIRVPatchData &patchData, const rdcstr &entryName,
                                      uint32_t outSpecConstant, rdcarray<uint32_t> &modSpirv,
                                      bool readTaskOffset, OutMeshletLayout &layout)
{
  rdcspv::Editor editor(modSpirv);

  editor.Prepare();

  rdcspv::Id baseAddrId;
  rdcspv::Id outSlotAddr;
  rdcspv::Id boolType = editor.DeclareType(rdcspv::scalar<bool>());
  rdcspv::Id uint32Type = editor.DeclareType(rdcspv::scalar<uint32_t>());
  rdcspv::Id uint32PayloadPtrType =
      editor.DeclareType(rdcspv::Pointer(uint32Type, rdcspv::StorageClass::TaskPayloadWorkgroupEXT));
  rdcspv::Id uvec2Type = editor.DeclareType(rdcspv::Vector(rdcspv::scalar<uint32_t>(), 2));
  rdcspv::Id uvec3Type = editor.DeclareType(rdcspv::Vector(rdcspv::scalar<uint32_t>(), 3));
  rdcspv::Id uvec2PtrType =
      editor.DeclareType(rdcspv::Pointer(uvec2Type, rdcspv::StorageClass::PhysicalStorageBuffer));
  rdcspv::Id uint64Type = editor.DeclareType(rdcspv::scalar<uint64_t>());

  rdcspv::Id zeroU32 = editor.AddConstantImmediate<uint32_t>(0);
  rdcspv::Id oneU32 = editor.AddConstantImmediate<uint32_t>(1);
  rdcspv::Id zeroU64 = editor.AddConstantImmediate<uint64_t>(0);
  rdcspv::Id sixteenU64 = editor.AddConstantImmediate<uint64_t>(16);

  {
    rdcspv::Id uint64ptrtype =
        editor.DeclareType(rdcspv::Pointer(uint64Type, rdcspv::StorageClass::Private));

    outSlotAddr = editor.AddVariable(
        rdcspv::OpVariable(uint64ptrtype, editor.MakeId(), rdcspv::StorageClass::Private));
    editor.SetName(outSlotAddr, "outSlot");
  }

  // set up BDA if it's not already used
  {
    editor.AddExtension("SPV_KHR_physical_storage_buffer");

    rdcspv::Iter it = editor.Begin(rdcspv::Section::MemoryModel);
    rdcspv::OpMemoryModel model(it);
    model.addressingModel = rdcspv::AddressingModel::PhysicalStorageBuffer64;
    it = model;

    editor.AddCapability(rdcspv::Capability::PhysicalStorageBufferAddresses);
    editor.AddCapability(rdcspv::Capability::Int64);

    baseAddrId = editor.AddSpecConstantImmediate<uint64_t>(0U, outSpecConstant);
    editor.SetName(baseAddrId, "baseAddr");
  }

  rdcarray<rdcspv::Id> newGlobals;

  newGlobals.push_back(outSlotAddr);

  rdcspv::Id indextype;
  uint32_t indexCount = 3;
  for(const SigParameter &sig : refl.outputSignature)
  {
    if(sig.systemValue == ShaderBuiltin::OutputIndices)
    {
      indexCount = sig.compCount;
      indextype = editor.DeclareType(rdcspv::Vector(rdcspv::scalar<float>(), sig.compCount));
    }
  }

  rdcspv::Id entryID;
  rdcarray<rdcspv::Id> entryInterface;

  for(const rdcspv::EntryPoint &entry : editor.GetEntries())
  {
    if(entry.name == entryName && entry.executionModel == rdcspv::ExecutionModel::MeshEXT)
    {
      entryID = entry.id;
      entryInterface = entry.usedIds;
    }
  }

  RDCASSERT(entryID);

  rdcspv::Id payloadId;
  rdcspv::Id payloadStructId;
  uint32_t taskOffsetIndex = 0;

  if(readTaskOffset)
  {
    rdcspv::Iter it = editor.GetEntry(entryID);

    RDCASSERT(it.opcode() == rdcspv::Op::EntryPoint);

    rdcspv::OpEntryPoint entry(it);

    for(rdcspv::Id id : entry.iface)
    {
      const rdcspv::DataType &type = editor.GetDataType(editor.GetIDType(id));

      if(type.type == rdcspv::DataType::PointerType &&
         type.pointerType.storage == rdcspv::StorageClass::TaskPayloadWorkgroupEXT)
      {
        payloadId = id;
        payloadStructId = type.InnerType();
        break;
      }
    }

    // append the uint offset to the payload struct type. This should not interfere with any other
    // definitions used anywhere else
    if(payloadId != rdcspv::Id())
    {
      it = editor.GetID(payloadStructId);

      rdcspv::OpTypeStruct structType(it);
      taskOffsetIndex = (uint32_t)structType.members.size();
      structType.members.push_back(uint32Type);

      // this is a bit of a hack, we use AddOperation to ensure the struct is in the same order
      // rather than AddType which adds it at the end of the types
      editor.Remove(it);
      editor.AddOperation(it, structType);
      editor.PostModify(it);
    }
    else
    {
      // if there was no payload, create our own with just the offset
      payloadStructId = editor.AddType(rdcspv::OpTypeStruct(editor.MakeId(), {uint32Type}));

      rdcspv::Id taskPtrType = editor.DeclareType(
          rdcspv::Pointer(payloadStructId, rdcspv::StorageClass::TaskPayloadWorkgroupEXT));

      payloadId = editor.AddVariable(rdcspv::OpVariable(
          taskPtrType, editor.MakeId(), rdcspv::StorageClass::TaskPayloadWorkgroupEXT));

      newGlobals.push_back(payloadId);
    }
  }

  uint32_t primOutByteCount = 0;
  uint32_t vertOutByteCount = 0;

  struct OutputGlobal
  {
    uint32_t offset;
    bool perPrim;
    bool indices;
    uint32_t arrayStride;
  };
  rdcspv::SparseIdMap<OutputGlobal> outputGlobals;
  rdcspv::SparseIdMap<rdcspv::Id> outputTypeReplacements;

  // iterate over all output variables and assign locations in the output data stream, as well as
  // creating correctly typed structures (with offsets) and a BDA pointer type to use instead
  // whenever any of these variables are referenced.
  for(const rdcspv::Variable &var : editor.GetGlobals())
  {
    if(var.storage != rdcspv::StorageClass::Output)
      continue;

    // skip variables that aren't for us
    if(!entryInterface.contains(var.id))
      continue;

    const rdcspv::Decorations &d = editor.GetDecorations(var.id);
    // global variables are all pointers
    const rdcspv::DataType &pointerType = editor.GetDataType(editor.GetIDType(var.id));
    RDCASSERT(pointerType.type == rdcspv::DataType::PointerType);

    // in mesh shaders, all output vairables are arrays
    const rdcspv::DataType &arrayType = editor.GetDataType(pointerType.InnerType());

    RDCASSERT(arrayType.type == rdcspv::DataType::ArrayType);
    const rdcspv::DataType &type = editor.GetDataType(arrayType.InnerType());

    uint32_t arrayLength = editor.EvaluateConstant(arrayType.length, specInfo).value.u32v[0];

    rdcspv::Id arrayInnerType = arrayType.InnerType();

    uint32_t byteSize = 1;
    uint32_t stride = 1;

    if(type.type == rdcspv::DataType::StructType)
    {
      LayOutStorageStruct(editor, specInfo, outputTypeReplacements, type, arrayInnerType, byteSize);

      stride = byteSize;

      byteSize *= arrayLength;

      uint32_t offset = 0;
      bool perPrim = false;

      if(d.others.contains(rdcspv::Decoration::PerPrimitiveEXT))
      {
        primOutByteCount = AlignUp16(primOutByteCount);
        offset = primOutByteCount;
        perPrim = true;
        primOutByteCount += byteSize;
      }
      else
      {
        vertOutByteCount = AlignUp16(vertOutByteCount);
        offset = vertOutByteCount;
        perPrim = false;
        vertOutByteCount += byteSize;
      }

      outputGlobals[var.id] = {
          offset,
          perPrim,
          false,
          stride,
      };
    }
    else
    {
      // loose variable
      const uint32_t scalarAlign = VarTypeByteSize(type.scalar().Type());
      byteSize = scalarAlign;
      if(type.type == rdcspv::DataType::VectorType)
        byteSize = scalarAlign * type.vector().count;

      stride = byteSize;

      uint32_t offset = 0;
      bool perPrim = false;
      bool indices = false;

      if(d.builtIn == rdcspv::BuiltIn::PrimitivePointIndicesEXT ||
         d.builtIn == rdcspv::BuiltIn::PrimitiveLineIndicesEXT ||
         d.builtIn == rdcspv::BuiltIn::PrimitiveTriangleIndicesEXT)
      {
        indices = true;
      }
      else if(d.others.contains(rdcspv::Decoration::PerPrimitiveEXT))
      {
        primOutByteCount = AlignUp(primOutByteCount, scalarAlign);
        offset = primOutByteCount;
        perPrim = true;
        primOutByteCount += byteSize * arrayLength;
      }
      else
      {
        vertOutByteCount = AlignUp(vertOutByteCount, scalarAlign);
        offset = vertOutByteCount;
        perPrim = false;
        vertOutByteCount += byteSize * arrayLength;
      }

      outputGlobals[var.id] = {
          offset,
          perPrim,
          indices,
          stride,
      };
    }

    // redeclare the array so we can decorate it with a stride
    rdcspv::Id stridedArrayType =
        editor.AddType(rdcspv::OpTypeArray(editor.MakeId(), arrayInnerType, arrayType.length));
    editor.SetName(stridedArrayType, StringFormat::Fmt("stridedArray%d", arrayType.id.value()));

    editor.AddDecoration(rdcspv::OpDecorate(
        stridedArrayType, rdcspv::DecorationParam<rdcspv::Decoration::ArrayStride>(stride)));

    outputTypeReplacements[arrayType.id] = stridedArrayType;
  }

  // for every output pointer type, declare an equivalent BDA pointer type
  for(rdcspv::Iter it = editor.Begin(rdcspv::Section::Types),
                   end = editor.End(rdcspv::Section::Types);
      it < end; ++it)
  {
    if(it.opcode() == rdcspv::Op::TypePointer)
    {
      rdcspv::OpTypePointer ptr(it);

      if(ptr.storageClass == rdcspv::StorageClass::Output)
      {
        rdcspv::Id inner = ptr.type;
        auto replace = outputTypeReplacements.find(ptr.type);
        if(replace != outputTypeReplacements.end())
          inner = replace->second;

        if(editor.GetDataType(inner).scalar().type == rdcspv::Op::TypeBool)
          inner = editor.GetType(rdcspv::scalar<uint32_t>());

        outputTypeReplacements[ptr.result] =
            editor.DeclareType(rdcspv::Pointer(inner, rdcspv::StorageClass::PhysicalStorageBuffer));
      }
    }
  }

  primOutByteCount = AlignUp16(primOutByteCount);
  vertOutByteCount = AlignUp16(vertOutByteCount);

  for(auto &it : outputGlobals)
  {
    // prim/vert counts
    it.second.offset += 32;

    // indices
    if(!it.second.indices)
      it.second.offset +=
          AlignUp16(patchData.maxPrimitives * indexCount * (uint32_t)sizeof(uint32_t));

    // per-vertex data
    if(it.second.perPrim)
    {
      it.second.offset += vertOutByteCount;
    }
  }

  layout.sigLocations.resize(refl.outputSignature.size());
  for(size_t i = 0; i < refl.outputSignature.size(); i++)
  {
    const SigParameter &sig = refl.outputSignature[i];
    const SPIRVInterfaceAccess &iface = patchData.outputs[i];

    auto glob = outputGlobals.find(iface.ID);

    if(glob == outputGlobals.end())
    {
      RDCERR("Couldn't find global for out signature '%s' (location %u)", sig.varName.c_str(),
             sig.regIndex);
      continue;
    }

    layout.sigLocations[i].offset = glob->second.offset;
    layout.sigLocations[i].stride = glob->second.arrayStride;

    rdcspv::DataType *type = &editor.GetDataType(editor.GetIDType(iface.ID));
    RDCASSERT(type->type == rdcspv::DataType::PointerType);

    type = &editor.GetDataType(type->InnerType());
    RDCASSERT(type->type == rdcspv::DataType::ArrayType);

    rdcspv::Id laidStruct = outputTypeReplacements[type->id];
    RDCASSERT(laidStruct != rdcspv::Id());

    type = &editor.GetDataType(laidStruct);
    RDCASSERT(type->type == rdcspv::DataType::ArrayType);

    // the access chain should always start with a 0 for the array for outputs, this will be
    // effectively skipped below
    rdcarray<uint32_t> memberChain = iface.accessChain;
    while(!memberChain.empty())
    {
      uint32_t memberIdx = memberChain.takeAt(0);

      if(type->type == rdcspv::DataType::ArrayType)
      {
        const rdcspv::Decorations &typeDec = editor.GetDecorations(type->id);
        RDCASSERT(typeDec.flags & rdcspv::Decorations::HasArrayStride);
        layout.sigLocations[i].offset += typeDec.arrayStride * memberIdx;
        type = &editor.GetDataType(type->InnerType());
        continue;
      }

      if(memberIdx >= type->children.size())
      {
        RDCERR(
            "Encountered unexpected child list at type %u looking for member %u for "
            "signature '%s' (location %u)",
            type->id.value(), memberIdx, sig.varName.c_str(), sig.regIndex);
        break;
      }

      RDCASSERT(type->children[memberIdx].decorations.flags & rdcspv::Decorations::HasOffset);
      layout.sigLocations[i].offset += type->children[memberIdx].decorations.offset;
      type = &editor.GetDataType(type->children[memberIdx].type);
    }
  }

  layout.primArrayLength = patchData.maxPrimitives;
  layout.vertArrayLength = patchData.maxVertices;
  layout.indexCountPerPrim = indexCount;
  layout.meshletByteSize =
      // real and fake meshlet size (prim/vert count)
      32 +
      // indices
      (uint32_t)AlignUp16(patchData.maxPrimitives * indexCount * sizeof(uint32_t)) +
      // per-vertex data
      vertOutByteCount +
      // per-primitive data
      primOutByteCount;

  // calculate base address for our meshlet's data
  {
    rdcspv::OperationList locationCalculate;

    {
      rdcspv::Id uint3Type = editor.DeclareType(rdcspv::Vector(rdcspv::scalar<uint32_t>(), 3));
      rdcspv::Id groupIdx, dispatchSize, newGlobal;

      rdctie(groupIdx, newGlobal) = editor.AddBuiltinInputLoad(
          locationCalculate, ShaderStage::Mesh, rdcspv::BuiltIn::WorkgroupId, uint3Type);
      if(newGlobal != rdcspv::Id())
        newGlobals.push_back(newGlobal);
      rdctie(dispatchSize, newGlobal) = editor.AddBuiltinInputLoad(
          locationCalculate, ShaderStage::Mesh, rdcspv::BuiltIn::NumWorkgroups, uint3Type);
      if(newGlobal != rdcspv::Id())
        newGlobals.push_back(newGlobal);

      // x + y * xsize + z * xsize * ysize

      rdcspv::Id xsize = locationCalculate.add(
          rdcspv::OpCompositeExtract(uint32Type, editor.MakeId(), dispatchSize, {0}));
      rdcspv::Id ysize = locationCalculate.add(
          rdcspv::OpCompositeExtract(uint32Type, editor.MakeId(), dispatchSize, {1}));

      rdcspv::Id xflat = locationCalculate.add(
          rdcspv::OpCompositeExtract(uint32Type, editor.MakeId(), groupIdx, {0}));
      rdcspv::Id yflat = locationCalculate.add(
          rdcspv::OpCompositeExtract(uint32Type, editor.MakeId(), groupIdx, {1}));
      rdcspv::Id zflat = locationCalculate.add(
          rdcspv::OpCompositeExtract(uint32Type, editor.MakeId(), groupIdx, {2}));

      rdcspv::Id xysize =
          locationCalculate.add(rdcspv::OpIMul(uint32Type, editor.MakeId(), xsize, ysize));

      yflat = locationCalculate.add(rdcspv::OpIMul(uint32Type, editor.MakeId(), yflat, xsize));
      zflat = locationCalculate.add(rdcspv::OpIMul(uint32Type, editor.MakeId(), zflat, xysize));

      rdcspv::Id flatIndex =
          locationCalculate.add(rdcspv::OpIAdd(uint32Type, editor.MakeId(), xflat, yflat));
      flatIndex =
          locationCalculate.add(rdcspv::OpIAdd(uint32Type, editor.MakeId(), flatIndex, zflat));

      rdcspv::Id total_stride = editor.AddConstantImmediate<uint64_t>(layout.meshletByteSize);

      rdcspv::Id idx64 =
          locationCalculate.add(rdcspv::OpUConvert(uint64Type, editor.MakeId(), flatIndex));

      if(readTaskOffset)
      {
        rdcspv::Id taskOffsetPtr = locationCalculate.add(
            rdcspv::OpAccessChain(uint32PayloadPtrType, editor.MakeId(), payloadId,
                                  {editor.AddConstantImmediate<uint32_t>(taskOffsetIndex)}));
        rdcspv::Id taskOffset =
            locationCalculate.add(rdcspv::OpLoad(uint32Type, editor.MakeId(), taskOffsetPtr));
        taskOffset =
            locationCalculate.add(rdcspv::OpUConvert(uint64Type, editor.MakeId(), taskOffset));
        idx64 = locationCalculate.add(rdcspv::OpIAdd(uint64Type, editor.MakeId(), idx64, taskOffset));
      }

      rdcspv::Id offset =
          locationCalculate.add(rdcspv::OpIMul(uint64Type, editor.MakeId(), total_stride, idx64));

      rdcspv::Id addr =
          locationCalculate.add(rdcspv::OpIAdd(uint64Type, editor.MakeId(), baseAddrId, offset));

      locationCalculate.add(rdcspv::OpStore(outSlotAddr, addr));
    }

    rdcspv::Iter it = editor.GetID(entryID);
    RDCASSERT(it.opcode() == rdcspv::Op::Function);
    ++it;

    // continue to the first label so we can insert things at the start of the entry point
    for(; it; ++it)
    {
      if(it.opcode() == rdcspv::Op::Label)
      {
        ++it;
        break;
      }
    }

    // skip past any local variables
    while(it.opcode() == rdcspv::Op::Variable || it.opcode() == rdcspv::Op::Line ||
          it.opcode() == rdcspv::Op::NoLine)
      ++it;

    editor.AddOperations(it, locationCalculate);
  }

  // ensure the variable is declared
  {
    rdcspv::OperationList ops;
    rdcspv::Id threadIndex, newGlobal;
    rdctie(threadIndex, newGlobal) = editor.AddBuiltinInputLoad(
        ops, ShaderStage::Mesh, rdcspv::BuiltIn::LocalInvocationIndex, uint32Type);
    if(newGlobal != rdcspv::Id())
      newGlobals.push_back(newGlobal);
  }

  // add the globals we registered
  {
    rdcspv::Iter it = editor.GetEntry(entryID);

    RDCASSERT(it.opcode() == rdcspv::Op::EntryPoint);

    rdcspv::OpEntryPoint entry(it);

    editor.Remove(it);

    entry.iface.append(newGlobals);

    editor.AddOperation(it, entry);
  }

  // take every store or access chain to an output pointer and patch it
  // also look for OpSetMeshOutputsEXT which will be called precisely once, and patch it to store
  // the values to our data (and emit 0/0)
  for(rdcspv::Iter it = editor.Begin(rdcspv::Section::Functions);
      it < editor.End(rdcspv::Section::Functions); ++it)
  {
    if(it.opcode() == rdcspv::Op::SetMeshOutputsEXT)
    {
      rdcspv::OpSetMeshOutputsEXT setOuts(it);

      rdcspv::OperationList ops;

      rdcspv::Id threadIndex, newGlobal;
      rdctie(threadIndex, newGlobal) = editor.AddBuiltinInputLoad(
          ops, ShaderStage::Mesh, rdcspv::BuiltIn::LocalInvocationIndex, uint32Type);

      rdcspv::Id threadIndexIsZero =
          ops.add(rdcspv::OpIEqual(boolType, editor.MakeId(), threadIndex, zeroU32));

      // to avoid messing up phi nodes in the application where this is called, we do this
      // branchless by either writing to offset 0 (for threadIndex == 0) or offset 16 (for
      // threadIndex > 0). Then we can ignore the second one
      rdcspv::Id byteOffset = ops.add(
          rdcspv::OpSelect(uint64Type, editor.MakeId(), threadIndexIsZero, zeroU64, sixteenU64));

      rdcspv::Id baseAddr = ops.add(rdcspv::OpLoad(uint64Type, editor.MakeId(), outSlotAddr));

      rdcspv::Id sizeAddr =
          ops.add(rdcspv::OpIAdd(uint64Type, editor.MakeId(), baseAddr, byteOffset));
      rdcspv::Id ptr = ops.add(rdcspv::OpConvertUToPtr(uvec2PtrType, editor.MakeId(), sizeAddr));

      rdcspv::MemoryAccessAndParamDatas memoryAccess;
      memoryAccess.setAligned(sizeof(uint32_t));

      rdcspv::Id vals = ops.add(rdcspv::OpCompositeConstruct(
          uvec2Type, editor.MakeId(), {setOuts.vertexCount, setOuts.primitiveCount}));
      ops.add(rdcspv::OpStore(ptr, vals, memoryAccess));

      it = editor.AddOperations(it, ops);

      setOuts.primitiveCount = zeroU32;
      setOuts.vertexCount = zeroU32;

      editor.PreModify(it);
      it = setOuts;
      editor.PostModify(it);

      continue;
    }

    rdcspv::Id ptr;

    if(it.opcode() == rdcspv::Op::Store)
    {
      rdcspv::OpStore store(it);
      ptr = store.pointer;
    }
    else if(it.opcode() == rdcspv::Op::AccessChain || it.opcode() == rdcspv::Op::InBoundsAccessChain)
    {
      rdcspv::OpAccessChain chain(it);
      chain.op = it.opcode();
      ptr = chain.base;

      const rdcspv::DataType &ptrDataType = editor.GetDataType(chain.resultType);

      // any access chains that produce an output pointer should instead produce a BDA ptr
      if(ptrDataType.pointerType.storage == rdcspv::StorageClass::Output)
      {
        chain.resultType = outputTypeReplacements[chain.resultType];

        editor.PreModify(it);
        it = chain;
        editor.PostModify(it);
      }
    }

    auto glob = outputGlobals.find(ptr);
    if(glob != outputGlobals.end())
    {
      rdcspv::Id baseAddr =
          editor.AddOperation(it, rdcspv::OpLoad(uint64Type, editor.MakeId(), outSlotAddr));
      ++it;
      rdcspv::Id offsettedAddr = editor.AddOperation(
          it, rdcspv::OpIAdd(uint64Type, editor.MakeId(), baseAddr,
                             editor.AddConstantDeferred<uint64_t>(glob->second.offset)));
      ++it;
      ptr = editor.AddOperation(
          it, rdcspv::OpConvertUToPtr(outputTypeReplacements[editor.GetIDType(ptr)],
                                      editor.MakeId(), offsettedAddr));
      ++it;

      if(it.opcode() == rdcspv::Op::Store)
      {
        rdcspv::OpStore store(it);
        store.pointer = ptr;

        editor.PreModify(it);
        it = store;
        editor.PostModify(it);
      }
      else if(it.opcode() == rdcspv::Op::AccessChain || it.opcode() == rdcspv::Op::InBoundsAccessChain)
      {
        rdcspv::OpAccessChain chain(it);
        chain.op = it.opcode();
        chain.base = ptr;

        editor.PreModify(it);
        it = chain;
        editor.PostModify(it);
      }
    }

    if(it.opcode() == rdcspv::Op::Store)
    {
      rdcspv::OpStore store(it);

      const rdcspv::DataType &ptrDataType = editor.GetDataType(editor.GetIDType(ptr));

      // any OpStores to BDA pointers should have suitable alignment defined. Note that this store
      // may not be one we patched above so we do this independently (though in many cases, it will
      // be the one we patched above).
      if(ptrDataType.pointerType.storage == rdcspv::StorageClass::PhysicalStorageBuffer)
      {
        if(editor.GetDataType(editor.GetIDType(store.object)).scalar().type == rdcspv::Op::TypeBool)
        {
          store.object = editor.AddOperation(
              it, rdcspv::OpSelect(uint32Type, editor.MakeId(), store.object, oneU32, zeroU32));
          ++it;
        }

        if(!(store.memoryAccess.flags & rdcspv::MemoryAccess::Aligned))
        {
          const rdcspv::DataType &pointeeDataType = editor.GetDataType(ptrDataType.InnerType());

          // for structs, we align them to 16 bytes, scalar/vector types are aligned to the scalar size
          if(pointeeDataType.scalar().type == rdcspv::Op::Max)
            store.memoryAccess.setAligned(16);
          else
            store.memoryAccess.setAligned(VarTypeByteSize(pointeeDataType.scalar().Type()));

          // remove and re-add as this may be larger than before
          editor.Remove(it);
          editor.AddOperation(it, store);
        }
      }
    }
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

void VulkanReplay::FetchMeshOut(uint32_t eventId, VulkanRenderState &state)
{
  VulkanCreationInfo &creationInfo = m_pDriver->m_CreationInfo;

  ActionDescription action = *m_pDriver->GetAction(eventId);

  // for indirect dispatches, fetch up to date dispatch sizes in case they're non-deterministic
  if(action.flags & ActionFlags::Indirect)
  {
    uint32_t chunkIdx = action.events.back().chunkIndex;

    const SDFile *file = GetStructuredFile();

    // it doesn't matter if this is an indirect sub command or an inlined 1-draw non-indirect count,
    // either way the 'offset' is valid - either from the start, or updated for this particular draw
    // when we originally patched (and fortunately that part doesn't change).
    if(chunkIdx < file->chunks.size())
    {
      const SDChunk *chunk = file->chunks[chunkIdx];

      ResourceId buf = chunk->FindChild("buffer")->AsResourceId();
      uint64_t offs = chunk->FindChild("offset")->AsUInt64();

      buf = GetResourceManager()->GetLiveID(buf);

      bytebuf dispatchArgs;
      GetBufferData(buf, offs, sizeof(VkDrawMeshTasksIndirectCommandEXT), dispatchArgs);

      if(dispatchArgs.size() >= sizeof(VkDrawMeshTasksIndirectCommandEXT))
      {
        VkDrawMeshTasksIndirectCommandEXT *meshArgs =
            (VkDrawMeshTasksIndirectCommandEXT *)dispatchArgs.data();

        action.dispatchDimension[0] = meshArgs->groupCountX;
        action.dispatchDimension[1] = meshArgs->groupCountY;
        action.dispatchDimension[2] = meshArgs->groupCountZ;
      }
    }
  }

  uint32_t totalNumMeshlets =
      action.dispatchDimension[0] * action.dispatchDimension[1] * action.dispatchDimension[2];

  const VulkanCreationInfo::Pipeline &pipeInfo = creationInfo.m_Pipeline[state.graphics.pipeline];

  const VulkanCreationInfo::ShaderObject &meshShadObjInfo =
      creationInfo.m_ShaderObject[state.shaderObjects[(size_t)ShaderStage::Mesh]];

  const VulkanCreationInfo::ShaderEntry &meshShad =
      state.graphics.shaderObject ? meshShadObjInfo.shad
                                  : pipeInfo.shaders[(size_t)ShaderStage::Mesh];

  const VulkanCreationInfo::ShaderModule &meshInfo = creationInfo.m_ShaderModule[meshShad.module];
  ShaderReflection *meshrefl = meshShad.refl;

  VulkanPostVSData &ret = m_PostVS.Data[eventId];

  // set defaults so that we don't try to fetch this output again if something goes wrong and the
  // same event is selected again
  {
    ret.meshout.buf = VK_NULL_HANDLE;
    ret.meshout.bufmem = VK_NULL_HANDLE;
    ret.meshout.instStride = 0;
    ret.meshout.vertStride = 0;
    ret.meshout.numViews = 1;
    ret.meshout.nearPlane = 0.0f;
    ret.meshout.farPlane = 0.0f;
    ret.meshout.useIndices = false;
    ret.meshout.hasPosOut = false;
    ret.meshout.flipY = false;
    ret.meshout.idxbuf = VK_NULL_HANDLE;
    ret.meshout.idxbufmem = VK_NULL_HANDLE;

    ret.meshout.topo = meshShad.refl->outputTopology;

    ret.taskout = ret.meshout;
  }

  if(meshShad.patchData->invalidTaskPayload)
  {
    ret.meshout.status = ret.taskout.status = "Invalid task payload, likely generated by dxc bug";
    return;
  }

  if(meshrefl->outputSignature.empty())
  {
    ret.meshout.status = "mesh shader has no declared outputs";
    return;
  }

  if(!m_pDriver->GetExtensions(NULL).ext_KHR_buffer_device_address ||
     Vulkan_Debug_DisableBufferDeviceAddress())
  {
    ret.meshout.status =
        "KHR_buffer_device_address extension not available, can't fetch mesh shader output";
    return;
  }

  if(!m_pDriver->GetExtensions(NULL).ext_EXT_scalar_block_layout)
  {
    ret.meshout.status =
        "EXT_scalar_block_layout extension not available, can't fetch mesh shader output";
    return;
  }

  if(!m_pDriver->GetDeviceEnabledFeatures().shaderInt64)
  {
    ret.meshout.status = "int64 device feature not available, can't fetch mesh shader output";
    return;
  }

  VkGraphicsPipelineCreateInfo pipeCreateInfo;

  // get pipeline create info
  m_pDriver->GetShaderCache()->MakeGraphicsPipelineInfo(pipeCreateInfo, state.graphics.pipeline);

  // get shader object create info for task/mesh
  VkShaderCreateInfoEXT taskCreateInfo = {}, meshCreateInfo = {};
  if(state.graphics.shaderObject)
  {
    m_pDriver->GetShaderCache()->MakeShaderObjectInfo(
        taskCreateInfo, state.shaderObjects[(size_t)ShaderStage::Task]);
    m_pDriver->GetShaderCache()->MakeShaderObjectInfo(
        meshCreateInfo, state.shaderObjects[(size_t)ShaderStage::Mesh]);
  }

  uint32_t bufSpecConstant = 0;

  bytebuf meshSpecData;
  rdcarray<VkSpecializationMapEntry> meshSpecEntries;
  bytebuf taskSpecData;
  rdcarray<VkSpecializationMapEntry> taskSpecEntries;

  // copy over specialization info
  for(uint32_t s = 0; s < pipeCreateInfo.stageCount; s++)
  {
    if(pipeCreateInfo.pStages[s].stage == VK_SHADER_STAGE_MESH_BIT_EXT &&
       pipeCreateInfo.pStages[s].pSpecializationInfo)
    {
      meshSpecData.append((const byte *)pipeCreateInfo.pStages[s].pSpecializationInfo->pData,
                          pipeCreateInfo.pStages[s].pSpecializationInfo->dataSize);
      meshSpecEntries.append(pipeCreateInfo.pStages[s].pSpecializationInfo->pMapEntries,
                             pipeCreateInfo.pStages[s].pSpecializationInfo->mapEntryCount);
    }
    else if(pipeCreateInfo.pStages[s].stage == VK_SHADER_STAGE_TASK_BIT_EXT &&
            pipeCreateInfo.pStages[s].pSpecializationInfo)
    {
      taskSpecData.append((const byte *)pipeCreateInfo.pStages[s].pSpecializationInfo->pData,
                          pipeCreateInfo.pStages[s].pSpecializationInfo->dataSize);
      taskSpecEntries.append(pipeCreateInfo.pStages[s].pSpecializationInfo->pMapEntries,
                             pipeCreateInfo.pStages[s].pSpecializationInfo->mapEntryCount);
    }
  }

  // copy over specialization info for shader objects
  if(state.graphics.shaderObject)
  {
    if(meshCreateInfo.pSpecializationInfo)
    {
      meshSpecData.append((const byte *)meshCreateInfo.pSpecializationInfo->pData,
                          meshCreateInfo.pSpecializationInfo->dataSize);
      meshSpecEntries.append(meshCreateInfo.pSpecializationInfo->pMapEntries,
                             meshCreateInfo.pSpecializationInfo->mapEntryCount);
    }
    if(taskCreateInfo.pSpecializationInfo)
    {
      taskSpecData.append((const byte *)taskCreateInfo.pSpecializationInfo->pData,
                          taskCreateInfo.pSpecializationInfo->dataSize);
      taskSpecEntries.append(taskCreateInfo.pSpecializationInfo->pMapEntries,
                             taskCreateInfo.pSpecializationInfo->mapEntryCount);
    }
  }

  // don't overlap with existing pipeline constants
  for(const VkSpecializationMapEntry &specConst : meshSpecEntries)
    bufSpecConstant = RDCMAX(bufSpecConstant, specConst.constantID + 1);
  for(const VkSpecializationMapEntry &specConst : taskSpecEntries)
    bufSpecConstant = RDCMAX(bufSpecConstant, specConst.constantID + 1);

  // forcibly set input assembly state to NULL, as AMD's driver still processes this and may crash
  // if the contents are not sensible. Since this does nothing otherwise we don't make it conditional
  pipeCreateInfo.pInputAssemblyState = NULL;

  // use the load RP if an RP is specified
  if(pipeCreateInfo.renderPass != VK_NULL_HANDLE)
  {
    pipeCreateInfo.renderPass =
        creationInfo.m_RenderPass[GetResID(pipeCreateInfo.renderPass)].loadRPs[pipeCreateInfo.subpass];
    pipeCreateInfo.subpass = 0;
  }

  const VkMemoryAllocateFlagsInfo memFlags = {
      VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO,
      NULL,
      VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT,
  };

  // we go through the driver for all these creations since they need to be properly
  // registered in order to be put in the partial replay state
  VkResult vkr = VK_SUCCESS;
  VkDevice dev = m_Device;

  VkBuffer taskBuffer = VK_NULL_HANDLE, readbackTaskBuffer = VK_NULL_HANDLE;
  VkDeviceMemory taskMem = VK_NULL_HANDLE, readbackTaskMem = VK_NULL_HANDLE;

  VkDeviceSize taskBufSize = 0;
  uint32_t taskPayloadSize = 0;
  VkDeviceAddress taskDataAddress = 0;

  rdcarray<VulkanPostVSData::InstData> taskDispatchSizes;
  const uint32_t totalNumTaskGroups = totalNumMeshlets;

  const VulkanCreationInfo::ShaderObject &taskShadObjInfo =
      creationInfo.m_ShaderObject[state.shaderObjects[(size_t)ShaderStage::Task]];

  // if we have a task shader, we fetch both outputs together as a necessary component.
  // In order to properly pre-allocate the mesh output buffer we need to run the task shader, cache
  // all of its payloads and mesh dispatches per-group, then run a dispatch for each task group that
  // passes along the cached payloads. With a CPU sync point this ensures that any non-deterministic
  // behaviour or ordering will remain consistent between both passes and still allow for the
  // allocation after we know the average case. This is necessary because with task expansion the
  // worst case buffer size could be massive
  if(state.graphics.shaderObject ? taskShadObjInfo.shad.refl
                                 : pipeInfo.shaders[(size_t)ShaderStage::Task].refl)
  {
    const VulkanCreationInfo::ShaderEntry &taskShad =
        state.graphics.shaderObject ? taskShadObjInfo.shad
                                    : pipeInfo.shaders[(size_t)ShaderStage::Task];

    if(taskShad.patchData->invalidTaskPayload)
    {
      ret.meshout.status = ret.taskout.status = "Invalid task payload, likely generated by dxc bug";
      return;
    }

    const VulkanCreationInfo::ShaderModule &taskInfo = creationInfo.m_ShaderModule[taskShad.module];

    rdcarray<uint32_t> taskSpirv = taskInfo.spirv.GetSPIRV();

    if(!Vulkan_Debug_PostVSDumpDirPath().empty())
      FileIO::WriteAll(Vulkan_Debug_PostVSDumpDirPath() + "/debug_postts_before.spv", taskSpirv);

    AddTaskShaderPayloadStores(taskShad.specialization, taskShad.entryPoint, bufSpecConstant + 1,
                               taskSpirv, taskPayloadSize);

    if(!Vulkan_Debug_PostVSDumpDirPath().empty())
      FileIO::WriteAll(Vulkan_Debug_PostVSDumpDirPath() + "/debug_postts_after.spv", taskSpirv);

    {
      // now that we know the stride, create buffer of sufficient size for the worst case (maximum
      // generation) of the meshlets
      VkBufferCreateInfo bufInfo = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};

      // we add an extra vec4u so that when feeding from this buffer we can load the oversized
      // payload, read "out of bounds" into that padding with the extra uint offset, and then fix
      // the uint offset with a composite insert
      taskBufSize = bufInfo.size =
          (taskPayloadSize + sizeof(Vec4u)) * totalNumTaskGroups + sizeof(Vec4u);

      bufInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
      bufInfo.usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
      bufInfo.usage |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

      vkr = m_pDriver->vkCreateBuffer(dev, &bufInfo, NULL, &taskBuffer);
      CheckVkResult(vkr);

      bufInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

      vkr = m_pDriver->vkCreateBuffer(dev, &bufInfo, NULL, &readbackTaskBuffer);
      CheckVkResult(vkr);

      VkMemoryRequirements mrq = {0};
      m_pDriver->vkGetBufferMemoryRequirements(dev, taskBuffer, &mrq);

      VkMemoryAllocateInfo allocInfo = {
          VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
          NULL,
          mrq.size,
          m_pDriver->GetGPULocalMemoryIndex(mrq.memoryTypeBits),
      };

      allocInfo.pNext = &memFlags;

      vkr = m_pDriver->vkAllocateMemory(dev, &allocInfo, NULL, &taskMem);

      if(vkr == VK_ERROR_OUT_OF_DEVICE_MEMORY || vkr == VK_ERROR_OUT_OF_HOST_MEMORY)
      {
        m_pDriver->vkDestroyBuffer(m_Device, taskBuffer, NULL);
        m_pDriver->vkDestroyBuffer(m_Device, readbackTaskBuffer, NULL);

        RDCWARN("Failed to allocate %llu bytes for output", mrq.size);
        ret.meshout.status = ret.taskout.status =
            StringFormat::Fmt("Failed to allocate %llu bytes", mrq.size);
        return;
      }

      CheckVkResult(vkr);

      vkr = m_pDriver->vkBindBufferMemory(dev, taskBuffer, taskMem, 0);
      CheckVkResult(vkr);

      m_pDriver->vkGetBufferMemoryRequirements(dev, readbackTaskBuffer, &mrq);

      allocInfo.pNext = NULL;
      allocInfo.memoryTypeIndex = m_pDriver->GetReadbackMemoryIndex(mrq.memoryTypeBits);

      vkr = m_pDriver->vkAllocateMemory(dev, &allocInfo, NULL, &readbackTaskMem);

      if(vkr == VK_ERROR_OUT_OF_DEVICE_MEMORY || vkr == VK_ERROR_OUT_OF_HOST_MEMORY)
      {
        m_pDriver->vkFreeMemory(m_Device, taskMem, NULL);
        m_pDriver->vkDestroyBuffer(m_Device, taskBuffer, NULL);
        m_pDriver->vkDestroyBuffer(m_Device, readbackTaskBuffer, NULL);

        RDCWARN("Failed to allocate %llu bytes for readback", mrq.size);
        ret.meshout.status = ret.taskout.status =
            StringFormat::Fmt("Failed to allocate %llu bytes", mrq.size);
        return;
      }

      CheckVkResult(vkr);

      vkr = m_pDriver->vkBindBufferMemory(dev, readbackTaskBuffer, readbackTaskMem, 0);
      CheckVkResult(vkr);

      // register address as specialisation constant

      // ensure we're 64-bit aligned first
      taskSpecData.resize(AlignUp(taskSpecData.size(), (size_t)8));

      VkBufferDeviceAddressInfo getAddressInfo = {
          VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
          NULL,
          taskBuffer,
      };

      taskDataAddress = m_pDriver->vkGetBufferDeviceAddress(dev, &getAddressInfo);

      VkSpecializationMapEntry entry;
      entry.offset = (uint32_t)taskSpecData.size();
      entry.constantID = bufSpecConstant + 1;
      entry.size = sizeof(uint64_t);
      taskSpecEntries.push_back(entry);
      taskSpecData.append((const byte *)&taskDataAddress, sizeof(uint64_t));
    }

    VkSpecializationInfo taskSpecInfo = {};
    taskSpecInfo.dataSize = taskSpecData.size();
    taskSpecInfo.pData = taskSpecData.data();
    taskSpecInfo.mapEntryCount = (uint32_t)taskSpecEntries.size();
    taskSpecInfo.pMapEntries = taskSpecEntries.data();

    // create task shader with modified code
    VkShaderModuleCreateInfo moduleCreateInfo = {
        VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        NULL,
        0,
        taskSpirv.size() * sizeof(uint32_t),
        taskSpirv.data(),
    };

    VkShaderModule taskModule = VK_NULL_HANDLE;
    if(!state.graphics.shaderObject)
    {
      vkr = m_pDriver->vkCreateShaderModule(dev, &moduleCreateInfo, NULL, &taskModule);
      CheckVkResult(vkr);
    }

    for(uint32_t s = 0; s < pipeCreateInfo.stageCount; s++)
    {
      if(pipeCreateInfo.pStages[s].stage == VK_SHADER_STAGE_TASK_BIT_EXT)
      {
        VkPipelineShaderStageCreateInfo &taskStage =
            (VkPipelineShaderStageCreateInfo &)pipeCreateInfo.pStages[s];
        taskStage.module = taskModule;
        taskStage.pSpecializationInfo = &taskSpecInfo;
      }
    }

    // create new pipeline
    VkPipeline taskPipe = VK_NULL_HANDLE;
    if(!state.graphics.shaderObject)
      vkr = m_pDriver->vkCreateGraphicsPipelines(m_Device, VK_NULL_HANDLE, 1, &pipeCreateInfo, NULL,
                                                 &taskPipe);

    // delete shader/shader module
    m_pDriver->vkDestroyShaderModule(dev, taskModule, NULL);

    if(vkr != VK_SUCCESS)
    {
      m_pDriver->vkFreeMemory(m_Device, taskMem, NULL);
      m_pDriver->vkFreeMemory(m_Device, readbackTaskMem, NULL);
      m_pDriver->vkDestroyBuffer(m_Device, taskBuffer, NULL);
      m_pDriver->vkDestroyBuffer(m_Device, readbackTaskBuffer, NULL);

      ret.meshout.status = ret.taskout.status =
          StringFormat::Fmt("Failed to create patched mesh shader pipeline: %s", ToStr(vkr).c_str());
      RDCERR("%s", ret.meshout.status.c_str());
      return;
    }

    // create task shader object with modified code
    VkShaderEXT taskShader = VK_NULL_HANDLE;
    if(state.graphics.shaderObject)
    {
      VkShaderCreateInfoEXT shaderCreateInfo = taskCreateInfo;
      shaderCreateInfo.codeType = VK_SHADER_CODE_TYPE_SPIRV_EXT;
      shaderCreateInfo.codeSize = taskSpirv.size() * sizeof(uint32_t);
      shaderCreateInfo.pCode = taskSpirv.data();

      vkr = m_pDriver->vkCreateShadersEXT(m_Device, 1, &shaderCreateInfo, NULL, &taskShader);

      if(vkr != VK_SUCCESS)
      {
        m_pDriver->vkFreeMemory(m_Device, taskMem, NULL);
        m_pDriver->vkFreeMemory(m_Device, readbackTaskMem, NULL);
        m_pDriver->vkDestroyBuffer(m_Device, taskBuffer, NULL);
        m_pDriver->vkDestroyBuffer(m_Device, readbackTaskBuffer, NULL);

        ret.meshout.status = ret.taskout.status =
            StringFormat::Fmt("Failed to create patched task shader object: %s", ToStr(vkr).c_str());
        RDCERR("%s", ret.meshout.status.c_str());
        return;
      }
    }

    // make copy of state to draw from
    VulkanRenderState modifiedstate = state;

    // bind created shader object or pipeline to partial replay state
    if(state.graphics.shaderObject)
    {
      modifiedstate.graphics.pipeline = ResourceId();
      modifiedstate.shaderObjects[(size_t)ShaderStage::Task] = GetResID(taskShader);
    }
    else
    {
      modifiedstate.graphics.pipeline = GetResID(taskPipe);
    }

    VkCommandBuffer cmd = m_pDriver->GetNextCmd();

    if(cmd == VK_NULL_HANDLE)
    {
      m_pDriver->vkFreeMemory(m_Device, taskMem, NULL);
      m_pDriver->vkFreeMemory(m_Device, readbackTaskMem, NULL);
      m_pDriver->vkDestroyBuffer(m_Device, taskBuffer, NULL);
      m_pDriver->vkDestroyBuffer(m_Device, readbackTaskBuffer, NULL);

      m_pDriver->vkDestroyPipeline(dev, taskPipe, NULL);
      if(taskShader != VK_NULL_HANDLE)
        m_pDriver->vkDestroyShaderEXT(dev, taskShader, NULL);
      return;
    }

    VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL,
                                          VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};

    vkr = ObjDisp(dev)->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
    CheckVkResult(vkr);

    // fill destination buffer with 0s to ensure unwritten vertices have sane data
    ObjDisp(dev)->CmdFillBuffer(Unwrap(cmd), Unwrap(taskBuffer), 0, taskBufSize, 0);

    VkBufferMemoryBarrier taskbufbarrier = {
        VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
        NULL,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
        VK_QUEUE_FAMILY_IGNORED,
        VK_QUEUE_FAMILY_IGNORED,
    };

    taskbufbarrier.buffer = Unwrap(taskBuffer);
    taskbufbarrier.size = taskBufSize;

    // wait for the above fill to finish.
    DoPipelineBarrier(cmd, 1, &taskbufbarrier);

    modifiedstate.subpassContents = VK_SUBPASS_CONTENTS_INLINE;
    modifiedstate.dynamicRendering.flags &= ~VK_RENDERING_CONTENTS_SECONDARY_COMMAND_BUFFERS_BIT;

    // do single draw
    modifiedstate.BeginRenderPassAndApplyState(m_pDriver, cmd, VulkanRenderState::BindGraphics,
                                               false);

    m_pDriver->ReplayDraw(cmd, action);

    modifiedstate.EndRenderPass(cmd);

    // wait for task output writing to finish
    taskbufbarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    taskbufbarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

    DoPipelineBarrier(cmd, 1, &taskbufbarrier);

    VkBufferCopy bufcopy = {
        0,
        0,
        taskBufSize,
    };

    // copy to readback buffer
    ObjDisp(dev)->CmdCopyBuffer(Unwrap(cmd), Unwrap(taskBuffer), Unwrap(readbackTaskBuffer), 1,
                                &bufcopy);

    taskbufbarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    taskbufbarrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
    taskbufbarrier.buffer = Unwrap(readbackTaskBuffer);

    // wait for copy to finish
    DoPipelineBarrier(cmd, 1, &taskbufbarrier);

    vkr = ObjDisp(dev)->EndCommandBuffer(Unwrap(cmd));
    CheckVkResult(vkr);

    // submit & flush so that we don't have to keep pipeline or shader object around for a while
    m_pDriver->SubmitCmds();
    m_pDriver->FlushQ();

    // delete pipeline
    m_pDriver->vkDestroyPipeline(dev, taskPipe, NULL);

    // delete task shader object
    if(taskShader != VK_NULL_HANDLE)
      m_pDriver->vkDestroyShaderEXT(dev, taskShader, NULL);

    // readback task data
    const byte *taskData = NULL;
    vkr = m_pDriver->vkMapMemory(m_Device, readbackTaskMem, 0, VK_WHOLE_SIZE, 0, (void **)&taskData);
    CheckVkResult(vkr);
    if(vkr != VK_SUCCESS || !taskData)
    {
      m_pDriver->vkFreeMemory(m_Device, taskMem, NULL);
      m_pDriver->vkFreeMemory(m_Device, readbackTaskMem, NULL);
      m_pDriver->vkDestroyBuffer(m_Device, taskBuffer, NULL);
      m_pDriver->vkDestroyBuffer(m_Device, readbackTaskBuffer, NULL);

      if(!taskData)
      {
        RDCERR("Manually reporting failed memory map");
        CheckVkResult(VK_ERROR_MEMORY_MAP_FAILED);
      }
      ret.meshout.status = ret.taskout.status = "Couldn't read back task output data from GPU";
      return;
    }

    VkMappedMemoryRange range = {
        VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE, NULL, readbackTaskMem, 0, VK_WHOLE_SIZE,
    };

    vkr = m_pDriver->vkInvalidateMappedMemoryRanges(m_Device, 1, &range);
    CheckVkResult(vkr);

    totalNumMeshlets = 0;
    const byte *taskDataBegin = taskData;

    cmd = m_pDriver->GetNextCmd();

    if(cmd == VK_NULL_HANDLE)
    {
      m_pDriver->vkFreeMemory(m_Device, taskMem, NULL);
      m_pDriver->vkFreeMemory(m_Device, readbackTaskMem, NULL);
      m_pDriver->vkDestroyBuffer(m_Device, taskBuffer, NULL);
      m_pDriver->vkDestroyBuffer(m_Device, readbackTaskBuffer, NULL);
      return;
    }

    vkr = ObjDisp(dev)->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
    CheckVkResult(vkr);

    for(uint32_t taskGroup = 0; taskGroup < totalNumTaskGroups; taskGroup++)
    {
      Vec4u meshDispatchSize = *(Vec4u *)taskData;
      RDCASSERT(meshDispatchSize.y <= 0xffff);
      RDCASSERT(meshDispatchSize.z <= 0xffff);

      // while we're going, we record writes into the real buffer with the cumulative sizes. This
      // should in theory be better than updating it via a buffer copy since the count should be
      // much smaller than the payload
      ObjDisp(dev)->CmdUpdateBuffer(Unwrap(cmd), Unwrap(taskBuffer),
                                    taskData - taskDataBegin + offsetof(Vec4u, w), 4,
                                    &totalNumMeshlets);

      totalNumMeshlets += meshDispatchSize.x * meshDispatchSize.y * meshDispatchSize.z;

      VulkanPostVSData::InstData i;
      i.taskDispatchSizeX = meshDispatchSize.x;
      i.taskDispatchSizeYZ.y = meshDispatchSize.y & 0xffff;
      i.taskDispatchSizeYZ.z = meshDispatchSize.z & 0xffff;
      taskDispatchSizes.push_back(i);

      taskData += sizeof(Vec4u) + taskPayloadSize;
    }

    m_pDriver->vkUnmapMemory(m_Device, readbackTaskMem);

    taskbufbarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    taskbufbarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    taskbufbarrier.buffer = Unwrap(taskBuffer);

    // wait for copy to finish
    DoPipelineBarrier(cmd, 1, &taskbufbarrier);

    vkr = ObjDisp(dev)->EndCommandBuffer(Unwrap(cmd));
    CheckVkResult(vkr);
  }

  // clean up temporary memories
  m_pDriver->vkDestroyBuffer(m_Device, readbackTaskBuffer, NULL);
  m_pDriver->vkFreeMemory(m_Device, readbackTaskMem, NULL);

  VkBuffer meshBuffer = VK_NULL_HANDLE, readbackBuffer = VK_NULL_HANDLE;
  VkDeviceMemory meshMem = VK_NULL_HANDLE, readbackMem = VK_NULL_HANDLE;

  VkDeviceSize bufSize = 0;

  uint32_t numViews = 1;

  if(state.dynamicRendering.active)
  {
    numViews = RDCMAX(numViews, Log2Ceil(state.dynamicRendering.viewMask + 1));
  }
  else
  {
    const VulkanCreationInfo::RenderPass &rp = creationInfo.m_RenderPass[state.GetRenderPass()];

    if(state.subpass < rp.subpasses.size())
    {
      numViews = RDCMAX(numViews, (uint32_t)rp.subpasses[state.subpass].multiviews.size());
    }
    else
    {
      RDCERR("Subpass is out of bounds to renderpass creation info");
    }
  }

  rdcarray<uint32_t> modSpirv = meshInfo.spirv.GetSPIRV();

  if(!Vulkan_Debug_PostVSDumpDirPath().empty())
    FileIO::WriteAll(Vulkan_Debug_PostVSDumpDirPath() + "/debug_postms_before.spv", modSpirv);

  OutMeshletLayout layout;

  AddMeshShaderOutputStores(*meshrefl, meshShad.specialization, *meshShad.patchData,
                            meshShad.entryPoint, bufSpecConstant, modSpirv, taskDataAddress != 0,
                            layout);

  if(!Vulkan_Debug_PostVSDumpDirPath().empty())
    FileIO::WriteAll(Vulkan_Debug_PostVSDumpDirPath() + "/debug_postms_after.spv", modSpirv);

  if(totalNumMeshlets > 0)
  {
    // now that we know the stride, create buffer of sufficient size for the worst case (maximum
    // generation) of the meshlets
    VkBufferCreateInfo bufInfo = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};

    bufSize = bufInfo.size = layout.meshletByteSize * totalNumMeshlets;

    bufInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bufInfo.usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    bufInfo.usage |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

    vkr = m_pDriver->vkCreateBuffer(dev, &bufInfo, NULL, &meshBuffer);
    CheckVkResult(vkr);

    bufInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

    vkr = m_pDriver->vkCreateBuffer(dev, &bufInfo, NULL, &readbackBuffer);
    CheckVkResult(vkr);

    VkMemoryRequirements mrq = {0};
    m_pDriver->vkGetBufferMemoryRequirements(dev, meshBuffer, &mrq);

    VkMemoryAllocateInfo allocInfo = {
        VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        NULL,
        mrq.size,
        m_pDriver->GetGPULocalMemoryIndex(mrq.memoryTypeBits),
    };

    allocInfo.pNext = &memFlags;

    vkr = m_pDriver->vkAllocateMemory(dev, &allocInfo, NULL, &meshMem);

    if(vkr == VK_ERROR_OUT_OF_DEVICE_MEMORY || vkr == VK_ERROR_OUT_OF_HOST_MEMORY)
    {
      m_pDriver->vkFreeMemory(m_Device, taskMem, NULL);
      m_pDriver->vkDestroyBuffer(m_Device, taskBuffer, NULL);
      m_pDriver->vkDestroyBuffer(m_Device, meshBuffer, NULL);
      m_pDriver->vkDestroyBuffer(m_Device, readbackBuffer, NULL);

      RDCWARN("Failed to allocate %llu bytes for output", mrq.size);
      ret.meshout.status = StringFormat::Fmt("Failed to allocate %llu bytes", mrq.size);
      return;
    }

    CheckVkResult(vkr);

    vkr = m_pDriver->vkBindBufferMemory(dev, meshBuffer, meshMem, 0);
    CheckVkResult(vkr);

    m_pDriver->vkGetBufferMemoryRequirements(dev, readbackBuffer, &mrq);

    allocInfo.pNext = NULL;
    allocInfo.memoryTypeIndex = m_pDriver->GetReadbackMemoryIndex(mrq.memoryTypeBits);

    vkr = m_pDriver->vkAllocateMemory(dev, &allocInfo, NULL, &readbackMem);

    if(vkr == VK_ERROR_OUT_OF_DEVICE_MEMORY || vkr == VK_ERROR_OUT_OF_HOST_MEMORY)
    {
      m_pDriver->vkFreeMemory(m_Device, taskMem, NULL);
      m_pDriver->vkDestroyBuffer(m_Device, taskBuffer, NULL);
      m_pDriver->vkDestroyBuffer(m_Device, meshBuffer, NULL);
      m_pDriver->vkFreeMemory(m_Device, meshMem, NULL);
      m_pDriver->vkDestroyBuffer(m_Device, readbackBuffer, NULL);

      RDCWARN("Failed to allocate %llu bytes for readback", mrq.size);
      ret.meshout.status = StringFormat::Fmt("Failed to allocate %llu bytes", mrq.size);
      return;
    }

    CheckVkResult(vkr);

    vkr = m_pDriver->vkBindBufferMemory(dev, readbackBuffer, readbackMem, 0);
    CheckVkResult(vkr);

    // register address as specialisation constant

    // ensure we're 64-bit aligned first
    meshSpecData.resize(AlignUp(meshSpecData.size(), (size_t)8));

    VkBufferDeviceAddressInfo getAddressInfo = {
        VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        NULL,
        meshBuffer,
    };

    VkDeviceAddress address = m_pDriver->vkGetBufferDeviceAddress(dev, &getAddressInfo);

    VkSpecializationMapEntry entry;
    entry.offset = (uint32_t)meshSpecData.size();
    entry.constantID = bufSpecConstant;
    entry.size = sizeof(uint64_t);
    meshSpecEntries.push_back(entry);
    meshSpecData.append((const byte *)&address, sizeof(uint64_t));
  }

  VkSpecializationInfo meshSpecInfo = {};
  meshSpecInfo.dataSize = meshSpecData.size();
  meshSpecInfo.pData = meshSpecData.data();
  meshSpecInfo.mapEntryCount = (uint32_t)meshSpecEntries.size();
  meshSpecInfo.pMapEntries = meshSpecEntries.data();

  VkSpecializationInfo taskSpecInfo = {};
  taskSpecInfo.dataSize = taskSpecData.size();
  taskSpecInfo.pData = taskSpecData.data();
  taskSpecInfo.mapEntryCount = (uint32_t)taskSpecEntries.size();
  taskSpecInfo.pMapEntries = taskSpecEntries.data();

  // create mesh shader with modified code
  VkShaderModuleCreateInfo moduleCreateInfo = {
      VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, NULL,         0,
      modSpirv.size() * sizeof(uint32_t),          &modSpirv[0],
  };

  VkShaderModule module = VK_NULL_HANDLE, taskFeedModule = VK_NULL_HANDLE;
  if(!state.graphics.shaderObject)
  {
    vkr = m_pDriver->vkCreateShaderModule(dev, &moduleCreateInfo, NULL, &module);
    CheckVkResult(vkr);
  }

  // create mesh shader object with modified code
  VkShaderCreateInfoEXT shaderCreateInfo = meshCreateInfo;
  VkShaderEXT taskShader = VK_NULL_HANDLE, meshShader = VK_NULL_HANDLE;

  if(state.graphics.shaderObject)
  {
    shaderCreateInfo.codeType = VK_SHADER_CODE_TYPE_SPIRV_EXT;
    shaderCreateInfo.codeSize = modSpirv.byteSize();
    shaderCreateInfo.pCode = modSpirv.data();
    shaderCreateInfo.pSpecializationInfo = &meshSpecInfo;

    vkr = m_pDriver->vkCreateShadersEXT(dev, 1, &shaderCreateInfo, NULL, &meshShader);

    if(vkr != VK_SUCCESS)
    {
      m_pDriver->vkFreeMemory(m_Device, taskMem, NULL);
      m_pDriver->vkDestroyBuffer(m_Device, taskBuffer, NULL);
      m_pDriver->vkDestroyBuffer(m_Device, meshBuffer, NULL);
      m_pDriver->vkFreeMemory(m_Device, meshMem, NULL);
      m_pDriver->vkDestroyBuffer(m_Device, readbackBuffer, NULL);
      m_pDriver->vkFreeMemory(m_Device, readbackMem, NULL);

      ret.meshout.status =
          StringFormat::Fmt("Failed to create patched mesh shader object: %s", ToStr(vkr).c_str());
      RDCERR("%s", ret.meshout.status.c_str());
      return;
    }
  }

  if(taskDataAddress != 0)
  {
    // use the shader object or shader module, as applicable
    const VulkanCreationInfo::ShaderEntry &taskShad =
        state.graphics.shaderObject ? taskShadObjInfo.shad
                                    : pipeInfo.shaders[(size_t)ShaderStage::Task];

    const VulkanCreationInfo::ShaderModule &taskInfo = creationInfo.m_ShaderModule[taskShad.module];

    modSpirv = taskInfo.spirv.GetSPIRV();

    ConvertToFixedTaskFeeder(taskShad.specialization, taskShad.entryPoint, bufSpecConstant + 1,
                             taskPayloadSize, modSpirv);

    if(!Vulkan_Debug_PostVSDumpDirPath().empty())
      FileIO::WriteAll(Vulkan_Debug_PostVSDumpDirPath() + "/debug_postts_feeder.spv", modSpirv);

    if(state.graphics.shaderObject)
    {
      shaderCreateInfo = taskCreateInfo;
      shaderCreateInfo.codeType = VK_SHADER_CODE_TYPE_SPIRV_EXT;
      shaderCreateInfo.codeSize = modSpirv.byteSize();
      shaderCreateInfo.pCode = modSpirv.data();
      shaderCreateInfo.pSpecializationInfo = &taskSpecInfo;

      vkr = m_pDriver->vkCreateShadersEXT(dev, 1, &shaderCreateInfo, NULL, &taskShader);

      if(vkr != VK_SUCCESS)
      {
        if(meshShader != VK_NULL_HANDLE)
          m_pDriver->vkDestroyShaderEXT(dev, meshShader, NULL);
        m_pDriver->vkFreeMemory(m_Device, taskMem, NULL);
        m_pDriver->vkDestroyBuffer(m_Device, taskBuffer, NULL);
        m_pDriver->vkDestroyBuffer(m_Device, meshBuffer, NULL);
        m_pDriver->vkFreeMemory(m_Device, meshMem, NULL);
        m_pDriver->vkDestroyBuffer(m_Device, readbackBuffer, NULL);
        m_pDriver->vkFreeMemory(m_Device, readbackMem, NULL);

        ret.meshout.status =
            StringFormat::Fmt("Failed to create patched task shader object: %s", ToStr(vkr).c_str());
        RDCERR("%s", ret.meshout.status.c_str());
        return;
      }
    }
    else
    {
      moduleCreateInfo.pCode = modSpirv.data();
      moduleCreateInfo.codeSize = modSpirv.byteSize();

      vkr = m_pDriver->vkCreateShaderModule(dev, &moduleCreateInfo, NULL, &taskFeedModule);
      CheckVkResult(vkr);
    }
  }

  for(uint32_t s = 0; s < pipeCreateInfo.stageCount; s++)
  {
    if(pipeCreateInfo.pStages[s].stage == VK_SHADER_STAGE_MESH_BIT_EXT)
    {
      VkPipelineShaderStageCreateInfo &meshStage =
          (VkPipelineShaderStageCreateInfo &)pipeCreateInfo.pStages[s];
      meshStage.module = module;
      meshStage.pSpecializationInfo = &meshSpecInfo;
    }
    else if(pipeCreateInfo.pStages[s].stage == VK_SHADER_STAGE_TASK_BIT_EXT)
    {
      VkPipelineShaderStageCreateInfo &taskStage =
          (VkPipelineShaderStageCreateInfo &)pipeCreateInfo.pStages[s];
      taskStage.module = taskFeedModule;
      taskStage.pSpecializationInfo = &taskSpecInfo;
    }
  }

  // create new pipeline
  VkPipeline pipe = VK_NULL_HANDLE;
  if(!state.graphics.shaderObject)
    vkr = m_pDriver->vkCreateGraphicsPipelines(m_Device, VK_NULL_HANDLE, 1, &pipeCreateInfo, NULL,
                                               &pipe);

  // delete shader/shader module
  m_pDriver->vkDestroyShaderModule(dev, module, NULL);

  // delete shader/shader module
  m_pDriver->vkDestroyShaderModule(dev, taskFeedModule, NULL);

  if(vkr != VK_SUCCESS)
  {
    m_pDriver->vkFreeMemory(m_Device, taskMem, NULL);
    m_pDriver->vkDestroyBuffer(m_Device, taskBuffer, NULL);
    m_pDriver->vkDestroyBuffer(m_Device, meshBuffer, NULL);
    m_pDriver->vkFreeMemory(m_Device, meshMem, NULL);
    m_pDriver->vkDestroyBuffer(m_Device, readbackBuffer, NULL);
    m_pDriver->vkFreeMemory(m_Device, readbackMem, NULL);

    ret.meshout.status =
        StringFormat::Fmt("Failed to create patched mesh shader pipeline: %s", ToStr(vkr).c_str());
    RDCERR("%s", ret.meshout.status.c_str());
    return;
  }

  // make copy of state to draw from
  VulkanRenderState modifiedstate = state;

  // bind created pipeline to partial replay state
  modifiedstate.graphics.pipeline = GetResID(pipe);

  // bind task/mesh to partial replay state if using shader objects
  if(state.graphics.shaderObject)
  {
    modifiedstate.graphics.pipeline = ResourceId();
    modifiedstate.shaderObjects[(size_t)ShaderStage::Task] = GetResID(taskShader);
    modifiedstate.shaderObjects[(size_t)ShaderStage::Mesh] = GetResID(meshShader);
  }

  if(totalNumMeshlets > 0)
  {
    VkCommandBuffer cmd = m_pDriver->GetNextCmd();

    if(cmd == VK_NULL_HANDLE)
    {
      if(taskShader != VK_NULL_HANDLE)
        m_pDriver->vkDestroyShaderEXT(dev, taskShader, NULL);
      if(meshShader != VK_NULL_HANDLE)
        m_pDriver->vkDestroyShaderEXT(dev, meshShader, NULL);
      m_pDriver->vkDestroyPipeline(dev, pipe, NULL);
      m_pDriver->vkFreeMemory(m_Device, taskMem, NULL);
      m_pDriver->vkDestroyBuffer(m_Device, taskBuffer, NULL);
      m_pDriver->vkDestroyBuffer(m_Device, meshBuffer, NULL);
      m_pDriver->vkFreeMemory(m_Device, meshMem, NULL);
      m_pDriver->vkDestroyBuffer(m_Device, readbackBuffer, NULL);
      m_pDriver->vkFreeMemory(m_Device, readbackMem, NULL);

      return;
    }

    VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL,
                                          VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};

    vkr = ObjDisp(dev)->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
    CheckVkResult(vkr);

    // fill destination buffer with 0s to ensure unwritten vertices have sane data
    ObjDisp(dev)->CmdFillBuffer(Unwrap(cmd), Unwrap(meshBuffer), 0, bufSize, 0);

    VkBufferMemoryBarrier meshbufbarrier = {
        VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
        NULL,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
        VK_QUEUE_FAMILY_IGNORED,
        VK_QUEUE_FAMILY_IGNORED,
    };

    meshbufbarrier.buffer = Unwrap(meshBuffer);
    meshbufbarrier.size = bufSize;

    // wait for the above fill to finish.
    DoPipelineBarrier(cmd, 1, &meshbufbarrier);

    modifiedstate.subpassContents = VK_SUBPASS_CONTENTS_INLINE;
    modifiedstate.dynamicRendering.flags &= ~VK_RENDERING_CONTENTS_SECONDARY_COMMAND_BUFFERS_BIT;

    // do single draw
    modifiedstate.BeginRenderPassAndApplyState(m_pDriver, cmd, VulkanRenderState::BindGraphics,
                                               false);

    m_pDriver->ReplayDraw(cmd, action);

    modifiedstate.EndRenderPass(cmd);

    // wait for mesh output writing to finish
    meshbufbarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    meshbufbarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

    DoPipelineBarrier(cmd, 1, &meshbufbarrier);

    VkBufferCopy bufcopy = {
        0,
        0,
        bufSize,
    };

    // copy to readback buffer
    ObjDisp(dev)->CmdCopyBuffer(Unwrap(cmd), Unwrap(meshBuffer), Unwrap(readbackBuffer), 1, &bufcopy);

    meshbufbarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    meshbufbarrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
    meshbufbarrier.buffer = Unwrap(readbackBuffer);

    // wait for copy to finish
    DoPipelineBarrier(cmd, 1, &meshbufbarrier);

    vkr = ObjDisp(dev)->EndCommandBuffer(Unwrap(cmd));
    CheckVkResult(vkr);

    // submit & flush so that we don't have to keep pipeline around for a while
    m_pDriver->SubmitCmds();
    m_pDriver->FlushQ();
  }

  // delete pipeline
  m_pDriver->vkDestroyPipeline(dev, pipe, NULL);

  // delete task/mesh shader objects
  if(taskShader != VK_NULL_HANDLE)
    m_pDriver->vkDestroyShaderEXT(dev, taskShader, NULL);
  if(meshShader != VK_NULL_HANDLE)
    m_pDriver->vkDestroyShaderEXT(dev, meshShader, NULL);

  rdcarray<VulkanPostVSData::InstData> meshletOffsets;

  uint32_t baseIndex = 0;

  rdcarray<uint32_t> rebasedIndices;
  bytebuf compactedVertices;

  float nearp = 0.1f;
  float farp = 100.0f;

  uint32_t totalVerts = 0, totalPrims = 0;
  uint32_t totalVertStride = 0;
  uint32_t totalPrimStride = 0;

  if(totalNumMeshlets > 0)
  {
    // readback mesh data
    const byte *meshletData = NULL;
    vkr = m_pDriver->vkMapMemory(m_Device, readbackMem, 0, VK_WHOLE_SIZE, 0, (void **)&meshletData);
    CheckVkResult(vkr);
    if(vkr != VK_SUCCESS || !meshletData)
    {
      if(!meshletData)
      {
        RDCERR("Manually reporting failed memory map");
        CheckVkResult(VK_ERROR_MEMORY_MAP_FAILED);
      }
      m_pDriver->vkFreeMemory(m_Device, taskMem, NULL);
      m_pDriver->vkDestroyBuffer(m_Device, taskBuffer, NULL);
      m_pDriver->vkDestroyBuffer(m_Device, meshBuffer, NULL);
      m_pDriver->vkFreeMemory(m_Device, meshMem, NULL);
      m_pDriver->vkDestroyBuffer(m_Device, readbackBuffer, NULL);
      m_pDriver->vkFreeMemory(m_Device, readbackMem, NULL);
      ret.meshout.status = "Couldn't read back mesh output data from GPU";
      return;
    }

    VkMappedMemoryRange range = {
        VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE, NULL, readbackMem, 0, VK_WHOLE_SIZE,
    };

    vkr = m_pDriver->vkInvalidateMappedMemoryRanges(m_Device, 1, &range);
    CheckVkResult(vkr);

    // do a super quick sum of the number of verts and prims
    for(uint32_t m = 0; m < totalNumMeshlets; m++)
    {
      Vec4u *counts = (Vec4u *)(meshletData + m * layout.meshletByteSize);
      totalVerts += counts->x;
      totalPrims += counts->y;
    }

    if(totalPrims == 0)
    {
      m_pDriver->vkFreeMemory(m_Device, taskMem, NULL);
      m_pDriver->vkDestroyBuffer(m_Device, taskBuffer, NULL);
      m_pDriver->vkDestroyBuffer(m_Device, meshBuffer, NULL);
      m_pDriver->vkFreeMemory(m_Device, meshMem, NULL);
      m_pDriver->vkDestroyBuffer(m_Device, readbackBuffer, NULL);
      m_pDriver->vkFreeMemory(m_Device, readbackMem, NULL);
      ret.meshout.status = "No mesh output data generated by GPU";
      return;
    }

    for(size_t o = 0; o < layout.sigLocations.size(); o++)
    {
      if(meshrefl->outputSignature[o].systemValue == ShaderBuiltin::OutputIndices)
        continue;

      const SigParameter &sig = meshrefl->outputSignature[o];
      const uint32_t byteSize = VarTypeByteSize(sig.varType) * sig.compCount;

      if(meshrefl->outputSignature[o].perPrimitiveRate)
        totalPrimStride += byteSize;
      else
        totalVertStride += byteSize;
    }

    rdcarray<uint32_t> sigOffsets;
    sigOffsets.resize(meshrefl->outputSignature.size());

    {
      uint32_t vertOffset = 0;
      uint32_t primOffset = 0;
      for(size_t o = 0; o < meshrefl->outputSignature.size(); o++)
      {
        const SigParameter &sig = meshrefl->outputSignature[o];
        const uint32_t byteSize = VarTypeByteSize(sig.varType) * sig.compCount;

        if(sig.systemValue == ShaderBuiltin::OutputIndices)
          continue;

        // move position to the front when compacting
        if(sig.systemValue == ShaderBuiltin::Position)
        {
          RDCASSERT(!sig.perPrimitiveRate);
          sigOffsets[o] = 0;
          vertOffset += byteSize;

          // shift all previous signatures up
          for(size_t prev = 0; prev < o; prev++)
            sigOffsets[prev] += byteSize;

          continue;
        }

        if(sig.perPrimitiveRate)
        {
          sigOffsets[o] = primOffset;
          primOffset += byteSize;
        }
        else
        {
          sigOffsets[o] = vertOffset;
          vertOffset += byteSize;
        }
      }

      RDCASSERT(vertOffset == totalVertStride);
      RDCASSERT(primOffset == totalPrimStride);
    }

    // now we reorganise and compact the data.
    // Some arrays will need to be decomposed (any non-struct outputs will be SoA and we want full
    // AoS). We also rebase indices so they can be used as a contiguous index buffer

    rebasedIndices.reserve(totalPrims * layout.indexCountPerPrim);
    compactedVertices.resize(totalVerts * totalVertStride + totalPrims * totalPrimStride);

    byte *vertData = compactedVertices.begin();
    byte *primData = vertData + totalVerts * totalVertStride;

    // calculate near/far as we're going
    bool found = false;
    Vec4f pos0;

    for(uint32_t meshlet = 0; meshlet < totalNumMeshlets; meshlet++)
    {
      Vec4u *counts = (Vec4u *)meshletData;
      const uint32_t numVerts = counts->x;
      const uint32_t numPrims = counts->y;

      const uint32_t padding = counts->z;
      const uint32_t padding2 = counts->w;
      RDCASSERTEQUAL(padding, 0);
      RDCASSERTEQUAL(padding2, 0);

      if(numVerts > layout.vertArrayLength)
      {
        RDCERR("Meshlet returned invalid vertex count %u with declared max %u", numVerts,
               layout.vertArrayLength);
        ret.meshout.status = "Got corrupted mesh output data from GPU";
      }

      if(numPrims > layout.primArrayLength)
      {
        RDCERR("Meshlet returned invalid primitive count %u with declared max %u", numPrims,
               layout.primArrayLength);
        ret.meshout.status = "Got corrupted mesh output data from GPU";
      }

      if(!ret.meshout.status.empty())
      {
        m_pDriver->vkFreeMemory(m_Device, taskMem, NULL);
        m_pDriver->vkDestroyBuffer(m_Device, taskBuffer, NULL);
        m_pDriver->vkDestroyBuffer(m_Device, meshBuffer, NULL);
        m_pDriver->vkFreeMemory(m_Device, meshMem, NULL);
        m_pDriver->vkDestroyBuffer(m_Device, readbackBuffer, NULL);
        m_pDriver->vkFreeMemory(m_Device, readbackMem, NULL);
        return;
      }

      VulkanPostVSData::InstData meshletOffsetData;
      meshletOffsetData.numIndices = numPrims * layout.indexCountPerPrim;
      meshletOffsetData.numVerts = numVerts;
      meshletOffsets.push_back(meshletOffsetData);

      uint32_t *indices = (uint32_t *)(counts + 2);

      for(uint32_t p = 0; p < numPrims; p++)
      {
        for(uint32_t idx = 0; idx < layout.indexCountPerPrim; idx++)
          rebasedIndices.push_back(indices[p * layout.indexCountPerPrim + idx] + baseIndex);
      }

      for(size_t o = 0; o < meshrefl->outputSignature.size(); o++)
      {
        const SigParameter &sig = meshrefl->outputSignature[o];
        const uint32_t byteSize = VarTypeByteSize(sig.varType) * sig.compCount;

        if(sig.systemValue == ShaderBuiltin::OutputIndices)
          continue;

        if(meshrefl->outputSignature[o].perPrimitiveRate)
        {
          for(uint32_t p = 0; p < numPrims; p++)
          {
            memcpy(primData + sigOffsets[o] + totalPrimStride * p,
                   meshletData + layout.sigLocations[o].offset + layout.sigLocations[o].stride * p,
                   byteSize);
          }
        }
        else
        {
          for(uint32_t v = 0; v < numVerts; v++)
          {
            byte *dst = vertData + sigOffsets[o] + totalVertStride * v;

            memcpy(dst,
                   meshletData + layout.sigLocations[o].offset + layout.sigLocations[o].stride * v,
                   byteSize);

            if(!found && sig.systemValue == ShaderBuiltin::Position)
            {
              Vec4f pos = *(Vec4f *)dst;

              if(v == 0)
              {
                pos0 = pos;
              }
              else
              {
                DeriveNearFar(pos, pos0, nearp, farp, found);
              }
            }
          }
        }
      }

      baseIndex += numVerts;
      meshletData += layout.meshletByteSize;
      vertData += totalVertStride * numVerts;
      primData += totalPrimStride * numPrims;
    }

    RDCASSERT(vertData == compactedVertices.begin() + totalVerts * totalVertStride);
    RDCASSERT(primData == compactedVertices.end());

    // if we didn't find any near/far plane, all z's and w's were identical.
    // If the z is positive and w greater for the first element then we detect this projection as
    // reversed z with infinite far plane
    if(!found && pos0.z > 0.0f && pos0.w > pos0.z)
    {
      nearp = pos0.z;
      farp = FLT_MAX;
    }

    m_pDriver->vkUnmapMemory(m_Device, readbackMem);
  }

  // clean up temporary memories
  m_pDriver->vkDestroyBuffer(m_Device, readbackBuffer, NULL);
  m_pDriver->vkFreeMemory(m_Device, readbackMem, NULL);

  // clean up temporary memories
  m_pDriver->vkDestroyBuffer(m_Device, meshBuffer, NULL);
  m_pDriver->vkFreeMemory(m_Device, meshMem, NULL);

  // fill out m_PostVS.Data
  if(layout.indexCountPerPrim == 3)
    ret.meshout.topo = Topology::TriangleList;
  else if(layout.indexCountPerPrim == 2)
    ret.meshout.topo = Topology::LineList;
  else if(layout.indexCountPerPrim == 1)
    ret.meshout.topo = Topology::PointList;

  if(totalNumMeshlets > 0)
  {
    VkBufferCreateInfo bufInfo = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};

    bufInfo.size = AlignUp16(compactedVertices.byteSize()) + rebasedIndices.byteSize();

    bufInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bufInfo.usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    bufInfo.usage |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    bufInfo.usage |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;

    vkr = m_pDriver->vkCreateBuffer(dev, &bufInfo, NULL, &meshBuffer);
    CheckVkResult(vkr);

    VkMemoryRequirements mrq = {0};
    m_pDriver->vkGetBufferMemoryRequirements(dev, meshBuffer, &mrq);

    VkMemoryAllocateInfo allocInfo = {
        VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        NULL,
        mrq.size,
        m_pDriver->GetUploadMemoryIndex(mrq.memoryTypeBits),
    };

    vkr = m_pDriver->vkAllocateMemory(dev, &allocInfo, NULL, &meshMem);

    if(vkr == VK_ERROR_OUT_OF_DEVICE_MEMORY || vkr == VK_ERROR_OUT_OF_HOST_MEMORY)
    {
      m_pDriver->vkDestroyBuffer(m_Device, meshBuffer, NULL);
      RDCWARN("Failed to allocate %llu bytes for output", mrq.size);
      ret.meshout.status = StringFormat::Fmt("Failed to allocate %llu bytes", mrq.size);
      return;
    }

    CheckVkResult(vkr);

    vkr = m_pDriver->vkBindBufferMemory(dev, meshBuffer, meshMem, 0);
    CheckVkResult(vkr);

    byte *uploadData = NULL;
    vkr = m_pDriver->vkMapMemory(m_Device, meshMem, 0, VK_WHOLE_SIZE, 0, (void **)&uploadData);
    CheckVkResult(vkr);
    if(vkr != VK_SUCCESS || !uploadData)
    {
      m_pDriver->vkDestroyBuffer(m_Device, meshBuffer, NULL);
      m_pDriver->vkFreeMemory(m_Device, meshMem, NULL);
      if(!uploadData)
      {
        RDCERR("Manually reporting failed memory map");
        CheckVkResult(VK_ERROR_MEMORY_MAP_FAILED);
      }
      ret.meshout.status = "Couldn't upload mesh output data to GPU";
      return;
    }

    memcpy(uploadData, compactedVertices.data(), compactedVertices.byteSize());
    memcpy(uploadData + AlignUp16(compactedVertices.byteSize()), rebasedIndices.data(),
           rebasedIndices.byteSize());

    VkMappedMemoryRange range = {
        VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE, NULL, meshMem, 0, VK_WHOLE_SIZE,
    };

    vkr = m_pDriver->vkFlushMappedMemoryRanges(m_Device, 1, &range);
    CheckVkResult(vkr);

    m_pDriver->vkUnmapMemory(m_Device, meshMem);
  }

  ret.taskout.buf = taskBuffer;
  ret.taskout.bufmem = taskMem;

  if(state.graphics.shaderObject ? !taskShadObjInfo.shad.refl
                                 : !pipeInfo.shaders[(size_t)ShaderStage::Task].refl)
    ret.taskout.status = "No task shader bound";

  ret.taskout.baseVertex = 0;

  // TODO handle multiple views
  ret.taskout.numViews = 1;

  ret.taskout.dispatchSize = action.dispatchDimension;

  ret.taskout.vertStride = taskPayloadSize + sizeof(Vec4u);
  ret.taskout.nearPlane = 0.0f;
  ret.taskout.farPlane = 1.0f;

  ret.taskout.primStride = 0;
  ret.taskout.primOffset = 0;

  ret.taskout.useIndices = false;
  ret.taskout.numVerts = totalNumTaskGroups;
  ret.taskout.instData = taskDispatchSizes;

  ret.taskout.instStride = 0;

  ret.taskout.idxbuf = VK_NULL_HANDLE;
  ret.taskout.idxOffset = 0;
  ret.taskout.idxbufmem = VK_NULL_HANDLE;
  ret.taskout.idxFmt = VK_INDEX_TYPE_UINT32;

  ret.taskout.hasPosOut = false;
  ret.taskout.flipY = state.views.empty() ? false : state.views[0].height < 0.0f;

  ret.meshout.buf = meshBuffer;
  ret.meshout.bufmem = meshMem;

  ret.meshout.baseVertex = 0;

  // TODO handle multiple views
  ret.meshout.numViews = 1;

  ret.meshout.dispatchSize = action.dispatchDimension;

  ret.meshout.vertStride = totalVertStride;
  ret.meshout.nearPlane = nearp;
  ret.meshout.farPlane = farp;

  ret.meshout.primStride = totalPrimStride;
  ret.meshout.primOffset = totalVertStride * totalVerts;

  ret.meshout.useIndices = true;
  ret.meshout.numVerts = totalPrims * layout.indexCountPerPrim;
  ret.meshout.instData = meshletOffsets;

  ret.meshout.instStride = 0;

  ret.meshout.idxbuf = meshBuffer;
  ret.meshout.idxOffset = AlignUp16(compactedVertices.byteSize());
  ret.meshout.idxbufmem = VK_NULL_HANDLE;
  ret.meshout.idxFmt = VK_INDEX_TYPE_UINT32;

  ret.meshout.hasPosOut = true;
  ret.meshout.flipY = state.views.empty() ? false : state.views[0].height < 0.0f;
}

void VulkanReplay::FetchVSOut(uint32_t eventId, VulkanRenderState &state)
{
  VulkanCreationInfo &creationInfo = m_pDriver->m_CreationInfo;

  const VulkanCreationInfo::Pipeline &pipeInfo = creationInfo.m_Pipeline[state.graphics.pipeline];

  const ActionDescription *action = m_pDriver->GetAction(eventId);

  const VulkanCreationInfo::ShaderObject &shadObjInfo =
      creationInfo.m_ShaderObject[state.shaderObjects[(size_t)ShaderStage::Vertex]];

  const VulkanCreationInfo::ShaderEntry &vertShad =
      state.graphics.shaderObject ? shadObjInfo.shad : pipeInfo.shaders[(size_t)ShaderStage::Vertex];

  const VulkanCreationInfo::ShaderModule &moduleInfo = creationInfo.m_ShaderModule[vertShad.module];

  ShaderReflection *refl = vertShad.refl;

  VulkanPostVSData &ret = m_PostVS.Data[eventId];

  // set defaults so that we don't try to fetch this output again if something goes wrong and the
  // same event is selected again
  {
    ret.vsout.buf = VK_NULL_HANDLE;
    ret.vsout.bufmem = VK_NULL_HANDLE;
    ret.vsout.instStride = 0;
    ret.vsout.vertStride = 0;
    ret.vsout.numViews = 1;
    ret.vsout.nearPlane = 0.0f;
    ret.vsout.farPlane = 0.0f;
    ret.vsout.useIndices = false;
    ret.vsout.hasPosOut = false;
    ret.vsout.flipY = false;
    ret.vsout.idxbuf = VK_NULL_HANDLE;
    ret.vsout.idxbufmem = VK_NULL_HANDLE;

    ret.vsout.topo = MakePrimitiveTopology(state.primitiveTopology, state.patchControlPoints);
  }

  // no outputs from this shader? unexpected but theoretically possible (dummy VS before
  // tessellation maybe). Just fill out an empty data set
  if(refl->outputSignature.empty())
    return;

  // we go through the driver for all these creations since they need to be properly
  // registered in order to be put in the partial replay state
  VkResult vkr = VK_SUCCESS;
  VkDevice dev = m_Device;

  VkDescriptorPool descpool = VK_NULL_HANDLE;
  rdcarray<VkDescriptorSetLayout> setLayouts;
  rdcarray<VkDescriptorSet> descSets;

  VkPipelineLayout pipeLayout = VK_NULL_HANDLE;

  StorageMode storageMode = Binding;

  if(m_pDriver->GetExtensions(NULL).ext_KHR_buffer_device_address)
  {
    storageMode = KHR_bda;
  }
  else if(m_pDriver->GetExtensions(NULL).ext_EXT_buffer_device_address)
  {
    storageMode = EXT_bda;

    if(!m_pDriver->GetDeviceEnabledFeatures().shaderInt64)
    {
      static bool warned = false;
      if(!warned)
      {
        warned = true;
        RDCLOG(
            "EXT_buffer_device_address is available but shaderInt64 isn't, falling back to binding "
            "storage mode");
      }
    }
  }

  if(Vulkan_Debug_DisableBufferDeviceAddress() ||
     m_pDriver->GetDriverInfo().BufferDeviceAddressBrokenDriver())
    storageMode = Binding;

  if(m_pDriver->GetDeviceProps().limits.maxPerStageDescriptorStorageBuffers - 2 <
     MeshOutputBufferArraySize)
  {
    RDCWARN("Default buffer descriptor array size %u is over device limit, clamping to %u",
            MeshOutputBufferArraySize,
            m_pDriver->GetDeviceProps().limits.maxPerStageDescriptorStorageBuffers - 2);

    MeshOutputBufferArraySize =
        m_pDriver->GetDeviceProps().limits.maxPerStageDescriptorStorageBuffers - 2;
  }

  for(size_t i = 0; i < refl->inputSignature.size(); i++)
  {
    if(refl->inputSignature[i].regIndex >= MeshOutputBufferArraySize)
    {
      ret.vsout.status = StringFormat::Fmt(
          "Input %s refers to attribute %u which is too large to be handled",
          refl->inputSignature[i].varName.c_str(), refl->inputSignature[i].regIndex);
      RDCERR("%s", ret.vsout.status.c_str());
      return;
    }
  }

  VkDescriptorSetLayoutBinding newBindings[] = {
      // output buffer
      {
          0,
          VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
          1,
          VK_SHADER_STAGE_COMPUTE_BIT,
          NULL,
      },
      // index buffer (if needed)
      {
          1,
          VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
          1,
          VK_SHADER_STAGE_COMPUTE_BIT,
          NULL,
      },
      // vertex buffers
      {
          2,
          VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
          MeshOutputBufferArraySize,
          VK_SHADER_STAGE_COMPUTE_BIT,
          NULL,
      },
  };
  RDCCOMPILE_ASSERT(ARRAY_COUNT(newBindings) == MeshOutputReservedBindings,
                    "MeshOutputReservedBindings is wrong");

  // the spec says only one push constant range may be used per stage, so at most one has
  // VERTEX_BIT. Find it, and make it COMPUTE_BIT
  VkPushConstantRange push;
  uint32_t numPush = 0;
  rdcarray<VkPushConstantRange> oldPush =
      state.graphics.shaderObject ? shadObjInfo.pushRanges
                                  : creationInfo.m_PipelineLayout[pipeInfo.vertLayout].pushRanges;

  // ensure the push range is visible to the compute shader
  for(const VkPushConstantRange &range : oldPush)
  {
    if(range.stageFlags & VK_SHADER_STAGE_VERTEX_BIT)
    {
      push = range;
      push.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
      numPush = 1;
      break;
    }
  }

  if(storageMode == Binding)
  {
    // create a duplicate set of descriptor sets, all visible to compute, with bindings shifted to
    // account for new ones we need. This also copies the existing bindings into the new sets
    PatchReservedDescriptors(state.graphics, descpool, setLayouts, descSets,
                             VK_SHADER_STAGE_COMPUTE_BIT, newBindings, ARRAY_COUNT(newBindings));

    // if the pool failed due to limits, it will be NULL so bail now
    if(descpool == VK_NULL_HANDLE)
    {
      ret.vsout.status =
          "Couldn't allocate and patch compatible descriptors for vertex output fetch";
      return;
    }

    VkPipelineLayoutCreateInfo pipeLayoutInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        NULL,
        0,
        (uint32_t)setLayouts.size(),
        setLayouts.data(),
        numPush,
        &push,
    };

    vkr = m_pDriver->vkCreatePipelineLayout(dev, &pipeLayoutInfo, NULL, &pipeLayout);
    CheckVkResult(vkr);
  }
  else
  {
    // using BDA we don't need to add any new bindings but we *do* need to patch the descriptor set
    // layouts to be compute visible. However with update-after-bind descriptors in the mix we can't
    // always reliably do this, as making a copy of the descriptor sets can't be done (in general).
    //
    // To get around this we patch descriptor set layouts at create time so that COMPUTE_BIT is
    // present wherever VERTEX_BIT was, so we can use the application's descriptor sets and layouts

    const rdcarray<ResourceId> &sets =
        state.graphics.shaderObject
            ? shadObjInfo.descSetLayouts
            : creationInfo.m_PipelineLayout[pipeInfo.vertLayout].descSetLayouts;

    setLayouts.reserve(sets.size());

    for(size_t i = 0; i < sets.size(); i++)
      setLayouts.push_back(GetResourceManager()->GetCurrentHandle<VkDescriptorSetLayout>(sets[i]));

    VkPipelineLayoutCreateInfo pipeLayoutInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        NULL,
        0,
        (uint32_t)setLayouts.size(),
        setLayouts.data(),
        numPush,
        &push,
    };

    vkr = m_pDriver->vkCreatePipelineLayout(dev, &pipeLayoutInfo, NULL, &pipeLayout);
    CheckVkResult(vkr);

    // clear the array because it's not needed after and we want to avoid releasing real resources
    setLayouts.clear();
  }

  VkBuffer meshBuffer = VK_NULL_HANDLE, readbackBuffer = VK_NULL_HANDLE;
  VkDeviceMemory meshMem = VK_NULL_HANDLE, readbackMem = VK_NULL_HANDLE;

  VkBuffer uniqIdxBuf = VK_NULL_HANDLE;
  VkDeviceMemory uniqIdxBufMem = VK_NULL_HANDLE;
  VkDescriptorBufferInfo uniqIdxBufDescriptor = {};

  VkBuffer rebasedIdxBuf = VK_NULL_HANDLE;
  VkDeviceMemory rebasedIdxBufMem = VK_NULL_HANDLE;

  uint32_t numVerts = action->numIndices;
  VkDeviceSize bufSize = 0;

  uint32_t numViews = 1;

  if(state.dynamicRendering.active)
  {
    numViews = RDCMAX(numViews, Log2Ceil(state.dynamicRendering.viewMask + 1));
  }
  else
  {
    const VulkanCreationInfo::RenderPass &rp = creationInfo.m_RenderPass[state.GetRenderPass()];

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
  if(idxsize == 0)
    idxsize = 4U;

  uint32_t maxIndex = RDCMAX(action->baseVertex, 0) + numVerts - 1;

  uint32_t maxInstance = action->instanceOffset + action->numInstances - 1;

  const VkMemoryAllocateFlagsInfo memFlags = {
      VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO,
      NULL,
      VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT,
  };

  if(action->flags & ActionFlags::Indexed)
  {
    const bool restart = state.primRestartEnable != VK_FALSE;
    bytebuf idxdata;
    rdcarray<uint32_t> indices;
    uint8_t *idx8 = NULL;
    uint16_t *idx16 = NULL;
    uint32_t *idx32 = NULL;

    // fetch ibuffer
    if(state.ibuffer.buf != ResourceId())
      GetBufferData(state.ibuffer.buf, state.ibuffer.offs + action->indexOffset * idxsize,
                    uint64_t(action->numIndices) * idxsize, idxdata);

    // figure out what the maximum index could be, so we can clamp our index buffer to something
    // sane
    uint32_t maxIdx = 0;

    // if there are no active bindings assume the vertex shader is generating its own data
    // and don't clamp the indices
    if(state.vertexBindings.empty())
      maxIdx = ~0U;

    for(uint32_t vb = 0; vb < state.vertexBindings.size(); vb++)
    {
      // only vertex inputs (not instance inputs) count
      if(state.vertexBindings[vb].inputRate == VK_VERTEX_INPUT_RATE_VERTEX)
      {
        uint32_t b = state.vertexBindings[vb].binding;
        if(b >= state.vbuffers.size())
          continue;

        ResourceId buf = state.vbuffers[b].buf;
        VkDeviceSize offs = state.vbuffers[b].offs;

        VkDeviceSize bufsize = creationInfo.m_Buffer[buf].size;

        // the maximum valid index on this particular input is the one that reaches
        // the end of the buffer. The maximum valid index at all is the one that reads
        // off the end of ALL buffers (so we max it with any other maxindex value
        // calculated).
        if(state.vbuffers[b].stride > 0)
          maxIdx = RDCMAX(maxIdx, uint32_t((bufsize - offs) / state.vbuffers[b].stride));
      }
    }

    // in case the vertex buffers were set but had invalid stride (0), max with the number
    // of vertices too. This is fine since the max here is just a conservative limit
    maxIdx = RDCMAX(maxIdx, action->numIndices);

    // do ibuffer rebasing/remapping

    if(idxsize == 4)
      idx32 = (uint32_t *)&idxdata[0];
    else if(idxsize == 1)
      idx8 = (uint8_t *)&idxdata[0];
    else
      idx16 = (uint16_t *)&idxdata[0];

    // only read as many indices as were available in the buffer
    uint32_t numIndices = RDCMIN(uint32_t(idxdata.size() / idxsize), action->numIndices);

    uint32_t idxclamp = 0;
    if(action->baseVertex < 0)
      idxclamp = uint32_t(-action->baseVertex);

    // grab all unique vertex indices referenced
    for(uint32_t i = 0; i < numIndices; i++)
    {
      uint32_t i32 = 0;
      if(idx32)
        i32 = idx32[i];
      else if(idx16)
        i32 = uint32_t(idx16[i]);
      else if(idx8)
        i32 = uint32_t(idx8[i]);

      // apply baseVertex but clamp to 0 (don't allow index to become negative)
      if(i32 < idxclamp)
        i32 = 0;
      else if(action->baseVertex < 0)
        i32 -= idxclamp;
      else if(action->baseVertex > 0)
        i32 += action->baseVertex;

      // we clamp to maxIdx here, to avoid any invalid indices like 0xffffffff
      // from filtering through. Worst case we index to the end of the vertex
      // buffers which is generally much more reasonable
      i32 = RDCMIN(maxIdx, i32);

      // ignore primitive restart indices
      if(restart && i32 == (0xffffffff >> ((4 - idxsize) * 8)))
        continue;

      auto it = std::lower_bound(indices.begin(), indices.end(), i32);

      if(it != indices.end() && *it == i32)
        continue;

      indices.insert(it - indices.begin(), i32);
    }

    // if we read out of bounds, we'll also have a 0 index being referenced
    // (as 0 is read). Don't insert 0 if we already have 0 though
    if(numIndices < action->numIndices && (indices.empty() || indices[0] != 0))
      indices.insert(0, 0);

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
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
    };

    // the flag is the same for KHR and EXT
    if(storageMode != Binding)
      bufInfo.usage |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

    vkr = m_pDriver->vkCreateBuffer(dev, &bufInfo, NULL, &uniqIdxBuf);
    CheckVkResult(vkr);

    uniqIdxBufDescriptor.buffer = uniqIdxBuf;
    uniqIdxBufDescriptor.offset = 0;
    uniqIdxBufDescriptor.range = VK_WHOLE_SIZE;

    VkMemoryRequirements mrq = {0};
    m_pDriver->vkGetBufferMemoryRequirements(dev, uniqIdxBuf, &mrq);

    VkMemoryAllocateInfo allocInfo = {
        VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        NULL,
        mrq.size,
        m_pDriver->GetUploadMemoryIndex(mrq.memoryTypeBits),
    };

    if(storageMode == KHR_bda)
      allocInfo.pNext = &memFlags;

    vkr = m_pDriver->vkAllocateMemory(dev, &allocInfo, NULL, &uniqIdxBufMem);

    if(vkr == VK_ERROR_OUT_OF_DEVICE_MEMORY || vkr == VK_ERROR_OUT_OF_HOST_MEMORY)
    {
      ret.vsout.status = StringFormat::Fmt("Failed to allocate %llu bytes", mrq.size);
      RDCERR("%s", ret.vsout.status.c_str());
      return;
    }

    CheckVkResult(vkr);

    vkr = m_pDriver->vkBindBufferMemory(dev, uniqIdxBuf, uniqIdxBufMem, 0);
    CheckVkResult(vkr);

    byte *idxData = NULL;
    vkr = m_pDriver->vkMapMemory(m_Device, uniqIdxBufMem, 0, VK_WHOLE_SIZE, 0, (void **)&idxData);
    CheckVkResult(vkr);
    if(vkr != VK_SUCCESS || !idxData)
    {
      if(!idxData)
      {
        RDCERR("Manually reporting failed memory map");
        CheckVkResult(VK_ERROR_MEMORY_MAP_FAILED);
      }
      ret.vsout.status = "Couldn't read back vertex output data from GPU";
      return;
    }

    memcpy(idxData, &indices[0], indices.size() * sizeof(uint32_t));

    VkMappedMemoryRange range = {
        VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE, NULL, uniqIdxBufMem, 0, VK_WHOLE_SIZE,
    };

    vkr = m_pDriver->vkFlushMappedMemoryRanges(m_Device, 1, &range);
    CheckVkResult(vkr);

    m_pDriver->vkUnmapMemory(m_Device, uniqIdxBufMem);

    // rebase existing index buffer to point to the right elements in our stream-out'd
    // vertex buffer
    for(uint32_t i = 0; i < numIndices; i++)
    {
      uint32_t i32 = 0;
      if(idx32)
        i32 = idx32[i];
      else if(idx16)
        i32 = uint32_t(idx16[i]);
      else if(idx8)
        i32 = uint32_t(idx8[i]);

      // preserve primitive restart indices
      if(restart && i32 == (0xffffffff >> ((4 - idxsize) * 8)))
        continue;

      // apply baseVertex but clamp to 0 (don't allow index to become negative)
      if(i32 < idxclamp)
        i32 = 0;
      else if(action->baseVertex < 0)
        i32 -= idxclamp;
      else if(action->baseVertex > 0)
        i32 += action->baseVertex;

      if(idx32)
        idx32[i] = uint32_t(indexRemap[i32]);
      else if(idx16)
        idx16[i] = uint16_t(indexRemap[i32]);
      else if(idx8)
        idx8[i] = uint8_t(indexRemap[i32]);
    }

    bufInfo.size = RDCMAX((VkDeviceSize)64, (VkDeviceSize)idxdata.size());
    bufInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

    vkr = m_pDriver->vkCreateBuffer(dev, &bufInfo, NULL, &rebasedIdxBuf);
    CheckVkResult(vkr);

    m_pDriver->vkGetBufferMemoryRequirements(dev, rebasedIdxBuf, &mrq);

    allocInfo.allocationSize = mrq.size;
    allocInfo.memoryTypeIndex = m_pDriver->GetUploadMemoryIndex(mrq.memoryTypeBits);

    vkr = m_pDriver->vkAllocateMemory(dev, &allocInfo, NULL, &rebasedIdxBufMem);

    if(vkr == VK_ERROR_OUT_OF_DEVICE_MEMORY || vkr == VK_ERROR_OUT_OF_HOST_MEMORY)
    {
      RDCWARN("Failed to allocate %llu bytes for rebased index buffer", mrq.size);
      ret.vsout.status = StringFormat::Fmt("Failed to allocate %llu bytes", mrq.size);
      return;
    }

    CheckVkResult(vkr);

    vkr = m_pDriver->vkBindBufferMemory(dev, rebasedIdxBuf, rebasedIdxBufMem, 0);
    CheckVkResult(vkr);

    vkr = m_pDriver->vkMapMemory(m_Device, rebasedIdxBufMem, 0, VK_WHOLE_SIZE, 0, (void **)&idxData);
    CheckVkResult(vkr);
    if(vkr != VK_SUCCESS || !idxData)
    {
      if(!idxData)
      {
        RDCERR("Manually reporting failed memory map");
        CheckVkResult(VK_ERROR_MEMORY_MAP_FAILED);
      }
      ret.vsout.status = "Couldn't read back vertex output data from GPU";
      return;
    }

    memcpy(idxData, idxdata.data(), idxdata.size());

    VkMappedMemoryRange rebasedRange = {
        VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE, NULL, rebasedIdxBufMem, 0, VK_WHOLE_SIZE,
    };

    vkr = m_pDriver->vkFlushMappedMemoryRanges(m_Device, 1, &rebasedRange);
    CheckVkResult(vkr);

    m_pDriver->vkUnmapMemory(m_Device, rebasedIdxBufMem);
  }

  uint32_t baseSpecConstant = 0;

  bytebuf specData;
  rdcarray<VkSpecializationMapEntry> specEntries;

  VkGraphicsPipelineCreateInfo pipeCreateInfo;

  // get pipeline create info
  m_pDriver->GetShaderCache()->MakeGraphicsPipelineInfo(pipeCreateInfo, state.graphics.pipeline);

  VkShaderCreateInfoEXT shaderCreateInfo;

  // get shader object create info
  m_pDriver->GetShaderCache()->MakeShaderObjectInfo(
      shaderCreateInfo, state.shaderObjects[(size_t)ShaderStage::Vertex]);

  // copy over specialization info
  const VkSpecializationInfo *prevSpecInfo = NULL;

  if(state.graphics.shaderObject)
  {
    prevSpecInfo = shaderCreateInfo.pSpecializationInfo;
  }
  else
  {
    for(uint32_t s = 0; s < pipeCreateInfo.stageCount; s++)
    {
      if(pipeCreateInfo.pStages[s].stage == VK_SHADER_STAGE_VERTEX_BIT)
      {
        prevSpecInfo = pipeCreateInfo.pStages[s].pSpecializationInfo;
        break;
      }
    }
  }

  if(prevSpecInfo)
  {
    specData.assign((const byte *)prevSpecInfo->pData, prevSpecInfo->dataSize);
    specEntries.assign(prevSpecInfo->pMapEntries, prevSpecInfo->mapEntryCount);
  }

  // don't overlap with existing pipeline constants
  for(const VkSpecializationMapEntry &specConst : specEntries)
    baseSpecConstant = RDCMAX(baseSpecConstant, specConst.constantID + 1);

  uint32_t bufStride = 0;
  rdcarray<uint32_t> modSpirv = moduleInfo.spirv.GetSPIRV();

  struct CompactedAttrBuffer
  {
    VkDeviceMemory mem;
    VkBuffer buf;
    VkDescriptorBufferInfo descriptor;
  };

  rdcarray<uint32_t> attrInstDivisor;
  rdcarray<CompactedAttrBuffer> vbuffers;
  vbuffers.resize(MeshOutputBufferArraySize);

  {
    rdcarray<VkWriteDescriptorSet> descWrites;
    descWrites.resize(MeshOutputBufferArraySize);
    uint32_t numWrites = 0;

    RDCASSERT(state.vertexAttributes.size() <= MeshOutputBufferArraySize);

    // we fetch the vertex buffer data up front here since there's a very high chance of either
    // overlap due to interleaved attributes, or no overlap and no wastage due to separate compact
    // attributes.
    rdcarray<bytebuf> origVBs;
    origVBs.reserve(16);

    for(uint32_t vb = 0; vb < state.vertexBindings.size(); vb++)
    {
      uint32_t binding = state.vertexBindings[vb].binding;
      if(binding >= state.vbuffers.size())
      {
        origVBs.push_back(bytebuf());
        continue;
      }

      VkDeviceSize offs = state.vbuffers[binding].offs;
      VkDeviceSize stride = state.vbuffers[binding].stride;
      uint64_t len = 0;

      if(state.vertexBindings[vb].inputRate == VK_VERTEX_INPUT_RATE_INSTANCE)
      {
        len = (uint64_t(maxInstance) + 1) * stride;

        offs += action->instanceOffset * stride;
      }
      else
      {
        len = (uint64_t(maxIndex) + 1) * stride;

        offs += action->vertexOffset * stride;
      }

      len = RDCMIN(len, state.vbuffers[binding].size);

      origVBs.push_back(bytebuf());
      if(state.vbuffers[binding].buf != ResourceId())
        GetBufferData(state.vbuffers[binding].buf, offs, len, origVBs.back());
    }

    for(uint32_t i = 0; i < state.vertexAttributes.size(); i++)
    {
      const VkVertexInputAttributeDescription2EXT &attrDesc = state.vertexAttributes[i];
      uint32_t attr = attrDesc.location;

      RDCASSERT(attr < 64);
      if(attr >= vbuffers.size())
      {
        RDCERR("Attribute index too high! Resize array.");
        continue;
      }

      uint32_t instDivisor = ~0U;
      size_t stride = 1;

      const byte *origVBBegin = NULL;
      const byte *origVBEnd = NULL;

      for(uint32_t vb = 0; vb < state.vertexBindings.size(); vb++)
      {
        const VkVertexInputBindingDescription2EXT &vbDesc = state.vertexBindings[vb];
        if(vbDesc.binding == attrDesc.binding)
        {
          origVBBegin = origVBs[vb].data() + attrDesc.offset;
          origVBEnd = origVBs[vb].data() + origVBs[vb].size();

          if(origVBs[vb].empty())
            origVBBegin = origVBEnd = NULL;

          stride = vbDesc.stride;
          if(vbDesc.inputRate == VK_VERTEX_INPUT_RATE_INSTANCE)
            instDivisor = vbDesc.divisor;
          else
            instDivisor = ~0U;
          break;
        }
      }

      if(attrDesc.binding < state.vbuffers.size())
        stride = (size_t)state.vbuffers[attrDesc.binding].stride;

      // in some limited cases, provided we added the UNIFORM_TEXEL_BUFFER usage bit, we could use
      // the original buffers here as-is and read out of them. However it is likely that the offset
      // is not a multiple of the minimum texel buffer offset for at least some of the buffers if
      // not all of them, so we simplify the code here by *always* reading back the vertex buffer
      // data and uploading a compacted version.

      // we also need to handle the case where the format is not natively supported as a texel
      // buffer.

      // we used to use expanded texel buffers (i.e. expand to uint4, float4, int4 etc from any
      // smaller format) but since we want to support buffer_device_address to avoid descriptor
      // patching entirely it's easier to have an SSBO-based path. For that reason we only upload
      // this data as 16-byte strided data and read it out of a uint4[] then bitcast to int4 or
      // float4. That way the uint4[] SSBO can be easily substituted for a buffer device address
      VkFormat origFormat = attrDesc.format;
      VkFormat expandedFormat = VK_FORMAT_R32G32B32A32_SFLOAT;

      if(Is64BitFormat(origFormat))
        expandedFormat = VK_FORMAT_R32G32B32A32_UINT;
      else if(IsUIntFormat(origFormat))
        expandedFormat = VK_FORMAT_R32G32B32A32_UINT;
      else if(IsSIntFormat(origFormat))
        expandedFormat = VK_FORMAT_R32G32B32A32_SINT;

      uint32_t origElemSize = (uint32_t)GetByteSize(1, 1, 1, origFormat, 0);
      uint32_t elemSize = (uint32_t)GetByteSize(1, 1, 1, expandedFormat, 0);

      // 64-bit values are packed as uvec2
      if(Is64BitFormat(origFormat))
        elemSize *= 2;

      // used for interpreting the original data, if we're upcasting
      ResourceFormat fmt = MakeResourceFormat(origFormat);

      {
        VkBufferCreateInfo bufInfo = {
            VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            NULL,
            0,
            elemSize * (maxIndex + 1),
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        };

        if(instDivisor != ~0U)
          bufInfo.size = elemSize * (maxInstance + 1);

        // the flag is the same for KHR and EXT
        if(storageMode != Binding)
          bufInfo.usage |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

        vkr = m_pDriver->vkCreateBuffer(dev, &bufInfo, NULL, &vbuffers[attr].buf);
        CheckVkResult(vkr);

        VkMemoryRequirements mrq = {0};
        m_pDriver->vkGetBufferMemoryRequirements(dev, vbuffers[attr].buf, &mrq);

        VkMemoryAllocateInfo allocInfo = {
            VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            NULL,
            mrq.size,
            m_pDriver->GetUploadMemoryIndex(mrq.memoryTypeBits),
        };

        if(storageMode == KHR_bda)
          allocInfo.pNext = &memFlags;

        vkr = m_pDriver->vkAllocateMemory(dev, &allocInfo, NULL, &vbuffers[attr].mem);

        if(vkr == VK_ERROR_OUT_OF_DEVICE_MEMORY || vkr == VK_ERROR_OUT_OF_HOST_MEMORY)
        {
          RDCWARN("Failed to allocate %llu bytes for patched vertex buffer", mrq.size);
          ret.vsout.status = StringFormat::Fmt("Failed to allocate %llu bytes", mrq.size);
          return;
        }

        CheckVkResult(vkr);

        vkr = m_pDriver->vkBindBufferMemory(dev, vbuffers[attr].buf, vbuffers[attr].mem, 0);
        CheckVkResult(vkr);

        byte *dst = NULL;
        vkr =
            m_pDriver->vkMapMemory(m_Device, vbuffers[attr].mem, 0, VK_WHOLE_SIZE, 0, (void **)&dst);
        CheckVkResult(vkr);
        if(vkr != VK_SUCCESS || !dst)
        {
          if(!dst)
          {
            RDCERR("Manually reporting failed memory map");
            CheckVkResult(VK_ERROR_MEMORY_MAP_FAILED);
          }
          ret.vsout.status = "Couldn't read back vertex output data from GPU";
          return;
        }

        const byte *dstBase = dst;
        (void)dstBase;

        const byte *dstEnd = dst + bufInfo.size;

        if(dst)
        {
          FloatVector defaultValue(0.0f, 0.0f, 0.0f, 1.0f);
          if(fmt.compType == CompType::UInt || fmt.compType == CompType::SInt || fmt.compCount == 4)
            defaultValue.w = 0.0f;

          const byte *src = origVBBegin;

          // fast memcpy compaction case for regular 32-bit types. Any type like R32G32B32 or so on
          // can be memcpy'd into place and read, since we discard any unused components and there's
          // no re-interpretation needed.
          if(fmt.type == ResourceFormatType::Regular && fmt.compByteWidth == 4)
          {
            size_t expandedComponentBytes = sizeof(FloatVector) - origElemSize;

            while(src < origVBEnd && dst < dstEnd)
            {
              if(expandedComponentBytes > 0)
                memcpy(dst + origElemSize, ((byte *)&defaultValue) + origElemSize,
                       expandedComponentBytes);
              memcpy(dst, src, origElemSize);

              // advance by the *destination* element size of 16 bytes
              dst += elemSize;
              src += stride;
            }

            // fill the rest with default values
            while(dst < dstEnd)
            {
              memcpy(dst, &defaultValue, sizeof(FloatVector));
              dst += elemSize;
            }
          }
          else
          {
            uint32_t zero = 0;

            // upcasting path
            if(Is64BitFormat(origFormat))
            {
              while(src < origVBEnd && dst < dstEnd)
              {
                // the 64-bit value (especially for doubles) is already in "packed uvec2" order,
                // with least significant 32-bits first, so we can copy directly
                memcpy(dst, src, sizeof(uint64_t) * fmt.compCount);
                dst += sizeof(uint64_t) * fmt.compCount;

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

              // fill the rest with default values
              while(dst < dstEnd)
              {
                memcpy(dst, &defaultValue, sizeof(FloatVector));
                dst += elemSize;
              }
            }
          }
        }

        VkMappedMemoryRange range = {
            VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE, NULL, vbuffers[attr].mem, 0, VK_WHOLE_SIZE,
        };

        vkr = m_pDriver->vkFlushMappedMemoryRanges(m_Device, 1, &range);
        CheckVkResult(vkr);

        m_pDriver->vkUnmapMemory(m_Device, vbuffers[attr].mem);
      }

      attrInstDivisor.resize(RDCMAX(attrInstDivisor.size(), size_t(attr + 1)));
      attrInstDivisor[attr] = instDivisor;

      vbuffers[attr].descriptor.buffer = vbuffers[attr].buf;
      vbuffers[attr].descriptor.offset = 0;
      vbuffers[attr].descriptor.range = VK_WHOLE_SIZE;

      if(!descSets.empty())
      {
        descWrites[numWrites].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descWrites[numWrites].dstSet = descSets[0];
        descWrites[numWrites].dstBinding = 2;
        descWrites[numWrites].dstArrayElement = attr;
        descWrites[numWrites].descriptorCount = 1;
        descWrites[numWrites].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descWrites[numWrites].pBufferInfo = &vbuffers[attr].descriptor;
        numWrites++;
      }
    }

    // add a write of the index buffer
    if(uniqIdxBuf != VK_NULL_HANDLE && !descSets.empty())
    {
      descWrites[numWrites].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      descWrites[numWrites].dstSet = descSets[0];
      descWrites[numWrites].dstBinding = 1;
      descWrites[numWrites].dstArrayElement = 0;
      descWrites[numWrites].descriptorCount = 1;
      descWrites[numWrites].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
      descWrites[numWrites].pBufferInfo = &uniqIdxBufDescriptor;
      numWrites++;
    }

    if(numWrites > 0)
      m_pDriver->vkUpdateDescriptorSets(dev, numWrites, descWrites.data(), 0, NULL);
  }

  if(!Vulkan_Debug_PostVSDumpDirPath().empty())
    FileIO::WriteAll(Vulkan_Debug_PostVSDumpDirPath() + "/debug_postvs_vert.spv", modSpirv);

  ConvertToMeshOutputCompute(*refl, *vertShad.patchData, vertShad.entryPoint, storageMode,
                             attrInstDivisor, action, numVerts, numViews, baseSpecConstant,
                             modSpirv, bufStride);

  if(!Vulkan_Debug_PostVSDumpDirPath().empty())
    FileIO::WriteAll(Vulkan_Debug_PostVSDumpDirPath() + "/debug_postvs_comp.spv", modSpirv);

  {
    // now that we know the stride, create buffer of sufficient size
    // this can't just be bufStride * num unique indices per instance, as we don't
    // have a compact 0-based index to index into the buffer. We must use
    // index-minIndex which is 0-based but potentially sparse, so this buffer may
    // be more or less wasteful
    VkBufferCreateInfo bufInfo = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};

    // set bufSize
    bufSize = bufInfo.size = uint64_t(numVerts) * uint64_t(action->numInstances) *
                             uint64_t(bufStride) * uint64_t(numViews);

    bufInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bufInfo.usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    bufInfo.usage |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    bufInfo.usage |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;

    // the flag is the same for KHR and EXT
    if(storageMode != Binding)
      bufInfo.usage |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

    vkr = m_pDriver->vkCreateBuffer(dev, &bufInfo, NULL, &meshBuffer);
    CheckVkResult(vkr);

    bufInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

    vkr = m_pDriver->vkCreateBuffer(dev, &bufInfo, NULL, &readbackBuffer);
    CheckVkResult(vkr);

    VkMemoryRequirements mrq = {0};
    m_pDriver->vkGetBufferMemoryRequirements(dev, meshBuffer, &mrq);

    if(mrq.size > m_pDriver->GetMaxMemoryAllocationSize())
    {
      ret.vsout.status = StringFormat::Fmt("OOM %llu bytes Max %llu bytes", mrq.size,
                                           m_pDriver->GetMaxMemoryAllocationSize());
      return;
    }

    VkMemoryAllocateInfo allocInfo = {
        VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        NULL,
        mrq.size,
        m_pDriver->GetGPULocalMemoryIndex(mrq.memoryTypeBits),
    };

    if(storageMode == KHR_bda)
      allocInfo.pNext = &memFlags;

    vkr = m_pDriver->vkAllocateMemory(dev, &allocInfo, NULL, &meshMem);

    if(vkr == VK_ERROR_OUT_OF_DEVICE_MEMORY || vkr == VK_ERROR_OUT_OF_HOST_MEMORY)
    {
      RDCWARN("Failed to allocate %llu bytes for output vertex SSBO", mrq.size);
      ret.vsout.status = StringFormat::Fmt("Failed to allocate %llu bytes", mrq.size);
      return;
    }

    CheckVkResult(vkr);

    vkr = m_pDriver->vkBindBufferMemory(dev, meshBuffer, meshMem, 0);
    CheckVkResult(vkr);

    m_pDriver->vkGetBufferMemoryRequirements(dev, readbackBuffer, &mrq);

    allocInfo.pNext = NULL;
    allocInfo.memoryTypeIndex = m_pDriver->GetReadbackMemoryIndex(mrq.memoryTypeBits);

    vkr = m_pDriver->vkAllocateMemory(dev, &allocInfo, NULL, &readbackMem);

    if(vkr == VK_ERROR_OUT_OF_DEVICE_MEMORY || vkr == VK_ERROR_OUT_OF_HOST_MEMORY)
    {
      RDCWARN("Failed to allocate %llu bytes for readback memory", mrq.size);
      ret.vsout.status = StringFormat::Fmt("Failed to allocate %llu bytes", mrq.size);
      return;
    }

    CheckVkResult(vkr);

    vkr = m_pDriver->vkBindBufferMemory(dev, readbackBuffer, readbackMem, 0);
    CheckVkResult(vkr);
  }

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
  CheckVkResult(vkr);

  compPipeInfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  compPipeInfo.stage.module = module;
  compPipeInfo.stage.pName = PatchedMeshOutputEntryPoint;
  compPipeInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;

  // append our own if we're using BDA
  if(storageMode != Binding)
  {
    // ensure we're 64-bit aligned first
    specData.resize(AlignUp(specData.size(), (size_t)8));

    uint32_t baseOffset = (uint32_t)specData.size();

    rdcarray<uint64_t> addresses;
    addresses.resize(MeshOutputBufferArraySize + 2);

    for(uint32_t i = 0; i <= MeshOutputBufferArraySize + 1; i++)
    {
      RDCCOMPILE_ASSERT(VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO ==
                            VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO_EXT,
                        "KHR and EXT buffer_device_address should be interchangeable here.");
      VkBufferDeviceAddressInfo getAddressInfo = {VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO};

      if(i < MeshOutputBufferArraySize)
        getAddressInfo.buffer = vbuffers[i].buf;
      else if(i == MeshOutputBufferArraySize)
        getAddressInfo.buffer = uniqIdxBuf;
      else if(i == MeshOutputBufferArraySize + 1)
        getAddressInfo.buffer = meshBuffer;

      // skip
      if(getAddressInfo.buffer == VK_NULL_HANDLE)
        continue;

      if(storageMode == KHR_bda)
        addresses[i] = m_pDriver->vkGetBufferDeviceAddress(dev, &getAddressInfo);
      else
        addresses[i] = m_pDriver->vkGetBufferDeviceAddressEXT(dev, &getAddressInfo);

      VkSpecializationMapEntry entry;
      entry.offset = baseOffset + i * sizeof(uint64_t);
      entry.constantID = baseSpecConstant + i * 2 + 0;

      // for EXT we have one 64-bit spec constant per address, for KHR we have a uvec2 - two
      // constants
      if(storageMode == EXT_bda)
      {
        entry.size = sizeof(uint64_t);
        specEntries.push_back(entry);
      }
      else
      {
        entry.size = sizeof(uint32_t);
        specEntries.push_back(entry);

        entry.offset += sizeof(uint32_t);
        entry.constantID++;

        entry.size = sizeof(uint32_t);
        specEntries.push_back(entry);
      }
    }

    specData.append((const byte *)addresses.data(), addresses.byteSize());
  }

  VkSpecializationInfo specInfo = {};
  specInfo.dataSize = specData.size();
  specInfo.pData = specData.data();
  specInfo.mapEntryCount = (uint32_t)specEntries.size();
  specInfo.pMapEntries = specEntries.data();

  compPipeInfo.stage.pSpecializationInfo = &specInfo;

  // create new pipeline
  VkPipeline pipe;
  vkr = m_pDriver->vkCreateComputePipelines(m_Device, VK_NULL_HANDLE, 1, &compPipeInfo, NULL, &pipe);

  if(vkr != VK_SUCCESS)
  {
    ret.vsout.status =
        StringFormat::Fmt("Failed to create patched compute pipeline: %s", ToStr(vkr).c_str());
    RDCERR("%s", ret.vsout.status.c_str());
    return;
  }

  // make copy of state to draw from
  VulkanRenderState modifiedstate = state;

  // bind created pipeline to partial replay state
  modifiedstate.compute.pipeline = GetResID(pipe);

  // move graphics descriptor sets onto the compute pipe.
  modifiedstate.compute.descSets = modifiedstate.graphics.descSets;

  if(!descSets.empty())
  {
    // replace descriptor set IDs with our temporary sets. The offsets we keep the same. If the
    // original draw had no sets, we ensure there's room (with no offsets needed)
    if(modifiedstate.compute.descSets.empty())
      modifiedstate.compute.descSets.resize(1);

    for(size_t i = 0; i < descSets.size(); i++)
    {
      modifiedstate.compute.descSets[i].pipeLayout = GetResID(pipeLayout);
      modifiedstate.compute.descSets[i].descSet = GetResID(descSets[i]);
    }
  }
  else
  {
    for(size_t i = 0; i < modifiedstate.compute.descSets.size(); i++)
      modifiedstate.compute.descSets[i].pipeLayout = GetResID(pipeLayout);
  }

  {
    VkCommandBuffer cmd = m_pDriver->GetNextCmd();

    if(cmd == VK_NULL_HANDLE)
      return;

    VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL,
                                          VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};

    vkr = ObjDisp(dev)->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
    CheckVkResult(vkr);

    // fill destination buffer with 0s to ensure unwritten vertices have sane data
    ObjDisp(dev)->CmdFillBuffer(Unwrap(cmd), Unwrap(meshBuffer), 0, bufSize, 0);

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
        VK_STRUCTURE_TYPE_MEMORY_BARRIER,
        NULL,
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
    fetchdesc.range = bufSize;

    if(!descSets.empty())
    {
      VkWriteDescriptorSet write = {
          VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, descSets[0], 0,   0, 1,
          VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,      NULL, &fetchdesc,  NULL};
      m_pDriver->vkUpdateDescriptorSets(dev, 1, &write, 0, NULL);
    }

    // do single draw
    modifiedstate.BindPipeline(m_pDriver, cmd, VulkanRenderState::BindCompute, true);
    uint64_t totalVerts = numVerts * uint64_t(action->numInstances) * uint64_t(numViews);

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
        0,
        0,
        bufSize,
    };

    // copy to readback buffer
    ObjDisp(dev)->CmdCopyBuffer(Unwrap(cmd), Unwrap(meshBuffer), Unwrap(readbackBuffer), 1, &bufcopy);

    meshbufbarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    meshbufbarrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
    meshbufbarrier.buffer = Unwrap(readbackBuffer);

    // wait for copy to finish
    DoPipelineBarrier(cmd, 1, &meshbufbarrier);

    vkr = ObjDisp(dev)->EndCommandBuffer(Unwrap(cmd));
    CheckVkResult(vkr);

    // submit & flush so that we don't have to keep pipeline around for a while
    m_pDriver->SubmitCmds();
    m_pDriver->FlushQ();
  }

  for(CompactedAttrBuffer attrBuf : vbuffers)
  {
    m_pDriver->vkDestroyBuffer(dev, attrBuf.buf, NULL);
    m_pDriver->vkFreeMemory(dev, attrBuf.mem, NULL);
  }

  // readback mesh data
  byte *byteData = NULL;
  vkr = m_pDriver->vkMapMemory(m_Device, readbackMem, 0, VK_WHOLE_SIZE, 0, (void **)&byteData);
  CheckVkResult(vkr);
  if(vkr != VK_SUCCESS || !byteData)
  {
    if(!byteData)
    {
      RDCERR("Manually reporting failed memory map");
      CheckVkResult(VK_ERROR_MEMORY_MAP_FAILED);
    }
    ret.vsout.status = "Couldn't read back vertex output data from GPU";
    return;
  }

  VkMappedMemoryRange range = {
      VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE, NULL, readbackMem, 0, VK_WHOLE_SIZE,
  };

  vkr = m_pDriver->vkInvalidateMappedMemoryRanges(m_Device, 1, &range);
  CheckVkResult(vkr);

  // do near/far calculations

  float nearp = 0.1f;
  float farp = 100.0f;

  Vec4f *pos0 = (Vec4f *)byteData;

  bool found = false;

  // expect position at the start of the buffer, as system values are sorted first
  // and position is the first value

  for(uint32_t i = 1;
      refl->outputSignature[0].systemValue == ShaderBuiltin::Position && !found && i < numVerts; i++)
  {
    Vec4f *pos = (Vec4f *)(byteData + i * bufStride);

    DeriveNearFar(*pos, *pos0, nearp, farp, found);

    if(found)
      break;
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

  // fill out m_PostVS.Data
  ret.vsout.topo = MakePrimitiveTopology(state.primitiveTopology, state.patchControlPoints);
  ret.vsout.buf = meshBuffer;
  ret.vsout.bufmem = meshMem;

  ret.vsout.baseVertex = 0;

  ret.vsout.numViews = numViews;

  ret.vsout.vertStride = bufStride;
  ret.vsout.nearPlane = nearp;
  ret.vsout.farPlane = farp;

  ret.vsout.useIndices = bool(action->flags & ActionFlags::Indexed);
  ret.vsout.numVerts = action->numIndices;

  ret.vsout.instStride = 0;
  if(action->flags & ActionFlags::Instanced)
    ret.vsout.instStride = uint32_t(bufSize / (action->numInstances * numViews));

  ret.vsout.idxbuf = VK_NULL_HANDLE;
  if(ret.vsout.useIndices && state.ibuffer.buf != ResourceId())
  {
    VkIndexType type = VK_INDEX_TYPE_UINT16;
    if(idxsize == 4)
      type = VK_INDEX_TYPE_UINT32;
    else if(idxsize == 1)
      type = VK_INDEX_TYPE_UINT8_KHR;

    ret.vsout.idxbuf = rebasedIdxBuf;
    ret.vsout.idxbufmem = rebasedIdxBufMem;
    ret.vsout.idxFmt = type;
  }

  ret.vsout.hasPosOut = refl->outputSignature[0].systemValue == ShaderBuiltin::Position;
  ret.vsout.flipY = state.views.empty() ? false : state.views[0].height < 0.0f;

  if(descpool != VK_NULL_HANDLE)
  {
    // delete descriptors. Technically we don't have to free the descriptor sets, but our tracking
    // on replay doesn't handle destroying children of pooled objects so we do it explicitly anyway.
    m_pDriver->vkFreeDescriptorSets(dev, descpool, (uint32_t)descSets.size(), descSets.data());

    m_pDriver->vkDestroyDescriptorPool(dev, descpool, NULL);

    for(VkDescriptorSetLayout layout : setLayouts)
      m_pDriver->vkDestroyDescriptorSetLayout(dev, layout, NULL);
  }

  // delete pipeline layout
  m_pDriver->vkDestroyPipelineLayout(dev, pipeLayout, NULL);

  // delete pipeline
  m_pDriver->vkDestroyPipeline(dev, pipe, NULL);

  // delete shader/shader module
  m_pDriver->vkDestroyShaderModule(dev, module, NULL);
}

void VulkanReplay::FetchTessGSOut(uint32_t eventId, VulkanRenderState &state)
{
  VulkanCreationInfo &creationInfo = m_pDriver->m_CreationInfo;

  const VulkanCreationInfo::Pipeline &pipeInfo = creationInfo.m_Pipeline[state.graphics.pipeline];

  const ActionDescription *action = m_pDriver->GetAction(eventId);

  VulkanPostVSData &ret = m_PostVS.Data[eventId];

  // set defaults so that we don't try to fetch this output again if something goes wrong and the
  // same event is selected again
  {
    ret.gsout.buf = VK_NULL_HANDLE;
    ret.gsout.bufmem = VK_NULL_HANDLE;
    ret.gsout.instStride = 0;
    ret.gsout.vertStride = 0;
    ret.gsout.numViews = 1;
    ret.gsout.nearPlane = 0.0f;
    ret.gsout.farPlane = 0.0f;
    ret.gsout.useIndices = false;
    ret.gsout.hasPosOut = false;
    ret.gsout.flipY = false;
    ret.gsout.idxbuf = VK_NULL_HANDLE;
    ret.gsout.idxbufmem = VK_NULL_HANDLE;
  }

  if(state.dynamicRendering.viewMask > 1 ||
     (state.GetRenderPass() != ResourceId() &&
      !creationInfo.m_RenderPass[state.GetRenderPass()].subpasses[state.subpass].multiviews.empty()))
  {
    ret.gsout.status = "Multiview is active for this draw, no GS/Tess mesh output is available";
    return;
  }

  // first try geometry stage
  int stageIndex = 3;

  // if there is no such shader bound, try tessellation
  if(state.graphics.shaderObject
         ? !creationInfo.m_ShaderObject[state.shaderObjects[stageIndex]].shad.refl
         : !pipeInfo.shaders[stageIndex].refl)
    stageIndex = 2;

  // if still nothing, do vertex
  if(state.graphics.shaderObject
         ? !creationInfo.m_ShaderObject[state.shaderObjects[stageIndex]].shad.refl
         : !pipeInfo.shaders[stageIndex].refl)
    stageIndex = 0;

  const VulkanCreationInfo::ShaderEntry &shader =
      state.graphics.shaderObject ? creationInfo.m_ShaderObject[state.shaderObjects[stageIndex]].shad
                                  : pipeInfo.shaders[stageIndex];

  ShaderReflection *lastRefl = shader.refl;

  RDCASSERT(lastRefl);

  uint32_t primitiveMultiplier = 1;

  // transform feedback expands strips to lists
  switch(lastRefl->outputTopology)
  {
    case Topology::PointList: ret.gsout.topo = Topology::PointList; break;
    case Topology::LineList:
    case Topology::LineStrip:
      ret.gsout.topo = Topology::LineList;
      primitiveMultiplier = 2;
      break;
    default:
      RDCERR("Unexpected output topology %s", ToStr(lastRefl->outputTopology).c_str());
      DELIBERATE_FALLTHROUGH();
    case Topology::TriangleList:
    case Topology::TriangleStrip:
      ret.gsout.topo = Topology::TriangleList;
      primitiveMultiplier = 3;
      break;
  }

  if(lastRefl->outputSignature.empty())
  {
    // empty output signature
    ret.gsout.buf = VK_NULL_HANDLE;
    ret.gsout.bufmem = VK_NULL_HANDLE;
    ret.gsout.instStride = 0;
    ret.gsout.vertStride = 0;
    ret.gsout.numViews = 1;
    ret.gsout.nearPlane = 0.0f;
    ret.gsout.farPlane = 0.0f;
    ret.gsout.useIndices = false;
    ret.gsout.hasPosOut = false;
    ret.gsout.flipY = false;
    ret.gsout.idxbuf = VK_NULL_HANDLE;
    ret.gsout.idxbufmem = VK_NULL_HANDLE;
    return;
  }

  if(!ObjDisp(m_Device)->CmdBeginTransformFeedbackEXT)
  {
    ret.gsout.status =
        "VK_EXT_transform_feedback extension not available, can't fetch tessellation/geometry "
        "output";
    return;
  }

  const VulkanCreationInfo::ShaderModule &moduleInfo = creationInfo.m_ShaderModule[shader.module];

  rdcarray<uint32_t> modSpirv = moduleInfo.spirv.GetSPIRV();

  uint32_t xfbStride = 0;

  if(!Vulkan_Debug_PostVSDumpDirPath().empty())
    FileIO::WriteAll(Vulkan_Debug_PostVSDumpDirPath() + "/debug_postgs_before.spv", modSpirv);

  // adds XFB annotations in order of the output signature (with the position first)
  AddXFBAnnotations(*lastRefl, *shader.patchData, pipeInfo.rasterizationStream,
                    shader.entryPoint.c_str(), modSpirv, xfbStride);

  if(!Vulkan_Debug_PostVSDumpDirPath().empty())
    FileIO::WriteAll(Vulkan_Debug_PostVSDumpDirPath() + "/debug_postgs_after.spv", modSpirv);

  // create vertex shader with modified code
  VkShaderModuleCreateInfo moduleCreateInfo = {
      VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, NULL,         0,
      modSpirv.size() * sizeof(uint32_t),          &modSpirv[0],
  };

  VkResult vkr = VK_SUCCESS;
  VkDevice dev = m_Device;

  VkShaderModule module;
  vkr = m_pDriver->vkCreateShaderModule(dev, &moduleCreateInfo, NULL, &module);
  CheckVkResult(vkr);

  VkGraphicsPipelineCreateInfo pipeCreateInfo;

  // get pipeline create info
  m_pDriver->GetShaderCache()->MakeGraphicsPipelineInfo(pipeCreateInfo, state.graphics.pipeline);

  VkShaderCreateInfoEXT shaderObjectCreateInfo;

  // get shader object create info
  m_pDriver->GetShaderCache()->MakeShaderObjectInfo(shaderObjectCreateInfo,
                                                    state.shaderObjects[stageIndex]);

  shaderObjectCreateInfo.codeSize = modSpirv.size() * sizeof(uint32_t);
  shaderObjectCreateInfo.pCode = &modSpirv[0];

  VkPipelineRasterizationStateCreateInfo *rs =
      (VkPipelineRasterizationStateCreateInfo *)pipeCreateInfo.pRasterizationState;
  if(rs)
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
  CheckVkResult(vkr);

  VkFramebufferCreateInfo fbinfo = {
      VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO, NULL, 0, rp, 0, NULL, 16U, 16U, 1,
  };

  vkr = m_pDriver->vkCreateFramebuffer(m_Device, &fbinfo, NULL, &fb);
  CheckVkResult(vkr);

  pipeCreateInfo.renderPass = rp;
  pipeCreateInfo.subpass = 0;

  // don't use dynamic rendering
  RemoveNextStruct(&pipeCreateInfo, VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO);

  VkPipeline pipe = VK_NULL_HANDLE;
  VkShaderEXT shad = VK_NULL_HANDLE;

  if(state.graphics.shaderObject)
    vkr = m_pDriver->vkCreateShadersEXT(m_Device, 1, &shaderObjectCreateInfo, NULL, &shad);
  else
    vkr = m_pDriver->vkCreateGraphicsPipelines(m_Device, VK_NULL_HANDLE, 1, &pipeCreateInfo, NULL,
                                               &pipe);

  CheckVkResult(vkr);

  if(state.graphics.shaderObject)
    state.shaderObjects[stageIndex] = GetResID(shad);
  else
    state.graphics.pipeline = GetResID(pipe);

  state.rastDiscardEnable = true;
  state.SetFramebuffer(m_pDriver, GetResID(fb));
  state.SetRenderPass(GetResID(rp));
  state.dynamicRendering = VulkanRenderState::DynamicRendering();
  state.subpass = 0;
  state.renderArea.offset.x = 0;
  state.renderArea.offset.y = 0;
  state.renderArea.extent.width = 16;
  state.renderArea.extent.height = 16;

  // disable any existing XFB
  state.xfbbuffers.clear();
  state.xfbcounters.clear();

  state.subpassContents = VK_SUBPASS_CONTENTS_INLINE;
  state.dynamicRendering.flags &= ~VK_RENDERING_CONTENTS_SECONDARY_COMMAND_BUFFERS_BIT;

  if(m_PostVS.XFBQueryPoolSize < action->numInstances)
  {
    if(m_PostVS.XFBQueryPoolSize != VK_NULL_HANDLE)
      m_pDriver->vkDestroyQueryPool(m_Device, m_PostVS.XFBQueryPool, NULL);

    VkQueryPoolCreateInfo info = {
        VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
        NULL,
        0,
        VK_QUERY_TYPE_TRANSFORM_FEEDBACK_STREAM_EXT,
        action->numInstances,
        0,
    };

    vkr = m_pDriver->vkCreateQueryPool(m_Device, &info, NULL, &m_PostVS.XFBQueryPool);
    CheckVkResult(vkr);

    m_PostVS.XFBQueryPoolSize = action->numInstances;
  }

  VkBuffer meshBuffer = VK_NULL_HANDLE;
  VkDeviceMemory meshMem = VK_NULL_HANDLE;

  // start with bare minimum size, which might be enough if no expansion happens
  VkDeviceSize bufferSize = 0;
  VkDeviceSize dataSize =
      uint64_t(action->numIndices) * uint64_t(action->numInstances) * uint64_t(xfbStride);

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
    CheckVkResult(vkr);

    VkMemoryRequirements mrq = {0};
    m_pDriver->vkGetBufferMemoryRequirements(dev, meshBuffer, &mrq);

    VkMemoryAllocateInfo allocInfo = {
        VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        NULL,
        mrq.size,
        m_pDriver->GetGPULocalMemoryIndex(mrq.memoryTypeBits),
    };

    vkr = m_pDriver->vkAllocateMemory(dev, &allocInfo, NULL, &meshMem);

    if(vkr == VK_ERROR_OUT_OF_DEVICE_MEMORY || vkr == VK_ERROR_OUT_OF_HOST_MEMORY)
    {
      RDCWARN("Output allocation for %llu bytes failed fetching tessellation/geometry output.",
              mrq.size);

      ret.gsout.status = StringFormat::Fmt("Failed to allocate %llu bytes", mrq.size);

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

    CheckVkResult(vkr);

    vkr = m_pDriver->vkBindBufferMemory(dev, meshBuffer, meshMem, 0);
    CheckVkResult(vkr);

    VkCommandBuffer cmd = m_pDriver->GetNextCmd();

    if(cmd == VK_NULL_HANDLE)
      return;

    VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL,
                                          VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};

    vkr = ObjDisp(dev)->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
    CheckVkResult(vkr);

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

    state.BeginRenderPassAndApplyState(m_pDriver, cmd, VulkanRenderState::BindGraphics, false);

    ObjDisp(cmd)->CmdBeginQuery(Unwrap(cmd), Unwrap(m_PostVS.XFBQueryPool), 0, 0);

    ObjDisp(cmd)->CmdBindTransformFeedbackBuffersEXT(Unwrap(cmd), 0, 1, UnwrapPtr(meshBuffer),
                                                     &meshbufbarrier.offset, &meshbufbarrier.size);

    ObjDisp(cmd)->CmdBeginTransformFeedbackEXT(Unwrap(cmd), 0, 1, NULL, NULL);

    m_pDriver->ReplayDraw(cmd, *action);

    ObjDisp(cmd)->CmdEndTransformFeedbackEXT(Unwrap(cmd), 0, 1, NULL, NULL);

    ObjDisp(cmd)->CmdEndQuery(Unwrap(cmd), Unwrap(m_PostVS.XFBQueryPool), 0);

    state.EndRenderPass(cmd);

    vkr = ObjDisp(dev)->EndCommandBuffer(Unwrap(cmd));
    CheckVkResult(vkr);

    m_pDriver->SubmitCmds();
    m_pDriver->FlushQ();

    vkr = ObjDisp(dev)->GetQueryPoolResults(
        Unwrap(dev), Unwrap(m_PostVS.XFBQueryPool), 0, 1, sizeof(VkXfbQueryResult), &queryResult,
        sizeof(VkXfbQueryResult), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);
    CheckVkResult(vkr);

    VkDeviceSize generatedSize = queryResult.numPrimitivesGenerated * 3 * xfbStride;

    // output buffer isn't big enough, delete it and re-run so we recreate it larger
    if(generatedSize > dataSize)
      dataSize = generatedSize;
  }

  rdcarray<VulkanPostVSData::InstData> instData;

  // instanced draws must be replayed one at a time so we can record the number of primitives from
  // each drawcall, as due to expansion this can vary per-instance.
  if(action->flags & ActionFlags::Instanced && action->numInstances > 1)
  {
    VkCommandBuffer cmd = m_pDriver->GetNextCmd();

    if(cmd == VK_NULL_HANDLE)
      return;

    VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL,
                                          VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};

    vkr = ObjDisp(dev)->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
    CheckVkResult(vkr);

    ObjDisp(dev)->CmdResetQueryPool(Unwrap(cmd), Unwrap(m_PostVS.XFBQueryPool), 0,
                                    action->numInstances);

    state.BeginRenderPassAndApplyState(m_pDriver, cmd, VulkanRenderState::BindGraphics, false);

    ActionDescription act = *action;

    // do incremental draws to get the output size. We have to do this O(N^2) style because
    // there's no way to replay only a single instance. We have to replay 1, 2, 3, ... N
    // instances and count the total number of verts each time, then we can see from the
    // difference how much each instance wrote.
    for(uint32_t inst = 1; inst <= action->numInstances; inst++)
    {
      ObjDisp(cmd)->CmdBeginQuery(Unwrap(cmd), Unwrap(m_PostVS.XFBQueryPool), inst - 1, 0);

      VkDeviceSize offset = 0;
      ObjDisp(cmd)->CmdBindTransformFeedbackBuffersEXT(Unwrap(cmd), 0, 1, UnwrapPtr(meshBuffer),
                                                       &offset, &bufferSize);

      ObjDisp(cmd)->CmdBeginTransformFeedbackEXT(Unwrap(cmd), 0, 1, NULL, NULL);

      act.numInstances = inst;
      m_pDriver->ReplayDraw(cmd, act);

      ObjDisp(cmd)->CmdEndTransformFeedbackEXT(Unwrap(cmd), 0, 1, NULL, NULL);

      ObjDisp(cmd)->CmdEndQuery(Unwrap(cmd), Unwrap(m_PostVS.XFBQueryPool), inst - 1);

      // Instanced draws with a wild number of instances can hang the GPU, sync after every 1000
      if((inst % 1000) == 0)
      {
        state.EndRenderPass(cmd);

        vkr = ObjDisp(dev)->EndCommandBuffer(Unwrap(cmd));
        CheckVkResult(vkr);

        cmd = m_pDriver->GetNextCmd();

        vkr = ObjDisp(dev)->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
        CheckVkResult(vkr);

        state.BeginRenderPassAndApplyState(m_pDriver, cmd, VulkanRenderState::BindGraphics, false);
      }
    }

    state.EndRenderPass(cmd);

    vkr = ObjDisp(dev)->EndCommandBuffer(Unwrap(cmd));
    CheckVkResult(vkr);

    m_pDriver->SubmitCmds();
    m_pDriver->FlushQ();

    rdcarray<VkXfbQueryResult> queryResults;
    queryResults.resize(action->numInstances);
    vkr = ObjDisp(dev)->GetQueryPoolResults(
        Unwrap(dev), Unwrap(m_PostVS.XFBQueryPool), 0, action->numInstances,
        sizeof(VkXfbQueryResult) * action->numInstances, queryResults.data(),
        sizeof(VkXfbQueryResult), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);
    CheckVkResult(vkr);

    uint64_t prevVertCount = 0;

    for(uint32_t inst = 0; inst < action->numInstances; inst++)
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

    for(uint32_t i = 0; !found && i < data.size() / xfbStride; i++)
    {
      Vec4f *pos = (Vec4f *)(data.data() + xfbStride * i);
      DeriveNearFar(*pos, pos0, nearp, farp, found);
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
  ret.gsout.buf = meshBuffer;
  ret.gsout.bufmem = meshMem;

  ret.gsout.baseVertex = 0;

  ret.gsout.numViews = 1;

  ret.gsout.vertStride = xfbStride;
  ret.gsout.nearPlane = nearp;
  ret.gsout.farPlane = farp;

  ret.gsout.useIndices = false;

  ret.gsout.numVerts = uint32_t(queryResult.numPrimitivesWritten) * primitiveMultiplier;

  // set instance stride to 0. If there's any stride needed, it will be calculated using instData
  ret.gsout.instStride = 0;
  ret.gsout.instData = instData;

  ret.gsout.idxbuf = VK_NULL_HANDLE;
  ret.gsout.idxbufmem = VK_NULL_HANDLE;

  ret.gsout.hasPosOut = true;
  ret.gsout.flipY = state.views.empty() ? false : state.views[0].height < 0.0f;

  // delete framebuffer and renderpass
  m_pDriver->vkDestroyFramebuffer(dev, fb, NULL);
  m_pDriver->vkDestroyRenderPass(dev, rp, NULL);

  // delete shader object
  if(shad != VK_NULL_HANDLE)
    m_pDriver->vkDestroyShaderEXT(dev, shad, NULL);

  // delete pipeline
  m_pDriver->vkDestroyPipeline(dev, pipe, NULL);

  // delete shader/shader module
  m_pDriver->vkDestroyShaderModule(dev, module, NULL);
}

void VulkanReplay::InitPostVSBuffers(uint32_t eventId, VulkanRenderState state)
{
  // go through any aliasing
  if(m_PostVS.Alias.find(eventId) != m_PostVS.Alias.end())
    eventId = m_PostVS.Alias[eventId];

  if(m_PostVS.Data.find(eventId) != m_PostVS.Data.end())
    return;

  // we handle out-of-memory errors while processing postvs, don't treat it as a fatal error
  ScopedOOMHandleVk oom(m_pDriver);

  VulkanCreationInfo &creationInfo = m_pDriver->m_CreationInfo;

  VulkanPostVSData &ret = m_PostVS.Data[eventId];

  if((state.graphics.pipeline == ResourceId() && !state.graphics.shaderObject) ||
     (state.GetRenderPass() == ResourceId() && !state.dynamicRendering.active))
  {
    ret.gsout.status = ret.vsout.status = "Draw outside of renderpass";
    return;
  }

  const VulkanCreationInfo::Pipeline &pipeInfo = creationInfo.m_Pipeline[state.graphics.pipeline];

  if(state.graphics.shaderObject && state.shaderObjects[size_t(ShaderStage::Vertex)] == ResourceId() &&
     state.shaderObjects[size_t(ShaderStage::Mesh)] == ResourceId())
  {
    ret.gsout.status = ret.vsout.status = "No vertex or mesh shader object";
    return;
  }

  if(!state.graphics.shaderObject &&
     pipeInfo.shaders[size_t(ShaderStage::Vertex)].module == ResourceId() &&
     pipeInfo.shaders[size_t(ShaderStage::Mesh)].module == ResourceId())
  {
    ret.gsout.status = ret.vsout.status = "No vertex or mesh shader in pipeline";
    return;
  }

  const ActionDescription *action = m_pDriver->GetAction(eventId);

  if(action == NULL)
  {
    ret.gsout.status = ret.vsout.status = "Invalid draw";
    return;
  }

  if(action->flags & ActionFlags::MeshDispatch)
  {
    FetchMeshOut(eventId, state);
    return;
  }

  if(action->numIndices == 0)
  {
    ret.gsout.status = ret.vsout.status = "Empty drawcall (0 indices/vertices)";
    return;
  }

  if(action->numInstances == 0)
  {
    ret.gsout.status = ret.vsout.status = "Empty drawcall (0 instances)";
    return;
  }

  VkMarkerRegion::Begin(StringFormat::Fmt("FetchVSOut for %u", eventId));

  FetchVSOut(eventId, state);

  VkMarkerRegion::End();

  bool noTessGS = false;

  if(state.graphics.shaderObject)
    noTessGS = state.shaderObjects[size_t(ShaderStage::Tess_Eval)] == ResourceId() &&
               state.shaderObjects[size_t(ShaderStage::Geometry)] == ResourceId();
  else
    noTessGS = pipeInfo.shaders[size_t(ShaderStage::Tess_Eval)].module == ResourceId() &&
               pipeInfo.shaders[size_t(ShaderStage::Geometry)].module == ResourceId();

  // if there's no tessellation or geometry shader active, bail out now
  if(noTessGS)
  {
    ret.gsout.status = "No geometry and no tessellation shader bound.";
    return;
  }

  VkMarkerRegion::Begin(StringFormat::Fmt("FetchTessGSOut for %u", eventId));

  FetchTessGSOut(eventId, state);

  VkMarkerRegion::End();
}

void VulkanReplay::InitPostVSBuffers(uint32_t eventId)
{
  InitPostVSBuffers(eventId, m_pDriver->GetRenderState());
}

struct VulkanInitPostVSCallback : public VulkanActionCallback
{
  VulkanInitPostVSCallback(WrappedVulkan *vk, const rdcarray<uint32_t> &events)
      : m_pDriver(vk), m_Events(events)
  {
    m_pDriver->SetActionCB(this);
  }
  ~VulkanInitPostVSCallback() { m_pDriver->SetActionCB(NULL); }
  void PreDraw(uint32_t eid, ActionFlags flags, VkCommandBuffer cmd)
  {
    if(m_Events.contains(eid))
      m_pDriver->GetReplay()->InitPostVSBuffers(eid, m_pDriver->GetCmdRenderState());
  }

  bool PostDraw(uint32_t eid, ActionFlags flags, VkCommandBuffer cmd) { return false; }
  void PostRedraw(uint32_t eid, ActionFlags flags, VkCommandBuffer cmd) {}
  // Dispatches don't rasterize, so do nothing
  void PreDispatch(uint32_t eid, ActionFlags flags, VkCommandBuffer cmd) {}
  bool PostDispatch(uint32_t eid, ActionFlags flags, VkCommandBuffer cmd) { return false; }
  void PostRedispatch(uint32_t eid, ActionFlags flags, VkCommandBuffer cmd) {}
  // Ditto copy/etc
  void PreMisc(uint32_t eid, ActionFlags flags, VkCommandBuffer cmd) {}
  bool PostMisc(uint32_t eid, ActionFlags flags, VkCommandBuffer cmd) { return false; }
  void PostRemisc(uint32_t eid, ActionFlags flags, VkCommandBuffer cmd) {}
  void PreEndCommandBuffer(VkCommandBuffer cmd) {}
  void AliasEvent(uint32_t primary, uint32_t alias)
  {
    if(m_Events.contains(primary))
      m_pDriver->GetReplay()->AliasPostVSBuffers(primary, alias);
  }
  bool SplitSecondary() { return false; }
  bool ForceLoadRPs() { return false; }
  void PreCmdExecute(uint32_t baseEid, uint32_t secondaryFirst, uint32_t secondaryLast,
                     VkCommandBuffer cmd)
  {
  }
  void PostCmdExecute(uint32_t baseEid, uint32_t secondaryFirst, uint32_t secondaryLast,
                      VkCommandBuffer cmd)
  {
  }

  WrappedVulkan *m_pDriver;
  const rdcarray<uint32_t> &m_Events;
};

void VulkanReplay::InitPostVSBuffers(const rdcarray<uint32_t> &events)
{
  size_t first = 0;

  for(; first < events.size(); first++)
  {
    const ActionDescription *action = m_pDriver->GetAction(events[first]);
    if(action->flags & ActionFlags::PassBoundary)
      continue;
    break;
  }

  // first we must replay up to the first event without replaying it. This ensures any
  // non-command buffer calls like memory unmaps etc all happen correctly before this
  // command buffer
  m_pDriver->ReplayLog(0, events[first], eReplay_WithoutDraw);

  VulkanInitPostVSCallback cb(m_pDriver, events);

  // now we replay the events, which are guaranteed (because we generated them in
  // GetPassEvents above) to come from the same command buffer, so the event IDs are
  // still locally continuous, even if we jump into replaying.
  m_pDriver->ReplayLog(events[first], events.back(), eReplay_Full);
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

  const ActionDescription *action = m_pDriver->GetAction(eventId);

  uint32_t numInstances = 1;
  if(action && (action->flags & ActionFlags::Instanced))
    numInstances = action->numInstances;

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
    if(s.idxFmt == VK_INDEX_TYPE_UINT32)
      ret.indexByteStride = 4;
    else if(s.idxFmt == VK_INDEX_TYPE_UINT8_KHR)
      ret.indexByteStride = 1;
    else
      ret.indexByteStride = 2;
    ret.indexByteSize = ~0ULL;
  }
  else
  {
    ret.indexResourceId = ResourceId();
    ret.indexByteStride = 0;
  }
  ret.indexByteOffset = s.idxOffset;
  ret.baseVertex = s.baseVertex;

  if(s.buf != VK_NULL_HANDLE)
  {
    ret.vertexResourceId = GetResID(s.buf);
    ret.vertexByteSize = ~0ULL;
  }
  else
  {
    ret.vertexResourceId = ResourceId();
  }

  ret.vertexByteOffset = s.instStride * (instID + viewID * numInstances);
  ret.vertexByteStride = s.vertStride;

  ret.format.compCount = 4;
  ret.format.compByteWidth = 4;
  ret.format.compType = CompType::Float;
  ret.format.type = ResourceFormatType::Regular;

  ret.showAlpha = false;

  ret.topology = s.topo;
  ret.numIndices = s.numVerts;

  ret.unproject = s.hasPosOut;
  ret.nearPlane = s.nearPlane;
  ret.farPlane = s.farPlane;
  ret.flipY = s.flipY;

  if(action && (action->flags & ActionFlags::MeshDispatch))
  {
    ret.perPrimitiveStride = s.primStride;
    ret.perPrimitiveOffset = s.primOffset;

    ret.dispatchSize = s.dispatchSize;

    if(stage == MeshDataStage::MeshOut)
    {
      ret.meshletSizes.resize(s.instData.size());
      for(size_t i = 0; i < s.instData.size(); i++)
        ret.meshletSizes[i] = {s.instData[i].numIndices, s.instData[i].numVerts};
    }
    else
    {
      // the buffer we're returning has the size vector. As long as the user respects our stride,
      // offsetting the start will do the trick
      ret.vertexByteOffset = sizeof(Vec4u);

      ret.taskSizes.resize(s.instData.size());
      for(size_t i = 0; i < s.instData.size(); i++)
        ret.taskSizes[i] = {
            s.instData[i].taskDispatchSizeX,
            s.instData[i].taskDispatchSizeYZ.y,
            s.instData[i].taskDispatchSizeYZ.z,
        };
    }
  }
  else if(instID < s.instData.size())
  {
    VulkanPostVSData::InstData inst = s.instData[instID];

    ret.vertexByteOffset = inst.bufOffset;
    ret.numIndices = inst.numVerts;
  }

  ret.status = s.status;

  return ret;
}
