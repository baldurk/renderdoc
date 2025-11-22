/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2024-2025 Baldur Karlsson
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

#pragma once

#include "driver/shaders/dxil/dxil_debug.h"
#include "d3d12_device.h"
#include "d3d12_shaderdebug.h"
#include "d3d12_state.h"

namespace DXILDebug
{
class Debugger;

class D3D12APIWrapper : public DebugAPIWrapper
{
public:
  D3D12APIWrapper(WrappedID3D12Device *device, const DXIL::Program *dxilProgram,
                  const ShaderReflection &refl, uint32_t eventId,
                  const rdcarray<SigParameter> &inputSig);
  ~D3D12APIWrapper();

  void FetchConstantBufferData(const D3D12RenderState::RootSignature &rootsig);

  ShaderValue TypedUAVLoad(const BindingSlot &slot, const DXILDebug::ViewFmt &fmt,
                           uint64_t dataOffset) const override;
  ShaderValue TypedSRVLoad(const BindingSlot &slot, const DXILDebug::ViewFmt &fmt,
                           uint64_t dataOffset) const override;
  bool TypedUAVStore(const BindingSlot &slot, const DXILDebug::ViewFmt &fmt, uint64_t dataOffset,
                     const ShaderValue &value) override;
  bool TypedSRVStore(const BindingSlot &slot, const DXILDebug::ViewFmt &fmt, uint64_t dataOffset,
                     const ShaderValue &value) override;
  UAVInfo GetUAV(const BindingSlot &slot) override;
  SRVInfo GetSRV(const BindingSlot &slot) override;

  bool QueueMathIntrinsic(DXIL::DXOp dxOp, const ShaderVariable &input) override;
  bool QueueSampleGather(DXIL::DXOp dxOp, SampleGatherResourceData resourceData,
                         SampleGatherSamplerData samplerData, const ShaderVariable &uv,
                         const ShaderVariable &ddxCalc, const ShaderVariable &ddyCalc,
                         const int8_t texelOffsets[3], int multisampleIndex, float lodValue,
                         float compareValue, GatherChannel gatherChannel, uint32_t instructionIdx,
                         int &sampleRetType) override;
  bool GetQueuedResults(rdcarray<ShaderVariable *> &mathOpResults,
                        rdcarray<ShaderVariable *> &sampleGatherResults,
                        const rdcarray<int> &sampleRetTypes) override;
  bool QueuedOpsHasSpace() const override;

  ShaderVariable GetResourceInfo(DXIL::ResourceClass resClass, const DXDebug::BindingSlot &slot,
                                 uint32_t mipLevel) override;
  ShaderVariable GetSampleInfo(DXIL::ResourceClass resClass, const DXDebug::BindingSlot &slot,
                               const char *opString) override;
  ShaderVariable GetRenderTargetSampleInfo(const char *opString) override;
  ResourceReferenceInfo GetResourceReferenceInfo(const DXDebug::BindingSlot &slot) override;
  ShaderDirectAccess GetShaderDirectAccess(DescriptorType type,
                                           const DXDebug::BindingSlot &slot) override;

  bool IsSRVCached(const DXDebug::BindingSlot &slot) const override;
  bool IsUAVCached(const DXDebug::BindingSlot &slot) const override;
  bool IsResourceInfoCached(const DXDebug::BindingSlot &slot, uint32_t mipLevel) override;
  bool IsSampleInfoCached(const DXDebug::BindingSlot &slot) override;
  bool IsRenderTargetSampleInfoCached() override;
  bool IsResourceReferenceInfoCached(const DXDebug::BindingSlot &slot) override;
  bool IsShaderDirectAccessCached(const DXDebug::BindingSlot &slot) override;

  void ResetReplay();

  void SetBuiltins(const BuiltinInputs &builtins) { m_Builtins = builtins; }
  void SetWorkgroupProperties(const rdcarray<DXILDebug::ThreadProperties> &workgroupProperties)
  {
    m_WorkgroupProperties = workgroupProperties;
  }
  void SetThreadsInputs(const rdcarray<ShaderVariable> &threadsInputs)
  {
    m_ThreadsInputs = threadsInputs;
  }
  void SetThreadsBuiltins(rdcarray<rdcflatmap<ShaderBuiltin, ShaderVariable>> &threadsBuiltins)
  {
    m_ThreadsBuiltins = threadsBuiltins;
  }
  void SetSubgroupSize(uint32_t subgroupSize) { m_SubgroupSize = subgroupSize; }

  const rdcarray<DXIL::EntryPointInterface::Signature> &GetDXILEntryPointInputs(void) const
  {
    return m_EntryPointInterface->inputs;
  }

  const rdcarray<DXILDebug::ThreadProperties> &GetWorkgroupProperties() const override
  {
    return m_WorkgroupProperties;
  }
  const rdcarray<ShaderVariable> &GetConstantBlocks() const override { return m_ConstantBlocks; }
  const std::map<ConstantBlockReference, bytebuf> &GetConstantBlocksDatas() const override
  {
    return m_ConstantBlocksDatas;
  }

