/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2017 Baldur Karlsson
 * Copyright (c) 2014 Crytek
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

#include "replay_proxy.h"
#include "lz4/lz4.h"

// these functions do compile time asserts on the size of the structure, to
// help prevent the structure changing without these functions being updated.
// This isn't perfect as a new variable could be added in padding space, or
// one removed and leaves padding. Most variables are 4 bytes in size though
// so it should be fairly reliable and it's better than nothing!
// Since structures contain pointers and vary in size, we do this only on
// Win32 to try and hide less padding with the larger alignment requirement
// of 8-byte pointers.

#if ENABLED(RDOC_WIN32) && ENABLED(RDOC_X64)
template <typename T, size_t actual, size_t expected>
class oversized
{
  int check[int(actual) - int(expected) + 1];
};
template <typename T, size_t actual, size_t expected>
class undersized
{
  int check[int(expected) - int(actual) + 1];
};

#define SIZE_CHECK(expected)                        \
  undersized<decltype(el), sizeof(el), expected>(); \
  oversized<decltype(el), sizeof(el), expected>();
#else
#define SIZE_CHECK(expected)
#endif

#pragma region General Shader / State

template <>
string ToStrHelper<false, ShaderBuiltin>::Get(const ShaderBuiltin &el)
{
  switch(el)
  {
    TOSTR_CASE_STRINGIZE_SCOPED(ShaderBuiltin, Undefined)
    TOSTR_CASE_STRINGIZE_SCOPED(ShaderBuiltin, Position)
    TOSTR_CASE_STRINGIZE_SCOPED(ShaderBuiltin, PointSize)
    TOSTR_CASE_STRINGIZE_SCOPED(ShaderBuiltin, ClipDistance)
    TOSTR_CASE_STRINGIZE_SCOPED(ShaderBuiltin, CullDistance)
    TOSTR_CASE_STRINGIZE_SCOPED(ShaderBuiltin, RTIndex)
    TOSTR_CASE_STRINGIZE_SCOPED(ShaderBuiltin, ViewportIndex)
    TOSTR_CASE_STRINGIZE_SCOPED(ShaderBuiltin, VertexIndex)
    TOSTR_CASE_STRINGIZE_SCOPED(ShaderBuiltin, PrimitiveIndex)
    TOSTR_CASE_STRINGIZE_SCOPED(ShaderBuiltin, InstanceIndex)
    TOSTR_CASE_STRINGIZE_SCOPED(ShaderBuiltin, DispatchSize)
    TOSTR_CASE_STRINGIZE_SCOPED(ShaderBuiltin, DispatchThreadIndex)
    TOSTR_CASE_STRINGIZE_SCOPED(ShaderBuiltin, GroupIndex)
    TOSTR_CASE_STRINGIZE_SCOPED(ShaderBuiltin, GroupFlatIndex)
    TOSTR_CASE_STRINGIZE_SCOPED(ShaderBuiltin, GroupThreadIndex)
    TOSTR_CASE_STRINGIZE_SCOPED(ShaderBuiltin, GSInstanceIndex)
    TOSTR_CASE_STRINGIZE_SCOPED(ShaderBuiltin, OutputControlPointIndex)
    TOSTR_CASE_STRINGIZE_SCOPED(ShaderBuiltin, DomainLocation)
    TOSTR_CASE_STRINGIZE_SCOPED(ShaderBuiltin, IsFrontFace)
    TOSTR_CASE_STRINGIZE_SCOPED(ShaderBuiltin, MSAACoverage)
    TOSTR_CASE_STRINGIZE_SCOPED(ShaderBuiltin, MSAASamplePosition)
    TOSTR_CASE_STRINGIZE_SCOPED(ShaderBuiltin, MSAASampleIndex)
    TOSTR_CASE_STRINGIZE_SCOPED(ShaderBuiltin, PatchNumVertices)
    TOSTR_CASE_STRINGIZE_SCOPED(ShaderBuiltin, OuterTessFactor)
    TOSTR_CASE_STRINGIZE_SCOPED(ShaderBuiltin, InsideTessFactor)
    TOSTR_CASE_STRINGIZE_SCOPED(ShaderBuiltin, ColorOutput)
    TOSTR_CASE_STRINGIZE_SCOPED(ShaderBuiltin, DepthOutput)
    TOSTR_CASE_STRINGIZE_SCOPED(ShaderBuiltin, DepthOutputGreaterEqual)
    TOSTR_CASE_STRINGIZE_SCOPED(ShaderBuiltin, DepthOutputLessEqual)
    default: break;
  }

  char tostrBuf[256] = {0};
  StringFormat::snprintf(tostrBuf, 255, "SystemAttribute<%d>", el);

  return tostrBuf;
}

template <>
void Serialiser::Serialise(const char *name, ResourceFormat &el)
{
  Serialise("", el.special);
  Serialise("", el.specialFormat);
  Serialise("", el.strname);
  Serialise("", el.compCount);
  Serialise("", el.compByteWidth);
  Serialise("", el.compType);
  Serialise("", el.bgraOrder);
  Serialise("", el.srgbCorrected);

  SIZE_CHECK(48);
}

template <>
void Serialiser::Serialise(const char *name, BindpointMap &el)
{
  Serialise("", el.bindset);
  Serialise("", el.bind);
  Serialise("", el.used);
  Serialise("", el.arraySize);

  SIZE_CHECK(16);
}

template <>
void Serialiser::Serialise(const char *name, ShaderBindpointMapping &el)
{
  Serialise("", el.InputAttributes);
  Serialise("", el.ConstantBlocks);
  Serialise("", el.ReadOnlyResources);
  Serialise("", el.ReadWriteResources);

  SIZE_CHECK(64);
}

template <>
void Serialiser::Serialise(const char *name, SigParameter &el)
{
  Serialise("", el.varName);
  Serialise("", el.semanticName);
  Serialise("", el.semanticIndex);
  Serialise("", el.semanticIdxName);
  Serialise("", el.needSemanticIndex);
  Serialise("", el.regIndex);
  Serialise("", el.systemValue);
  Serialise("", el.compType);
  Serialise("", el.regChannelMask);
  Serialise("", el.channelUsedMask);
  Serialise("", el.compCount);
  Serialise("", el.stream);
  Serialise("", el.arrayIndex);

  SIZE_CHECK(88);
}

template <>
void Serialiser::Serialise(const char *name, ShaderVariableType &el)
{
  Serialise("", el.descriptor.name);
  Serialise("", el.descriptor.type);
  Serialise("", el.descriptor.rows);
  Serialise("", el.descriptor.cols);
  Serialise("", el.descriptor.elements);
  Serialise("", el.descriptor.rowMajorStorage);
  Serialise("", el.descriptor.arrayStride);
  Serialise("", el.members);

  SIZE_CHECK(56);
}

template <>
void Serialiser::Serialise(const char *name, ShaderConstant &el)
{
  Serialise("", el.name);
  Serialise("", el.reg.vec);
  Serialise("", el.reg.comp);
  Serialise("", el.defaultValue);
  Serialise("", el.type);

  SIZE_CHECK(88);
}

template <>
void Serialiser::Serialise(const char *name, ConstantBlock &el)
{
  Serialise("", el.name);
  Serialise("", el.variables);
  Serialise("", el.bufferBacked);
  Serialise("", el.bindPoint);
  Serialise("", el.byteSize);

  SIZE_CHECK(48);
}

template <>
void Serialiser::Serialise(const char *name, ShaderResource &el)
{
  Serialise("", el.IsSampler);
  Serialise("", el.IsTexture);
  Serialise("", el.IsReadOnly);
  Serialise("", el.resType);
  Serialise("", el.name);
  Serialise("", el.variableType);
  Serialise("", el.bindPoint);

  SIZE_CHECK(96);
}

template <>
void Serialiser::Serialise(const char *name, ShaderReflection &el)
{
  Serialise("", el.ID);
  Serialise("", el.EntryPoint);

  Serialise("", el.DebugInfo.compileFlags);
  Serialise("", el.DebugInfo.files);

  SerialisePODArray<3>("", el.DispatchThreadsDimension);

  Serialise("", el.RawBytes);

  Serialise("", el.InputSig);
  Serialise("", el.OutputSig);

  Serialise("", el.ConstantBlocks);

  Serialise("", el.ReadOnlyResources);
  Serialise("", el.ReadWriteResources);

  Serialise("", el.Interfaces);

  SIZE_CHECK(176);
}

template <>
void Serialiser::Serialise(const char *name, ShaderVariable &el)
{
  Serialise("", el.rows);
  Serialise("", el.columns);
  Serialise("", el.name);
  Serialise("", el.type);

  SerialisePODArray<16>("", el.value.dv);

  Serialise("", el.isStruct);

  Serialise("", el.members);

  SIZE_CHECK(184);
}

template <>
void Serialiser::Serialise(const char *name, ShaderDebugState &el)
{
  Serialise("", el.registers);
  Serialise("", el.outputs);
  Serialise("", el.nextInstruction);
  Serialise("", el.flags);

  vector<vector<ShaderVariable> > indexableTemps;

  int32_t numidxtemps = el.indexableTemps.count;
  Serialise("", numidxtemps);

  if(m_Mode == READING)
    create_array_uninit(el.indexableTemps, numidxtemps);

  for(int32_t i = 0; i < numidxtemps; i++)
    Serialise("", el.indexableTemps[i]);

  SIZE_CHECK(56);
}

template <>
void Serialiser::Serialise(const char *name, ShaderDebugTrace &el)
{
  Serialise("", el.inputs);

  int32_t numcbuffers = el.cbuffers.count;
  Serialise("", numcbuffers);

  if(m_Mode == READING)
    create_array_uninit(el.cbuffers, numcbuffers);

  for(int32_t i = 0; i < numcbuffers; i++)
    Serialise("", el.cbuffers[i]);

  Serialise("", el.states);

  SIZE_CHECK(48);
}

#pragma endregion General Shader / State

#pragma region D3D11 pipeline state

template <>
void Serialiser::Serialise(const char *name, D3D11Pipe::Layout &el)
{
  Serialise("", el.SemanticName);
  Serialise("", el.SemanticIndex);
  Serialise("", el.Format);
  Serialise("", el.InputSlot);
  Serialise("", el.ByteOffset);
  Serialise("", el.PerInstance);
  Serialise("", el.InstanceDataStepRate);

  SIZE_CHECK(88);
}

template <>
void Serialiser::Serialise(const char *name, D3D11Pipe::IA &el)
{
  Serialise("", el.ibuffer.Buffer);
  Serialise("", el.ibuffer.Offset);

  Serialise("", el.customName);
  Serialise("", el.name);

  Serialise("", el.vbuffers);
  Serialise("", el.layouts);

  SIZE_CHECK(88);
}

template <>
void Serialiser::Serialise(const char *name, D3D11Pipe::View &el)
{
  Serialise("", el.Object);
  Serialise("", el.Resource);
  Serialise("", el.Type);
  Serialise("", el.Format);

  Serialise("", el.Structured);
  Serialise("", el.BufferStructCount);
  Serialise("", el.FirstElement);
  Serialise("", el.NumElements);

  Serialise("", el.Flags);
  Serialise("", el.HighestMip);
  Serialise("", el.NumMipLevels);
  Serialise("", el.ArraySize);
  Serialise("", el.FirstArraySlice);

  SIZE_CHECK(112);
}

