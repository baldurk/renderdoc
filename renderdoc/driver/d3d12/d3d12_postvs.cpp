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

#include <algorithm>
#include "core/settings.h"
#include "driver/dxgi/dxgi_common.h"
#include "driver/shaders/dxil/dxil_bytecode_editor.h"
#include "replay/replay_driver.h"
#include "strings/string_utils.h"
#include "d3d12_command_list.h"
#include "d3d12_command_queue.h"
#include "d3d12_debug.h"
#include "d3d12_device.h"
#include "d3d12_replay.h"
#include "d3d12_shader_cache.h"

RDOC_CONFIG(rdcstr, D3D12_Debug_PostVSDumpDirPath, "",
            "Path to dump post mesh shader patched DXIL files.");

struct ScopedOOMHandle12
{
  ScopedOOMHandle12(WrappedID3D12Device *dev)
  {
    m_pDevice = dev;
    m_pDevice->HandleOOM(true);
  }

  ~ScopedOOMHandle12() { m_pDevice->HandleOOM(false); }
  WrappedID3D12Device *m_pDevice;
};

struct OutDXILSigLocation
{
  uint32_t offset;
  uint32_t scalarElemSize;
  uint32_t rowCount;
  uint32_t colCount;
};

struct OutDXILMeshletLayout
{
  rdcarray<OutDXILSigLocation> sigLocations;
  uint32_t meshletByteSize;
  uint32_t indexCountPerPrim;
  uint32_t vertArrayLength;
  uint32_t primArrayLength;
  uint32_t vertStride;
  uint32_t primStride;
};

enum PayloadCopyDir
{
  BufferToPayload,
  PayloadToBuffer,
};

static rdcstr makeBufferLoadStoreSuffix(const DXIL::Type *type)
{
  return StringFormat::Fmt("%c%u", type->scalarType == DXIL::Type::Float ? 'f' : 'i', type->bitWidth);
}

static void PayloadBufferCopy(PayloadCopyDir dir, DXIL::ProgramEditor &editor, DXIL::Function *f,
                              size_t &curInst, DXIL::Instruction *baseOffset,
                              DXIL::Instruction *handle, const DXIL::Type *memberType,
                              uint32_t &uavByteOffset, const rdcarray<DXIL::Value *> &gepChain)
{
  using namespace DXIL;

  if(memberType->type == Type::Scalar)
  {
    const Type *i32 = editor.GetInt32Type();
    const Type *i8 = editor.GetInt8Type();
    const Type *voidType = editor.GetVoidType();
    const Type *handleType = editor.CreateNamedStructType(
        "dx.types.Handle", {editor.CreatePointerType(i8, Type::PointerAddrSpace::Default)});
    makeBufferLoadStoreSuffix(memberType);

    const uint32_t alignment = RDCMAX(4U, memberType->bitWidth / 8);
    Constant *align = editor.CreateConstant(alignment);

    Constant *payloadGep = editor.CreateConstantGEP(
        editor.GetPointerType(memberType, gepChain[0]->type->addrSpace), gepChain);

    Instruction *offset = editor.CreateInstruction(
        Operation::Add, i32, {baseOffset, editor.CreateConstant(uavByteOffset)});
    offset->opFlags() = offset->opFlags() | InstructionFlags::NoSignedWrap;

    rdcstr suffix = makeBufferLoadStoreSuffix(memberType);

    if(dir == BufferToPayload)
    {
      const Type *resRet = editor.CreateNamedStructType(
          "dx.types.ResRet." + suffix, {memberType, memberType, memberType, memberType, i32});
      const Function *loadBuf = editor.DeclareFunction("dx.op.rawBufferLoad." + suffix, resRet,
                                                       {i32, handleType, i32, i32, i8, i32},
                                                       Attribute::NoUnwind | Attribute::ReadOnly);

      editor.InsertInstruction(f, curInst++, offset);

      Instruction *srcRet = editor.InsertInstruction(
          f, curInst++,
          editor.CreateInstruction(loadBuf, DXOp::RawBufferLoad,
                                   {handle, offset, editor.CreateUndef(i32),
                                    editor.CreateConstant((uint8_t)0x1), align}));

      Instruction *src = editor.InsertInstruction(
          f, curInst++,
          editor.CreateInstruction(Operation::ExtractVal, i32, {srcRet, editor.CreateLiteral(0)}));

      Instruction *store = editor.CreateInstruction(Operation::Store);

      store->type = voidType;
      store->align = (Log2Floor(alignment) + 1) & 0xff;
      store->args = {payloadGep, src};

      editor.InsertInstruction(f, curInst++, store);
    }
    else if(dir == PayloadToBuffer)
    {
      Instruction *load = editor.CreateInstruction(Operation::Load);

      load->type = memberType;
      load->align = (Log2Floor(alignment) + 1) & 0xff;
      load->args = {payloadGep};

      editor.InsertInstruction(f, curInst++, load);

      editor.InsertInstruction(f, curInst++, offset);

      const Function *storeBuf = editor.DeclareFunction(
          "dx.op.rawBufferStore." + suffix, voidType,
          {i32, handleType, i32, i32, memberType, memberType, memberType, memberType, i8, i32},
          Attribute::NoUnwind);

      editor.InsertInstruction(
          f, curInst++,
          editor.CreateInstruction(
              storeBuf, DXOp::RawBufferStore,
              {handle, offset, editor.CreateUndef(i32), load, editor.CreateUndef(memberType),
               editor.CreateUndef(memberType), editor.CreateUndef(memberType),
               editor.CreateConstant((uint8_t)0x1), align}));
    }

    uavByteOffset += memberType->bitWidth / 8U;
  }
  else if(memberType->type == Type::Array)
  {
    rdcarray<Value *> elemGepChain = gepChain;
    elemGepChain.push_back(NULL);
    for(uint32_t i = 0; i < memberType->elemCount; i++)
    {
      elemGepChain.back() = editor.CreateConstant(i);
      PayloadBufferCopy(dir, editor, f, curInst, baseOffset, handle, memberType->inner,
                        uavByteOffset, elemGepChain);
    }
  }
  else if(memberType->type == Type::Struct)
  {
    rdcarray<Value *> elemGepChain = gepChain;
    elemGepChain.push_back(NULL);
    for(uint32_t i = 0; i < memberType->members.size(); i++)
    {
      elemGepChain.back() = editor.CreateConstant(i);
      PayloadBufferCopy(dir, editor, f, curInst, baseOffset, handle, memberType->members[i],
                        uavByteOffset, elemGepChain);
    }
  }
  else
  {
    // shouldn't see functions, pointers, metadata or labels
    // also (for DXIL) shouldn't see vectors
    RDCERR("Unexpected element type in payload struct");
  }
}

static void AddDXILAmpShaderPayloadStores(const DXBC::DXBCContainer *dxbc, uint32_t space,
                                          const rdcfixedarray<uint32_t, 3> &dispatchDim,
                                          uint32_t &payloadSize, bytebuf &editedBlob)
{
  using namespace DXIL;

  ProgramEditor editor(dxbc, editedBlob);

  bool isShaderModel6_6OrAbove =
      dxbc->m_Version.Major > 6 || (dxbc->m_Version.Major == 6 && dxbc->m_Version.Minor >= 6);

  const Type *i32 = editor.GetInt32Type();
  const Type *i8 = editor.GetInt8Type();
  const Type *i1 = editor.GetBoolType();
  const Type *voidType = editor.GetVoidType();

  const Type *handleType = editor.CreateNamedStructType(
      "dx.types.Handle", {editor.CreatePointerType(i8, Type::PointerAddrSpace::Default)});

  // this function is named differently based on the payload struct name, so search by prefix, we
  // expect the actual type to be the same as we're just modifying the payload in place
  const Function *dispatchMesh = editor.GetFunctionByPrefix("dx.op.dispatchMesh");

  const Function *createHandle = NULL;
  const Function *createHandleFromBinding = NULL;
  const Function *annotateHandle = NULL;

  // reading from a binding uses a different function in SM6.6+
  if(isShaderModel6_6OrAbove)
  {
    const Type *resBindType = editor.CreateNamedStructType("dx.types.ResBind", {i32, i32, i32, i8});
    createHandleFromBinding = editor.DeclareFunction("dx.op.createHandleFromBinding", handleType,
                                                     {i32, resBindType, i32, i1},
                                                     Attribute::NoUnwind | Attribute::ReadNone);

    const Type *resourcePropertiesType =
        editor.CreateNamedStructType("dx.types.ResourceProperties", {i32, i32});
    annotateHandle = editor.DeclareFunction("dx.op.annotateHandle", handleType,
                                            {i32, handleType, resourcePropertiesType},
                                            Attribute::NoUnwind | Attribute::ReadNone);
  }
  else if(!createHandle && !isShaderModel6_6OrAbove)
  {
    createHandle = editor.DeclareFunction("dx.op.createHandle", handleType, {i32, i8, i32, i32, i1},
                                          Attribute::NoUnwind | Attribute::ReadOnly);
  }

  const Function *barrier = editor.DeclareFunction("dx.op.barrier", voidType, {i32, i32},
                                                   Attribute::NoUnwind | Attribute::NoDuplicate);
  const Function *flattenedThreadIdInGroup = editor.DeclareFunction(
      "dx.op.flattenedThreadIdInGroup.i32", i32, {i32}, Attribute::NoUnwind | Attribute::ReadNone);
  const Function *groupId = editor.DeclareFunction("dx.op.groupId.i32", i32, {i32, i32},
                                                   Attribute::NoUnwind | Attribute::ReadNone);
  const Function *rawBufferStore = editor.DeclareFunction(
      "dx.op.rawBufferStore.i32", voidType,
      {i32, handleType, i32, i32, i32, i32, i32, i32, i8, i32}, Attribute::NoUnwind);

  // declare the resource, this happens purely in metadata but we need to store the slot
  uint32_t regSlot = 0;
  Metadata *reslist = NULL;
  {
    const Type *rw = editor.CreateNamedStructType("struct.RWByteAddressBuffer", {i32});
    const Type *rwptr = editor.CreatePointerType(rw, Type::PointerAddrSpace::Default);

    Metadata *resources = editor.CreateNamedMetadata("dx.resources");
    if(resources->children.empty())
      resources->children.push_back(editor.CreateMetadata());

    reslist = resources->children[0];

    if(reslist->children.empty())
      reslist->children.resize(4);

    Metadata *uavs = reslist->children[1];
    // if there isn't a UAV list, create an empty one so we can add our own
    if(!uavs)
      uavs = reslist->children[1] = editor.CreateMetadata();

    for(size_t i = 0; i < uavs->children.size(); i++)
    {
      // each UAV child should have a fixed format, [0] is the reg ID and I think this should always
      // be == the index
      const Metadata *uav = uavs->children[i];
      const Constant *slot = cast<Constant>(uav->children[(size_t)ResField::ID]->value);

      if(!slot)
      {
        RDCWARN("Unexpected non-constant slot ID in UAV");
        continue;
      }

      RDCASSERT(slot->getU32() == i);

      uint32_t id = slot->getU32();
      regSlot = RDCMAX(id + 1, regSlot);
    }

    Constant rwundef;
    rwundef.type = rwptr;
    rwundef.setUndef(true);

    // create the new UAV record
    Metadata *uav = editor.CreateMetadata();
    uav->children = {
        editor.CreateConstantMetadata(regSlot),
        editor.CreateConstantMetadata(editor.CreateConstant(rwundef)),
        editor.CreateConstantMetadata(""),
        editor.CreateConstantMetadata(space),
        editor.CreateConstantMetadata(1U),                                   // reg base
        editor.CreateConstantMetadata(1U),                                   // reg count
        editor.CreateConstantMetadata(uint32_t(ResourceKind::RawBuffer)),    // shape
        editor.CreateConstantMetadata(false),                                // globally coherent
        editor.CreateConstantMetadata(false),                                // hidden counter
        editor.CreateConstantMetadata(false),                                // raster order
        NULL,                                                                // UAV tags
    };

    uavs->children.push_back(uav);
  }

  payloadSize = 0;

  rdcstr entryName;
  // add the entry point tags
  {
    Metadata *entryPoints = editor.GetMetadataByName("dx.entryPoints");

    if(!entryPoints)
    {
      RDCERR("Couldn't find entry point list");
      return;
    }

    // TODO select the entry point for multiple entry points? RT only for now
    Metadata *entry = entryPoints->children[0];

    entryName = entry->children[1]->str;

    Metadata *taglist = entry->children[4];
    if(!taglist)
      taglist = entry->children[4] = editor.CreateMetadata();

    // find existing shader flags tag, if there is one
    Metadata *shaderFlagsTag = NULL;
    Metadata *shaderFlagsData = NULL;
    Metadata *ampData = NULL;
    size_t flagsIndex = 0;
    for(size_t t = 0; taglist && t < taglist->children.size(); t += 2)
    {
      RDCASSERT(taglist->children[t]->isConstant);
      if(cast<Constant>(taglist->children[t]->value)->getU32() ==
         (uint32_t)ShaderEntryTag::ShaderFlags)
      {
        shaderFlagsTag = taglist->children[t];
        shaderFlagsData = taglist->children[t + 1];
        flagsIndex = t + 1;
      }
      else if(cast<Constant>(taglist->children[t]->value)->getU32() ==
              (uint32_t)ShaderEntryTag::Amplification)
      {
        ampData = taglist->children[t + 1];
      }
    }

    uint32_t shaderFlagsValue =
        shaderFlagsData ? cast<Constant>(shaderFlagsData->value)->getU32() : 0U;

    // raw and structured buffers
    shaderFlagsValue |= 0x10;

    // UAVs on non-PS/CS stages
    shaderFlagsValue |= 0x10000;

    // (re-)create shader flags tag
    Type *i64 = editor.CreateScalarType(Type::Int, 64);
    shaderFlagsData =
        editor.CreateConstantMetadata(editor.CreateConstant(Constant(i64, shaderFlagsValue)));

    // if we didn't have a shader tags entry at all, create the metadata node for the shader flags
    // tag
    if(!shaderFlagsTag)
      shaderFlagsTag = editor.CreateConstantMetadata((uint32_t)ShaderEntryTag::ShaderFlags);

    // if we had a tag already, we can just re-use that tag node and replace the data node.
    // Otherwise we need to add both, and we insert them first
    if(flagsIndex)
    {
      taglist->children[flagsIndex] = shaderFlagsData;
    }
    else
    {
      taglist->children.insert(0, shaderFlagsTag);
      taglist->children.insert(1, shaderFlagsData);
    }

    // set reslist and taglist in case they were null before
    entry->children[3] = reslist;
    entry->children[4] = taglist;

    // get payload size from amplification tags
    payloadSize = cast<Constant>(ampData->children[1]->value)->getU32();
  }

  // get the editor to patch PSV0 with our extra UAV
  editor.RegisterUAV(DXILResourceType::ByteAddressUAV, space, 1, 1, ResourceKind::RawBuffer);

  Function *f = editor.GetFunctionByName(entryName);

  if(!f)
  {
    RDCERR("Couldn't find entry point function '%s'", entryName.c_str());
    return;
  }

  // find the dispatchMesh call, and from there the global groupshared variable that's the payload
  GlobalVar *payloadVariable = NULL;
  Type *payloadType = NULL;
  for(size_t i = 0; i < f->instructions.size(); i++)
  {
    const Instruction &inst = *f->instructions[i];

    if(inst.op == Operation::Call && inst.getFuncCall()->name == dispatchMesh->name)
    {
      if(inst.args.size() != 5)
      {
        RDCERR("Unexpected number of arguments to dispatchMesh");
        continue;
      }
      payloadVariable = cast<GlobalVar>(inst.args[4]);
      if(!payloadVariable)
      {
        RDCERR("Unexpected non-variable payload argument to dispatchMesh");
        continue;
      }

      payloadType = (Type *)payloadVariable->type;

      RDCASSERT(payloadType->type == Type::Pointer);
      payloadType = (Type *)payloadType->inner;

      break;
    }
  }

  // don't need to patch the payload type here because it's not going to be used for anything
  RDCASSERT(payloadType && payloadType->type == Type::Struct);

  // create our handle first thing
  Constant *annotateConstant = NULL;
  Instruction *handle = NULL;
  size_t prelimInst = 0;
  if(createHandle)
  {
    RDCASSERT(!isShaderModel6_6OrAbove);
    handle = editor.InsertInstruction(
        f, prelimInst++,
        editor.CreateInstruction(createHandle, DXOp::CreateHandle,
                                 {
                                     // kind = UAV
                                     editor.CreateConstant((uint8_t)HandleKind::UAV),
                                     // ID/slot
                                     editor.CreateConstant(regSlot),
                                     // register
                                     editor.CreateConstant(1U),
                                     // non-uniform
                                     editor.CreateConstant(false),
                                 }));
  }
  else if(createHandleFromBinding)
  {
    RDCASSERT(isShaderModel6_6OrAbove);
    const Type *resBindType = editor.CreateNamedStructType("dx.types.ResBind", {});
    Constant *resBindConstant =
        editor.CreateConstant(resBindType, {
                                               // Lower id bound
                                               editor.CreateConstant(1U),
                                               // Upper id bound
                                               editor.CreateConstant(1U),
                                               // Space ID
                                               editor.CreateConstant(space),
                                               // kind = UAV
                                               editor.CreateConstant((uint8_t)HandleKind::UAV),
                                           });

    Instruction *unannotatedHandle = editor.InsertInstruction(
        f, prelimInst++,
        editor.CreateInstruction(createHandleFromBinding, DXOp::CreateHandleFromBinding,
                                 {
                                     // resBind
                                     resBindConstant,
                                     // ID/slot
                                     editor.CreateConstant(1U),
                                     // non-uniform
                                     editor.CreateConstant(false),
                                 }));

    annotateConstant = editor.CreateConstant(
        editor.CreateNamedStructType("dx.types.ResourceProperties", {}),
        {
            // IsUav : (1 << 12)
            editor.CreateConstant(uint32_t((1 << 12) | (uint32_t)ResourceKind::RawBuffer)),
            //
            editor.CreateConstant(0U),
        });

    handle = editor.InsertInstruction(f, prelimInst++,
                                      editor.CreateInstruction(annotateHandle, DXOp::AnnotateHandle,
                                                               {
                                                                   // Resource handle
                                                                   unannotatedHandle,
                                                                   // Resource properties
                                                                   annotateConstant,
                                                               }));
  }

  RDCASSERT(handle);

  // now calculate our offset
  Constant *i32_0 = editor.CreateConstant(0U);
  Constant *i32_1 = editor.CreateConstant(1U);
  Constant *i32_2 = editor.CreateConstant(2U);

  Instruction *baseOffset = NULL;

  Instruction *groupX = NULL, *groupY = NULL, *groupZ = NULL;

  {
    // get our output location from group ID
    groupX = editor.InsertInstruction(f, prelimInst++,
                                      editor.CreateInstruction(groupId, DXOp::GroupId, {i32_0}));
    groupY = editor.InsertInstruction(f, prelimInst++,
                                      editor.CreateInstruction(groupId, DXOp::GroupId, {i32_1}));
    groupZ = editor.InsertInstruction(f, prelimInst++,
                                      editor.CreateInstruction(groupId, DXOp::GroupId, {i32_2}));
  }

  // get the flat thread ID for comparisons
  Instruction *flatId = editor.InsertInstruction(
      f, prelimInst++,
      editor.CreateInstruction(flattenedThreadIdInGroup, DXOp::FlattenedThreadIdInGroup, {}));

  Value *dimX = editor.CreateConstant(dispatchDim[0]);
  Value *dimY = editor.CreateConstant(dispatchDim[1]);

  {
    Instruction *dimXY = editor.InsertInstruction(
        f, prelimInst++, editor.CreateInstruction(Operation::Mul, i32, {dimX, dimY}));

    // linearise to slot based on the number of dispatches
    Instruction *groupYMul = editor.InsertInstruction(
        f, prelimInst++, editor.CreateInstruction(Operation::Mul, i32, {groupY, dimX}));
    Instruction *groupZMul = editor.InsertInstruction(
        f, prelimInst++, editor.CreateInstruction(Operation::Mul, i32, {groupZ, dimXY}));
    Instruction *groupYZAdd = editor.InsertInstruction(
        f, prelimInst++, editor.CreateInstruction(Operation::Add, i32, {groupYMul, groupZMul}));
    Instruction *flatIndex = editor.InsertInstruction(
        f, prelimInst++, editor.CreateInstruction(Operation::Add, i32, {groupX, groupYZAdd}));

    baseOffset = editor.InsertInstruction(
        f, prelimInst++,
        editor.CreateInstruction(Operation::Mul, i32,
                                 {flatIndex, editor.CreateConstant(payloadSize + 16)}));
  }

  size_t curBlock = 0;
  for(size_t i = 0; i < f->instructions.size(); i++)
  {
    const Instruction &inst = *f->instructions[i];
    if(inst.op == Operation::Branch || inst.op == Operation::Unreachable ||
       inst.op == Operation::Switch || inst.op == Operation::Ret)
    {
      curBlock++;
    }

    if(inst.op == Operation::Call && inst.getFuncCall()->name == dispatchMesh->name)
    {
      Instruction *threadIsZero = editor.InsertInstruction(
          f, i++, editor.CreateInstruction(Operation::IEqual, i1, {flatId, i32_0}));

      // we are currently in one block X that looks like:
      //
      //   ...X...
      //   ...X...
      //   ...X...
      //   ...X...
      //   dispatchMesh
      //   ret
      //
      // we want to split this into:
      //
      //   ...X...
      //   ...X...
      //   ...X...
      //   ...X...
      //   %a = cmp threadId
      //   br %a, block Y, block Z
      //
      // Y:
      //   <actual buffer writing here>
      //   br block Z
      //
      // Z:
      //   dispatchMesh
      //   ret
      //
      // so we create two new blocks (Y and Z) and insert them after the current block
      Block *trueBlock = editor.CreateBlock();
      Block *falseBlock = editor.CreateBlock();
      f->blocks.insert(curBlock + 1, trueBlock);
      f->blocks.insert(curBlock + 2, falseBlock);

      editor.InsertInstruction(f, i++,
                               editor.CreateInstruction(Operation::Branch, voidType,
                                                        {trueBlock, falseBlock, threadIsZero}));

      curBlock++;

      // true block

      editor.InsertInstruction(
          f, i++,
          editor.CreateInstruction(barrier, DXOp::Barrier,
                                   {
                                       // barrier & TGSM sync
                                       editor.CreateConstant(uint32_t(0x1 | 0x8)),
                                   }));

      // write the dimensions
      Instruction *xOffset = baseOffset;

      Constant *align = editor.CreateConstant((uint32_t)4U);

      editor.InsertInstruction(
          f, i++,
          editor.CreateInstruction(
              rawBufferStore, DXOp::RawBufferStore,
              {handle, xOffset, editor.CreateUndef(i32), inst.args[1], editor.CreateUndef(i32),
               editor.CreateUndef(i32), editor.CreateUndef(i32),
               editor.CreateConstant((uint8_t)0x1), align}));
      Instruction *yOffset = editor.InsertInstruction(
          f, i++,
          editor.CreateInstruction(Operation::Add, i32,
                                   {baseOffset, editor.CreateConstant((uint32_t)4U)}));

      editor.InsertInstruction(
          f, i++,
          editor.CreateInstruction(
              rawBufferStore, DXOp::RawBufferStore,
              {handle, yOffset, editor.CreateUndef(i32), inst.args[2], editor.CreateUndef(i32),
               editor.CreateUndef(i32), editor.CreateUndef(i32),
               editor.CreateConstant((uint8_t)0x1), align}));
      Instruction *zOffset = editor.InsertInstruction(
          f, i++,
          editor.CreateInstruction(Operation::Add, i32,
                                   {baseOffset, editor.CreateConstant((uint32_t)8U)}));

      editor.InsertInstruction(
          f, i++,
          editor.CreateInstruction(
              rawBufferStore, DXOp::RawBufferStore,
              {handle, zOffset, editor.CreateUndef(i32), inst.args[3], editor.CreateUndef(i32),
               editor.CreateUndef(i32), editor.CreateUndef(i32),
               editor.CreateConstant((uint8_t)0x1), align}));

      // write the payload contents
      uint32_t uavByteOffset = 16;
      for(uint32_t m = 0; m < payloadType->members.size(); m++)
      {
        PayloadBufferCopy(PayloadToBuffer, editor, f, i, baseOffset, handle, payloadType->members[m],
                          uavByteOffset, {payloadVariable, i32_0, editor.CreateConstant(m)});
      }

      editor.InsertInstruction(f, i++,
                               editor.CreateInstruction(Operation::Branch, voidType, {falseBlock}));

      curBlock++;

      // false/merge block

      // the dispatchMesh we found is here. Patch the dimensions arguments to be zero. Then we'll
      // proceed in the loop to look at the ret which doesn't need patched
      RDCASSERT(f->instructions[i] == &inst);
      f->instructions[i]->args[1] = i32_0;
      f->instructions[i]->args[2] = i32_0;
      f->instructions[i]->args[3] = i32_0;
    }
  }
}

