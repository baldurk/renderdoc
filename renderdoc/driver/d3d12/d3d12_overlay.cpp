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

#include "common/shader_cache.h"
#include "core/settings.h"
#include "data/resource.h"
#include "driver/dx/official/d3dcompiler.h"
#include "driver/dxgi/dxgi_common.h"
#include "driver/shaders/dxbc/dxbc_bytecode_editor.h"
#include "driver/shaders/dxil/dxil_bytecode_editor.h"
#include "driver/shaders/dxil/dxil_common.h"
#include "maths/formatpacking.h"
#include "maths/matrix.h"
#include "maths/vec.h"
#include "serialise/streamio.h"
#include "stb/stb_truetype.h"
#include "strings/string_utils.h"
#include "d3d12_command_list.h"
#include "d3d12_command_queue.h"
#include "d3d12_debug.h"
#include "d3d12_device.h"
#include "d3d12_replay.h"
#include "d3d12_shader_cache.h"

#include "data/hlsl/hlsl_cbuffers.h"

RDOC_CONFIG(rdcstr, D3D12_Debug_OverlayDumpDirPath, "",
            "Path to dump quad overdraw patched DXIL files.");

RDOC_EXTERN_CONFIG(bool, D3D12_Debug_SingleSubmitFlushing);

struct D3D12QuadOverdrawCallback : public D3D12ActionCallback
{
  D3D12QuadOverdrawCallback(WrappedID3D12Device *dev, const rdcarray<uint32_t> &events,
                            ID3D12Resource *depth, ID3D12Resource *msdepth, PortableHandle dsv,
                            PortableHandle uav)
      : m_pDevice(dev), m_Events(events), m_Depth(depth), m_MSDepth(msdepth), m_DSV(dsv), m_UAV(uav)
  {
    m_pDevice->GetQueue()->GetCommandData()->m_ActionCallback = this;
  }
  ~D3D12QuadOverdrawCallback() { m_pDevice->GetQueue()->GetCommandData()->m_ActionCallback = NULL; }
  void PreDraw(uint32_t eid, ID3D12GraphicsCommandListX *cmd)
  {
    if(!m_Events.contains(eid))
      return;

    // we customise the pipeline to disable framebuffer writes, but perform normal testing
    // and substitute our quad calculation fragment shader that writes to a storage image
    // that is bound in a new root signature element.

    D3D12RenderState &rs = m_pDevice->GetQueue()->GetCommandData()->GetCurRenderState();
    m_PrevState = rs;

    // check cache first
    CachedPipeline cache = m_PipelineCache[rs.pipe];

    // if we don't get a hit, create a modified pipeline
    if(cache.pipe == NULL)
    {
      HRESULT hr = S_OK;

      WrappedID3D12RootSignature *sig =
          m_pDevice->GetResourceManager()->GetCurrentAs<WrappedID3D12RootSignature>(
              rs.graphics.rootsig);

      // need to be able to add a descriptor table with our UAV without hitting the 64 DWORD limit
      RDCASSERT(sig->sig.dwordLength < 64);

      D3D12RootSignature modsig = sig->sig;

      uint32_t regSpace = GetFreeRegSpace(modsig, QUADOVERDRAW_UAV_SPACE, D3D12DescriptorType::UAV,
                                          D3D12_SHADER_VISIBILITY_PIXEL);

      if(regSpace != QUADOVERDRAW_UAV_SPACE)
      {
        RDCLOG("Register space %u wasn't available for use, recompiling shaders",
               QUADOVERDRAW_UAV_SPACE);
      }

      D3D12_DESCRIPTOR_RANGE1 range;
      range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
      range.NumDescriptors = 1;
      range.BaseShaderRegister = 0;
      range.RegisterSpace = regSpace;
      range.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_NONE;
      range.OffsetInDescriptorsFromTableStart = 0;

      modsig.Flags &= ~D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;
      modsig.Parameters.push_back(D3D12RootSignatureParameter());
      D3D12RootSignatureParameter &param = modsig.Parameters.back();
      param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
      param.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
      param.DescriptorTable.NumDescriptorRanges = 1;
      param.DescriptorTable.pDescriptorRanges = &range;

      cache.sigElem = uint32_t(modsig.Parameters.size() - 1);

      ID3DBlob *root = m_pDevice->GetShaderCache()->MakeRootSig(modsig);

      hr = m_pDevice->CreateRootSignature(0, root->GetBufferPointer(), root->GetBufferSize(),
                                          __uuidof(ID3D12RootSignature), (void **)&cache.sig);
      RDCASSERTEQUAL(hr, S_OK);

      SAFE_RELEASE(root);

      WrappedID3D12PipelineState *origPSO =
          m_pDevice->GetResourceManager()->GetCurrentAs<WrappedID3D12PipelineState>(rs.pipe);

      RDCASSERT(origPSO->IsGraphics());

      D3D12_EXPANDED_PIPELINE_STATE_STREAM_DESC pipeDesc;
      origPSO->Fill(pipeDesc);

      for(size_t i = 0; i < ARRAY_COUNT(pipeDesc.BlendState.RenderTarget); i++)
        pipeDesc.BlendState.RenderTarget[i].RenderTargetWriteMask = 0;

      // disable depth/stencil writes
      pipeDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
      pipeDesc.DepthStencilState.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
      pipeDesc.DepthStencilState.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
      pipeDesc.DepthStencilState.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
      pipeDesc.DepthStencilState.BackFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
      pipeDesc.DepthStencilState.BackFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
      pipeDesc.DepthStencilState.BackFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
      pipeDesc.DepthStencilState.FrontFace.StencilWriteMask = 0;
      pipeDesc.DepthStencilState.BackFace.StencilWriteMask = 0;

      // disable any multisampling
      pipeDesc.SampleDesc.Count = 1;
      pipeDesc.SampleDesc.Quality = 0;

      bool dxil =
          pipeDesc.MS.BytecodeLength > 0 ||
          DXBC::DXBCContainer::CheckForDXIL(pipeDesc.VS.pShaderBytecode, pipeDesc.VS.BytecodeLength);

      // dxil is stricter about pipeline signatures matching. On D3D11 there's an error but all
      // drivers handle a PS that reads no VS outputs and only screenspace SV_Position and
      // SV_Coverage. On D3D12 we need to patch to generate a new PS
      m_pDevice->GetReplay()->PatchQuadWritePS(pipeDesc, regSpace, dxil);
      if(pipeDesc.PS.BytecodeLength == 0)
      {
        m_pDevice->AddDebugMessage(
            MessageCategory::Shaders, MessageSeverity::High, MessageSource::UnsupportedConfiguration,
            StringFormat::Fmt("No quad write %s shader available for overlay",
                              dxil ? "DXIL" : "DXBC"));
        return;
      }

      pipeDesc.pRootSignature = cache.sig;

      hr = m_pDevice->CreatePipeState(pipeDesc, &cache.pipe);
      RDCASSERTEQUAL(hr, S_OK);

      m_PipelineCache[rs.pipe] = cache;
    }

    // modify state for first action call
    rs.pipe = GetResID(cache.pipe);
    rs.graphics.rootsig = GetResID(cache.sig);

    rs.rts.clear();
    if(m_Depth)
    {
      rs.dsv = *DescriptorFromPortableHandle(m_pDevice->GetResourceManager(), m_DSV);

      D3D12_RESOURCE_BARRIER b = {};

      b.Transition.pResource = m_Depth;
      b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
      b.Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
      b.Transition.StateAfter = D3D12_RESOURCE_STATE_DEPTH_WRITE;
      cmd->ResourceBarrier(1, &b);

      b.Transition.pResource = m_MSDepth;
      b.Transition.StateBefore = D3D12_RESOURCE_STATE_DEPTH_WRITE;
      b.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
      cmd->ResourceBarrier(1, &b);

      m_pDevice->GetDebugManager()->CopyTex2DMSToArray(Unwrap(cmd), Unwrap(m_Depth),
                                                       Unwrap(m_MSDepth));
    }

    AddDebugDescriptorsToRenderState(m_pDevice, rs, {m_UAV}, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
                                     cache.sigElem, m_CopiedHeaps);

    // as we're changing the root signature, we need to reapply all elements,
    // so just apply all state
    if(cmd)
      rs.ApplyState(m_pDevice, cmd);
  }

  bool PostDraw(uint32_t eid, ID3D12GraphicsCommandListX *cmd)
  {
    if(!m_Events.contains(eid))
      return false;

    if(m_Depth)
    {
      D3D12_RESOURCE_BARRIER b = {};

      b.Transition.pResource = m_Depth;
      b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
      b.Transition.StateBefore = D3D12_RESOURCE_STATE_DEPTH_WRITE;
      b.Transition.StateAfter = D3D12_RESOURCE_STATE_COMMON;
      cmd->ResourceBarrier(1, &b);

      b.Transition.pResource = m_MSDepth;
      b.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
      b.Transition.StateAfter = D3D12_RESOURCE_STATE_DEPTH_WRITE;
      cmd->ResourceBarrier(1, &b);
    }

    // restore the render state and go ahead with the real action
    m_pDevice->GetQueue()->GetCommandData()->GetCurRenderState() = m_PrevState;

    RDCASSERT(cmd);
    m_pDevice->GetQueue()->GetCommandData()->GetCurRenderState().ApplyState(m_pDevice, cmd);

    return true;
  }

  void PostRedraw(uint32_t eid, ID3D12GraphicsCommandListX *cmd)
  {
    // nothing to do
  }

  // Dispatches don't rasterize, so do nothing
  void PreDispatch(uint32_t eid, ID3D12GraphicsCommandListX *cmd) {}
  bool PostDispatch(uint32_t eid, ID3D12GraphicsCommandListX *cmd) { return false; }
  void PostRedispatch(uint32_t eid, ID3D12GraphicsCommandListX *cmd) {}
  // Ditto copy/etc
  void PreMisc(uint32_t eid, ActionFlags flags, ID3D12GraphicsCommandListX *cmd) {}
  bool PostMisc(uint32_t eid, ActionFlags flags, ID3D12GraphicsCommandListX *cmd) { return false; }
  void PostRemisc(uint32_t eid, ActionFlags flags, ID3D12GraphicsCommandListX *cmd) {}
  void PreCloseCommandList(ID3D12GraphicsCommandListX *cmd) {}
  void AliasEvent(uint32_t primary, uint32_t alias)
  {
    // don't care
  }

  WrappedID3D12Device *m_pDevice;
  const rdcarray<uint32_t> &m_Events;
  PortableHandle m_UAV;
  PortableHandle m_DSV;
  ID3D12Resource *m_Depth, *m_MSDepth;

  // cache modified pipelines
  struct CachedPipeline
  {
    ID3D12RootSignature *sig;
    uint32_t sigElem;
    ID3D12PipelineState *pipe;
  };
  std::map<ResourceId, CachedPipeline> m_PipelineCache;
  std::set<ResourceId> m_CopiedHeaps;
  D3D12RenderState m_PrevState;
};

static void SetRTVDesc(D3D12_RENDER_TARGET_VIEW_DESC &rtDesc, const D3D12_RESOURCE_DESC &texDesc,
                       const RenderOutputSubresource &sub)
{
  if(texDesc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE3D && texDesc.DepthOrArraySize > 1)
  {
    if(texDesc.SampleDesc.Count > 1)
    {
      rtDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DMSARRAY;
      rtDesc.Texture2DMSArray.FirstArraySlice = sub.slice;
      rtDesc.Texture2DMSArray.ArraySize = sub.numSlices;
    }
    else
    {
      rtDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
      rtDesc.Texture2DArray.FirstArraySlice = sub.slice;
      rtDesc.Texture2DArray.ArraySize = sub.numSlices;
      rtDesc.Texture2DArray.MipSlice = sub.mip;
    }
  }
  else
  {
    rtDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
    rtDesc.Texture2D.MipSlice = sub.mip;

    if(texDesc.SampleDesc.Count > 1)
      rtDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DMS;
  }
}

