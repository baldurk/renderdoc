/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2021-2023 Baldur Karlsson
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
#include "driver/shaders/dxbc/dxbc_bytecode_editor.h"
#include "driver/shaders/dxil/dxil_bytecode_editor.h"
#include "driver/shaders/dxil/dxil_common.h"
#include "d3d12_command_queue.h"
#include "d3d12_debug.h"
#include "d3d12_device.h"
#include "d3d12_replay.h"
#include "d3d12_resources.h"
#include "d3d12_shader_cache.h"

RDOC_CONFIG(rdcstr, D3D12_Debug_FeedbackDumpDirPath, "",
            "Path to dump bindless feedback annotation generated DXBC/DXIL files.");
RDOC_CONFIG(
    bool, D3D12_BindlessFeedback, true,
    "Enable fetching from GPU which descriptors were dynamically used in descriptor arrays.");

struct D3D12FeedbackKey
{
  DXBCBytecode::OperandType type;
  Bindpoint bind;

  bool operator<(const D3D12FeedbackKey &key) const
  {
    if(type != key.type)
      return type < key.type;
    return bind < key.bind;
  }
};

struct D3D12FeedbackSlot
{
public:
  D3D12FeedbackSlot()
  {
    slot = 0;
    used = 0;
  }
  void SetSlot(uint32_t s) { slot = s; }
  void SetStaticUsed() { used = 0x1; }
  bool StaticUsed() const { return used != 0x0; }
  uint32_t Slot() const { return slot; }
private:
  uint32_t slot : 31;
  uint32_t used : 1;
};

static const uint32_t numReservedSlots = 4;
static const uint32_t magicFeedbackValue = 0xbeebf330;
// keep the 4 lower bits to store the type of resource for direct heap access bindless
static const uint32_t magicFeedbackTypeMask = 0x0000000f;

static D3D12FeedbackKey GetDirectHeapAccessKey()
{
  Bindpoint bind;
  bind.bind = -1;
  bind.bindset = -1;
  D3D12FeedbackKey key;
  key.type = DXBCBytecode::OperandType::TYPE_RESOURCE;
  key.bind = bind;
  return key;
}

static bool AnnotateDXBCShader(const DXBC::DXBCContainer *dxbc, uint32_t space,
                               const std::map<D3D12FeedbackKey, D3D12FeedbackSlot> &slots,
                               uint32_t maxSlot, bytebuf &editedBlob)
{
  using namespace DXBCBytecode;
  using namespace DXBCBytecode::Edit;

  ProgramEditor editor(dxbc, editedBlob);

  // get ourselves a temp
  uint32_t t = editor.AddTemp();

  // declare the output UAV
  ResourceDecl desc;
  desc.compType = CompType::UInt;
  desc.type = TextureType::Buffer;
  desc.raw = true;

  ResourceIdentifier u = {~0U, ~0U};

  for(size_t i = 0; i < editor.GetNumInstructions(); i++)
  {
    const Operation &op = editor.GetInstruction(i);

    for(const Operand &operand : op.operands)
    {
      if(operand.type != TYPE_RESOURCE && operand.type != TYPE_UNORDERED_ACCESS_VIEW)
        continue;

      const Declaration *decl =
          editor.FindDeclaration(operand.type, (uint32_t)operand.indices[0].index);

      if(!decl)
      {
        RDCERR("Couldn't find declaration for %d operand identifier %u", operand.type,
               (uint32_t)operand.indices[0].index);
        continue;
      }

      // ignore non-arrayed declarations
      if(decl->operand.indices[1].index == decl->operand.indices[2].index)
        continue;

      bool dynamic = operand.indices[1].relative;

      Operand idx;

      if(dynamic)
      {
        // the operand should be relative addressing like r0.x + 6 for a t6 resource being indexed
        // with [r0.x]
        RDCASSERT(operand.indices[1].index == decl->operand.indices[1].index);

        idx = operand.indices[1].operand;

        // should be getting a scalar index
        if(idx.comps[1] != 0xff || idx.comps[2] != 0xff || idx.comps[3] != 0xff)
        {
          RDCERR("Unexpected vector index for resource: %s",
                 operand.toString(dxbc->GetReflection(), ToString::None).c_str());
          continue;
        }
      }
      else
      {
        // shader could be indexing into an array with a fixed index. Handle that by subtracting the
        // base manually
        RDCASSERT(operand.indices[1].index >= decl->operand.indices[1].index);

        idx = imm(uint32_t(operand.indices[1].index - decl->operand.indices[1].index));
      }

      D3D12FeedbackKey key;
      key.type = operand.type;
      key.bind.bindset = decl->space;
      key.bind.bind = (int32_t)decl->operand.indices[1].index;

      auto it = slots.find(key);

      if(it == slots.end())
      {
        RDCERR("Couldn't find reserved base slot for %d at space %u and bind %u", key.type,
               key.bind.bindset, key.bind.bind);
        continue;
      }

      if(u.first == ~0U && u.second == ~0U)
        u = editor.DeclareUAV(desc, space, 0, 0);

      // clamp to number of slots
      editor.InsertOperation(i++, oper(OPCODE_UMIN, {temp(t).swizzle(0), imm(maxSlot), idx}));
      // resource base plus index
      editor.InsertOperation(
          i++, oper(OPCODE_IADD, {temp(t).swizzle(0), temp(t).swizzle(0), imm(it->second.Slot())}));
      // multiply by 4 for byte index
      editor.InsertOperation(i++,
                             oper(OPCODE_ISHL, {temp(t).swizzle(0), temp(t).swizzle(0), imm(2)}));
      // atomic or the slot
      editor.InsertOperation(
          i++, oper(OPCODE_ATOMIC_OR, {uav(u), temp(t).swizzle(0), imm(magicFeedbackValue)}));

      // only one resource operand per instruction
      break;
    }
  }

  if(u.first != ~0U || u.second != ~0U)
  {
    editor.InsertOperation(0, oper(OPCODE_MOV, {temp(t).swizzle(0), imm(0)}));
    editor.InsertOperation(
        1, oper(OPCODE_ATOMIC_OR, {uav(u), temp(t).swizzle(0), imm(magicFeedbackValue)}));
    return true;
  }

  return false;
}