template <>
void Serialiser::Serialise(const char *name, D3D11Pipe::Sampler &el)
{
  Serialise("", el.Samp);
  Serialise("", el.name);
  Serialise("", el.customName);
  Serialise("", el.AddressU);
  Serialise("", el.AddressV);
  Serialise("", el.AddressW);
  SerialisePODArray<4>("", el.BorderColor);
  Serialise("", el.Comparison);
  Serialise("", el.Filter);
  Serialise("", el.MaxAniso);
  Serialise("", el.MaxLOD);
  Serialise("", el.MinLOD);
  Serialise("", el.MipLODBias);

  SIZE_CHECK(96);
}

template <>
void Serialiser::Serialise(const char *name, D3D11Pipe::Shader &el)
{
  Serialise("", el.Object);
  Serialise("", el.stage);
  Serialise("", el.name);
  Serialise("", el.customName);

  if(m_Mode == READING)
    el.ShaderDetails = NULL;

  Serialise("", el.BindpointMapping);

  Serialise("", el.SRVs);
  Serialise("", el.UAVs);
  Serialise("", el.Samplers);
  Serialise("", el.ConstantBuffers);
  Serialise("", el.ClassInstances);

  SIZE_CHECK(192);
}

template <>
void Serialiser::Serialise(const char *name, D3D11Pipe::Rasterizer &el)
{
  Serialise("", el.m_State);
  Serialise("", el.Scissors);
  Serialise("", el.Viewports);

  SIZE_CHECK(88);
}

template <>
void Serialiser::Serialise(const char *name, D3D11Pipe::Blend &el)
{
  Serialise("", el.m_Blend.Source);
  Serialise("", el.m_Blend.Destination);
  Serialise("", el.m_Blend.Operation);

  Serialise("", el.m_AlphaBlend.Source);
  Serialise("", el.m_AlphaBlend.Destination);
  Serialise("", el.m_AlphaBlend.Operation);

  Serialise("", el.Logic);

  Serialise("", el.Enabled);
  Serialise("", el.LogicEnabled);
  Serialise("", el.WriteMask);

  SIZE_CHECK(40);
}

template <>
void Serialiser::Serialise(const char *name, D3D11Pipe::OM &el)
{
  {
    Serialise("", el.m_State.State);
    Serialise("", el.m_State.DepthEnable);
    Serialise("", el.m_State.DepthFunc);
    Serialise("", el.m_State.DepthWrites);
    Serialise("", el.m_State.StencilEnable);
    Serialise("", el.m_State.StencilReadMask);
    Serialise("", el.m_State.StencilWriteMask);

    Serialise("", el.m_State.m_FrontFace.FailOp);
    Serialise("", el.m_State.m_FrontFace.DepthFailOp);
    Serialise("", el.m_State.m_FrontFace.PassOp);
    Serialise("", el.m_State.m_FrontFace.Func);

    Serialise("", el.m_State.m_BackFace.FailOp);
    Serialise("", el.m_State.m_BackFace.DepthFailOp);
    Serialise("", el.m_State.m_BackFace.PassOp);
    Serialise("", el.m_State.m_BackFace.Func);

    Serialise("", el.m_State.StencilRef);
  }

  {
    Serialise("", el.m_BlendState.State);
    Serialise("", el.m_BlendState.AlphaToCoverage);
    Serialise("", el.m_BlendState.IndependentBlend);
    Serialise("", el.m_BlendState.Blends);
    SerialisePODArray<4>("", el.m_BlendState.BlendFactor);

    Serialise("", el.m_BlendState.SampleMask);
  }

  Serialise("", el.RenderTargets);
  Serialise("", el.UAVStartSlot);
  Serialise("", el.UAVs);
  Serialise("", el.DepthTarget);
  Serialise("", el.DepthReadOnly);
  Serialise("", el.StencilReadOnly);

  SIZE_CHECK(280);
}

template <>
void Serialiser::Serialise(const char *name, D3D11Pipe::State &el)
{
  Serialise("", el.m_IA);

  Serialise("", el.m_VS);
  Serialise("", el.m_HS);
  Serialise("", el.m_DS);
  Serialise("", el.m_GS);
  Serialise("", el.m_PS);
  Serialise("", el.m_CS);

  Serialise("", el.m_SO.Outputs);

  Serialise("", el.m_RS);
  Serialise("", el.m_OM);

  SIZE_CHECK(1624);
}

#pragma endregion D3D11 pipeline state

#pragma region D3D12 pipeline state

template <>
void Serialiser::Serialise(const char *name, D3D12Pipe::Layout &el)
{
  Serialise("", el.SemanticName);
  Serialise("", el.SemanticIndex);
  Serialise("", el.Format);
  Serialise("", el.InputSlot);
  Serialise("", el.ByteOffset);
  Serialise("", el.PerInstance);
  Serialise("", el.InstanceDataStepRate);

  SIZE_CHECK(88);
}

template <>
void Serialiser::Serialise(const char *name, D3D12Pipe::IA &el)
{
  Serialise("", el.ibuffer.Buffer);
  Serialise("", el.ibuffer.Offset);
  Serialise("", el.ibuffer.Size);

  Serialise("", el.vbuffers);
  Serialise("", el.layouts);

  Serialise("", el.indexStripCutValue);

  SIZE_CHECK(64);
}

template <>
void Serialiser::Serialise(const char *name, D3D12Pipe::CBuffer &el)
{
  Serialise("", el.Immediate);
  Serialise("", el.RootElement);
  Serialise("", el.TableIndex);
  Serialise("", el.Buffer);
  Serialise("", el.Offset);
  Serialise("", el.ByteSize);
  Serialise("", el.RootValues);

  SIZE_CHECK(56);
}

template <>
void Serialiser::Serialise(const char *name, D3D12Pipe::Sampler &el)
{
  Serialise("", el.Immediate);
  Serialise("", el.RootElement);
  Serialise("", el.TableIndex);
  Serialise("", el.AddressU);
  Serialise("", el.AddressV);
  Serialise("", el.AddressW);
  SerialisePODArray<4>("", el.BorderColor);
  Serialise("", el.Comparison);
  Serialise("", el.Filter);
  Serialise("", el.MaxAniso);
  Serialise("", el.MaxLOD);
  Serialise("", el.MinLOD);
  Serialise("", el.MipLODBias);

  SIZE_CHECK(76);
}

template <>
void Serialiser::Serialise(const char *name, D3D12Pipe::View &el)
{
  Serialise("", el.Immediate);
  Serialise("", el.RootElement);
  Serialise("", el.TableIndex);
  Serialise("", el.Resource);
  Serialise("", el.Resource);
  Serialise("", el.Type);
  Serialise("", el.Format);

  SerialisePODArray<4>("", el.swizzle);
  Serialise("", el.BufferFlags);
  Serialise("", el.BufferStructCount);
  Serialise("", el.ElementSize);
  Serialise("", el.FirstElement);
  Serialise("", el.NumElements);

  Serialise("", el.CounterResource);
  Serialise("", el.CounterByteOffset);

  Serialise("", el.HighestMip);
  Serialise("", el.NumMipLevels);
  Serialise("", el.ArraySize);
  Serialise("", el.FirstArraySlice);

  Serialise("", el.MinLODClamp);

  SIZE_CHECK(168);
}

template <>
void Serialiser::Serialise(const char *name, D3D12Pipe::RegisterSpace &el)
{
  Serialise("", el.ConstantBuffers);
  Serialise("", el.Samplers);
  Serialise("", el.SRVs);
  Serialise("", el.UAVs);

  SIZE_CHECK(64);
}

template <>
void Serialiser::Serialise(const char *name, D3D12Pipe::Shader &el)
{
  Serialise("", el.Object);
  Serialise("", el.BindpointMapping);
  Serialise("", el.stage);
  Serialise("", el.Spaces);

  SIZE_CHECK(104);
}

template <>
void Serialiser::Serialise(const char *name, D3D12Pipe::Rasterizer &el)
{
  Serialise("", el.SampleMask);
  Serialise("", el.Scissors);
  Serialise("", el.Viewports);
  Serialise("", el.m_State);

  SIZE_CHECK(88);
}

template <>
void Serialiser::Serialise(const char *name, D3D12Pipe::Blend &el)
{
  Serialise("", el.m_Blend.Source);
  Serialise("", el.m_Blend.Destination);
  Serialise("", el.m_Blend.Operation);

  Serialise("", el.m_AlphaBlend.Source);
  Serialise("", el.m_AlphaBlend.Destination);
  Serialise("", el.m_AlphaBlend.Operation);

  Serialise("", el.Logic);

  Serialise("", el.Enabled);
  Serialise("", el.LogicEnabled);
  Serialise("", el.WriteMask);

  SIZE_CHECK(40);
}

template <>
void Serialiser::Serialise(const char *name, D3D12Pipe::OM &el)
{
  {
    Serialise("", el.m_State.DepthEnable);
    Serialise("", el.m_State.DepthWrites);
    Serialise("", el.m_State.DepthFunc);
    Serialise("", el.m_State.StencilEnable);
    Serialise("", el.m_State.StencilReadMask);
    Serialise("", el.m_State.StencilWriteMask);

    Serialise("", el.m_State.m_FrontFace.FailOp);
    Serialise("", el.m_State.m_FrontFace.DepthFailOp);
    Serialise("", el.m_State.m_FrontFace.PassOp);
    Serialise("", el.m_State.m_FrontFace.Func);

    Serialise("", el.m_State.m_BackFace.FailOp);
    Serialise("", el.m_State.m_BackFace.DepthFailOp);
    Serialise("", el.m_State.m_BackFace.PassOp);
    Serialise("", el.m_State.m_BackFace.Func);

    Serialise("", el.m_State.StencilRef);
  }

  {
    Serialise("", el.m_BlendState.AlphaToCoverage);
    Serialise("", el.m_BlendState.IndependentBlend);
    Serialise("", el.m_BlendState.Blends);
    SerialisePODArray<4>("", el.m_BlendState.BlendFactor);
  }

  Serialise("", el.RenderTargets);
  Serialise("", el.DepthTarget);
  Serialise("", el.DepthReadOnly);
  Serialise("", el.StencilReadOnly);

  Serialise("", el.multiSampleCount);
  Serialise("", el.multiSampleQuality);

  SIZE_CHECK(296);
}

template <>
void Serialiser::Serialise(const char *name, D3D12Pipe::ResourceState &el)
{
  Serialise("", el.name);

  SIZE_CHECK(16);
}

template <>
void Serialiser::Serialise(const char *name, D3D12Pipe::ResourceData &el)
{
  Serialise("", el.id);
  Serialise("", el.states);

  SIZE_CHECK(24);
}

template <>
void Serialiser::Serialise(const char *name, D3D12Pipe::State &el)
{
  Serialise("", el.pipeline);
  Serialise("", el.customName);
  Serialise("", el.name);

  Serialise("", el.rootSig);

  Serialise("", el.m_IA);

  Serialise("", el.m_VS);
  Serialise("", el.m_HS);
  Serialise("", el.m_DS);
  Serialise("", el.m_GS);
  Serialise("", el.m_PS);
  Serialise("", el.m_CS);

  Serialise("", el.m_SO.Outputs);

  Serialise("", el.m_RS);

  Serialise("", el.m_OM);

  Serialise("", el.Resources);

  SIZE_CHECK(1144);
}

#pragma endregion D3D12 pipeline state

#pragma region OpenGL pipeline state

template <>
void Serialiser::Serialise(const char *name, GLPipe::VertexAttribute &el)
{
  Serialise("", el.Enabled);
  Serialise("", el.Format);
  SerialisePODArray<4>("", el.GenericValue.value_f);
  Serialise("", el.BufferSlot);
  Serialise("", el.RelativeOffset);

  SIZE_CHECK(80);
}