void D3D12Replay::PatchQuadWritePS(D3D12_EXPANDED_PIPELINE_STATE_STREAM_DESC &pipeDesc,
                                   uint32_t regSpace, bool dxil)
{
  pipeDesc.PS.pShaderBytecode = NULL;
  pipeDesc.PS.BytecodeLength = 0;

  ID3DBlob *quadWriteBlob = dxil ? m_Overlay.QuadOverdrawWriteDXILPS : m_Overlay.QuadOverdrawWritePS;

  if(regSpace != QUADOVERDRAW_UAV_SPACE)
  {
    rdcstr hlsl = GetEmbeddedResource(quadoverdraw_hlsl);

    hlsl =
        "#define D3D12 1\n\n" + StringFormat::Fmt("#define UAV_SPACE space%u\n\n", regSpace) + hlsl;

    if(dxil)
    {
      m_pDevice->GetShaderCache()->GetShaderBlob(hlsl.c_str(), "RENDERDOC_QuadOverdrawPS",
                                                 D3DCOMPILE_WARNINGS_ARE_ERRORS, {}, "ps_6_0",
                                                 &quadWriteBlob);
    }
    else
    {
      m_pDevice->GetShaderCache()->GetShaderBlob(hlsl.c_str(), "RENDERDOC_QuadOverdrawPS",
                                                 D3DCOMPILE_WARNINGS_ARE_ERRORS, {}, "ps_5_1",
                                                 &quadWriteBlob);
    }
  }

  if(!quadWriteBlob)
  {
    RDCERR("Compiled quad overdraw write %s blob isn't available", dxil ? "DXIL" : "DXBC");
    return;
  }

  D3D12_SHADER_BYTECODE *rastFeeding = &pipeDesc.VS;

  if(pipeDesc.DS.BytecodeLength > 0)
    rastFeeding = &pipeDesc.DS;
  if(pipeDesc.GS.BytecodeLength > 0)
    rastFeeding = &pipeDesc.GS;
  if(pipeDesc.MS.BytecodeLength > 0)
    rastFeeding = &pipeDesc.MS;

  uint32_t hash[4];
  DXBC::DXBCContainer::GetHash(hash, rastFeeding->pShaderBytecode, rastFeeding->BytecodeLength);

  rdcfixedarray<uint32_t, 4> key = hash;

  bytebuf &patchedPs = m_PatchedPSCache[key];

  // check if we have this shader's matching PS cached already
  if(!patchedPs.empty())
  {
    pipeDesc.PS.pShaderBytecode = patchedPs.data();
    pipeDesc.PS.BytecodeLength = patchedPs.size();
    return;
  }

  bytebuf rastFeedingBytes((const byte *)rastFeeding->pShaderBytecode, rastFeeding->BytecodeLength);

  // get the DXBC for the previous stage
  DXBC::DXBCContainer rastFeedingDXBC(rastFeedingBytes, rdcstr(), GraphicsAPI::D3D12, ~0U, ~0U);

  bytebuf patchedDXBC((const byte *)quadWriteBlob->GetBufferPointer(),
                      quadWriteBlob->GetBufferSize());

  // if the previous stage already outputs position as the first register, we're done as the
  // precompiled quadwrite will be compatible! no patching necessary
  if(rastFeedingDXBC.GetReflection()->OutputSig.size() >= 1 &&
     rastFeedingDXBC.GetReflection()->OutputSig[0].regIndex == 0 &&
     rastFeedingDXBC.GetReflection()->OutputSig[0].systemValue == ShaderBuiltin::Position)
  {
    patchedDXBC.swap(patchedPs);

    pipeDesc.PS.pShaderBytecode = patchedPs.data();
    pipeDesc.PS.BytecodeLength = patchedPs.size();
    return;
  }

  if(!D3D12_Debug_OverlayDumpDirPath().empty())
    FileIO::WriteAll(D3D12_Debug_OverlayDumpDirPath() + "/before_quadps.dxbc", patchedDXBC);

  DXBC::DXBCContainer quadOverdrawDXBC(patchedDXBC, rdcstr(), GraphicsAPI::D3D12, ~0U, ~0U);

  if(dxil)
  {
    using namespace DXIL;

    rdcstr stringTable;
    stringTable.push_back('\0');

    rdcarray<uint32_t> semanticIndexTable;

    rdcarray<uint32_t> stringTableOffsets;
    rdcarray<uint32_t> semanticIndexTableOffsets;

    // !!!NOTE!!!
    //
    // In the DXIL editing below we directly reference the raster feed DXIL metadata from the edited
    // metadata. This is fine as long as the metadata 'externally' passed into the editor has
    // lifetime longer than the editor
    {
      // use a local bytebuf so that if we error out, patchedPs above won't be modified
      ProgramEditor editor(&quadOverdrawDXBC, patchedDXBC);

      // We need to make two changes: copy the raster-feeding shader's output signature wholesale
      // into the pixel shader. It only needs position, which *must* have been written by
      // definition, the coverage input comes from an intrinsic. None of the properties should need
      // to change, so it's a pure deep copy of metadata and properties to ensure a compatible
      // signature.
      //
      // After that, we need to find the input load ops in the original shader, and patch the row it
      // refers to (it would have been 0 previously). Since position is a full float4 we shouldn't
      // have to change anything else

      const Metadata *rastEntryPoints =
          rastFeedingDXBC.GetDXILByteCode()->GetMetadataByName("dx.entryPoints");

      if(!rastEntryPoints)
      {
        RDCERR("Couldn't find entry point list");
        return;
      }

      // TODO select the entry point for multiple entry points? RT only for now
      RDCASSERT(rastEntryPoints->children.size() > 0 && rastEntryPoints->children[0]);
      const Metadata *rastEntry = rastEntryPoints->children[0];

      RDCASSERT(rastEntry->children.size() > 2 && rastEntry->children[2]);
      const Metadata *rastSigs = rastEntry->children[2];

      RDCASSERT(rastSigs->children.size() > 1 && rastSigs->children[1]);
      const Metadata *rastOutSig = rastSigs->children[1];

      Metadata *quadPSentryPoints = editor.GetMetadataByName("dx.entryPoints");

      if(!quadPSentryPoints)
      {
        RDCERR("Couldn't find entry point list");
        return;
      }

      // TODO select the entry point for multiple entry points? RT only for now
      RDCASSERT(quadPSentryPoints->children.size() > 0 && quadPSentryPoints->children[0]);
      Metadata *quadPSentry = quadPSentryPoints->children[0];

      rdcstr entryName = quadPSentry->children[1]->str;

      RDCASSERT(quadPSentry->children.size() > 2 && quadPSentry->children[2]);
      Metadata *quadPSsigs = quadPSentry->children[2];

      RDCASSERT(quadPSsigs->children.size() > 0);

      // create new input signature list
      Metadata *newInSig = quadPSsigs->children[0] = editor.CreateMetadata();

      uint32_t posID = ~0U;

      // process signature to get string table & index table for semantics
      for(size_t i = 0; i < rastOutSig->children.size(); i++)
      {
        const Metadata *inSigEl = rastOutSig->children[i];

        Metadata *outSigEl = editor.CreateMetadata();
        newInSig->children.push_back(outSigEl);

        outSigEl->children = {
            editor.CreateConstantMetadata(cast<Constant>(inSigEl->children[0]->value)->getU32()),
            editor.CreateConstantMetadata(inSigEl->children[1]->str),
            editor.CreateConstantMetadata(
                (uint8_t)cast<Constant>(inSigEl->children[2]->value)->getU32()),
            editor.CreateConstantMetadata(
                (uint8_t)cast<Constant>(inSigEl->children[3]->value)->getU32()),
            editor.CreateMetadata(),
            editor.CreateConstantMetadata(
                (uint8_t)cast<Constant>(inSigEl->children[5]->value)->getU32()),
            editor.CreateConstantMetadata(cast<Constant>(inSigEl->children[6]->value)->getU32()),
            editor.CreateConstantMetadata(
                (uint8_t)cast<Constant>(inSigEl->children[7]->value)->getU32()),
            editor.CreateConstantMetadata(cast<Constant>(inSigEl->children[8]->value)->getU32()),
            editor.CreateConstantMetadata(
                (uint8_t)cast<Constant>(inSigEl->children[9]->value)->getU32()),
            editor.CreateMetadata(),
        };

        // only append non-system values to the string table
        uint32_t systemValue = cast<Constant>(inSigEl->children[3]->value)->getU32();
        if(systemValue == 0)
        {
          stringTableOffsets.push_back((uint32_t)stringTable.size());
          stringTable.append(inSigEl->children[1]->str);
          stringTable.push_back('\0');
        }
        else
        {
          stringTableOffsets.push_back(0);

          // SV_Position is 3
          if(systemValue == 3)
            posID = cast<Constant>(inSigEl->children[0]->value)->getU32();
        }

        rdcarray<uint32_t> semIndexValues;

        // semantic indices
        if(const Metadata *semIdxs = inSigEl->children[4])
        {
          // the semantic index node is a list of constants
          for(size_t sidx = 0; sidx < semIdxs->children.size(); sidx++)
          {
            semIndexValues.push_back(cast<Constant>(semIdxs->children[sidx]->value)->getU32());
            outSigEl->children[4]->children.push_back(
                editor.CreateConstantMetadata(semIndexValues.back()));
          }
        }

        if(const Metadata *props = inSigEl->children[10])
        {
          for(size_t sidx = 0; sidx < props->children.size(); sidx++)
            outSigEl->children[10]->children.push_back(editor.CreateConstantMetadata(
                cast<Constant>(props->children[sidx]->value)->getU32()));
        }

        size_t tableOffset = ~0U;

        // try to find semIndexValues in semanticIndexTable
        for(size_t offs = 0; offs + semIndexValues.size() <= semanticIndexTable.size(); offs++)
        {
          bool match = true;
          for(size_t sidx = 0; sidx < semIndexValues.size(); sidx++)
          {
            if(semanticIndexTable[offs + sidx] != semIndexValues[sidx])
            {
              match = false;
              break;
            }
          }

          if(match)
          {
            tableOffset = offs;
            break;
          }
        }

        // if we didn't find it, append
        if(tableOffset == ~0U)
        {
          tableOffset = semanticIndexTable.size();
          semanticIndexTable.append(semIndexValues);
        }

        semanticIndexTableOffsets.push_back((uint32_t)tableOffset);
      }

      if(posID == ~0U)
      {
        RDCERR("Couldn't find position output in previous shader");
        return;
      }

      Function *f = editor.GetFunctionByName(entryName);

      if(!f)
      {
        RDCERR("Couldn't find entry point function '%s'", entryName.c_str());
        return;
      }

      Value *inputIDValue = editor.CreateConstant(posID);

      // now locate the loadInputs and patch the row they refer to. We can unconditionally patch
      // them all as there was only one input previously
      for(size_t i = 0; i < f->instructions.size(); i++)
      {
        Instruction &inst = *f->instructions[i];

        if(inst.op == Operation::Call && inst.getFuncCall()->name == "dx.op.loadInput.f32")
        {
          if(inst.args.size() != 5)
          {
            RDCERR("Unexpected number of arguments to createHandle");
            continue;
          }

          // arg[0] is the loadInput magic number
          // arg[1] is the ID we want to patch

          inst.args[1] = inputIDValue;
        }
      }
    }

    {
      // do a horrible franken-patch to merge the PSV0 chunks. We use the header from the existing
      // PS,
      // change the number of declared input elements, then copy the signature elements from the
      // last
      // shader's chunk. We can't copy the whole string table because that will likely include other
      // strings and then the damned thing won't match according to the runtime's validation.
      size_t rastPsv0Size = 0;
      const byte *rastPsv0Bytes =
          DXBC::DXBCContainer::FindChunk(rastFeedingBytes, DXBC::FOURCC_PSV0, rastPsv0Size);
      StreamReader rastPsv0(rastPsv0Bytes, rastPsv0Size);

      size_t psPsv0Size = 0;
      const byte *psPsv0Bytes =
          DXBC::DXBCContainer::FindChunk(patchedDXBC, DXBC::FOURCC_PSV0, psPsv0Size);
      StreamReader psPsv0(psPsv0Bytes, psPsv0Size);

      StreamWriter mergedPsv0(1024);

      uint32_t rastHeaderSize = 0;
      if(!rastPsv0.Read<uint32_t>(rastHeaderSize))
        return;

      uint32_t psHeaderSize = 0;
      if(!psPsv0.Read<uint32_t>(psHeaderSize))
        return;

      struct PSVHeader0
      {
        uint32_t unused[6];
      };

      struct PSVHeader1 : public PSVHeader0
      {
        // other data
        uint32_t unused1;

        // signature element counts
        uint8_t inputEls;
        uint8_t outputEls;
        uint8_t patchConstEls;

        // signature vector counts
        uint8_t inputVecs;
        uint8_t outputVecs[4];
      };

      struct PSVHeader2 : public PSVHeader1
      {
        uint32_t NumThreadsX;
        uint32_t NumThreadsY;
        uint32_t NumThreadsZ;
      };

      bytebuf copyBuf;
      PSVHeader2 rastHeader = {}, psHeader = {};

      if(rastHeaderSize < sizeof(PSVHeader1))
      {
        // only copy the header0 part out of the ps one since we won't have signature data to copy,
        // hope this is OK

        // read the whole ps header
        psPsv0.Read(&psHeader, psHeaderSize);

        // write only the old sized header
        mergedPsv0.Write(rastHeaderSize);
        mergedPsv0.Write(&psHeader, rastHeaderSize);
      }
      else
      {
        rastPsv0.Read(&rastHeader, rastHeaderSize);
        psPsv0.Read(&psHeader, psHeaderSize);

        // copy the previous output signature into the ps input
        psHeader.inputEls = rastHeader.outputEls;
        psHeader.inputVecs = rastHeader.outputVecs[0];

        // the ps header should have no other elements for us to worry about
        RDCASSERT(psHeader.outputEls == 0);
        RDCASSERT(psHeader.patchConstEls == 0);
        RDCASSERT(psHeader.outputVecs[0] == 0);
        RDCASSERT(psHeader.outputVecs[1] == 0);
        RDCASSERT(psHeader.outputVecs[2] == 0);
        RDCASSERT(psHeader.outputVecs[3] == 0);

        // we should have a table offset for each output entry
        RDCASSERT(rastHeader.outputEls == stringTableOffsets.size());
        RDCASSERT(rastHeader.outputEls == semanticIndexTableOffsets.size());

        mergedPsv0.Write(psHeaderSize);
        mergedPsv0.Write(&psHeader, psHeaderSize);
      }

      // skip resource counts in raster side shader
      uint32_t rastResCount = 0;
      if(!rastPsv0.Read<uint32_t>(rastResCount))
        return;

      if(rastResCount > 0)
      {
        uint32_t resSize = 0;
        if(!rastPsv0.Read<uint32_t>(resSize))
          return;
        rastPsv0.SkipBytes(rastResCount * resSize);
      }

      uint32_t psResCount = 0;
      if(!psPsv0.Read<uint32_t>(psResCount))
        return;
      mergedPsv0.Write(psResCount);

      // copy any resources in the pixel psv0
      if(psResCount > 0)
      {
        uint32_t resSize = 0;
        if(!psPsv0.Read<uint32_t>(resSize))
          return;
        mergedPsv0.Write(resSize);
        copyBuf.resize(psResCount * resSize);
        psPsv0.Read(copyBuf.data(), copyBuf.size());
        mergedPsv0.Write(copyBuf.data(), copyBuf.size());
      }

      // if we have a new header with signature elements (what we expect)
      if(rastHeaderSize >= sizeof(PSVHeader1))
      {
        // we're effectively done with the rest of the ps chunk here, we're just going to copy the
        // old
        // chunk except skipping the input signature. There might be data we don't need in the
        // string/indices tables but that's fine.

        // align string table to multiple of 4 size
        stringTable.resize(AlignUp4(stringTable.size()));

        // skip the old string table and semantic index table
        uint32_t stringTableSize = 0;
        if(!rastPsv0.Read<uint32_t>(stringTableSize))
          return;
        rastPsv0.SkipBytes(stringTableSize);

        uint32_t indexTableSize = 0;
        if(!rastPsv0.Read<uint32_t>(indexTableSize))
          return;
        rastPsv0.SkipBytes(indexTableSize * sizeof(uint32_t));

        mergedPsv0.Write((uint32_t)stringTable.size());
        mergedPsv0.Write(stringTable.data(), stringTable.size());

        mergedPsv0.Write((uint32_t)semanticIndexTable.size());
        mergedPsv0.Write(semanticIndexTable.data(), semanticIndexTable.byteSize());

        uint32_t sigElSize = 0;
        if(!rastPsv0.Read<uint32_t>(sigElSize))
          return;
        mergedPsv0.Write(sigElSize);

        // skip any inputs from the previous stage, we don't want to copy that
        rastPsv0.SkipBytes(rastHeader.inputEls * sigElSize);

        struct PSVSigElement
        {
          uint32_t stringTableOffset;
          uint32_t semanticTableOffset;
        };

        // copy the output elements, this will become the input elements. We need to modify the
        // table
        // offsets to match the one we generated
        for(uint8_t el = 0; el < rastHeader.outputEls; el++)
        {
          copyBuf.resize(sigElSize);
          rastPsv0.Read(copyBuf.data(), copyBuf.size());
          PSVSigElement *sigEl = (PSVSigElement *)copyBuf.data();
          sigEl->stringTableOffset = stringTableOffsets[el];
          sigEl->semanticTableOffset = semanticIndexTableOffsets[el];

          mergedPsv0.Write(copyBuf.data(), copyBuf.size());
        }
      }

      DXBC::DXBCContainer::ReplaceChunk(patchedDXBC, DXBC::FOURCC_PSV0, mergedPsv0.GetData(),
                                        (size_t)mergedPsv0.GetOffset());
    }
  }
  else    // dxbc bytecode not dxil
  {
    using namespace DXBCBytecode;
    using namespace DXBCBytecode::Edit;

    ProgramEditor editor(&quadOverdrawDXBC, patchedDXBC);

    // find out which register the previous shader used to write position, we don't need to declare
    // any of the others just match the register
    uint32_t posReg = 0;
    for(const SigParameter &sig : rastFeedingDXBC.GetReflection()->OutputSig)
    {
      if(sig.systemValue == ShaderBuiltin::Position)
      {
        posReg = sig.regIndex;
        break;
      }
    }

    for(size_t i = 0; i < editor.GetNumDeclarations(); i++)
    {
      Declaration &decl = editor.GetDeclaration(i);

      // there's only one SIV input
      if(decl.declaration == OpcodeType::OPCODE_DCL_INPUT_PS_SIV)
      {
        RDCASSERT(decl.operand.type == OperandType::TYPE_INPUT);
        if(decl.operand.indices.size() >= 1)
        {
          decl.operand.indices[0].index = posReg;
        }
        else
        {
          RDCERR("Unexpected number of indices for declared PS input");
        }

        break;
      }
    }

    // now patch any instructions that reference the input
    for(size_t i = 0; i < editor.GetNumInstructions(); i++)
    {
      Operation &op = editor.GetInstruction(i);

      for(Operand &operand : op.operands)
      {
        if(operand.type == OperandType::TYPE_INPUT && operand.indices.size() == 1 &&
           operand.indices[0].index == 0)
          operand.indices[0].index = posReg;
      }
    }
  }

  // copy the raster shader's OSGX to the pixel's ISGX
  {
    struct SigElement
    {
      uint32_t nameOffset;
      uint32_t semanticIdx;
      uint32_t systemType;
      uint32_t componentType;
      uint32_t registerNum;
      uint8_t mask;
      uint8_t rwMask;
      uint16_t unused;
    };

    struct SigElement7
    {
      uint32_t stream;
      SigElement el;
    };

    struct SigElement1
    {
      SigElement7 el7;
      uint32_t precision;
    };

    bytebuf osg;

    StreamWriter isg(1024);

    size_t inSigElSize = 0;
    size_t outSigElSize = 0;

    size_t rastOSGSize = 0;
    const byte *rastOSGBytes =
        DXBC::DXBCContainer::FindChunk(rastFeedingBytes, DXBC::FOURCC_OSG1, rastOSGSize);

    if(rastOSGBytes)
    {
      osg.assign(rastOSGBytes, rastOSGSize);
      inSigElSize = sizeof(SigElement1);
    }
    else
    {
      rastOSGBytes = DXBC::DXBCContainer::FindChunk(rastFeedingBytes, DXBC::FOURCC_OSG5, rastOSGSize);

      if(rastOSGBytes)
      {
        osg.assign(rastOSGBytes, rastOSGSize);
        inSigElSize = sizeof(SigElement7);
      }
      else
      {
        rastOSGBytes =
            DXBC::DXBCContainer::FindChunk(rastFeedingBytes, DXBC::FOURCC_OSGN, rastOSGSize);

        if(!rastOSGBytes)
        {
          RDCERR("Couldn't find any output signature in rasterizing-feeding shader");
          return;
        }

        osg.assign(rastOSGBytes, rastOSGSize);
        inSigElSize = sizeof(SigElement);
      }
    }

    size_t sz;

    if(DXBC::DXBCContainer::FindChunk(patchedDXBC, DXBC::FOURCC_ISG1, sz))
    {
      outSigElSize = sizeof(SigElement1);
    }
    else if(DXBC::DXBCContainer::FindChunk(patchedDXBC, DXBC::FOURCC_ISGN, sz))
    {
      outSigElSize = sizeof(SigElement);
    }
    else
    {
      RDCERR("Couldn't find any input signature in pixel shader");
      return;
    }

    uint32_t *u = (uint32_t *)osg.data();

    uint32_t numSigEls = *u;

    isg.Write(u[0]);
    isg.Write(u[1]);

    for(uint32_t el = 0; el < numSigEls; el++)
    {
      SigElement1 s = {};

      size_t offset = sizeof(uint32_t) * 2 + inSigElSize * el;

      // read the input element into wherever it sits. We can leave any other elements
      // (stream/precision) as 0 and that's fine
      if(inSigElSize == sizeof(SigElement1))
        memcpy(&s, osg.data() + offset, inSigElSize);
      else if(inSigElSize == sizeof(SigElement7))
        memcpy(&s.el7, osg.data() + offset, inSigElSize);
      else if(inSigElSize == sizeof(SigElement))
        memcpy(&s.el7.el, osg.data() + offset, inSigElSize);

      // set the rw mask
      s.el7.el.rwMask = 0;

      // dxbc seems to set the rwMask to .xy for position being read
      if(!dxil && s.el7.el.systemType == 1)
        s.el7.el.rwMask = 0x3;

      // write the output element
      if(inSigElSize == sizeof(SigElement1))
        isg.Write(s);
      else if(inSigElSize == sizeof(SigElement))
        isg.Write(s.el7.el);
    }

    size_t stringsOffset = sizeof(uint32_t) * 2 + inSigElSize * numSigEls;
    isg.Write(osg.data() + stringsOffset, osg.size() - stringsOffset);

    DXBC::DXBCContainer::ReplaceChunk(
        patchedDXBC, outSigElSize == sizeof(SigElement1) ? DXBC::FOURCC_ISG1 : DXBC::FOURCC_ISGN,
        isg.GetData(), (size_t)isg.GetOffset());
  }

  // store the patched DXBC into the cache result
  patchedDXBC.swap(patchedPs);

  if(!D3D12_Debug_OverlayDumpDirPath().empty())
    FileIO::WriteAll(D3D12_Debug_OverlayDumpDirPath() + "/after_quadps.dxbc", patchedPs);

  DXBC::DXBCContainer(patchedPs, rdcstr(), GraphicsAPI::D3D12, ~0U, ~0U).GetDisassembly();

  pipeDesc.PS.pShaderBytecode = patchedPs.data();
  pipeDesc.PS.BytecodeLength = patchedPs.size();
}

