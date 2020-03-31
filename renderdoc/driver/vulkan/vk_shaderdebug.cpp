/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2020 Baldur Karlsson
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

#include "core/settings.h"
#include "driver/shaders/spirv/spirv_debug.h"
#include "driver/shaders/spirv/spirv_editor.h"
#include "driver/shaders/spirv/spirv_op_helpers.h"
#include "maths/formatpacking.h"
#include "vk_core.h"
#include "vk_debug.h"
#include "vk_replay.h"

RDOC_DEBUG_CONFIG(rdcstr, Vulkan_Debug_PSDebugDumpDirPath, "",
                  "Path to dump before and after pixel shader input SPIR-V files.");

class VulkanAPIWrapper : public rdcspv::DebugAPIWrapper
{
public:
  VulkanAPIWrapper(WrappedVulkan *vk) { m_pDriver = vk; }
  virtual void AddDebugMessage(MessageCategory c, MessageSeverity sv, MessageSource src,
                               rdcstr d) override
  {
    m_pDriver->AddDebugMessage(c, sv, src, d);
  }

  virtual void ReadConstantBufferValue(uint32_t set, uint32_t bind, uint32_t offset,
                                       uint32_t byteSize, void *dst) override
  {
    auto it = cbuffers.find(make_rdcpair(set, bind));
    if(it == cbuffers.end())
      return;

    bytebuf &data = it->second;

    if(offset + byteSize <= data.size())
      memcpy(dst, data.data() + offset, byteSize);
  }

  virtual void FillInputValue(ShaderVariable &var, ShaderBuiltin builtin, uint32_t location,
                              uint32_t offset) override
  {
    if(builtin != ShaderBuiltin::Undefined)
    {
      auto it = builtin_inputs.find(builtin);
      if(it != builtin_inputs.end())
      {
        var.value = it->second.value;
        return;
      }

      RDCERR("Couldn't get input for %s", ToStr(builtin).c_str());
      return;
    }

    RDCASSERT(offset == 0);

    if(location < location_inputs.size())
    {
      var.value = location_inputs[location].value;
      return;
    }

    RDCERR("Couldn't get input for location=%u, offset=%u", location, offset);
  }

  std::map<rdcpair<uint32_t, uint32_t>, bytebuf> cbuffers;
  std::map<ShaderBuiltin, ShaderVariable> builtin_inputs;
  rdcarray<ShaderVariable> location_inputs;

private:
  WrappedVulkan *m_pDriver = NULL;
};

enum StorageMode
{
  Binding,
  EXT_bda,
  KHR_bda,
};

enum class InputSpecConstant
{
  Address = 1,
  ArrayLength,
  DestX,
  DestY,
};