template <>
void Serialiser::Serialise(const char *name, GLPipe::VertexInput &el)
{
  Serialise("", el.attributes);
  Serialise("", el.vbuffers);
  Serialise("", el.ibuffer);
  Serialise("", el.primitiveRestart);
  Serialise("", el.restartIndex);
  Serialise("", el.provokingVertexLast);

  SIZE_CHECK(56);
}

template <>
void Serialiser::Serialise(const char *name, GLPipe::Shader &el)
{
  Serialise("", el.Object);

  Serialise("", el.ShaderName);
  Serialise("", el.customShaderName);

  Serialise("", el.ProgramName);
  Serialise("", el.customProgramName);

  Serialise("", el.PipelineActive);
  Serialise("", el.PipelineName);
  Serialise("", el.customPipelineName);

  Serialise("", el.stage);
  Serialise("", el.BindpointMapping);
  Serialise("", el.Subroutines);

  if(m_Mode == READING)
    el.ShaderDetails = NULL;

  SIZE_CHECK(176);
}

template <>
void Serialiser::Serialise(const char *name, GLPipe::Sampler &el)
{
  Serialise("", el.Samp);
  Serialise("", el.AddressS);
  Serialise("", el.AddressT);
  Serialise("", el.AddressR);
  SerialisePODArray<4>("", el.BorderColor);
  Serialise("", el.Comparison);
  Serialise("", el.Filter);
  Serialise("", el.SeamlessCube);
  Serialise("", el.MaxAniso);
  Serialise("", el.MaxLOD);
  Serialise("", el.MinLOD);
  Serialise("", el.MipLODBias);

  SIZE_CHECK(80);
}

template <>
void Serialiser::Serialise(const char *name, GLPipe::ImageLoadStore &el)
{
  Serialise("", el.Resource);
  Serialise("", el.Level);
  Serialise("", el.Layered);
  Serialise("", el.Layer);
  Serialise("", el.ResType);
  Serialise("", el.readAllowed);
  Serialise("", el.writeAllowed);
  Serialise("", el.Format);

  SIZE_CHECK(80);
}

template <>
void Serialiser::Serialise(const char *name, GLPipe::Rasterizer &el)
{
  Serialise("", el.Viewports);
  Serialise("", el.Scissors);
  Serialise("", el.m_State);

  SIZE_CHECK(120);
}

template <>
void Serialiser::Serialise(const char *name, GLPipe::DepthState &el)
{
  Serialise("", el.DepthEnable);
  Serialise("", el.DepthFunc);
  Serialise("", el.DepthWrites);
  Serialise("", el.DepthBounds);
  Serialise("", el.NearBound);
  Serialise("", el.FarBound);

  SIZE_CHECK(32);
}

template <>
void Serialiser::Serialise(const char *name, GLPipe::StencilState &el)
{
  Serialise("", el.StencilEnable);

  Serialise("", el.m_FrontFace.FailOp);
  Serialise("", el.m_FrontFace.DepthFailOp);
  Serialise("", el.m_FrontFace.PassOp);
  Serialise("", el.m_FrontFace.Func);
  Serialise("", el.m_FrontFace.Ref);
  Serialise("", el.m_FrontFace.ValueMask);
  Serialise("", el.m_FrontFace.WriteMask);

  Serialise("", el.m_BackFace.FailOp);
  Serialise("", el.m_BackFace.DepthFailOp);
  Serialise("", el.m_BackFace.PassOp);
  Serialise("", el.m_BackFace.Func);
  Serialise("", el.m_BackFace.Ref);
  Serialise("", el.m_BackFace.ValueMask);
  Serialise("", el.m_BackFace.WriteMask);

  SIZE_CHECK(44);
}

template <>
void Serialiser::Serialise(const char *name, GLPipe::Blend &el)
{
  Serialise("", el.Enabled);
  Serialise("", el.WriteMask);
  Serialise("", el.Logic);

  Serialise("", el.m_Blend.Source);
  Serialise("", el.m_Blend.Destination);
  Serialise("", el.m_Blend.Operation);

  Serialise("", el.m_AlphaBlend.Source);
  Serialise("", el.m_AlphaBlend.Destination);
  Serialise("", el.m_AlphaBlend.Operation);

  SIZE_CHECK(36);
}

template <>
void Serialiser::Serialise(const char *name, GLPipe::BlendState &el)
{
  SerialisePODArray<4>("", el.BlendFactor);
  Serialise("", el.Blends);

  SIZE_CHECK(32);
}

template <>
void Serialiser::Serialise(const char *name, GLPipe::Attachment &el)
{
  Serialise("", el.Obj);
  Serialise("", el.Layer);
  Serialise("", el.Mip);
  SerialisePODArray<4>("", el.Swizzle);

  SIZE_CHECK(32);
}

template <>
void Serialiser::Serialise(const char *name, GLPipe::FrameBuffer &el)
{
  Serialise("", el.FramebufferSRGB);
  Serialise("", el.Dither);

  Serialise("", el.m_DrawFBO.Obj);
  Serialise("", el.m_DrawFBO.Color);
  Serialise("", el.m_DrawFBO.Depth);
  Serialise("", el.m_DrawFBO.Stencil);
  Serialise("", el.m_DrawFBO.DrawBuffers);
  Serialise("", el.m_DrawFBO.ReadBuffer);

  Serialise("", el.m_ReadFBO.Obj);
  Serialise("", el.m_ReadFBO.Color);
  Serialise("", el.m_ReadFBO.Depth);
  Serialise("", el.m_ReadFBO.Stencil);
  Serialise("", el.m_ReadFBO.DrawBuffers);
  Serialise("", el.m_ReadFBO.ReadBuffer);

  Serialise("", el.m_Blending);

  SIZE_CHECK(264);
}

template <>
void Serialiser::Serialise(const char *name, GLPipe::State &el)
{
  Serialise("", el.m_VtxIn);

  Serialise("", el.m_VS);
  Serialise("", el.m_TCS);
  Serialise("", el.m_TES);
  Serialise("", el.m_GS);
  Serialise("", el.m_FS);
  Serialise("", el.m_CS);

  Serialise("", el.m_VtxProcess);

  Serialise("", el.Textures);
  Serialise("", el.Samplers);
  Serialise("", el.AtomicBuffers);
  Serialise("", el.UniformBuffers);
  Serialise("", el.ShaderStorageBuffers);
  Serialise("", el.Images);

  Serialise("", el.m_Feedback);

  Serialise("", el.m_Rasterizer);
  Serialise("", el.m_DepthState);
  Serialise("", el.m_StencilState);

  Serialise("", el.m_FB);

  Serialise("", el.m_Hints);

  SIZE_CHECK(1880);
}

#pragma endregion OpenGL pipeline state

#pragma region Vulkan pipeline state

template <>
void Serialiser::Serialise(const char *name, VKPipe::BindingElement &el)
{
  Serialise("", el.view);
  Serialise("", el.res);
  Serialise("", el.sampler);
  Serialise("", el.immutableSampler);

  Serialise("", el.name);
  Serialise("", el.customName);

  Serialise("", el.viewfmt);
  SerialisePODArray<4>("", el.swizzle);
  Serialise("", el.baseMip);
  Serialise("", el.baseLayer);
  Serialise("", el.numMip);
  Serialise("", el.numLayer);

  Serialise("", el.offset);
  Serialise("", el.size);

  Serialise("", el.Filter);
  Serialise("", el.AddressU);
  Serialise("", el.AddressV);
  Serialise("", el.AddressW);
  Serialise("", el.mipBias);
  Serialise("", el.maxAniso);
  Serialise("", el.comparison);
  Serialise("", el.minlod);
  Serialise("", el.maxlod);
  SerialisePODArray<4>("", el.BorderColor);
  Serialise("", el.unnormalized);

  SIZE_CHECK(224);
};

template <>
void Serialiser::Serialise(const char *name, VKPipe::DescriptorBinding &el)
{
  Serialise("", el.descriptorCount);
  Serialise("", el.type);
  Serialise("", el.stageFlags);

  Serialise("", el.binds);

  SIZE_CHECK(32);
}

template <>
void Serialiser::Serialise(const char *name, VKPipe::DescriptorSet &el)
{
  Serialise("", el.layout);
  Serialise("", el.descset);

  Serialise("", el.bindings);

  SIZE_CHECK(32);
}

template <>
void Serialiser::Serialise(const char *name, VKPipe::Pipeline &el)
{
  Serialise("", el.obj);
  Serialise("", el.flags);

  Serialise("", el.DescSets);

  SIZE_CHECK(32);
}

template <>
void Serialiser::Serialise(const char *name, VKPipe::VertexAttribute &el)
{
  Serialise("", el.location);
  Serialise("", el.binding);
  Serialise("", el.format);
  Serialise("", el.byteoffset);

  SIZE_CHECK(64);
}

template <>
void Serialiser::Serialise(const char *name, VKPipe::VertexInput &el)
{
  Serialise("", el.attrs);
  Serialise("", el.binds);
  Serialise("", el.vbuffers);

  SIZE_CHECK(48);
}

template <>
void Serialiser::Serialise(const char *name, VKPipe::SpecInfo &el)
{
  Serialise("", el.specID);
  Serialise("", el.data);

  SIZE_CHECK(24);
}

template <>
void Serialiser::Serialise(const char *name, VKPipe::Shader &el)
{
  Serialise("", el.Object);
  Serialise("", el.entryPoint);

  Serialise("", el.name);
  Serialise("", el.customName);
  Serialise("", el.BindpointMapping);
  Serialise("", el.stage);

  if(m_Mode == READING)
    el.ShaderDetails = NULL;

  Serialise("", el.specialization);

  SIZE_CHECK(144);
}

template <>
void Serialiser::Serialise(const char *name, VKPipe::ViewState &el)
{
  Serialise("", el.viewportScissors);

  SIZE_CHECK(16);
}

template <>
void Serialiser::Serialise(const char *name, VKPipe::Blend &el)
{
  Serialise("", el.blendEnable);

  Serialise("", el.blend.Source);
  Serialise("", el.blend.Destination);
  Serialise("", el.blend.Operation);

  Serialise("", el.alphaBlend.Source);
  Serialise("", el.alphaBlend.Destination);
  Serialise("", el.alphaBlend.Operation);

  Serialise("", el.writeMask);

  SIZE_CHECK(32);
}

template <>
void Serialiser::Serialise(const char *name, VKPipe::ColorBlend &el)
{
  Serialise("", el.alphaToCoverageEnable);
  Serialise("", el.alphaToOneEnable);
  Serialise("", el.logicOpEnable);
  Serialise("", el.logic);

  Serialise("", el.attachments);

  SerialisePODArray<4>("", el.blendConst);

  SIZE_CHECK(48);
}

template <>
void Serialiser::Serialise(const char *name, VKPipe::Attachment &el)
{
  Serialise("", el.view);
  Serialise("", el.img);

  Serialise("", el.viewfmt);
  SerialisePODArray<4>("", el.swizzle);

  Serialise("", el.baseMip);
  Serialise("", el.baseLayer);
  Serialise("", el.numMip);
  Serialise("", el.numLayer);

  SIZE_CHECK(96);
}