  const BuiltinInputs &GetBuiltins() const override { return m_Builtins; }
  uint32_t GetSubgroupSize() const override { return m_SubgroupSize; }
  const rdcarray<rdcflatmap<ShaderBuiltin, ShaderVariable>> &GetThreadsBuiltins() const override
  {
    return m_ThreadsBuiltins;
  }
  const rdcarray<ShaderVariable> &GetThreadsInputs() const override { return m_ThreadsInputs; }
  const rdcarray<SourceVariableMapping> &GetSourceVars() const override { return m_SourceVars; }
  const ShaderVariable &GetInputPlaceholder() const override { return m_InputPlaceholder; }

private:
  bool IsDeviceThread() const { return Threading::GetCurrentID() == m_DeviceThreadID; }
  void PrepareReplayForResources();
  void AddCBufferToGlobalState(const BindingSlot &slot, bytebuf &cbufData);
  void FlattenSingleVariable(const rdcstr &cbufferName, uint32_t byteOffset, const rdcstr &basename,
                             const ShaderVariable &v, rdcarray<ShaderVariable> &outvars);
  void FlattenVariables(const rdcstr &cbufferName, const rdcarray<ShaderConstant> &constants,
                        const rdcarray<ShaderVariable> &invars, rdcarray<ShaderVariable> &outvars,
                        const rdcstr &prefix, uint32_t baseOffset);

  SRVInfo FetchSRV(const BindingSlot &slot);
  SRVInfo FetchSRV(const D3D12Descriptor *resDescriptor, const BindingSlot &slot);

  UAVInfo FetchUAV(const BindingSlot &slot);
  UAVInfo FetchUAV(const D3D12Descriptor *resDescriptor, const BindingSlot &slot);

  ShaderVariable FetchResourceInfo(DXIL::ResourceClass resClass, const DXDebug::BindingSlot &slot,
                                   uint32_t mipLevel);
  ShaderVariable FetchSampleInfo(DXIL::ResourceClass resClass, const DXDebug::BindingSlot &slot,
                                 const char *opString);
  ResourceReferenceInfo FetchResourceReferenceInfo(const DXDebug::BindingSlot &slot);
  ShaderDirectAccess FetchShaderDirectAccess(DescriptorType type, const DXDebug::BindingSlot &slot);
  bool StartQueuedOps();

  BuiltinInputs m_Builtins;
  rdcarray<DXILDebug::ThreadProperties> m_WorkgroupProperties;
  rdcarray<ShaderVariable> m_ThreadsInputs;
  rdcarray<rdcflatmap<ShaderBuiltin, ShaderVariable>> m_ThreadsBuiltins;
  rdcarray<SourceVariableMapping> m_SourceVars;
  rdcarray<ShaderVariable> m_ConstantBlocks;
  std::map<ConstantBlockReference, bytebuf> m_ConstantBlocksDatas;
  ShaderVariable m_InputPlaceholder;
  uint32_t m_SubgroupSize = 1;

  struct ResourceInfoMiplevel
  {
    BindingSlot slot;
    uint32_t mipLevel;

    bool operator<(const ResourceInfoMiplevel &o) const
    {
      if(mipLevel == o.mipLevel)
        return slot < o.slot;
      return mipLevel < o.mipLevel;
    }

    bool operator==(const ResourceInfoMiplevel &o) const
    {
      return slot == o.slot && mipLevel == o.mipLevel;
    }
  };

  mutable Threading::RWLock m_UAVsLock;
  std::map<BindingSlot, UAVInfo> m_UAVInfos;
  std::map<BindingSlot, bytebuf> m_UAVBuffers;
  mutable Threading::RWLock m_SRVsLock;
  std::map<BindingSlot, SRVInfo> m_SRVInfos;
  std::map<BindingSlot, bytebuf> m_SRVBuffers;
  Threading::RWLock m_ResourceInfosLock;
  std::map<ResourceInfoMiplevel, ShaderVariable> m_ResourceInfos;
  Threading::RWLock m_SampleInfosLock;
  std::map<BindingSlot, ShaderVariable> m_SampleInfos;
  Threading::RWLock m_ResourceReferenceInfosLock;
  std::map<BindingSlot, ResourceReferenceInfo> m_ResourceReferenceInfos;

  Threading::RWLock m_RenderTargetSampleInfoLock;
  ShaderVariable m_RenderTargetSampleInfo;
  bool m_RenderTargetSampleInfoValid = false;

  Threading::RWLock m_ShaderDirectAccessesLock;
  std::map<BindingSlot, ShaderDirectAccess> m_ShaderDirectAccesses;

  const ShaderReflection &m_Reflection;
  WrappedID3D12Device *m_Device = NULL;

  ID3D12GraphicsCommandListX *m_QueuedOpCmdList = NULL;
  uint32_t m_QueuedMathOpIndex = 0;
  uint32_t m_QueuedSampleGatherOpIndex = 0;
  uint64_t m_MathOpResultOffset = 0;
  const uint32_t m_MaxQueuedOps = 0;
  const uint64_t m_MathOpResultByteSize = sizeof(Vec4f) * 2;
  const uint64_t m_SampleGatherOpResultByteSize = sizeof(Vec4f);
  const uint64_t m_SampleGatherOpResultsStart;

  const DXIL::Program *m_Program = NULL;
  const DXIL::EntryPointInterface *m_EntryPointInterface = NULL;
  const DXBC::ShaderType m_ShaderType;
  const uint64_t m_DeviceThreadID;
  const uint32_t m_EventId;
  bool m_ResourcesDirty = true;
};

};