static void CreatePSInputFetcher(rdcarray<uint32_t> &fragspv, uint32_t &structStride,
                                 VulkanCreationInfo::ShaderModuleReflection &shadRefl,
                                 StorageMode storageMode, bool usePrimitiveID, bool useSampleID)
{
  rdcspv::Editor editor(fragspv);

  editor.Prepare();

  // first delete all functions. We will recreate the entry point with just what we need
  {
    rdcarray<rdcspv::Id> removedIds;

    rdcspv::Iter it = editor.Begin(rdcspv::Section::Functions);
    rdcspv::Iter end = editor.End(rdcspv::Section::Functions);
    while(it < end)
    {
      removedIds.push_back(rdcspv::OpDecoder(it).result);
      editor.Remove(it);
      it++;
    }

    // remove any OpName that refers to deleted IDs - functions or results
    it = editor.Begin(rdcspv::Section::Debug);
    end = editor.End(rdcspv::Section::Debug);
    while(it < end)
    {
      if(it.opcode() == rdcspv::Op::Name)
      {
        rdcspv::OpName name(it);

        if(removedIds.contains(name.target))
          editor.Remove(it);
      }
      it++;
    }
  }

  // remove all other entry point
  rdcspv::Id entryID;
  for(const rdcspv::EntryPoint &e : editor.GetEntries())
  {
    if(e.name == shadRefl.entryPoint)
    {
      entryID = e.id;
      break;
    }
  }

  rdcarray<rdcspv::Id> addedInputs;

  // builtin inputs we need
  struct BuiltinAccess
  {
    rdcspv::Id base;
    uint32_t member = ~0U;
  } fragCoord, primitiveID, sampleIndex;

  // look to see which ones are already provided
  for(int32_t i = 0; i < shadRefl.refl.inputSignature.size(); i++)
  {
    const SigParameter &param = shadRefl.refl.inputSignature[i];

    BuiltinAccess *access = NULL;

    if(param.systemValue == ShaderBuiltin::Position)
      access = &fragCoord;
    else if(param.systemValue == ShaderBuiltin::PrimitiveIndex)
      access = &primitiveID;
    else if(param.systemValue == ShaderBuiltin::MSAASampleIndex)
      access = &sampleIndex;

    if(access)
    {
      SPIRVInterfaceAccess &patch = shadRefl.patchData.inputs[i];
      access->base = patch.ID;
      // should only be one deep at most, built-in interface block isn't allowed to be nested
      RDCASSERT(patch.accessChain.size() <= 1);
      if(!patch.accessChain.empty())
        access->member = patch.accessChain[0];
    }
  }

  // now declare any variables we didn't already have
  if(fragCoord.base == rdcspv::Id())
  {
    rdcspv::Id type = editor.DeclareType(rdcspv::Vector(rdcspv::scalar<float>(), 4));
    rdcspv::Id ptrType = editor.DeclareType(rdcspv::Pointer(type, rdcspv::StorageClass::Input));

    fragCoord.base =
        editor.AddVariable(rdcspv::OpVariable(ptrType, editor.MakeId(), rdcspv::StorageClass::Input));

    editor.AddDecoration(rdcspv::OpDecorate(
        fragCoord.base,
        rdcspv::DecorationParam<rdcspv::Decoration::BuiltIn>(rdcspv::BuiltIn::FragCoord)));

    addedInputs.push_back(fragCoord.base);
  }
  if(primitiveID.base == rdcspv::Id() && usePrimitiveID)
  {
    rdcspv::Id type = editor.DeclareType(rdcspv::scalar<uint32_t>());
    rdcspv::Id ptrType = editor.DeclareType(rdcspv::Pointer(type, rdcspv::StorageClass::Input));

    primitiveID.base =
        editor.AddVariable(rdcspv::OpVariable(ptrType, editor.MakeId(), rdcspv::StorageClass::Input));

    editor.AddDecoration(rdcspv::OpDecorate(
        primitiveID.base,
        rdcspv::DecorationParam<rdcspv::Decoration::BuiltIn>(rdcspv::BuiltIn::PrimitiveId)));
    editor.AddDecoration(rdcspv::OpDecorate(primitiveID.base, rdcspv::Decoration::Flat));

    addedInputs.push_back(primitiveID.base);

    editor.AddCapability(rdcspv::Capability::Geometry);
  }
  if(sampleIndex.base == rdcspv::Id() && useSampleID)
  {
    rdcspv::Id type = editor.DeclareType(rdcspv::scalar<uint32_t>());
    rdcspv::Id ptrType = editor.DeclareType(rdcspv::Pointer(type, rdcspv::StorageClass::Input));

    sampleIndex.base =
        editor.AddVariable(rdcspv::OpVariable(ptrType, editor.MakeId(), rdcspv::StorageClass::Input));

    editor.AddDecoration(rdcspv::OpDecorate(
        sampleIndex.base,
        rdcspv::DecorationParam<rdcspv::Decoration::BuiltIn>(rdcspv::BuiltIn::SampleId)));
    editor.AddDecoration(rdcspv::OpDecorate(sampleIndex.base, rdcspv::Decoration::Flat));

    addedInputs.push_back(sampleIndex.base);

    editor.AddCapability(rdcspv::Capability::SampleRateShading);
  }

  // add our inputs to the entry point's ID list. Since we're expanding the list we have to copy,
  // erase, and insert. Modifying in-place doesn't support expanding
  if(!addedInputs.empty())
  {
    rdcspv::Iter it = editor.GetEntry(entryID);

    // this copies into the helper struct
    rdcspv::OpEntryPoint entry(it);

    // add our IDs
    entry.iface.append(addedInputs);

    // erase the old one
    editor.Remove(it);

    editor.AddOperation(it, entry);
  }

  rdcspv::Id PSInput;

  // TODO generate actual PSInput for real inputs
  {
    PSInput =
        editor.DeclareStructType({editor.DeclareType(rdcspv::Vector(rdcspv::scalar<float>(), 4))});

    editor.AddDecoration(rdcspv::OpMemberDecorate(
        PSInput, 0, rdcspv::DecorationParam<rdcspv::Decoration::Offset>(0)));

    editor.SetName(PSInput, "__rd_PSInput");
    editor.SetMemberName(PSInput, 0, "dummy");

    structStride = sizeof(Vec4f);
  }

  rdcspv::Id uint32Type = editor.DeclareType(rdcspv::scalar<uint32_t>());
  rdcspv::Id floatType = editor.DeclareType(rdcspv::scalar<float>());
  rdcspv::Id boolType = editor.DeclareType(rdcspv::scalar<bool>());
  rdcspv::Id float4Type = editor.DeclareType(rdcspv::Vector(rdcspv::scalar<float>(), 4));
  rdcspv::Id float2Type = editor.DeclareType(rdcspv::Vector(rdcspv::scalar<float>(), 2));

  rdcspv::Id arrayLength = editor.AddConstant(rdcspv::Operation(
      rdcspv::Op::SpecConstant, {uint32Type.value(), editor.MakeId().value(), 1U}));

  editor.AddDecoration(rdcspv::OpDecorate(
      arrayLength,
      rdcspv::DecorationParam<rdcspv::Decoration::SpecId>((uint32_t)InputSpecConstant::ArrayLength)));

  editor.SetName(arrayLength, "arrayLength");

  rdcspv::Id destX = editor.AddConstant(rdcspv::Operation(
      rdcspv::Op::SpecConstant, {floatType.value(), editor.MakeId().value(), 0U}));
  rdcspv::Id destY = editor.AddConstant(rdcspv::Operation(
      rdcspv::Op::SpecConstant, {floatType.value(), editor.MakeId().value(), 0U}));

  editor.SetName(destX, "destX");
  editor.SetName(destY, "destY");

  editor.AddDecoration(rdcspv::OpDecorate(destX, rdcspv::DecorationParam<rdcspv::Decoration::SpecId>(
                                                     (uint32_t)InputSpecConstant::DestX)));
  editor.AddDecoration(rdcspv::OpDecorate(destY, rdcspv::DecorationParam<rdcspv::Decoration::SpecId>(
                                                     (uint32_t)InputSpecConstant::DestY)));

  rdcspv::Id destXY = editor.AddConstant(
      rdcspv::OpSpecConstantComposite(float2Type, editor.MakeId(), {destX, destY}));

  editor.SetName(destXY, "destXY");

  rdcspv::Id uintConsts[] = {
      editor.AddConstantImmediate<uint32_t>(0), editor.AddConstantImmediate<uint32_t>(1),
      editor.AddConstantImmediate<uint32_t>(2),
  };

  rdcspv::Id PSHit = editor.DeclareStructType({
      // float4 pos;
      float4Type,
      // uint prim;
      uint32Type,
      // uint sample;
      uint32Type,
      // float derivValid;
      floatType,
      // <uint padding>

      // IN
      PSInput,
      // INddxcoarse
      PSInput,
      // INddycoarse
      PSInput,
      // INddxfine
      PSInput,
      // INddxfine
      PSInput,
  });

  {
    editor.SetName(PSHit, "__rd_PSHit");

    uint32_t offs = 0, member = 0;

    editor.AddDecoration(rdcspv::OpMemberDecorate(
        PSHit, member, rdcspv::DecorationParam<rdcspv::Decoration::Offset>(offs)));
    editor.SetMemberName(PSHit, member, "pos");
    offs += sizeof(Vec4f), member++;

    editor.AddDecoration(rdcspv::OpMemberDecorate(
        PSHit, member, rdcspv::DecorationParam<rdcspv::Decoration::Offset>(offs)));
    editor.SetMemberName(PSHit, member, "prim");
    offs += sizeof(uint32_t), member++;

    editor.AddDecoration(rdcspv::OpMemberDecorate(
        PSHit, member, rdcspv::DecorationParam<rdcspv::Decoration::Offset>(offs)));
    editor.SetMemberName(PSHit, member, "sample");
    offs += sizeof(uint32_t), member++;

    editor.AddDecoration(rdcspv::OpMemberDecorate(
        PSHit, member, rdcspv::DecorationParam<rdcspv::Decoration::Offset>(offs)));
    editor.SetMemberName(PSHit, member, "derivValid");
    offs += sizeof(float), member++;

    offs += sizeof(float);    // padding

    RDCASSERT((offs % sizeof(Vec4f)) == 0);
    RDCASSERT((structStride % sizeof(Vec4f)) == 0);

    editor.AddDecoration(rdcspv::OpMemberDecorate(
        PSHit, member, rdcspv::DecorationParam<rdcspv::Decoration::Offset>(offs)));
    editor.SetMemberName(PSHit, member, "IN");
    offs += structStride, member++;

    editor.AddDecoration(rdcspv::OpMemberDecorate(
        PSHit, member, rdcspv::DecorationParam<rdcspv::Decoration::Offset>(offs)));
    editor.SetMemberName(PSHit, member, "INddxcoarse");
    offs += structStride, member++;

    editor.AddDecoration(rdcspv::OpMemberDecorate(
        PSHit, member, rdcspv::DecorationParam<rdcspv::Decoration::Offset>(offs)));
    editor.SetMemberName(PSHit, member, "INddycoarse");
    offs += structStride, member++;

    editor.AddDecoration(rdcspv::OpMemberDecorate(
        PSHit, member, rdcspv::DecorationParam<rdcspv::Decoration::Offset>(offs)));
    editor.SetMemberName(PSHit, member, "INddxfine");
    offs += structStride, member++;

    editor.AddDecoration(rdcspv::OpMemberDecorate(
        PSHit, member, rdcspv::DecorationParam<rdcspv::Decoration::Offset>(offs)));
    editor.SetMemberName(PSHit, member, "INddyfine");
    offs += structStride, member++;
  }

  // we have 5 input structs, and two vectors for our data
  structStride = sizeof(Vec4f) + sizeof(Vec4f) + structStride * 5;

  rdcspv::Id PSHitRTArray = editor.AddType(rdcspv::OpTypeRuntimeArray(editor.MakeId(), PSHit));

  editor.AddDecoration(rdcspv::OpDecorate(
      PSHitRTArray, rdcspv::DecorationParam<rdcspv::Decoration::ArrayStride>(structStride)));

  rdcspv::Id bufBase = editor.DeclareStructType({
      // uint hit_count;
      uint32Type,
      // <uint3 padding>

      //  PSHit hits[];
      PSHitRTArray,
  });

  {
    editor.SetName(bufBase, "__rd_HitStorage");

    editor.AddDecoration(rdcspv::OpMemberDecorate(
        bufBase, 0, rdcspv::DecorationParam<rdcspv::Decoration::Offset>(0)));
    editor.SetMemberName(bufBase, 0, "hit_count");

    editor.AddDecoration(rdcspv::OpMemberDecorate(
        bufBase, 1, rdcspv::DecorationParam<rdcspv::Decoration::Offset>(sizeof(Vec4f))));
    editor.SetMemberName(bufBase, 1, "hits");
  }

  rdcspv::StorageClass bufferClass;
  rdcspv::Id bufptrtype;
  rdcspv::Id ssboVar;
  rdcspv::Id addressConstant;

  if(storageMode == Binding)
  {
    bufferClass = editor.StorageBufferClass();

    // the pointers are SSBO pointers
    bufptrtype = editor.DeclareType(rdcspv::Pointer(bufBase, bufferClass));

    // patch all bindings up by 1
    for(rdcspv::Iter it = editor.Begin(rdcspv::Section::Annotations),
                     end = editor.End(rdcspv::Section::Annotations);
        it < end; ++it)
    {
      // we will use descriptor set 0 for our own purposes if we don't have a buffer address.
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
          RDCASSERT(dec.decoration.binding != 0xffffffff);
          dec.decoration.binding += 1;
          it = dec;
        }
      }
    }

    // add our SSBO variable, at set 0 binding 0
    ssboVar = editor.MakeId();
    editor.AddVariable(rdcspv::OpVariable(bufptrtype, ssboVar, bufferClass));
    editor.AddDecoration(
        rdcspv::OpDecorate(ssboVar, rdcspv::DecorationParam<rdcspv::Decoration::DescriptorSet>(0)));
    editor.AddDecoration(
        rdcspv::OpDecorate(ssboVar, rdcspv::DecorationParam<rdcspv::Decoration::Binding>(0)));

    editor.SetName(ssboVar, "__rd_HitBuffer");

    editor.DecorateStorageBufferStruct(bufBase);
  }
  else
  {
    // our pointers are physical storage buffer pointers
    bufferClass = rdcspv::StorageClass::PhysicalStorageBuffer;

    bufptrtype = editor.DeclareType(rdcspv::Pointer(bufBase, bufferClass));

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
    editor.AddCapability(rdcspv::Capability::Int64);

    // declare the address constant which we will specialise later. There is a chicken-and-egg where
    // this function determines how big the buffer needs to be so instead of hardcoding the address
    // here we let it be allocated later and specialised in.
    addressConstant = editor.AddConstant(rdcspv::Operation(
        rdcspv::Op::SpecConstant,
        {editor.DeclareType(rdcspv::scalar<uint64_t>()).value(), editor.MakeId().value(), 0U, 0U}));

    editor.AddDecoration(rdcspv::OpDecorate(
        addressConstant,
        rdcspv::DecorationParam<rdcspv::Decoration::SpecId>((uint32_t)InputSpecConstant::Address)));

    editor.SetName(addressConstant, "__rd_bufAddress");

    // struct is block decorated
    editor.AddDecoration(rdcspv::OpDecorate(bufBase, rdcspv::Decoration::Block));
  }

  rdcspv::Id float4InPtr =
      editor.DeclareType(rdcspv::Pointer(float4Type, rdcspv::StorageClass::Input));
  rdcspv::Id float4BufPtr = editor.DeclareType(rdcspv::Pointer(float4Type, bufferClass));

  rdcspv::Id uint32InPtr =
      editor.DeclareType(rdcspv::Pointer(uint32Type, rdcspv::StorageClass::Input));
  rdcspv::Id uint32BufPtr = editor.DeclareType(rdcspv::Pointer(uint32Type, bufferClass));

  rdcspv::Id glsl450 = editor.ImportExtInst("GLSL.std.450");

  {
    rdcarray<rdcspv::Operation> ops;

    rdcspv::Id voidType = editor.DeclareType(rdcspv::scalar<void>());

    ops.push_back(rdcspv::OpFunction(voidType, entryID, rdcspv::FunctionControl::None,
                                     editor.DeclareType(rdcspv::FunctionType(voidType, {}))));

    ops.push_back(rdcspv::OpLabel(editor.MakeId()));
    {
      rdcspv::Id structPtr = ssboVar;

      if(structPtr == rdcspv::Id())
      {
        // if we don't have the struct as a bind, we need to cast it from the pointer
        structPtr = editor.MakeId();
        ops.push_back(rdcspv::OpConvertUToPtr(bufptrtype, structPtr, addressConstant));

        editor.SetName(structPtr, "HitBuffer");
      }

      rdcspv::Id uintPtr = editor.DeclareType(rdcspv::Pointer(uint32Type, bufferClass));
      rdcspv::Id hit_count = editor.MakeId();

      // get a pointer to buffer.hit_count
      ops.push_back(rdcspv::OpAccessChain(uintPtr, hit_count, structPtr, {uintConsts[0]}));

      rdcspv::Id scope = editor.AddConstantImmediate<uint32_t>((uint32_t)rdcspv::Scope::Device);
      rdcspv::Id semantics =
          editor.AddConstantImmediate<uint32_t>((uint32_t)rdcspv::MemorySemantics::AcquireRelease);

      // look up the fragcoord
      rdcspv::Id fragCoordLoaded = editor.MakeId();
      if(fragCoord.member == ~0U)
      {
        ops.push_back(rdcspv::OpLoad(float4Type, fragCoordLoaded, fragCoord.base));
      }
      else
      {
        rdcspv::Id posptr = editor.MakeId();
        ops.push_back(rdcspv::OpAccessChain(float4InPtr, posptr, fragCoord.base,
                                            {editor.AddConstantImmediate(fragCoord.member)}));
        ops.push_back(rdcspv::OpLoad(float4Type, fragCoordLoaded, posptr));
      }

      rdcspv::Id bool2Type = editor.DeclareType(rdcspv::Vector(rdcspv::scalar<bool>(), 2));

      // grab x and y
      rdcspv::Id fragXY = editor.MakeId();
      ops.push_back(
          rdcspv::OpVectorShuffle(float2Type, fragXY, fragCoordLoaded, fragCoordLoaded, {0, 1}));

      // subtract from the destination co-ord
      rdcspv::Id fragXYRelative = editor.MakeId();
      ops.push_back(rdcspv::OpFSub(float2Type, fragXYRelative, fragXY, destXY));

      // abs()
      rdcspv::Id fragXYAbs = editor.MakeId();
      ops.push_back(rdcspv::Operation(
          rdcspv::Op::ExtInst, {
                                   float2Type.value(), fragXYAbs.value(), glsl450.value(),
                                   (uint32_t)rdcspv::GLSLstd450::FAbs, fragXYRelative.value(),
                               }));

      rdcspv::Id half = editor.AddConstantImmediate<float>(0.5f);
      rdcspv::Id threshold =
          editor.AddConstant(rdcspv::OpConstantComposite(float2Type, editor.MakeId(), {half, half}));

      // less than 0.5
      rdcspv::Id inPixelXY = editor.MakeId();
      ops.push_back(rdcspv::OpFOrdLessThan(bool2Type, inPixelXY, fragXYAbs, threshold));

      // both less than 0.5
      rdcspv::Id inPixel = editor.MakeId();
      ops.push_back(rdcspv::OpAll(boolType, inPixel, inPixelXY));

      // bool inPixel = all(abs(gl_FragCoord.xy - dest.xy) < 0.5f);

      rdcspv::Id killLabel = editor.MakeId();
      rdcspv::Id continueLabel = editor.MakeId();
      ops.push_back(rdcspv::OpSelectionMerge(killLabel, rdcspv::SelectionControl::None));
      ops.push_back(rdcspv::OpBranchConditional(inPixel, continueLabel, killLabel));
      ops.push_back(rdcspv::OpLabel(continueLabel));

      // allocate a slot with atomic add
      rdcspv::Id slot = editor.MakeId();
      ops.push_back(
          rdcspv::OpAtomicIAdd(uint32Type, slot, hit_count, scope, semantics, uintConsts[1]));

      editor.SetName(slot, "slot");

      rdcspv::Id inRange = editor.MakeId();
      ops.push_back(rdcspv::OpULessThan(boolType, inRange, slot, arrayLength));

      rdcspv::Id killLabel2 = editor.MakeId();
      continueLabel = editor.MakeId();
      ops.push_back(rdcspv::OpSelectionMerge(killLabel2, rdcspv::SelectionControl::None));
      ops.push_back(rdcspv::OpBranchConditional(inRange, continueLabel, killLabel2));
      ops.push_back(rdcspv::OpLabel(continueLabel));

      rdcspv::Id hitptr = editor.DeclareType(rdcspv::Pointer(PSHit, bufferClass));

      // get a pointer to the hit for our slot
      rdcspv::Id hit = editor.MakeId();
      ops.push_back(rdcspv::OpAccessChain(hitptr, hit, structPtr, {uintConsts[1], slot}));

      // store fixed properties

      rdcspv::MemoryAccessAndParamDatas alignedAccess;
      alignedAccess.setAligned(sizeof(uint32_t));

      rdcspv::Id storePtr = editor.MakeId();
      ops.push_back(rdcspv::OpAccessChain(float4BufPtr, storePtr, hit, {uintConsts[0]}));
      ops.push_back(rdcspv::OpStore(storePtr, fragCoordLoaded, alignedAccess));

      rdcspv::Id loaded = editor.MakeId();
      if(primitiveID.base != rdcspv::Id())
      {
        loaded = editor.MakeId();
        if(primitiveID.member == ~0U)
        {
          ops.push_back(rdcspv::OpLoad(uint32Type, loaded, primitiveID.base));
        }
        else
        {
          rdcspv::Id posptr = editor.MakeId();
          ops.push_back(rdcspv::OpAccessChain(uint32InPtr, posptr, primitiveID.base,
                                              {editor.AddConstantImmediate(primitiveID.member)}));
          ops.push_back(rdcspv::OpLoad(uint32Type, loaded, posptr));
        }
      }
      else
      {
        // explicitly store 0
        loaded = uintConsts[0];
      }

      storePtr = editor.MakeId();
      ops.push_back(rdcspv::OpAccessChain(uint32BufPtr, storePtr, hit, {uintConsts[1]}));
      ops.push_back(rdcspv::OpStore(storePtr, loaded, alignedAccess));

      if(sampleIndex.base != rdcspv::Id())
      {
        loaded = editor.MakeId();
        if(sampleIndex.member == ~0U)
        {
          ops.push_back(rdcspv::OpLoad(uint32Type, loaded, sampleIndex.base));
        }
        else
        {
          rdcspv::Id posptr = editor.MakeId();
          ops.push_back(rdcspv::OpAccessChain(uint32InPtr, posptr, sampleIndex.base,
                                              {editor.AddConstantImmediate(sampleIndex.member)}));
          ops.push_back(rdcspv::OpLoad(uint32Type, loaded, posptr));
        }
      }
      else
      {
        // explicitly store 0
        loaded = uintConsts[0];
      }

      storePtr = editor.MakeId();
      ops.push_back(rdcspv::OpAccessChain(uint32BufPtr, storePtr, hit, {uintConsts[2]}));
      ops.push_back(rdcspv::OpStore(storePtr, loaded, alignedAccess));

      // join up with the early-outs we did
      ops.push_back(rdcspv::OpBranch(killLabel2));
      ops.push_back(rdcspv::OpLabel(killLabel2));
      ops.push_back(rdcspv::OpBranch(killLabel));
      ops.push_back(rdcspv::OpLabel(killLabel));
    }
    // don't return, kill. This makes it well-defined that we don't write anything to our outputs
    ops.push_back(rdcspv::OpKill());

    ops.push_back(rdcspv::OpFunctionEnd());

    editor.AddFunction(ops.data(), ops.size());
  }
}