static void ConvertToFixedDXILAmpFeeder(const DXBC::DXBCContainer *dxbc, uint32_t space,
                                        rdcfixedarray<uint32_t, 3> dispatchDim, bytebuf &editedBlob)
{
  using namespace DXIL;

  ProgramEditor editor(dxbc, editedBlob);
  bool isShaderModel6_6OrAbove =
      dxbc->m_Version.Major > 6 || (dxbc->m_Version.Major == 6 && dxbc->m_Version.Minor >= 6);

  const Type *i32 = editor.GetInt32Type();
  const Type *i8 = editor.GetInt8Type();
  const Type *i1 = editor.GetBoolType();
  const Type *voidType = editor.GetVoidType();

  const Type *handleType = editor.CreateNamedStructType(
      "dx.types.Handle", {editor.CreatePointerType(i8, Type::PointerAddrSpace::Default)});

  // this function is named differently based on the payload struct name, so search by prefix, we
  // expect the actual type to be the same as we're just modifying the payload in place
  const Function *dispatchMesh = editor.GetFunctionByPrefix("dx.op.dispatchMesh");

  const Function *createHandle = NULL;
  const Function *createHandleFromBinding = NULL;
  const Function *annotateHandle = NULL;

  // reading from a binding uses a different function in SM6.6+
  if(isShaderModel6_6OrAbove)
  {
    const Type *resBindType = editor.CreateNamedStructType("dx.types.ResBind", {i32, i32, i32, i8});
    createHandleFromBinding = editor.DeclareFunction("dx.op.createHandleFromBinding", handleType,
                                                     {i32, resBindType, i32, i1},
                                                     Attribute::NoUnwind | Attribute::ReadNone);

    const Type *resourcePropertiesType =
        editor.CreateNamedStructType("dx.types.ResourceProperties", {i32, i32});
    annotateHandle = editor.DeclareFunction("dx.op.annotateHandle", handleType,
                                            {i32, handleType, resourcePropertiesType},
                                            Attribute::NoUnwind | Attribute::ReadNone);
  }
  else if(!createHandle && !isShaderModel6_6OrAbove)
  {
    createHandle = editor.DeclareFunction("dx.op.createHandle", handleType, {i32, i8, i32, i32, i1},
                                          Attribute::NoUnwind | Attribute::ReadNone);
  }

  const Function *groupId = editor.DeclareFunction("dx.op.groupId.i32", i32, {i32, i32},
                                                   Attribute::NoUnwind | Attribute::ReadNone);
  const Type *resRet_i32 =
      editor.CreateNamedStructType("dx.types.ResRet.i32", {i32, i32, i32, i32, i32});
  const Function *rawBufferLoad = editor.DeclareFunction("dx.op.rawBufferLoad.i32", resRet_i32,
                                                         {i32, handleType, i32, i32, i8, i32},
                                                         Attribute::NoUnwind | Attribute::ReadOnly);

  // declare the resource, this happens purely in metadata but we need to store the slot
  uint32_t regSlot = 0;
  Metadata *reslist = NULL;
  {
    const Type *rw = editor.CreateNamedStructType("struct.RWByteAddressBuffer", {i32});
    const Type *rwptr = editor.CreatePointerType(rw, Type::PointerAddrSpace::Default);

    Metadata *resources = editor.CreateNamedMetadata("dx.resources");
    if(resources->children.empty())
      resources->children.push_back(editor.CreateMetadata());

    reslist = resources->children[0];

    if(reslist->children.empty())
      reslist->children.resize(4);

    Metadata *uavs = reslist->children[1];
    // if there isn't a UAV list, create an empty one so we can add our own
    if(!uavs)
      uavs = reslist->children[1] = editor.CreateMetadata();

    for(size_t i = 0; i < uavs->children.size(); i++)
    {
      // each UAV child should have a fixed format, [0] is the reg ID and I think this should always
      // be == the index
      const Metadata *uav = uavs->children[i];
      const Constant *slot = cast<Constant>(uav->children[(size_t)ResField::ID]->value);

      if(!slot)
      {
        RDCWARN("Unexpected non-constant slot ID in UAV");
        continue;
      }

      RDCASSERT(slot->getU32() == i);

      uint32_t id = slot->getU32();
      regSlot = RDCMAX(id + 1, regSlot);
    }

    Constant rwundef;
    rwundef.type = rwptr;
    rwundef.setUndef(true);

    // create the new UAV record
    Metadata *uav = editor.CreateMetadata();
    uav->children = {
        editor.CreateConstantMetadata(regSlot),
        editor.CreateConstantMetadata(editor.CreateConstant(rwundef)),
        editor.CreateConstantMetadata(""),
        editor.CreateConstantMetadata(space),
        editor.CreateConstantMetadata(1U),                                   // reg base
        editor.CreateConstantMetadata(1U),                                   // reg count
        editor.CreateConstantMetadata(uint32_t(ResourceKind::RawBuffer)),    // shape
        editor.CreateConstantMetadata(false),                                // globally coherent
        editor.CreateConstantMetadata(false),                                // hidden counter
        editor.CreateConstantMetadata(false),                                // raster order
        NULL,                                                                // UAV tags
    };

    uavs->children.push_back(uav);
  }

  uint32_t payloadSize = 0;

  rdcstr entryName;
  // add the entry point tags
  {
    Metadata *entryPoints = editor.GetMetadataByName("dx.entryPoints");

    if(!entryPoints)
    {
      RDCERR("Couldn't find entry point list");
      return;
    }

    // TODO select the entry point for multiple entry points? RT only for now
    Metadata *entry = entryPoints->children[0];

    entryName = entry->children[1]->str;

    Metadata *taglist = entry->children[4];
    if(!taglist)
      taglist = entry->children[4] = editor.CreateMetadata();

    // find existing shader flags tag, if there is one
    Metadata *shaderFlagsTag = NULL;
    Metadata *shaderFlagsData = NULL;
    Metadata *ampData = NULL;
    size_t flagsIndex = 0;
    for(size_t t = 0; taglist && t < taglist->children.size(); t += 2)
    {
      RDCASSERT(taglist->children[t]->isConstant);
      if(cast<Constant>(taglist->children[t]->value)->getU32() ==
         (uint32_t)ShaderEntryTag::ShaderFlags)
      {
        shaderFlagsTag = taglist->children[t];
        shaderFlagsData = taglist->children[t + 1];
        flagsIndex = t + 1;
      }
      else if(cast<Constant>(taglist->children[t]->value)->getU32() ==
              (uint32_t)ShaderEntryTag::Amplification)
      {
        ampData = taglist->children[t + 1];
      }
    }

    uint32_t shaderFlagsValue =
        shaderFlagsData ? cast<Constant>(shaderFlagsData->value)->getU32() : 0U;

    // raw and structured buffers
    shaderFlagsValue |= 0x10;

    // UAVs on non-PS/CS stages
    shaderFlagsValue |= 0x10000;

    // REMOVE wave ops flag as we don't use it but the original shader might have. DXIL requires
    // flags to be strictly minimum :(
    shaderFlagsValue &= ~0x80000;

    // (re-)create shader flags tag
    Type *i64 = editor.CreateScalarType(Type::Int, 64);
    shaderFlagsData =
        editor.CreateConstantMetadata(editor.CreateConstant(Constant(i64, shaderFlagsValue)));
    // shaderFlagsData = editor.CreateConstantMetadata(shaderFlagsValue);

    // if we didn't have a shader tags entry at all, create the metadata node for the shader flags
    // tag
    if(!shaderFlagsTag)
      shaderFlagsTag = editor.CreateConstantMetadata((uint32_t)ShaderEntryTag::ShaderFlags);

    // if we had a tag already, we can just re-use that tag node and replace the data node.
    // Otherwise we need to add both, and we insert them first
    if(flagsIndex)
    {
      taglist->children[flagsIndex] = shaderFlagsData;
    }
    else
    {
      taglist->children.insert(0, shaderFlagsTag);
      taglist->children.insert(1, shaderFlagsData);
    }

    // set reslist and taglist in case they were null before
    entry->children[3] = reslist;
    entry->children[4] = taglist;

    // we must have found an amplification tag. Patch the number of threads and payload size here
    ampData->children[0] = editor.CreateMetadata();
    ampData->children[0]->children.push_back(editor.CreateConstantMetadata((uint32_t)1));
    ampData->children[0]->children.push_back(editor.CreateConstantMetadata((uint32_t)1));
    ampData->children[0]->children.push_back(editor.CreateConstantMetadata((uint32_t)1));

    payloadSize = cast<Constant>(ampData->children[1]->value)->getU32();
    // add room for our dimensions + offset
    ampData->children[1] = editor.CreateConstantMetadata(payloadSize + 16);
  }

  // get the editor to patch PSV0 with our extra UAV
  editor.RegisterUAV(DXILResourceType::ByteAddressUAV, space, 1, 1, ResourceKind::RawBuffer);
  uint32_t dim[] = {1, 1, 1};
  editor.SetNumThreads(dim);
  editor.SetASPayloadSize(payloadSize + 16);

  // remove some flags that will no longer be valid
  editor.PatchGlobalShaderFlags(
      [](DXBC::GlobalShaderFlags &flags) { flags &= ~DXBC::GlobalShaderFlags::WaveOps; });

  Function *f = editor.GetFunctionByName(entryName);

  if(!f)
  {
    RDCERR("Couldn't find entry point function '%s'", entryName.c_str());
    return;
  }

  // find the dispatchMesh call, and from there the global groupshared variable that's the payload
  GlobalVar *payloadVariable = NULL;
  Type *payloadType = NULL;
  for(size_t i = 0; i < f->instructions.size(); i++)
  {
    const Instruction &inst = *f->instructions[i];

    if(inst.op == Operation::Call && inst.getFuncCall()->name == dispatchMesh->name)
    {
      if(inst.args.size() != 5)
      {
        RDCERR("Unexpected number of arguments to dispatchMesh");
        continue;
      }
      payloadVariable = cast<GlobalVar>(inst.args[4]);
      if(!payloadVariable)
      {
        RDCERR("Unexpected non-variable payload argument to dispatchMesh");
        continue;
      }

      payloadType = (Type *)payloadVariable->type;

      RDCASSERT(payloadType->type == Type::Pointer);
      payloadType = (Type *)payloadType->inner;

      break;
    }
  }

  // add the dimensions and offset to the payload type, at the end so we don't have to patch any
  // GEPs in future. We'll swizzle these to the start when copying to/from buffers still
  RDCASSERT(payloadType && payloadType->type == Type::Struct);
  payloadType->members.append({i32, i32, i32, i32});

  // recreate the function with our own instructions
  f->instructions.clear();
  f->blocks.resize(1);

  // create our handle first thing
  Constant *annotateConstant = NULL;
  Instruction *handle = NULL;
  if(createHandle)
  {
    RDCASSERT(!isShaderModel6_6OrAbove);
    handle = editor.AddInstruction(
        f, editor.CreateInstruction(createHandle, DXOp::CreateHandle,
                                    {
                                        // kind = UAV
                                        editor.CreateConstant((uint8_t)HandleKind::UAV),
                                        // ID/slot
                                        editor.CreateConstant(regSlot),
                                        // register
                                        editor.CreateConstant(1U),
                                        // non-uniform
                                        editor.CreateConstant(false),
                                    }));
  }
  else if(createHandleFromBinding)
  {
    RDCASSERT(isShaderModel6_6OrAbove);
    const Type *resBindType = editor.CreateNamedStructType("dx.types.ResBind", {});
    Constant *resBindConstant =
        editor.CreateConstant(resBindType, {
                                               // Lower id bound
                                               editor.CreateConstant(1U),
                                               // Upper id bound
                                               editor.CreateConstant(1U),
                                               // Space ID
                                               editor.CreateConstant(space),
                                               // kind = UAV
                                               editor.CreateConstant((uint8_t)HandleKind::UAV),
                                           });

    Instruction *unannotatedHandle = editor.AddInstruction(
        f, editor.CreateInstruction(createHandleFromBinding, DXOp::CreateHandleFromBinding,
                                    {
                                        // resBind
                                        resBindConstant,
                                        // ID/slot
                                        editor.CreateConstant(1U),
                                        // non-uniform
                                        editor.CreateConstant(false),
                                    }));

    annotateConstant = editor.CreateConstant(
        editor.CreateNamedStructType("dx.types.ResourceProperties", {}),
        {
            // IsUav : (1 << 12)
            editor.CreateConstant(uint32_t((1 << 12) | (uint32_t)ResourceKind::RawBuffer)),
            //
            editor.CreateConstant(0U),
        });

    handle = editor.AddInstruction(f, editor.CreateInstruction(annotateHandle, DXOp::AnnotateHandle,
                                                               {
                                                                   // Resource handle
                                                                   unannotatedHandle,
                                                                   // Resource properties
                                                                   annotateConstant,
                                                               }));
  }

  RDCASSERT(handle);

  Constant *i32_0 = editor.CreateConstant(0U);
  Constant *i32_1 = editor.CreateConstant(1U);
  Constant *i32_2 = editor.CreateConstant(2U);
  Constant *i32_4 = editor.CreateConstant(4U);

  // get our output location from group ID
  Instruction *groupX =
      editor.AddInstruction(f, editor.CreateInstruction(groupId, DXOp::GroupId, {i32_0}));
  Instruction *groupY =
      editor.AddInstruction(f, editor.CreateInstruction(groupId, DXOp::GroupId, {i32_1}));
  Instruction *groupZ =
      editor.AddInstruction(f, editor.CreateInstruction(groupId, DXOp::GroupId, {i32_2}));

  // linearise it based on the number of dispatches
  Instruction *groupYMul = editor.AddInstruction(
      f, editor.CreateInstruction(Operation::Mul, i32,
                                  {groupY, editor.CreateConstant(dispatchDim[0])}));
  Instruction *groupZMul = editor.AddInstruction(
      f, editor.CreateInstruction(Operation::Mul, i32,
                                  {groupZ, editor.CreateConstant(dispatchDim[0] * dispatchDim[1])}));
  Instruction *groupYZAdd = editor.AddInstruction(
      f, editor.CreateInstruction(Operation::Add, i32, {groupYMul, groupZMul}));
  Instruction *flatIndex =
      editor.AddInstruction(f, editor.CreateInstruction(Operation::Add, i32, {groupX, groupYZAdd}));

  Instruction *baseOffset = editor.AddInstruction(
      f, editor.CreateInstruction(Operation::Mul, i32,
                                  {flatIndex, editor.CreateConstant(payloadSize + 16)}));

  Instruction *dimAndOffset = editor.AddInstruction(
      f, editor.CreateInstruction(rawBufferLoad, DXOp::RawBufferLoad,
                                  {handle, baseOffset, editor.CreateUndef(i32),
                                   editor.CreateConstant((uint8_t)0xf), i32_4}));

  Instruction *dimX =
      editor.AddInstruction(f, editor.CreateInstruction(Operation::ExtractVal, i32,
                                                        {dimAndOffset, editor.CreateLiteral(0)}));
  Instruction *dimY =
      editor.AddInstruction(f, editor.CreateInstruction(Operation::ExtractVal, i32,
                                                        {dimAndOffset, editor.CreateLiteral(1)}));
  Instruction *dimZ =
      editor.AddInstruction(f, editor.CreateInstruction(Operation::ExtractVal, i32,
                                                        {dimAndOffset, editor.CreateLiteral(2)}));
  Instruction *offset =
      editor.AddInstruction(f, editor.CreateInstruction(Operation::ExtractVal, i32,
                                                        {dimAndOffset, editor.CreateLiteral(3)}));

  size_t curInst = f->instructions.size();
  // start at 16 bytes, to account for our own data
  uint32_t uavByteOffset = 16;
  for(uint32_t i = 0; i < payloadType->members.size() - 4; i++)
  {
    PayloadBufferCopy(BufferToPayload, editor, f, curInst, baseOffset, handle,
                      payloadType->members[i], uavByteOffset,
                      {payloadVariable, i32_0, editor.CreateConstant(i)});
  }

  for(size_t i = 0; i < 4; i++)
  {
    Value *srcs[] = {dimX, dimY, dimZ, offset};

    Constant *dst = editor.CreateConstantGEP(
        editor.GetPointerType(i32, payloadVariable->type->addrSpace),
        {payloadVariable, i32_0,
         editor.CreateConstant(uint32_t(payloadType->members.size() - 4 + i))});

    DXIL::Instruction *store = editor.CreateInstruction(Operation::Store);

    store->type = voidType;
    store->op = Operation::Store;
    store->align = 4;
    store->args = {dst, srcs[i]};

    editor.AddInstruction(f, store);
  }

  editor.AddInstruction(f, editor.CreateInstruction(dispatchMesh, DXOp::DispatchMesh,
                                                    {dimX, dimY, dimZ, payloadVariable}));
  editor.AddInstruction(f, editor.CreateInstruction(Operation::Ret, voidType, {}));
}