template <>
void Serialiser::Serialise(const char *name, VKPipe::DepthStencil &el)
{
  Serialise("", el.depthTestEnable);
  Serialise("", el.depthWriteEnable);
  Serialise("", el.depthBoundsEnable);
  Serialise("", el.depthCompareOp);

  Serialise("", el.stencilTestEnable);

  Serialise("", el.front.FailOp);
  Serialise("", el.front.DepthFailOp);
  Serialise("", el.front.PassOp);
  Serialise("", el.front.Func);
  Serialise("", el.front.ref);
  Serialise("", el.front.compareMask);
  Serialise("", el.front.writeMask);

  Serialise("", el.back.FailOp);
  Serialise("", el.back.DepthFailOp);
  Serialise("", el.back.PassOp);
  Serialise("", el.back.Func);
  Serialise("", el.back.ref);
  Serialise("", el.back.compareMask);
  Serialise("", el.back.writeMask);

  Serialise("", el.minDepthBounds);
  Serialise("", el.maxDepthBounds);

  SIZE_CHECK(84);
}

template <>
void Serialiser::Serialise(const char *name, VKPipe::CurrentPass &el)
{
  Serialise("", el.renderpass.obj);
  Serialise("", el.renderpass.inputAttachments);
  Serialise("", el.renderpass.colorAttachments);
  Serialise("", el.renderpass.resolveAttachments);
  Serialise("", el.renderpass.depthstencilAttachment);

  Serialise("", el.framebuffer.obj);
  Serialise("", el.framebuffer.attachments);
  Serialise("", el.framebuffer.width);
  Serialise("", el.framebuffer.height);
  Serialise("", el.framebuffer.layers);

  Serialise("", el.renderArea);

  SIZE_CHECK(120);
}

template <>
void Serialiser::Serialise(const char *name, VKPipe::ImageLayout &el)
{
  Serialise("", el.baseMip);
  Serialise("", el.baseLayer);
  Serialise("", el.numMip);
  Serialise("", el.numLayer);
  Serialise("", el.name);

  SIZE_CHECK(32);
}

template <>
void Serialiser::Serialise(const char *name, VKPipe::ImageData &el)
{
  Serialise("", el.image);
  Serialise("", el.layouts);

  SIZE_CHECK(24);
}

template <>
void Serialiser::Serialise(const char *name, VKPipe::State &el)
{
  Serialise("", el.compute);
  Serialise("", el.graphics);

  Serialise("", el.IA);
  Serialise("", el.VI);

  Serialise("", el.m_VS);
  Serialise("", el.m_TCS);
  Serialise("", el.m_TES);
  Serialise("", el.m_GS);
  Serialise("", el.m_FS);
  Serialise("", el.m_CS);

  Serialise("", el.Tess);

  Serialise("", el.VP);
  Serialise("", el.RS);
  Serialise("", el.MSAA);
  Serialise("", el.CB);
  Serialise("", el.DS);
  Serialise("", el.Pass);

  Serialise("", el.images);

  SIZE_CHECK(1352);
}

#pragma endregion Vulkan pipeline state

#pragma region Data descriptors

template <>
void Serialiser::Serialise(const char *name, TextureDescription &el)
{
  Serialise("", el.name);
  Serialise("", el.customName);
  Serialise("", el.format);
  Serialise("", el.dimension);
  Serialise("", el.resType);
  Serialise("", el.width);
  Serialise("", el.height);
  Serialise("", el.depth);
  Serialise("", el.ID);
  Serialise("", el.cubemap);
  Serialise("", el.mips);
  Serialise("", el.arraysize);
  Serialise("", el.creationFlags);
  Serialise("", el.msQual);
  Serialise("", el.msSamp);
  Serialise("", el.byteSize);

  SIZE_CHECK(136);
}

template <>
void Serialiser::Serialise(const char *name, BufferDescription &el)
{
  Serialise("", el.ID);
  Serialise("", el.name);
  Serialise("", el.customName);
  Serialise("", el.creationFlags);
  Serialise("", el.length);

  SIZE_CHECK(40);
}

template <>
void Serialiser::Serialise(const char *name, APIProperties &el)
{
  Serialise("", el.pipelineType);
  Serialise("", el.localRenderer);
  Serialise("", el.degraded);

  SIZE_CHECK(12);
}

template <>
void Serialiser::Serialise(const char *name, DebugMessage &el)
{
  Serialise("", el.eventID);
  Serialise("", el.category);
  Serialise("", el.severity);
  Serialise("", el.source);
  Serialise("", el.messageID);
  Serialise("", el.description);

  SIZE_CHECK(40);
}

template <>
void Serialiser::Serialise(const char *name, APIEvent &el)
{
  Serialise("", el.eventID);
  Serialise("", el.callstack);
  Serialise("", el.eventDesc);
  Serialise("", el.fileOffset);

  SIZE_CHECK(48);
}

template <>
void Serialiser::Serialise(const char *name, DrawcallDescription &el)
{
  Serialise("", el.eventID);
  Serialise("", el.drawcallID);

  Serialise("", el.name);

  Serialise("", el.flags);

  SerialisePODArray<4>("", el.markerColor);

  Serialise("", el.numIndices);
  Serialise("", el.numInstances);
  Serialise("", el.baseVertex);
  Serialise("", el.indexOffset);
  Serialise("", el.vertexOffset);
  Serialise("", el.instanceOffset);

  SerialisePODArray<3>("", el.dispatchDimension);
  SerialisePODArray<3>("", el.dispatchThreadsDimension);

  Serialise("", el.indexByteWidth);
  Serialise("", el.topology);

  Serialise("", el.copySource);
  Serialise("", el.copyDestination);

  Serialise("", el.parent);
  Serialise("", el.previous);
  Serialise("", el.next);

  SerialisePODArray<8>("", el.outputs);
  Serialise("", el.depthOut);

  Serialise("", el.events);
  Serialise("", el.children);

  SIZE_CHECK(248);
}

template <>
void Serialiser::Serialise(const char *name, ConstantBindStats &el)
{
  Serialise("", el.calls);
  Serialise("", el.sets);
  Serialise("", el.nulls);
  Serialise("", el.bindslots);
  Serialise("", el.sizes);

  SIZE_CHECK(48);
}

template <>
void Serialiser::Serialise(const char *name, SamplerBindStats &el)
{
  Serialise("", el.calls);
  Serialise("", el.sets);
  Serialise("", el.nulls);
  Serialise("", el.bindslots);

  SIZE_CHECK(32);
}

template <>
void Serialiser::Serialise(const char *name, ResourceBindStats &el)
{
  Serialise("", el.calls);
  Serialise("", el.sets);
  Serialise("", el.nulls);
  Serialise("", el.types);
  Serialise("", el.bindslots);

  SIZE_CHECK(48);
}

template <>
void Serialiser::Serialise(const char *name, ResourceUpdateStats &el)
{
  Serialise("", el.calls);
  Serialise("", el.clients);
  Serialise("", el.servers);
  Serialise("", el.types);
  Serialise("", el.sizes);

  SIZE_CHECK(48);
}

template <>
void Serialiser::Serialise(const char *name, DrawcallStats &el)
{
  Serialise("", el.calls);
  Serialise("", el.instanced);
  Serialise("", el.indirect);
  Serialise("", el.counts);

  SIZE_CHECK(32);
}

template <>
void Serialiser::Serialise(const char *name, DispatchStats &el)
{
  Serialise("", el.calls);
  Serialise("", el.indirect);

  SIZE_CHECK(8);
}

template <>
void Serialiser::Serialise(const char *name, IndexBindStats &el)
{
  Serialise("", el.calls);
  Serialise("", el.sets);
  Serialise("", el.nulls);

  SIZE_CHECK(12);
}

template <>
void Serialiser::Serialise(const char *name, VertexBindStats &el)
{
  Serialise("", el.calls);
  Serialise("", el.sets);
  Serialise("", el.nulls);
  Serialise("", el.bindslots);

  SIZE_CHECK(32);
}

template <>
void Serialiser::Serialise(const char *name, LayoutBindStats &el)
{
  Serialise("", el.calls);
  Serialise("", el.sets);
  Serialise("", el.nulls);

  SIZE_CHECK(12);
}

template <>
void Serialiser::Serialise(const char *name, ShaderChangeStats &el)
{
  Serialise("", el.calls);
  Serialise("", el.sets);
  Serialise("", el.nulls);
  Serialise("", el.redundants);

  SIZE_CHECK(16);
}

template <>
void Serialiser::Serialise(const char *name, BlendStats &el)
{
  Serialise("", el.calls);
  Serialise("", el.sets);
  Serialise("", el.nulls);
  Serialise("", el.redundants);

  SIZE_CHECK(16);
}

template <>
void Serialiser::Serialise(const char *name, DepthStencilStats &el)
{
  Serialise("", el.calls);
  Serialise("", el.sets);
  Serialise("", el.nulls);
  Serialise("", el.redundants);

  SIZE_CHECK(16);
}

template <>
void Serialiser::Serialise(const char *name, RasterizationStats &el)
{
  Serialise("", el.calls);
  Serialise("", el.sets);
  Serialise("", el.nulls);
  Serialise("", el.redundants);
  Serialise("", el.viewports);
  Serialise("", el.rects);

  SIZE_CHECK(48);
}

template <>
void Serialiser::Serialise(const char *name, OutputTargetStats &el)
{
  Serialise("", el.calls);
  Serialise("", el.sets);
  Serialise("", el.nulls);
  Serialise("", el.bindslots);

  SIZE_CHECK(32);
}

template <>
void Serialiser::Serialise(const char *name, FrameStatistics &el)
{
  Serialise("", el.recorded);
  // #mivance note this is technically error-prone from the perspective
  // that we're passing references to pointers, but as we're really
  // dealing with arrays,t hey'll never be NULL and need to be assigned
  // to, so this is fine
  ConstantBindStats *constants = el.constants;
  SerialiseComplexArray<(uint32_t)ShaderStage::Count>("", constants);
  SamplerBindStats *samplers = el.samplers;
  SerialiseComplexArray<(uint32_t)ShaderStage::Count>("", samplers);
  ResourceBindStats *resources = el.resources;
  SerialiseComplexArray<(uint32_t)ShaderStage::Count>("", resources);
  Serialise("", el.updates);
  Serialise("", el.draws);
  Serialise("", el.dispatches);
  Serialise("", el.indices);
  Serialise("", el.vertices);
  Serialise("", el.layouts);
  ShaderChangeStats *shaders = el.shaders;
  SerialiseComplexArray<(uint32_t)ShaderStage::Count>("", shaders);
  Serialise("", el.blends);
  Serialise("", el.depths);
  Serialise("", el.rasters);
  Serialise("", el.outputs);

  SIZE_CHECK(1136);
}

template <>
void Serialiser::Serialise(const char *name, FrameDescription &el)
{
  Serialise("", el.frameNumber);
  Serialise("", el.fileOffset);
  Serialise("", el.uncompressedFileSize);
  Serialise("", el.compressedFileSize);
  Serialise("", el.persistentSize);
  Serialise("", el.initDataSize);
  Serialise("", el.captureTime);
  Serialise("", el.stats);
  Serialise("", el.debugMessages);

  SIZE_CHECK(1208);
}

template <>
void Serialiser::Serialise(const char *name, FrameRecord &el)
{
  Serialise("", el.frameInfo);
  Serialise("", el.drawcallList);

  SIZE_CHECK(1224);
}

