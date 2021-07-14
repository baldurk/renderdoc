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
#include "d3d12_command_queue.h"
#include "d3d12_debug.h"
#include "d3d12_device.h"
#include "d3d12_replay.h"
#include "d3d12_resources.h"
#include "d3d12_shader_cache.h"

RDOC_CONFIG(rdcstr, D3D12_Debug_FeedbackDumpDirPath, "",
            "Path to dump bindless feedback annotation generated DXBC/DXIL files.");
RDOC_CONFIG(bool, D3D12_Experimental_BindlessFeedback, true,
            "EXPERIMENTAL: Enable fetching from GPU which descriptors were dynamically used in "
            "descriptor arrays.");

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

static bool AnnotateShader(const DXBC::DXBCContainer *dxbc, uint32_t space,
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

      // the operand should be relative addressing like r0.x + 6 for a t6 resource being indexed
      // with [r0.x]
      RDCASSERT(operand.indices[1].relative &&
                operand.indices[1].index == decl->operand.indices[1].index);

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

      // should be getting a scalar index
      if(operand.indices[1].operand.comps[1] != 0xff ||
         operand.indices[1].operand.comps[2] != 0xff || operand.indices[1].operand.comps[3] != 0xff)
      {
        RDCERR("Unexpected vector index for resource: %s",
               operand.toString(dxbc->GetReflection(), ToString::None).c_str());
        continue;
      }

      if(u.first == ~0U && u.second == ~0U)
        u = editor.DeclareUAV(desc, space, 0, 0);

      // resource base plus index
      editor.InsertOperation(i++, oper(OPCODE_IADD, {temp(t).swizzle(0), imm(it->second.Slot()),
                                                     operand.indices[1].operand}));
      // multiply by 4 for byte index
      editor.InsertOperation(i++,
                             oper(OPCODE_ISHL, {temp(t).swizzle(0), temp(t).swizzle(0), imm(2)}));
      // atomic or the slot
      editor.InsertOperation(i++, oper(OPCODE_ATOMIC_OR, {uav(u), temp(t).swizzle(0), imm(~0U)}));

      // only one resource operand per instruction
      break;
    }
  }

  if(u.first != ~0U || u.second != ~0U)
  {
    editor.InsertOperation(0, oper(OPCODE_MOV, {temp(t).swizzle(0), imm(0)}));
    editor.InsertOperation(1, oper(OPCODE_ATOMIC_OR, {uav(u), temp(t).swizzle(0), imm(~0U)}));
    return true;
  }

  return false;
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

  if(shad->GetDXBC()->m_Version.Major > 6)
  {
    RDCERR("DXIL shaders are not supported for bindless feedback currently");
  }
  else
  {
    // only SM5.1 can have dynamic array indexing
    if(shad->GetDXBC()->m_Version.Major == 5 && shad->GetDXBC()->m_Version.Minor == 1)
    {
      if(AnnotateShader(shad->GetDXBC(), space, slots, editedBlob))
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
  }
}

void D3D12Replay::FetchShaderFeedback(uint32_t eventId)
{
  if(m_BindlessFeedback.Usage.find(eventId) != m_BindlessFeedback.Usage.end())
    return;

  if(!D3D12_Experimental_BindlessFeedback())
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
  uint32_t numSlots = 4;

  if(result.compute)
  {
    ID3D12RootSignature *sig = rm->GetCurrentAs<ID3D12RootSignature>(rs.compute.rootsig);
    modsig = ((WrappedID3D12RootSignature *)sig)->sig;

    space = modsig.maxSpaceIndex;

    AddArraySlots(pipe->CS(), space, maxDescriptors, slots[0], numSlots, editedBlob[0], pipeDesc.CS);
  }
  else
  {
    ID3D12RootSignature *sig = rm->GetCurrentAs<ID3D12RootSignature>(rs.graphics.rootsig);
    modsig = ((WrappedID3D12RootSignature *)sig)->sig;

    space = modsig.maxSpaceIndex;

    AddArraySlots(pipe->VS(), space, maxDescriptors, slots[0], numSlots, editedBlob[0], pipeDesc.VS);
    AddArraySlots(pipe->HS(), space, maxDescriptors, slots[1], numSlots, editedBlob[1], pipeDesc.HS);
    AddArraySlots(pipe->DS(), space, maxDescriptors, slots[2], numSlots, editedBlob[2], pipeDesc.DS);
    AddArraySlots(pipe->GS(), space, maxDescriptors, slots[3], numSlots, editedBlob[3], pipeDesc.GS);
    AddArraySlots(pipe->PS(), space, maxDescriptors, slots[4], numSlots, editedBlob[4], pipeDesc.PS);
  }

  // if numSlots was 0, none of the resources were arrayed so we have nothing to do. Silently return
  if(numSlots == 0)
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

    result.valid = true;

    // now we iterate over descriptor ranges and find which (of potentially multiple) registers each
    // descriptor maps to and store the index if it's dynamically or statically used. We do this
    // here so it only happens once instead of doing it when looking up the data.

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
          curKey.bind.bind = range.BaseShaderRegister;

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
              // the feedback entries start here
              auto slotIt = slots[st].lower_bound(curKey);

              curIdentifier.descIndex = 0;

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
                if((uint32_t)bind.bind > range.BaseShaderRegister + num)
                  break;

                int32_t lastBind = bind.bind + (int32_t)RDCCLAMP(bind.arraySize, 1U, maxDescriptors);

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
  }

  // replay from the start as we may have corrupted state while fetching the above feedback.
  m_pDevice->ReplayLog(0, eventId, eReplay_Full);
}

void D3D12Replay::ClearFeedbackCache()
{
  m_BindlessFeedback.Usage.clear();
}