static void AddDXILMeshShaderOutputStores(uint32_t ampPayloadSize, const DXBC::DXBCContainer *dxbc,
                                          uint32_t space, bool readAmpOffset,
                                          rdcfixedarray<uint32_t, 3> dispatchDim,
                                          OutDXILMeshletLayout &layout, bytebuf &editedBlob)
{
  using namespace DXIL;

  ProgramEditor editor(dxbc, editedBlob);

  bool isShaderModel6_6OrAbove =
      dxbc->m_Version.Major > 6 || (dxbc->m_Version.Major == 6 && dxbc->m_Version.Minor >= 6);

  const Type *i32 = editor.GetInt32Type();
  const Type *i8 = editor.GetInt8Type();
  const Type *i1 = editor.GetBoolType();
  const Type *voidType = editor.GetVoidType();

  const Type *handleType = editor.CreateNamedStructType(
      "dx.types.Handle", {editor.CreatePointerType(i8, Type::PointerAddrSpace::Default)});

  const Function *createHandle = NULL;
  const Function *createHandleFromBinding = NULL;
  const Function *annotateHandle = NULL;

  // reading from a binding uses a different function in SM6.6+
  if(isShaderModel6_6OrAbove)
  {
    const Type *resBindType = editor.CreateNamedStructType("dx.types.ResBind", {i32, i32, i32, i8});
    createHandleFromBinding = editor.DeclareFunction("dx.op.createHandleFromBinding", handleType,
                                                     {i32, resBindType, i32, i1},
                                                     Attribute::NoUnwind | Attribute::ReadNone);

    const Type *resourcePropertiesType =
        editor.CreateNamedStructType("dx.types.ResourceProperties", {i32, i32});
    annotateHandle = editor.DeclareFunction("dx.op.annotateHandle", handleType,
                                            {i32, handleType, resourcePropertiesType},
                                            Attribute::NoUnwind | Attribute::ReadNone);
  }
  else if(!createHandle && !isShaderModel6_6OrAbove)
  {
    createHandle = editor.DeclareFunction("dx.op.createHandle", handleType, {i32, i8, i32, i32, i1},
                                          Attribute::NoUnwind | Attribute::ReadOnly);
  }

  const Function *flattenedThreadIdInGroup = editor.DeclareFunction(
      "dx.op.flattenedThreadIdInGroup.i32", i32, {i32}, Attribute::NoUnwind | Attribute::ReadNone);
  const Function *groupId = editor.DeclareFunction("dx.op.groupId.i32", i32, {i32, i32},
                                                   Attribute::NoUnwind | Attribute::ReadNone);

  const Function *getMeshPayload = editor.GetFunctionByPrefix("dx.op.getMeshPayload");

  const Function *setMeshOutputCounts = editor.DeclareFunction(
      "dx.op.setMeshOutputCounts", voidType, {i32, i32, i32}, Attribute::NoUnwind);
  const Function *emitIndices = editor.DeclareFunction(
      "dx.op.emitIndices", voidType, {i32, i32, i32, i32, i32}, Attribute::NoUnwind);

  // declare the resource, this happens purely in metadata but we need to store the slot
  uint32_t regSlot = 0;
  Metadata *reslist = NULL;
  {
    const Type *rw = editor.CreateNamedStructType("struct.RWByteAddressBuffer", {i32});
    const Type *rwptr = editor.CreatePointerType(rw, Type::PointerAddrSpace::Default);

    Metadata *resources = editor.CreateNamedMetadata("dx.resources");
    if(resources->children.empty())
      resources->children.push_back(editor.CreateMetadata());

    reslist = resources->children[0];

    if(reslist->children.empty())
      reslist->children.resize(4);

    Metadata *uavs = reslist->children[1];
    // if there isn't a UAV list, create an empty one so we can add our own
    if(!uavs)
      uavs = reslist->children[1] = editor.CreateMetadata();

    for(size_t i = 0; i < uavs->children.size(); i++)
    {
      // each UAV child should have a fixed format, [0] is the reg ID and I think this should always
      // be == the index
      const Metadata *uav = uavs->children[i];
      const Constant *slot = cast<Constant>(uav->children[(size_t)ResField::ID]->value);

      if(!slot)
      {
        RDCWARN("Unexpected non-constant slot ID in UAV");
        continue;
      }

      RDCASSERT(slot->getU32() == i);

      uint32_t id = slot->getU32();
      regSlot = RDCMAX(id + 1, regSlot);
    }

    Constant rwundef;
    rwundef.type = rwptr;
    rwundef.setUndef(true);

    // create the new UAV record
    Metadata *uav = editor.CreateMetadata();
    uav->children = {
        editor.CreateConstantMetadata(regSlot),
        editor.CreateConstantMetadata(editor.CreateConstant(rwundef)),
        editor.CreateConstantMetadata(""),
        editor.CreateConstantMetadata(space),
        editor.CreateConstantMetadata(0U),                                   // reg base
        editor.CreateConstantMetadata(1U),                                   // reg count
        editor.CreateConstantMetadata(uint32_t(ResourceKind::RawBuffer)),    // shape
        editor.CreateConstantMetadata(false),                                // globally coherent
        editor.CreateConstantMetadata(false),                                // hidden counter
        editor.CreateConstantMetadata(false),                                // raster order
        NULL,                                                                // UAV tags
    };

    uavs->children.push_back(uav);
  }

  rdcstr entryName;

  // add the entry point tags
  bool hadPayload = false;

  Metadata *outSig = NULL, *primOutSig = NULL;
  {
    Metadata *entryPoints = editor.GetMetadataByName("dx.entryPoints");

    if(!entryPoints)
    {
      RDCERR("Couldn't find entry point list");
      return;
    }

    // TODO select the entry point for multiple entry points? RT only for now
    Metadata *entry = entryPoints->children[0];

    entryName = entry->children[1]->str;

    Metadata *taglist = entry->children[4];
    if(!taglist)
      taglist = entry->children[4] = editor.CreateMetadata();

    Metadata *sigs = entry->children[2];
    outSig = sigs->children[1];
    primOutSig = sigs->children[2];

    // find existing shader flags tag, if there is one
    Metadata *shaderFlagsTag = NULL;
    Metadata *shaderFlagsData = NULL;
    Metadata *meshData = NULL;
    size_t flagsIndex = 0;
    for(size_t t = 0; taglist && t < taglist->children.size(); t += 2)
    {
      RDCASSERT(taglist->children[t]->isConstant);
      if(cast<Constant>(taglist->children[t]->value)->getU32() ==
         (uint32_t)ShaderEntryTag::ShaderFlags)
      {
        shaderFlagsTag = taglist->children[t];
        shaderFlagsData = taglist->children[t + 1];
        flagsIndex = t + 1;
      }
      else if(cast<Constant>(taglist->children[t]->value)->getU32() == (uint32_t)ShaderEntryTag::Mesh)
      {
        meshData = taglist->children[t + 1];
      }
    }

    uint32_t shaderFlagsValue =
        shaderFlagsData ? cast<Constant>(shaderFlagsData->value)->getU32() : 0U;

    // raw and structured buffers
    shaderFlagsValue |= 0x10;

    // UAVs on non-PS/CS stages
    shaderFlagsValue |= 0x10000;

    // (re-)create shader flags tag
    Type *i64 = editor.CreateScalarType(Type::Int, 64);
    shaderFlagsData =
        editor.CreateConstantMetadata(editor.CreateConstant(Constant(i64, shaderFlagsValue)));

    // if we didn't have a shader tags entry at all, create the metadata node for the shader flags
    // tag
    if(!shaderFlagsTag)
      shaderFlagsTag = editor.CreateConstantMetadata((uint32_t)ShaderEntryTag::ShaderFlags);

    // if we had a tag already, we can just re-use that tag node and replace the data node.
    // Otherwise we need to add both, and we insert them first
    if(flagsIndex)
    {
      taglist->children[flagsIndex] = shaderFlagsData;
    }
    else
    {
      taglist->children.insert(0, shaderFlagsTag);
      taglist->children.insert(1, shaderFlagsData);
    }

    // set reslist and taglist in case they were null before
    entry->children[3] = reslist;
    entry->children[4] = taglist;

    // patch payload size in mesh tags if we're reading from amplification shader
    if(readAmpOffset)
    {
      uint32_t payloadSize = cast<Constant>(meshData->children[4]->value)->getU32();
      // DXIL payload can't be empty, so if the previous size was non-zero we had one previously
      hadPayload = payloadSize != 0;

      // if the amplification shader declares a payload, but mesh shader doesn't, we need to be sure
      // we match them in size for validation
      if(!hadPayload && ampPayloadSize != 0)
        payloadSize = ampPayloadSize;

      // if the mesh shader did have a payload, these sizes should match!
      RDCASSERTEQUAL(payloadSize, ampPayloadSize);

      payloadSize += 16;
      meshData->children[4] = editor.CreateConstantMetadata(payloadSize);
      editor.SetMSPayloadSize(payloadSize);
    }

    // if the topology (child [3]) is 1, then it's lines, otherwise triangles
    layout.indexCountPerPrim = cast<Constant>(meshData->children[3]->value)->getU32() == 1 ? 2 : 3;

    layout.vertArrayLength = cast<Constant>(meshData->children[1]->value)->getU32();
    layout.primArrayLength = cast<Constant>(meshData->children[2]->value)->getU32();
  }

  // get the editor to patch PSV0 with our extra UAV
  editor.RegisterUAV(DXILResourceType::ByteAddressUAV, space, 0, 0, ResourceKind::RawBuffer);

  Function *f = editor.GetFunctionByName(entryName);

  if(!f)
  {
    RDCERR("Couldn't find entry point function '%s'", entryName.c_str());
    return;
  }

  Type *payloadType = NULL;
  if(hadPayload)
  {
    if(getMeshPayload)
    {
      // if we had a payload and it was loaded, seek the dx.op.getMeshPayload to find its type
      for(size_t i = 0; i < f->instructions.size(); i++)
      {
        const Instruction &inst = *f->instructions[i];

        if(inst.op == Operation::Call && inst.getFuncCall()->name == getMeshPayload->name)
        {
          payloadType = (Type *)inst.type;

          RDCASSERT(payloadType->type == Type::Pointer);
          payloadType = (Type *)payloadType->inner;

          payloadType->members.append({i32, i32, i32, i32});

          break;
        }
      }
    }
    else
    {
      // if we had a payload declared but it wasn't ever fetched, there will be no function or type.
      // We create a synthetic type of the right size then patch it

      rdcarray<const Type *> members;
      for(uint32_t i = 0; i < ampPayloadSize / sizeof(uint32_t); i++)
        members.push_back(i32);

      // unclear if HLSL allows non-4byte aligned types
      RDCASSERT((ampPayloadSize % sizeof(uint32_t)) == 0);

      members.append({i32, i32, i32, i32});

      // no payload before. We get to make up our own!
      payloadType = editor.CreateNamedStructType("struct.payload_t", members);

      const Type *payloadPtrType =
          editor.CreatePointerType(payloadType, Type::PointerAddrSpace::Default);

      getMeshPayload = editor.DeclareFunction("dx.op.getMeshPayload.struct.payload_t", payloadPtrType,
                                              {i32}, Attribute::NoUnwind | Attribute::ReadOnly);
    }
  }
  else if(readAmpOffset)
  {
    // no payload before. We get to make up our own!
    payloadType = editor.CreateNamedStructType("struct.payload_t", {i32, i32, i32, i32});

    const Type *payloadPtrType =
        editor.CreatePointerType(payloadType, Type::PointerAddrSpace::Default);

    getMeshPayload = editor.DeclareFunction("dx.op.getMeshPayload.struct.payload_t", payloadPtrType,
                                            {i32}, Attribute::NoUnwind | Attribute::ReadOnly);
  }

  if(readAmpOffset)
  {
    RDCASSERT(payloadType && payloadType->type == Type::Struct);
  }

  uint32_t byteCounter = 0;

  layout.sigLocations.resize((outSig ? outSig->children.size() : 0) +
                             (primOutSig ? primOutSig->children.size() : 0));
  size_t firstPrimOutput = (outSig ? outSig->children.size() : 0);

  for(size_t i = 0; outSig && i < outSig->children.size(); i++)
  {
    OutDXILSigLocation &loc = layout.sigLocations[i];

    Metadata *sigMeta = outSig->children[i];

    uint32_t semantic = cast<Constant>(sigMeta->children[3]->value)->getU32();

    loc.offset = byteCounter;

    VarType type =
        VarTypeForComponentType((ComponentType)cast<Constant>(sigMeta->children[2]->value)->getU32());

    loc.scalarElemSize = VarTypeByteSize(type);
    loc.rowCount = cast<Constant>(sigMeta->children[6]->value)->getU32();
    loc.colCount = cast<Constant>(sigMeta->children[7]->value)->getU32();

    // move position to the front when storing, if semantic 3 (position, guaranteed to be per-vertex
    // by definition) isn't at index 0, we shuffle up everything we've added so far by 16 bytes and
    // add position here regardless of byte offset.
    if(semantic == 3 && i != 0)
    {
      RDCASSERT(loc.scalarElemSize * loc.rowCount * loc.colCount == sizeof(Vec4f),
                loc.scalarElemSize, loc.rowCount, loc.colCount);

      // shift all previous signatures up
      for(size_t prev = 0; prev < i; prev++)
        layout.sigLocations[prev].offset += sizeof(Vec4f);

      loc.offset = 0;
    }

    byteCounter += loc.scalarElemSize * loc.rowCount * loc.colCount;
  }

  layout.vertStride = AlignUp4(byteCounter);
  byteCounter = 0;

  // per primitive outputs are after output signature outputs
  for(size_t i = 0; primOutSig && i < primOutSig->children.size(); i++)
  {
    OutDXILSigLocation &loc = layout.sigLocations[firstPrimOutput + i];

    Metadata *sigMeta = primOutSig->children[i];

    loc.offset = byteCounter;

    VarType type =
        VarTypeForComponentType((ComponentType)cast<Constant>(sigMeta->children[2]->value)->getU32());

    loc.scalarElemSize = VarTypeByteSize(type);
    loc.rowCount = cast<Constant>(sigMeta->children[6]->value)->getU32();
    loc.colCount = cast<Constant>(sigMeta->children[7]->value)->getU32();

    byteCounter += loc.scalarElemSize * loc.rowCount * loc.colCount;
  }

  layout.primStride = AlignUp4(byteCounter);

  for(size_t i = 0; i < layout.sigLocations.size(); i++)
  {
    // prim/vert counts
    layout.sigLocations[i].offset += 32;

    // indices
    layout.sigLocations[i].offset +=
        AlignUp16(layout.primArrayLength * layout.indexCountPerPrim * (uint32_t)sizeof(uint32_t));

    if(i >= firstPrimOutput)
      layout.sigLocations[i].offset += layout.vertArrayLength * layout.vertStride;
  }

  // meshlet data begins with real and fake meshlet size (prim/vert count)
  layout.meshletByteSize = 32;
  const uint32_t idxDataOffset = layout.meshletByteSize;

  // then comes indices
  layout.meshletByteSize +=
      (uint32_t)AlignUp16(layout.primArrayLength * layout.indexCountPerPrim * sizeof(uint32_t));

  // after that per-vertex data
  layout.meshletByteSize += layout.vertArrayLength * layout.vertStride;

  // and finally per-primitive data
  layout.meshletByteSize += layout.primArrayLength * layout.primStride;

  // create our handle first thing
  Constant *annotateConstant = NULL;
  Instruction *handle = NULL;
  size_t prelimInst = 0;
  if(createHandle)
  {
    RDCASSERT(!isShaderModel6_6OrAbove);
    handle = editor.InsertInstruction(
        f, prelimInst++,
        editor.CreateInstruction(createHandle, DXOp::CreateHandle,
                                 {
                                     // kind = UAV
                                     editor.CreateConstant((uint8_t)HandleKind::UAV),
                                     // ID/slot
                                     editor.CreateConstant(regSlot),
                                     // register
                                     editor.CreateConstant(0U),
                                     // non-uniform
                                     editor.CreateConstant(false),
                                 }));
  }
  else if(createHandleFromBinding)
  {
    RDCASSERT(isShaderModel6_6OrAbove);
    const Type *resBindType = editor.CreateNamedStructType("dx.types.ResBind", {});
    Constant *resBindConstant =
        editor.CreateConstant(resBindType, {
                                               // Lower id bound
                                               editor.CreateConstant(0U),
                                               // Upper id bound
                                               editor.CreateConstant(0U),
                                               // Space ID
                                               editor.CreateConstant(space),
                                               // kind = UAV
                                               editor.CreateConstant((uint8_t)HandleKind::UAV),
                                           });

    Instruction *unannotatedHandle = editor.InsertInstruction(
        f, prelimInst++,
        editor.CreateInstruction(createHandleFromBinding, DXOp::CreateHandleFromBinding,
                                 {
                                     // resBind
                                     resBindConstant,
                                     // ID/slot
                                     editor.CreateConstant(0U),
                                     // non-uniform
                                     editor.CreateConstant(false),
                                 }));

    annotateConstant = editor.CreateConstant(
        editor.CreateNamedStructType("dx.types.ResourceProperties", {}),
        {
            // IsUav : (1 << 12)
            editor.CreateConstant(uint32_t((1 << 12) | (uint32_t)ResourceKind::RawBuffer)),
            //
            editor.CreateConstant(0U),
        });

    handle = editor.InsertInstruction(f, prelimInst++,
                                      editor.CreateInstruction(annotateHandle, DXOp::AnnotateHandle,
                                                               {
                                                                   // Resource handle
                                                                   unannotatedHandle,
                                                                   // Resource properties
                                                                   annotateConstant,
                                                               }));
  }

  RDCASSERT(handle);

  // now calculate our offset
  Constant *i32_0 = editor.CreateConstant(0U);
  Constant *i32_1 = editor.CreateConstant(1U);
  Constant *i32_2 = editor.CreateConstant(2U);
  Constant *i32_4 = editor.CreateConstant(4U);

  Instruction *baseOffset = NULL;

  Instruction *groupX = NULL, *groupY = NULL, *groupZ = NULL;

  {
    // get our output location from group ID
    groupX = editor.InsertInstruction(f, prelimInst++,
                                      editor.CreateInstruction(groupId, DXOp::GroupId, {i32_0}));
    groupY = editor.InsertInstruction(f, prelimInst++,
                                      editor.CreateInstruction(groupId, DXOp::GroupId, {i32_1}));
    groupZ = editor.InsertInstruction(f, prelimInst++,
                                      editor.CreateInstruction(groupId, DXOp::GroupId, {i32_2}));
  }

  // get the flat thread ID for comparisons
  Instruction *flatId = editor.InsertInstruction(
      f, prelimInst++,
      editor.CreateInstruction(flattenedThreadIdInGroup, DXOp::FlattenedThreadIdInGroup, {}));

  Value *dimX = NULL, *dimY = NULL;
  Instruction *dispatchBaseMeshletIdx = NULL;

  if(readAmpOffset)
  {
    // reading the payload has no dependencies but can only happen once per shader. If there was a
    // load before we search for it and bring it to the front here so we can use it ourselves. The
    // llvm value-referencing will continue to work as normal since the pointer remains the same
    Instruction *payloadLoad = NULL;
    for(size_t i = 0; i < f->instructions.size(); i++)
    {
      const Instruction &inst = *f->instructions[i];
      if(inst.op == Operation::Call && inst.getFuncCall()->name == getMeshPayload->name)
      {
        payloadLoad = editor.InsertInstruction(f, prelimInst++, f->instructions.takeAt(i));
        break;
      }
    }

    // if there wasn't one before (because we added the payload, or it was unused) we can just add our own
    if(!payloadLoad)
      payloadLoad = editor.InsertInstruction(
          f, prelimInst++, editor.CreateInstruction(getMeshPayload, DXOp::GetMeshPayload, {}));

    Type *i32ptr = editor.CreatePointerType(i32, Type::PointerAddrSpace::Default);

    // .x = x dimension
    Instruction *dimXPtr = editor.InsertInstruction(
        f, prelimInst++,
        editor.CreateInstruction(
            Operation::GetElementPtr, i32ptr,
            {payloadLoad, i32_0, editor.CreateConstant(uint32_t(payloadType->members.size() - 4))}));
    // .y = y dimension
    Instruction *dimYPtr = editor.InsertInstruction(
        f, prelimInst++,
        editor.CreateInstruction(
            Operation::GetElementPtr, i32ptr,
            {payloadLoad, i32_0, editor.CreateConstant(uint32_t(payloadType->members.size() - 3))}));
    // .w = offset for this set of mesh groups
    Instruction *offsetPtr = editor.InsertInstruction(
        f, prelimInst++,
        editor.CreateInstruction(
            Operation::GetElementPtr, i32ptr,
            {payloadLoad, i32_0, editor.CreateConstant(uint32_t(payloadType->members.size() - 1))}));

    Instruction *dimXLoad = editor.InsertInstruction(
        f, prelimInst++, editor.CreateInstruction(Operation::Load, i32, {dimXPtr}));
    dimXLoad->align = 4;
    dimX = dimXLoad;

    Instruction *dimYLoad = editor.InsertInstruction(
        f, prelimInst++, editor.CreateInstruction(Operation::Load, i32, {dimYPtr}));
    dimYLoad->align = 4;
    dimY = dimYLoad;

    dispatchBaseMeshletIdx = editor.InsertInstruction(
        f, prelimInst++, editor.CreateInstruction(Operation::Load, i32, {offsetPtr}));
    dispatchBaseMeshletIdx->align = 4;
  }
  else
  {
    dimX = editor.CreateConstant(dispatchDim[0]);
    dimY = editor.CreateConstant(dispatchDim[1]);
  }

  {
    Instruction *dimXY = editor.InsertInstruction(
        f, prelimInst++, editor.CreateInstruction(Operation::Mul, i32, {dimX, dimY}));

    // linearise to slot based on the number of dispatches
    Instruction *groupYMul = editor.InsertInstruction(
        f, prelimInst++, editor.CreateInstruction(Operation::Mul, i32, {groupY, dimX}));
    Instruction *groupZMul = editor.InsertInstruction(
        f, prelimInst++, editor.CreateInstruction(Operation::Mul, i32, {groupZ, dimXY}));
    Instruction *groupYZAdd = editor.InsertInstruction(
        f, prelimInst++, editor.CreateInstruction(Operation::Add, i32, {groupYMul, groupZMul}));
    Instruction *flatIndex = editor.InsertInstruction(
        f, prelimInst++, editor.CreateInstruction(Operation::Add, i32, {groupX, groupYZAdd}));

    if(dispatchBaseMeshletIdx)
    {
      flatIndex = editor.InsertInstruction(
          f, prelimInst++,
          editor.CreateInstruction(Operation::Add, i32, {flatIndex, dispatchBaseMeshletIdx}));
    }

    baseOffset = editor.InsertInstruction(
        f, prelimInst++,
        editor.CreateInstruction(Operation::Mul, i32,
                                 {flatIndex, editor.CreateConstant(layout.meshletByteSize)}));
  }

  Constant *threadZeroCountOffset = i32_0;
  Constant *threadOtherCountOffset = editor.CreateConstant(uint32_t(16U));

  Constant *indexStride =
      editor.CreateConstant(uint32_t(layout.indexCountPerPrim * sizeof(uint32_t)));

  for(size_t i = 0; i < f->instructions.size(); i++)
  {
    const Instruction &inst = *f->instructions[i];
    if(inst.op == Operation::Call && inst.getFuncCall()->name == setMeshOutputCounts->name)
    {
      Instruction *threadIsZero = editor.InsertInstruction(
          f, i++, editor.CreateInstruction(Operation::IEqual, i1, {flatId, i32_0}));

      // to avoid messing up phi nodes in the application where this is called, we do this
      // branchless by either writing to offset 0 (for threadIndex == 0) or offset 16 (for
      // threadIndex > 0). Then we can ignore the second one
      Instruction *byteOffset = editor.InsertInstruction(
          f, i++,
          editor.CreateInstruction(Operation::Select, i32,
                                   {threadZeroCountOffset, threadOtherCountOffset, threadIsZero}));

      Instruction *writeOffset = editor.InsertInstruction(
          f, i++, editor.CreateInstruction(Operation::Add, i32, {baseOffset, byteOffset}));

      const Function *rawBufferStore = editor.DeclareFunction(
          "dx.op.rawBufferStore.i32", voidType,
          {i32, handleType, i32, i32, i32, i32, i32, i32, i8, i32}, Attribute::NoUnwind);

      editor.InsertInstruction(
          f, i++,
          editor.CreateInstruction(
              rawBufferStore, DXOp::RawBufferStore,
              {handle, writeOffset, editor.CreateUndef(i32), inst.args[1], editor.CreateUndef(i32),
               editor.CreateUndef(i32), editor.CreateUndef(i32),
               editor.CreateConstant((uint8_t)0x1), i32_4}));

      writeOffset = editor.InsertInstruction(
          f, i++, editor.CreateInstruction(Operation::Add, i32, {writeOffset, i32_4}));

      editor.InsertInstruction(
          f, i++,
          editor.CreateInstruction(
              rawBufferStore, DXOp::RawBufferStore,
              {handle, writeOffset, editor.CreateUndef(i32), inst.args[2], editor.CreateUndef(i32),
               editor.CreateUndef(i32), editor.CreateUndef(i32),
               editor.CreateConstant((uint8_t)0x1), i32_4}));

      // disable the actual output
      f->instructions[i]->args[1] = i32_0;
      f->instructions[i]->args[2] = i32_0;
    }
    else if(inst.op == Operation::Call &&
            inst.getFuncCall()->name.beginsWith("dx.op.storeVertexOutput"))
    {
      uint32_t sigId = cast<Constant>(inst.args[1])->getU32();
      Value *row = inst.args[2];
      Value *col = inst.args[3];
      Value *value = inst.args[4];
      Value *vert = inst.args[5];

      OutDXILSigLocation &loc = layout.sigLocations[sigId];

      Instruction *colByteOffset = NULL;

      // col is i8, but DXIL doesn't support i8 as values (sigh...). So if that value is a constant
      // (currently must be true) then we re-create it as u32. We handle the case where it's not a
      // constant in future perhaps
      Constant *colConst = cast<Constant>(col);
      if(colConst)
      {
        colByteOffset = editor.InsertInstruction(
            f, i++,
            editor.CreateInstruction(Operation::Mul, i32,
                                     {editor.CreateConstant(colConst->getU32()),
                                      editor.CreateConstant(loc.scalarElemSize)}));
      }
      else
      {
        colByteOffset = editor.InsertInstruction(
            f, i++,
            editor.CreateInstruction(Operation::Mul, i8,
                                     {col, editor.CreateConstant(uint8_t(loc.scalarElemSize))}));

        colByteOffset =
            editor.InsertInstruction(f, i++, editor.CreateInstruction(Operation::ZExt, i32, {col}));
      }

      Instruction *elemByteOffset = colByteOffset;

      if(loc.rowCount > 1)
      {
        uint32_t rowStride = loc.scalarElemSize * loc.colCount;

        Instruction *rowOffset = editor.InsertInstruction(
            f, i++,
            editor.CreateInstruction(Operation::Mul, i32, {row, editor.CreateConstant(rowStride)}));

        elemByteOffset = editor.InsertInstruction(
            f, i++, editor.CreateInstruction(Operation::Add, i32, {rowOffset, colByteOffset}));
      }

      Instruction *vertexOffset = editor.InsertInstruction(
          f, i++,
          editor.CreateInstruction(Operation::Mul, i32,
                                   {vert, editor.CreateConstant(layout.vertStride)}));

      // base + sig indexed offset + vertex indexed offset + elem offset

      Instruction *writeOffset = editor.InsertInstruction(
          f, i++,
          editor.CreateInstruction(Operation::Add, i32,
                                   {baseOffset, editor.CreateConstant(loc.offset)}));

      writeOffset = editor.InsertInstruction(
          f, i++, editor.CreateInstruction(Operation::Add, i32, {writeOffset, vertexOffset}));

      writeOffset = editor.InsertInstruction(
          f, i++, editor.CreateInstruction(Operation::Add, i32, {writeOffset, elemByteOffset}));

      rdcstr suffix = makeBufferLoadStoreSuffix(value->type);

      const Function *rawBufferStore = editor.DeclareFunction(
          "dx.op.rawBufferStore." + suffix, voidType,
          {i32, handleType, i32, i32, value->type, value->type, value->type, value->type, i8, i32},
          Attribute::NoUnwind);

      editor.InsertInstruction(
          f, i++,
          editor.CreateInstruction(
              rawBufferStore, DXOp::RawBufferStore,
              {handle, writeOffset, editor.CreateUndef(i32), value, editor.CreateUndef(value->type),
               editor.CreateUndef(value->type), editor.CreateUndef(value->type),
               editor.CreateConstant((uint8_t)0x1), i32_4}));
    }
    else if(inst.op == Operation::Call &&
            inst.getFuncCall()->name.beginsWith("dx.op.storePrimitiveOutput"))
    {
      uint32_t sigId = cast<Constant>(inst.args[1])->getU32();
      Value *row = inst.args[2];
      Value *col = inst.args[3];
      Value *value = inst.args[4];
      Value *prim = inst.args[5];

      OutDXILSigLocation &loc = layout.sigLocations[firstPrimOutput + sigId];

      Instruction *colByteOffset = NULL;

      // col is i8, but DXIL doesn't support i8 as values (sigh...). So if that value is a constant
      // (currently must be true) then we re-create it as u32. We handle the case where it's not a
      // constant in future perhaps
      Constant *colConst = cast<Constant>(col);
      if(colConst)
      {
        colByteOffset = editor.InsertInstruction(
            f, i++,
            editor.CreateInstruction(Operation::Mul, i32,
                                     {editor.CreateConstant(colConst->getU32()),
                                      editor.CreateConstant(loc.scalarElemSize)}));
      }
      else
      {
        colByteOffset = editor.InsertInstruction(
            f, i++,
            editor.CreateInstruction(Operation::Mul, i8,
                                     {col, editor.CreateConstant(uint8_t(loc.scalarElemSize))}));

        colByteOffset =
            editor.InsertInstruction(f, i++, editor.CreateInstruction(Operation::ZExt, i32, {col}));
      }

      Instruction *elemByteOffset = colByteOffset;

      if(loc.rowCount > 1)
      {
        uint32_t rowStride = loc.scalarElemSize * loc.colCount;

        Instruction *rowOffset = editor.InsertInstruction(
            f, i++,
            editor.CreateInstruction(Operation::Mul, i32, {row, editor.CreateConstant(rowStride)}));

        elemByteOffset = editor.InsertInstruction(
            f, i++, editor.CreateInstruction(Operation::Add, i32, {rowOffset, colByteOffset}));
      }

      Instruction *primOffset = editor.InsertInstruction(
          f, i++,
          editor.CreateInstruction(Operation::Mul, i32,
                                   {prim, editor.CreateConstant(layout.primStride)}));

      // base + sig indexed offset + vertex indexed offset + elem offset

      Instruction *writeOffset = editor.InsertInstruction(
          f, i++,
          editor.CreateInstruction(Operation::Add, i32,
                                   {baseOffset, editor.CreateConstant(loc.offset)}));

      writeOffset = editor.InsertInstruction(
          f, i++, editor.CreateInstruction(Operation::Add, i32, {writeOffset, primOffset}));

      writeOffset = editor.InsertInstruction(
          f, i++, editor.CreateInstruction(Operation::Add, i32, {writeOffset, elemByteOffset}));

      rdcstr suffix = makeBufferLoadStoreSuffix(value->type);

      const Function *rawBufferStore = editor.DeclareFunction(
          "dx.op.rawBufferStore." + suffix, voidType,
          {i32, handleType, i32, i32, value->type, value->type, value->type, value->type, i8, i32},
          Attribute::NoUnwind);

      editor.InsertInstruction(
          f, i++,
          editor.CreateInstruction(
              rawBufferStore, DXOp::RawBufferStore,
              {handle, writeOffset, editor.CreateUndef(i32), value, editor.CreateUndef(value->type),
               editor.CreateUndef(value->type), editor.CreateUndef(value->type),
               editor.CreateConstant((uint8_t)0x1), i32_4}));
    }
    else if(inst.op == Operation::Call && inst.getFuncCall()->name == emitIndices->name)
    {
      // primitive index in args[1], so multiply to get location of indices
      Instruction *byteOffset = editor.InsertInstruction(
          f, i++, editor.CreateInstruction(Operation::Mul, i32, {inst.args[1], indexStride}));

      Instruction *writeOffset = editor.InsertInstruction(
          f, i++,
          editor.CreateInstruction(Operation::Add, i32,
                                   {baseOffset, editor.CreateConstant(idxDataOffset)}));

      writeOffset = editor.InsertInstruction(
          f, i++, editor.CreateInstruction(Operation::Add, i32, {writeOffset, byteOffset}));

      const Function *rawBufferStore = editor.DeclareFunction(
          "dx.op.rawBufferStore.i32", voidType,
          {i32, handleType, i32, i32, i32, i32, i32, i32, i8, i32}, Attribute::NoUnwind);

      // idx0
      editor.InsertInstruction(
          f, i++,
          editor.CreateInstruction(
              rawBufferStore, DXOp::RawBufferStore,
              {handle, writeOffset, editor.CreateUndef(i32), inst.args[2], editor.CreateUndef(i32),
               editor.CreateUndef(i32), editor.CreateUndef(i32),
               editor.CreateConstant((uint8_t)0x1), i32_4}));

      // idx1
      writeOffset = editor.InsertInstruction(
          f, i++, editor.CreateInstruction(Operation::Add, i32, {writeOffset, i32_4}));

      editor.InsertInstruction(
          f, i++,
          editor.CreateInstruction(
              rawBufferStore, DXOp::RawBufferStore,
              {handle, writeOffset, editor.CreateUndef(i32), inst.args[3], editor.CreateUndef(i32),
               editor.CreateUndef(i32), editor.CreateUndef(i32),
               editor.CreateConstant((uint8_t)0x1), i32_4}));

      if(layout.indexCountPerPrim > 2)
      {
        // idx2
        writeOffset = editor.InsertInstruction(
            f, i++, editor.CreateInstruction(Operation::Add, i32, {writeOffset, i32_4}));

        editor.InsertInstruction(
            f, i++,
            editor.CreateInstruction(
                rawBufferStore, DXOp::RawBufferStore,
                {handle, writeOffset, editor.CreateUndef(i32), inst.args[4],
                 editor.CreateUndef(i32), editor.CreateUndef(i32), editor.CreateUndef(i32),
                 editor.CreateConstant((uint8_t)0x1), i32_4}));
      }
    }
  }
}