template <>
void Serialiser::Serialise(const char *name, MeshFormat &el)
{
  Serialise("", el.idxbuf);
  Serialise("", el.idxoffs);
  Serialise("", el.idxByteWidth);
  Serialise("", el.baseVertex);
  Serialise("", el.buf);
  Serialise("", el.offset);
  Serialise("", el.stride);
  Serialise("", el.compCount);
  Serialise("", el.compByteWidth);
  Serialise("", el.compType);
  Serialise("", el.bgraOrder);
  Serialise("", el.specialFormat);
  Serialise("", el.meshColor);
  Serialise("", el.showAlpha);
  Serialise("", el.topo);
  Serialise("", el.numVerts);
  Serialise("", el.unproject);
  Serialise("", el.nearPlane);
  Serialise("", el.farPlane);

  SIZE_CHECK(104);
}

template <>
void Serialiser::Serialise(const char *name, CounterDescription &el)
{
  Serialise("", el.counterID);
  Serialise("", el.name);
  Serialise("", el.description);
  Serialise("", el.resultType);
  Serialise("", el.resultByteWidth);
  Serialise("", el.unit);

  SIZE_CHECK(56);
}

template <>
void Serialiser::Serialise(const char *name, PixelModification &el)
{
  Serialise("", el.eventID);

  Serialise("", el.directShaderWrite);
  Serialise("", el.unboundPS);

  Serialise("", el.fragIndex);
  Serialise("", el.primitiveID);

  SerialisePODArray<4>("", el.preMod.col.value_u);
  Serialise("", el.preMod.depth);
  Serialise("", el.preMod.stencil);
  SerialisePODArray<4>("", el.shaderOut.col.value_u);
  Serialise("", el.shaderOut.depth);
  Serialise("", el.shaderOut.stencil);
  SerialisePODArray<4>("", el.postMod.col.value_u);
  Serialise("", el.postMod.depth);
  Serialise("", el.postMod.stencil);

  Serialise("", el.sampleMasked);
  Serialise("", el.backfaceCulled);
  Serialise("", el.depthClipped);
  Serialise("", el.viewClipped);
  Serialise("", el.scissorClipped);
  Serialise("", el.shaderDiscarded);
  Serialise("", el.depthTestFailed);
  Serialise("", el.stencilTestFailed);

  SIZE_CHECK(124);
}

#pragma endregion Data descriptors

#pragma region Ignored Enums

// don't need string representation of these enums
template <>
string ToStrHelper<false, SpecialFormat>::Get(const SpecialFormat &el)
{
  return "<...>";
}
template <>
string ToStrHelper<false, CompType>::Get(const CompType &el)
{
  return "<...>";
}
template <>
string ToStrHelper<false, ShaderEvents>::Get(const ShaderEvents &el)
{
  return "<...>";
}
template <>
string ToStrHelper<false, TextureCategory>::Get(const TextureCategory &el)
{
  return "<...>";
}
template <>
string ToStrHelper<false, BufferCategory>::Get(const BufferCategory &el)
{
  return "<...>";
}
template <>
string ToStrHelper<false, D3DBufferViewFlags>::Get(const D3DBufferViewFlags &el)
{
  return "<...>";
}
template <>
string ToStrHelper<false, DrawFlags>::Get(const DrawFlags &el)
{
  return "<...>";
}
template <>
string ToStrHelper<false, GPUCounter>::Get(const GPUCounter &el)
{
  return "<...>";
}
template <>
string ToStrHelper<false, TextureSwizzle>::Get(const TextureSwizzle &el)
{
  return "<...>";
}
template <>
string ToStrHelper<false, AddressMode>::Get(const AddressMode &el)
{
  return "<...>";
}
template <>
string ToStrHelper<false, CompareFunc>::Get(const CompareFunc &el)
{
  return "<...>";
}
template <>
string ToStrHelper<false, BlendMultiplier>::Get(const BlendMultiplier &el)
{
  return "<...>";
}
template <>
string ToStrHelper<false, BlendOp>::Get(const BlendOp &el)
{
  return "<...>";
}
template <>
string ToStrHelper<false, LogicOp>::Get(const LogicOp &el)
{
  return "<...>";
}
template <>
string ToStrHelper<false, StencilOp>::Get(const StencilOp &el)
{
  return "<...>";
}
template <>
string ToStrHelper<false, CounterUnit>::Get(const CounterUnit &el)
{
  return "<...>";
}
template <>
string ToStrHelper<false, Topology>::Get(const Topology &el)
{
  return "<...>";
}
template <>
string ToStrHelper<false, ShaderStage>::Get(const ShaderStage &el)
{
  return "<...>";
}
template <>
string ToStrHelper<false, ShaderStageMask>::Get(const ShaderStageMask &el)
{
  return "<...>";
}
template <>
string ToStrHelper<false, BindType>::Get(const BindType &el)
{
  return "<...>";
}
template <>
string ToStrHelper<false, TextureDim>::Get(const TextureDim &el)
{
  return "<...>";
}
template <>
string ToStrHelper<false, MessageCategory>::Get(const MessageCategory &el)
{
  return "<...>";
}
template <>
string ToStrHelper<false, MessageSeverity>::Get(const MessageSeverity &el)
{
  return "<...>";
}
template <>
string ToStrHelper<false, MessageSource>::Get(const MessageSource &el)
{
  return "<...>";
}
template <>
string ToStrHelper<false, VarType>::Get(const VarType &el)
{
  return "<...>";
}
template <>
string ToStrHelper<false, MeshDataStage>::Get(const MeshDataStage &el)
{
  return "<...>";
}
template <>
string ToStrHelper<false, DebugOverlay>::Get(const DebugOverlay &el)
{
  return "<...>";
}
template <>
string ToStrHelper<false, GraphicsAPI>::Get(const GraphicsAPI &el)
{
  return "<...>";
}

#pragma endregion Ignored Enums

#pragma region Plain - old data structures

// these structures we can just serialise as a blob, since they're POD.
template <>
string ToStrHelper<false, D3D11Pipe::VB>::Get(const D3D11Pipe::VB &el)
{
  return "<...>";
}
template <>
string ToStrHelper<false, D3D11Pipe::RasterizerState>::Get(const D3D11Pipe::RasterizerState &el)
{
  return "<...>";
}
template <>
string ToStrHelper<false, D3D11Pipe::CBuffer>::Get(const D3D11Pipe::CBuffer &el)
{
  return "<...>";
}
template <>
string ToStrHelper<false, D3D11Pipe::Scissor>::Get(const D3D11Pipe::Scissor &el)
{
  return "<...>";
}
template <>
string ToStrHelper<false, D3D11Pipe::Viewport>::Get(const D3D11Pipe::Viewport &el)
{
  return "<...>";
}
template <>
string ToStrHelper<false, D3D11Pipe::SOBind>::Get(const D3D11Pipe::SOBind &el)
{
  return "<...>";
}
template <>
string ToStrHelper<false, D3D12Pipe::VB>::Get(const D3D12Pipe::VB &el)
{
  return "<...>";
}
template <>
string ToStrHelper<false, D3D12Pipe::SOBind>::Get(const D3D12Pipe::SOBind &el)
{
  return "<...>";
}
template <>
string ToStrHelper<false, D3D12Pipe::Scissor>::Get(const D3D12Pipe::Scissor &el)
{
  return "<...>";
}
template <>
string ToStrHelper<false, D3D12Pipe::Viewport>::Get(const D3D12Pipe::Viewport &el)
{
  return "<...>";
}
template <>
string ToStrHelper<false, D3D12Pipe::RasterizerState>::Get(const D3D12Pipe::RasterizerState &el)
{
  return "<...>";
}
template <>
string ToStrHelper<false, GLPipe::VB>::Get(const GLPipe::VB &el)
{
  return "<...>";
}
template <>
string ToStrHelper<false, GLPipe::FixedVertexProcessing>::Get(const GLPipe::FixedVertexProcessing &el)
{
  return "<...>";
}
template <>
string ToStrHelper<false, GLPipe::Texture>::Get(const GLPipe::Texture &el)
{
  return "<...>";
}
template <>
string ToStrHelper<false, GLPipe::Buffer>::Get(const GLPipe::Buffer &el)
{
  return "<...>";
}
template <>
string ToStrHelper<false, GLPipe::Feedback>::Get(const GLPipe::Feedback &el)
{
  return "<...>";
}
template <>
string ToStrHelper<false, GLPipe::Viewport>::Get(const GLPipe::Viewport &el)
{
  return "<...>";
}
template <>
string ToStrHelper<false, GLPipe::Scissor>::Get(const GLPipe::Scissor &el)
{
  return "<...>";
}
template <>
string ToStrHelper<false, GLPipe::RasterizerState>::Get(const GLPipe::RasterizerState &el)
{
  return "<...>";
}
template <>
string ToStrHelper<false, GLPipe::Hints>::Get(const GLPipe::Hints &el)
{
  return "<...>";
}
template <>
string ToStrHelper<false, VKPipe::RenderArea>::Get(const VKPipe::RenderArea &el)
{
  return "<...>";
}
template <>
string ToStrHelper<false, VKPipe::InputAssembly>::Get(const VKPipe::InputAssembly &el)
{
  return "<...>";
}
template <>
string ToStrHelper<false, VKPipe::Tessellation>::Get(const VKPipe::Tessellation &el)
{
  return "<...>";
}
template <>
string ToStrHelper<false, VKPipe::Raster>::Get(const VKPipe::Raster &el)
{
  return "<...>";
}
template <>
string ToStrHelper<false, VKPipe::MultiSample>::Get(const VKPipe::MultiSample &el)
{
  return "<...>";
}
template <>
string ToStrHelper<false, VKPipe::BindingElement>::Get(const VKPipe::BindingElement &el)
{
  return "<...>";
}
template <>
string ToStrHelper<false, VKPipe::VertexBinding>::Get(const VKPipe::VertexBinding &el)
{
  return "<...>";
}
template <>
string ToStrHelper<false, VKPipe::VB>::Get(const VKPipe::VB &el)
{
  return "<...>";
}
template <>
string ToStrHelper<false, VKPipe::ViewportScissor>::Get(const VKPipe::ViewportScissor &el)
{
  return "<...>";
}
template <>
string ToStrHelper<false, EventUsage>::Get(const EventUsage &el)
{
  return "<...>";
}
template <>
string ToStrHelper<false, CounterResult>::Get(const CounterResult &el)
{
  return "<...>";
}
template <>
string ToStrHelper<false, ReplayLogType>::Get(const ReplayLogType &el)
{
  return "<...>";
}
template <>
string ToStrHelper<false, FloatVector>::Get(const FloatVector &el)
{
  return "<...>";
}
template <>
string ToStrHelper<false, TextureFilter>::Get(const TextureFilter &el)
{
  return "<...>";
}

#pragma endregion Plain - old data structures

ReplayProxy::~ReplayProxy()
{
  SAFE_DELETE(m_FromReplaySerialiser);
  m_ToReplaySerialiser = NULL;    // we don't own this

  if(m_Proxy)
    m_Proxy->Shutdown();
  m_Proxy = NULL;

  for(auto it = m_ShaderReflectionCache.begin(); it != m_ShaderReflectionCache.end(); ++it)
    delete it->second;
}

bool ReplayProxy::SendReplayCommand(ReplayProxyPacket type)
{
  if(!m_Socket->Connected())
    return false;

  if(!SendPacket(m_Socket, type, *m_ToReplaySerialiser))
    return false;

  m_ToReplaySerialiser->Rewind();

  SAFE_DELETE(m_FromReplaySerialiser);

  if(!RecvPacket(m_Socket, type, &m_FromReplaySerialiser))
    return false;

  return true;
}

