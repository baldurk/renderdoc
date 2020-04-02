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
#include "vk_shader_cache.h"

#undef None

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
  Address = 0,
  ArrayLength,
  DestX,
  DestY,
  Count,
};

static const uint32_t validMagicNumber = 12345;

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

  rdcspv::MemoryAccessAndParamDatas alignedAccess;
  alignedAccess.setAligned(sizeof(uint32_t));

  rdcspv::Id uint32Type = editor.DeclareType(rdcspv::scalar<uint32_t>());
  rdcspv::Id floatType = editor.DeclareType(rdcspv::scalar<float>());
  rdcspv::Id boolType = editor.DeclareType(rdcspv::scalar<bool>());

  rdcarray<rdcspv::Id> uintConsts;

  auto getUIntConst = [&uintConsts, &editor](uint32_t c) {
    for(uint32_t i = (uint32_t)uintConsts.size(); i <= c; i++)
      uintConsts.push_back(editor.AddConstantImmediate<uint32_t>(uint32_t(i)));

    return uintConsts[c];
  };

  rdcspv::StorageClass bufferClass;

  if(storageMode == Binding)
    bufferClass = editor.StorageBufferClass();
  else
    bufferClass = rdcspv::StorageClass::PhysicalStorageBuffer;

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
  for(size_t i = 0; i < shadRefl.refl.inputSignature.size(); i++)
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

  enum Variant
  {
    Variant_Base,
    Variant_ddxcoarse,
    Variant_ddycoarse,
    Variant_ddxfine,
    Variant_ddyfine,
    Variant_Count,
  };

  struct valueAndDerivs
  {
    rdcspv::Id valueType;
    rdcspv::Id data[Variant_Count];
    uint32_t structIndex;
    rdcspv::OperationList storeOps;
  };

  rdcarray<valueAndDerivs> values;
  values.resize(shadRefl.refl.inputSignature.size());

  {
    rdcarray<rdcspv::Id> ids;
    rdcarray<uint32_t> offsets;
    rdcarray<uint32_t> indices;
    for(size_t i = 0; i < shadRefl.refl.inputSignature.size(); i++)
    {
      const SigParameter &param = shadRefl.refl.inputSignature[i];

      rdcspv::Scalar base;
      switch(param.compType)
      {
        case CompType::Float: base = rdcspv::scalar<float>(); break;
        case CompType::UInt: base = rdcspv::scalar<uint32_t>(); break;
        case CompType::SInt: base = rdcspv::scalar<int32_t>(); break;
        case CompType::Double: base = rdcspv::scalar<double>(); break;
        default: RDCERR("Unexpected type %s", ToStr(param.compType).c_str());
      }

      values[i].structIndex = (uint32_t)offsets.size();

      offsets.push_back(structStride);
      structStride += param.compCount * (base.width / 8);

      if(param.compCount == 1)
        values[i].valueType = editor.DeclareType(base);
      else
        values[i].valueType = editor.DeclareType(rdcspv::Vector(base, param.compCount));

      ids.push_back(values[i].valueType);

      // align offset conservatively, to 16-byte aligned. We do this with explicit uints so we can
      // preview with spirv-cross (and because it doesn't cost anything particularly)
      uint32_t paddingWords = ((16 - (structStride % 16)) / 4) % 4;
      for(uint32_t p = 0; p < paddingWords; p++)
      {
        ids.push_back(uint32Type);
        offsets.push_back(structStride);
        structStride += 4;
      }
    }

    PSInput = editor.DeclareStructType(ids);

    for(size_t i = 0; i < offsets.size(); i++)
    {
      editor.AddDecoration(rdcspv::OpMemberDecorate(
          PSInput, uint32_t(i), rdcspv::DecorationParam<rdcspv::Decoration::Offset>(offsets[i])));
    }

    for(size_t i = 0; i < values.size(); i++)
      editor.SetMemberName(PSInput, values[i].structIndex, shadRefl.refl.inputSignature[i].varName);

    editor.SetName(PSInput, "__rd_PSInput");
  }

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

  rdcspv::Id PSHit = editor.DeclareStructType({
      // float4 pos;
      float4Type,
      // uint prim;
      uint32Type,
      // uint sample;
      uint32Type,
      // uint valid;
      uint32Type,
      // uint padding;
      uint32Type,

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
    offs += sizeof(Vec4f);
    member++;

    editor.AddDecoration(rdcspv::OpMemberDecorate(
        PSHit, member, rdcspv::DecorationParam<rdcspv::Decoration::Offset>(offs)));
    editor.SetMemberName(PSHit, member, "prim");
    offs += sizeof(uint32_t);
    member++;

    editor.AddDecoration(rdcspv::OpMemberDecorate(
        PSHit, member, rdcspv::DecorationParam<rdcspv::Decoration::Offset>(offs)));
    editor.SetMemberName(PSHit, member, "sample");
    offs += sizeof(uint32_t);
    member++;

    editor.AddDecoration(rdcspv::OpMemberDecorate(
        PSHit, member, rdcspv::DecorationParam<rdcspv::Decoration::Offset>(offs)));
    editor.SetMemberName(PSHit, member, "valid");
    offs += sizeof(uint32_t);
    member++;

    editor.AddDecoration(rdcspv::OpMemberDecorate(
        PSHit, member, rdcspv::DecorationParam<rdcspv::Decoration::Offset>(offs)));
    editor.SetMemberName(PSHit, member, "padding");
    offs += sizeof(uint32_t);
    member++;

    RDCASSERT((offs % sizeof(Vec4f)) == 0);
    RDCASSERT((structStride % sizeof(Vec4f)) == 0);

    editor.AddDecoration(rdcspv::OpMemberDecorate(
        PSHit, member, rdcspv::DecorationParam<rdcspv::Decoration::Offset>(offs)));
    editor.SetMemberName(PSHit, member, "IN");
    offs += structStride;
    member++;

    editor.AddDecoration(rdcspv::OpMemberDecorate(
        PSHit, member, rdcspv::DecorationParam<rdcspv::Decoration::Offset>(offs)));
    editor.SetMemberName(PSHit, member, "INddxcoarse");
    offs += structStride;
    member++;

    editor.AddDecoration(rdcspv::OpMemberDecorate(
        PSHit, member, rdcspv::DecorationParam<rdcspv::Decoration::Offset>(offs)));
    editor.SetMemberName(PSHit, member, "INddycoarse");
    offs += structStride;
    member++;

    editor.AddDecoration(rdcspv::OpMemberDecorate(
        PSHit, member, rdcspv::DecorationParam<rdcspv::Decoration::Offset>(offs)));
    editor.SetMemberName(PSHit, member, "INddxfine");
    offs += structStride;
    member++;

    editor.AddDecoration(rdcspv::OpMemberDecorate(
        PSHit, member, rdcspv::DecorationParam<rdcspv::Decoration::Offset>(offs)));
    editor.SetMemberName(PSHit, member, "INddyfine");
    offs += structStride;
    member++;
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

  rdcspv::Id bufptrtype;
  rdcspv::Id ssboVar;
  rdcspv::Id addressConstant;

  if(storageMode == Binding)
  {
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

  editor.AddCapability(rdcspv::Capability::DerivativeControl);

  {
    rdcspv::OperationList ops;

    rdcspv::Id voidType = editor.DeclareType(rdcspv::scalar<void>());

    ops.add(rdcspv::OpFunction(voidType, entryID, rdcspv::FunctionControl::None,
                               editor.DeclareType(rdcspv::FunctionType(voidType, {}))));

    ops.add(rdcspv::OpLabel(editor.MakeId()));
    {
      // grab all the values here and get any derivatives we need now before we branch non-uniformly
      for(size_t i = 0; i < values.size(); i++)
      {
        const SPIRVInterfaceAccess &access = shadRefl.patchData.inputs[i];
        const SigParameter &param = shadRefl.refl.inputSignature[i];

        rdcarray<rdcspv::Id> accessIndices;
        for(uint32_t idx : access.accessChain)
          accessIndices.push_back(getUIntConst(idx));

        rdcspv::Id ptrType =
            editor.DeclareType(rdcspv::Pointer(values[i].valueType, rdcspv::StorageClass::Input));
        rdcspv::Id ptr =
            ops.add(rdcspv::OpAccessChain(ptrType, editor.MakeId(), access.ID, accessIndices));
        rdcspv::Id base = ops.add(rdcspv::OpLoad(values[i].valueType, editor.MakeId(), ptr));

        values[i].data[Variant_Base] = base;

        editor.SetName(base, StringFormat::Fmt("__rd_base_%zu_%s", i, param.varName.c_str()));

        // only float values have derivatives
        if(param.compType == CompType::Float)
        {
          values[i].data[Variant_ddxcoarse] =
              ops.add(rdcspv::OpDPdxCoarse(values[i].valueType, editor.MakeId(), base));
          values[i].data[Variant_ddycoarse] =
              ops.add(rdcspv::OpDPdyCoarse(values[i].valueType, editor.MakeId(), base));
          values[i].data[Variant_ddxfine] =
              ops.add(rdcspv::OpDPdxFine(values[i].valueType, editor.MakeId(), base));
          values[i].data[Variant_ddyfine] =
              ops.add(rdcspv::OpDPdyFine(values[i].valueType, editor.MakeId(), base));

          editor.SetName(values[i].data[Variant_ddxcoarse],
                         StringFormat::Fmt("__rd_ddxcoarse_%zu_%s", i, param.varName.c_str()));
          editor.SetName(values[i].data[Variant_ddycoarse],
                         StringFormat::Fmt("__rd_ddycoarse_%zu_%s", i, param.varName.c_str()));
          editor.SetName(values[i].data[Variant_ddxfine],
                         StringFormat::Fmt("__rd_ddxfine_%zu_%s", i, param.varName.c_str()));
          editor.SetName(values[i].data[Variant_ddyfine],
                         StringFormat::Fmt("__rd_ddyfine_%zu_%s", i, param.varName.c_str()));
        }
        else
        {
          values[i].data[Variant_ddxcoarse] = values[i].data[Variant_ddycoarse] =
              values[i].data[Variant_ddxfine] = values[i].data[Variant_ddyfine] =
                  editor.AddConstant(rdcspv::OpConstantNull(values[i].valueType, editor.MakeId()));

          editor.SetName(values[i].data[Variant_ddxcoarse],
                         StringFormat::Fmt("__rd_noderiv_%zu_%s", i, param.varName.c_str()));
        }
      }

      rdcspv::Id structPtr = ssboVar;

      if(structPtr == rdcspv::Id())
      {
        // if we don't have the struct as a bind, we need to cast it from the pointer
        structPtr = ops.add(rdcspv::OpConvertUToPtr(bufptrtype, editor.MakeId(), addressConstant));

        editor.SetName(structPtr, "HitBuffer");
      }

      rdcspv::Id uintPtr = editor.DeclareType(rdcspv::Pointer(uint32Type, bufferClass));

      // get a pointer to buffer.hit_count
      rdcspv::Id hit_count =
          ops.add(rdcspv::OpAccessChain(uintPtr, editor.MakeId(), structPtr, {getUIntConst(0)}));

      rdcspv::Id scope = editor.AddConstantImmediate<uint32_t>((uint32_t)rdcspv::Scope::Device);
      rdcspv::Id semantics =
          editor.AddConstantImmediate<uint32_t>((uint32_t)rdcspv::MemorySemantics::AcquireRelease);

      // look up the fragcoord
      rdcspv::Id fragCoordLoaded = editor.MakeId();
      if(fragCoord.member == ~0U)
      {
        ops.add(rdcspv::OpLoad(float4Type, fragCoordLoaded, fragCoord.base));
      }
      else
      {
        rdcspv::Id posptr =
            ops.add(rdcspv::OpAccessChain(float4InPtr, editor.MakeId(), fragCoord.base,
                                          {editor.AddConstantImmediate(fragCoord.member)}));
        ops.add(rdcspv::OpLoad(float4Type, fragCoordLoaded, posptr));
      }

      rdcspv::Id bool2Type = editor.DeclareType(rdcspv::Vector(rdcspv::scalar<bool>(), 2));

      // grab x and y
      rdcspv::Id fragXY = ops.add(rdcspv::OpVectorShuffle(
          float2Type, editor.MakeId(), fragCoordLoaded, fragCoordLoaded, {0, 1}));

      // subtract from the destination co-ord
      rdcspv::Id fragXYRelative =
          ops.add(rdcspv::OpFSub(float2Type, editor.MakeId(), fragXY, destXY));

      // abs()
      rdcspv::Id fragXYAbs = ops.add(rdcspv::OpGLSL450(float2Type, editor.MakeId(), glsl450,
                                                       rdcspv::GLSLstd450::FAbs, {fragXYRelative}));

      rdcspv::Id half = editor.AddConstantImmediate<float>(0.5f);
      rdcspv::Id threshold =
          editor.AddConstant(rdcspv::OpConstantComposite(float2Type, editor.MakeId(), {half, half}));

      // less than 0.5
      rdcspv::Id inPixelXY =
          ops.add(rdcspv::OpFOrdLessThan(bool2Type, editor.MakeId(), fragXYAbs, threshold));

      // both less than 0.5
      rdcspv::Id inPixel = ops.add(rdcspv::OpAll(boolType, editor.MakeId(), inPixelXY));

      // bool inPixel = all(abs(gl_FragCoord.xy - dest.xy) < 0.5f);

      rdcspv::Id killLabel = editor.MakeId();
      rdcspv::Id continueLabel = editor.MakeId();
      ops.add(rdcspv::OpSelectionMerge(killLabel, rdcspv::SelectionControl::None));
      ops.add(rdcspv::OpBranchConditional(inPixel, continueLabel, killLabel));
      ops.add(rdcspv::OpLabel(continueLabel));

      // allocate a slot with atomic add
      rdcspv::Id slot = ops.add(rdcspv::OpAtomicIAdd(uint32Type, editor.MakeId(), hit_count, scope,
                                                     semantics, getUIntConst(1)));

      editor.SetName(slot, "slot");

      rdcspv::Id inRange = ops.add(rdcspv::OpULessThan(boolType, editor.MakeId(), slot, arrayLength));

      rdcspv::Id killLabel2 = editor.MakeId();
      continueLabel = editor.MakeId();
      ops.add(rdcspv::OpSelectionMerge(killLabel2, rdcspv::SelectionControl::None));
      ops.add(rdcspv::OpBranchConditional(inRange, continueLabel, killLabel2));
      ops.add(rdcspv::OpLabel(continueLabel));

      rdcspv::Id hitptr = editor.DeclareType(rdcspv::Pointer(PSHit, bufferClass));

      // get a pointer to the hit for our slot
      rdcspv::Id hit =
          ops.add(rdcspv::OpAccessChain(hitptr, editor.MakeId(), structPtr, {getUIntConst(1), slot}));

      // store fixed properties

      rdcspv::Id storePtr =
          ops.add(rdcspv::OpAccessChain(float4BufPtr, editor.MakeId(), hit, {getUIntConst(0)}));
      ops.add(rdcspv::OpStore(storePtr, fragCoordLoaded, alignedAccess));

      rdcspv::Id loaded;
      if(primitiveID.base != rdcspv::Id())
      {
        if(primitiveID.member == ~0U)
        {
          loaded = ops.add(rdcspv::OpLoad(uint32Type, editor.MakeId(), primitiveID.base));
        }
        else
        {
          rdcspv::Id posptr =
              ops.add(rdcspv::OpAccessChain(uint32InPtr, editor.MakeId(), primitiveID.base,
                                            {editor.AddConstantImmediate(primitiveID.member)}));
          loaded = ops.add(rdcspv::OpLoad(uint32Type, editor.MakeId(), posptr));
        }
      }
      else
      {
        // explicitly store 0
        loaded = getUIntConst(0);
      }

      storePtr =
          ops.add(rdcspv::OpAccessChain(uint32BufPtr, editor.MakeId(), hit, {getUIntConst(1)}));
      ops.add(rdcspv::OpStore(storePtr, loaded, alignedAccess));

      if(sampleIndex.base != rdcspv::Id())
      {
        if(sampleIndex.member == ~0U)
        {
          loaded = ops.add(rdcspv::OpLoad(uint32Type, editor.MakeId(), sampleIndex.base));
        }
        else
        {
          rdcspv::Id posptr =
              ops.add(rdcspv::OpAccessChain(uint32InPtr, editor.MakeId(), sampleIndex.base,
                                            {editor.AddConstantImmediate(sampleIndex.member)}));
          loaded = ops.add(rdcspv::OpLoad(uint32Type, editor.MakeId(), posptr));
        }
      }
      else
      {
        // explicitly store 0
        loaded = getUIntConst(0);
      }

      storePtr =
          ops.add(rdcspv::OpAccessChain(uint32BufPtr, editor.MakeId(), hit, {getUIntConst(2)}));
      ops.add(rdcspv::OpStore(storePtr, loaded, alignedAccess));

      storePtr =
          ops.add(rdcspv::OpAccessChain(uint32BufPtr, editor.MakeId(), hit, {getUIntConst(3)}));
      ops.add(rdcspv::OpStore(storePtr, editor.AddConstantImmediate(validMagicNumber), alignedAccess));

      // store 0 in the padding
      storePtr =
          ops.add(rdcspv::OpAccessChain(uint32BufPtr, editor.MakeId(), hit, {getUIntConst(4)}));
      ops.add(rdcspv::OpStore(storePtr, getUIntConst(0), alignedAccess));

      {
        rdcspv::Id inputPtrType = editor.DeclareType(rdcspv::Pointer(PSInput, bufferClass));

        rdcspv::Id outputPtrs[Variant_Count] = {
            ops.add(rdcspv::OpAccessChain(inputPtrType, editor.MakeId(), hit, {getUIntConst(5)})),
            ops.add(rdcspv::OpAccessChain(inputPtrType, editor.MakeId(), hit, {getUIntConst(6)})),
            ops.add(rdcspv::OpAccessChain(inputPtrType, editor.MakeId(), hit, {getUIntConst(7)})),
            ops.add(rdcspv::OpAccessChain(inputPtrType, editor.MakeId(), hit, {getUIntConst(8)})),
            ops.add(rdcspv::OpAccessChain(inputPtrType, editor.MakeId(), hit, {getUIntConst(9)})),
        };

        for(size_t i = 0; i < values.size(); i++)
        {
          rdcspv::Id ptrType = editor.DeclareType(rdcspv::Pointer(values[i].valueType, bufferClass));

          for(size_t j = 0; j < Variant_Count; j++)
          {
            rdcspv::Id ptr = ops.add(rdcspv::OpAccessChain(ptrType, editor.MakeId(), outputPtrs[j],
                                                           {getUIntConst(values[i].structIndex)}));
            ops.add(rdcspv::OpStore(ptr, values[i].data[j], alignedAccess));
          }
        }
      }

      // join up with the early-outs we did
      ops.add(rdcspv::OpBranch(killLabel2));
      ops.add(rdcspv::OpLabel(killLabel2));
      ops.add(rdcspv::OpBranch(killLabel));
      ops.add(rdcspv::OpLabel(killLabel));
    }
    // don't return, kill. This makes it well-defined that we don't write anything to our outputs
    ops.add(rdcspv::OpKill());

    ops.add(rdcspv::OpFunctionEnd());

    editor.AddFunction(ops);
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

  VkDevice dev = m_pDriver->GetDev();
  VkResult vkr = VK_SUCCESS;

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

  uint32_t overdrawLevels = 100;    // maximum number of overdraw levels

  VkGraphicsPipelineCreateInfo graphicsInfo = {};

  m_pDriver->GetShaderCache()->MakeGraphicsPipelineInfo(graphicsInfo, state.graphics.pipeline);

  VkDeviceSize feedbackStorageSize = overdrawLevels * structStride + sizeof(Vec4f) + 1024;

  if(feedbackStorageSize > m_BindlessFeedback.FeedbackBuffer.sz)
  {
    uint32_t flags = GPUBuffer::eGPUBufferGPULocal | GPUBuffer::eGPUBufferSSBO;

    if(storageMode != Binding)
      flags |= GPUBuffer::eGPUBufferAddressable;

    m_BindlessFeedback.FeedbackBuffer.Destroy();
    m_BindlessFeedback.FeedbackBuffer.Create(m_pDriver, dev, feedbackStorageSize, 1, flags);
  }

  struct SpecData
  {
    VkDeviceAddress bufferAddress;
    uint32_t arrayLength;
    float destX;
    float destY;
  } specData;

  specData.arrayLength = overdrawLevels;
  specData.destX = float(x) + 0.5f;
  specData.destY = float(y) + 0.5f;

  VkDescriptorPool descpool = VK_NULL_HANDLE;
  rdcarray<VkDescriptorSetLayout> setLayouts;
  rdcarray<VkDescriptorSet> descSets;

  VkPipelineLayout pipeLayout = VK_NULL_HANDLE;

  if(storageMode != Binding)
  {
    RDCCOMPILE_ASSERT(VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO ==
                          VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO_EXT,
                      "KHR and EXT buffer_device_address should be interchangeable here.");
    VkBufferDeviceAddressInfo getAddressInfo = {VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO};
    getAddressInfo.buffer = m_BindlessFeedback.FeedbackBuffer.buf;

    if(storageMode == KHR_bda)
      specData.bufferAddress = m_pDriver->vkGetBufferDeviceAddress(dev, &getAddressInfo);
    else
      specData.bufferAddress = m_pDriver->vkGetBufferDeviceAddressEXT(dev, &getAddressInfo);
  }
  else
  {
    VkDescriptorSetLayoutBinding newBindings[] = {
        {0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VkShaderStageFlags(VK_SHADER_STAGE_FRAGMENT_BIT),
         NULL},
    };
    RDCCOMPILE_ASSERT(ARRAY_COUNT(newBindings) == 1,
                      "Should only be one new descriptor for fetching PS inputs");

    // create a duplicate set of descriptor sets, all visible to compute, with bindings shifted to
    // account for new ones we need. This also copies the existing bindings into the new sets
    PatchReservedDescriptors(state.graphics, descpool, setLayouts, descSets,
                             VkShaderStageFlagBits(), newBindings, ARRAY_COUNT(newBindings));

    // create pipeline layout with new descriptor set layouts
    const rdcarray<VkPushConstantRange> &push = c.m_PipelineLayout[pipe.layout].pushRanges;

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

    graphicsInfo.layout = pipeLayout;

    // vkUpdateDescriptorSet desc set to point to buffer
    VkDescriptorBufferInfo desc = {0};

    m_BindlessFeedback.FeedbackBuffer.FillDescriptor(desc);

    VkWriteDescriptorSet write = {
        VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        NULL,
        Unwrap(descSets[0]),
        0,
        0,
        1,
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        NULL,
        &desc,
        NULL,
    };

    ObjDisp(dev)->UpdateDescriptorSets(Unwrap(dev), 1, &write, 0, NULL);
  }

  // create fragment shader with modified code
  VkShaderModuleCreateInfo moduleCreateInfo = {VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};

  VkSpecializationMapEntry specMaps[] = {
      {
          (uint32_t)InputSpecConstant::Address, offsetof(SpecData, bufferAddress),
          sizeof(SpecData::bufferAddress),
      },
      {
          (uint32_t)InputSpecConstant::ArrayLength, offsetof(SpecData, arrayLength),
          sizeof(SpecData::arrayLength),
      },
      {
          (uint32_t)InputSpecConstant::DestX, offsetof(SpecData, destX), sizeof(SpecData::destX),
      },
      {
          (uint32_t)InputSpecConstant::DestY, offsetof(SpecData, destY), sizeof(SpecData::destY),
      },
  };

  VkShaderModule module = VK_NULL_HANDLE;
  VkSpecializationInfo specInfo = {};
  specInfo.dataSize = sizeof(specData);
  specInfo.pData = &specData;
  specInfo.mapEntryCount = ARRAY_COUNT(specMaps);
  specInfo.pMapEntries = specMaps;

  RDCCOMPILE_ASSERT((size_t)InputSpecConstant::Count == ARRAY_COUNT(specMaps),
                    "Spec constants changed");

  for(uint32_t i = 0; i < graphicsInfo.stageCount; i++)
  {
    VkPipelineShaderStageCreateInfo &stage =
        (VkPipelineShaderStageCreateInfo &)graphicsInfo.pStages[i];

    if(stage.stage == VK_SHADER_STAGE_FRAGMENT_BIT)
    {
      moduleCreateInfo.pCode = fragspv.data();
      moduleCreateInfo.codeSize = fragspv.size() * sizeof(uint32_t);

      vkr = m_pDriver->vkCreateShaderModule(dev, &moduleCreateInfo, NULL, &module);
      RDCASSERTEQUAL(vkr, VK_SUCCESS);

      stage.module = module;

      stage.pSpecializationInfo = &specInfo;

      break;
    }
  }

  if(module == VK_NULL_HANDLE)
  {
    RDCERR("Couldn't find fragment shader in pipeline info!");
  }

  VkPipeline inputsPipe;
  vkr =
      m_pDriver->vkCreateGraphicsPipelines(dev, VK_NULL_HANDLE, 1, &graphicsInfo, NULL, &inputsPipe);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  // make copy of state to draw from
  VulkanRenderState modifiedstate = state;

  // bind created pipeline to partial replay state
  modifiedstate.graphics.pipeline = GetResID(inputsPipe);

  if(storageMode == Binding)
  {
    // Treplace descriptor set IDs with our temporary sets. The offsets we keep the same. If the
    // original draw had no sets, we ensure there's room (with no offsets needed)
    if(modifiedstate.graphics.descSets.empty())
      modifiedstate.graphics.descSets.resize(1);

    for(size_t i = 0; i < descSets.size(); i++)
    {
      modifiedstate.graphics.descSets[i].pipeLayout = GetResID(pipeLayout);
      modifiedstate.graphics.descSets[i].descSet = GetResID(descSets[i]);
    }
  }

  {
    VkCommandBuffer cmd = m_pDriver->GetNextCmd();

    VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL,
                                          VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};

    vkr = ObjDisp(dev)->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    // fill destination buffer with 0s to ensure a baseline to then feedback against
    ObjDisp(dev)->CmdFillBuffer(Unwrap(cmd), Unwrap(m_BindlessFeedback.FeedbackBuffer.buf), 0,
                                feedbackStorageSize, 0);

    VkBufferMemoryBarrier feedbackbufBarrier = {
        VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
        NULL,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_ACCESS_SHADER_WRITE_BIT,
        VK_QUEUE_FAMILY_IGNORED,
        VK_QUEUE_FAMILY_IGNORED,
        Unwrap(m_BindlessFeedback.FeedbackBuffer.buf),
        0,
        feedbackStorageSize,
    };

    // wait for the above fill to finish.
    DoPipelineBarrier(cmd, 1, &feedbackbufBarrier);

    modifiedstate.BeginRenderPassAndApplyState(m_pDriver, cmd, VulkanRenderState::BindGraphics);

    if(draw->flags & DrawFlags::Indexed)
    {
      ObjDisp(cmd)->CmdDrawIndexed(Unwrap(cmd), draw->numIndices, draw->numInstances,
                                   draw->indexOffset, draw->baseVertex, draw->instanceOffset);
    }
    else
    {
      ObjDisp(cmd)->CmdDraw(Unwrap(cmd), draw->numIndices, draw->numInstances, draw->vertexOffset,
                            draw->instanceOffset);
    }

    modifiedstate.EndRenderPass(cmd);

    vkr = ObjDisp(dev)->EndCommandBuffer(Unwrap(cmd));
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    m_pDriver->SubmitCmds();
    m_pDriver->FlushQ();
  }

  bytebuf data;
  GetBufferData(GetResID(m_BindlessFeedback.FeedbackBuffer.buf), 0, 0, data);

  byte *base = data.data();
  uint32_t numHits = *(uint32_t *)base;

  if(numHits > overdrawLevels)
  {
    RDCERR("%u hits, more than max overdraw levels allowed %u. Clamping", numHits, overdrawLevels);
    numHits = overdrawLevels;
  }

  base += sizeof(Vec4f);

  struct PSHit
  {
    Vec4f pos;
    uint32_t prim;
    uint32_t sample;
    uint32_t valid;
    uint32_t padding;
    // PSInput base, ddx, ....
  };

  for(uint32_t i = 0; i < numHits; i++)
  {
    PSHit *hit = (PSHit *)(base + structStride * i);

    RDCLOG("Hit %u at %f, %f, %f, %f", i, hit->pos.x, hit->pos.y, hit->pos.z, hit->pos.w);
  }

  rdcspv::Debugger *debugger = new rdcspv::Debugger;
  debugger->Parse(shader.spirv.GetSPIRV());
  ShaderDebugTrace *ret = debugger->BeginDebug(apiWrapper, ShaderStage::Pixel, entryPoint, spec,
                                               shadRefl.instructionLines, shadRefl.patchData, 0);

  if(descpool != VK_NULL_HANDLE)
  {
    // delete descriptors. Technically we don't have to free the descriptor sets, but our tracking
    // on replay doesn't handle destroying children of pooled objects so we do it explicitly anyway.
    m_pDriver->vkFreeDescriptorSets(dev, descpool, (uint32_t)descSets.size(), descSets.data());

    m_pDriver->vkDestroyDescriptorPool(dev, descpool, NULL);
  }

  for(VkDescriptorSetLayout layout : setLayouts)
    m_pDriver->vkDestroyDescriptorSetLayout(dev, layout, NULL);

  // delete pipeline layout
  m_pDriver->vkDestroyPipelineLayout(dev, pipeLayout, NULL);

  // delete pipeline
  m_pDriver->vkDestroyPipeline(dev, inputsPipe, NULL);

  // delete shader module
  m_pDriver->vkDestroyShaderModule(dev, module, NULL);

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