bool D3D12Replay::CreateSOBuffers()
{
  HRESULT hr = S_OK;

  SAFE_RELEASE(m_SOBuffer);
  SAFE_RELEASE(m_SOStagingBuffer);
  SAFE_RELEASE(m_SOPatchedIndexBuffer);
  SAFE_RELEASE(m_SOQueryHeap);

  if(m_SOBufferSize >= 0xFFFF0000ULL)
  {
    RDCERR(
        "Stream-out buffer size %llu is close to or over 4GB, out of memory very likely so "
        "skipping",
        m_SOBufferSize);
    m_SOBufferSize = 0;
    return false;
  }

  D3D12_RESOURCE_DESC soBufDesc;
  soBufDesc.Alignment = 0;
  soBufDesc.DepthOrArraySize = 1;
  soBufDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
  // need to allow UAV access to reset the counter each time
  soBufDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
  soBufDesc.Format = DXGI_FORMAT_UNKNOWN;
  soBufDesc.Height = 1;
  soBufDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
  soBufDesc.MipLevels = 1;
  soBufDesc.SampleDesc.Count = 1;
  soBufDesc.SampleDesc.Quality = 0;
  // add 64 bytes for the counter at the start
  soBufDesc.Width = m_SOBufferSize + 64;

  D3D12_HEAP_PROPERTIES heapProps;
  heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
  heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
  heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
  heapProps.CreationNodeMask = 1;
  heapProps.VisibleNodeMask = 1;

  hr = m_pDevice->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &soBufDesc,
                                          D3D12_RESOURCE_STATE_COMMON, NULL,
                                          __uuidof(ID3D12Resource), (void **)&m_SOBuffer);

  if(FAILED(hr))
  {
    RDCERR("Failed to create SO output buffer, HRESULT: %s", ToStr(hr).c_str());
    m_SOBufferSize = 0;
    return false;
  }

  m_SOBuffer->SetName(L"m_SOBuffer");

  soBufDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
  heapProps.Type = D3D12_HEAP_TYPE_READBACK;

  hr = m_pDevice->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &soBufDesc,
                                          D3D12_RESOURCE_STATE_COPY_DEST, NULL,
                                          __uuidof(ID3D12Resource), (void **)&m_SOStagingBuffer);

  if(FAILED(hr))
  {
    RDCERR("Failed to create readback buffer, HRESULT: %s", ToStr(hr).c_str());
    m_SOBufferSize = 0;
    return false;
  }

  m_SOStagingBuffer->SetName(L"m_SOStagingBuffer");

  // this is a buffer of unique indices, so it allows for
  // the worst case - float4 per vertex, all unique indices.
  soBufDesc.Width = m_SOBufferSize / sizeof(Vec4f);
  heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

  hr = m_pDevice->CreateCommittedResource(
      &heapProps, D3D12_HEAP_FLAG_NONE, &soBufDesc, D3D12_RESOURCE_STATE_GENERIC_READ, NULL,
      __uuidof(ID3D12Resource), (void **)&m_SOPatchedIndexBuffer);

  if(FAILED(hr))
  {
    RDCERR("Failed to create SO index buffer, HRESULT: %s", ToStr(hr).c_str());
    m_SOBufferSize = 0;
    return false;
  }

  m_SOPatchedIndexBuffer->SetName(L"m_SOPatchedIndexBuffer");

  D3D12_QUERY_HEAP_DESC queryDesc;
  queryDesc.Count = 16;
  queryDesc.NodeMask = 1;
  queryDesc.Type = D3D12_QUERY_HEAP_TYPE_SO_STATISTICS;
  hr = m_pDevice->CreateQueryHeap(&queryDesc, __uuidof(m_SOQueryHeap), (void **)&m_SOQueryHeap);

  if(FAILED(hr))
  {
    RDCERR("Failed to create SO query heap, HRESULT: %s", ToStr(hr).c_str());
    m_SOBufferSize = 0;
    return false;
  }

  D3D12_UNORDERED_ACCESS_VIEW_DESC counterDesc = {};
  counterDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
  counterDesc.Format = DXGI_FORMAT_R32_UINT;
  counterDesc.Buffer.FirstElement = 0;
  counterDesc.Buffer.NumElements = UINT(m_SOBufferSize / sizeof(UINT));

  m_pDevice->CreateUnorderedAccessView(m_SOBuffer, NULL, &counterDesc,
                                       GetDebugManager()->GetCPUHandle(STREAM_OUT_UAV));

  m_pDevice->CreateUnorderedAccessView(m_SOBuffer, NULL, &counterDesc,
                                       GetDebugManager()->GetUAVClearHandle(STREAM_OUT_UAV));

  return true;
}