template <>
string ToStrHelper<false, RemapTextureEnum>::Get(const RemapTextureEnum &el)
{
  switch(el)
  {
    TOSTR_CASE_STRINGIZE(eRemap_None)
    TOSTR_CASE_STRINGIZE(eRemap_RGBA8)
    TOSTR_CASE_STRINGIZE(eRemap_RGBA16)
    TOSTR_CASE_STRINGIZE(eRemap_RGBA32)
    TOSTR_CASE_STRINGIZE(eRemap_D32S8)
    default: break;
  }

  return StringFormat::Fmt("RemapTextureEnum<%d>", el);
}

// If a remap is required, modify the params that are used when getting the proxy texture data
// for replay on the current driver.
void ReplayProxy::RemapProxyTextureIfNeeded(ResourceFormat &format, GetTextureDataParams &params)
{
  if(m_Proxy->IsTextureSupported(format))
    return;

  if(format.special)
  {
    switch(format.specialFormat)
    {
      case SpecialFormat::S8:
      case SpecialFormat::D16S8: params.remap = eRemap_D32S8; break;
      case SpecialFormat::ASTC:
      case SpecialFormat::EAC:
      case SpecialFormat::R5G6B5:
      case SpecialFormat::ETC2: params.remap = eRemap_RGBA8; break;
      default:
        RDCERR("Don't know how to remap special format %u, falling back to RGBA32",
               format.specialFormat);
        params.remap = eRemap_RGBA32;
        break;
    }
    format.special = false;
  }
  else
  {
    if(format.compByteWidth == 4)
      params.remap = eRemap_RGBA32;
    else if(format.compByteWidth == 2)
      params.remap = eRemap_RGBA16;
    else if(format.compByteWidth == 1)
      params.remap = eRemap_RGBA8;
  }

  switch(params.remap)
  {
    case eRemap_None: RDCERR("IsTextureSupported == false, but we have no remap"); break;
    case eRemap_RGBA8:
      format.compCount = 4;
      format.compByteWidth = 1;
      format.compType = CompType::UNorm;
      // Range adaptation is only needed when remapping a higher precision format down to RGBA8.
      params.whitePoint = 1.0f;
      break;
    case eRemap_RGBA16:
      format.compCount = 4;
      format.compByteWidth = 2;
      format.compType = CompType::UNorm;
      break;
    case eRemap_RGBA32:
      format.compCount = 4;
      format.compByteWidth = 4;
      format.compType = CompType::Float;
      break;
    case eRemap_D32S8: RDCERR("Remapping depth/stencil formats not implemented."); break;
  }
}

void ReplayProxy::EnsureTexCached(ResourceId texid, uint32_t arrayIdx, uint32_t mip)
{
  if(!m_Socket->Connected())
    return;

  TextureCacheEntry entry = {texid, arrayIdx, mip};

  if(m_LocalTextures.find(texid) != m_LocalTextures.end())
    return;

  if(m_TextureProxyCache.find(entry) == m_TextureProxyCache.end())
  {
    if(m_ProxyTextures.find(texid) == m_ProxyTextures.end())
    {
      TextureDescription tex = GetTexture(texid);

      ProxyTextureProperties proxy;
      RemapProxyTextureIfNeeded(tex.format, proxy.params);

      proxy.id = m_Proxy->CreateProxyTexture(tex);
      m_ProxyTextures[texid] = proxy;
    }

    const ProxyTextureProperties &proxy = m_ProxyTextures[texid];

    size_t size;
    byte *data = GetTextureData(texid, arrayIdx, mip, proxy.params, size);

    if(data)
      m_Proxy->SetProxyTextureData(proxy.id, arrayIdx, mip, data, size);

    delete[] data;

    m_TextureProxyCache.insert(entry);
  }
}

void ReplayProxy::EnsureBufCached(ResourceId bufid)
{
  if(!m_Socket->Connected())
    return;

  if(m_BufferProxyCache.find(bufid) == m_BufferProxyCache.end())
  {
    if(m_ProxyBufferIds.find(bufid) == m_ProxyBufferIds.end())
    {
      BufferDescription buf = GetBuffer(bufid);
      m_ProxyBufferIds[bufid] = m_Proxy->CreateProxyBuffer(buf);
    }

    ResourceId proxyid = m_ProxyBufferIds[bufid];

    vector<byte> data;
    GetBufferData(bufid, 0, 0, data);

    if(!data.empty())
      m_Proxy->SetProxyBufferData(proxyid, &data[0], data.size());

    m_BufferProxyCache.insert(bufid);
  }
}

bool ReplayProxy::Tick(int type, Serialiser *incomingPacket)
{
  if(!m_RemoteServer)
    return true;

  if(!m_Socket || !m_Socket->Connected())
    return false;

  m_ToReplaySerialiser = incomingPacket;

  m_FromReplaySerialiser->Rewind();

  switch(type)
  {
    case eReplayProxy_ReplayLog: ReplayLog(0, (ReplayLogType)0); break;
    case eReplayProxy_GetPassEvents: GetPassEvents(0); break;
    case eReplayProxy_GetAPIProperties: GetAPIProperties(); break;
    case eReplayProxy_GetTextures: GetTextures(); break;
    case eReplayProxy_GetTexture: GetTexture(ResourceId()); break;
    case eReplayProxy_GetBuffers: GetBuffers(); break;
    case eReplayProxy_GetBuffer: GetBuffer(ResourceId()); break;
    case eReplayProxy_GetShader: GetShader(ResourceId(), ""); break;
    case eReplayProxy_GetDebugMessages: GetDebugMessages(); break;
    case eReplayProxy_SavePipelineState: SavePipelineState(); break;
    case eReplayProxy_GetUsage: GetUsage(ResourceId()); break;
    case eReplayProxy_GetLiveID: GetLiveID(ResourceId()); break;
    case eReplayProxy_GetFrameRecord: GetFrameRecord(); break;
    case eReplayProxy_IsRenderOutput: IsRenderOutput(ResourceId()); break;
    case eReplayProxy_HasResolver: HasCallstacks(); break;
    case eReplayProxy_InitStackResolver: InitCallstackResolver(); break;
    case eReplayProxy_HasStackResolver: GetCallstackResolver(); break;
    case eReplayProxy_GetAddressDetails: GetAddr(0); break;
    case eReplayProxy_FreeResource: FreeTargetResource(ResourceId()); break;
    case eReplayProxy_FetchCounters:
    {
      vector<GPUCounter> counters;
      FetchCounters(counters);
      break;
    }
    case eReplayProxy_EnumerateCounters: EnumerateCounters(); break;
    case eReplayProxy_DescribeCounter:
    {
      CounterDescription desc;
      DescribeCounter(GPUCounter::EventGPUDuration, desc);
      break;
    }
    case eReplayProxy_FillCBufferVariables:
    {
      vector<ShaderVariable> vars;
      vector<byte> data;
      FillCBufferVariables(ResourceId(), "", 0, vars, data);
      break;
    }
    case eReplayProxy_GetBufferData:
    {
      vector<byte> dummy;
      GetBufferData(ResourceId(), 0, 0, dummy);
      break;
    }
    case eReplayProxy_GetTextureData:
    {
      size_t dummy;
      GetTextureData(ResourceId(), 0, 0, GetTextureDataParams(), dummy);
      break;
    }
    case eReplayProxy_InitPostVS: InitPostVSBuffers(0); break;
    case eReplayProxy_InitPostVSVec:
    {
      vector<uint32_t> dummy;
      InitPostVSBuffers(dummy);
      break;
    }
    case eReplayProxy_GetPostVS: GetPostVSBuffers(0, 0, MeshDataStage::Unknown); break;
    case eReplayProxy_BuildTargetShader:
      BuildTargetShader("", "", 0, ShaderStage::Vertex, NULL, NULL);
      break;
    case eReplayProxy_ReplaceResource: ReplaceResource(ResourceId(), ResourceId()); break;
    case eReplayProxy_RemoveReplacement: RemoveReplacement(ResourceId()); break;
    case eReplayProxy_RenderOverlay:
      RenderOverlay(ResourceId(), CompType::Typeless, DebugOverlay::NoOverlay, 0, vector<uint32_t>());
      break;
    case eReplayProxy_PixelHistory:
      PixelHistory(vector<EventUsage>(), ResourceId(), 0, 0, 0, 0, 0, CompType::Typeless);
      break;
    case eReplayProxy_DebugVertex: DebugVertex(0, 0, 0, 0, 0, 0); break;
    case eReplayProxy_DebugPixel: DebugPixel(0, 0, 0, 0, 0); break;
    case eReplayProxy_DebugThread:
    {
      uint32_t dummy1[3] = {0};
      uint32_t dummy2[3] = {0};
      DebugThread(0, dummy1, dummy2);
      break;
    }
    case eReplayProxy_DisassembleShader: DisassembleShader(NULL, ""); break;
    case eReplayProxy_GetISATargets: GetDisassemblyTargets(); break;
    default: RDCERR("Unexpected command"); return false;
  }

  if(!SendPacket(m_Socket, type, *m_FromReplaySerialiser))
    return false;

  return true;
}

bool ReplayProxy::IsRenderOutput(ResourceId id)
{
  bool ret = false;

  m_ToReplaySerialiser->Serialise("", id);

  if(m_RemoteServer)
  {
    ret = m_Remote->IsRenderOutput(id);
  }
  else
  {
    if(!SendReplayCommand(eReplayProxy_IsRenderOutput))
      return ret;
  }

  m_FromReplaySerialiser->Serialise("", ret);

  return ret;
}

APIProperties ReplayProxy::GetAPIProperties()
{
  APIProperties ret;
  RDCEraseEl(ret);

  if(m_RemoteServer)
  {
    ret = m_Remote->GetAPIProperties();
  }
  else
  {
    if(!SendReplayCommand(eReplayProxy_GetAPIProperties))
      return ret;
  }

  m_FromReplaySerialiser->Serialise("", ret);

  if(!m_RemoteServer)
    ret.localRenderer = m_Proxy->GetAPIProperties().localRenderer;

  m_APIProps = ret;

  return ret;
}

vector<ResourceId> ReplayProxy::GetTextures()
{
  vector<ResourceId> ret;

  if(m_RemoteServer)
  {
    ret = m_Remote->GetTextures();
  }
  else
  {
    if(!SendReplayCommand(eReplayProxy_GetTextures))
      return ret;
  }

  m_FromReplaySerialiser->Serialise("", ret);

  return ret;
}

vector<DebugMessage> ReplayProxy::GetDebugMessages()
{
  vector<DebugMessage> ret;

  if(m_RemoteServer)
  {
    ret = m_Remote->GetDebugMessages();
  }
  else
  {
    if(!SendReplayCommand(eReplayProxy_GetDebugMessages))
      return ret;
  }

  m_FromReplaySerialiser->Serialise("", ret);

  return ret;
}

TextureDescription ReplayProxy::GetTexture(ResourceId id)
{
  TextureDescription ret = {};

  m_ToReplaySerialiser->Serialise("", id);

  if(m_RemoteServer)
  {
    ret = m_Remote->GetTexture(id);
  }
  else
  {
    if(!SendReplayCommand(eReplayProxy_GetTexture))
      return ret;
  }

  m_FromReplaySerialiser->Serialise("", ret);

  return ret;
}

vector<ResourceId> ReplayProxy::GetBuffers()
{
  vector<ResourceId> ret;

  if(m_RemoteServer)
  {
    ret = m_Remote->GetBuffers();
  }
  else
  {
    if(!SendReplayCommand(eReplayProxy_GetBuffers))
      return ret;
  }

  m_FromReplaySerialiser->Serialise("", ret);

  return ret;
}

