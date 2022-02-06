/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2021 Baldur Karlsson
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
static const uint32_t magicFeedbackValue = 0xbeebf33d;

static bool AnnotateDXBCShader(const DXBC::DXBCContainer *dxbc, uint32_t space,
                               const std::map<D3D12FeedbackKey, D3D12FeedbackSlot> &slots,
                               bytebuf &editedBlob)
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

      // resource base plus index
      editor.InsertOperation(i++,
                             oper(OPCODE_IADD, {temp(t).swizzle(0), imm(it->second.Slot()), idx}));
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
                               bytebuf &editedBlob)
{
  DXIL::ProgramEditor editor(dxbc, slots.size() + 64, editedBlob);

  const DXIL::Type *handleType = editor.GetTypeByName("dx.types.Handle");
  const DXIL::Function *createHandle = editor.GetFunctionByName("dx.op.createHandle");

  // if we don't have the handle type then this shader can't use any dynamically! we have no
  // feedback to get
  if(!handleType || !createHandle)
    return false;

  const DXIL::Type *i32 = editor.GetInt32Type();
  const DXIL::Type *i8 = editor.GetInt8Type();
  const DXIL::Type *i1 = editor.GetBoolType();

  // get the atomic function we'll need
  const DXIL::Function *atomicBinOp = editor.GetFunctionByName("dx.op.atomicBinOp.i32");
  if(!atomicBinOp)
  {
    DXIL::Type atomicType;
    atomicType.type = DXIL::Type::Function;
    // return type
    atomicType.inner = i32;
    atomicType.members = {i32, handleType, i32, i32, i32, i32, i32};

    const DXIL::Type *funcType = editor.AddType(atomicType);

    DXIL::Type funcPtrType;
    funcPtrType.type = DXIL::Type::Pointer;
    funcPtrType.inner = funcType;

    DXIL::Function atomicFunc;
    atomicFunc.name = "dx.op.atomicBinOp.i32";
    atomicFunc.funcType = editor.AddType(funcPtrType);
    atomicFunc.external = true;

    for(const DXIL::AttributeSet &attrs : editor.GetAttributeSets())
    {
      if(attrs.functionSlot && attrs.functionSlot->params == DXIL::Attribute::NoUnwind)
      {
        atomicFunc.attrs = &attrs;
        break;
      }
    }

    if(!atomicFunc.attrs)
      RDCWARN("Couldn't find existing nounwind attr set");
    // should we add a set here? or assume the attrs don't matter because this is a builtin function
    // anyway?

    atomicBinOp = editor.DeclareFunction(atomicFunc);
  }

  // while we're iterating through the metadata to add our UAV, we'll also note the shader-local
  // register IDs of each SRV/UAV with slots, and record the base slot in this array for easy access
  // later when annotating dx.op.createHandle calls. We also need to know the base register because
  // the index dxc provides is register-relative
  rdcarray<rdcpair<uint32_t, int32_t>> srvBaseSlots, uavBaseSlots;

  // when we add metadata we do it in reverse order since it's unclear if we're supposed to have
  // forward references (LLVM seems to handle it, but not emit it, so it's very much a grey area)
  // we do this by recreating any nodes that we modify (or their parents recursively up to the named
  // metadata)

  // declare the resource, this happens purely in metadata but we need to store the slot
  uint32_t regSlot = 0;
  DXIL::Metadata *reslist = NULL;
  {
    const DXIL::Type *rw = editor.GetTypeByName("struct.RWByteAddressBuffer");

    // declare the type if not present
    if(!rw)
    {
      DXIL::Type rwType;
      rwType.name = "struct.RWByteAddressBuffer";
      rwType.type = DXIL::Type::Struct;
      rwType.members = {i32};

      rw = editor.AddType(rwType);
    }

    const DXIL::Type *rwptr = editor.GetPointerType(rw, DXIL::Type::PointerAddrSpace::Default);

    if(!rwptr)
    {
      DXIL::Type rwPtrType;
      rwPtrType.type = DXIL::Type::Pointer;
      rwPtrType.addrSpace = DXIL::Type::PointerAddrSpace::Default;
      rwPtrType.inner = rw;

      rwptr = editor.AddType(rwPtrType);
    }

    DXIL::Metadata *resources = editor.GetMetadataByName("dx.resources");

    // if there are no resources declared we can't have any dynamic indexing
    if(!resources)
      return false;

    reslist = resources->children[0];

    DXIL::Metadata *srvs = reslist->children[0];
    DXIL::Metadata *uavs = reslist->children[1];

    D3D12FeedbackKey key;

    key.type = DXBCBytecode::TYPE_RESOURCE;

    for(size_t i = 0; srvs && i < srvs->children.size(); i++)
    {
      // each SRV child should have a fixed format
      const DXIL::Metadata *srv = srvs->children[i];
      const DXIL::Metadata *slot = srv->children[(size_t)DXIL::ResField::ID];
      const DXIL::Metadata *srvSpace = srv->children[(size_t)DXIL::ResField::Space];
      const DXIL::Metadata *reg = srv->children[(size_t)DXIL::ResField::RegBase];

      if(slot->value.type != DXIL::ValueType::Constant)
      {
        RDCWARN("Unexpected non-constant slot ID in SRV");
        continue;
      }

      if(srvSpace->value.type != DXIL::ValueType::Constant)
      {
        RDCWARN("Unexpected non-constant register space in SRV");
        continue;
      }

      if(reg->value.type != DXIL::ValueType::Constant)
      {
        RDCWARN("Unexpected non-constant register base in SRV");
        continue;
      }

      uint32_t id = slot->value.constant->val.u32v[0];
      key.bind.bindset = srvSpace->value.constant->val.u32v[0];
      key.bind.bind = reg->value.constant->val.u32v[0];

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

    for(size_t i = 0; uavs && i < uavs->children.size(); i++)
    {
      // each UAV child should have a fixed format, [0] is the reg ID and I think this should always
      // be == the index
      const DXIL::Metadata *uav = uavs->children[i];
      const DXIL::Metadata *slot = uav->children[(size_t)DXIL::ResField::ID];
      const DXIL::Metadata *uavSpace = uav->children[(size_t)DXIL::ResField::Space];
      const DXIL::Metadata *reg = uav->children[(size_t)DXIL::ResField::RegBase];

      if(slot->value.type != DXIL::ValueType::Constant)
      {
        RDCWARN("Unexpected non-constant slot ID in UAV");
        continue;
      }

      if(uavSpace->value.type != DXIL::ValueType::Constant)
      {
        RDCWARN("Unexpected non-constant register space in UAV");
        continue;
      }

      if(reg->value.type != DXIL::ValueType::Constant)
      {
        RDCWARN("Unexpected non-constant register base in UAV");
        continue;
      }

      RDCASSERT(slot->value.constant->val.u32v[0] == i);

      uint32_t id = slot->value.constant->val.u32v[0];
      regSlot = RDCMAX(id + 1, regSlot);

      // ensure every valid ID has an index, even if it's 0
      uavBaseSlots.resize_for_index(id);

      key.bind.bindset = uavSpace->value.constant->val.u32v[0];
      key.bind.bind = reg->value.constant->val.u32v[0];

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

    DXIL::Metadata *uavRecord[11] = {
        editor.AddMetadata(DXIL::Metadata()),
        editor.AddMetadata(DXIL::Metadata()),
        editor.AddMetadata(DXIL::Metadata()),
        editor.AddMetadata(DXIL::Metadata()),
        editor.AddMetadata(DXIL::Metadata()),
        editor.AddMetadata(DXIL::Metadata()),
        editor.AddMetadata(DXIL::Metadata()),
        editor.AddMetadata(DXIL::Metadata()),
        editor.AddMetadata(DXIL::Metadata()),
        editor.AddMetadata(DXIL::Metadata()),
        NULL,
    };

#define SET_CONST_META(m, t, v) \
  m->isConstant = true;         \
  m->type = t;                  \
  m->value = DXIL::Value(editor.GetOrAddConstant(DXIL::Constant(t, v)));

    // linear reg slot/ID
    SET_CONST_META(uavRecord[0], i32, regSlot);
    // variable
    {
      DXIL::Constant c;
      c.type = rwptr;
      c.undef = true;
      uavRecord[1]->isConstant = true;
      uavRecord[1]->type = rwptr;
      uavRecord[1]->value = DXIL::Value(editor.GetOrAddConstant(c));
    }
    // name, empty
    uavRecord[2]->isString = true;
    // reg space
    SET_CONST_META(uavRecord[3], i32, space);
    // reg base
    SET_CONST_META(uavRecord[4], i32, 0U);
    // reg count
    SET_CONST_META(uavRecord[5], i32, 1U);
    // shape
    SET_CONST_META(uavRecord[6], i32, uint32_t(DXIL::ResourceKind::RawBuffer));
    // globally coherent
    SET_CONST_META(uavRecord[7], i1, 0U);
    // hidden counter
    SET_CONST_META(uavRecord[8], i1, 0U);
    // raster order
    SET_CONST_META(uavRecord[9], i1, 0U);
    // UAV tags
    uavRecord[10] = NULL;

    // create the new UAV record
    DXIL::Metadata *feedbackuav = editor.AddMetadata(DXIL::Metadata());
    feedbackuav->children.assign(uavRecord, ARRAY_COUNT(uavRecord));

    // now push our record onto a new UAV list (either copied from the existing, or created fresh if
    // there isn't an existing one). This ensures it has all backwards references
    if(uavs)
      uavs = editor.AddMetadata(*uavs);
    else
      uavs = editor.AddMetadata(DXIL::Metadata());

    uavs->children.push_back(feedbackuav);

    // now recreate the reslist, and repoint the uavs
    reslist = editor.AddMetadata(*reslist);
    reslist->children[1] = uavs;

    // finally repoint the named metadata
    resources->children[0] = reslist;
  }

  rdcstr entryName;
  // add the entry point tags
  {
    DXIL::Metadata *entryPoints = editor.GetMetadataByName("dx.entryPoints");

    if(!entryPoints)
    {
      RDCERR("Couldn't find entry point list");
      return false;
    }

    // TODO select the entry point for multiple entry points? RT only for now
    DXIL::Metadata *entry = entryPoints->children[0];

    entryName = entry->children[1]->str;

    DXIL::Metadata *taglist = entry->children[4];

    // find existing shader flags tag, if there is one
    DXIL::Metadata *shaderFlagsTag = NULL;
    DXIL::Metadata *shaderFlagsData = NULL;
    size_t existingTag = 0;
    for(size_t t = 0; taglist && t < taglist->children.size(); t += 2)
    {
      RDCASSERT(taglist->children[t]->isConstant);
      if(taglist->children[t]->value.constant->val.u32v[0] ==
         (uint32_t)DXIL::ShaderEntryTag::ShaderFlags)
      {
        shaderFlagsTag = taglist->children[t];
        shaderFlagsData = taglist->children[t + 1];
        existingTag = t + 1;
        break;
      }
    }

    uint32_t shaderFlagsValue = shaderFlagsData ? shaderFlagsData->value.constant->val.u32v[0] : 0U;

    // raw and structured buffers
    shaderFlagsValue |= 0x10;

    if(editor.GetShaderType() != DXBC::ShaderType::Compute &&
       editor.GetShaderType() != DXBC::ShaderType::Pixel)
    {
      // UAVs on non-PS/CS stages
      shaderFlagsValue |= 0x10000;
    }

    // (re-)create shader flags tag
    shaderFlagsData = editor.AddMetadata(DXIL::Metadata());
    SET_CONST_META(shaderFlagsData, i32, shaderFlagsValue);

    // if we didn't have a shader tags entry at all, create the metadata node for the shader flags
    // tag
    if(!shaderFlagsTag)
    {
      shaderFlagsTag = editor.AddMetadata(DXIL::Metadata());
      SET_CONST_META(shaderFlagsTag, i32, (uint32_t)DXIL::ShaderEntryTag::ShaderFlags);
    }

    // (re-)create tag list
    if(taglist)
      taglist = editor.AddMetadata(*taglist);
    else
      taglist = editor.AddMetadata(DXIL::Metadata());

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

    // recreate the entry
    entry = editor.AddMetadata(*entry);

    // repoint taglist and reslist
    entry->children[3] = reslist;
    entry->children[4] = taglist;

    // finally repoint the named metadata
    entryPoints->children[0] = entry;
  }

  // get the editor to patch PSV0 with our extra UAV
  editor.RegisterUAV(DXIL::DXILResourceType::ByteAddressUAV, space, 0, 0,
                     DXIL::ResourceKind::RawBuffer);

  DXIL::Function *f = editor.GetFunctionByName(entryName);

  if(!f)
  {
    RDCERR("Couldn't find entry point function '%s'", entryName.c_str());
    return false;
  }

  // create our handle first thing
  DXIL::Instruction *handle;
  {
    DXIL::Instruction inst;
    inst.op = DXIL::Operation::Call;
    inst.type = handleType;
    inst.funcCall = createHandle;
    inst.args = {
        // dx.op.createHandle opcode
        DXIL::Value(editor.GetOrAddConstant(f, DXIL::Constant(i32, 57U))),
        // kind = UAV
        DXIL::Value(editor.GetOrAddConstant(f, DXIL::Constant(i8, (uint32_t)DXIL::HandleKind::UAV))),
        // ID/slot
        DXIL::Value(editor.GetOrAddConstant(f, DXIL::Constant(i32, regSlot))),
        // array index
        DXIL::Value(editor.GetOrAddConstant(f, DXIL::Constant(i32, 0U))),
        // non-uniform
        DXIL::Value(editor.GetOrAddConstant(f, DXIL::Constant(i1, 0U))),
    };

    handle = editor.AddInstruction(f, 0, inst);
  }

  const DXIL::Constant *undefi32;
  {
    DXIL::Constant c;
    c.type = i32;
    c.undef = true;
    undefi32 = editor.GetOrAddConstant(f, c);
  }

  // insert an or to offset 0, just to indicate validity
  {
    DXIL::Instruction inst;
    inst.op = DXIL::Operation::Call;
    inst.type = i32;
    inst.funcCall = atomicBinOp;
    inst.args = {
        // dx.op.atomicBinOp.i32 opcode
        DXIL::Value(editor.GetOrAddConstant(f, DXIL::Constant(i32, 78U))),
        // feedback UAV handle
        DXIL::Value(handle),
        // operation OR
        DXIL::Value(editor.GetOrAddConstant(f, DXIL::Constant(i32, 2U))),
        // offset
        DXIL::Value(editor.GetOrAddConstant(f, DXIL::Constant(i32, 0U))),
        // offset 2
        DXIL::Value(undefi32),
        // offset 3
        DXIL::Value(undefi32),
        // value
        DXIL::Value(editor.GetOrAddConstant(f, DXIL::Constant(i32, magicFeedbackValue))),
    };

    editor.AddInstruction(f, 1, inst);
  }

  for(size_t i = 2; i < f->instructions.size(); i++)
  {
    const DXIL::Instruction &inst = f->instructions[i];
    // we want to annotate any calls to createHandle
    if(inst.op == DXIL::Operation::Call && inst.funcCall->name == createHandle->name)
    {
      if(inst.args.size() != 5)
      {
        RDCERR("Unexpected number of arguments to createHandle");
        continue;
      }

      DXIL::Value kindArg = inst.args[1];
      DXIL::Value idArg = inst.args[2];
      DXIL::Value idxArg = inst.args[3];

      if(kindArg.type != DXIL::ValueType::Constant || idArg.type != DXIL::ValueType::Constant)
      {
        RDCERR("Unexpected non-constant argument to createHandle");
        continue;
      }

      DXIL::HandleKind kind = (DXIL::HandleKind)kindArg.constant->val.u32v[0];
      uint32_t id = idArg.constant->val.u32v[0];

      rdcpair<uint32_t, int32_t> slotInfo = {0, 0};

      if(kind == DXIL::HandleKind::SRV && id < srvBaseSlots.size())
        slotInfo = srvBaseSlots[id];
      else if(kind == DXIL::HandleKind::UAV && id < uavBaseSlots.size())
        slotInfo = uavBaseSlots[id];

      if(slotInfo.first == 0)
        continue;

      DXIL::Instruction op;
      op.op = DXIL::Operation::Sub;
      op.type = i32;
      op.args = {
          // idx to the createHandle op
          idxArg,
          // register that this is relative to
          DXIL::Value(editor.GetOrAddConstant(f, DXIL::Constant(i32, (uint32_t)slotInfo.second))),
      };

      // idx0Based = idx - baseReg
      DXIL::Instruction *idx0Based = editor.AddInstruction(f, i, op);
      i++;

      op.op = DXIL::Operation::Add;
      op.type = i32;
      op.args = {
          // idx to the createHandle op
          DXIL::Value(idx0Based),
          // base slot
          DXIL::Value(editor.GetOrAddConstant(f, DXIL::Constant(i32, slotInfo.first))),
      };

      // slotPlusBase = idx0Based + slot
      DXIL::Instruction *slotPlusBase = editor.AddInstruction(f, i, op);
      i++;

      op.op = DXIL::Operation::ShiftLeft;
      op.type = i32;
      op.args = {
          DXIL::Value(slotPlusBase), DXIL::Value(editor.GetOrAddConstant(f, DXIL::Constant(i32, 2U))),
      };

      // byteOffset = slotPlusBase << 2
      DXIL::Instruction *byteOffset = editor.AddInstruction(f, i, op);
      i++;

      op.op = DXIL::Operation::Call;
      op.type = i32;
      op.funcCall = atomicBinOp;
      op.args = {
          // dx.op.atomicBinOp.i32 opcode
          DXIL::Value(editor.GetOrAddConstant(f, DXIL::Constant(i32, 78U))),
          // feedback UAV handle
          DXIL::Value(handle),
          // operation OR
          DXIL::Value(editor.GetOrAddConstant(f, DXIL::Constant(i32, 2U))),
          // offset
          DXIL::Value(byteOffset),
          // offset 2
          DXIL::Value(undefi32),
          // offset 3
          DXIL::Value(undefi32),
          // value
          DXIL::Value(editor.GetOrAddConstant(f, DXIL::Constant(i32, magicFeedbackValue))),
      };

      // don't care about the return value from this
      editor.AddInstruction(f, i, op);
      i++;
    }
  }

  return true;
}

static void AddArraySlots(WrappedID3D12PipelineState::ShaderEntry *shad, uint32_t space,
                          uint32_t maxDescriptors,
                          std::map<D3D12FeedbackKey, D3D12FeedbackSlot> &slots, uint32_t &numSlots,
                          bytebuf &editedBlob, D3D12_SHADER_BYTECODE &desc)
{
  if(!shad)
    return;

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

  // if we haven't encountered any array slots, no need to do any patching
  if(numSlots == numReservedSlots)
    return;

  // only SM5.1 can have dynamic array indexing
  if(shad->GetDXBC()->m_Version.Major == 5 && shad->GetDXBC()->m_Version.Minor == 1)
  {
    if(AnnotateDXBCShader(shad->GetDXBC(), space, slots, editedBlob))
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
    if(AnnotateDXILShader(shad->GetDXBC(), space, slots, editedBlob))
    {
      // strip ILDB because it's valid code (with debug info) and who knows what might use it
      DXBC::DXBCContainer::StripChunk(editedBlob, DXBC::FOURCC_ILDB);

      if(!D3D12_Debug_FeedbackDumpDirPath().empty())
      {
        bytebuf orig = shad->GetDXBC()->GetShaderBlob();

        DXBC::DXBCContainer::StripChunk(orig, DXBC::FOURCC_ILDB);

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
}

void D3D12Replay::FetchShaderFeedback(uint32_t eventId)
{
  if(m_BindlessFeedback.Usage.find(eventId) != m_BindlessFeedback.Usage.end())
    return;

  if(!D3D12_BindlessFeedback())
    return;

  if(m_pDevice->HasFatalError())
    return;

  // create it here so we won't re-run any code if the event is re-selected. We'll mark it as valid
  // if it actually has any data in it later.
  D3D12DynamicShaderFeedback &result = m_BindlessFeedback.Usage[eventId];

  const ActionDescription *action = m_pDevice->GetAction(eventId);

  if(action == NULL || !(action->flags & (ActionFlags::Dispatch | ActionFlags::Drawcall)))
    return;

  result.compute = bool(action->flags & ActionFlags::Dispatch);

  D3D12RenderState &rs = m_pDevice->GetQueue()->GetCommandData()->m_RenderState;

  D3D12ResourceManager *rm = m_pDevice->GetResourceManager();

  WrappedID3D12PipelineState *pipe =
      (WrappedID3D12PipelineState *)rm->GetCurrentAs<ID3D12PipelineState>(rs.pipe);
  D3D12RootSignature modsig;

  if(!pipe)
  {
    RDCERR("Can't fetch shader feedback, no pipeline state bound");
    return;
  }

  bytebuf editedBlob[5];

  D3D12_EXPANDED_PIPELINE_STATE_STREAM_DESC pipeDesc;
  pipe->Fill(pipeDesc);

  uint32_t space = 1;

  uint32_t maxDescriptors = 0;
  for(ResourceId id : rs.heaps)
  {
    D3D12_DESCRIPTOR_HEAP_DESC desc = rm->GetCurrentAs<ID3D12DescriptorHeap>(id)->GetDesc();

    if(desc.Type == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)
    {
      maxDescriptors = desc.NumDescriptors;
      RDCDEBUG("Clamping any unbounded ranges to %u descriptors", maxDescriptors);
      break;
    }
  }

  std::map<D3D12FeedbackKey, D3D12FeedbackSlot> slots[6];

  // reserve the first 4 dwords for debug info and a validity flag
  uint32_t numSlots = numReservedSlots;

#if ENABLED(RDOC_DEVEL)
  m_pDevice->GetShaderCache()->LoadDXC();
#endif

  if(result.compute)
  {
    ID3D12RootSignature *sig = rm->GetCurrentAs<ID3D12RootSignature>(rs.compute.rootsig);

    if(!sig)
      return;

    modsig = ((WrappedID3D12RootSignature *)sig)->sig;

    space = modsig.maxSpaceIndex;

    AddArraySlots(pipe->CS(), space, maxDescriptors, slots[0], numSlots, editedBlob[0], pipeDesc.CS);
  }
  else
  {
    ID3D12RootSignature *sig = rm->GetCurrentAs<ID3D12RootSignature>(rs.graphics.rootsig);

    if(!sig)
      return;

    modsig = ((WrappedID3D12RootSignature *)sig)->sig;

    space = modsig.maxSpaceIndex;

    AddArraySlots(pipe->VS(), space, maxDescriptors, slots[0], numSlots, editedBlob[0], pipeDesc.VS);
    AddArraySlots(pipe->HS(), space, maxDescriptors, slots[1], numSlots, editedBlob[1], pipeDesc.HS);
    AddArraySlots(pipe->DS(), space, maxDescriptors, slots[2], numSlots, editedBlob[2], pipeDesc.DS);
    AddArraySlots(pipe->GS(), space, maxDescriptors, slots[3], numSlots, editedBlob[3], pipeDesc.GS);
    AddArraySlots(pipe->PS(), space, maxDescriptors, slots[4], numSlots, editedBlob[4], pipeDesc.PS);
  }

  // if numSlots wasn't increased, none of the resources were arrayed so we have nothing to do.
  // Silently return
  if(numSlots == numReservedSlots)
    return;

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
    desc.Width = numSlots * sizeof(uint32_t);

    D3D12_HEAP_PROPERTIES heapProps;
    heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
    heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    heapProps.CreationNodeMask = 1;
    heapProps.VisibleNodeMask = 1;

    HRESULT hr = m_pDevice->CreateCommittedResource(
        &heapProps, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, NULL,
        __uuidof(ID3D12Resource), (void **)&m_BindlessFeedback.FeedbackBuffer);

    if(m_BindlessFeedback.FeedbackBuffer == NULL || FAILED(hr))
    {
      RDCERR("Couldn't create feedback buffer with %u slots: %s", numSlots, ToStr(hr).c_str());
      return;
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
      return;

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
      RDCERR("Couldn't create feedback modified root signature: %s", ToStr(hr).c_str());
      return;
    }
  }

  ID3D12PipelineState *annotatedPipe = NULL;

  {
    pipeDesc.pRootSignature = annotatedSig;

    HRESULT hr = m_pDevice->CreatePipeState(pipeDesc, &annotatedPipe);
    if(annotatedPipe == NULL || FAILED(hr))
    {
      SAFE_RELEASE(annotatedSig);
      RDCERR("Couldn't create feedback modified pipeline: %s", ToStr(hr).c_str());
      return;
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

  m_pDevice->ReplayLog(0, eventId, eReplay_OnlyDraw);

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
              case D3D12_SHADER_VISIBILITY_ALL: visMask = result.compute ? 0x1 : 0xff; break;
              case D3D12_SHADER_VISIBILITY_VERTEX: visMask = 1 << 0; break;
              case D3D12_SHADER_VISIBILITY_HULL: visMask = 1 << 1; break;
              case D3D12_SHADER_VISIBILITY_DOMAIN: visMask = 1 << 2; break;
              case D3D12_SHADER_VISIBILITY_GEOMETRY: visMask = 1 << 3; break;
              case D3D12_SHADER_VISIBILITY_PIXEL: visMask = 1 << 4; break;
              default: RDCERR("Unexpected shader visibility %d", p.ShaderVisibility); return;
            }

            // set the key type
            if(range.RangeType == D3D12_DESCRIPTOR_RANGE_TYPE_SRV)
              curKey.type = DXBCBytecode::TYPE_RESOURCE;
            else if(range.RangeType == D3D12_DESCRIPTOR_RANGE_TYPE_UAV)
              curKey.type = DXBCBytecode::TYPE_UNORDERED_ACCESS_VIEW;

            for(uint32_t st = 0; st < 5; st++)
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

      std::sort(result.used.begin(), result.used.end());
    }
    else
    {
      RDCERR("Didn't get valid feedback identifier. Expected %08x got %08x", magicFeedbackValue,
             slotsData[0]);
    }
  }

  // replay from the start as we may have corrupted state while fetching the above feedback.
  m_pDevice->ReplayLog(0, eventId, eReplay_Full);
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