void D3D12Replay::ClearPostVSCache()
{
  for(auto it = m_PostVSData.begin(); it != m_PostVSData.end(); ++it)
  {
    SAFE_RELEASE(it->second.vsout.buf);
    SAFE_RELEASE(it->second.vsout.idxBuf);
    SAFE_RELEASE(it->second.gsout.buf);
    SAFE_RELEASE(it->second.gsout.idxBuf);
  }

  m_PostVSData.clear();
}

void D3D12Replay::InitPostMSBuffers(uint32_t eventId)
{
  D3D12PostVSData &ret = m_PostVSData[eventId];

  const ActionDescription *action = m_pDevice->GetAction(eventId);

  rdcfixedarray<uint32_t, 3> dispatchSize = action->dispatchDimension;

  D3D12RenderState &rs = m_pDevice->GetQueue()->GetCommandData()->m_RenderState;

  D3D12ResourceManager *rm = m_pDevice->GetResourceManager();

  WrappedID3D12PipelineState *pipe =
      (WrappedID3D12PipelineState *)rm->GetCurrentAs<ID3D12PipelineState>(rs.pipe);
  D3D12RootSignature modsig;

  // for indirect dispatches, fetch up to date dispatch sizes in case they're non-deterministic
  if(action->flags & ActionFlags::Indirect)
  {
    uint32_t chunkIdx = action->events.back().chunkIndex;
    uint32_t parentIdx = action->parent->events.back().chunkIndex;
    const SDFile *file = m_pDevice->GetStructuredFile();

    if(chunkIdx < file->chunks.size() && parentIdx < file->chunks.size())
    {
      const SDChunk *chunk = file->chunks[chunkIdx];
      const SDChunk *parentChunk = file->chunks[parentIdx];

      uint32_t cmdIdx = chunk->FindChild("CommandIndex")->AsUInt32();
      uint32_t argIdx = chunk->FindChild("ArgumentIndex")->AsUInt32();

      WrappedID3D12CommandSignature *comSig = rm->GetLiveAs<WrappedID3D12CommandSignature>(
          parentChunk->FindChild("pCommandSignature")->AsResourceId());
      ID3D12Resource *argBuf =
          rm->GetLiveAs<ID3D12Resource>(parentChunk->FindChild("pArgumentBuffer")->AsResourceId());
      uint64_t argOffs = parentChunk->FindChild("ArgumentBufferOffset")->AsUInt64();

      argOffs += cmdIdx * comSig->sig.ByteStride;

      for(uint32_t i = 0; i < argIdx; i++)
        argOffs += ArgumentTypeByteSize(comSig->sig.arguments[i]);

      bytebuf dispatchArgs;
      GetDebugManager()->GetBufferData(argBuf, argOffs, sizeof(D3D12_DISPATCH_MESH_ARGUMENTS),
                                       dispatchArgs);

      if(dispatchArgs.size() >= sizeof(D3D12_DISPATCH_MESH_ARGUMENTS))
      {
        D3D12_DISPATCH_MESH_ARGUMENTS *meshArgs =
            (D3D12_DISPATCH_MESH_ARGUMENTS *)dispatchArgs.data();

        dispatchSize[0] = meshArgs->ThreadGroupCountX;
        dispatchSize[1] = meshArgs->ThreadGroupCountY;
        dispatchSize[2] = meshArgs->ThreadGroupCountZ;
      }
    }
  }

  uint32_t totalNumMeshlets = dispatchSize[0] * dispatchSize[1] * dispatchSize[2];

  // set defaults so that we don't try to fetch this output again if something goes wrong and the
  // same event is selected again
  {
    ret.meshout.buf = NULL;
    ret.meshout.bufSize = ~0ULL;
    ret.meshout.instStride = 0;
    ret.meshout.vertStride = 0;
    ret.meshout.nearPlane = 0.0f;
    ret.meshout.farPlane = 0.0f;
    ret.meshout.useIndices = false;
    ret.meshout.hasPosOut = false;
    ret.meshout.idxBuf = NULL;
    ret.meshout.idxBufSize = ~0ULL;

    ret.meshout.topo = pipe->MS()->GetDetails().outputTopology;
    ret.ampout = ret.meshout;
  }

#if ENABLED(RDOC_DEVEL)
  m_pDevice->GetShaderCache()->LoadDXC();
#endif

  D3D12_EXPANDED_PIPELINE_STATE_STREAM_DESC pipeDesc;
  pipe->Fill(pipeDesc);

  ID3D12RootSignature *rootsig = rm->GetCurrentAs<ID3D12RootSignature>(rs.graphics.rootsig);

  if(!rootsig)
  {
    ret.ampout.status = ret.meshout.status = "No root signature bound at draw";
    return;
  }

  modsig = ((WrappedID3D12RootSignature *)rootsig)->sig;

  uint32_t space = modsig.maxSpaceIndex;

  // add root UAV elements
  {
    modsig.Parameters.push_back(D3D12RootSignatureParameter());
    D3D12RootSignatureParameter &param = modsig.Parameters.back();
    param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV;
    param.ShaderVisibility = D3D12_SHADER_VISIBILITY_MESH;
    param.Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_DATA_VOLATILE;
    param.Descriptor.RegisterSpace = space;
    param.Descriptor.ShaderRegister = 0;
  }

  if(pipeDesc.AS.BytecodeLength > 0)
  {
    modsig.Parameters.push_back(D3D12RootSignatureParameter());
    D3D12RootSignatureParameter &param = modsig.Parameters.back();
    param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV;
    param.ShaderVisibility = D3D12_SHADER_VISIBILITY_AMPLIFICATION;
    param.Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_DATA_VOLATILE;
    param.Descriptor.RegisterSpace = space;
    param.Descriptor.ShaderRegister = 1;
  }

  ID3D12RootSignature *annotatedSig = NULL;

  {
    ID3DBlob *blob = m_pDevice->GetShaderCache()->MakeRootSig(modsig);
    HRESULT hr =
        m_pDevice->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(),
                                       __uuidof(ID3D12RootSignature), (void **)&annotatedSig);

    SAFE_RELEASE(blob);

    if(annotatedSig == NULL || FAILED(hr))
    {
      ret.ampout.status = ret.meshout.status = StringFormat::Fmt(
          "Couldn't create mesh-fetch modified root signature: HRESULT: %s", ToStr(hr).c_str());
      RDCERR("%s", ret.meshout.status.c_str());
      return;
    }
  }

  pipeDesc.pRootSignature = annotatedSig;

  HRESULT hr = S_OK;

  ID3D12Resource *meshBuffer = NULL;
  ID3D12Resource *ampBuffer = NULL;

  uint64_t ampBufSize = 0;
  uint32_t payloadSize = 0;

  rdcarray<D3D12PostVSData::InstData> ampDispatchSizes;
  const uint32_t totalNumAmpGroups = totalNumMeshlets;

  bytebuf ampFetchDXIL;
  bytebuf ampFeederDXIL;

  if(pipeDesc.AS.BytecodeLength > 0)
  {
    AddDXILAmpShaderPayloadStores(pipe->AS()->GetDXBC(), space, dispatchSize, payloadSize,
                                  ampFetchDXIL);

    // strip the root signature, we shouldn't need it and it may no longer match and fail validation
    DXBC::DXBCContainer::StripChunk(ampFetchDXIL, DXBC::FOURCC_RTS0);

    if(!D3D12_Debug_PostVSDumpDirPath().empty())
    {
      bytebuf orig = pipe->AS()->GetDXBC()->GetShaderBlob();

      DXBC::DXBCContainer::StripChunk(orig, DXBC::FOURCC_ILDB);
      DXBC::DXBCContainer::StripChunk(orig, DXBC::FOURCC_STAT);

      FileIO::WriteAll(D3D12_Debug_PostVSDumpDirPath() + "/debug_postts_before.dxbc", orig);
    }

    if(!D3D12_Debug_PostVSDumpDirPath().empty())
    {
      FileIO::WriteAll(D3D12_Debug_PostVSDumpDirPath() + "/debug_postts_after.dxbc", ampFetchDXIL);
    }

    // now that we know the stride, create buffer of sufficient size for the worst case (maximum
    // generation) of the meshlets
    ampBufSize = (payloadSize + sizeof(Vec4u)) * totalNumAmpGroups + sizeof(Vec4u);

    {
      D3D12_RESOURCE_DESC desc = {};
      desc.Alignment = 0;
      desc.DepthOrArraySize = 1;
      desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
      desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
      desc.Format = DXGI_FORMAT_UNKNOWN;
      desc.Height = 1;
      desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
      desc.MipLevels = 1;
      desc.SampleDesc.Count = 1;
      desc.SampleDesc.Quality = 0;
      desc.Width = ampBufSize;

      D3D12_HEAP_PROPERTIES heapProps;
      heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
      heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
      heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
      heapProps.CreationNodeMask = 1;
      heapProps.VisibleNodeMask = 1;

      hr = m_pDevice->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &desc,
                                              D3D12_RESOURCE_STATE_COMMON, NULL,
                                              __uuidof(ID3D12Resource), (void **)&ampBuffer);

      if(ampBuffer == NULL || FAILED(hr))
      {
        SAFE_RELEASE(annotatedSig);
        ret.ampout.status = ret.meshout.status = StringFormat::Fmt(
            "Couldn't create amplification output buffer: HRESULT: %s", ToStr(hr).c_str());
        RDCERR("%s", ret.meshout.status.c_str());
        return;
      }

      ampBuffer->SetName(L"Amp. output");
    }

    pipeDesc.AS.pShaderBytecode = ampFetchDXIL.data();
    pipeDesc.AS.BytecodeLength = ampFetchDXIL.size();

    ID3D12PipelineState *ampOutPipe = NULL;
    hr = m_pDevice->CreatePipeState(pipeDesc, &ampOutPipe);
    if(ampOutPipe == NULL || FAILED(hr))
    {
      SAFE_RELEASE(annotatedSig);
      SAFE_RELEASE(ampBuffer);
      ret.ampout.status = ret.meshout.status =
          StringFormat::Fmt("Couldn't create amplification output pipeline: %s", ToStr(hr).c_str());
      RDCERR("%s", ret.meshout.status.c_str());
      return;
    }

    D3D12RenderState prev = rs;

    rs.pipe = GetResID(ampOutPipe);
    rs.graphics.rootsig = GetResID(annotatedSig);

    // we don't use the mesh buffer root parameter, so just fill it in with the same buffer
    {
      size_t idx = modsig.Parameters.size() - 2;
      rs.graphics.sigelems.resize(modsig.Parameters.size());
      rs.graphics.sigelems[idx] =
          D3D12RenderState::SignatureElement(eRootUAV, GetResID(ampBuffer), 0);
      idx++;
      rs.graphics.sigelems[idx] =
          D3D12RenderState::SignatureElement(eRootUAV, GetResID(ampBuffer), 0);
    }

    ID3D12GraphicsCommandListX *list = GetDebugManager()->ResetDebugList();

    rs.ApplyState(m_pDevice, list);

    list->DispatchMesh(dispatchSize[0], dispatchSize[1], dispatchSize[2]);

    list->Close();

    ID3D12CommandList *l = list;
    m_pDevice->GetQueue()->ExecuteCommandLists(1, &l);
    m_pDevice->GPUSync();

    GetDebugManager()->ResetDebugAlloc();

    SAFE_RELEASE(ampOutPipe);

    rs = prev;

    totalNumMeshlets = 0;
    bytebuf ampBufContents;
    GetDebugManager()->GetBufferData(ampBuffer, 0, ampBufSize, ampBufContents);
    ampBufContents.resize((size_t)ampBufSize);

    const byte *ampData = ampBufContents.data();
    const byte *ampDataBegin = ampData;

    rdcarray<D3D12_WRITEBUFFERIMMEDIATE_PARAMETER> writes;

    for(uint32_t ampGroup = 0; ampGroup < totalNumAmpGroups; ampGroup++)
    {
      Vec4u meshDispatchSize = *(Vec4u *)ampData;
      RDCASSERT(meshDispatchSize.y <= 0xffff);
      RDCASSERT(meshDispatchSize.z <= 0xffff);

      // while we're going, we record writes into the real buffer with the cumulative sizes. This
      // should in theory be better than updating it via a buffer copy since the count should be
      // much smaller than the payload
      writes.push_back(
          {ampBuffer->GetGPUVirtualAddress() + ampData - ampDataBegin + offsetof(Vec4u, w),
           totalNumMeshlets});

      totalNumMeshlets += meshDispatchSize.x * meshDispatchSize.y * meshDispatchSize.z;

      D3D12PostVSData::InstData i;
      i.ampDispatchSizeX = meshDispatchSize.x;
      i.ampDispatchSizeYZ.y = meshDispatchSize.y & 0xffff;
      i.ampDispatchSizeYZ.z = meshDispatchSize.z & 0xffff;
      ampDispatchSizes.push_back(i);

      ampData += sizeof(Vec4u) + payloadSize;
    }

    list = GetDebugManager()->ResetDebugList();

    list->WriteBufferImmediate(writes.count(), writes.data(), NULL);
    list->Close();

    m_pDevice->GetQueue()->ExecuteCommandLists(1, &l);
    m_pDevice->GPUSync();

    GetDebugManager()->ResetDebugAlloc();

    ConvertToFixedDXILAmpFeeder(pipe->AS()->GetDXBC(), space, dispatchSize, ampFeederDXIL);

    // strip the root signature, we shouldn't need it and it may no longer match and fail validation
    DXBC::DXBCContainer::StripChunk(ampFeederDXIL, DXBC::FOURCC_RTS0);

    if(!D3D12_Debug_PostVSDumpDirPath().empty())
      FileIO::WriteAll(D3D12_Debug_PostVSDumpDirPath() + "/debug_postts_feeder.dxbc", ampFeederDXIL);
  }

  OutDXILMeshletLayout layout;

  bytebuf meshOutputDXIL;

  AddDXILMeshShaderOutputStores(payloadSize, pipe->MS()->GetDXBC(), space, ampBuffer != NULL,
                                dispatchSize, layout, meshOutputDXIL);

  {
    // strip the root signature, we shouldn't need it and it may no longer match and fail validation
    DXBC::DXBCContainer::StripChunk(meshOutputDXIL, DXBC::FOURCC_RTS0);

    if(!D3D12_Debug_PostVSDumpDirPath().empty())
    {
      bytebuf orig = pipe->MS()->GetDXBC()->GetShaderBlob();

      DXBC::DXBCContainer::StripChunk(orig, DXBC::FOURCC_ILDB);
      DXBC::DXBCContainer::StripChunk(orig, DXBC::FOURCC_STAT);

      FileIO::WriteAll(D3D12_Debug_PostVSDumpDirPath() + "/debug_postms_before.dxbc", orig);
    }

    if(!D3D12_Debug_PostVSDumpDirPath().empty())
    {
      FileIO::WriteAll(D3D12_Debug_PostVSDumpDirPath() + "/debug_postms_after.dxbc", meshOutputDXIL);
    }
  }

  if(totalNumMeshlets > 0)
  {
    // now that we know the stride, create buffer of sufficient size for the worst case (maximum
    // generation) of the meshlets

    {
      D3D12_RESOURCE_DESC desc = {};
      desc.Alignment = 0;
      desc.DepthOrArraySize = 1;
      desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
      desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
      desc.Format = DXGI_FORMAT_UNKNOWN;
      desc.Height = 1;
      desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
      desc.MipLevels = 1;
      desc.SampleDesc.Count = 1;
      desc.SampleDesc.Quality = 0;
      desc.Width = layout.meshletByteSize * totalNumMeshlets;

      D3D12_HEAP_PROPERTIES heapProps;
      heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
      heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
      heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
      heapProps.CreationNodeMask = 1;
      heapProps.VisibleNodeMask = 1;

      hr = m_pDevice->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &desc,
                                              D3D12_RESOURCE_STATE_COMMON, NULL,
                                              __uuidof(ID3D12Resource), (void **)&meshBuffer);

      if(meshBuffer == NULL || FAILED(hr))
      {
        SAFE_RELEASE(annotatedSig);
        SAFE_RELEASE(ampBuffer);
        ret.meshout.status =
            StringFormat::Fmt("Couldn't create mesh output buffer: HRESULT: %s", ToStr(hr).c_str());
        RDCERR("%s", ret.meshout.status.c_str());
        return;
      }

      meshBuffer->SetName(L"Mesh output");
    }

    if(ampFeederDXIL.empty())
    {
      pipeDesc.AS.pShaderBytecode = NULL;
      pipeDesc.AS.BytecodeLength = 0;
    }
    else
    {
      pipeDesc.AS.pShaderBytecode = ampFeederDXIL.data();
      pipeDesc.AS.BytecodeLength = ampFeederDXIL.size();
    }

    pipeDesc.MS.pShaderBytecode = meshOutputDXIL.data();
    pipeDesc.MS.BytecodeLength = meshOutputDXIL.size();

    ID3D12PipelineState *meshOutPipe = NULL;
    hr = m_pDevice->CreatePipeState(pipeDesc, &meshOutPipe);
    if(meshOutPipe == NULL || FAILED(hr))
    {
      SAFE_RELEASE(annotatedSig);
      SAFE_RELEASE(ampBuffer);
      SAFE_RELEASE(meshBuffer);
      ret.meshout.status =
          StringFormat::Fmt("Couldn't create mesh output pipeline: %s", ToStr(hr).c_str());
      RDCERR("%s", ret.meshout.status.c_str());
      return;
    }

    D3D12RenderState prev = rs;
    rs.pipe = GetResID(meshOutPipe);
    rs.graphics.rootsig = GetResID(annotatedSig);
    if(pipeDesc.AS.BytecodeLength > 0)
    {
      size_t idx = modsig.Parameters.size() - 2;
      rs.graphics.sigelems.resize(modsig.Parameters.size());
      rs.graphics.sigelems[idx] =
          D3D12RenderState::SignatureElement(eRootUAV, GetResID(meshBuffer), 0);
      idx++;
      rs.graphics.sigelems[idx] =
          D3D12RenderState::SignatureElement(eRootUAV, GetResID(ampBuffer), 0);
    }
    else
    {
      size_t idx = modsig.Parameters.size() - 1;
      rs.graphics.sigelems.resize(modsig.Parameters.size());
      rs.graphics.sigelems[idx] =
          D3D12RenderState::SignatureElement(eRootUAV, GetResID(meshBuffer), 0);
    }

    ID3D12GraphicsCommandListX *list = GetDebugManager()->ResetDebugList();

    rs.ApplyState(m_pDevice, list);

    list->DispatchMesh(dispatchSize[0], dispatchSize[1], dispatchSize[2]);

    list->Close();

    ID3D12CommandList *l = list;
    m_pDevice->GetQueue()->ExecuteCommandLists(1, &l);
    m_pDevice->GPUSync();

    GetDebugManager()->ResetDebugAlloc();

    rs = prev;

    SAFE_RELEASE(meshOutPipe);
  }
  SAFE_RELEASE(annotatedSig);

  rdcarray<D3D12PostVSData::InstData> meshletOffsets;

  uint32_t baseIndex = 0;

  rdcarray<uint32_t> rebasedIndices;
  bytebuf compactedVertices;

  float nearp = 0.1f;
  float farp = 100.0f;

  uint32_t totalVerts = 0, totalPrims = 0;

  if(totalNumMeshlets > 0)
  {
    bytebuf meshBufferContents;
    GetDebugManager()->GetBufferData(meshBuffer, 0, 0, meshBufferContents);

    if(meshBufferContents.empty())
    {
      SAFE_RELEASE(ampBuffer);
      SAFE_RELEASE(meshBuffer);

      ret.meshout.status = "Couldn't read back mesh output data from GPU";
      return;
    }

    const byte *meshletData = meshBufferContents.data();

    // do a super quick sum of the number of verts and prims
    for(uint32_t m = 0; m < totalNumMeshlets; m++)
    {
      Vec4u *counts = (Vec4u *)(meshletData + m * layout.meshletByteSize);
      totalVerts += counts->x;
      totalPrims += counts->y;
    }

    if(totalPrims == 0)
    {
      SAFE_RELEASE(ampBuffer);
      SAFE_RELEASE(meshBuffer);

      ret.meshout.status = "No mesh output data generated by GPU";
      return;
    }

    // now we compact the data.
    // Arrays are already written interleaved, we just have to omit the empty space from
    // smaller-than-max meshlets.
    // We also rebase indices so they can be used as a contiguous index buffer

    rebasedIndices.reserve(totalPrims * layout.indexCountPerPrim);
    compactedVertices.resize(totalVerts * layout.vertStride + totalPrims * layout.primStride);

    byte *vertData = compactedVertices.begin();
    byte *primData = vertData + totalVerts * layout.vertStride;

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
        SAFE_RELEASE(ampBuffer);
        SAFE_RELEASE(meshBuffer);

        RDCERR("Meshlet returned invalid vertex count %u with declared max %u", numVerts,
               layout.vertArrayLength);
        ret.meshout.status = "Got corrupted mesh output data from GPU";
        return;
      }

      if(numPrims > layout.primArrayLength)
      {
        SAFE_RELEASE(ampBuffer);
        SAFE_RELEASE(meshBuffer);

        RDCERR("Meshlet returned invalid primitive count %u with declared max %u", numPrims,
               layout.primArrayLength);
        ret.meshout.status = "Got corrupted mesh output data from GPU";
        return;
      }

      meshletOffsets.push_back({numPrims * layout.indexCountPerPrim, numVerts});

      uint32_t *indices = (uint32_t *)(counts + 2);

      for(uint32_t p = 0; p < numPrims; p++)
      {
        for(uint32_t idx = 0; idx < layout.indexCountPerPrim; idx++)
          rebasedIndices.push_back(indices[p * layout.indexCountPerPrim + idx] + baseIndex);
      }

      byte *perVertData =
          (byte *)(indices + AlignUp4(layout.indexCountPerPrim * layout.primArrayLength));

      memcpy(vertData, perVertData, layout.vertStride * numVerts);

      byte *perPrimData = (byte *)(perVertData + layout.vertStride * layout.vertArrayLength);

      if(layout.primStride > 0)
        memcpy(primData, perPrimData, layout.primStride * numPrims);

      if(!found)
      {
        pos0 = *(Vec4f *)vertData;

        for(uint32_t v = 0; !found && v < numVerts; v++)
        {
          Vec4f *pos = (Vec4f *)(vertData + layout.vertStride * v);
          DeriveNearFar(*pos, pos0, nearp, farp, found);
        }
      }

      baseIndex += numVerts;
      meshletData += layout.meshletByteSize;
      vertData += layout.vertStride * numVerts;
      primData += layout.primStride * numPrims;
    }

    RDCASSERT(vertData == compactedVertices.begin() + totalVerts * layout.vertStride);
    RDCASSERT(primData == compactedVertices.end());

    // if we didn't find any near/far plane, all z's and w's were identical.
    // If the z is positive and w greater for the first element then we detect this projection as
    // reversed z with infinite far plane
    if(!found && pos0.z > 0.0f && pos0.w > pos0.z)
    {
      nearp = pos0.z;
      farp = FLT_MAX;
    }
  }

  SAFE_RELEASE(meshBuffer);

  // fill out m_PostVS.Data
  if(layout.indexCountPerPrim == 3)
    ret.meshout.topo = Topology::TriangleList;
  else if(layout.indexCountPerPrim == 2)
    ret.meshout.topo = Topology::LineList;
  else if(layout.indexCountPerPrim == 1)
    ret.meshout.topo = Topology::PointList;

  uint64_t meshBufSize = ~0ULL;
  if(totalNumMeshlets > 0)
  {
    D3D12_RESOURCE_DESC desc = {};
    desc.Alignment = 0;
    desc.DepthOrArraySize = 1;
    desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Flags = D3D12_RESOURCE_FLAG_NONE;
    desc.Format = DXGI_FORMAT_UNKNOWN;
    desc.Height = 1;
    desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    desc.MipLevels = 1;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    meshBufSize = AlignUp16(compactedVertices.byteSize()) + rebasedIndices.byteSize();
    desc.Width = meshBufSize;

    D3D12_HEAP_PROPERTIES heapProps;
    heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
    heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    heapProps.CreationNodeMask = 1;
    heapProps.VisibleNodeMask = 1;

    hr = m_pDevice->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &desc,
                                            D3D12_RESOURCE_STATE_GENERIC_READ, NULL,
                                            __uuidof(ID3D12Resource), (void **)&meshBuffer);

    if(meshBuffer == NULL || FAILED(hr))
    {
      SAFE_RELEASE(annotatedSig);
      ret.meshout.status =
          StringFormat::Fmt("Couldn't create mesh output storage: HRESULT: %s", ToStr(hr).c_str());
      RDCERR("%s", ret.meshout.status.c_str());
      return;
    }

    meshBuffer->SetName(L"Baked mesh output + indices1");

    byte *uploadData = NULL;
    hr = meshBuffer->Map(0, NULL, (void **)&uploadData);
    if(FAILED(hr))
    {
      SAFE_RELEASE(ampBuffer);
      SAFE_RELEASE(meshBuffer);
      ret.meshout.status = "Couldn't upload mesh output data to GPU";
      return;
    }

    memcpy(uploadData, compactedVertices.data(), compactedVertices.byteSize());
    memcpy(uploadData + AlignUp16(compactedVertices.byteSize()), rebasedIndices.data(),
           rebasedIndices.byteSize());

    meshBuffer->Unmap(0, NULL);
  }

  ret.ampout.buf = ampBuffer;
  ret.ampout.bufSize = ampBufSize;

  if(pipeDesc.AS.BytecodeLength == 0)
    ret.ampout.status = "No amplification shader bound";

  ret.ampout.vertStride = payloadSize + sizeof(Vec4u);
  ret.ampout.nearPlane = 0.0f;
  ret.ampout.farPlane = 1.0f;

  ret.ampout.primStride = 0;
  ret.ampout.primOffset = 0;

  ret.ampout.useIndices = false;
  ret.ampout.numVerts = totalNumAmpGroups;
  ret.ampout.instData = ampDispatchSizes;

  ret.ampout.instStride = 0;

  ret.ampout.idxBuf = NULL;
  ret.ampout.idxBufSize = ~0ULL;
  ret.ampout.idxOffset = 0;
  ret.ampout.idxFmt = DXGI_FORMAT_UNKNOWN;

  ret.ampout.hasPosOut = false;

  ret.ampout.dispatchSize = dispatchSize;

  ret.meshout.buf = meshBuffer;
  ret.meshout.bufSize = meshBufSize;

  ret.meshout.vertStride = layout.vertStride;
  ret.meshout.nearPlane = nearp;
  ret.meshout.farPlane = farp;

  ret.meshout.primStride = layout.primStride;
  ret.meshout.primOffset = layout.primStride * totalVerts;

  ret.meshout.useIndices = true;
  ret.meshout.numVerts = totalPrims * layout.indexCountPerPrim;
  ret.meshout.instData = meshletOffsets;

  ret.meshout.dispatchSize = dispatchSize;

  ret.meshout.instStride = 0;

  ret.meshout.idxBuf = meshBuffer;
  ret.meshout.idxBufSize = meshBufSize;
  ret.meshout.idxOffset = AlignUp16(compactedVertices.byteSize());
  ret.meshout.idxFmt = DXGI_FORMAT_R32_UINT;

  ret.meshout.hasPosOut = true;
}