RenderOutputSubresource D3D12Replay::GetRenderOutputSubresource(ResourceId id)
{
  const D3D12RenderState &rs = m_pDevice->GetQueue()->GetCommandData()->m_RenderState;

  D3D12Pipe::View view;

  for(size_t i = 0; i < rs.rts.size(); i++)
  {
    if(id == rs.rts[i].GetResResourceId())
    {
      FillResourceView(view, &rs.rts[i]);
      return RenderOutputSubresource(view.firstMip, view.firstSlice, view.numSlices);
    }
  }

  if(id == rs.dsv.GetResResourceId() && rs.dsv.GetResResourceId() != ResourceId())
  {
    FillResourceView(view, &rs.dsv);
    return RenderOutputSubresource(view.firstMip, view.firstSlice, view.numSlices);
  }

  return RenderOutputSubresource(~0U, ~0U, 0);
}

ResourceId D3D12Replay::RenderOverlay(ResourceId texid, FloatVector clearCol, DebugOverlay overlay,
                                      uint32_t eventId, const rdcarray<uint32_t> &passEvents)
{
  ID3D12Resource *resource = NULL;

  {
    auto it = m_pDevice->GetResourceList().find(texid);
    if(it != m_pDevice->GetResourceList().end())
      resource = it->second;
  }

  if(resource == NULL)
    return ResourceId();

  RenderOutputSubresource sub = GetRenderOutputSubresource(texid);

  if(sub.slice == ~0U)
  {
    RDCERR("Rendering overlay for %s couldn't find output to get subresource.", ToStr(texid).c_str());
    sub = RenderOutputSubresource(0, 0, 1);
  }

  D3D12MarkerRegion renderoverlay(m_pDevice->GetQueue(),
                                  StringFormat::Fmt("RenderOverlay %d", overlay));

  D3D12_RESOURCE_DESC resourceDesc = resource->GetDesc();

  D3D12_RESOURCE_DESC overlayTexDesc;
  overlayTexDesc.Alignment = 0;
  overlayTexDesc.DepthOrArraySize = resourceDesc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE3D
                                        ? resourceDesc.DepthOrArraySize
                                        : 1;
  overlayTexDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
  overlayTexDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
  overlayTexDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
  overlayTexDesc.Height = resourceDesc.Height;
  overlayTexDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
  overlayTexDesc.MipLevels = resourceDesc.MipLevels;
  overlayTexDesc.SampleDesc = resourceDesc.SampleDesc;
  overlayTexDesc.Width = resourceDesc.Width;

  D3D12_HEAP_PROPERTIES heapProps;
  heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
  heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
  heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
  heapProps.CreationNodeMask = 1;
  heapProps.VisibleNodeMask = 1;

  D3D12_RESOURCE_DESC currentOverlayDesc;
  RDCEraseEl(currentOverlayDesc);
  if(m_Overlay.Texture)
    currentOverlayDesc = m_Overlay.Texture->GetDesc();

  WrappedID3D12Resource *wrappedCustomRenderTex = (WrappedID3D12Resource *)m_Overlay.Texture;

  // need to recreate backing custom render tex
  if(overlayTexDesc.Width != currentOverlayDesc.Width ||
     overlayTexDesc.Height != currentOverlayDesc.Height ||
     overlayTexDesc.Format != currentOverlayDesc.Format ||
     overlayTexDesc.DepthOrArraySize != currentOverlayDesc.DepthOrArraySize ||
     overlayTexDesc.MipLevels != currentOverlayDesc.MipLevels ||
     overlayTexDesc.SampleDesc.Count != currentOverlayDesc.SampleDesc.Count ||
     overlayTexDesc.SampleDesc.Quality != currentOverlayDesc.SampleDesc.Quality)
  {
    SAFE_RELEASE(m_Overlay.Texture);
    m_Overlay.resourceId = ResourceId();

    ID3D12Resource *customRenderTex = NULL;
    HRESULT hr = m_pDevice->CreateCommittedResource(
        &heapProps, D3D12_HEAP_FLAG_NONE, &overlayTexDesc, D3D12_RESOURCE_STATE_RENDER_TARGET, NULL,
        __uuidof(ID3D12Resource), (void **)&customRenderTex);
    if(FAILED(hr))
    {
      RDCERR("Failed to create custom render tex HRESULT: %s", ToStr(hr).c_str());
      return ResourceId();
    }
    wrappedCustomRenderTex = (WrappedID3D12Resource *)customRenderTex;

    customRenderTex->SetName(L"customRenderTex");

    m_Overlay.Texture = wrappedCustomRenderTex;
    m_Overlay.resourceId = wrappedCustomRenderTex->GetResourceID();
  }

  D3D12RenderState &rs = m_pDevice->GetQueue()->GetCommandData()->m_RenderState;

  ID3D12Resource *renderDepth = NULL;

  D3D12Descriptor dsView = rs.dsv;

  D3D12_RESOURCE_DESC depthTexDesc = {};
  D3D12_DEPTH_STENCIL_VIEW_DESC dsViewDesc = {};
  if(dsView.GetResResourceId() != ResourceId())
  {
    ID3D12Resource *realDepth =
        m_pDevice->GetResourceManager()->GetCurrentAs<ID3D12Resource>(dsView.GetResResourceId());

    dsViewDesc = dsView.GetDSV();

    depthTexDesc = realDepth->GetDesc();
    depthTexDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
    depthTexDesc.Alignment = 0;

    HRESULT hr = S_OK;

    hr = m_pDevice->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &depthTexDesc,
                                            D3D12_RESOURCE_STATE_COPY_DEST, NULL,
                                            __uuidof(ID3D12Resource), (void **)&renderDepth);
    if(FAILED(hr))
    {
      RDCERR("Failed to create renderDepth HRESULT: %s", ToStr(hr).c_str());
      return m_Overlay.resourceId;
    }

    renderDepth->SetName(L"Overlay renderDepth");

    ID3D12GraphicsCommandListX *list = m_pDevice->GetNewList();
    if(!list)
      return ResourceId();

    BarrierSet barriers;
    barriers.Configure(realDepth, m_pDevice->GetSubresourceStates(GetResID(realDepth)),
                       BarrierSet::CopySourceAccess);

    barriers.Apply(list);

    list->CopyResource(renderDepth, realDepth);

    barriers.Unapply(list);

    D3D12_RESOURCE_BARRIER b = {};

    b.Transition.pResource = renderDepth;
    b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    b.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    b.Transition.StateAfter = D3D12_RESOURCE_STATE_DEPTH_WRITE;

    // prepare tex resource for writing
    list->ResourceBarrier(1, &b);

    list->Close();
  }

  D3D12_CPU_DESCRIPTOR_HANDLE rtv = GetDebugManager()->GetCPUHandle(OVERLAY_RTV);
  D3D12_RENDER_TARGET_VIEW_DESC rtDesc = {};
  rtDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;

  ID3D12GraphicsCommandListX *list = m_pDevice->GetNewList();
  if(!list)
    return ResourceId();

  // clear all mips and all slices first
  for(UINT mip = 0; mip < overlayTexDesc.MipLevels; mip++)
  {
    SetRTVDesc(rtDesc, overlayTexDesc,
               RenderOutputSubresource(mip, 0, overlayTexDesc.DepthOrArraySize));

    m_pDevice->CreateRenderTargetView(wrappedCustomRenderTex, &rtDesc, rtv);
    FLOAT black[] = {0.0f, 0.0f, 0.0f, 0.0f};
    list->ClearRenderTargetView(rtv, black, 0, NULL);
  }

  SetRTVDesc(rtDesc, overlayTexDesc, sub);
  m_pDevice->CreateRenderTargetView(wrappedCustomRenderTex, &rtDesc, rtv);

  D3D12_CPU_DESCRIPTOR_HANDLE dsv = {};

  if(renderDepth)
  {
    dsv = GetDebugManager()->GetCPUHandle(OVERLAY_DSV);
    m_pDevice->CreateDepthStencilView(
        renderDepth, dsViewDesc.Format == DXGI_FORMAT_UNKNOWN ? NULL : &dsViewDesc, dsv);
  }

  WrappedID3D12PipelineState *pipe = NULL;

  if(rs.pipe != ResourceId())
    pipe = m_pDevice->GetResourceManager()->GetCurrentAs<WrappedID3D12PipelineState>(rs.pipe);

  if(overlay == DebugOverlay::NaN || overlay == DebugOverlay::Clipping)
  {
    // just need the basic texture
  }
  else if(overlay == DebugOverlay::Drawcall)
  {
    if(pipe && pipe->IsGraphics())
    {
      D3D12_EXPANDED_PIPELINE_STATE_STREAM_DESC psoDesc;
      pipe->Fill(psoDesc);

      bool dxil =
          psoDesc.MS.BytecodeLength > 0 ||
          DXBC::DXBCContainer::CheckForDXIL(psoDesc.VS.pShaderBytecode, psoDesc.VS.BytecodeLength);

      ID3DBlob *ps =
          m_pDevice->GetShaderCache()->MakeFixedColShader(D3D12ShaderCache::HIGHLIGHT, dxil);

      psoDesc.DepthStencilState.DepthEnable = FALSE;
      psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
      psoDesc.DepthStencilState.StencilEnable = FALSE;
      psoDesc.DepthStencilState.DepthBoundsTestEnable = FALSE;

      psoDesc.BlendState.AlphaToCoverageEnable = FALSE;
      psoDesc.BlendState.IndependentBlendEnable = FALSE;
      psoDesc.BlendState.RenderTarget[0].BlendEnable = FALSE;
      psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = 0xf;
      psoDesc.BlendState.RenderTarget[0].LogicOpEnable = FALSE;
      RDCEraseEl(psoDesc.RTVFormats.RTFormats);
      psoDesc.RTVFormats.RTFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
      psoDesc.RTVFormats.NumRenderTargets = 1;
      psoDesc.SampleMask = ~0U;
      psoDesc.SampleDesc.Count = RDCMAX(1U, psoDesc.SampleDesc.Count);
      psoDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;

      psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
      psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
      psoDesc.RasterizerState.FrontCounterClockwise = FALSE;
      psoDesc.RasterizerState.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
      psoDesc.RasterizerState.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
      psoDesc.RasterizerState.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
      psoDesc.RasterizerState.DepthClipEnable = FALSE;
      psoDesc.RasterizerState.LineRasterizationMode = D3D12_LINE_RASTERIZATION_MODE_ALIASED;

      float clearColour[] = {0.0f, 0.0f, 0.0f, 0.5f};
      list->ClearRenderTargetView(rtv, clearColour, 0, NULL);

      list->Close();
      list = NULL;

      if(!ps)
      {
        m_pDevice->AddDebugMessage(MessageCategory::Shaders, MessageSeverity::High,
                                   MessageSource::UnsupportedConfiguration,
                                   "No DXIL shader available for overlay");
        return m_Overlay.resourceId;
      }

      psoDesc.PS.pShaderBytecode = ps->GetBufferPointer();
      psoDesc.PS.BytecodeLength = ps->GetBufferSize();

      ID3D12PipelineState *pso = NULL;
      HRESULT hr = m_pDevice->CreatePipeState(psoDesc, &pso);
      if(FAILED(hr))
      {
        RDCERR("Failed to create overlay pso HRESULT: %s", ToStr(hr).c_str());
        SAFE_RELEASE(ps);
        return m_Overlay.resourceId;
      }

      D3D12RenderState prev = rs;

      rs.pipe = GetResID(pso);
      rs.rts.resize(1);
      rs.rts[0] = *GetWrapped(rtv);
      RDCEraseEl(rs.dsv);

      for(D3D12_RECT &r : rs.scissors)
        r = {0, 0, 32768, 32768};

      m_pDevice->ReplayLog(0, eventId, eReplay_OnlyDraw);

      rs = prev;

      m_pDevice->ExecuteLists();
      m_pDevice->FlushLists();

      SAFE_RELEASE(pso);
      SAFE_RELEASE(ps);
    }
  }
  else if(overlay == DebugOverlay::BackfaceCull)
  {
    if(pipe && pipe->IsGraphics())
    {
      D3D12_EXPANDED_PIPELINE_STATE_STREAM_DESC psoDesc;
      pipe->Fill(psoDesc);

      D3D12_CULL_MODE origCull = psoDesc.RasterizerState.CullMode;
      BOOL origFrontCCW = psoDesc.RasterizerState.FrontCounterClockwise;

      bool dxil =
          psoDesc.MS.BytecodeLength > 0 ||
          DXBC::DXBCContainer::CheckForDXIL(psoDesc.VS.pShaderBytecode, psoDesc.VS.BytecodeLength);

      ID3DBlob *red = m_pDevice->GetShaderCache()->MakeFixedColShader(D3D12ShaderCache::RED, dxil);
      ID3DBlob *green =
          m_pDevice->GetShaderCache()->MakeFixedColShader(D3D12ShaderCache::GREEN, dxil);

      psoDesc.DepthStencilState.DepthEnable = FALSE;
      psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
      psoDesc.DepthStencilState.StencilEnable = FALSE;

      psoDesc.BlendState.AlphaToCoverageEnable = FALSE;
      psoDesc.BlendState.IndependentBlendEnable = FALSE;
      psoDesc.BlendState.RenderTarget[0].BlendEnable = FALSE;
      psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = 0xf;
      psoDesc.BlendState.RenderTarget[0].LogicOpEnable = FALSE;
      RDCEraseEl(psoDesc.RTVFormats.RTFormats);
      psoDesc.RTVFormats.RTFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
      psoDesc.RTVFormats.NumRenderTargets = 1;
      psoDesc.SampleMask = ~0U;
      psoDesc.SampleDesc.Count = RDCMAX(1U, psoDesc.SampleDesc.Count);
      psoDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;

      psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
      psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
      psoDesc.RasterizerState.FrontCounterClockwise = FALSE;
      psoDesc.RasterizerState.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
      psoDesc.RasterizerState.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
      psoDesc.RasterizerState.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
      psoDesc.RasterizerState.DepthClipEnable = FALSE;
      psoDesc.RasterizerState.LineRasterizationMode = D3D12_LINE_RASTERIZATION_MODE_ALIASED;

      float clearColour[] = {0.0f, 0.0f, 0.0f, 0.0f};
      list->ClearRenderTargetView(rtv, clearColour, 0, NULL);

      list->Close();
      list = NULL;

      if(!red || !green)
      {
        SAFE_RELEASE(red);
        SAFE_RELEASE(green);
        m_pDevice->AddDebugMessage(MessageCategory::Shaders, MessageSeverity::High,
                                   MessageSource::UnsupportedConfiguration,
                                   "No DXIL shader available for overlay");
        return m_Overlay.resourceId;
      }

      psoDesc.PS.pShaderBytecode = red->GetBufferPointer();
      psoDesc.PS.BytecodeLength = red->GetBufferSize();

      ID3D12PipelineState *redPSO = NULL;
      HRESULT hr = m_pDevice->CreatePipeState(psoDesc, &redPSO);
      if(FAILED(hr))
      {
        RDCERR("Failed to create overlay pso HRESULT: %s", ToStr(hr).c_str());
        SAFE_RELEASE(red);
        SAFE_RELEASE(green);
        return m_Overlay.resourceId;
      }

      psoDesc.RasterizerState.CullMode = origCull;
      psoDesc.RasterizerState.FrontCounterClockwise = origFrontCCW;
      psoDesc.PS.pShaderBytecode = green->GetBufferPointer();
      psoDesc.PS.BytecodeLength = green->GetBufferSize();

      ID3D12PipelineState *greenPSO = NULL;
      hr = m_pDevice->CreatePipeState(psoDesc, &greenPSO);
      if(FAILED(hr))
      {
        RDCERR("Failed to create overlay pso HRESULT: %s", ToStr(hr).c_str());
        SAFE_RELEASE(red);
        SAFE_RELEASE(redPSO);
        SAFE_RELEASE(green);
        return m_Overlay.resourceId;
      }

      D3D12RenderState prev = rs;

      rs.pipe = GetResID(redPSO);
      rs.rts.resize(1);
      rs.rts[0] = *GetWrapped(rtv);
      RDCEraseEl(rs.dsv);

      m_pDevice->ReplayLog(0, eventId, eReplay_OnlyDraw);

      rs.pipe = GetResID(greenPSO);

      m_pDevice->ReplayLog(0, eventId, eReplay_OnlyDraw);

      rs = prev;

      m_pDevice->ExecuteLists();
      m_pDevice->FlushLists();

      SAFE_RELEASE(red);
      SAFE_RELEASE(green);
      SAFE_RELEASE(redPSO);
      SAFE_RELEASE(greenPSO);
    }
  }
  else if(overlay == DebugOverlay::Wireframe)
  {
    if(pipe && pipe->IsGraphics())
    {
      D3D12_EXPANDED_PIPELINE_STATE_STREAM_DESC psoDesc;
      pipe->Fill(psoDesc);

      bool dxil =
          psoDesc.MS.BytecodeLength > 0 ||
          DXBC::DXBCContainer::CheckForDXIL(psoDesc.VS.pShaderBytecode, psoDesc.VS.BytecodeLength);

      ID3DBlob *ps =
          m_pDevice->GetShaderCache()->MakeFixedColShader(D3D12ShaderCache::WIREFRAME, dxil);

      psoDesc.DepthStencilState.DepthEnable = FALSE;
      psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
      psoDesc.DepthStencilState.StencilEnable = FALSE;

      psoDesc.BlendState.AlphaToCoverageEnable = FALSE;
      psoDesc.BlendState.IndependentBlendEnable = FALSE;
      psoDesc.BlendState.RenderTarget[0].BlendEnable = FALSE;
      psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = 0xf;
      psoDesc.BlendState.RenderTarget[0].LogicOpEnable = FALSE;
      RDCEraseEl(psoDesc.RTVFormats.RTFormats);
      psoDesc.RTVFormats.RTFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
      psoDesc.RTVFormats.NumRenderTargets = 1;
      psoDesc.SampleMask = ~0U;
      psoDesc.SampleDesc.Count = RDCMAX(1U, psoDesc.SampleDesc.Count);
      psoDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;

      psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
      psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
      psoDesc.RasterizerState.FrontCounterClockwise = FALSE;
      psoDesc.RasterizerState.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
      psoDesc.RasterizerState.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
      psoDesc.RasterizerState.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
      psoDesc.RasterizerState.DepthClipEnable = FALSE;
      psoDesc.RasterizerState.LineRasterizationMode = D3D12_LINE_RASTERIZATION_MODE_ALIASED;

      float wireClearCol[4] = {200.0f / 255.0f, 255.0f / 255.0f, 0.0f / 255.0f, 0.0f};
      list->ClearRenderTargetView(rtv, wireClearCol, 0, NULL);

      list->Close();
      list = NULL;

      if(!ps)
      {
        m_pDevice->AddDebugMessage(MessageCategory::Shaders, MessageSeverity::High,
                                   MessageSource::UnsupportedConfiguration,
                                   "No DXIL shader available for overlay");
        return m_Overlay.resourceId;
      }

      psoDesc.PS.pShaderBytecode = ps->GetBufferPointer();
      psoDesc.PS.BytecodeLength = ps->GetBufferSize();

      ID3D12PipelineState *pso = NULL;
      HRESULT hr = m_pDevice->CreatePipeState(psoDesc, &pso);
      if(FAILED(hr))
      {
        RDCERR("Failed to create overlay pso HRESULT: %s", ToStr(hr).c_str());
        SAFE_RELEASE(ps);
        return m_Overlay.resourceId;
      }

      D3D12RenderState prev = rs;

      rs.pipe = GetResID(pso);
      rs.rts.resize(1);
      rs.rts[0] = *GetWrapped(rtv);
      RDCEraseEl(rs.dsv);

      m_pDevice->ReplayLog(0, eventId, eReplay_OnlyDraw);

      rs = prev;

      m_pDevice->ExecuteLists();
      m_pDevice->FlushLists();

      SAFE_RELEASE(pso);
      SAFE_RELEASE(ps);
    }
  }
  else if(overlay == DebugOverlay::ClearBeforePass || overlay == DebugOverlay::ClearBeforeDraw)
  {
    rdcarray<uint32_t> events = passEvents;

    if(overlay == DebugOverlay::ClearBeforeDraw)
      events.clear();

    events.push_back(eventId);

    if(!events.empty())
    {
      list->Close();
      list = NULL;

      rdcarray<D3D12Descriptor> rts = rs.rts;

      if(overlay == DebugOverlay::ClearBeforePass)
        m_pDevice->ReplayLog(0, events[0], eReplay_WithoutDraw);

      list = m_pDevice->GetNewList();
      if(!list)
        return ResourceId();

      for(size_t i = 0; i < rts.size(); i++)
      {
        const D3D12Descriptor &desc = rts[i];

        if(desc.GetResResourceId() != ResourceId())
          Unwrap(list)->ClearRenderTargetView(Unwrap(GetDebugManager()->GetTempDescriptor(desc)),
                                              &clearCol.x, 0, NULL);
      }

      // Try to clear depth as well, to help debug shadow rendering
      if(rs.dsv.GetResResourceId() != ResourceId() && IsDepthFormat(resourceDesc.Format))
      {
        WrappedID3D12PipelineState *origPSO =
            m_pDevice->GetResourceManager()->GetCurrentAs<WrappedID3D12PipelineState>(rs.pipe);
        if(origPSO && origPSO->IsGraphics())
        {
          D3D12_COMPARISON_FUNC depthFunc = origPSO->graphics->DepthStencilState.DepthFunc;

          // If the depth func is equal or not equal, don't clear at all since the output would be
          // altered in an way that would cause replay to produce mostly incorrect results.
          // Similarly, skip if the depth func is always, as we'd have a 50% chance of guessing the
          // wrong clear value.
          if(depthFunc != D3D12_COMPARISON_FUNC_EQUAL &&
             depthFunc != D3D12_COMPARISON_FUNC_NOT_EQUAL &&
             depthFunc != D3D12_COMPARISON_FUNC_ALWAYS)
          {
            // If the depth func is less or less equal, clear to 1 instead of 0
            bool depthFuncLess = depthFunc == D3D12_COMPARISON_FUNC_LESS ||
                                 depthFunc == D3D12_COMPARISON_FUNC_LESS_EQUAL;
            float depthClear = depthFuncLess ? 1.0f : 0.0f;

            Unwrap(list)->ClearDepthStencilView(Unwrap(GetDebugManager()->GetTempDescriptor(rs.dsv)),
                                                D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL,
                                                depthClear, 0, 0, NULL);
          }
        }
      }

      list->Close();
      list = NULL;

      for(size_t i = 0; i < events.size(); i++)
      {
        m_pDevice->ReplayLog(events[i], events[i], eReplay_OnlyDraw);

        if(overlay == DebugOverlay::ClearBeforePass && i + 1 < events.size())
          m_pDevice->ReplayLog(events[i] + 1, events[i + 1], eReplay_WithoutDraw);
      }
    }
  }
  else if(overlay == DebugOverlay::ViewportScissor)
  {
    if(pipe && pipe->IsGraphics() && !rs.views.empty())
    {
      D3D12_EXPANDED_PIPELINE_STATE_STREAM_DESC psoDesc;
      pipe->Fill(psoDesc);

      bool dxil =
          psoDesc.MS.BytecodeLength > 0 ||
          DXBC::DXBCContainer::CheckForDXIL(psoDesc.VS.pShaderBytecode, psoDesc.VS.BytecodeLength);

      ID3DBlob *red = m_pDevice->GetShaderCache()->MakeFixedColShader(D3D12ShaderCache::RED, dxil);
      ID3DBlob *green =
          m_pDevice->GetShaderCache()->MakeFixedColShader(D3D12ShaderCache::GREEN, dxil);

      psoDesc.DepthStencilState.DepthEnable = FALSE;
      psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
      psoDesc.DepthStencilState.StencilEnable = FALSE;

      psoDesc.BlendState.AlphaToCoverageEnable = FALSE;
      psoDesc.BlendState.IndependentBlendEnable = FALSE;
      psoDesc.BlendState.RenderTarget[0].BlendEnable = FALSE;
      psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = 0xf;
      psoDesc.BlendState.RenderTarget[0].LogicOpEnable = FALSE;
      RDCEraseEl(psoDesc.RTVFormats.RTFormats);
      psoDesc.RTVFormats.RTFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
      psoDesc.RTVFormats.NumRenderTargets = 1;
      psoDesc.SampleMask = ~0U;
      psoDesc.SampleDesc.Count = RDCMAX(1U, psoDesc.SampleDesc.Count);
      psoDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;

      psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
      psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
      psoDesc.RasterizerState.FrontCounterClockwise = FALSE;
      psoDesc.RasterizerState.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
      psoDesc.RasterizerState.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
      psoDesc.RasterizerState.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
      psoDesc.RasterizerState.DepthClipEnable = FALSE;
      psoDesc.RasterizerState.LineRasterizationMode = D3D12_LINE_RASTERIZATION_MODE_ALIASED;

      psoDesc.PS.pShaderBytecode = red->GetBufferPointer();
      psoDesc.PS.BytecodeLength = red->GetBufferSize();

      ID3D12PipelineState *redPSO = NULL;
      HRESULT hr = m_pDevice->CreatePipeState(psoDesc, &redPSO);
      if(FAILED(hr))
      {
        RDCERR("Failed to create overlay pso HRESULT: %s", ToStr(hr).c_str());
        SAFE_RELEASE(red);
        SAFE_RELEASE(redPSO);
        SAFE_RELEASE(green);
        return m_Overlay.resourceId;
      }

      psoDesc.PS.pShaderBytecode = green->GetBufferPointer();
      psoDesc.PS.BytecodeLength = green->GetBufferSize();

      ID3D12PipelineState *greenPSO = NULL;
      hr = m_pDevice->CreatePipeState(psoDesc, &greenPSO);
      if(FAILED(hr))
      {
        RDCERR("Failed to create overlay pso HRESULT: %s", ToStr(hr).c_str());
        SAFE_RELEASE(red);
        SAFE_RELEASE(redPSO);
        SAFE_RELEASE(green);
        SAFE_RELEASE(greenPSO);
        return m_Overlay.resourceId;
      }

      list->Close();
      list = NULL;

      D3D12_RECT scissor = {0, 0, 16384, 16384};

      D3D12RenderState prev = rs;

      rs.rts = {*GetWrapped(rtv)};

      for(D3D12_RECT &s : rs.scissors)
        s = scissor;

      rs.pipe = GetResID(redPSO);
      m_pDevice->ReplayLog(0, eventId, eReplay_OnlyDraw);

      rs.scissors = prev.scissors;

      rs.pipe = GetResID(greenPSO);
      m_pDevice->ReplayLog(0, eventId, eReplay_OnlyDraw);

      rs = prev;

      list = m_pDevice->GetNewList();
      if(!list)
        return ResourceId();

      rs.ApplyState(m_pDevice, list);

      list->OMSetRenderTargets(1, &rtv, TRUE, NULL);

      D3D12_VIEWPORT viewport = rs.views[0];
      list->RSSetViewports(1, &viewport);

      list->RSSetScissorRects(1, &scissor);

      list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

      list->SetPipelineState(
          m_General.CheckerboardF16Pipe[Log2Floor(overlayTexDesc.SampleDesc.Count)]);

      list->SetGraphicsRootSignature(m_General.CheckerboardRootSig);

      CheckerboardCBuffer pixelData = {0};

      pixelData.BorderWidth = 3;
      pixelData.CheckerSquareDimension = 16.0f;

      // set primary/secondary to the same to 'disable' checkerboard
      pixelData.PrimaryColor = pixelData.SecondaryColor = Vec4f(0.1f, 0.1f, 0.1f, 1.0f);
      pixelData.InnerColor = Vec4f(0.2f, 0.2f, 0.9f, 0.4f);

      // set viewport rect
      pixelData.RectPosition = Vec2f(viewport.TopLeftX, viewport.TopLeftY);
      pixelData.RectSize = Vec2f(viewport.Width, viewport.Height);

      D3D12_GPU_VIRTUAL_ADDRESS viewCB =
          GetDebugManager()->UploadConstants(&pixelData, sizeof(pixelData));

      list->SetGraphicsRootConstantBufferView(0, viewCB);

      float factor[4] = {1.0f, 1.0f, 1.0f, 1.0f};
      list->OMSetBlendFactor(factor);

      list->DrawInstanced(3, 1, 0, 0);

      if(rs.scissors.empty())
      {
        viewport = {};
      }
      else
      {
        viewport.TopLeftX = (float)rs.scissors[0].left;
        viewport.TopLeftY = (float)rs.scissors[0].top;
        viewport.Width = (float)(rs.scissors[0].right - rs.scissors[0].left);
        viewport.Height = (float)(rs.scissors[0].bottom - rs.scissors[0].top);
      }
      list->RSSetViewports(1, &viewport);

      // black/white checkered border
      pixelData.PrimaryColor = Vec4f(1.0f, 1.0f, 1.0f, 1.0f);
      pixelData.SecondaryColor = Vec4f(0.0f, 0.0f, 0.0f, 1.0f);

      // nothing at all inside
      pixelData.InnerColor = Vec4f(0.0f, 0.0f, 0.0f, 0.0f);

      // set scissor rect
      pixelData.RectPosition = Vec2f(viewport.TopLeftX, viewport.TopLeftY);
      pixelData.RectSize = Vec2f(viewport.Width, viewport.Height);

      D3D12_GPU_VIRTUAL_ADDRESS scissorCB =
          GetDebugManager()->UploadConstants(&pixelData, sizeof(pixelData));

      list->SetGraphicsRootConstantBufferView(0, scissorCB);

      list->DrawInstanced(3, 1, 0, 0);

      list->Close();
      list = NULL;

      m_pDevice->ExecuteLists();
      m_pDevice->FlushLists();

      SAFE_RELEASE(red);
      SAFE_RELEASE(redPSO);
      SAFE_RELEASE(green);
      SAFE_RELEASE(greenPSO);
    }
  }
  else if(overlay == DebugOverlay::TriangleSizeDraw || overlay == DebugOverlay::TriangleSizePass)
  {
    if(pipe && pipe->IsGraphics())
    {
      SCOPED_TIMER("Triangle size");

      rdcarray<uint32_t> events = passEvents;

      if(overlay == DebugOverlay::TriangleSizeDraw)
        events.clear();

      while(!events.empty())
      {
        const ActionDescription *action = m_pDevice->GetAction(events[0]);

        // remove any non-drawcalls, like the pass boundary.
        if(!(action->flags & (ActionFlags::MeshDispatch | ActionFlags::Drawcall)))
          events.erase(0);
        else
          break;
      }

      events.push_back(eventId);

      if(overlay == DebugOverlay::TriangleSizePass)
      {
        list->Close();
        list = NULL;

        m_pDevice->ReplayLog(0, events[0], eReplay_WithoutDraw);
      }

      pipe = m_pDevice->GetResourceManager()->GetCurrentAs<WrappedID3D12PipelineState>(rs.pipe);

      D3D12_EXPANDED_PIPELINE_STATE_STREAM_DESC pipeDesc;
      pipe->Fill(pipeDesc);
      pipeDesc.pRootSignature = GetDebugManager()->GetMeshRootSig();
      pipeDesc.SampleMask = 0xFFFFFFFF;
      pipeDesc.SampleDesc = overlayTexDesc.SampleDesc;
      pipeDesc.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;

      RDCEraseEl(pipeDesc.RTVFormats.RTFormats);
      pipeDesc.RTVFormats.NumRenderTargets = 1;
      pipeDesc.RTVFormats.RTFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
      pipeDesc.BlendState.RenderTarget[0].BlendEnable = FALSE;
      pipeDesc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
      pipeDesc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
      pipeDesc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
      pipeDesc.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_SRC_ALPHA;
      pipeDesc.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
      pipeDesc.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
      pipeDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

      D3D12_INPUT_ELEMENT_DESC ia[2] = {};
      ia[0].SemanticName = "pos";
      ia[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
      ia[1].SemanticName = "sec";
      ia[1].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
      ia[1].InputSlot = 1;
      ia[1].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA;

      pipeDesc.InputLayout.NumElements = 2;
      pipeDesc.InputLayout.pInputElementDescs = ia;

      pipeDesc.VS.BytecodeLength = m_Overlay.MeshVS->GetBufferSize();
      pipeDesc.VS.pShaderBytecode = m_Overlay.MeshVS->GetBufferPointer();
      RDCEraseEl(pipeDesc.HS);
      RDCEraseEl(pipeDesc.DS);
      RDCEraseEl(pipeDesc.AS);
      RDCEraseEl(pipeDesc.MS);
      pipeDesc.GS.BytecodeLength = m_Overlay.TriangleSizeGS->GetBufferSize();
      pipeDesc.GS.pShaderBytecode = m_Overlay.TriangleSizeGS->GetBufferPointer();
      pipeDesc.PS.BytecodeLength = m_Overlay.TriangleSizePS->GetBufferSize();
      pipeDesc.PS.pShaderBytecode = m_Overlay.TriangleSizePS->GetBufferPointer();

      pipeDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;

      if(pipeDesc.DepthStencilState.DepthFunc == D3D12_COMPARISON_FUNC_GREATER)
        pipeDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_GREATER_EQUAL;
      if(pipeDesc.DepthStencilState.DepthFunc == D3D12_COMPARISON_FUNC_LESS)
        pipeDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

      // enough for all primitive topology types
      ID3D12PipelineState *pipes[D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH + 1] = {};

      MeshVertexCBuffer vertexData = {};
      vertexData.ModelViewProj = Matrix4f::Identity();
      vertexData.SpriteSize = Vec2f();
      vertexData.homogenousInput = 1U;

      D3D12RenderState::SignatureElement vertexElem(eRootCBV, ResourceId(), 0);
      WrappedID3D12Resource::GetResIDFromAddr(
          GetDebugManager()->UploadConstants(&vertexData, sizeof(vertexData)), vertexElem.id,
          vertexElem.offset);

      for(size_t i = 0; i < events.size(); i++)
      {
        D3D12RenderState prevState = rs;

        Vec4f viewport;

        if(!rs.views.empty())
          viewport = Vec4f(rs.views[0].Width, rs.views[0].Height);

        D3D12RenderState::SignatureElement viewportElem(eRootCBV, ResourceId(), 0);
        WrappedID3D12Resource::GetResIDFromAddr(
            GetDebugManager()->UploadConstants(&viewport, sizeof(viewport)), viewportElem.id,
            viewportElem.offset);

        D3D12RenderState::SignatureElement viewportConstElem(eRootConst, ResourceId(), 0);
        viewportConstElem.SetConstants(4, &viewport, 0);

        rs.graphics.rootsig = GetResID(GetDebugManager()->GetMeshRootSig());
        rs.graphics.sigelems = {
            vertexElem,
            viewportElem,
            viewportConstElem,
        };

        rs.rts = {*(D3D12Descriptor *)rtv.ptr};

        if(list == NULL)
          list = m_pDevice->GetNewList();
        if(!list)
          return ResourceId();

        rs.ApplyState(m_pDevice, list);

        const ActionDescription *action = m_pDevice->GetAction(events[i]);

        for(uint32_t inst = 0; action && inst < RDCMAX(1U, action->numInstances); inst++)
        {
          MeshFormat fmt = GetPostVSBuffers(events[i], inst, 0, MeshDataStage::GSOut);
          if(fmt.vertexResourceId == ResourceId())
            fmt = GetPostVSBuffers(events[i], inst, 0, MeshDataStage::VSOut);

          if(fmt.vertexResourceId != ResourceId())
          {
            D3D_PRIMITIVE_TOPOLOGY topo = MakeD3DPrimitiveTopology(fmt.topology);

            // can't show triangle size for points or lines
            if(topo == D3D_PRIMITIVE_TOPOLOGY_POINTLIST ||
               topo >= D3D_PRIMITIVE_TOPOLOGY_1_CONTROL_POINT_PATCHLIST)
              continue;
            else if(topo == D3D_PRIMITIVE_TOPOLOGY_LINESTRIP ||
                    topo == D3D_PRIMITIVE_TOPOLOGY_LINELIST ||
                    topo == D3D_PRIMITIVE_TOPOLOGY_LINESTRIP_ADJ ||
                    topo == D3D_PRIMITIVE_TOPOLOGY_LINELIST_ADJ)
              continue;
            else
              pipeDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

            list->IASetPrimitiveTopology(topo);

            if(pipes[pipeDesc.PrimitiveTopologyType] == NULL)
            {
              HRESULT hr =
                  m_pDevice->CreatePipeState(pipeDesc, &pipes[pipeDesc.PrimitiveTopologyType]);
              RDCASSERTEQUAL(hr, S_OK);
            }

            ID3D12Resource *vb =
                m_pDevice->GetResourceManager()->GetCurrentAs<ID3D12Resource>(fmt.vertexResourceId);

            D3D12_VERTEX_BUFFER_VIEW vbView = {};
            vbView.BufferLocation = vb->GetGPUVirtualAddress() + fmt.vertexByteOffset;
            vbView.StrideInBytes = fmt.vertexByteStride;
            vbView.SizeInBytes = UINT(vb->GetDesc().Width - fmt.vertexByteOffset);

            // second bind is just a dummy, so we don't have to make a shader
            // that doesn't accept the secondary stream
            list->IASetVertexBuffers(0, 1, &vbView);
            list->IASetVertexBuffers(1, 1, &vbView);

            list->SetPipelineState(pipes[pipeDesc.PrimitiveTopologyType]);

            if(fmt.indexByteStride && fmt.indexResourceId != ResourceId())
            {
              ID3D12Resource *ib =
                  m_pDevice->GetResourceManager()->GetCurrentAs<ID3D12Resource>(fmt.indexResourceId);

              D3D12_INDEX_BUFFER_VIEW view;
              view.BufferLocation = ib->GetGPUVirtualAddress() + fmt.indexByteOffset;
              view.SizeInBytes = UINT(ib->GetDesc().Width - fmt.indexByteOffset);
              view.Format = fmt.indexByteStride == 2 ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;
              list->IASetIndexBuffer(&view);

              list->DrawIndexedInstanced(fmt.numIndices, 1, 0, fmt.baseVertex, 0);
            }
            else
            {
              list->DrawInstanced(fmt.numIndices, 1, 0, 0);
            }
          }
        }

        list->Close();
        list = NULL;

        rs = prevState;

        if(overlay == DebugOverlay::TriangleSizePass)
        {
          m_pDevice->ReplayLog(events[i], events[i], eReplay_OnlyDraw);

          if(i + 1 < events.size())
            m_pDevice->ReplayLog(events[i], events[i + 1], eReplay_WithoutDraw);
        }
      }

      m_pDevice->ExecuteLists();
      m_pDevice->FlushLists();

      for(size_t i = 0; i < ARRAY_COUNT(pipes); i++)
        SAFE_RELEASE(pipes[i]);
    }

    // restore back to normal
    m_pDevice->ReplayLog(0, eventId, eReplay_WithoutDraw);
  }
  else if(overlay == DebugOverlay::QuadOverdrawPass || overlay == DebugOverlay::QuadOverdrawDraw)
  {
    SCOPED_TIMER("Quad Overdraw");

    rdcarray<uint32_t> events = passEvents;

    if(overlay == DebugOverlay::QuadOverdrawDraw)
      events.clear();

    events.push_back(eventId);

    if(!events.empty())
    {
      if(overlay == DebugOverlay::QuadOverdrawPass)
      {
        list->Close();
        m_pDevice->ReplayLog(0, events[0], eReplay_WithoutDraw);
        list = m_pDevice->GetNewList();
        if(!list)
          return ResourceId();
      }

      uint32_t width = uint32_t(RDCMAX(1ULL, overlayTexDesc.Width >> (sub.mip + 1)));
      uint32_t height = RDCMAX(1U, overlayTexDesc.Height >> (sub.mip + 1));

      width = RDCMAX(1U, width);
      height = RDCMAX(1U, height);

      D3D12_RESOURCE_DESC uavTexDesc = {};
      uavTexDesc.Alignment = 0;
      uavTexDesc.DepthOrArraySize = 4;
      uavTexDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
      uavTexDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
      uavTexDesc.Format = DXGI_FORMAT_R32_UINT;
      uavTexDesc.Height = height;
      uavTexDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
      uavTexDesc.MipLevels = 1;
      uavTexDesc.SampleDesc.Count = 1;
      uavTexDesc.SampleDesc.Quality = 0;
      uavTexDesc.Width = width;

      ID3D12Resource *overdrawTex = NULL;
      HRESULT hr = m_pDevice->CreateCommittedResource(
          &heapProps, D3D12_HEAP_FLAG_NONE, &uavTexDesc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
          NULL, __uuidof(ID3D12Resource), (void **)&overdrawTex);
      if(FAILED(hr))
      {
        RDCERR("Failed to create overdrawTex HRESULT: %s", ToStr(hr).c_str());
        list->Close();
        list = NULL;
        return m_Overlay.resourceId;
      }

      m_pDevice->CreateShaderResourceView(overdrawTex, NULL,
                                          GetDebugManager()->GetCPUHandle(OVERDRAW_SRV));
      m_pDevice->CreateUnorderedAccessView(overdrawTex, NULL, NULL,
                                           GetDebugManager()->GetCPUHandle(OVERDRAW_UAV));
      m_pDevice->CreateUnorderedAccessView(overdrawTex, NULL, NULL,
                                           GetDebugManager()->GetUAVClearHandle(OVERDRAW_UAV));

      GetDebugManager()->SetDescriptorHeaps(list, true, false);

      UINT zeroes[4] = {0, 0, 0, 0};
      list->ClearUnorderedAccessViewUint(GetDebugManager()->GetGPUHandle(OVERDRAW_UAV),
                                         GetDebugManager()->GetUAVClearHandle(OVERDRAW_UAV),
                                         overdrawTex, zeroes, 0, NULL);
      list->Close();
      list = NULL;

      if(D3D12_Debug_SingleSubmitFlushing())
      {
        m_pDevice->ExecuteLists();
        m_pDevice->FlushLists();
      }

      m_pDevice->ReplayLog(0, events[0], eReplay_WithoutDraw);

      ID3D12Resource *overrideDepth = NULL;

      ResourceId res = rs.GetDSVID();

      ID3D12Resource *curDepth = m_pDevice->GetResourceManager()->GetCurrentAs<ID3D12Resource>(res);
      D3D12_RESOURCE_DESC curDepthDesc = curDepth ? curDepth->GetDesc() : D3D12_RESOURCE_DESC();
      if(curDepthDesc.SampleDesc.Count > 1)
      {
        curDepthDesc.Alignment = 0;
        curDepthDesc.DepthOrArraySize *= (UINT16)curDepthDesc.SampleDesc.Count;
        curDepthDesc.SampleDesc.Count = 1;
        curDepthDesc.SampleDesc.Quality = 0;

        hr = m_pDevice->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &curDepthDesc,
                                                D3D12_RESOURCE_STATE_COMMON, NULL,
                                                __uuidof(ID3D12Resource), (void **)&overrideDepth);
        if(FAILED(hr))
        {
          RDCERR("Failed to create overrideDepth HRESULT: %s", ToStr(hr).c_str());
          return m_Overlay.resourceId;
        }

        dsv = GetDebugManager()->GetCPUHandle(OVERLAY_DSV);

        D3D12_DEPTH_STENCIL_VIEW_DESC viewDesc = rs.dsv.GetDSV();
        viewDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
        viewDesc.Texture2DArray.ArraySize = 1;
        viewDesc.Texture2DArray.FirstArraySlice = 0;
        viewDesc.Texture2DArray.MipSlice = 0;

        m_pDevice->CreateDepthStencilView(overrideDepth, &viewDesc, dsv);
      }

      // declare callback struct here
      D3D12QuadOverdrawCallback cb(m_pDevice, events, overrideDepth, overrideDepth ? curDepth : NULL,
                                   overrideDepth ? ToPortableHandle(dsv) : PortableHandle(),
                                   ToPortableHandle(GetDebugManager()->GetCPUHandle(OVERDRAW_UAV)));

      m_pDevice->ReplayLog(events.front(), events.back(), eReplay_Full);

      // resolve pass
      {
        list = m_pDevice->GetNewList();
        if(!list)
          return ResourceId();

        D3D12_RESOURCE_BARRIER overdrawBarriers[2] = {};

        // make sure UAV work is done then prepare for reading in PS
        overdrawBarriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
        overdrawBarriers[0].UAV.pResource = overdrawTex;
        overdrawBarriers[1].Transition.pResource = overdrawTex;
        overdrawBarriers[1].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        overdrawBarriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        overdrawBarriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

        // prepare tex resource for copying
        list->ResourceBarrier(2, overdrawBarriers);

        list->OMSetRenderTargets(1, &rtv, TRUE, NULL);

        D3D12_VIEWPORT view = {0.0f, 0.0f, (float)resourceDesc.Width, (float)resourceDesc.Height,
                               0.0f, 1.0f};
        list->RSSetViewports(1, &view);

        D3D12_RECT scissor = {0, 0, 16384, 16384};
        list->RSSetScissorRects(1, &scissor);

        list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        list->SetPipelineState(m_Overlay.QuadResolvePipe[Log2Floor(overlayTexDesc.SampleDesc.Count)]);

        list->SetGraphicsRootSignature(m_Overlay.QuadResolveRootSig);

        GetDebugManager()->SetDescriptorHeaps(list, true, false);

        list->SetGraphicsRootDescriptorTable(0, GetDebugManager()->GetGPUHandle(OVERDRAW_SRV));

        list->DrawInstanced(3, 1, 0, 0);

        list->Close();
        list = NULL;
      }

      m_pDevice->ExecuteLists();
      m_pDevice->FlushLists();

      for(auto it = cb.m_PipelineCache.begin(); it != cb.m_PipelineCache.end(); ++it)
      {
        SAFE_RELEASE(it->second.pipe);
        SAFE_RELEASE(it->second.sig);
      }

      SAFE_RELEASE(overdrawTex);
      SAFE_RELEASE(overrideDepth);
    }

    if(overlay == DebugOverlay::QuadOverdrawPass)
      m_pDevice->ReplayLog(0, eventId, eReplay_WithoutDraw);
  }
  else if(overlay == DebugOverlay::Depth || overlay == DebugOverlay::Stencil)
  {
    if(pipe && pipe->IsGraphics())
    {
      ID3D12Resource *renderDepthStencil = NULL;
      bool useDepthWriteStencilPass = (overlay == DebugOverlay::Depth) && renderDepth;

      if(useDepthWriteStencilPass)
      {
        useDepthWriteStencilPass = false;
        WrappedID3D12PipelineState::ShaderEntry *wrappedPS = pipe->PS();
        if(wrappedPS)
        {
          ShaderReflection &reflection = pipe->PS()->GetDetails();
          for(SigParameter &output : reflection.outputSignature)
          {
            if(output.systemValue == ShaderBuiltin::DepthOutput)
              useDepthWriteStencilPass = true;
          }
        }
      }

      HRESULT hr;
      DXGI_FORMAT dsFmt = dsViewDesc.Format;
      // the depth overlay uses stencil buffer as a mask for the passing pixels
      DXGI_FORMAT dsNewFmt = dsFmt;
      size_t fmtIndex = ARRAY_COUNT(m_Overlay.DepthCopyPipe);
      size_t sampleIndex = Log2Floor(overlayTexDesc.SampleDesc.Count);
      if(useDepthWriteStencilPass)
      {
        if(dsFmt == DXGI_FORMAT_D32_FLOAT_S8X24_UINT)
          dsNewFmt = DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
        else if(dsFmt == DXGI_FORMAT_D24_UNORM_S8_UINT)
          dsNewFmt = DXGI_FORMAT_D24_UNORM_S8_UINT;
        else if(dsFmt == DXGI_FORMAT_D32_FLOAT)
          dsNewFmt = DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
        else if(dsFmt == DXGI_FORMAT_D16_UNORM)
          dsNewFmt = DXGI_FORMAT_D24_UNORM_S8_UINT;
        else
          dsNewFmt = DXGI_FORMAT_D32_FLOAT_S8X24_UINT;

        RDCASSERT((dsNewFmt == DXGI_FORMAT_D24_UNORM_S8_UINT) ||
                  (dsNewFmt == DXGI_FORMAT_D32_FLOAT_S8X24_UINT));
        fmtIndex = (dsNewFmt == DXGI_FORMAT_D24_UNORM_S8_UINT) ? 0 : 1;
        if(m_Overlay.DepthResolvePipe[fmtIndex][sampleIndex] == NULL)
        {
          RDCERR("Unhandled depth resolve format : %s", ToStr(dsNewFmt).c_str());
          useDepthWriteStencilPass = false;
        }

        if(m_Overlay.DepthCopyPipe[fmtIndex][sampleIndex] == NULL)
        {
          RDCERR("Unhandled depth copy format : %s", ToStr(dsNewFmt).c_str());
          useDepthWriteStencilPass = false;
        }

        // Currently depth-copy is only supported for Texture2D and Texture2DMS
        if(dsFmt != dsNewFmt)
        {
          if(depthTexDesc.DepthOrArraySize > 1)
            useDepthWriteStencilPass = false;
          if((dsViewDesc.ViewDimension != D3D12_DSV_DIMENSION_TEXTURE2D) &&
             (dsViewDesc.ViewDimension != D3D12_DSV_DIMENSION_TEXTURE2DMS))
            useDepthWriteStencilPass = false;
        }
        if(!useDepthWriteStencilPass)
        {
          RDCWARN("Depth overlay using fallback method instead of stencil mask");
          dsNewFmt = dsFmt;
        }
      }
      if(useDepthWriteStencilPass)
      {
        // copy depth over to a new depth-stencil buffer
        if(dsFmt != dsNewFmt)
        {
          D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
          srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
          if(overlayTexDesc.SampleDesc.Count == 1)
          {
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
            srvDesc.Texture2D.MipLevels = ~0U;
            srvDesc.Texture2D.MostDetailedMip = 0;
            srvDesc.Texture2D.PlaneSlice = 0;
            srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
          }
          else
          {
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMS;
          }

          srvDesc.Format = DXGI_FORMAT_UNKNOWN;
          switch(dsFmt)
          {
            case DXGI_FORMAT_D32_FLOAT:
            case DXGI_FORMAT_R32_FLOAT:
            case DXGI_FORMAT_R32_TYPELESS: srvDesc.Format = DXGI_FORMAT_R32_FLOAT; break;

            case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
            case DXGI_FORMAT_R32G8X24_TYPELESS:
            case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
            case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
              srvDesc.Format = DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
              break;

            case DXGI_FORMAT_D24_UNORM_S8_UINT:
            case DXGI_FORMAT_R24G8_TYPELESS:
            case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
            case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
              srvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
              break;

            case DXGI_FORMAT_D16_UNORM:
            case DXGI_FORMAT_R16_TYPELESS: srvDesc.Format = DXGI_FORMAT_R16_UNORM; break;

            default: break;
          }
          if(srvDesc.Format == DXGI_FORMAT_UNKNOWN)
          {
            RDCERR("Unknown Depth overlay format %s", dsFmt);
            SAFE_RELEASE(renderDepth);
            return m_Overlay.resourceId;
          }

          m_pDevice->CreateShaderResourceView(renderDepth, &srvDesc,
                                              GetDebugManager()->GetCPUHandle(DEPTH_COPY_SRV));

          // New depth-stencil texture
          dsFmt = dsNewFmt;
          depthTexDesc.Format = dsFmt;
          hr = m_pDevice->CreateCommittedResource(
              &heapProps, D3D12_HEAP_FLAG_NONE, &depthTexDesc, D3D12_RESOURCE_STATE_DEPTH_WRITE,
              NULL, __uuidof(ID3D12Resource), (void **)&renderDepthStencil);
          if(FAILED(hr))
          {
            RDCERR("Failed to create renderDepthStencil HRESULT: %s", ToStr(hr).c_str());
            SAFE_RELEASE(renderDepth);
            return m_Overlay.resourceId;
          }

          // Copy renderDepth depth data into renderDepthStencil depth data using fullscreen pass
          // the shader writes 0 to the stencil during the copy
          D3D12_RESOURCE_BARRIER b = {};

          b.Transition.pResource = renderDepth;
          b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
          b.Transition.StateBefore = D3D12_RESOURCE_STATE_DEPTH_WRITE;
          b.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
          list->ResourceBarrier(1, &b);

          D3D12_DEPTH_STENCIL_VIEW_DESC dsNewViewDesc = dsViewDesc;
          dsNewViewDesc.Format = dsFmt;
          m_pDevice->CreateDepthStencilView(renderDepthStencil, &dsNewViewDesc, dsv);

          list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

          D3D12_VIEWPORT view = {0.0f, 0.0f, (float)resourceDesc.Width, (float)resourceDesc.Height,
                                 0.0f, 1.0f};
          list->RSSetViewports(1, &view);

          D3D12_RECT scissor = {0, 0, 16384, 16384};
          list->RSSetScissorRects(1, &scissor);

          list->SetPipelineState(m_Overlay.DepthCopyPipe[fmtIndex][sampleIndex]);
          list->SetGraphicsRootSignature(m_Overlay.DepthCopyResolveRootSig);

          GetDebugManager()->SetDescriptorHeaps(list, true, false);
          list->SetGraphicsRootDescriptorTable(0, GetDebugManager()->GetGPUHandle(DEPTH_COPY_SRV));

          list->OMSetRenderTargets(0, NULL, FALSE, &dsv);

          list->DrawInstanced(3, 1, 0, 0);

          rs.ApplyState(m_pDevice, list);
        }
      }

      D3D12_EXPANDED_PIPELINE_STATE_STREAM_DESC psoDesc;
      pipe->Fill(psoDesc);

      bool dxil =
          psoDesc.MS.BytecodeLength > 0 ||
          DXBC::DXBCContainer::CheckForDXIL(psoDesc.VS.pShaderBytecode, psoDesc.VS.BytecodeLength);

      ID3DBlob *red = m_pDevice->GetShaderCache()->MakeFixedColShader(D3D12ShaderCache::RED, dxil);
      ID3DBlob *green =
          m_pDevice->GetShaderCache()->MakeFixedColShader(D3D12ShaderCache::GREEN, dxil);

      D3D12_SHADER_BYTECODE originalPS = psoDesc.PS;

      // make sure that if a test is disabled, it shows all
      // pixels passing
      if(!psoDesc.DepthStencilState.DepthEnable)
        psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
      if(!psoDesc.DepthStencilState.StencilEnable)
      {
        psoDesc.DepthStencilState.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
        psoDesc.DepthStencilState.BackFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
      }

      if(useDepthWriteStencilPass)
      {
        // Do not replace shader
        // disable colour write
        psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = 0x0;
        // Write stencil 0x1 for depth passing pixels
        psoDesc.DepthStencilState.StencilEnable = TRUE;
        psoDesc.DepthStencilState.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
        psoDesc.DepthStencilState.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
        psoDesc.DepthStencilState.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
        psoDesc.DepthStencilState.FrontFace.StencilPassOp = D3D12_STENCIL_OP_REPLACE;
        psoDesc.DepthStencilState.FrontFace.StencilReadMask = 0xff;
        psoDesc.DepthStencilState.FrontFace.StencilWriteMask = 0xff;
        psoDesc.DepthStencilState.BackFace = psoDesc.DepthStencilState.FrontFace;
      }
      else
      {
        psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
        if(overlay == DebugOverlay::Depth)
        {
          psoDesc.DepthStencilState.StencilEnable = FALSE;
          psoDesc.DepthStencilState.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
          psoDesc.DepthStencilState.BackFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
        }
        else
        {
          psoDesc.DepthStencilState.DepthEnable = FALSE;
          psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
          psoDesc.DepthStencilState.DepthBoundsTestEnable = FALSE;
        }
      }

      RDCEraseEl(psoDesc.RTVFormats.RTFormats);
      psoDesc.RTVFormats.RTFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
      psoDesc.RTVFormats.NumRenderTargets = 1;
      psoDesc.SampleMask = ~0U;
      psoDesc.SampleDesc.Count = RDCMAX(1U, psoDesc.SampleDesc.Count);
      psoDesc.BlendState.AlphaToCoverageEnable = FALSE;
      psoDesc.BlendState.IndependentBlendEnable = FALSE;
      psoDesc.BlendState.RenderTarget[0].BlendEnable = FALSE;
      psoDesc.BlendState.RenderTarget[0].LogicOpEnable = FALSE;

      psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
      psoDesc.RasterizerState.LineRasterizationMode = D3D12_LINE_RASTERIZATION_MODE_ALIASED;

      float clearColour[] = {0.0f, 0.0f, 0.0f, 0.0f};
      list->ClearRenderTargetView(rtv, clearColour, 0, NULL);

      list->Close();
      list = NULL;

      if(!red || !green)
      {
        SAFE_RELEASE(red);
        SAFE_RELEASE(green);
        SAFE_RELEASE(renderDepthStencil);
        SAFE_RELEASE(renderDepth);
        m_pDevice->AddDebugMessage(MessageCategory::Shaders, MessageSeverity::High,
                                   MessageSource::UnsupportedConfiguration,
                                   "No DXIL shader available for overlay");
        return m_Overlay.resourceId;
      }

      psoDesc.PS.pShaderBytecode = green->GetBufferPointer();
      psoDesc.PS.BytecodeLength = green->GetBufferSize();

      ID3D12PipelineState *greenPSO = NULL;
      hr = m_pDevice->CreatePipeState(psoDesc, &greenPSO);
      if(FAILED(hr))
      {
        RDCERR("Failed to create overlay pso HRESULT: %s", ToStr(hr).c_str());
        SAFE_RELEASE(red);
        SAFE_RELEASE(green);
        SAFE_RELEASE(renderDepthStencil);
        SAFE_RELEASE(renderDepth);
        return m_Overlay.resourceId;
      }

      ID3D12PipelineState *depthWriteStencilPSO = NULL;
      if(useDepthWriteStencilPass)
      {
        psoDesc.DSVFormat = dsFmt;
        psoDesc.PS = originalPS;

        hr = m_pDevice->CreatePipeState(psoDesc, &depthWriteStencilPSO);
        if(FAILED(hr))
        {
          RDCERR("Failed to create depth write overlay pso HRESULT: %s", ToStr(hr).c_str());
          SAFE_RELEASE(greenPSO);
          SAFE_RELEASE(red);
          SAFE_RELEASE(green);
          SAFE_RELEASE(renderDepthStencil);
          SAFE_RELEASE(renderDepth);
          return m_Overlay.resourceId;
        }
      }

      psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

      psoDesc.DepthStencilState.DepthEnable = FALSE;
      psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
      psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
      psoDesc.DepthStencilState.StencilEnable = FALSE;
      psoDesc.DepthStencilState.DepthBoundsTestEnable = FALSE;

      psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
      psoDesc.RasterizerState.DepthClipEnable = FALSE;

      psoDesc.PS.pShaderBytecode = red->GetBufferPointer();
      psoDesc.PS.BytecodeLength = red->GetBufferSize();

      ID3D12PipelineState *redPSO = NULL;
      hr = m_pDevice->CreatePipeState(psoDesc, &redPSO);
      if(FAILED(hr))
      {
        RDCERR("Failed to create overlay pso HRESULT: %s", ToStr(hr).c_str());
        SAFE_RELEASE(depthWriteStencilPSO);
        SAFE_RELEASE(greenPSO);
        SAFE_RELEASE(red);
        SAFE_RELEASE(green);
        SAFE_RELEASE(renderDepthStencil);
        SAFE_RELEASE(renderDepth);
        return m_Overlay.resourceId;
      }

      D3D12RenderState prev = rs;

      rs.pipe = GetResID(redPSO);
      rs.rts.resize(1);
      rs.rts[0] = *GetWrapped(rtv);
      if(dsv.ptr)
        rs.dsv = *GetWrapped(dsv);
      else
        RDCEraseEl(rs.dsv);

      m_pDevice->ReplayLog(0, eventId, eReplay_OnlyDraw);

      if(useDepthWriteStencilPass)
      {
        rs.stencilRefBack = rs.stencilRefFront = 0x1;
        rs.pipe = GetResID(depthWriteStencilPSO);
      }
      else
      {
        rs.pipe = GetResID(greenPSO);
      }

      if(useDepthWriteStencilPass)
      {
        list = m_pDevice->GetNewList();
        if(!list)
          return ResourceId();
        list->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_STENCIL, 0.0f, 0, 0, NULL);
        list->Close();
        list = NULL;
      }

      m_pDevice->ReplayLog(0, eventId, eReplay_OnlyDraw);

      rs = prev;

      if(useDepthWriteStencilPass)
      {
        // Resolve stencil = 0x1 pixels to green
        list = m_pDevice->GetNewList();
        if(!list)
          return ResourceId();

        list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        D3D12_VIEWPORT view = {0.0f, 0.0f, (float)resourceDesc.Width, (float)resourceDesc.Height,
                               0.0f, 1.0f};
        list->RSSetViewports(1, &view);

        D3D12_RECT scissor = {0, 0, 16384, 16384};
        list->RSSetScissorRects(1, &scissor);

        RDCASSERT((dsFmt == DXGI_FORMAT_D24_UNORM_S8_UINT) ||
                  (dsFmt == DXGI_FORMAT_D32_FLOAT_S8X24_UINT));
        fmtIndex = (dsFmt == DXGI_FORMAT_D24_UNORM_S8_UINT) ? 0 : 1;

        list->SetPipelineState(m_Overlay.DepthResolvePipe[fmtIndex][sampleIndex]);
        list->SetGraphicsRootSignature(m_Overlay.DepthCopyResolveRootSig);

        GetDebugManager()->SetDescriptorHeaps(list, true, false);
        list->SetGraphicsRootDescriptorTable(0, GetDebugManager()->GetGPUHandle(DEPTH_COPY_SRV));

        list->OMSetStencilRef(0x1);
        list->OMSetRenderTargets(1, &rtv, TRUE, &dsv);

        list->DrawInstanced(3, 1, 0, 0);

        list->Close();
        list = NULL;
      }

      m_pDevice->ExecuteLists();
      m_pDevice->FlushLists();

      SAFE_RELEASE(red);
      SAFE_RELEASE(green);
      SAFE_RELEASE(redPSO);
      SAFE_RELEASE(greenPSO);
      SAFE_RELEASE(depthWriteStencilPSO);
      SAFE_RELEASE(renderDepthStencil);
    }
  }
  else
  {
    RDCERR("Unhandled overlay case!");
  }

  if(list)
    list->Close();

  m_pDevice->ExecuteLists();
  m_pDevice->FlushLists();

  SAFE_RELEASE(renderDepth);

  return m_Overlay.resourceId;
}