ShaderDebugTrace *VulkanReplay::DebugVertex(uint32_t eventId, uint32_t vertid, uint32_t instid,
                                            uint32_t idx)
{
  if(!GetAPIProperties().shaderDebugging)
  {
    RDCUNIMPLEMENTED("Vertex debugging not yet implemented for Vulkan");
    return new ShaderDebugTrace;
  }

  const VulkanRenderState &state = m_pDriver->m_RenderState;
  VulkanCreationInfo &c = m_pDriver->m_CreationInfo;

  VkMarkerRegion region(
      StringFormat::Fmt("DebugVertex @ %u of (%u,%u,%u)", eventId, vertid, instid, idx));

  const DrawcallDescription *draw = m_pDriver->GetDrawcall(eventId);

  if(!(draw->flags & DrawFlags::Drawcall))
    return new ShaderDebugTrace();

  const VulkanCreationInfo::Pipeline &pipe = c.m_Pipeline[state.graphics.pipeline];
  VulkanCreationInfo::ShaderModule &shader = c.m_ShaderModule[pipe.shaders[0].module];
  rdcstr entryPoint = pipe.shaders[0].entryPoint;
  const rdcarray<SpecConstant> &spec = pipe.shaders[0].specialization;

  VulkanCreationInfo::ShaderModuleReflection &shadRefl =
      shader.GetReflection(entryPoint, state.graphics.pipeline);

  shadRefl.PopulateDisassembly(shader.spirv);
  VulkanAPIWrapper *apiWrapper = new VulkanAPIWrapper(m_pDriver);

  for(uint32_t set = 0; set < state.graphics.descSets.size(); set++)
  {
    const VulkanStatePipeline::DescriptorAndOffsets &src = state.graphics.descSets[set];

    const WrappedVulkan::DescriptorSetInfo &setInfo = m_pDriver->m_DescriptorSetState[src.descSet];
    ResourceId layoutId = setInfo.layout;

    uint32_t dynOffsetIdx = 0;

    for(uint32_t bind = 0; bind < setInfo.currentBindings.size(); bind++)
    {
      DescriptorSetSlot *info = setInfo.currentBindings[bind];
      const DescSetLayout::Binding &layoutBind = c.m_DescSetLayout[layoutId].bindings[bind];

      if(layoutBind.stageFlags == 0)
        continue;

      uint32_t dynOffset = 0;

      if(layoutBind.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC ||
         layoutBind.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC)
        dynOffset = src.offsets[dynOffsetIdx++];

      // TODO handle arrays of bindings
      const uint32_t arrayIdx = 0;

      if(layoutBind.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER ||
         layoutBind.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC)
      {
        const DescriptorSetSlotBufferInfo &bufInfo = info[arrayIdx].bufferInfo;
        GetDebugManager()->GetBufferData(bufInfo.buffer, bufInfo.offset + dynOffset, bufInfo.range,
                                         apiWrapper->cbuffers[make_rdcpair(set, bind)]);
      }
    }
  }

  std::map<ShaderBuiltin, ShaderVariable> &builtins = apiWrapper->builtin_inputs;
  builtins[ShaderBuiltin::BaseInstance] = ShaderVariable(rdcstr(), draw->instanceOffset, 0U, 0U, 0U);
  builtins[ShaderBuiltin::BaseVertex] = ShaderVariable(
      rdcstr(), (draw->flags & DrawFlags::Indexed) ? draw->baseVertex : draw->vertexOffset, 0U, 0U,
      0U);
  builtins[ShaderBuiltin::DeviceIndex] = ShaderVariable(rdcstr(), 0U, 0U, 0U, 0U);
  builtins[ShaderBuiltin::DrawIndex] = ShaderVariable(rdcstr(), draw->drawIndex, 0U, 0U, 0U);
  builtins[ShaderBuiltin::VertexIndex] = ShaderVariable(rdcstr(), vertid, 0U, 0U, 0U);
  builtins[ShaderBuiltin::InstanceIndex] = ShaderVariable(rdcstr(), instid, 0U, 0U, 0U);

  rdcarray<ShaderVariable> &locations = apiWrapper->location_inputs;
  for(const VulkanCreationInfo::Pipeline::Attribute &attr : pipe.vertexAttrs)
  {
    if(attr.location >= locations.size())
      locations.resize(attr.location + 1);

    ShaderValue &val = locations[attr.location].value;

    bytebuf data;

    size_t size = GetByteSize(1, 1, 1, attr.format, 0);

    if(attr.binding < pipe.vertexBindings.size())
    {
      const VulkanCreationInfo::Pipeline::Binding &bind = pipe.vertexBindings[attr.binding];

      if(bind.vbufferBinding < state.vbuffers.size())
      {
        const VulkanRenderState::VertBuffer &vb = state.vbuffers[bind.vbufferBinding];

        uint32_t vertexOffset = 0;

        if(bind.perInstance)
        {
          if(bind.instanceDivisor == 0)
            vertexOffset = draw->instanceOffset * bind.bytestride;
          else
            vertexOffset = draw->instanceOffset + (instid / bind.instanceDivisor) * bind.bytestride;
        }
        else
        {
          vertexOffset = idx * bind.bytestride;
        }

        GetDebugManager()->GetBufferData(vb.buf, vb.offs + attr.byteoffset + vertexOffset, size,
                                         data);
      }
    }

    if(size > data.size())
    {
      // out of bounds read
      m_pDriver->AddDebugMessage(
          MessageCategory::Execution, MessageSeverity::Medium, MessageSource::RuntimeWarning,
          StringFormat::Fmt(
              "Attribute location %u from binding %u reads out of bounds at vertex %u "
              "(index %u) in instance %u.",
              attr.location, attr.binding, vertid, idx, instid));

      if(IsUIntFormat(attr.format) || IsSIntFormat(attr.format))
        val.u = {0, 0, 0, 1};
      else
        val.f = {0.0f, 0.0f, 0.0f, 1.0f};
    }
    else
    {
      FloatVector decoded = ConvertComponents(MakeResourceFormat(attr.format), data.data());

      val.f.x = decoded.x;
      val.f.y = decoded.y;
      val.f.z = decoded.z;
      val.f.w = decoded.w;
    }
  }

  rdcspv::Debugger *debugger = new rdcspv::Debugger;
  debugger->Parse(shader.spirv.GetSPIRV());
  ShaderDebugTrace *ret = debugger->BeginDebug(apiWrapper, ShaderStage::Vertex, entryPoint, spec,
                                               shadRefl.instructionLines, shadRefl.patchData, 0);

  return ret;
}