void D3D12Replay::InitPostVSBuffers(uint32_t eventId)
{
  // go through any aliasing
  if(m_PostVSAlias.find(eventId) != m_PostVSAlias.end())
    eventId = m_PostVSAlias[eventId];

  if(m_PostVSData.find(eventId) != m_PostVSData.end())
    return;

  D3D12PostVSData &ret = m_PostVSData[eventId];

  // we handle out-of-memory errors while processing postvs, don't treat it as a fatal error
  ScopedOOMHandle12 oom(m_pDevice);

  D3D12MarkerRegion postvs(m_pDevice->GetQueue(), StringFormat::Fmt("PostVS for %u", eventId));

  D3D12CommandData *cmd = m_pDevice->GetQueue()->GetCommandData();
  const D3D12RenderState &rs = cmd->m_RenderState;

  if(rs.pipe == ResourceId())
  {
    ret.gsout.status = ret.vsout.status = "No pipeline bound";
    return;
  }

  WrappedID3D12PipelineState *origPSO =
      m_pDevice->GetResourceManager()->GetCurrentAs<WrappedID3D12PipelineState>(rs.pipe);

  if(!origPSO || !origPSO->IsGraphics())
  {
    ret.gsout.status = ret.vsout.status = "No graphics pipeline bound";
    return;
  }

  D3D12_EXPANDED_PIPELINE_STATE_STREAM_DESC psoDesc;
  origPSO->Fill(psoDesc);

  if(psoDesc.MS.BytecodeLength > 0)
  {
    InitPostMSBuffers(eventId);
    return;
  }

  if(psoDesc.VS.BytecodeLength == 0)
  {
    ret.gsout.status = ret.vsout.status = "No vertex shader in pipeline";
    return;
  }

  WrappedID3D12Shader *vs = origPSO->VS();

  D3D_PRIMITIVE_TOPOLOGY topo = rs.topo;

  ret.vsout.topo = MakePrimitiveTopology(topo);

  const ActionDescription *action = m_pDevice->GetAction(eventId);

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

  DXBC::DXBCContainer *dxbcVS = vs->GetDXBC();

  RDCASSERT(dxbcVS);

  DXBC::DXBCContainer *dxbcGS = NULL;

  WrappedID3D12Shader *gs = origPSO->GS();

  if(gs)
  {
    dxbcGS = gs->GetDXBC();

    RDCASSERT(dxbcGS);
  }

  DXBC::DXBCContainer *dxbcDS = NULL;

  WrappedID3D12Shader *ds = origPSO->DS();

  if(ds)
  {
    dxbcDS = ds->GetDXBC();

    RDCASSERT(dxbcDS);
  }

  DXBC::DXBCContainer *lastShader = dxbcDS;
  if(dxbcGS)
    lastShader = dxbcGS;

  if(lastShader)
  {
    // put a general error in here in case anything goes wrong fetching VS outputs
    ret.gsout.status =
        "No geometry/tessellation output fetched due to error processing vertex stage.";
  }
  else
  {
    ret.gsout.status = "No geometry and no tessellation shader bound.";
  }

  ID3D12RootSignature *soSig = NULL;

  HRESULT hr = S_OK;

  {
    WrappedID3D12RootSignature *sig =
        m_pDevice->GetResourceManager()->GetCurrentAs<WrappedID3D12RootSignature>(rs.graphics.rootsig);

    D3D12RootSignature rootsig = sig->sig;

    // create a root signature that allows stream out, if necessary
    if((rootsig.Flags & D3D12_ROOT_SIGNATURE_FLAG_ALLOW_STREAM_OUTPUT) == 0)
    {
      rootsig.Flags |= D3D12_ROOT_SIGNATURE_FLAG_ALLOW_STREAM_OUTPUT;

      ID3DBlob *blob = m_pDevice->GetShaderCache()->MakeRootSig(rootsig);

      hr = m_pDevice->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(),
                                          __uuidof(ID3D12RootSignature), (void **)&soSig);
      if(FAILED(hr))
      {
        ret.vsout.status = StringFormat::Fmt(
            "Couldn't enable stream-out in root signature: HRESULT: %s", ToStr(hr).c_str());
        RDCERR("%s", ret.vsout.status.c_str());
        return;
      }

      SAFE_RELEASE(blob);
    }
  }

  rdcarray<D3D12_SO_DECLARATION_ENTRY> sodecls;

  UINT stride = 0;
  int posidx = -1;
  int numPosComponents = 0;

  if(!dxbcVS->GetReflection()->OutputSig.empty())
  {
    for(const SigParameter &sign : dxbcVS->GetReflection()->OutputSig)
    {
      D3D12_SO_DECLARATION_ENTRY decl;

      decl.Stream = 0;
      decl.OutputSlot = 0;

      decl.SemanticName = sign.semanticName.c_str();
      decl.SemanticIndex = sign.semanticIndex;
      decl.StartComponent = 0;
      decl.ComponentCount = sign.compCount & 0xff;

      if(sign.systemValue == ShaderBuiltin::Position)
      {
        posidx = (int)sodecls.size();
        numPosComponents = decl.ComponentCount = 4;
      }

      stride += decl.ComponentCount * sizeof(float);
      sodecls.push_back(decl);
    }

    if(stride == 0)
    {
      RDCERR("Didn't get valid stride! Setting to 4 bytes");
      stride = 4;
    }

    // shift position attribute up to first, keeping order otherwise
    // the same
    if(posidx > 0)
    {
      D3D12_SO_DECLARATION_ENTRY pos = sodecls[posidx];
      sodecls.erase(posidx);
      sodecls.insert(0, pos);
    }

    // set up stream output entries and buffers
    psoDesc.StreamOutput.NumEntries = (UINT)sodecls.size();
    psoDesc.StreamOutput.pSODeclaration = &sodecls[0];
    psoDesc.StreamOutput.NumStrides = 1;
    psoDesc.StreamOutput.pBufferStrides = &stride;
    psoDesc.StreamOutput.RasterizedStream = D3D12_SO_NO_RASTERIZED_STREAM;

    // disable all other shader stages
    psoDesc.HS.BytecodeLength = 0;
    psoDesc.HS.pShaderBytecode = NULL;
    psoDesc.DS.BytecodeLength = 0;
    psoDesc.DS.pShaderBytecode = NULL;
    psoDesc.GS.BytecodeLength = 0;
    psoDesc.GS.pShaderBytecode = NULL;
    psoDesc.PS.BytecodeLength = 0;
    psoDesc.PS.pShaderBytecode = NULL;

    // disable any rasterization/use of output targets
    psoDesc.DepthStencilState.DepthEnable = FALSE;
    psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    psoDesc.DepthStencilState.StencilEnable = FALSE;

    if(soSig)
      psoDesc.pRootSignature = soSig;

    // render as points
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;

    // disable MSAA
    psoDesc.SampleDesc.Count = 1;
    psoDesc.SampleDesc.Quality = 0;

    // disable outputs
    RDCEraseEl(psoDesc.RTVFormats);
    psoDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;

    // for now disable view instancing, unclear if this is legal but it
    psoDesc.ViewInstancing.Flags = D3D12_VIEW_INSTANCING_FLAG_NONE;
    psoDesc.ViewInstancing.ViewInstanceCount = 0;

    ID3D12PipelineState *pipe = NULL;
    hr = m_pDevice->CreatePipeState(psoDesc, &pipe);
    if(FAILED(hr))
    {
      SAFE_RELEASE(soSig);
      ret.vsout.status = StringFormat::Fmt("Couldn't create patched graphics pipeline: HRESULT: %s",
                                           ToStr(hr).c_str());
      RDCERR("%s", ret.vsout.status.c_str());
      return;
    }

    ID3D12Resource *idxBuf = NULL;
    uint64_t idxBufSize = ~0ULL;

    bool recreate = false;
    // we add 64 to account for the stream-out data counter
    uint64_t outputSize = uint64_t(action->numIndices) * action->numInstances * stride + 64;

    if(m_SOBufferSize < outputSize)
    {
      uint64_t oldSize = m_SOBufferSize;
      m_SOBufferSize = CalcMeshOutputSize(m_SOBufferSize, outputSize);
      RDCWARN("Resizing stream-out buffer from %llu to %llu for output data", oldSize,
              m_SOBufferSize);
      recreate = true;
    }

    ID3D12GraphicsCommandListX *list = NULL;

    if(!(action->flags & ActionFlags::Indexed))
    {
      if(recreate)
      {
        m_pDevice->GPUSync();

        uint64_t newSize = m_SOBufferSize;
        if(!CreateSOBuffers())
        {
          ret.vsout.status = StringFormat::Fmt(
              "Vertex output generated %llu bytes of data which ran out of memory", newSize);
          return;
        }
      }

      list = GetDebugManager()->ResetDebugList();

      rs.ApplyState(m_pDevice, list);

      list->SetPipelineState(pipe);

      if(soSig)
      {
        list->SetGraphicsRootSignature(soSig);
        rs.ApplyGraphicsRootElements(list);
      }

      D3D12_STREAM_OUTPUT_BUFFER_VIEW view;
      view.BufferFilledSizeLocation = m_SOBuffer->GetGPUVirtualAddress();
      view.BufferLocation = m_SOBuffer->GetGPUVirtualAddress() + 64;
      view.SizeInBytes = m_SOBufferSize - 64;
      list->SOSetTargets(0, 1, &view);

      list->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
      list->DrawInstanced(action->numIndices, action->numInstances, action->vertexOffset,
                          action->instanceOffset);
    }
    else    // drawcall is indexed
    {
      bytebuf idxdata;
      if(rs.ibuffer.buf != ResourceId() && rs.ibuffer.size > 0)
        GetBufferData(rs.ibuffer.buf, rs.ibuffer.offs + action->indexOffset * rs.ibuffer.bytewidth,
                      RDCMIN(action->numIndices * rs.ibuffer.bytewidth, rs.ibuffer.size), idxdata);

      rdcarray<uint32_t> indices;

      uint16_t *idx16 = (uint16_t *)&idxdata[0];
      uint32_t *idx32 = (uint32_t *)&idxdata[0];

      // only read as many indices as were available in the buffer
      uint32_t numIndices =
          RDCMIN(uint32_t(idxdata.size() / RDCMAX(1, rs.ibuffer.bytewidth)), action->numIndices);

      // grab all unique vertex indices referenced
      for(uint32_t i = 0; i < numIndices; i++)
      {
        uint32_t i32 = rs.ibuffer.bytewidth == 2 ? uint32_t(idx16[i]) : idx32[i];

        auto it = std::lower_bound(indices.begin(), indices.end(), i32);

        if(it != indices.end() && *it == i32)
          continue;

        indices.insert(it - indices.begin(), i32);
      }

      // if we read out of bounds, we'll also have a 0 index being referenced
      // (as 0 is read). Don't insert 0 if we already have 0 though
      if(numIndices < action->numIndices && (indices.empty() || indices[0] != 0))
        indices.insert(0, 0);

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

      outputSize = uint64_t(indices.size() * sizeof(uint32_t) * sizeof(Vec4f));

      if(m_SOBufferSize < outputSize)
      {
        uint64_t oldSize = m_SOBufferSize;
        m_SOBufferSize = CalcMeshOutputSize(m_SOBufferSize, outputSize);
        RDCWARN("Resizing stream-out buffer from %llu to %llu for indices", oldSize, m_SOBufferSize);
        recreate = true;
      }

      if(recreate)
      {
        m_pDevice->GPUSync();

        uint64_t newSize = m_SOBufferSize;
        if(!CreateSOBuffers())
        {
          ret.vsout.status = StringFormat::Fmt(
              "Vertex output generated %llu bytes of data which ran out of memory", newSize);
          return;
        }
      }

      GetDebugManager()->FillBuffer(m_SOPatchedIndexBuffer, 0, &indices[0],
                                    indices.size() * sizeof(uint32_t));

      D3D12_INDEX_BUFFER_VIEW patchedIB;

      patchedIB.BufferLocation = m_SOPatchedIndexBuffer->GetGPUVirtualAddress();
      patchedIB.Format = DXGI_FORMAT_R32_UINT;
      patchedIB.SizeInBytes = UINT(indices.size() * sizeof(uint32_t));

      list = GetDebugManager()->ResetDebugList();

      rs.ApplyState(m_pDevice, list);

      list->SetPipelineState(pipe);

      list->IASetIndexBuffer(&patchedIB);

      if(soSig)
      {
        list->SetGraphicsRootSignature(soSig);
        rs.ApplyGraphicsRootElements(list);
      }

      D3D12_STREAM_OUTPUT_BUFFER_VIEW view;
      view.BufferFilledSizeLocation = m_SOBuffer->GetGPUVirtualAddress();
      view.BufferLocation = m_SOBuffer->GetGPUVirtualAddress() + 64;
      view.SizeInBytes = m_SOBufferSize - 64;
      list->SOSetTargets(0, 1, &view);

      list->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);

      list->DrawIndexedInstanced((UINT)indices.size(), action->numInstances, 0, action->baseVertex,
                                 action->instanceOffset);

      uint32_t stripCutValue = 0;
      if(psoDesc.IBStripCutValue == D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_0xFFFF)
        stripCutValue = 0xffff;
      else if(psoDesc.IBStripCutValue == D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_0xFFFFFFFF)
        stripCutValue = 0xffffffff;

      // rebase existing index buffer to point to the right elements in our stream-out'd
      // vertex buffer
      for(uint32_t i = 0; i < numIndices; i++)
      {
        uint32_t i32 = rs.ibuffer.bytewidth == 2 ? uint32_t(idx16[i]) : idx32[i];

        // preserve primitive restart indices
        if(stripCutValue && i32 == stripCutValue)
          continue;

        if(rs.ibuffer.bytewidth == 2)
          idx16[i] = uint16_t(indexRemap[i32]);
        else
          idx32[i] = uint32_t(indexRemap[i32]);
      }

      idxBuf = NULL;

      if(!idxdata.empty())
      {
        D3D12_RESOURCE_DESC idxBufDesc;
        idxBufDesc.Alignment = 0;
        idxBufDesc.DepthOrArraySize = 1;
        idxBufDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        idxBufDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
        idxBufDesc.Format = DXGI_FORMAT_UNKNOWN;
        idxBufDesc.Height = 1;
        idxBufDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        idxBufDesc.MipLevels = 1;
        idxBufDesc.SampleDesc.Count = 1;
        idxBufDesc.SampleDesc.Quality = 0;
        idxBufDesc.Width = idxdata.size();

        D3D12_HEAP_PROPERTIES heapProps;
        heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
        heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        heapProps.CreationNodeMask = 1;
        heapProps.VisibleNodeMask = 1;

        hr = m_pDevice->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &idxBufDesc,
                                                D3D12_RESOURCE_STATE_GENERIC_READ, NULL,
                                                __uuidof(ID3D12Resource), (void **)&idxBuf);
        RDCASSERTEQUAL(hr, S_OK);

        SetObjName(idxBuf, StringFormat::Fmt("PostVS idxBuf for %u", eventId));

        GetDebugManager()->FillBuffer(idxBuf, 0, &idxdata[0], idxdata.size());
        idxBufSize = idxdata.size();
      }
    }

    D3D12_RESOURCE_BARRIER sobarr = {};
    sobarr.Transition.pResource = m_SOBuffer;
    sobarr.Transition.StateBefore = D3D12_RESOURCE_STATE_STREAM_OUT;
    sobarr.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;

    list->ResourceBarrier(1, &sobarr);

    list->CopyResource(m_SOStagingBuffer, m_SOBuffer);

    // we're done with this after the copy, so we can discard it and reset
    // the counter for the next stream-out
    sobarr.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
    sobarr.Transition.StateAfter = D3D12_RESOURCE_STATE_COMMON;
    list->DiscardResource(m_SOBuffer, NULL);
    list->ResourceBarrier(1, &sobarr);

    GetDebugManager()->SetDescriptorHeaps(list, true, false);

    UINT zeroes[4] = {0, 0, 0, 0};
    list->ClearUnorderedAccessViewUint(GetDebugManager()->GetGPUHandle(STREAM_OUT_UAV),
                                       GetDebugManager()->GetUAVClearHandle(STREAM_OUT_UAV),
                                       m_SOBuffer, zeroes, 0, NULL);

    list->Close();

    ID3D12CommandList *l = list;
    m_pDevice->GetQueue()->ExecuteCommandLists(1, &l);
    m_pDevice->GPUSync();

    GetDebugManager()->ResetDebugAlloc();

    SAFE_RELEASE(pipe);

    byte *byteData = NULL;
    D3D12_RANGE range = {0, (SIZE_T)m_SOBufferSize};
    hr = m_SOStagingBuffer->Map(0, &range, (void **)&byteData);
    m_pDevice->CheckHRESULT(hr);
    if(FAILED(hr))
    {
      RDCERR("Failed to map sobuffer HRESULT: %s", ToStr(hr).c_str());
      ret.vsout.status = "Couldn't read back vertex output data from GPU";
      SAFE_RELEASE(idxBuf);
      SAFE_RELEASE(soSig);
      return;
    }

    range.End = 0;

    uint64_t numBytesWritten = *(uint64_t *)byteData;

    if(numBytesWritten == 0)
    {
      ret = D3D12PostVSData();
      SAFE_RELEASE(idxBuf);
      SAFE_RELEASE(soSig);
      ret.vsout.status = "Vertex output data from GPU contained no vertex data";
      return;
    }

    // skip past the counter
    byteData += 64;

    uint64_t numPrims = numBytesWritten / stride;

    ID3D12Resource *vsoutBuffer = NULL;

    {
      D3D12_RESOURCE_DESC vertBufDesc;
      vertBufDesc.Alignment = 0;
      vertBufDesc.DepthOrArraySize = 1;
      vertBufDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
      vertBufDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
      vertBufDesc.Format = DXGI_FORMAT_UNKNOWN;
      vertBufDesc.Height = 1;
      vertBufDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
      vertBufDesc.MipLevels = 1;
      vertBufDesc.SampleDesc.Count = 1;
      vertBufDesc.SampleDesc.Quality = 0;
      vertBufDesc.Width = numBytesWritten;

      D3D12_HEAP_PROPERTIES heapProps;
      heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
      heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
      heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
      heapProps.CreationNodeMask = 1;
      heapProps.VisibleNodeMask = 1;

      hr = m_pDevice->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &vertBufDesc,
                                              D3D12_RESOURCE_STATE_GENERIC_READ, NULL,
                                              __uuidof(ID3D12Resource), (void **)&vsoutBuffer);
      RDCASSERTEQUAL(hr, S_OK);

      if(vsoutBuffer)
      {
        SetObjName(vsoutBuffer, StringFormat::Fmt("PostVS vsoutBuffer for %u", eventId));
        GetDebugManager()->FillBuffer(vsoutBuffer, 0, byteData, (size_t)numBytesWritten);
      }
    }

    float nearp = 0.1f;
    float farp = 100.0f;

    Vec4f *pos0 = (Vec4f *)byteData;

    bool found = false;

    for(uint64_t i = 1; numPosComponents == 4 && i < numPrims; i++)
    {
      Vec4f *pos = (Vec4f *)(byteData + i * stride);

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

    m_SOStagingBuffer->Unmap(0, &range);

    ret.vsout.buf = vsoutBuffer;
    ret.vsout.vertStride = stride;
    ret.vsout.nearPlane = nearp;
    ret.vsout.farPlane = farp;
    ret.vsout.bufSize = numBytesWritten;

    ret.vsout.useIndices = bool(action->flags & ActionFlags::Indexed);
    ret.vsout.numVerts = action->numIndices;

    ret.vsout.instStride = 0;
    if(action->flags & ActionFlags::Instanced)
      ret.vsout.instStride = uint32_t(numBytesWritten / RDCMAX(1U, action->numInstances));

    ret.vsout.idxBuf = NULL;
    if(ret.vsout.useIndices && idxBuf)
    {
      ret.vsout.idxBuf = idxBuf;
      ret.vsout.idxFmt = rs.ibuffer.bytewidth == 2 ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;
      ret.vsout.idxBufSize = idxBufSize;
    }

    ret.vsout.hasPosOut = posidx >= 0;

    ret.vsout.topo = MakePrimitiveTopology(topo);
  }
  else
  {
    // empty vertex output signature
    ret.vsout.buf = NULL;
    ret.vsout.bufSize = ~0ULL;
    ret.vsout.instStride = 0;
    ret.vsout.vertStride = 0;
    ret.vsout.nearPlane = 0.0f;
    ret.vsout.farPlane = 0.0f;
    ret.vsout.useIndices = false;
    ret.vsout.hasPosOut = false;
    ret.vsout.idxBuf = NULL;
    ret.vsout.idxBufSize = ~0ULL;

    ret.vsout.topo = MakePrimitiveTopology(topo);
  }

  if(lastShader)
  {
    ret.gsout.status.clear();

    stride = 0;
    posidx = -1;
    numPosComponents = 0;

    sodecls.clear();
    for(const SigParameter &sign : lastShader->GetReflection()->OutputSig)
    {
      D3D12_SO_DECLARATION_ENTRY decl;

      // skip streams that aren't rasterized, or if none are rasterized skip non-zero
      if(psoDesc.StreamOutput.RasterizedStream == ~0U)
      {
        if(sign.stream != 0)
          continue;
      }
      else
      {
        if(sign.stream != psoDesc.StreamOutput.RasterizedStream)
          continue;
      }

      decl.Stream = 0;
      decl.OutputSlot = 0;

      decl.SemanticName = sign.semanticName.c_str();
      decl.SemanticIndex = sign.semanticIndex;
      decl.StartComponent = 0;
      decl.ComponentCount = sign.compCount & 0xff;

      if(sign.systemValue == ShaderBuiltin::Position)
      {
        posidx = (int)sodecls.size();
        numPosComponents = decl.ComponentCount = 4;
      }

      stride += decl.ComponentCount * sizeof(float);
      sodecls.push_back(decl);
    }

    // shift position attribute up to first, keeping order otherwise
    // the same
    if(posidx > 0)
    {
      D3D12_SO_DECLARATION_ENTRY pos = sodecls[posidx];
      sodecls.erase(posidx);
      sodecls.insert(0, pos);
    }

    // enable the other shader stages again
    if(origPSO->DS())
      psoDesc.DS = origPSO->DS()->GetDesc();
    if(origPSO->HS())
      psoDesc.HS = origPSO->HS()->GetDesc();
    if(origPSO->GS())
      psoDesc.GS = origPSO->GS()->GetDesc();

    // configure new SO declarations
    psoDesc.StreamOutput.NumEntries = (UINT)sodecls.size();
    psoDesc.StreamOutput.pSODeclaration = &sodecls[0];
    psoDesc.StreamOutput.NumStrides = 1;
    psoDesc.StreamOutput.pBufferStrides = &stride;

    // we're using the same topology this time
    psoDesc.PrimitiveTopologyType = origPSO->graphics->PrimitiveTopologyType;

    ID3D12PipelineState *pipe = NULL;
    hr = m_pDevice->CreatePipeState(psoDesc, &pipe);
    if(FAILED(hr))
    {
      SAFE_RELEASE(soSig);
      ret.gsout.status = StringFormat::Fmt("Couldn't create patched graphics pipeline: HRESULT: %s",
                                           ToStr(hr).c_str());
      RDCERR("%s", ret.gsout.status.c_str());
      return;
    }

    D3D12_STREAM_OUTPUT_BUFFER_VIEW view;

    ID3D12GraphicsCommandListX *list = NULL;

    view.BufferFilledSizeLocation = m_SOBuffer->GetGPUVirtualAddress();
    view.BufferLocation = m_SOBuffer->GetGPUVirtualAddress() + 64;
    view.SizeInBytes = m_SOBufferSize - 64;
    // draws with multiple instances must be replayed one at a time so we can record the number of
    // primitives from each action, as due to expansion this can vary per-instance.
    if(action->numInstances > 1)
    {
      list = GetDebugManager()->ResetDebugList();

      rs.ApplyState(m_pDevice, list);

      list->SetPipelineState(pipe);

      if(soSig)
      {
        list->SetGraphicsRootSignature(soSig);
        rs.ApplyGraphicsRootElements(list);
      }

      view.BufferFilledSizeLocation = m_SOBuffer->GetGPUVirtualAddress();
      view.BufferLocation = m_SOBuffer->GetGPUVirtualAddress() + 64;
      view.SizeInBytes = m_SOBufferSize - 64;

      // do a dummy draw to make sure we have enough space in the output buffer
      list->SOSetTargets(0, 1, &view);

      list->BeginQuery(m_SOQueryHeap, D3D12_QUERY_TYPE_SO_STATISTICS_STREAM0, 0);

      // because the result is expanded we don't have to remap index buffers or anything
      if(action->flags & ActionFlags::Indexed)
      {
        list->DrawIndexedInstanced(action->numIndices, action->numInstances, action->indexOffset,
                                   action->baseVertex, action->instanceOffset);
      }
      else
      {
        list->DrawInstanced(action->numIndices, action->numInstances, action->vertexOffset,
                            action->instanceOffset);
      }

      list->EndQuery(m_SOQueryHeap, D3D12_QUERY_TYPE_SO_STATISTICS_STREAM0, 0);

      list->ResolveQueryData(m_SOQueryHeap, D3D12_QUERY_TYPE_SO_STATISTICS_STREAM0, 0, 1,
                             m_SOStagingBuffer, 0);

      list->Close();

      ID3D12CommandList *l = list;
      m_pDevice->GetQueue()->ExecuteCommandLists(1, &l);
      m_pDevice->GPUSync();

      // check that things are OK, and resize up if needed
      D3D12_RANGE range;
      range.Begin = 0;
      range.End = (SIZE_T)sizeof(D3D12_QUERY_DATA_SO_STATISTICS);

      D3D12_QUERY_DATA_SO_STATISTICS *data;
      hr = m_SOStagingBuffer->Map(0, &range, (void **)&data);
      m_pDevice->CheckHRESULT(hr);
      if(FAILED(hr))
      {
        RDCERR("Couldn't get SO statistics data");
        ret.gsout.status =
            StringFormat::Fmt("Couldn't get stream-out statistics: HRESULT: %s", ToStr(hr).c_str());
        return;
      }

      D3D12_QUERY_DATA_SO_STATISTICS result = *data;

      range.End = 0;
      m_SOStagingBuffer->Unmap(0, &range);

      // reserve space for enough 'buffer filled size' locations
      UINT64 SizeCounterBytes = AlignUp(uint64_t(action->numInstances * sizeof(UINT64)), 64ULL);
      uint64_t outputSize = SizeCounterBytes + result.PrimitivesStorageNeeded * 3 * stride;

      if(m_SOBufferSize < outputSize)
      {
        uint64_t oldSize = m_SOBufferSize;
        m_SOBufferSize = CalcMeshOutputSize(m_SOBufferSize, outputSize);
        RDCWARN("Resizing stream-out buffer from %llu to %llu for output", oldSize, m_SOBufferSize);

        uint64_t newSize = m_SOBufferSize;
        if(!CreateSOBuffers())
        {
          ret.gsout.status = StringFormat::Fmt(
              "Geometry/tessellation output generated %llu bytes of data which ran out of memory",
              newSize);
          return;
        }
      }

      GetDebugManager()->ResetDebugAlloc();

      // now do the actual stream out
      list = GetDebugManager()->ResetDebugList();

      // first need to reset the counter byte values which may have either been written to above, or
      // are newly created
      {
        D3D12_RESOURCE_BARRIER sobarr = {};
        sobarr.Transition.pResource = m_SOBuffer;
        sobarr.Transition.StateBefore = D3D12_RESOURCE_STATE_STREAM_OUT;
        sobarr.Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;

        list->ResourceBarrier(1, &sobarr);

        GetDebugManager()->SetDescriptorHeaps(list, true, false);

        UINT zeroes[4] = {0, 0, 0, 0};
        list->ClearUnorderedAccessViewUint(GetDebugManager()->GetGPUHandle(STREAM_OUT_UAV),
                                           GetDebugManager()->GetUAVClearHandle(STREAM_OUT_UAV),
                                           m_SOBuffer, zeroes, 0, NULL);

        std::swap(sobarr.Transition.StateBefore, sobarr.Transition.StateAfter);
        list->ResourceBarrier(1, &sobarr);
      }

      rs.ApplyState(m_pDevice, list);

      list->SetPipelineState(pipe);

      if(soSig)
      {
        list->SetGraphicsRootSignature(soSig);
        rs.ApplyGraphicsRootElements(list);
      }

      view.BufferLocation = m_SOBuffer->GetGPUVirtualAddress() + SizeCounterBytes;
      view.SizeInBytes = m_SOBufferSize - SizeCounterBytes;

      // do incremental draws to get the output size. We have to do this O(N^2) style because
      // there's no way to replay only a single instance. We have to replay 1, 2, 3, ... N instances
      // and count the total number of verts each time, then we can see from the difference how much
      // each instance wrote.
      for(uint32_t inst = 1; inst <= action->numInstances; inst++)
      {
        if(action->flags & ActionFlags::Indexed)
        {
          view.BufferFilledSizeLocation =
              m_SOBuffer->GetGPUVirtualAddress() + (inst - 1) * sizeof(UINT64);
          list->SOSetTargets(0, 1, &view);
          list->DrawIndexedInstanced(action->numIndices, inst, action->indexOffset,
                                     action->baseVertex, action->instanceOffset);
        }
        else
        {
          view.BufferFilledSizeLocation =
              m_SOBuffer->GetGPUVirtualAddress() + (inst - 1) * sizeof(UINT64);
          list->SOSetTargets(0, 1, &view);
          list->DrawInstanced(action->numIndices, inst, action->vertexOffset, action->instanceOffset);
        }

        // Instanced draws with a wild number of instances can hang the GPU, sync after every 1000
        if((inst % 1000) == 0)
        {
          list->Close();

          l = list;
          m_pDevice->GetQueue()->ExecuteCommandLists(1, &l);
          m_pDevice->GPUSync();

          GetDebugManager()->ResetDebugAlloc();

          list = GetDebugManager()->ResetDebugList();

          rs.ApplyState(m_pDevice, list);

          list->SetPipelineState(pipe);

          if(soSig)
          {
            list->SetGraphicsRootSignature(soSig);
            rs.ApplyGraphicsRootElements(list);
          }
        }
      }

      list->Close();

      l = list;
      m_pDevice->GetQueue()->ExecuteCommandLists(1, &l);
      m_pDevice->GPUSync();

      GetDebugManager()->ResetDebugAlloc();

      // the last draw will have written the actual data we want into the buffer
    }
    else
    {
      // this only loops if we find from a query that we need to resize up
      while(true)
      {
        list = GetDebugManager()->ResetDebugList();

        rs.ApplyState(m_pDevice, list);

        list->SetPipelineState(pipe);

        if(soSig)
        {
          list->SetGraphicsRootSignature(soSig);
          rs.ApplyGraphicsRootElements(list);
        }

        view.BufferFilledSizeLocation = m_SOBuffer->GetGPUVirtualAddress();
        view.BufferLocation = m_SOBuffer->GetGPUVirtualAddress() + 64;
        view.SizeInBytes = m_SOBufferSize - 64;

        list->SOSetTargets(0, 1, &view);

        list->BeginQuery(m_SOQueryHeap, D3D12_QUERY_TYPE_SO_STATISTICS_STREAM0, 0);

        // because the result is expanded we don't have to remap index buffers or anything
        if(action->flags & ActionFlags::Indexed)
        {
          list->DrawIndexedInstanced(action->numIndices, action->numInstances, action->indexOffset,
                                     action->baseVertex, action->instanceOffset);
        }
        else
        {
          list->DrawInstanced(action->numIndices, action->numInstances, action->vertexOffset,
                              action->instanceOffset);
        }

        list->EndQuery(m_SOQueryHeap, D3D12_QUERY_TYPE_SO_STATISTICS_STREAM0, 0);

        list->ResolveQueryData(m_SOQueryHeap, D3D12_QUERY_TYPE_SO_STATISTICS_STREAM0, 0, 1,
                               m_SOStagingBuffer, 0);

        list->Close();

        ID3D12CommandList *l = list;
        m_pDevice->GetQueue()->ExecuteCommandLists(1, &l);
        m_pDevice->GPUSync();

        // check that things are OK, and resize up if needed
        D3D12_RANGE range;
        range.Begin = 0;
        range.End = (SIZE_T)sizeof(D3D12_QUERY_DATA_SO_STATISTICS);

        D3D12_QUERY_DATA_SO_STATISTICS *data;
        hr = m_SOStagingBuffer->Map(0, &range, (void **)&data);
        m_pDevice->CheckHRESULT(hr);
        if(FAILED(hr))
        {
          RDCERR("Couldn't get SO statistics data");
          ret.gsout.status = StringFormat::Fmt("Couldn't get stream-out statistics: HRESULT: %s",
                                               ToStr(hr).c_str());
          return;
        }

        uint64_t outputSize = data->PrimitivesStorageNeeded * 3 * stride;

        if(m_SOBufferSize < outputSize)
        {
          uint64_t oldSize = m_SOBufferSize;
          m_SOBufferSize = CalcMeshOutputSize(m_SOBufferSize, outputSize);
          RDCWARN("Resizing stream-out buffer from %llu to %llu for output", oldSize, m_SOBufferSize);

          uint64_t newSize = m_SOBufferSize;
          if(!CreateSOBuffers())
          {
            ret.gsout.status = StringFormat::Fmt(
                "Geometry/tessellation output generated %llu bytes of data which ran out of memory",
                newSize);
            return;
          }

          continue;
        }

        range.End = 0;
        m_SOStagingBuffer->Unmap(0, &range);

        GetDebugManager()->ResetDebugAlloc();

        break;
      }
    }

    list = GetDebugManager()->ResetDebugList();

    D3D12_RESOURCE_BARRIER sobarr = {};
    sobarr.Transition.pResource = m_SOBuffer;
    sobarr.Transition.StateBefore = D3D12_RESOURCE_STATE_STREAM_OUT;
    sobarr.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;

    list->ResourceBarrier(1, &sobarr);

    list->CopyResource(m_SOStagingBuffer, m_SOBuffer);

    // we're done with this after the copy, so we can discard it and reset
    // the counter for the next stream-out
    sobarr.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
    sobarr.Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    list->DiscardResource(m_SOBuffer, NULL);
    list->ResourceBarrier(1, &sobarr);

    GetDebugManager()->SetDescriptorHeaps(list, true, false);

    UINT zeroes[4] = {0, 0, 0, 0};
    list->ClearUnorderedAccessViewUint(GetDebugManager()->GetGPUHandle(STREAM_OUT_UAV),
                                       GetDebugManager()->GetUAVClearHandle(STREAM_OUT_UAV),
                                       m_SOBuffer, zeroes, 0, NULL);

    list->Close();

    ID3D12CommandList *l = list;
    m_pDevice->GetQueue()->ExecuteCommandLists(1, &l);
    m_pDevice->GPUSync();

    GetDebugManager()->ResetDebugAlloc();

    SAFE_RELEASE(pipe);

    byte *byteData = NULL;
    D3D12_RANGE range = {0, (SIZE_T)m_SOBufferSize};
    hr = m_SOStagingBuffer->Map(0, &range, (void **)&byteData);
    m_pDevice->CheckHRESULT(hr);
    if(FAILED(hr))
    {
      RDCERR("Failed to map sobuffer HRESULT: %s", ToStr(hr).c_str());
      ret.gsout.status = "Couldn't read back geometry/tessellation output data from GPU";
      SAFE_RELEASE(soSig);
      return;
    }

    range.End = 0;

    uint64_t *counters = (uint64_t *)byteData;

    uint64_t numBytesWritten = 0;
    rdcarray<D3D12PostVSData::InstData> instData;
    if(action->numInstances > 1)
    {
      uint64_t prevByteCount = 0;

      for(uint32_t inst = 0; inst < action->numInstances; inst++)
      {
        uint64_t byteCount = counters[inst];

        D3D12PostVSData::InstData d;
        d.numVerts = uint32_t((byteCount - prevByteCount) / stride);
        d.bufOffset = prevByteCount;
        prevByteCount = byteCount;

        instData.push_back(d);
      }

      numBytesWritten = prevByteCount;
    }
    else
    {
      numBytesWritten = counters[0];
    }

    if(numBytesWritten == 0)
    {
      SAFE_RELEASE(soSig);
      ret.gsout.status = "No detectable output generated by geometry/tessellation shaders";
      m_SOStagingBuffer->Unmap(0, &range);
      return;
    }

    // skip past the counter(s)
    byteData += (view.BufferLocation - m_SOBuffer->GetGPUVirtualAddress());

    uint64_t numVerts = numBytesWritten / stride;

    ID3D12Resource *gsoutBuffer = NULL;

    {
      D3D12_RESOURCE_DESC vertBufDesc;
      vertBufDesc.Alignment = 0;
      vertBufDesc.DepthOrArraySize = 1;
      vertBufDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
      vertBufDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
      vertBufDesc.Format = DXGI_FORMAT_UNKNOWN;
      vertBufDesc.Height = 1;
      vertBufDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
      vertBufDesc.MipLevels = 1;
      vertBufDesc.SampleDesc.Count = 1;
      vertBufDesc.SampleDesc.Quality = 0;
      vertBufDesc.Width = numBytesWritten;

      D3D12_HEAP_PROPERTIES heapProps;
      heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
      heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
      heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
      heapProps.CreationNodeMask = 1;
      heapProps.VisibleNodeMask = 1;

      hr = m_pDevice->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &vertBufDesc,
                                              D3D12_RESOURCE_STATE_GENERIC_READ, NULL,
                                              __uuidof(ID3D12Resource), (void **)&gsoutBuffer);
      RDCASSERTEQUAL(hr, S_OK);

      if(gsoutBuffer)
      {
        SetObjName(gsoutBuffer, StringFormat::Fmt("PostVS gsoutBuffer for %u", eventId));
        GetDebugManager()->FillBuffer(gsoutBuffer, 0, byteData, (size_t)numBytesWritten);
      }
    }

    float nearp = 0.1f;
    float farp = 100.0f;

    Vec4f *pos0 = (Vec4f *)byteData;

    bool found = false;

    for(UINT64 i = 1; numPosComponents == 4 && i < numVerts; i++)
    {
      Vec4f *pos = (Vec4f *)(byteData + i * stride);

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

    m_SOStagingBuffer->Unmap(0, &range);

    ret.gsout.buf = gsoutBuffer;
    ret.gsout.bufSize = numBytesWritten;
    ret.gsout.instStride = 0;
    if(action->flags & ActionFlags::Instanced)
      ret.gsout.instStride = uint32_t(numBytesWritten / RDCMAX(1U, action->numInstances));
    ret.gsout.vertStride = stride;
    ret.gsout.nearPlane = nearp;
    ret.gsout.farPlane = farp;
    ret.gsout.useIndices = false;
    ret.gsout.hasPosOut = posidx >= 0;
    ret.gsout.idxBuf = NULL;
    ret.gsout.idxBufSize = ~0ULL;

    topo = lastShader->GetOutputTopology();

    // streamout expands strips unfortunately
    if(topo == D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP)
      topo = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    else if(topo == D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP)
      topo = D3D11_PRIMITIVE_TOPOLOGY_LINELIST;
    else if(topo == D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP_ADJ)
      topo = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST_ADJ;
    else if(topo == D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP_ADJ)
      topo = D3D11_PRIMITIVE_TOPOLOGY_LINELIST_ADJ;

    ret.gsout.topo = MakePrimitiveTopology(topo);

    ret.gsout.numVerts = (uint32_t)numVerts;

    if(action->flags & ActionFlags::Instanced)
      ret.gsout.numVerts /= RDCMAX(1U, action->numInstances);

    ret.gsout.instData = instData;
  }

  SAFE_RELEASE(soSig);
}