static bool AnnotateDXILShader(const DXBC::DXBCContainer *dxbc, uint32_t space,
                               const std::map<D3D12FeedbackKey, D3D12FeedbackSlot> &slots,
                               uint32_t maxSlot, bytebuf &editedBlob)
{
  using namespace DXIL;

  ProgramEditor editor(dxbc, editedBlob);

  const Type *i32 = editor.GetInt32Type();
  const Type *i8 = editor.GetInt8Type();
  const Type *i1 = editor.GetBoolType();

  const Type *handleType = editor.CreateNamedStructType(
      "dx.types.Handle", {editor.CreatePointerType(i8, Type::PointerAddrSpace::Default)});
  const Function *createHandle = editor.GetFunctionByName("dx.op.createHandle");
  const Function *createHandleFromBinding =
      editor.GetFunctionByName("dx.op.createHandleFromBinding");
  const Function *createHandleFromHeap = editor.GetFunctionByName("dx.op.createHandleFromHeap");
  const Function *annotateHandle = editor.GetFunctionByName("dx.op.annotateHandle");
  bool isShaderModel6_6OrAbove =
      dxbc->m_Version.Major > 6 || (dxbc->m_Version.Major == 6 && dxbc->m_Version.Minor >= 6);

  // if we don't have the handle type then this shader can't use any dynamically! we have no
  // feedback to get
  if(!handleType || (!createHandle && !isShaderModel6_6OrAbove) ||
     (isShaderModel6_6OrAbove && !createHandleFromHeap && !createHandleFromBinding))
    return false;

  // Create createHandleFromBinding we'll need to create the feedback UAV
  if(!createHandleFromBinding && isShaderModel6_6OrAbove)
  {
    const Type *resBindType = editor.CreateNamedStructType("dx.types.ResBind", {i32, i32, i32, i8});
    createHandleFromBinding = editor.DeclareFunction("dx.op.createHandleFromBinding", handleType,
                                                     {i32, resBindType, i32, i1},
                                                     Attribute::NoUnwind | Attribute::ReadNone);
  }

  // get the functions we'll need
  const Function *atomicBinOp = editor.DeclareFunction("dx.op.atomicBinOp.i32", i32,
                                                       {i32, handleType, i32, i32, i32, i32, i32},
                                                       Attribute::NoUnwind | Attribute::ReadNone);
  const Function *binOp = editor.DeclareFunction("dx.op.binary.i32", i32, {i32, i32, i32},
                                                 Attribute::NoUnwind | Attribute::ReadNone);

  // while we're iterating through the metadata to add our UAV, we'll also note the shader-local
  // register IDs of each SRV/UAV with slots, and record the base slot in this array for easy access
  // later when annotating dx.op.createHandle calls. We also need to know the base register because
  // the index dxc provides is register-relative
  rdcarray<rdcpair<uint32_t, int32_t>> srvBaseSlots, uavBaseSlots;

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

    Metadata *srvs = reslist->children[0];
    Metadata *uavs = reslist->children[1];
    // if there isn't a UAV list, create an empty one so we can add our own
    if(!uavs)
      uavs = reslist->children[1] = editor.CreateMetadata();

    D3D12FeedbackKey key;

    key.type = DXBCBytecode::TYPE_RESOURCE;

    for(size_t i = 0; srvs && i < srvs->children.size(); i++)
    {
      // each SRV child should have a fixed format
      const Metadata *srv = srvs->children[i];
      const Constant *slot = cast<Constant>(srv->children[(size_t)ResField::ID]->value);
      const Constant *srvSpace = cast<Constant>(srv->children[(size_t)ResField::Space]->value);
      const Constant *reg = cast<Constant>(srv->children[(size_t)ResField::RegBase]->value);

      if(!slot)
      {
        RDCWARN("Unexpected non-constant slot ID in SRV");
        continue;
      }

      if(!srvSpace)
      {
        RDCWARN("Unexpected non-constant register space in SRV");
        continue;
      }

      if(!reg)
      {
        RDCWARN("Unexpected non-constant register base in SRV");
        continue;
      }

      uint32_t id = slot->getU32();
      key.bind.bindset = srvSpace->getU32();
      key.bind.bind = reg->getU32();

      // ensure every valid ID has an index, even if it's 0
      srvBaseSlots.resize_for_index(id);

      auto it = slots.find(key);

      // not annotated
      if(it == slots.end())
        continue;

      // static used i.e. not arrayed? ignore
      if(it->second.StaticUsed())
        continue;

      uint32_t feedbackSlot = it->second.Slot();

      // we assume all feedback slots are non-zero, so that a 0 base slot can be used as an
      // identifier for 'this resource isn't annotated'
      RDCASSERT(feedbackSlot > 0);

      srvBaseSlots[id] = {feedbackSlot, key.bind.bind};
    }

    key.type = DXBCBytecode::TYPE_UNORDERED_ACCESS_VIEW;

    for(size_t i = 0; i < uavs->children.size(); i++)
    {
      // each UAV child should have a fixed format, [0] is the reg ID and I think this should always
      // be == the index
      const Metadata *uav = uavs->children[i];
      const Constant *slot = cast<Constant>(uav->children[(size_t)ResField::ID]->value);
      const Constant *uavSpace = cast<Constant>(uav->children[(size_t)ResField::Space]->value);
      const Constant *reg = cast<Constant>(uav->children[(size_t)ResField::RegBase]->value);

      if(!slot)
      {
        RDCWARN("Unexpected non-constant slot ID in UAV");
        continue;
      }

      if(!uavSpace)
      {
        RDCWARN("Unexpected non-constant register space in UAV");
        continue;
      }

      if(!reg)
      {
        RDCWARN("Unexpected non-constant register base in UAV");
        continue;
      }

      RDCASSERT(slot->getU32() == i);

      uint32_t id = slot->getU32();
      regSlot = RDCMAX(id + 1, regSlot);

      // ensure every valid ID has an index, even if it's 0
      uavBaseSlots.resize_for_index(id);

      key.bind.bindset = uavSpace->getU32();
      key.bind.bind = reg->getU32();

      auto it = slots.find(key);

      // not annotated
      if(it == slots.end())
        continue;

      // static used i.e. not arrayed? ignore
      if(it->second.StaticUsed())
        continue;

      uint32_t feedbackSlot = it->second.Slot();

      // we assume all feedback slots are non-zero, so that a 0 base slot can be used as an
      // identifier for 'this resource isn't annotated'
      RDCASSERT(feedbackSlot > 0);

      uavBaseSlots[id] = {feedbackSlot, key.bind.bind};
    }

    Constant rwundef;
    rwundef.type = rwptr;
    rwundef.setUndef(true);

    // create the new UAV record
    Metadata *feedbackuav = editor.CreateMetadata();
    feedbackuav->children = {
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

    uavs->children.push_back(feedbackuav);
  }

  rdcstr entryName;
  // add the entry point tags
  {
    Metadata *entryPoints = editor.GetMetadataByName("dx.entryPoints");

    if(!entryPoints)
    {
      RDCERR("Couldn't find entry point list");
      return false;
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
    size_t existingTag = 0;
    for(size_t t = 0; taglist && t < taglist->children.size(); t += 2)
    {
      RDCASSERT(taglist->children[t]->isConstant);
      if(cast<Constant>(taglist->children[t]->value)->getU32() ==
         (uint32_t)ShaderEntryTag::ShaderFlags)
      {
        shaderFlagsTag = taglist->children[t];
        shaderFlagsData = taglist->children[t + 1];
        existingTag = t + 1;
        break;
      }
    }

    uint32_t shaderFlagsValue =
        shaderFlagsData ? cast<Constant>(shaderFlagsData->value)->getU32() : 0U;

    // raw and structured buffers
    shaderFlagsValue |= 0x10;

    if(editor.GetShaderType() != DXBC::ShaderType::Compute &&
       editor.GetShaderType() != DXBC::ShaderType::Pixel)
    {
      // UAVs on non-PS/CS stages
      shaderFlagsValue |= 0x10000;
    }

    // (re-)create shader flags tag
    shaderFlagsData = editor.CreateConstantMetadata(shaderFlagsValue);

    // if we didn't have a shader tags entry at all, create the metadata node for the shader flags
    // tag
    if(!shaderFlagsTag)
      shaderFlagsTag = editor.CreateConstantMetadata((uint32_t)ShaderEntryTag::ShaderFlags);

    // if we had a tag already, we can just re-use that tag node and replace the data node.
    // Otherwise we need to add both, and we insert them first
    if(existingTag)
    {
      taglist->children[existingTag] = shaderFlagsData;
    }
    else
    {
      taglist->children.insert(0, shaderFlagsTag);
      taglist->children.insert(1, shaderFlagsData);
    }

    // set reslist and taglist in case they were null before
    entry->children[3] = reslist;
    entry->children[4] = taglist;
  }

  // get the editor to patch PSV0 with our extra UAV
  editor.RegisterUAV(DXILResourceType::ByteAddressUAV, space, 0, 0, ResourceKind::RawBuffer);

  Function *f = editor.GetFunctionByName(entryName);

  if(!f)
  {
    RDCERR("Couldn't find entry point function '%s'", entryName.c_str());
    return false;
  }

  // One ( and only one ) of createHandle or createHandleFromBinding should be defined
  RDCASSERTNOTEQUAL(createHandle == NULL, createHandleFromBinding == NULL);
  int startInst = 0;
  // create our handle first thing
  Instruction *handle = NULL;
  if(createHandle)
  {
    RDCASSERT(!isShaderModel6_6OrAbove);
    handle = editor.InsertInstruction(
        f, startInst++,
        editor.CreateInstruction(createHandle, DXOp::createHandle,
                                 {
                                     // kind = UAV
                                     editor.CreateConstant((uint8_t)HandleKind::UAV),
                                     // ID/slot
                                     editor.CreateConstant(regSlot),
                                     // array index
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

    Instruction *handleCreate = editor.InsertInstruction(
        f, startInst++,
        editor.CreateInstruction(createHandleFromBinding, DXOp::createHandleFromBinding,
                                 {
                                     // resBind
                                     resBindConstant,
                                     // ID/slot
                                     editor.CreateConstant(0U),
                                     // non-uniform
                                     editor.CreateConstant(false),
                                 }));

    handle = editor.InsertInstruction(
        f, startInst++,
        editor.CreateInstruction(
            annotateHandle, DXOp::annotateHandle,
            {
                // Resource handle
                handleCreate,
                // Resource properties
                editor.CreateConstant(
                    editor.CreateNamedStructType("dx.types.ResourceProperties", {}),
                    {
                        // IsUav : (1 << 12)
                        editor.CreateConstant(uint32_t((1 << 12) | (uint32_t)ResourceKind::RawBuffer)),
                        //
                        editor.CreateConstant(0U),
                    }),
            }));
  }

  Constant *undefi32 = editor.CreateUndef(i32);

  // insert an OR to offset 0, just to indicate validity
  editor.InsertInstruction(f, startInst++,
                           editor.CreateInstruction(atomicBinOp, DXOp::atomicBinOp,
                                                    {
                                                        // feedback UAV handle
                                                        handle,
                                                        // operation OR
                                                        editor.CreateConstant(2U),
                                                        // offset
                                                        editor.CreateConstant(0U),
                                                        // offset 2
                                                        undefi32,
                                                        // offset 3
                                                        undefi32,
                                                        // value
                                                        editor.CreateConstant(magicFeedbackValue),
                                                    }));

  for(size_t i = startInst; i < f->instructions.size(); i++)
  {
    const Instruction &inst = *f->instructions[i];
    // we want to annotate any calls to createHandle
    if(inst.op == Operation::Call &&
       ((createHandle && inst.getFuncCall()->name == createHandle->name) ||
        (createHandleFromBinding && inst.getFuncCall()->name == createHandleFromBinding->name)))
    {
      Value *idxArg;
      rdcpair<uint32_t, int32_t> slotInfo = {0, 0};
      if((createHandle && inst.getFuncCall()->name == createHandle->name))
      {
        RDCASSERT(!isShaderModel6_6OrAbove);
        if(inst.args.size() != 5)
        {
          RDCERR("Unexpected number of arguments to createHandle");
          continue;
        }

        Constant *kindArg = cast<Constant>(inst.args[1]);
        Constant *idArg = cast<Constant>(inst.args[2]);
        idxArg = inst.args[3];

        if(!kindArg || !idArg)
        {
          RDCERR("Unexpected non-constant argument to createHandle");
          continue;
        }
        HandleKind kind = (HandleKind)kindArg->getU32();
        uint32_t id = idArg->getU32();
        if(kind == HandleKind::SRV && id < srvBaseSlots.size())
          slotInfo = srvBaseSlots[id];
        else if(kind == HandleKind::UAV && id < uavBaseSlots.size())
          slotInfo = uavBaseSlots[id];
      }
      else
      {
        RDCASSERT(isShaderModel6_6OrAbove);
        if(inst.args.size() != 4)
        {
          RDCERR("Unexpected number of arguments to createHandleFromBinding");
          continue;
        }
        Constant *resBindArg = cast<Constant>(inst.args[1]);
        idxArg = inst.args[2];
        if(!resBindArg)
        {
          RDCERR("Unexpected non-constant argument to createHandleFromBinding");
          continue;
        }
        if(!resBindArg->isNULL() && resBindArg->getMembers().size() != 4)
        {
          RDCERR("Unexpected number of members to resBind");
          continue;
        }

        D3D12FeedbackKey key;
        if(resBindArg->isNULL())
        {
          key.type = DXBCBytecode::TYPE_RESOURCE;
          key.bind.bindset = 0;
          key.bind.bind = 0;
        }
        else
        {
          Constant *regArg = cast<Constant>(resBindArg->getMembers()[0]);
          Constant *spaceArg = cast<Constant>(resBindArg->getMembers()[2]);
          Constant *kindArg = cast<Constant>(resBindArg->getMembers()[3]);
          if(!regArg || !spaceArg || !kindArg)
          {
            RDCERR("Unexpected non-constant argument to createHandleFromBinding");
            continue;
          }

          HandleKind kind = (HandleKind)kindArg->getU32();
          if(kind != HandleKind::SRV && kind != HandleKind::UAV)
            continue;
          key.type = kind == HandleKind::UAV ? DXBCBytecode::TYPE_UNORDERED_ACCESS_VIEW
                                             : DXBCBytecode::TYPE_RESOURCE;
          key.bind.bindset = spaceArg->getU32();
          key.bind.bind = regArg->getU32();
        }

        auto it = slots.find(key);
        // not annotated
        if(it == slots.end())
          continue;
        // static used i.e. not arrayed? ignore
        if(it->second.StaticUsed())
          continue;
        slotInfo = {it->second.Slot(), key.bind.bind};
      }
      if(slotInfo.first == 0)
        continue;

      // idx0Based = idx - baseReg
      Instruction *idx0Based = editor.InsertInstruction(
          f, i++,
          editor.CreateInstruction(Operation::Sub, i32,
                                   {
                                       // idx to the createHandle op
                                       idxArg,
                                       // register that this is relative to
                                       editor.CreateConstant((uint32_t)slotInfo.second),
                                   }));

      // slotPlusBase = idx0Based + slot
      Instruction *slotPlusBase = editor.InsertInstruction(
          f, i++,
          editor.CreateInstruction(Operation::Add, i32,
                                   {
                                       // idx to the createHandle op
                                       idx0Based,
                                       // base slot
                                       editor.CreateConstant(slotInfo.first),
                                   }));

      // slotPlusBaseClamped = min(slotPlusBase, maxSlot)
      Instruction *slotPlusBaseClamped =
          editor.InsertInstruction(f, i++,
                                   editor.CreateInstruction(binOp, DXOp::UMin,
                                                            {
                                                                // slotPlusBase
                                                                slotPlusBase,
                                                                // max slot
                                                                editor.CreateConstant(maxSlot),
                                                            }));

      // byteOffset = slotPlusBaseClamped << 2
      Instruction *byteOffset =
          editor.InsertInstruction(f, i++,
                                   editor.CreateInstruction(Operation::ShiftLeft, i32,
                                                            {
                                                                slotPlusBaseClamped,
                                                                editor.CreateConstant(2U),
                                                            }));

      editor.InsertInstruction(f, i++,
                               editor.CreateInstruction(atomicBinOp, DXOp::atomicBinOp,
                                                        {
                                                            // feedback UAV handle
                                                            handle,
                                                            // operation OR
                                                            editor.CreateConstant(2U),
                                                            // offset
                                                            byteOffset,
                                                            // offset 2
                                                            undefi32,
                                                            // offset 3
                                                            undefi32,
                                                            // value
                                                            editor.CreateConstant(magicFeedbackValue),
                                                        }));
    }
    else if(inst.op == Operation::Call && createHandleFromHeap &&
            inst.getFuncCall()->name == createHandleFromHeap->name)
    {
      RDCASSERT(isShaderModel6_6OrAbove);
      if(inst.args.size() != 4)
      {
        RDCERR("Unexpected number of arguments to createHandleFromHeap");
        continue;
      }
      Constant *isSamplerArg = cast<Constant>(inst.args[2]);
      if(!isSamplerArg)
      {
        RDCERR("Unexpected non-constant argument to createHandleFromHeap");
        continue;
      }
      bool isSampler = isSamplerArg->getU32() != 0;

      D3D12FeedbackKey key = GetDirectHeapAccessKey();
      auto it = slots.find(key);
      if(it == slots.end())
        continue;

      // Look for annotation for the type of this view ( SRV/UAV/CBV/Sampler )
      const Instruction *annotateInst = NULL;
      for(size_t nextInstIndex = i + 1; nextInstIndex < f->instructions.size(); nextInstIndex++)
      {
        const Instruction &nextInst = *f->instructions[nextInstIndex];
        if(nextInst.op == Operation::Call && annotateHandle &&
           nextInst.getFuncCall()->name == annotateHandle->name)
        {
          if(nextInst.args.size() != 3)
          {
            RDCERR("Unexpected number of arguments to annotateHandle");
            continue;
          }
          Value *idxArg = nextInst.args[1];
          if(idxArg->id == inst.id)
          {
            annotateInst = &nextInst;
            break;
          }
        }
      }
      if(annotateInst == NULL)
      {
        RDCERR("Unexpected, could not find annotateHandle for createHandleFromHeap");
        continue;
      }

      Constant *resPropArg = cast<Constant>(annotateInst->args[2]);
      if(!resPropArg)
      {
        RDCERR("Unexpected non-constant argument for dx.types.ResourceProperties");
        continue;
      }
      if(resPropArg->getMembers().size() != 2)
      {
        RDCERR("Unexpected number of arguments to dx.types.ResourceProperties");
        continue;
      }
      Constant *resKindArg = cast<Constant>(resPropArg->getMembers()[0]);
      if(!resKindArg)
      {
        RDCERR("Unexpected non-constant argument for dx.types.ResourceProperties's resource kind");
        continue;
      }
      HandleKind handleKind = HandleKind::SRV;
      ResourceKind resKind = (ResourceKind)(resKindArg->getU32() & 0xFF);
      bool isUav = (resKindArg->getU32() & (1 << 12)) != 0;
      if(resKind == ResourceKind::Sampler || resKind == ResourceKind::SamplerComparison)
      {
        handleKind = HandleKind::Sampler;
        RDCASSERT(isSampler);
        RDCASSERT(!isUav);
      }
      else if(resKind == ResourceKind::CBuffer)
      {
        handleKind = HandleKind::CBuffer;
        RDCASSERT(!isSampler);
        RDCASSERT(!isUav);
      }
      else if(isUav)
      {
        handleKind = HandleKind::UAV;
        RDCASSERT(!isSampler);
      }
      else
      {
        handleKind = HandleKind::SRV;
        RDCASSERT(!isSampler);
      }

      Value *idxArg = inst.args[1];
      Instruction op;

      // slotPlusBase = idx0Based + slot
      Instruction *slotPlusBase = editor.InsertInstruction(
          f, i++,
          editor.CreateInstruction(Operation::Add, i32,
                                   {
                                       // idx to the createHandleFromHeap op
                                       idxArg,
                                       // base slot
                                       editor.CreateConstant(it->second.Slot()),
                                   }));

      // slotPlusBaseClamped = min(slotPlusBase, maxSlot)
      Instruction *slotPlusBaseClamped =
          editor.InsertInstruction(f, i++,
                                   editor.CreateInstruction(binOp, DXOp::UMin,
                                                            {
                                                                // slotPlusBase
                                                                slotPlusBase,
                                                                // max slot
                                                                editor.CreateConstant(maxSlot),
                                                            }));

      // byteOffset = slotPlusBase << 2
      Instruction *byteOffset =
          editor.InsertInstruction(f, i++,
                                   editor.CreateInstruction(Operation::ShiftLeft, i32,
                                                            {
                                                                slotPlusBaseClamped,
                                                                editor.CreateConstant(2U),
                                                            }));

      uint32_t feedbackValue = magicFeedbackValue | (1 << (uint32_t)handleKind);
      editor.InsertInstruction(f, i++,
                               editor.CreateInstruction(atomicBinOp, DXOp::atomicBinOp,
                                                        {
                                                            // feedback UAV handle
                                                            handle,
                                                            // operation OR
                                                            editor.CreateConstant(2U),
                                                            // offset
                                                            byteOffset,
                                                            // offset 2
                                                            undefi32,
                                                            // offset 3
                                                            undefi32,
                                                            // value
                                                            editor.CreateConstant(feedbackValue),
                                                        }));
    }
  }

  return true;
}

static bool AddArraySlots(WrappedID3D12PipelineState::ShaderEntry *shad, uint32_t space,
                          uint32_t maxDescriptors,
                          std::map<D3D12FeedbackKey, D3D12FeedbackSlot> &slots, uint32_t &numSlots,
                          bytebuf &editedBlob, D3D12_SHADER_BYTECODE &desc, bool directHeapAccess)
{
  if(!shad)
    return false;

  bool dynamicUsed = false;

  ShaderReflection &refl = shad->GetDetails();
  const ShaderBindpointMapping &mapping = shad->GetMapping();

  for(const ShaderResource &ro : refl.readOnlyResources)
  {
    const Bindpoint &bind = mapping.readOnlyResources[ro.bindPoint];
    if(bind.arraySize > 1)
    {
      D3D12FeedbackKey key;
      key.type = DXBCBytecode::TYPE_RESOURCE;
      key.bind = bind;

      slots[key].SetSlot(numSlots);
      numSlots += RDCMIN(maxDescriptors, bind.arraySize);
      dynamicUsed = true;
    }
    else if(bind.arraySize <= 1 && bind.used)
    {
      // since the eventual descriptor range iteration won't know which descriptors map to arrays
      // and which to fixed slots, it can't mark fixed descriptors as dynamically used itself. So
      // instead we don't reserve a slot and set the top bit for these binds to indicate that
      // they're fixed used. This allows for overlap between an array and a fixed resource which is
      // allowed
      D3D12FeedbackKey key;
      key.type = DXBCBytecode::TYPE_RESOURCE;
      key.bind = bind;

      slots[key].SetStaticUsed();
    }
  }

  for(const ShaderResource &rw : refl.readWriteResources)
  {
    const Bindpoint &bind = mapping.readWriteResources[rw.bindPoint];
    if(bind.arraySize > 1)
    {
      D3D12FeedbackKey key;
      key.type = DXBCBytecode::TYPE_UNORDERED_ACCESS_VIEW;
      key.bind = bind;

      slots[key].SetSlot(numSlots);
      numSlots += RDCMIN(maxDescriptors, bind.arraySize);
      dynamicUsed = true;
    }
    else if(bind.arraySize <= 1 && bind.used)
    {
      // since the eventual descriptor range iteration won't know which descriptors map to arrays
      // and which to fixed slots, it can't mark fixed descriptors as dynamically used itself. So
      // instead we don't reserve a slot and set the top bit for these binds to indicate that
      // they're fixed used. This allows for overlap between an array and a fixed resource which is
      // allowed
      D3D12FeedbackKey key;
      key.type = DXBCBytecode::TYPE_UNORDERED_ACCESS_VIEW;
      key.bind = bind;

      slots[key].SetStaticUsed();
    }
  }

  if(shad->GetDXBC()->m_Version.Major < 6 ||
     (shad->GetDXBC()->m_Version.Major == 6 && shad->GetDXBC()->m_Version.Minor < 6))
  {
    directHeapAccess = false;
  }
  if(directHeapAccess)
    directHeapAccess = shad->GetDXBC()->GetDXILByteCode()->GetDirectHeapAcessCount() > 0;

  if(directHeapAccess)
  {
    D3D12FeedbackKey key = GetDirectHeapAccessKey();
    slots[key].SetSlot(numSlots);
    numSlots += maxDescriptors;
    dynamicUsed = true;
  }

  // if we haven't encountered any array slots, no need to do any patching
  if(!dynamicUsed)
    return false;

  // only SM5.1 can have dynamic array indexing
  if(shad->GetDXBC()->m_Version.Major == 5 && shad->GetDXBC()->m_Version.Minor == 1)
  {
    if(AnnotateDXBCShader(shad->GetDXBC(), space, slots, numSlots, editedBlob))
    {
      if(!D3D12_Debug_FeedbackDumpDirPath().empty())
        FileIO::WriteAll(D3D12_Debug_FeedbackDumpDirPath() + "/before_dxbc_" +
                             ToStr(shad->GetDetails().stage).c_str() + ".dxbc",
                         shad->GetDXBC()->GetShaderBlob());

      if(!D3D12_Debug_FeedbackDumpDirPath().empty())
        FileIO::WriteAll(D3D12_Debug_FeedbackDumpDirPath() + "/after_dxbc_" +
                             ToStr(shad->GetDetails().stage).c_str() + ".dxbc",
                         editedBlob);

      desc.pShaderBytecode = editedBlob.data();
      desc.BytecodeLength = editedBlob.size();
    }
  }
  else if(shad->GetDXBC()->m_Version.Major >= 6)
  {
    if(AnnotateDXILShader(shad->GetDXBC(), space, slots, numSlots, editedBlob))
    {
      if(!D3D12_Debug_FeedbackDumpDirPath().empty())
      {
        bytebuf orig = shad->GetDXBC()->GetShaderBlob();

        DXBC::DXBCContainer::StripChunk(orig, DXBC::FOURCC_ILDB);
        DXBC::DXBCContainer::StripChunk(orig, DXBC::FOURCC_STAT);

        FileIO::WriteAll(D3D12_Debug_FeedbackDumpDirPath() + "/before_dxil_" +
                             ToStr(shad->GetDetails().stage).c_str() + ".dxbc",
                         orig);
      }

      if(!D3D12_Debug_FeedbackDumpDirPath().empty())
      {
        FileIO::WriteAll(D3D12_Debug_FeedbackDumpDirPath() + "/after_dxil_" +
                             ToStr(shad->GetDetails().stage).c_str() + ".dxbc",
                         editedBlob);
      }

      desc.pShaderBytecode = editedBlob.data();
      desc.BytecodeLength = editedBlob.size();
    }
  }

  return true;
}

struct D3D12StatCallback : public D3D12ActionCallback
{
  D3D12StatCallback(WrappedID3D12Device *dev, ID3D12QueryHeap *heap)
      : m_pDevice(dev), m_PipeStatsQueryHeap(heap)
  {
    m_pDevice->GetQueue()->GetCommandData()->m_ActionCallback = this;
  }
  ~D3D12StatCallback() { m_pDevice->GetQueue()->GetCommandData()->m_ActionCallback = NULL; }
  void PreDraw(uint32_t eid, ID3D12GraphicsCommandListX *cmd) override
  {
    if(cmd->GetType() == D3D12_COMMAND_LIST_TYPE_DIRECT)
      cmd->BeginQuery(m_PipeStatsQueryHeap, D3D12_QUERY_TYPE_PIPELINE_STATISTICS, 0);
  }

  bool PostDraw(uint32_t eid, ID3D12GraphicsCommandListX *cmd) override
  {
    if(cmd->GetType() == D3D12_COMMAND_LIST_TYPE_DIRECT)
      cmd->EndQuery(m_PipeStatsQueryHeap, D3D12_QUERY_TYPE_PIPELINE_STATISTICS, 0);
    return false;
  }

  void PostRedraw(uint32_t eid, ID3D12GraphicsCommandListX *cmd) override {}
  // we don't need to distinguish, call the Draw functions
  void PreDispatch(uint32_t eid, ID3D12GraphicsCommandListX *cmd) override { PreDraw(eid, cmd); }
  bool PostDispatch(uint32_t eid, ID3D12GraphicsCommandListX *cmd) override
  {
    return PostDraw(eid, cmd);
  }
  void PostRedispatch(uint32_t eid, ID3D12GraphicsCommandListX *cmd) override
  {
    PostRedraw(eid, cmd);
  }
  void PreMisc(uint32_t eid, ActionFlags flags, ID3D12GraphicsCommandListX *cmd) override
  {
    if(flags & ActionFlags::PassBoundary)
      return;
    PreDraw(eid, cmd);
  }
  bool PostMisc(uint32_t eid, ActionFlags flags, ID3D12GraphicsCommandListX *cmd) override
  {
    if(flags & ActionFlags::PassBoundary)
      return false;
    return PostDraw(eid, cmd);
  }
  void PostRemisc(uint32_t eid, ActionFlags flags, ID3D12GraphicsCommandListX *cmd) override
  {
    if(flags & ActionFlags::PassBoundary)
      return;
    PostRedraw(eid, cmd);
  }

  void PreCloseCommandList(ID3D12GraphicsCommandListX *cmd) override{};
  void AliasEvent(uint32_t primary, uint32_t alias) override {}
  WrappedID3D12Device *m_pDevice;
  ID3D12QueryHeap *m_PipeStatsQueryHeap;
};

bool D3D12Replay::FetchShaderFeedback(uint32_t eventId)
{
  if(m_BindlessFeedback.Usage.find(eventId) != m_BindlessFeedback.Usage.end())
    return false;

  if(!D3D12_BindlessFeedback())
    return false;

  if(m_pDevice->HasFatalError())
    return false;

  // create it here so we won't re-run any code if the event is re-selected. We'll mark it as valid
  // if it actually has any data in it later.
  D3D12DynamicShaderFeedback &result = m_BindlessFeedback.Usage[eventId];

  const ActionDescription *action = m_pDevice->GetAction(eventId);

  if(action == NULL ||
     !(action->flags & (ActionFlags::Dispatch | ActionFlags::MeshDispatch | ActionFlags::Drawcall)))
  {
    // deliberately show no bindings as used for non-draws
    result.valid = true;
    return false;
  }

  result.compute = bool(action->flags & ActionFlags::Dispatch);

  D3D12RenderState &rs = m_pDevice->GetQueue()->GetCommandData()->m_RenderState;

  D3D12ResourceManager *rm = m_pDevice->GetResourceManager();

  WrappedID3D12PipelineState *pipe =
      (WrappedID3D12PipelineState *)rm->GetCurrentAs<ID3D12PipelineState>(rs.pipe);
  D3D12RootSignature modsig;

  if(!pipe)
  {
    RDCERR("Can't fetch shader feedback, no pipeline state bound");
    result.valid = true;
    return false;
  }

  bytebuf editedBlob[(uint32_t)ShaderStage::Count];

  D3D12_EXPANDED_PIPELINE_STATE_STREAM_DESC pipeDesc;
  pipe->Fill(pipeDesc);

  uint32_t space = 1;

  uint32_t maxDescriptors = 0;
  for(ResourceId id : rs.heaps)
  {
    WrappedID3D12DescriptorHeap *heap =
        (WrappedID3D12DescriptorHeap *)rm->GetCurrentAs<ID3D12DescriptorHeap>(id);
    D3D12_DESCRIPTOR_HEAP_DESC desc = heap->GetDesc();
    maxDescriptors = RDCMAX(maxDescriptors, desc.NumDescriptors);
  }
  RDCDEBUG("Clamping any unbounded ranges to %u descriptors", maxDescriptors);

  std::map<D3D12FeedbackKey, D3D12FeedbackSlot> slots[(uint32_t)ShaderStage::Count];

  // reserve the first 4 dwords for debug info and a validity flag
  uint32_t numSlots = numReservedSlots;

#if ENABLED(RDOC_DEVEL)
  m_pDevice->GetShaderCache()->LoadDXC();
#endif

  bool dynamicAccessPerStage[NumShaderStages] = {};

  if(result.compute)
  {
    ID3D12RootSignature *sig = rm->GetCurrentAs<ID3D12RootSignature>(rs.compute.rootsig);

    if(!sig)
    {
      result.valid = true;
      return false;
    }

    modsig = ((WrappedID3D12RootSignature *)sig)->sig;
    bool directHeapAccess =
        (modsig.Flags & (D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED |
                         D3D12_ROOT_SIGNATURE_FLAG_SAMPLER_HEAP_DIRECTLY_INDEXED)) != 0;
    space = modsig.maxSpaceIndex;

    dynamicAccessPerStage[5] = AddArraySlots(
        pipe->CS(), space, maxDescriptors, slots[(uint32_t)ShaderStage::Compute], numSlots,
        editedBlob[(uint32_t)ShaderStage::Compute], pipeDesc.CS, directHeapAccess);
  }
  else
  {
    ID3D12RootSignature *sig = rm->GetCurrentAs<ID3D12RootSignature>(rs.graphics.rootsig);

    if(!sig)
    {
      result.valid = true;
      return false;
    }

    modsig = ((WrappedID3D12RootSignature *)sig)->sig;
    bool directHeapAccess =
        (modsig.Flags & (D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED |
                         D3D12_ROOT_SIGNATURE_FLAG_SAMPLER_HEAP_DIRECTLY_INDEXED)) != 0;

    space = modsig.maxSpaceIndex;

    dynamicAccessPerStage[0] = AddArraySlots(
        pipe->VS(), space, maxDescriptors, slots[(uint32_t)ShaderStage::Vertex], numSlots,
        editedBlob[uint32_t(ShaderStage::Vertex)], pipeDesc.VS, directHeapAccess);
    dynamicAccessPerStage[1] = AddArraySlots(
        pipe->HS(), space, maxDescriptors, slots[(uint32_t)ShaderStage::Hull], numSlots,
        editedBlob[uint32_t(ShaderStage::Hull)], pipeDesc.HS, directHeapAccess);
    dynamicAccessPerStage[2] = AddArraySlots(
        pipe->DS(), space, maxDescriptors, slots[(uint32_t)ShaderStage::Domain], numSlots,
        editedBlob[uint32_t(ShaderStage::Domain)], pipeDesc.DS, directHeapAccess);
    dynamicAccessPerStage[3] = AddArraySlots(
        pipe->GS(), space, maxDescriptors, slots[(uint32_t)ShaderStage::Geometry], numSlots,
        editedBlob[uint32_t(ShaderStage::Geometry)], pipeDesc.GS, directHeapAccess);
    dynamicAccessPerStage[4] = AddArraySlots(
        pipe->PS(), space, maxDescriptors, slots[(uint32_t)ShaderStage::Pixel], numSlots,
        editedBlob[uint32_t(ShaderStage::Pixel)], pipeDesc.PS, directHeapAccess);
    dynamicAccessPerStage[6] = AddArraySlots(
        pipe->AS(), space, maxDescriptors, slots[(uint32_t)ShaderStage::Amplification], numSlots,
        editedBlob[uint32_t(ShaderStage::Amplification)], pipeDesc.AS, directHeapAccess);
    dynamicAccessPerStage[7] = AddArraySlots(
        pipe->MS(), space, maxDescriptors, slots[(uint32_t)ShaderStage::Mesh], numSlots,
        editedBlob[uint32_t(ShaderStage::Mesh)], pipeDesc.MS, directHeapAccess);
  }

  // if numSlots wasn't increased, none of the resources were arrayed so we have nothing to do.
  // Silently return
  if(numSlots == numReservedSlots)
  {
    return false;
  }

  numSlots = AlignUp16(numSlots);

  // need to be able to add a descriptor of our UAV without hitting the 64 DWORD limit
  if(modsig.dwordLength > 62)
  {
    RDCWARN("Root signature is 64 DWORDS, adding feedback buffer might fail");
  }

  // add root UAV element
  modsig.Parameters.push_back(D3D12RootSignatureParameter());
  {
    D3D12RootSignatureParameter &param = modsig.Parameters.back();
    param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV;
    param.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    param.Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_DATA_VOLATILE;
    param.Descriptor.RegisterSpace = space;
    param.Descriptor.ShaderRegister = 0;
  }

  if(m_BindlessFeedback.PipeStatsHeap == NULL)
  {
    D3D12_QUERY_HEAP_DESC pipestatsQueryDesc;
    pipestatsQueryDesc.Count = 1;
    pipestatsQueryDesc.NodeMask = 1;
    pipestatsQueryDesc.Type = D3D12_QUERY_HEAP_TYPE_PIPELINE_STATISTICS;
    HRESULT hr = m_pDevice->CreateQueryHeap(&pipestatsQueryDesc, __uuidof(ID3D12QueryHeap),
                                            (void **)&m_BindlessFeedback.PipeStatsHeap);
    m_pDevice->CheckHRESULT(hr);
    if(FAILED(hr))
    {
      RDCERR("Failed to create shader feedback pipeline query heap HRESULT: %s", ToStr(hr).c_str());
      return false;
    }
  }

  if(m_BindlessFeedback.FeedbackBuffer == NULL ||
     m_BindlessFeedback.FeedbackBuffer->GetDesc().Width < numSlots * sizeof(uint32_t))
  {
    SAFE_RELEASE(m_BindlessFeedback.FeedbackBuffer);

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
    desc.Width = AlignUp<uint32_t>(numSlots * sizeof(uint32_t), 1024) +
                 sizeof(D3D12_QUERY_DATA_PIPELINE_STATISTICS);

    D3D12_HEAP_PROPERTIES heapProps;
    heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
    heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    heapProps.CreationNodeMask = 1;
    heapProps.VisibleNodeMask = 1;

    HRESULT hr = m_pDevice->CreateCommittedResource(
        &heapProps, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_COMMON, NULL,
        __uuidof(ID3D12Resource), (void **)&m_BindlessFeedback.FeedbackBuffer);

    if(m_BindlessFeedback.FeedbackBuffer == NULL || FAILED(hr))
    {
      RDCERR("Couldn't create feedback buffer with %u slots: %s", numSlots, ToStr(hr).c_str());
      return false;
    }

    m_BindlessFeedback.FeedbackBuffer->SetName(L"m_BindlessFeedback.FeedbackBuffer");
  }

  {
    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
    uavDesc.Format = DXGI_FORMAT_R32_UINT;
    uavDesc.Buffer.CounterOffsetInBytes = 0;
    // start with elements after the counter
    uavDesc.Buffer.FirstElement = 0;
    uavDesc.Buffer.NumElements = numSlots;
    uavDesc.Buffer.StructureByteStride = 0;

    m_pDevice->CreateUnorderedAccessView(m_BindlessFeedback.FeedbackBuffer, NULL, &uavDesc,
                                         GetDebugManager()->GetCPUHandle(FEEDBACK_CLEAR_UAV));
    m_pDevice->CreateUnorderedAccessView(m_BindlessFeedback.FeedbackBuffer, NULL, &uavDesc,
                                         GetDebugManager()->GetUAVClearHandle(FEEDBACK_CLEAR_UAV));

    ID3D12GraphicsCommandList *list = m_pDevice->GetNewList();
    if(!list)
      return false;

    GetDebugManager()->SetDescriptorHeaps(list, true, false);

    UINT zeroes[4] = {0, 0, 0, 0};
    list->ClearUnorderedAccessViewUint(GetDebugManager()->GetGPUHandle(FEEDBACK_CLEAR_UAV),
                                       GetDebugManager()->GetUAVClearHandle(FEEDBACK_CLEAR_UAV),
                                       m_BindlessFeedback.FeedbackBuffer, zeroes, 0, NULL);

    list->Close();
  }

  ID3D12RootSignature *annotatedSig = NULL;

  {
    ID3DBlob *root = m_pDevice->GetShaderCache()->MakeRootSig(modsig);
    HRESULT hr =
        m_pDevice->CreateRootSignature(0, root->GetBufferPointer(), root->GetBufferSize(),
                                       __uuidof(ID3D12RootSignature), (void **)&annotatedSig);

    if(annotatedSig == NULL || FAILED(hr))
    {
      SAFE_RELEASE(root);
      RDCERR("Couldn't create feedback modified root signature: %s", ToStr(hr).c_str());
      return false;
    }

    SAFE_RELEASE(root);
  }

  ID3D12PipelineState *annotatedPipe = NULL;

  {
    pipeDesc.pRootSignature = annotatedSig;

    HRESULT hr = m_pDevice->CreatePipeState(pipeDesc, &annotatedPipe);
    if(annotatedPipe == NULL || FAILED(hr))
    {
      SAFE_RELEASE(annotatedSig);
      RDCERR("Couldn't create feedback modified pipeline: %s", ToStr(hr).c_str());
      return false;
    }
  }

  D3D12RenderState prev = rs;

  rs.pipe = GetResID(annotatedPipe);

  if(result.compute)
  {
    rs.compute.rootsig = GetResID(annotatedSig);
    size_t idx = modsig.Parameters.size() - 1;
    rs.compute.sigelems.resize_for_index(idx);
    rs.compute.sigelems[idx] =
        D3D12RenderState::SignatureElement(eRootUAV, GetResID(m_BindlessFeedback.FeedbackBuffer), 0);
  }
  else
  {
    rs.graphics.rootsig = GetResID(annotatedSig);
    size_t idx = modsig.Parameters.size() - 1;
    rs.graphics.sigelems.resize_for_index(idx);
    rs.graphics.sigelems[idx] =
        D3D12RenderState::SignatureElement(eRootUAV, GetResID(m_BindlessFeedback.FeedbackBuffer), 0);
  }

  {
    D3D12StatCallback cb(m_pDevice, m_BindlessFeedback.PipeStatsHeap);

    m_pDevice->ReplayLog(0, eventId, eReplay_OnlyDraw);

    ID3D12GraphicsCommandList *list = m_pDevice->GetNewList();
    if(!list)
      return true;

    list->ResolveQueryData(m_BindlessFeedback.PipeStatsHeap, D3D12_QUERY_TYPE_PIPELINE_STATISTICS,
                           0, 1, m_BindlessFeedback.FeedbackBuffer, numSlots * sizeof(uint32_t));

    list->Close();
  }

  m_pDevice->ExecuteLists();
  m_pDevice->FlushLists();

  SAFE_RELEASE(annotatedPipe);
  SAFE_RELEASE(annotatedSig);

  rs = prev;

  bytebuf results;
  GetDebugManager()->GetBufferData(m_BindlessFeedback.FeedbackBuffer, 0, 0, results);

  if(results.size() < numSlots * sizeof(uint32_t))
  {
    RDCERR("Results buffer not the right size!");
  }
  else
  {
    uint32_t *slotsData = (uint32_t *)results.data();

    D3D12_QUERY_DATA_PIPELINE_STATISTICS *pipelinestats =
        (D3D12_QUERY_DATA_PIPELINE_STATISTICS *)(slotsData + numSlots);

    if(slotsData[0] == magicFeedbackValue)
    {
      result.valid = true;

      // now we iterate over descriptor ranges and find which (of potentially multiple) registers
      // each descriptor maps to and store the index if it's dynamically or statically used. We do
      // this here so it only happens once instead of doing it when looking up the data.

      D3D12FeedbackKey curKey;
      D3D12FeedbackBindIdentifier curIdentifier = {};
      // don't iterate the last signature element because that's ours!
      for(size_t rootEl = 0; rootEl < modsig.Parameters.size() - 1; rootEl++)
      {
        curIdentifier.rootEl = rootEl;

        const D3D12RootSignatureParameter &p = modsig.Parameters[rootEl];

        // only tables need feedback data, others all are treated as dynamically used
        if(p.ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE)
        {
          for(size_t r = 0; r < p.ranges.size(); r++)
          {
            const D3D12_DESCRIPTOR_RANGE1 &range = p.ranges[r];

            curIdentifier.rangeIndex = r;

            curKey.bind.bindset = range.RegisterSpace;

            UINT num = range.NumDescriptors;
            uint32_t visMask = 0;
            // see which shader's binds we should look up for this range
            switch(p.ShaderVisibility)
            {
              case D3D12_SHADER_VISIBILITY_ALL:
                visMask = result.compute ? (uint32_t)ShaderStageMask::Compute : 0xff;
                break;
              case D3D12_SHADER_VISIBILITY_VERTEX:
                visMask = (uint32_t)ShaderStageMask::Vertex;
                break;
              case D3D12_SHADER_VISIBILITY_HULL: visMask = (uint32_t)ShaderStageMask::Hull; break;
              case D3D12_SHADER_VISIBILITY_DOMAIN:
                visMask = (uint32_t)ShaderStageMask::Domain;
                break;
              case D3D12_SHADER_VISIBILITY_GEOMETRY:
                visMask = (uint32_t)ShaderStageMask::Geometry;
                break;
              case D3D12_SHADER_VISIBILITY_PIXEL: visMask = uint32_t(ShaderStageMask::Pixel); break;
              case D3D12_SHADER_VISIBILITY_AMPLIFICATION:
                visMask = uint32_t(ShaderStageMask::Amplification);
                break;
              case D3D12_SHADER_VISIBILITY_MESH: visMask = uint32_t(ShaderStageMask::Mesh); break;
              default: RDCERR("Unexpected shader visibility %d", p.ShaderVisibility); return true;
            }

            // set the key type
            if(range.RangeType == D3D12_DESCRIPTOR_RANGE_TYPE_SRV)
              curKey.type = DXBCBytecode::TYPE_RESOURCE;
            else if(range.RangeType == D3D12_DESCRIPTOR_RANGE_TYPE_UAV)
              curKey.type = DXBCBytecode::TYPE_UNORDERED_ACCESS_VIEW;

            for(uint32_t st = 0; st < (uint32_t)ShaderStage::Count; st++)
            {
              if(visMask & (1 << st))
              {
                curIdentifier.descIndex = 0;
                curKey.bind.bind = range.BaseShaderRegister;

                // the feedback entries start here
                auto slotIt = slots[st].lower_bound(curKey);

                // iterate over the declared range. This could be unbounded, so we might exit
                // another way
                for(uint32_t i = 0; i < num; i++)
                {
                  // stop when we've run out of recorded used slots
                  if(slotIt == slots[st].end())
                    break;

                  Bindpoint bind = slotIt->first.bind;

                  // stop if the next used slot is in another space or is another type
                  if(bind.bindset > curKey.bind.bindset || slotIt->first.type != curKey.type)
                    break;

                  // if the next bind is definitely outside this range, early out now instead of
                  // iterating fruitlessly
                  if(((uint32_t)bind.bind - range.BaseShaderRegister) > num)
                    break;

                  int32_t lastBind =
                      bind.bind + (int32_t)RDCCLAMP(bind.arraySize, 1U, maxDescriptors);

                  // if this slot's array covers the current bind, check the result
                  if(bind.bind <= curKey.bind.bind && curKey.bind.bind < lastBind)
                  {
                    // if it's static used by having a fixed result declared, it's used
                    const bool staticUsed = slotIt->second.StaticUsed();

                    // otherwise check the feedback we got
                    const uint32_t baseSlot = slotIt->second.Slot();
                    const uint32_t arrayIndex = curKey.bind.bind - bind.bind;

                    if(staticUsed || slotsData[baseSlot + arrayIndex])
                      result.used.push_back(curIdentifier);
                  }

                  curKey.bind.bind++;
                  curIdentifier.descIndex++;

                  // if we've passed this slot, move to the next one. Because we're iterating a
                  // contiguous range of binds the next slot will be enough for the next iteration
                  if(curKey.bind.bind >= lastBind)
                    slotIt++;
                }
              }
            }
          }
        }
      }

      D3D12FeedbackKey directAccessKey = GetDirectHeapAccessKey();
      for(uint32_t shaderStage = 0; shaderStage < (uint32_t)ShaderStage::Count; shaderStage++)
      {
        if(slots[shaderStage].find(directAccessKey) == slots[shaderStage].end())
          continue;
        D3D12FeedbackSlot &feedbackSlots = slots[shaderStage].at(directAccessKey);
        for(uint32_t i = feedbackSlots.Slot(); i < feedbackSlots.Slot() + maxDescriptors; ++i)
        {
          if((slotsData[i] & magicFeedbackValue) == magicFeedbackValue)
          {
            uint32_t usedSlot = i - feedbackSlots.Slot();
            D3D12FeedbackBindIdentifier directAccessIdentifier = {};
            directAccessIdentifier.descIndex = usedSlot;
            directAccessIdentifier.rootEl = ~0U;
            directAccessIdentifier.rangeIndex = ~0U;
            directAccessIdentifier.directAccess = true;
            directAccessIdentifier.shaderStage = (ShaderStage)shaderStage;
            uint32_t handleKind = slotsData[i] & magicFeedbackTypeMask;
            if((handleKind & (1 << (uint32_t)DXIL::HandleKind::Sampler)) != 0)
            {
              directAccessIdentifier.bindType = BindType::Sampler;
              result.used.push_back(directAccessIdentifier);
            }
            bool isCBV = (handleKind & (1 << (uint32_t)DXIL::HandleKind::CBuffer)) != 0;
            bool isSRV = (handleKind & (1 << (uint32_t)DXIL::HandleKind::SRV)) != 0;
            bool isUAV = (handleKind & (1 << (uint32_t)DXIL::HandleKind::UAV)) != 0;
            if(isCBV || isSRV || isUAV)
            {
              if((isCBV && isSRV) || (isCBV && isUAV) || (isSRV && isUAV))
              {
                RDCERR("Unexpected, resource used with multiple incompatible types");
                continue;
              }
              if(isCBV)
                directAccessIdentifier.bindType = BindType::ConstantBuffer;
              else if(isSRV)
                directAccessIdentifier.bindType = BindType::ReadOnlyResource;
              else if(isUAV)
                directAccessIdentifier.bindType = BindType::ReadWriteResource;
              result.used.push_back(directAccessIdentifier);
            }
          }
        }
      }

      std::sort(result.used.begin(), result.used.end());
    }
    else if((pipelinestats->VSInvocations == 0 || !dynamicAccessPerStage[0]) &&
            (pipelinestats->HSInvocations == 0 || !dynamicAccessPerStage[1]) &&
            (pipelinestats->DSInvocations == 0 || !dynamicAccessPerStage[2]) &&
            (pipelinestats->GSInvocations == 0 || !dynamicAccessPerStage[3]) &&
            (pipelinestats->PSInvocations == 0 || !dynamicAccessPerStage[4]) &&
            (pipelinestats->CSInvocations == 0 || !dynamicAccessPerStage[5]))
    {
      RDCDEBUG(
          "No results from shader feedback but no dynamically-accessing shaders were actually "
          "invoked");
      result.valid = true;
    }
    else
    {
      RDCERR("Didn't get valid feedback identifier. Expected %08x got %08x", magicFeedbackValue,
             slotsData[0]);
    }
  }

  return true;
}

void D3D12Replay::ClearFeedbackCache()
{
  m_BindlessFeedback.Usage.clear();
}

#if ENABLED(ENABLE_UNIT_TESTS) && 0

#include "catch/catch.hpp"

TEST_CASE("DO NOT COMMIT - convenience test", "[dxbc]")
{
  // this test loads a file from disk and does a no-op edit pass on it then an annotation pass on
  // it. Useful for when you are iterating on a shader and don't want to have to load a whole
  // capture.
  bytebuf buf;
  FileIO::ReadAll("/path/to/container_file.dxbc", buf);
  bytebuf editedBlob;

  {
    DXBC::DXBCContainer dxbc(buf, rdcstr(), GraphicsAPI::D3D12, ~0U, ~0U);

    DXIL::ProgramEditor editor(&dxbc, 1234, editedBlob);
  }

  {
    DXBC::DXBCContainer container(editedBlob, rdcstr(), GraphicsAPI::D3D12, ~0U, ~0U);

    rdcstr disasm = container.GetDisassembly();

    RDCLOG("no edits - %s", disasm.c_str());
  }

  {
    WrappedID3D12Device device(NULL, D3D12InitParams(), false);

    D3D12_SHADER_BYTECODE desc;
    desc.BytecodeLength = buf.size();
    desc.pShaderBytecode = buf.data();

    WrappedID3D12PipelineState::ShaderEntry shad(desc, &device);

    std::map<D3D12FeedbackKey, D3D12FeedbackSlot> slots;
    uint32_t numSlots = 4;
    AddArraySlots(&shad, 123456, 1000000, slots, numSlots, editedBlob, desc);
  }

  {
    DXBC::DXBCContainer container(editedBlob, rdcstr(), GraphicsAPI::D3D12, ~0U, ~0U);

    rdcstr disasm = container.GetDisassembly();

    RDCLOG("annotated - %s", disasm.c_str());
  }
}

#endif