ShaderDebugTrace *VulkanReplay::DebugPixel(uint32_t eventId, uint32_t x, uint32_t y,
                                           uint32_t sample, uint32_t primitive)
{
  if(!GetAPIProperties().shaderDebugging)
  {
    RDCUNIMPLEMENTED("Pixel debugging not yet implemented for Vulkan");
    return new ShaderDebugTrace;
  }

  if(!m_pDriver->GetDeviceFeatures().fragmentStoresAndAtomics)
  {
    RDCWARN("Pixel debugging is not supported without fragment stores");
    return new ShaderDebugTrace;
  }

  const VulkanRenderState &state = m_pDriver->m_RenderState;
  VulkanCreationInfo &c = m_pDriver->m_CreationInfo;

  VkMarkerRegion region(StringFormat::Fmt("DebugPixel @ %u of (%u,%u) sample %u primitive %u",
                                          eventId, x, y, sample, primitive));

  const DrawcallDescription *draw = m_pDriver->GetDrawcall(eventId);

  if(!(draw->flags & DrawFlags::Drawcall))
    return new ShaderDebugTrace();

  const VulkanCreationInfo::Pipeline &pipe = c.m_Pipeline[state.graphics.pipeline];
  VulkanCreationInfo::ShaderModule &shader = c.m_ShaderModule[pipe.shaders[4].module];
  rdcstr entryPoint = pipe.shaders[4].entryPoint;
  const rdcarray<SpecConstant> &spec = pipe.shaders[4].specialization;

  VulkanCreationInfo::ShaderModuleReflection &shadRefl =
      shader.GetReflection(entryPoint, state.graphics.pipeline);

  shadRefl.PopulateDisassembly(shader.spirv);
  VulkanAPIWrapper *apiWrapper = new VulkanAPIWrapper(m_pDriver);

  for(uint32_t set = 0; set < state.graphics.descSets.size(); set++)
  {
    const VulkanStatePipeline::DescriptorAndOffsets &src = state.graphics.descSets[set];

    const WrappedVulkan::DescriptorSetInfo &setInfo = m_pDriver->m_DescriptorSetState[src.descSet];
    ResourceId layoutId = setInfo.layout;

    uint32_t dynOffsetIdx = 0;

    for(uint32_t bind = 0; bind < setInfo.currentBindings.size(); bind++)
    {
      DescriptorSetSlot *info = setInfo.currentBindings[bind];
      const DescSetLayout::Binding &layoutBind = c.m_DescSetLayout[layoutId].bindings[bind];

      if(layoutBind.stageFlags == 0)
        continue;

      uint32_t dynOffset = 0;

      if(layoutBind.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC ||
         layoutBind.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC)
        dynOffset = src.offsets[dynOffsetIdx++];

      // TODO handle arrays of bindings
      const uint32_t arrayIdx = 0;

      if(layoutBind.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER ||
         layoutBind.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC)
      {
        const DescriptorSetSlotBufferInfo &bufInfo = info[arrayIdx].bufferInfo;
        GetDebugManager()->GetBufferData(bufInfo.buffer, bufInfo.offset + dynOffset, bufInfo.range,
                                         apiWrapper->cbuffers[make_rdcpair(set, bind)]);
      }
    }
  }

  std::map<ShaderBuiltin, ShaderVariable> &builtins = apiWrapper->builtin_inputs;
  builtins[ShaderBuiltin::DeviceIndex] = ShaderVariable(rdcstr(), 0U, 0U, 0U, 0U);
  builtins[ShaderBuiltin::DrawIndex] = ShaderVariable(rdcstr(), draw->drawIndex, 0U, 0U, 0U);

  // If the pipe contains a geometry shader, then Primitive ID cannot be used in the pixel
  // shader without being emitted from the geometry shader. For now, check if this semantic
  // will succeed in a new pixel shader with the rest of the pipe unchanged
  bool usePrimitiveID = false;
  if(pipe.shaders[3].module != ResourceId())
  {
    VulkanCreationInfo::ShaderModuleReflection &gsRefl =
        c.m_ShaderModule[pipe.shaders[3].module].GetReflection(pipe.shaders[3].entryPoint,
                                                               state.graphics.pipeline);

    // check to see if the shader outputs a primitive ID
    for(const SigParameter &e : gsRefl.refl.outputSignature)
    {
      if(e.systemValue == ShaderBuiltin::PrimitiveIndex)
      {
        usePrimitiveID = true;
        break;
      }
    }
  }
  else
  {
    // no geometry shader - safe to use as long as the geometry shader capability is available
    usePrimitiveID = m_pDriver->GetDeviceFeatures().geometryShader != VK_FALSE;
  }

  bool useSampleID = m_pDriver->GetDeviceFeatures().sampleRateShading != VK_FALSE;

  StorageMode storageMode = Binding;

  if(m_pDriver->GetDeviceFeatures().shaderInt64)
  {
    if(m_pDriver->GetExtensions(NULL).ext_KHR_buffer_device_address)
      storageMode = KHR_bda;
    else if(m_pDriver->GetExtensions(NULL).ext_EXT_buffer_device_address)
      storageMode = EXT_bda;
  }

  rdcarray<uint32_t> fragspv = shader.spirv.GetSPIRV();

  if(!Vulkan_Debug_PSDebugDumpDirPath.empty())
    FileIO::WriteAll(Vulkan_Debug_PSDebugDumpDirPath + "/debug_psinput_before.spv", fragspv);

  uint32_t structStride = 0;
  CreatePSInputFetcher(fragspv, structStride, shadRefl, storageMode, usePrimitiveID, useSampleID);

  if(!Vulkan_Debug_PSDebugDumpDirPath.empty())
    FileIO::WriteAll(Vulkan_Debug_PSDebugDumpDirPath + "/debug_psinput_after.spv", fragspv);

  rdcspv::Debugger *debugger = new rdcspv::Debugger;
  debugger->Parse(shader.spirv.GetSPIRV());
  ShaderDebugTrace *ret = debugger->BeginDebug(apiWrapper, ShaderStage::Pixel, entryPoint, spec,
                                               shadRefl.instructionLines, shadRefl.patchData, 0);

  return ret;
}