struct D3D12InitPostVSCallback : public D3D12ActionCallback
{
  D3D12InitPostVSCallback(WrappedID3D12Device *dev, D3D12Replay *replay,
                          const rdcarray<uint32_t> &events)
      : m_pDevice(dev), m_Replay(replay), m_Events(events)
  {
    m_pDevice->GetQueue()->GetCommandData()->m_ActionCallback = this;
  }
  ~D3D12InitPostVSCallback() { m_pDevice->GetQueue()->GetCommandData()->m_ActionCallback = NULL; }
  void PreDraw(uint32_t eid, ID3D12GraphicsCommandListX *cmd) override
  {
    if(m_Events.contains(eid))
      m_Replay->InitPostVSBuffers(eid);
  }

  bool PostDraw(uint32_t eid, ID3D12GraphicsCommandListX *cmd) override { return false; }
  void PostRedraw(uint32_t eid, ID3D12GraphicsCommandListX *cmd) override {}
  // Dispatches don't rasterize, so do nothing
  void PreDispatch(uint32_t eid, ID3D12GraphicsCommandListX *cmd) override {}
  bool PostDispatch(uint32_t eid, ID3D12GraphicsCommandListX *cmd) override { return false; }
  void PostRedispatch(uint32_t eid, ID3D12GraphicsCommandListX *cmd) override {}
  // Ditto copy/etc
  void PreMisc(uint32_t eid, ActionFlags flags, ID3D12GraphicsCommandListX *cmd) {}
  bool PostMisc(uint32_t eid, ActionFlags flags, ID3D12GraphicsCommandListX *cmd) { return false; }
  void PostRemisc(uint32_t eid, ActionFlags flags, ID3D12GraphicsCommandListX *cmd) {}
  void PreCloseCommandList(ID3D12GraphicsCommandListX *cmd) override {}
  void AliasEvent(uint32_t primary, uint32_t alias) override
  {
    if(m_Events.contains(primary))
      m_Replay->AliasPostVSBuffers(primary, alias);
  }