BufferDescription ReplayProxy::GetBuffer(ResourceId id)
{
  BufferDescription ret = {};

  m_ToReplaySerialiser->Serialise("", id);

  if(m_RemoteServer)
  {
    ret = m_Remote->GetBuffer(id);
  }
  else
  {
    if(!SendReplayCommand(eReplayProxy_GetBuffer))
      return ret;
  }

  m_FromReplaySerialiser->Serialise("", ret);

  return ret;
}

void ReplayProxy::SavePipelineState()
{
  if(m_RemoteServer)
  {
    m_Remote->SavePipelineState();
    m_D3D11PipelineState = m_Remote->GetD3D11PipelineState();
    m_D3D12PipelineState = m_Remote->GetD3D12PipelineState();
    m_GLPipelineState = m_Remote->GetGLPipelineState();
    m_VulkanPipelineState = m_Remote->GetVulkanPipelineState();
  }
  else
  {
    if(!SendReplayCommand(eReplayProxy_SavePipelineState))
      return;

    m_D3D11PipelineState = D3D11Pipe::State();
    m_D3D12PipelineState = D3D12Pipe::State();
    m_GLPipelineState = GLPipe::State();
    m_VulkanPipelineState = VKPipe::State();
  }

  m_FromReplaySerialiser->Serialise("", m_D3D11PipelineState);
  m_FromReplaySerialiser->Serialise("", m_D3D12PipelineState);
  m_FromReplaySerialiser->Serialise("", m_GLPipelineState);
  m_FromReplaySerialiser->Serialise("", m_VulkanPipelineState);
}

void ReplayProxy::ReplayLog(uint32_t endEventID, ReplayLogType replayType)
{
  m_ToReplaySerialiser->Serialise("", endEventID);
  m_ToReplaySerialiser->Serialise("", replayType);

  if(m_RemoteServer)
  {
    m_Remote->ReplayLog(endEventID, replayType);
  }
  else
  {
    if(!SendReplayCommand(eReplayProxy_ReplayLog))
      return;

    m_TextureProxyCache.clear();
    m_BufferProxyCache.clear();
  }
}

vector<uint32_t> ReplayProxy::GetPassEvents(uint32_t eventID)
{
  vector<uint32_t> ret;

  m_ToReplaySerialiser->Serialise("", eventID);

  if(m_RemoteServer)
  {
    ret = m_Remote->GetPassEvents(eventID);
  }
  else
  {
    if(!SendReplayCommand(eReplayProxy_GetPassEvents))
      return ret;
  }

  m_FromReplaySerialiser->Serialise("", ret);

  return ret;
}

vector<EventUsage> ReplayProxy::GetUsage(ResourceId id)
{
  vector<EventUsage> ret;

  m_ToReplaySerialiser->Serialise("", id);

  if(m_RemoteServer)
  {
    ret = m_Remote->GetUsage(id);
  }
  else
  {
    if(!SendReplayCommand(eReplayProxy_GetUsage))
      return ret;
  }

  m_FromReplaySerialiser->Serialise("", ret);

  return ret;
}

FrameRecord ReplayProxy::GetFrameRecord()
{
  FrameRecord ret;

  if(m_RemoteServer)
  {
    ret = m_Remote->GetFrameRecord();
  }
  else
  {
    if(!SendReplayCommand(eReplayProxy_GetFrameRecord))
      return ret;
  }

  m_FromReplaySerialiser->Serialise("", ret);

  return ret;
}

bool ReplayProxy::HasCallstacks()
{
  bool ret = false;

  RDCASSERT(m_RemoteServer || m_ToReplaySerialiser->GetSize() == 0);

  if(m_RemoteServer)
  {
    ret = m_Remote->HasCallstacks();
  }
  else
  {
    if(!SendReplayCommand(eReplayProxy_HasResolver))
      return ret;
  }

  RDCASSERT(!m_RemoteServer || m_FromReplaySerialiser->GetSize() == 0);

  m_FromReplaySerialiser->Serialise("", ret);

  return ret;
}

ResourceId ReplayProxy::GetLiveID(ResourceId id)
{
  if(!m_RemoteServer && m_LiveIDs.find(id) != m_LiveIDs.end())
    return m_LiveIDs[id];

  if(!m_RemoteServer && m_LocalTextures.find(id) != m_LocalTextures.end())
    return id;

  if(!m_Socket->Connected())
    return ResourceId();

  ResourceId ret;

  RDCASSERT(m_RemoteServer || m_ToReplaySerialiser->GetSize() == 0);

  m_ToReplaySerialiser->Serialise("", id);

  if(m_RemoteServer)
  {
    ret = m_Remote->GetLiveID(id);
  }
  else
  {
    if(!SendReplayCommand(eReplayProxy_GetLiveID))
      return ret;
  }

  RDCASSERT(!m_RemoteServer || m_FromReplaySerialiser->GetSize() == 0);

  m_FromReplaySerialiser->Serialise("", ret);

  if(!m_RemoteServer)
    m_LiveIDs[id] = ret;

  return ret;
}

vector<CounterResult> ReplayProxy::FetchCounters(const vector<GPUCounter> &counters)
{
  vector<CounterResult> ret;

  m_ToReplaySerialiser->Serialise("", (vector<GPUCounter> &)counters);

  if(m_RemoteServer)
  {
    ret = m_Remote->FetchCounters(counters);
  }
  else
  {
    if(!SendReplayCommand(eReplayProxy_FetchCounters))
      return ret;
  }

  m_FromReplaySerialiser->Serialise("", ret);

  return ret;
}

vector<GPUCounter> ReplayProxy::EnumerateCounters()
{
  vector<GPUCounter> ret;

  if(m_RemoteServer)
  {
    ret = m_Remote->EnumerateCounters();
  }
  else
  {
    if(!SendReplayCommand(eReplayProxy_EnumerateCounters))
      return ret;
  }

  m_FromReplaySerialiser->Serialise("", ret);

  return ret;
}

void ReplayProxy::DescribeCounter(GPUCounter counterID, CounterDescription &desc)
{
  m_ToReplaySerialiser->Serialise("", counterID);

  if(m_RemoteServer)
  {
    m_Remote->DescribeCounter(counterID, desc);
  }
  else
  {
    if(!SendReplayCommand(eReplayProxy_DescribeCounter))
      return;
  }

  m_FromReplaySerialiser->Serialise("", desc);

  return;
}

void ReplayProxy::FillCBufferVariables(ResourceId shader, string entryPoint, uint32_t cbufSlot,
                                       vector<ShaderVariable> &outvars, const vector<byte> &data)
{
  m_ToReplaySerialiser->Serialise("", shader);
  m_ToReplaySerialiser->Serialise("", entryPoint);
  m_ToReplaySerialiser->Serialise("", cbufSlot);
  m_ToReplaySerialiser->Serialise("", outvars);
  m_ToReplaySerialiser->Serialise("", (vector<byte> &)data);

  if(m_RemoteServer)
  {
    m_Remote->FillCBufferVariables(shader, entryPoint, cbufSlot, outvars, data);
  }
  else
  {
    if(!SendReplayCommand(eReplayProxy_FillCBufferVariables))
      return;
  }

  m_FromReplaySerialiser->Serialise("", outvars);

  return;
}

void ReplayProxy::GetBufferData(ResourceId buff, uint64_t offset, uint64_t len, vector<byte> &retData)
{
  m_ToReplaySerialiser->Serialise("", buff);
  m_ToReplaySerialiser->Serialise("", offset);
  m_ToReplaySerialiser->Serialise("", len);

  if(m_RemoteServer)
  {
    m_Remote->GetBufferData(buff, offset, len, retData);

    uint64_t sz = retData.size();
    m_FromReplaySerialiser->Serialise("", sz);
    m_FromReplaySerialiser->RawWriteBytes(&retData[0], (size_t)sz);
  }
  else
  {
    if(!SendReplayCommand(eReplayProxy_GetBufferData))
      return;

    uint64_t sz = 0;
    m_FromReplaySerialiser->Serialise("", sz);
    retData.resize((size_t)sz);
    memcpy(&retData[0], m_FromReplaySerialiser->RawReadBytes((size_t)sz), (size_t)sz);
  }
}

byte *ReplayProxy::GetTextureData(ResourceId tex, uint32_t arrayIdx, uint32_t mip,
                                  const GetTextureDataParams &_params, size_t &dataSize)
{
  GetTextureDataParams params = _params;    // Serialiser is non-const

  m_ToReplaySerialiser->Serialise("", tex);
  m_ToReplaySerialiser->Serialise("", arrayIdx);
  m_ToReplaySerialiser->Serialise("", mip);
  m_ToReplaySerialiser->Serialise("", params.forDiskSave);
  m_ToReplaySerialiser->Serialise("", params.typeHint);
  m_ToReplaySerialiser->Serialise("", params.resolve);
  m_ToReplaySerialiser->Serialise("", params.remap);
  m_ToReplaySerialiser->Serialise("", params.blackPoint);
  m_ToReplaySerialiser->Serialise("", params.whitePoint);

  if(m_RemoteServer)
  {
    byte *data = m_Remote->GetTextureData(tex, arrayIdx, mip, params, dataSize);

    byte *compressed = new byte[LZ4_COMPRESSBOUND(dataSize)];

    uint32_t uncompressedSize = (uint32_t)dataSize;
    uint32_t compressedSize =
        (uint32_t)LZ4_compress((const char *)data, (char *)compressed, (int)uncompressedSize);

    m_FromReplaySerialiser->Serialise("", uncompressedSize);
    m_FromReplaySerialiser->Serialise("", compressedSize);
    m_FromReplaySerialiser->RawWriteBytes(compressed, (size_t)compressedSize);

    delete[] data;
    delete[] compressed;
  }
  else
  {
    if(!SendReplayCommand(eReplayProxy_GetTextureData))
    {
      dataSize = 0;
      return NULL;
    }

    uint32_t uncompressedSize = 0;
    uint32_t compressedSize = 0;

    m_FromReplaySerialiser->Serialise("", uncompressedSize);
    m_FromReplaySerialiser->Serialise("", compressedSize);

    if(uncompressedSize == 0 || compressedSize == 0)
    {
      dataSize = 0;
      return NULL;
    }

    dataSize = (size_t)uncompressedSize;

    byte *ret = new byte[dataSize + 512];

    byte *compressed = (byte *)m_FromReplaySerialiser->RawReadBytes((size_t)compressedSize);

    LZ4_decompress_fast((const char *)compressed, (char *)ret, (int)dataSize);

    return ret;
  }

  return NULL;
}

void ReplayProxy::InitPostVSBuffers(uint32_t eventID)
{
  m_ToReplaySerialiser->Serialise("", eventID);

  if(m_RemoteServer)
  {
    m_Remote->InitPostVSBuffers(eventID);
  }
  else
  {
    if(!SendReplayCommand(eReplayProxy_InitPostVS))
      return;
  }
}

void ReplayProxy::InitPostVSBuffers(const vector<uint32_t> &events)
{
  m_ToReplaySerialiser->Serialise("", (vector<uint32_t> &)events);

  if(m_RemoteServer)
  {
    m_Remote->InitPostVSBuffers(events);
  }
  else
  {
    if(!SendReplayCommand(eReplayProxy_InitPostVSVec))
      return;
  }
}