ShaderDebugTrace *VulkanReplay::DebugThread(uint32_t eventId, const uint32_t groupid[3],
                                            const uint32_t threadid[3])
{
  if(!GetAPIProperties().shaderDebugging)
  {
    RDCUNIMPLEMENTED("Compute debugging not yet implemented for Vulkan");
    return new ShaderDebugTrace;
  }

  const VulkanRenderState &state = m_pDriver->m_RenderState;
  VulkanCreationInfo &c = m_pDriver->m_CreationInfo;

  VkMarkerRegion region(StringFormat::Fmt("DebugThread @ %u of (%u,%u,%u) (%u,%u,%u)", eventId,
                                          groupid[0], groupid[1], groupid[2], threadid[0],
                                          threadid[1], threadid[2]));

  const DrawcallDescription *draw = m_pDriver->GetDrawcall(eventId);

  if(!(draw->flags & DrawFlags::Dispatch))
    return new ShaderDebugTrace();

  const VulkanCreationInfo::Pipeline &pipe = c.m_Pipeline[state.compute.pipeline];
  VulkanCreationInfo::ShaderModule &shader = c.m_ShaderModule[pipe.shaders[5].module];
  rdcstr entryPoint = pipe.shaders[5].entryPoint;
  const rdcarray<SpecConstant> &spec = pipe.shaders[5].specialization;

  VulkanCreationInfo::ShaderModuleReflection &shadRefl =
      shader.GetReflection(entryPoint, state.compute.pipeline);

  shadRefl.PopulateDisassembly(shader.spirv);
  VulkanAPIWrapper *apiWrapper = new VulkanAPIWrapper(m_pDriver);

  for(uint32_t set = 0; set < state.compute.descSets.size(); set++)
  {
    const VulkanStatePipeline::DescriptorAndOffsets &src = state.compute.descSets[set];

    const WrappedVulkan::DescriptorSetInfo &setInfo = m_pDriver->m_DescriptorSetState[src.descSet];
    ResourceId layoutId = setInfo.layout;

    uint32_t dynOffsetIdx = 0;

    for(uint32_t bind = 0; bind < setInfo.currentBindings.size(); bind++)
    {
      DescriptorSetSlot *info = setInfo.currentBindings[bind];
      const DescSetLayout::Binding &layoutBind = c.m_DescSetLayout[layoutId].bindings[bind];

      if(layoutBind.stageFlags == 0)
        continue;

      uint32_t dynOffset = 0;

      if(layoutBind.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC ||
         layoutBind.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC)
        dynOffset = src.offsets[dynOffsetIdx++];

      // TODO handle arrays of bindings
      const uint32_t arrayIdx = 0;

      if(layoutBind.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER ||
         layoutBind.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC)
      {
        const DescriptorSetSlotBufferInfo &bufInfo = info[arrayIdx].bufferInfo;
        GetDebugManager()->GetBufferData(bufInfo.buffer, bufInfo.offset + dynOffset, bufInfo.range,
                                         apiWrapper->cbuffers[make_rdcpair(set, bind)]);
      }
    }
  }

  uint32_t threadDim[3];
  threadDim[0] = shadRefl.refl.dispatchThreadsDimension[0];
  threadDim[1] = shadRefl.refl.dispatchThreadsDimension[1];
  threadDim[2] = shadRefl.refl.dispatchThreadsDimension[2];

  std::map<ShaderBuiltin, ShaderVariable> &builtins = apiWrapper->builtin_inputs;
  builtins[ShaderBuiltin::DispatchSize] =
      ShaderVariable(rdcstr(), draw->dispatchDimension[0], draw->dispatchDimension[1],
                     draw->dispatchDimension[2], 0U);
  builtins[ShaderBuiltin::DispatchThreadIndex] = ShaderVariable(
      rdcstr(), groupid[0] * threadDim[0] + threadid[0], groupid[1] * threadDim[1] + threadid[1],
      groupid[2] * threadDim[2] + threadid[2], 0U);
  builtins[ShaderBuiltin::GroupIndex] =
      ShaderVariable(rdcstr(), groupid[0], groupid[1], groupid[2], 0U);
  builtins[ShaderBuiltin::GroupSize] =
      ShaderVariable(rdcstr(), threadDim[0], threadDim[1], threadDim[2], 0U);
  builtins[ShaderBuiltin::GroupThreadIndex] =
      ShaderVariable(rdcstr(), threadid[0], threadid[1], threadid[2], 0U);
  builtins[ShaderBuiltin::GroupFlatIndex] = ShaderVariable(
      rdcstr(), threadid[2] * threadDim[0] * threadDim[1] + threadid[1] * threadDim[0] + threadid[0],
      0U, 0U, 0U);
  builtins[ShaderBuiltin::DeviceIndex] = ShaderVariable(rdcstr(), 0U, 0U, 0U, 0U);

  rdcspv::Debugger *debugger = new rdcspv::Debugger;
  debugger->Parse(shader.spirv.GetSPIRV());
  ShaderDebugTrace *ret = debugger->BeginDebug(apiWrapper, ShaderStage::Compute, entryPoint, spec,
                                               shadRefl.instructionLines, shadRefl.patchData, 0);

  return ret;
}

rdcarray<ShaderDebugState> VulkanReplay::ContinueDebug(ShaderDebugger *debugger)
{
  rdcspv::Debugger *spvDebugger = (rdcspv::Debugger *)debugger;

  if(!spvDebugger)
    return {};

  VkMarkerRegion region("ContinueDebug Simulation Loop");

  return spvDebugger->ContinueDebug();
}