  WrappedID3D12Device *m_pDevice;
  D3D12Replay *m_Replay;
  const rdcarray<uint32_t> &m_Events;
};

void D3D12Replay::InitPostVSBuffers(const rdcarray<uint32_t> &events)
{
  // first we must replay up to the first event without replaying it. This ensures any
  // non-command buffer calls like memory unmaps etc all happen correctly before this
  // command buffer
  m_pDevice->ReplayLog(0, events.front(), eReplay_WithoutDraw);

  D3D12InitPostVSCallback cb(m_pDevice, this, events);

  // now we replay the events, which are guaranteed (because we generated them in
  // GetPassEvents above) to come from the same command buffer, so the event IDs are
  // still locally continuous, even if we jump into replaying.
  m_pDevice->ReplayLog(events.front(), events.back(), eReplay_Full);
}

MeshFormat D3D12Replay::GetPostVSBuffers(uint32_t eventId, uint32_t instID, uint32_t viewID,
                                         MeshDataStage stage)
{
  // go through any aliasing
  if(m_PostVSAlias.find(eventId) != m_PostVSAlias.end())
    eventId = m_PostVSAlias[eventId];

  D3D12PostVSData postvs;
  RDCEraseEl(postvs);

  // no multiview support
  (void)viewID;

  if(m_PostVSData.find(eventId) != m_PostVSData.end())
    postvs = m_PostVSData[eventId];

  const D3D12PostVSData::StageData &s = postvs.GetStage(stage);

  MeshFormat ret;

  if(s.useIndices && s.idxBuf != NULL)
  {
    ret.indexResourceId = GetResID(s.idxBuf);
    ret.indexByteStride = s.idxFmt == DXGI_FORMAT_R16_UINT ? 2 : 4;
    ret.indexByteSize = s.idxBufSize;
  }
  else if(s.useIndices)
  {
    // indicate that an index buffer is still needed
    ret.indexByteStride = 4;
  }
  else
  {
    ret.indexResourceId = ResourceId();
    ret.indexByteStride = 0;
  }
  ret.indexByteOffset = s.idxOffset;
  ret.baseVertex = 0;

  if(s.buf != NULL)
  {
    ret.vertexResourceId = GetResID(s.buf);
    ret.vertexByteSize = s.bufSize;
  }
  else
  {
    ret.vertexResourceId = ResourceId();
    ret.vertexByteSize = 0;
  }

  ret.vertexByteOffset = s.instStride * instID;
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

  const ActionDescription *action = m_pDevice->GetAction(eventId);

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
            s.instData[i].ampDispatchSizeX,
            s.instData[i].ampDispatchSizeYZ.y,
            s.instData[i].ampDispatchSizeYZ.z,
        };
    }
  }
  else if(instID < s.instData.size())
  {
    D3D12PostVSData::InstData inst = s.instData[instID];

    ret.vertexByteOffset = inst.bufOffset;
    ret.numIndices = inst.numVerts;
  }

  ret.status = s.status;

  return ret;
}