MeshFormat ReplayProxy::GetPostVSBuffers(uint32_t eventID, uint32_t instID, MeshDataStage stage)
{
  MeshFormat ret;

  m_ToReplaySerialiser->Serialise("", eventID);
  m_ToReplaySerialiser->Serialise("", instID);
  m_ToReplaySerialiser->Serialise("", stage);

  if(m_RemoteServer)
  {
    ret = m_Remote->GetPostVSBuffers(eventID, instID, stage);
  }
  else
  {
    if(!SendReplayCommand(eReplayProxy_GetPostVS))
      return ret;
  }

  m_FromReplaySerialiser->Serialise("", ret);

  return ret;
}

ResourceId ReplayProxy::RenderOverlay(ResourceId texid, CompType typeHint, DebugOverlay overlay,
                                      uint32_t eventID, const vector<uint32_t> &passEvents)
{
  ResourceId ret;

  vector<uint32_t> events = passEvents;

  m_ToReplaySerialiser->Serialise("", texid);
  m_ToReplaySerialiser->Serialise("", typeHint);
  m_ToReplaySerialiser->Serialise("", overlay);
  m_ToReplaySerialiser->Serialise("", eventID);
  m_ToReplaySerialiser->Serialise("", events);

  if(m_RemoteServer)
  {
    ret = m_Remote->RenderOverlay(texid, typeHint, overlay, eventID, events);
  }
  else
  {
    if(!SendReplayCommand(eReplayProxy_RenderOverlay))
      return ret;
  }

  m_FromReplaySerialiser->Serialise("", ret);

  return ret;
}

ShaderReflection *ReplayProxy::GetShader(ResourceId id, string entryPoint)
{
  if(m_RemoteServer)
  {
    m_ToReplaySerialiser->Serialise("", id);
    m_ToReplaySerialiser->Serialise("", entryPoint);

    ShaderReflection *refl = m_Remote->GetShader(id, entryPoint);

    bool hasrefl = (refl != NULL);
    m_FromReplaySerialiser->Serialise("", hasrefl);

    if(hasrefl)
      m_FromReplaySerialiser->Serialise("", *refl);

    return NULL;
  }

  ShaderReflKey key(id, entryPoint);

  if(m_ShaderReflectionCache.find(key) == m_ShaderReflectionCache.end())
  {
    m_ToReplaySerialiser->Serialise("", id);
    m_ToReplaySerialiser->Serialise("", entryPoint);

    if(!SendReplayCommand(eReplayProxy_GetShader))
      return NULL;

    bool hasrefl = false;
    m_FromReplaySerialiser->Serialise("", hasrefl);

    if(hasrefl)
    {
      m_ShaderReflectionCache[key] = new ShaderReflection();

      m_FromReplaySerialiser->Serialise("", *m_ShaderReflectionCache[key]);
    }
    else
    {
      m_ShaderReflectionCache[key] = NULL;
    }
  }

  return m_ShaderReflectionCache[key];
}

string ReplayProxy::DisassembleShader(const ShaderReflection *refl, const string &target)
{
  string ret;

  ResourceId id;
  std::string entryPoint;
  std::string isatarget = target;

  if(refl)
  {
    id = refl->ID;
    entryPoint = refl->EntryPoint;
  }

  m_ToReplaySerialiser->Serialise("", id);
  m_ToReplaySerialiser->Serialise("", entryPoint);
  m_ToReplaySerialiser->Serialise("", isatarget);

  if(m_RemoteServer)
  {
    refl = m_Remote->GetShader(m_Remote->GetLiveID(id), entryPoint);
    if(refl)
      ret = m_Remote->DisassembleShader(m_Remote->GetShader(m_Remote->GetLiveID(id), entryPoint),
                                        isatarget);
  }
  else
  {
    if(!SendReplayCommand(eReplayProxy_DisassembleShader))
      return ret;
  }

  m_FromReplaySerialiser->Serialise("", ret);

  return ret;
}

vector<string> ReplayProxy::GetDisassemblyTargets()
{
  vector<string> ret;

  if(m_RemoteServer)
  {
    ret = m_Remote->GetDisassemblyTargets();
  }
  else
  {
    if(!SendReplayCommand(eReplayProxy_GetISATargets))
      return ret;
  }

  m_FromReplaySerialiser->Serialise("", ret);

  return ret;
}

void ReplayProxy::FreeTargetResource(ResourceId id)
{
  m_ToReplaySerialiser->Serialise("", id);

  if(m_RemoteServer)
  {
    m_Remote->FreeTargetResource(id);
  }
  else
  {
    if(!SendReplayCommand(eReplayProxy_FreeResource))
      return;
  }
}

void ReplayProxy::InitCallstackResolver()
{
  if(m_RemoteServer)
  {
    m_Remote->InitCallstackResolver();
  }
  else
  {
    if(!SendReplayCommand(eReplayProxy_InitStackResolver))
      return;
  }
}

Callstack::StackResolver *ReplayProxy::GetCallstackResolver()
{
  if(m_RemoteHasResolver)
    return this;

  bool remoteHasResolver = false;

  if(m_RemoteServer)
  {
    remoteHasResolver = m_Remote->GetCallstackResolver() != NULL;
  }
  else
  {
    if(!SendReplayCommand(eReplayProxy_HasStackResolver))
      return NULL;
  }

  m_FromReplaySerialiser->Serialise("", remoteHasResolver);

  if(remoteHasResolver)
  {
    if(!m_RemoteServer)
      m_RemoteHasResolver = true;

    return this;
  }

  return NULL;
}

Callstack::AddressDetails ReplayProxy::GetAddr(uint64_t addr)
{
  Callstack::AddressDetails ret;

  if(m_RemoteServer)
  {
    Callstack::StackResolver *resolv = m_Remote->GetCallstackResolver();
    if(resolv)
      ret = resolv->GetAddr(addr);
  }
  else
  {
    if(!SendReplayCommand(eReplayProxy_HasStackResolver))
      return ret;
  }

  m_FromReplaySerialiser->Serialise("", ret.filename);
  m_FromReplaySerialiser->Serialise("", ret.function);
  m_FromReplaySerialiser->Serialise("", ret.line);

  return ret;
}

void ReplayProxy::BuildTargetShader(string source, string entry, const uint32_t compileFlags,
                                    ShaderStage type, ResourceId *id, string *errors)
{
  uint32_t flags = compileFlags;
  m_ToReplaySerialiser->Serialise("", source);
  m_ToReplaySerialiser->Serialise("", entry);
  m_ToReplaySerialiser->Serialise("", flags);
  m_ToReplaySerialiser->Serialise("", type);

  ResourceId outId;
  string outErrs;

  if(m_RemoteServer)
  {
    m_Remote->BuildTargetShader(source, entry, flags, type, &outId, &outErrs);
  }
  else
  {
    if(!SendReplayCommand(eReplayProxy_BuildTargetShader))
      return;
  }

  m_FromReplaySerialiser->Serialise("", outId);
  m_FromReplaySerialiser->Serialise("", outErrs);

  if(!m_RemoteServer)
  {
    if(id)
      *id = outId;
    if(errors)
      *errors = outErrs;
  }
}

void ReplayProxy::ReplaceResource(ResourceId from, ResourceId to)
{
  m_ToReplaySerialiser->Serialise("", from);
  m_ToReplaySerialiser->Serialise("", to);

  if(m_RemoteServer)
  {
    m_Remote->ReplaceResource(from, to);
  }
  else
  {
    if(!SendReplayCommand(eReplayProxy_ReplaceResource))
      return;
  }
}

void ReplayProxy::RemoveReplacement(ResourceId id)
{
  m_ToReplaySerialiser->Serialise("", id);

  if(m_RemoteServer)
  {
    m_Remote->RemoveReplacement(id);
  }
  else
  {
    if(!SendReplayCommand(eReplayProxy_RemoveReplacement))
      return;
  }
}

vector<PixelModification> ReplayProxy::PixelHistory(vector<EventUsage> events, ResourceId target,
                                                    uint32_t x, uint32_t y, uint32_t slice,
                                                    uint32_t mip, uint32_t sampleIdx,
                                                    CompType typeHint)
{
  vector<PixelModification> ret;

  m_ToReplaySerialiser->Serialise("", events);
  m_ToReplaySerialiser->Serialise("", target);
  m_ToReplaySerialiser->Serialise("", x);
  m_ToReplaySerialiser->Serialise("", y);
  m_ToReplaySerialiser->Serialise("", slice);
  m_ToReplaySerialiser->Serialise("", mip);
  m_ToReplaySerialiser->Serialise("", sampleIdx);
  m_ToReplaySerialiser->Serialise("", typeHint);

  if(m_RemoteServer)
  {
    ret = m_Remote->PixelHistory(events, target, x, y, slice, mip, sampleIdx, typeHint);
  }
  else
  {
    if(!SendReplayCommand(eReplayProxy_PixelHistory))
      return ret;
  }

  m_FromReplaySerialiser->Serialise("", ret);

  return ret;
}

ShaderDebugTrace ReplayProxy::DebugVertex(uint32_t eventID, uint32_t vertid, uint32_t instid,
                                          uint32_t idx, uint32_t instOffset, uint32_t vertOffset)
{
  ShaderDebugTrace ret;

  m_ToReplaySerialiser->Serialise("", eventID);
  m_ToReplaySerialiser->Serialise("", vertid);
  m_ToReplaySerialiser->Serialise("", instid);
  m_ToReplaySerialiser->Serialise("", idx);
  m_ToReplaySerialiser->Serialise("", instOffset);
  m_ToReplaySerialiser->Serialise("", vertOffset);

  if(m_RemoteServer)
  {
    ret = m_Remote->DebugVertex(eventID, vertid, instid, idx, instOffset, vertOffset);
  }
  else
  {
    if(!SendReplayCommand(eReplayProxy_DebugVertex))
      return ret;
  }

  m_FromReplaySerialiser->Serialise("", ret);

  return ret;
}

ShaderDebugTrace ReplayProxy::DebugPixel(uint32_t eventID, uint32_t x, uint32_t y, uint32_t sample,
                                         uint32_t primitive)
{
  ShaderDebugTrace ret;

  m_ToReplaySerialiser->Serialise("", eventID);
  m_ToReplaySerialiser->Serialise("", x);
  m_ToReplaySerialiser->Serialise("", y);
  m_ToReplaySerialiser->Serialise("", sample);
  m_ToReplaySerialiser->Serialise("", primitive);

  if(m_RemoteServer)
  {
    ret = m_Remote->DebugPixel(eventID, x, y, sample, primitive);
  }
  else
  {
    if(!SendReplayCommand(eReplayProxy_DebugPixel))
      return ret;
  }

  m_FromReplaySerialiser->Serialise("", ret);

  return ret;
}

ShaderDebugTrace ReplayProxy::DebugThread(uint32_t eventID, const uint32_t groupid[3],
                                          const uint32_t threadid[3])
{
  ShaderDebugTrace ret;

  uint32_t g[] = {groupid[0], groupid[1], groupid[2]};
  uint32_t t[] = {threadid[0], threadid[1], threadid[2]};

  m_ToReplaySerialiser->Serialise("", eventID);
  m_ToReplaySerialiser->SerialisePODArray<3>("", g);
  m_ToReplaySerialiser->SerialisePODArray<3>("", t);

  if(m_RemoteServer)
  {
    ret = m_Remote->DebugThread(eventID, g, t);
  }
  else
  {
    if(!SendReplayCommand(eReplayProxy_DebugThread))
      return ret;
  }

  m_FromReplaySerialiser->Serialise("", ret);

  return ret;
}
