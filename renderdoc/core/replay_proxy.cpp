/******************************************************************************
 * The MIT License (MIT)
 * 
 * Copyright (c) 2015-2016 Baldur Karlsson
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

#if defined(RENDERDOC_PLATFORM_WIN32) && defined(RDC64BIT)
template<typename T, size_t actual, size_t expected> class oversized { int check[int(actual)-int(expected)+1]; };
template<typename T, size_t actual, size_t expected> class undersized { int check[int(expected)-int(actual)+1]; };

#define SIZE_CHECK(T, expected) undersized<T, sizeof(T), expected>(); oversized<T, sizeof(T), expected>();
#else
#define SIZE_CHECK(T, expected)
#endif

#pragma region General Shader/State

template<>
string ToStrHelper<false, SystemAttribute>::Get(const SystemAttribute &el)
{
	switch(el)
	{
		TOSTR_CASE_STRINGIZE(eAttr_None)
		TOSTR_CASE_STRINGIZE(eAttr_Position)
		TOSTR_CASE_STRINGIZE(eAttr_PointSize)
		TOSTR_CASE_STRINGIZE(eAttr_ClipDistance)
		TOSTR_CASE_STRINGIZE(eAttr_CullDistance)
		TOSTR_CASE_STRINGIZE(eAttr_RTIndex)
		TOSTR_CASE_STRINGIZE(eAttr_ViewportIndex)
		TOSTR_CASE_STRINGIZE(eAttr_VertexIndex)
		TOSTR_CASE_STRINGIZE(eAttr_PrimitiveIndex)
		TOSTR_CASE_STRINGIZE(eAttr_InstanceIndex)
		TOSTR_CASE_STRINGIZE(eAttr_InvocationIndex)
		TOSTR_CASE_STRINGIZE(eAttr_DispatchSize)
		TOSTR_CASE_STRINGIZE(eAttr_DispatchThreadIndex)
		TOSTR_CASE_STRINGIZE(eAttr_GroupIndex)
		TOSTR_CASE_STRINGIZE(eAttr_GroupFlatIndex)
		TOSTR_CASE_STRINGIZE(eAttr_GroupThreadIndex)
		TOSTR_CASE_STRINGIZE(eAttr_GSInstanceIndex)
		TOSTR_CASE_STRINGIZE(eAttr_OutputControlPointIndex)
		TOSTR_CASE_STRINGIZE(eAttr_DomainLocation)
		TOSTR_CASE_STRINGIZE(eAttr_IsFrontFace)
		TOSTR_CASE_STRINGIZE(eAttr_MSAACoverage)
		TOSTR_CASE_STRINGIZE(eAttr_MSAASamplePosition)
		TOSTR_CASE_STRINGIZE(eAttr_MSAASampleIndex)
		TOSTR_CASE_STRINGIZE(eAttr_PatchNumVertices)
		TOSTR_CASE_STRINGIZE(eAttr_OuterTessFactor)
		TOSTR_CASE_STRINGIZE(eAttr_InsideTessFactor)
		TOSTR_CASE_STRINGIZE(eAttr_ColourOutput)
		TOSTR_CASE_STRINGIZE(eAttr_DepthOutput)
		TOSTR_CASE_STRINGIZE(eAttr_DepthOutputGreaterEqual)
		TOSTR_CASE_STRINGIZE(eAttr_DepthOutputLessEqual)
		default: break;
	}
	
	char tostrBuf[256] = {0};
	StringFormat::snprintf(tostrBuf, 255, "SystemAttribute<%d>", el);

	return tostrBuf;
}

template<>
void Serialiser::Serialise(const char *name, ResourceFormat &el)
{
	Serialise("", el.rawType);
	Serialise("", el.special);
	Serialise("", el.specialFormat);
	Serialise("", el.strname);
	Serialise("", el.compCount);
	Serialise("", el.compByteWidth);
	Serialise("", el.compType);
	Serialise("", el.bgraOrder);
	Serialise("", el.srgbCorrected);

	SIZE_CHECK(ResourceFormat, 56);
}

template<>
void Serialiser::Serialise(const char *name, BindpointMap &el)
{
	Serialise("", el.bindset);
	Serialise("", el.bind);
	Serialise("", el.used);
	Serialise("", el.arraySize);

	SIZE_CHECK(BindpointMap, 16);
}

template<>
void Serialiser::Serialise(const char *name, ShaderBindpointMapping &el)
{
	Serialise("", el.InputAttributes);
	Serialise("", el.ConstantBlocks);
	Serialise("", el.ReadOnlyResources);
	Serialise("", el.ReadWriteResources);

	SIZE_CHECK(ShaderBindpointMapping, 64);
}

template<>
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

	SIZE_CHECK(SigParameter, 88);
}

template<>
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

	SIZE_CHECK(ShaderVariableType, 56);
}

template<>
void Serialiser::Serialise(const char *name, ShaderConstant &el)
{
	Serialise("", el.name);
	Serialise("", el.reg.vec);
	Serialise("", el.reg.comp);
	Serialise("", el.type);

	SIZE_CHECK(ShaderConstant, 80);
}

template<>
void Serialiser::Serialise(const char *name, ConstantBlock &el)
{
	Serialise("", el.name);
	Serialise("", el.variables);
	Serialise("", el.bufferBacked);
	Serialise("", el.bindPoint);

	SIZE_CHECK(ConstantBlock, 40);
}

template<>
void Serialiser::Serialise(const char *name, ShaderResource &el)
{
	Serialise("", el.IsSampler);
	Serialise("", el.IsTexture);
	Serialise("", el.IsSRV);
	Serialise("", el.resType);
	Serialise("", el.name);
	Serialise("", el.variableType);
	Serialise("", el.bindPoint);

	SIZE_CHECK(ShaderResource, 96);
}

template<>
void Serialiser::Serialise(const char *name, ShaderReflection &el)
{
	Serialise("", el.DebugInfo.compileFlags);
	Serialise("", el.DebugInfo.entryFunc);
	Serialise("", el.DebugInfo.files);

	SerialisePODArray<3>("", el.DispatchThreadsDimension);

	Serialise("", el.Disassembly);
	
	Serialise("", el.InputSig);
	Serialise("", el.OutputSig);

	Serialise("", el.ConstantBlocks);

	Serialise("", el.ReadOnlyResources);
	Serialise("", el.ReadWriteResources);

	Serialise("", el.Interfaces);

	SIZE_CHECK(ShaderReflection, 176);
}

template<>
void Serialiser::Serialise(const char *name, ShaderVariable &el)
{
	Serialise("", el.rows);
	Serialise("", el.columns);
	Serialise("", el.name);
	Serialise("", el.type);

	SerialisePODArray<16>("", el.value.dv);
	
	Serialise("", el.isStruct);
	
	Serialise("", el.members);

	SIZE_CHECK(ShaderVariable, 184);
}

template<>
void Serialiser::Serialise(const char *name, ShaderDebugState &el)
{
	Serialise("", el.registers);
	Serialise("", el.outputs);
	Serialise("", el.nextInstruction);
	
	vector< vector<ShaderVariable> > indexableTemps;
	
	int32_t numidxtemps = el.indexableTemps.count;
	Serialise("", numidxtemps);

	if(m_Mode == READING) create_array_uninit(el.indexableTemps, numidxtemps);

	for(int32_t i=0; i < numidxtemps; i++)
		Serialise("", el.indexableTemps[i]);

	SIZE_CHECK(ShaderDebugState, 56);
}

template<>
void Serialiser::Serialise(const char *name, ShaderDebugTrace &el)
{
	Serialise("", el.inputs);

	int32_t numcbuffers = el.cbuffers.count;
	Serialise("", numcbuffers);

	if(m_Mode == READING) create_array_uninit(el.cbuffers, numcbuffers);

	for(int32_t i=0; i < numcbuffers; i++)
		Serialise("", el.cbuffers[i]);

	Serialise("", el.states);

	SIZE_CHECK(ShaderDebugTrace, 48);
}

#pragma endregion General Shader/State

#pragma region D3D11 pipeline state

template<>
void Serialiser::Serialise(const char *name, D3D11PipelineState::InputAssembler::LayoutInput &el)
{
	Serialise("", el.SemanticName);
	Serialise("", el.SemanticIndex);
	Serialise("", el.Format);
	Serialise("", el.InputSlot);
	Serialise("", el.ByteOffset);
	Serialise("", el.PerInstance);
	Serialise("", el.InstanceDataStepRate);

	SIZE_CHECK(D3D11PipelineState::InputAssembler::LayoutInput, 96);
}

template<>
void Serialiser::Serialise(const char *name, D3D11PipelineState::InputAssembler &el)
{
	Serialise("", el.ibuffer.Buffer);
	Serialise("", el.ibuffer.Offset);

	Serialise("", el.customName);
	Serialise("", el.LayoutName);
	
	Serialise("", el.vbuffers);
	Serialise("", el.layouts);

	SIZE_CHECK(D3D11PipelineState::InputAssembler, 88);
}

template<>
void Serialiser::Serialise(const char *name, D3D11PipelineState::ShaderStage::ResourceView &el)
{
	Serialise("", el.View);
	Serialise("", el.Resource);
	Serialise("", el.Type);
	Serialise("", el.Format);
			
	Serialise("", el.Structured);
	Serialise("", el.BufferStructCount);
	Serialise("", el.ElementOffset);
	Serialise("", el.ElementWidth);
	Serialise("", el.FirstElement);
	Serialise("", el.NumElements);

	Serialise("", el.Flags);
	Serialise("", el.HighestMip);
	Serialise("", el.NumMipLevels);
	Serialise("", el.ArraySize);
	Serialise("", el.FirstArraySlice);

	SIZE_CHECK(D3D11PipelineState::ShaderStage::ResourceView, 136);
}

template<>
void Serialiser::Serialise(const char *name, D3D11PipelineState::ShaderStage::Sampler &el)
{
	Serialise("", el.Samp);
	Serialise("", el.AddressU);
	Serialise("", el.AddressV);
	Serialise("", el.AddressW);
	SerialisePODArray<4>("", el.BorderColor);
	Serialise("", el.Comparison);
	Serialise("", el.Filter);
	Serialise("", el.UseBorder);
	Serialise("", el.UseComparison);
	Serialise("", el.MaxAniso);
	Serialise("", el.MaxLOD);
	Serialise("", el.MinLOD);
	Serialise("", el.MipLODBias);

	SIZE_CHECK(D3D11PipelineState::ShaderStage::Sampler, 128);
}

template<>
void Serialiser::Serialise(const char *name, D3D11PipelineState::ShaderStage &el)
{
	Serialise("", el.Shader);
	Serialise("", el.stage);
	Serialise("", el.ShaderName);
	Serialise("", el.customName);

	if(m_Mode == READING)
		el.ShaderDetails = NULL;

	Serialise("", el.BindpointMapping);
	
	Serialise("", el.SRVs);
	Serialise("", el.UAVs);
	Serialise("", el.Samplers);
	Serialise("", el.ConstantBuffers);
	Serialise("", el.ClassInstances);

	SIZE_CHECK(D3D11PipelineState::ShaderStage, 192);
}

template<>
void Serialiser::Serialise(const char *name, D3D11PipelineState::Rasterizer &el)
{
	Serialise("", el.m_State);
	Serialise("", el.Scissors);
	Serialise("", el.Viewports);

	SIZE_CHECK(D3D11PipelineState::Rasterizer, 88);
}

template<>
void Serialiser::Serialise(const char *name, D3D11PipelineState::OutputMerger::BlendState::RTBlend &el)
{
	Serialise("", el.m_Blend.Source);
	Serialise("", el.m_Blend.Destination);
	Serialise("", el.m_Blend.Operation);
				
	Serialise("", el.m_AlphaBlend.Source);
	Serialise("", el.m_AlphaBlend.Destination);
	Serialise("", el.m_AlphaBlend.Operation);

	Serialise("", el.LogicOp);

	Serialise("", el.Enabled);
	Serialise("", el.LogicEnabled);
	Serialise("", el.WriteMask);

	SIZE_CHECK(D3D11PipelineState::OutputMerger::BlendState::RTBlend, 128);
}

template<>
void Serialiser::Serialise(const char *name, D3D11PipelineState::OutputMerger &el)
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

	SIZE_CHECK(D3D11PipelineState::OutputMerger, 424);
}

template<>
void Serialiser::Serialise(const char *name, D3D11PipelineState &el)
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

	SIZE_CHECK(D3D11PipelineState, 1768);
}

#pragma endregion D3D11 pipeline state

#pragma region OpenGL pipeline state

template<>
void Serialiser::Serialise(const char *name, GLPipelineState::VertexInput::VertexAttribute &el)
{
	Serialise("", el.Enabled);
	Serialise("", el.Format);
	SerialisePODArray<4>("", el.GenericValue.f);
	Serialise("", el.BufferSlot);
	Serialise("", el.RelativeOffset);

	SIZE_CHECK(GLPipelineState::VertexInput::VertexAttribute, 88);
}

template<>
void Serialiser::Serialise(const char *name, GLPipelineState::VertexInput &el)
{
	Serialise("", el.attributes);
	Serialise("", el.vbuffers);
	Serialise("", el.ibuffer);
	Serialise("", el.primitiveRestart);
	Serialise("", el.restartIndex);
	Serialise("", el.provokingVertexLast);

	SIZE_CHECK(GLPipelineState::VertexInput, 56);
}

template<>
void Serialiser::Serialise(const char *name, GLPipelineState::ShaderStage &el)
{
	Serialise("", el.Shader);
	
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

	SIZE_CHECK(GLPipelineState::ShaderStage, 176);
}

template<>
void Serialiser::Serialise(const char *name, GLPipelineState::Sampler &el)
{
	Serialise("", el.Samp);
	Serialise("", el.AddressS);
	Serialise("", el.AddressT);
	Serialise("", el.AddressR);
	SerialisePODArray<4>("", el.BorderColor);
	Serialise("", el.Comparison);
	Serialise("", el.MinFilter);
	Serialise("", el.MagFilter);
	Serialise("", el.UseBorder);
	Serialise("", el.UseComparison);
	Serialise("", el.SeamlessCube);
	Serialise("", el.MaxAniso);
	Serialise("", el.MaxLOD);
	Serialise("", el.MinLOD);
	Serialise("", el.MipLODBias);

	SIZE_CHECK(GLPipelineState::Sampler, 152);
}

template<>
void Serialiser::Serialise(const char *name, GLPipelineState::ImageLoadStore &el)
{
	Serialise("", el.Resource);
	Serialise("", el.Level);
	Serialise("", el.Layered);
	Serialise("", el.Layer);
	Serialise("", el.ResType);
	Serialise("", el.readAllowed);
	Serialise("", el.writeAllowed);
	Serialise("", el.Format);

	SIZE_CHECK(GLPipelineState::ImageLoadStore, 88);
}

template<>
void Serialiser::Serialise(const char *name, GLPipelineState::Rasterizer &el)
{
	Serialise("", el.Viewports);
	Serialise("", el.Scissors);
	Serialise("", el.m_State);

	SIZE_CHECK(GLPipelineState::Rasterizer, 120);
}

template<>
void Serialiser::Serialise(const char *name, GLPipelineState::DepthState &el)
{
	Serialise("", el.DepthEnable);
	Serialise("", el.DepthFunc);
	Serialise("", el.DepthWrites);
	Serialise("", el.DepthBounds);
	Serialise("", el.NearBound);
	Serialise("", el.FarBound);

	SIZE_CHECK(GLPipelineState::DepthState, 48);
}

template<>
void Serialiser::Serialise(const char *name, GLPipelineState::StencilState &el)
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

	SIZE_CHECK(GLPipelineState::StencilState, 168);
}

template<>
void Serialiser::Serialise(const char *name, GLPipelineState::FrameBuffer::BlendState::RTBlend &el)
{
	Serialise("", el.Enabled);
	Serialise("", el.WriteMask);
	Serialise("", el.LogicOp);

	Serialise("", el.m_Blend.Source);
	Serialise("", el.m_Blend.Destination);
	Serialise("", el.m_Blend.Operation);

	Serialise("", el.m_AlphaBlend.Source);
	Serialise("", el.m_AlphaBlend.Destination);
	Serialise("", el.m_AlphaBlend.Operation);

	SIZE_CHECK(GLPipelineState::FrameBuffer::BlendState::RTBlend, 120);
}

template<>
void Serialiser::Serialise(const char *name, GLPipelineState::FrameBuffer::BlendState &el)
{
	SerialisePODArray<4>("", el.BlendFactor);
	Serialise("", el.Blends);

	SIZE_CHECK(GLPipelineState::FrameBuffer::BlendState, 32);
}

template<>
void Serialiser::Serialise(const char *name, GLPipelineState::FrameBuffer::Attachment &el)
{
	Serialise("", el.Obj);
	Serialise("", el.Layer);
	Serialise("", el.Mip);

	SIZE_CHECK(GLPipelineState::FrameBuffer::Attachment, 16);
}

template<>
void Serialiser::Serialise(const char *name, GLPipelineState::FrameBuffer &el)
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

	SIZE_CHECK(GLPipelineState::FrameBuffer, 200);
}

template<>
void Serialiser::Serialise(const char *name, GLPipelineState &el)
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

	SIZE_CHECK(GLPipelineState, 1952);
}

#pragma endregion OpenGL pipeline state

#pragma region Vulkan pipeline state

template<>
void Serialiser::Serialise(const char *name, VulkanPipelineState::Pipeline::DescriptorSet::DescriptorBinding::BindingElement &el)
{
	Serialise("", el.view);
	Serialise("", el.res);
	Serialise("", el.sampler);

	Serialise("", el.baseMip);
	Serialise("", el.baseLayer);

	Serialise("", el.offset);
	Serialise("", el.size);

	Serialise("", el.mag);
	Serialise("", el.min); 
	Serialise("", el.mip);
	Serialise("", el.addrU);
	Serialise("", el.addrV); 
	Serialise("", el.addrW);
	Serialise("", el.mipBias);
	Serialise("", el.maxAniso);
	Serialise("", el.compareEnable);
	Serialise("", el.comparison);
	Serialise("", el.minlod);
	Serialise("", el.maxlod);
	Serialise("", el.borderEnable);
	Serialise("", el.border);
	Serialise("", el.unnormalized);

	SIZE_CHECK(VulkanPipelineState::Pipeline::DescriptorSet::DescriptorBinding::BindingElement, 216);
};

template<>
void Serialiser::Serialise(const char *name, VulkanPipelineState::Pipeline::DescriptorSet::DescriptorBinding &el)
{
	Serialise("", el.descriptorCount);
	Serialise("", el.type);
	Serialise("", el.stageFlags);

	Serialise("", el.binds);

	SIZE_CHECK(VulkanPipelineState::Pipeline::DescriptorSet::DescriptorBinding, 32);
}

template<>
void Serialiser::Serialise(const char *name, VulkanPipelineState::Pipeline::DescriptorSet &el)
{
	Serialise("", el.layout);
	Serialise("", el.descset);

	Serialise("", el.bindings);

	SIZE_CHECK(VulkanPipelineState::Pipeline::DescriptorSet, 32);
}

template<>
void Serialiser::Serialise(const char *name, VulkanPipelineState::Pipeline &el)
{
	Serialise("", el.obj);
	Serialise("", el.flags);

	Serialise("", el.DescSets);

	SIZE_CHECK(VulkanPipelineState::Pipeline, 32);
}


template<>
void Serialiser::Serialise(const char *name, VulkanPipelineState::VertexInput::Attribute &el)
{
	Serialise("", el.location);
	Serialise("", el.binding);
	Serialise("", el.format);
	Serialise("", el.byteoffset);

	SIZE_CHECK(VulkanPipelineState::VertexInput::Attribute, 72);
}

template<>
void Serialiser::Serialise(const char *name, VulkanPipelineState::VertexInput &el)
{
	Serialise("", el.attrs);
	Serialise("", el.binds);
	Serialise("", el.vbuffers);

	SIZE_CHECK(VulkanPipelineState::VertexInput, 48);
}

template<>
void Serialiser::Serialise(const char *name, VulkanPipelineState::ShaderStage::SpecInfo &el)
{
	Serialise("", el.specID);
	Serialise("", el.data);

	SIZE_CHECK(VulkanPipelineState::ShaderStage::SpecInfo, 24);
}

template<>
void Serialiser::Serialise(const char *name, VulkanPipelineState::ShaderStage &el)
{
	Serialise("", el.Shader);
	Serialise("", el.entryPoint);

	Serialise("", el.ShaderName);
	Serialise("", el.customName);
	Serialise("", el.BindpointMapping);
	Serialise("", el.stage);

	if(m_Mode == READING)
		el.ShaderDetails = NULL;
	
	Serialise("", el.specialization);

	SIZE_CHECK(VulkanPipelineState::ShaderStage, 144);
}

template<>
void Serialiser::Serialise(const char *name, VulkanPipelineState::ViewState &el)
{
	Serialise("", el.viewportScissors);

	SIZE_CHECK(VulkanPipelineState::ViewState, 16);
}

template<>
void Serialiser::Serialise(const char *name, VulkanPipelineState::ColorBlend::Attachment &el)
{
	Serialise("", el.blendEnable);

	Serialise("", el.blend.Source);
	Serialise("", el.blend.Destination);
	Serialise("", el.blend.Operation);

	Serialise("", el.alphaBlend.Source);
	Serialise("", el.alphaBlend.Destination);
	Serialise("", el.alphaBlend.Operation);

	Serialise("", el.writeMask);

	SIZE_CHECK(VulkanPipelineState::ColorBlend::Attachment, 112);
}

template<>
void Serialiser::Serialise(const char *name, VulkanPipelineState::ColorBlend &el)
{
	Serialise("", el.alphaToCoverageEnable);
	Serialise("", el.alphaToOneEnable);
	Serialise("", el.logicOpEnable);
	Serialise("", el.logicOp);

	Serialise("", el.attachments);

	SerialisePODArray<4>("", el.blendConst);

	SIZE_CHECK(VulkanPipelineState::ColorBlend, 64);
}

template<>
void Serialiser::Serialise(const char *name, VulkanPipelineState::DepthStencil &el)
{
	Serialise("", el.depthTestEnable);
	Serialise("", el.depthWriteEnable);
	Serialise("", el.depthBoundsEnable);
	Serialise("", el.depthCompareOp);
	
	Serialise("", el.stencilTestEnable);
	
	Serialise("", el.front.failOp);
	Serialise("", el.front.depthFailOp);
	Serialise("", el.front.passOp);
	Serialise("", el.front.func);
	Serialise("", el.front.ref);
	Serialise("", el.front.compareMask);
	Serialise("", el.front.writeMask);
	
	Serialise("", el.back.failOp);
	Serialise("", el.back.depthFailOp);
	Serialise("", el.back.passOp);
	Serialise("", el.back.func);
	Serialise("", el.back.ref);
	Serialise("", el.back.compareMask);
	Serialise("", el.back.writeMask);
	
	Serialise("", el.minDepthBounds);
	Serialise("", el.maxDepthBounds);

	SIZE_CHECK(VulkanPipelineState::DepthStencil, 208);
}

template<>
void Serialiser::Serialise(const char *name, VulkanPipelineState::CurrentPass &el)
{
	Serialise("", el.renderpass.obj);
	Serialise("", el.renderpass.inputAttachments);
	Serialise("", el.renderpass.colorAttachments);
	Serialise("", el.renderpass.depthstencilAttachment);
	
	Serialise("", el.framebuffer.obj);
	Serialise("", el.framebuffer.attachments);
	Serialise("", el.framebuffer.width);
	Serialise("", el.framebuffer.height);
	Serialise("", el.framebuffer.layers);
	
	Serialise("", el.renderArea);

	SIZE_CHECK(VulkanPipelineState::CurrentPass, 104);
}

template<>
void Serialiser::Serialise(const char *name, VulkanPipelineState &el)
{
	Serialise("", el.compute);
	Serialise("", el.graphics);

	Serialise("", el.IA);
	Serialise("", el.VI);

	Serialise("", el.VS);
	Serialise("", el.TCS);
	Serialise("", el.TES);
	Serialise("", el.GS);
	Serialise("", el.FS);
	Serialise("", el.CS);

	Serialise("", el.Tess);

	Serialise("", el.VP);
	Serialise("", el.RS);
	Serialise("", el.MSAA);
	Serialise("", el.CB);
	Serialise("", el.DS);
	Serialise("", el.Pass);

	SIZE_CHECK(VulkanPipelineState, 1456);
}

#pragma endregion Vulkan pipeline state

#pragma region Data descriptors

template<>
void Serialiser::Serialise(const char *name, FetchTexture &el)
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
	Serialise("", el.numSubresources);
	Serialise("", el.creationFlags);
	Serialise("", el.msQual);
	Serialise("", el.msSamp);

	SIZE_CHECK(FetchTexture, 152);
}

template<>
void Serialiser::Serialise(const char *name, FetchBuffer &el)
{
	Serialise("", el.ID);
	Serialise("", el.name);
	Serialise("", el.customName);
	Serialise("", el.length);
	Serialise("", el.structureSize);
	Serialise("", el.creationFlags);
	Serialise("", el.byteSize);

	SIZE_CHECK(FetchBuffer, 48);
}

template<>
void Serialiser::Serialise(const char *name, APIProperties &el)
{
	Serialise("", el.pipelineType);
	Serialise("", el.degraded);

	SIZE_CHECK(APIProperties, 8);
}

template<>
void Serialiser::Serialise(const char *name, DebugMessage &el)
{
	Serialise("", el.eventID);
	Serialise("", el.category);
	Serialise("", el.severity);
	Serialise("", el.source);
	Serialise("", el.messageID);
	Serialise("", el.description);

	SIZE_CHECK(DebugMessage, 40);
}

template<>
void Serialiser::Serialise(const char *name, FetchAPIEvent &el)
{
	Serialise("", el.eventID);
	Serialise("", el.context);
	Serialise("", el.callstack);
	Serialise("", el.eventDesc);
	Serialise("", el.fileOffset);

	SIZE_CHECK(FetchAPIEvent, 56);
}

template<>
void Serialiser::Serialise(const char *name, FetchDrawcall &el)
{
	Serialise("", el.eventID);
	Serialise("", el.drawcallID);
	
	Serialise("", el.name);
	
	Serialise("", el.flags);
	
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

	Serialise("", el.context);

	Serialise("", el.copySource);
	Serialise("", el.copyDestination);
	
	Serialise("", el.parent);
	Serialise("", el.previous);
	Serialise("", el.next);
	
	SerialisePODArray<8>("", el.outputs);
	Serialise("", el.depthOut);
	
	Serialise("", el.events);
	Serialise("", el.children);

	SIZE_CHECK(FetchDrawcall, 240);
}

template<>
void Serialiser::Serialise(const char *name, FetchFrameConstantBindStats &el)
{
	Serialise("", el.calls);
	Serialise("", el.sets);
	Serialise("", el.nulls);
	Serialise("", el.bindslots);
	Serialise("", el.sizes);

	SIZE_CHECK(FetchFrameConstantBindStats, 48);
}

template<>
void Serialiser::Serialise(const char *name, FetchFrameSamplerBindStats &el)
{
	Serialise("", el.calls);
	Serialise("", el.sets);
	Serialise("", el.nulls);
	Serialise("", el.bindslots);

	SIZE_CHECK(FetchFrameSamplerBindStats, 32);
}

template<>
void Serialiser::Serialise(const char *name, FetchFrameResourceBindStats &el)
{
	Serialise("", el.calls);
	Serialise("", el.sets);
	Serialise("", el.nulls);
	Serialise("", el.types);
	Serialise("", el.bindslots);

	SIZE_CHECK(FetchFrameResourceBindStats, 48);
}

template<>
void Serialiser::Serialise(const char *name, FetchFrameUpdateStats &el)
{
	Serialise("", el.calls);
	Serialise("", el.clients);
	Serialise("", el.servers);
	Serialise("", el.types);
	Serialise("", el.sizes);

	SIZE_CHECK(FetchFrameUpdateStats, 48);
}

template<>
void Serialiser::Serialise(const char *name, FetchFrameDrawStats &el)
{
	Serialise("", el.calls);
	Serialise("", el.instanced);
	Serialise("", el.indirect);
	Serialise("", el.counts);

	SIZE_CHECK(FetchFrameDrawStats, 32);
}

template<>
void Serialiser::Serialise(const char *name, FetchFrameDispatchStats &el)
{
	Serialise("", el.calls);
	Serialise("", el.indirect);

	SIZE_CHECK(FetchFrameDispatchStats, 8);
}

template<>
void Serialiser::Serialise(const char *name, FetchFrameIndexBindStats &el)
{
	Serialise("", el.calls);
	Serialise("", el.sets);
	Serialise("", el.nulls);

	SIZE_CHECK(FetchFrameIndexBindStats, 12);
}

template<>
void Serialiser::Serialise(const char *name, FetchFrameVertexBindStats &el)
{
	Serialise("", el.calls);
	Serialise("", el.sets);
	Serialise("", el.nulls);
	Serialise("", el.bindslots);

	SIZE_CHECK(FetchFrameVertexBindStats, 32);
}

template<>
void Serialiser::Serialise(const char *name, FetchFrameLayoutBindStats &el)
{
	Serialise("", el.calls);
	Serialise("", el.sets);
	Serialise("", el.nulls);

	SIZE_CHECK(FetchFrameLayoutBindStats, 12);
}

template<>
void Serialiser::Serialise(const char *name, FetchFrameShaderStats &el)
{
	Serialise("", el.calls);
	Serialise("", el.sets);
	Serialise("", el.nulls);
	Serialise("", el.redundants);

	SIZE_CHECK(FetchFrameShaderStats, 16);
}

template<>
void Serialiser::Serialise(const char *name, FetchFrameBlendStats &el)
{
	Serialise("", el.calls);
	Serialise("", el.sets);
	Serialise("", el.nulls);
	Serialise("", el.redundants);

	SIZE_CHECK(FetchFrameBlendStats, 16);
}

template<>
void Serialiser::Serialise(const char *name, FetchFrameDepthStencilStats &el)
{
	Serialise("", el.calls);
	Serialise("", el.sets);
	Serialise("", el.nulls);
	Serialise("", el.redundants);

	SIZE_CHECK(FetchFrameDepthStencilStats, 16);
}

template<>
void Serialiser::Serialise(const char *name, FetchFrameRasterizationStats &el)
{
	Serialise("", el.calls);
	Serialise("", el.sets);
	Serialise("", el.nulls);
	Serialise("", el.redundants);
	Serialise("", el.viewports);
	Serialise("", el.rects);

	SIZE_CHECK(FetchFrameRasterizationStats, 48);
}

template<>
void Serialiser::Serialise(const char *name, FetchFrameOutputStats &el)
{
	Serialise("", el.calls);
	Serialise("", el.sets);
	Serialise("", el.nulls);
	Serialise("", el.bindslots);

	SIZE_CHECK(FetchFrameOutputStats, 32);
}

template<>
void Serialiser::Serialise(const char *name, FetchFrameStatistics &el)
{
	Serialise("", el.recorded);
	// #mivance note this is technically error-prone from the perspective
	// that we're passing references to pointers, but as we're really
	// dealing with arrays,t hey'll never be NULL and need to be assigned
	// to, so this is fine
	FetchFrameConstantBindStats* constants = el.constants;
	SerialiseComplexArray<eShaderStage_Count>("", constants);
	FetchFrameSamplerBindStats* samplers = el.samplers;
	SerialiseComplexArray<eShaderStage_Count>("", samplers);
	FetchFrameResourceBindStats* resources = el.resources;
	SerialiseComplexArray<eShaderStage_Count>("", resources);
	Serialise("", el.updates);
	Serialise("", el.draws);
	Serialise("", el.dispatches);
	Serialise("", el.indices);
	Serialise("", el.vertices);
	Serialise("", el.layouts);
	FetchFrameShaderStats* shaders = el.shaders;
	SerialiseComplexArray<eShaderStage_Count>("", shaders);
	Serialise("", el.blends);
	Serialise("", el.depths);
	Serialise("", el.rasters);
	Serialise("", el.outputs);

	SIZE_CHECK(FetchFrameStatistics, 1136);
}

template<>
void Serialiser::Serialise(const char *name, FetchFrameInfo &el)
{
	Serialise("", el.frameNumber);
	Serialise("", el.firstEvent);
	Serialise("", el.fileOffset);
	Serialise("", el.fileSize);
	Serialise("", el.persistentSize);
	Serialise("", el.initDataSize);
	Serialise("", el.captureTime);
	Serialise("", el.immContextId);
	Serialise("", el.stats);
	Serialise("", el.debugMessages);

	SIZE_CHECK(FetchFrameInfo, 1208);
}

template<>
void Serialiser::Serialise(const char *name, FetchFrameRecord &el)
{
	Serialise("", el.frameInfo);
	Serialise("", el.drawcallList);
	
	SIZE_CHECK(FetchFrameRecord, 1224);
}

template<>
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
	Serialise("", el.showAlpha);
	Serialise("", el.topo);
	Serialise("", el.numVerts);
	Serialise("", el.unproject);
	Serialise("", el.nearPlane);
	Serialise("", el.farPlane);

	SIZE_CHECK(MeshFormat, 88);
}

template<>
void Serialiser::Serialise(const char *name, CounterDescription &el)
{
	Serialise("", el.counterID);
	Serialise("", el.name);
	Serialise("", el.description);
	Serialise("", el.resultCompType);
	Serialise("", el.resultByteWidth);
	Serialise("", el.units);

	SIZE_CHECK(CounterDescription, 56);
}

template<>
void Serialiser::Serialise(const char *name, PixelModification &el)
{
	Serialise("", el.eventID);
	
	Serialise("", el.uavWrite);
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
	
	Serialise("", el.backfaceCulled);
	Serialise("", el.depthClipped);
	Serialise("", el.viewClipped);
	Serialise("", el.scissorClipped);
	Serialise("", el.shaderDiscarded);
	Serialise("", el.depthTestFailed);
	Serialise("", el.stencilTestFailed);

	SIZE_CHECK(PixelModification, 116);
}

#pragma endregion Data descriptors

#pragma region Ignored Enums

// don't need string representation of these enums
template<>
string ToStrHelper<false, SpecialFormat>::Get(const SpecialFormat &el) { return "<...>"; }
template<>
string ToStrHelper<false, FormatComponentType>::Get(const FormatComponentType &el) { return "<...>"; }
template<>
string ToStrHelper<false, CounterUnits>::Get(const CounterUnits &el) { return "<...>"; }
template<>
string ToStrHelper<false, PrimitiveTopology>::Get(const PrimitiveTopology &el) { return "<...>"; }
template<>
string ToStrHelper<false, ShaderStageType>::Get(const ShaderStageType &el) { return "<...>"; }
template<>
string ToStrHelper<false, ShaderStageBits>::Get(const ShaderStageBits &el) { return "<...>"; }
template<>
string ToStrHelper<false, ShaderBindType>::Get(const ShaderBindType &el) { return "<...>"; }
template<>
string ToStrHelper<false, ShaderResourceType>::Get(const ShaderResourceType &el) { return "<...>"; }
template<>
string ToStrHelper<false, DebugMessageCategory>::Get(const DebugMessageCategory &el) { return "<...>"; }
template<>
string ToStrHelper<false, DebugMessageSeverity>::Get(const DebugMessageSeverity &el) { return "<...>"; }
template<>
string ToStrHelper<false, DebugMessageSource>::Get(const DebugMessageSource &el) { return "<...>"; }
template<>
string ToStrHelper<false, VarType>::Get(const VarType &el) { return "<...>"; }
template<>
string ToStrHelper<false, MeshDataStage>::Get(const MeshDataStage &el) { return "<...>"; }
template<>
string ToStrHelper<false, TextureDisplayOverlay>::Get(const TextureDisplayOverlay &el) { return "<...>"; }
template<>
string ToStrHelper<false, APIPipelineStateType>::Get(const APIPipelineStateType &el) { return "<...>"; }

#pragma endregion Ignored Enums

#pragma region Plain-old data structures

// these structures we can just serialise as a blob, since they're POD.
template<>
string ToStrHelper<false, D3D11PipelineState::InputAssembler::VertexBuffer>::Get(const D3D11PipelineState::InputAssembler::VertexBuffer &el) { return "<...>"; }
template<>
string ToStrHelper<false, D3D11PipelineState::Rasterizer::RasterizerState>::Get(const D3D11PipelineState::Rasterizer::RasterizerState &el) { return "<...>"; }
template<>
string ToStrHelper<false, D3D11PipelineState::ShaderStage::CBuffer>::Get(const D3D11PipelineState::ShaderStage::CBuffer &el) { return "<...>"; }
template<>
string ToStrHelper<false, D3D11PipelineState::Rasterizer::Scissor>::Get(const D3D11PipelineState::Rasterizer::Scissor &el) { return "<...>"; }
template<>
string ToStrHelper<false, D3D11PipelineState::Rasterizer::Viewport>::Get(const D3D11PipelineState::Rasterizer::Viewport &el) { return "<...>"; }
template<>
string ToStrHelper<false, D3D11PipelineState::Streamout::Output>::Get(const D3D11PipelineState::Streamout::Output &el) { return "<...>"; }
template<>
string ToStrHelper<false, GLPipelineState::VertexInput::VertexBuffer>::Get(const GLPipelineState::VertexInput::VertexBuffer &el) { return "<...>"; }
template<>
string ToStrHelper<false, GLPipelineState::FixedVertexProcessing>::Get(const GLPipelineState::FixedVertexProcessing &el) { return "<...>"; }
template<>
string ToStrHelper<false, GLPipelineState::Texture>::Get(const GLPipelineState::Texture &el) { return "<...>"; }
template<>
string ToStrHelper<false, GLPipelineState::Buffer>::Get(const GLPipelineState::Buffer &el) { return "<...>"; }
template<>
string ToStrHelper<false, GLPipelineState::Feedback>::Get(const GLPipelineState::Feedback &el) { return "<...>"; }
template<>
string ToStrHelper<false, GLPipelineState::Rasterizer::Viewport>::Get(const GLPipelineState::Rasterizer::Viewport &el) { return "<...>"; }
template<>
string ToStrHelper<false, GLPipelineState::Rasterizer::Scissor>::Get(const GLPipelineState::Rasterizer::Scissor &el) { return "<...>"; }
template<>
string ToStrHelper<false, GLPipelineState::Rasterizer::RasterizerState>::Get(const GLPipelineState::Rasterizer::RasterizerState &el) { return "<...>"; }
template<>
string ToStrHelper<false, GLPipelineState::Hints>::Get(const GLPipelineState::Hints &el) { return "<...>"; }
template<>
string ToStrHelper<false, VulkanPipelineState::CurrentPass::RenderArea>::Get(const VulkanPipelineState::CurrentPass::RenderArea &el) { return "<...>"; }
template<>
string ToStrHelper<false, VulkanPipelineState::InputAssembly>::Get(const VulkanPipelineState::InputAssembly &el) { return "<...>"; }
template<>
string ToStrHelper<false, VulkanPipelineState::Tessellation>::Get(const VulkanPipelineState::Tessellation &el) { return "<...>"; }
template<>
string ToStrHelper<false, VulkanPipelineState::Raster>::Get(const VulkanPipelineState::Raster &el) { return "<...>"; }
template<>
string ToStrHelper<false, VulkanPipelineState::MultiSample>::Get(const VulkanPipelineState::MultiSample &el) { return "<...>"; }
template<>
string ToStrHelper<false, VulkanPipelineState::Pipeline::DescriptorSet::DescriptorBinding::BindingElement>::Get(const VulkanPipelineState::Pipeline::DescriptorSet::DescriptorBinding::BindingElement &el) { return "<...>"; }
template<>
string ToStrHelper<false, VulkanPipelineState::VertexInput::Binding>::Get(const VulkanPipelineState::VertexInput::Binding &el) { return "<...>"; }
template<>
string ToStrHelper<false, VulkanPipelineState::VertexInput::VertexBuffer>::Get(const VulkanPipelineState::VertexInput::VertexBuffer &el) { return "<...>"; }
template<>
string ToStrHelper<false, VulkanPipelineState::ViewState::ViewportScissor>::Get(const VulkanPipelineState::ViewState::ViewportScissor &el) { return "<...>"; }
template<>
string ToStrHelper<false, VulkanPipelineState::CurrentPass::Framebuffer::Attachment>::Get(const VulkanPipelineState::CurrentPass::Framebuffer::Attachment &el) { return "<...>"; }
template<>
string ToStrHelper<false, EventUsage>::Get(const EventUsage &el) { return "<...>"; }
template<>
string ToStrHelper<false, CounterResult>::Get(const CounterResult &el) { return "<...>"; }
template<>
string ToStrHelper<false, ReplayLogType>::Get(const ReplayLogType &el) { return "<...>"; }

#pragma endregion Plain-old data structures

ProxySerialiser::~ProxySerialiser()
{
	SAFE_DELETE(m_FromReplaySerialiser);
	SAFE_DELETE(m_ToReplaySerialiser);

	if(m_Proxy) m_Proxy->Shutdown();
	m_Proxy = NULL;

	for(auto it=m_ShaderReflectionCache.begin(); it != m_ShaderReflectionCache.end(); ++it)
		delete it->second;
}

bool ProxySerialiser::SendReplayCommand(CommandPacketType type)
{
	if(!SendPacket(m_Socket, type, *m_ToReplaySerialiser))
		return false;

	m_ToReplaySerialiser->Rewind();

	SAFE_DELETE(m_FromReplaySerialiser);

	if(!RecvPacket(m_Socket, type, &m_FromReplaySerialiser))
		return false;

	return true;
}

void ProxySerialiser::EnsureTexCached(ResourceId texid, uint32_t arrayIdx, uint32_t mip)
{
	TextureCacheEntry entry = { texid, arrayIdx, mip };

	if(m_LocalTextures.find(texid) != m_LocalTextures.end())
		return;
	
	if(m_TextureProxyCache.find(entry) == m_TextureProxyCache.end())
	{
		if(m_ProxyTextureIds.find(texid) == m_ProxyTextureIds.end())
		{
			FetchTexture tex = GetTexture(texid);
			m_ProxyTextureIds[texid] = m_Proxy->CreateProxyTexture(tex);
		}

		ResourceId proxyid = m_ProxyTextureIds[texid];
		
		size_t size;
		byte *data = GetTextureData(texid, arrayIdx, mip, false, false, 0.0f, 0.0f, size);

		if(data)
			m_Proxy->SetProxyTextureData(proxyid, arrayIdx, mip, data, size);
		
		delete[] data;

		m_TextureProxyCache.insert(entry);
	}
}

void ProxySerialiser::EnsureBufCached(ResourceId bufid)
{
	if(m_BufferProxyCache.find(bufid) == m_BufferProxyCache.end())
	{
		if(m_ProxyBufferIds.find(bufid) == m_ProxyBufferIds.end())
		{
			FetchBuffer buf = GetBuffer(bufid);
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

bool ProxySerialiser::Tick()
{
	if(!m_ReplayHost) return true;

	if(!m_Socket) return false;

	CommandPacketType type;

	if(!RecvPacket(m_Socket, type, &m_ToReplaySerialiser))
		return false;

	m_FromReplaySerialiser->Rewind();

	switch(type)
	{
		case eCommand_SetCtxFilter:
			SetContextFilter(ResourceId(), 0, 0);
			break;
		case eCommand_ReplayLog:
			ReplayLog(0, (ReplayLogType)0);
			break;
		case eCommand_GetPassEvents:
			GetPassEvents(0);
			break;
		case eCommand_GetAPIProperties:
			GetAPIProperties();
			break;
		case eCommand_GetTextures:
			GetTextures();
			break;
		case eCommand_GetTexture:
			GetTexture(ResourceId());
			break;
		case eCommand_GetBuffers:
			GetBuffers();
			break;
		case eCommand_GetBuffer:
			GetBuffer(ResourceId());
			break;
		case eCommand_GetShader:
			GetShader(ResourceId(), "");
			break;
		case eCommand_GetDebugMessages:
			GetDebugMessages();
			break;
		case eCommand_SavePipelineState:
			SavePipelineState();
			break;
		case eCommand_GetUsage:
			GetUsage(ResourceId());
			break;
		case eCommand_GetLiveID:
			GetLiveID(ResourceId());
			break;
		case eCommand_GetFrameRecord:
			GetFrameRecord();
			break;
		case eCommand_HasResolver:
			HasCallstacks();
			break;
		case eCommand_InitStackResolver:
			InitCallstackResolver();
			break;
		case eCommand_HasStackResolver:
			GetCallstackResolver();
			break;
		case eCommand_GetAddressDetails:
			GetAddr(0);
			break;
		case eCommand_FreeResource:
			FreeTargetResource(ResourceId());
			break;
		case eCommand_FetchCounters:
		{
			vector<uint32_t> counters;
			FetchCounters(counters);
			break;
		}
		case eCommand_EnumerateCounters:
			EnumerateCounters();
			break;
		case eCommand_DescribeCounter:
		{
			CounterDescription desc;
			DescribeCounter(0, desc);
			break;
		}
		case eCommand_FillCBufferVariables:
		{
			vector<ShaderVariable> vars;
			vector<byte> data;
			FillCBufferVariables(ResourceId(), "", 0, vars, data);
			break;
		}
		case eCommand_GetBufferData:
		{
			vector<byte> dummy;
			GetBufferData(ResourceId(), 0, 0, dummy);
			break;
		}
		case eCommand_GetTextureData:
		{
			size_t dummy;
			GetTextureData(ResourceId(), 0, 0, false, false, 0.0f, 0.0f, dummy);
			break;
		}
		case eCommand_InitPostVS:
			InitPostVSBuffers(0);
			break;
		case eCommand_InitPostVSVec:
		{
			vector<uint32_t> dummy;
			InitPostVSBuffers(dummy);
			break;
		}
		case eCommand_GetPostVS:
			GetPostVSBuffers(0, 0, eMeshDataStage_Unknown);
			break;
		case eCommand_BuildTargetShader:
			BuildTargetShader("", "", 0, eShaderStage_Vertex, NULL, NULL);
			break;
		case eCommand_ReplaceResource:
			ReplaceResource(ResourceId(), ResourceId());
			break;
		case eCommand_RemoveReplacement:
			RemoveReplacement(ResourceId());
			break;
		case eCommand_RenderOverlay:
			RenderOverlay(ResourceId(), eTexOverlay_None, 0, vector<uint32_t>());
			break;
		case eCommand_PixelHistory:
			PixelHistory(vector<EventUsage>(), ResourceId(), 0, 0, 0, 0, 0);
			break;
		case eCommand_DebugVertex:
			DebugVertex(0, 0, 0, 0, 0, 0);
			break;
		case eCommand_DebugPixel:
			DebugPixel(0, 0, 0, 0, 0);
			break;
		case eCommand_DebugThread:
		{
			uint32_t dummy1[3] = {0};
			uint32_t dummy2[3] = {0};
			DebugThread(0, dummy1, dummy2);
			break;
		}	
		default:
			RDCERR("Unexpected command");
			break;
	}

	SAFE_DELETE(m_ToReplaySerialiser);

	if(!SendPacket(m_Socket, type, *m_FromReplaySerialiser))
		return false;

	return true;
}

bool ProxySerialiser::IsRenderOutput(ResourceId id)
{
	// TODO this should go remote

	for(int32_t i=0; i < m_D3D11PipelineState.m_OM.RenderTargets.count; i++)
	{
		if(m_D3D11PipelineState.m_OM.RenderTargets[i].View == id ||
			 m_D3D11PipelineState.m_OM.RenderTargets[i].Resource == id)
				return true;
	}
	
	if(m_D3D11PipelineState.m_OM.DepthTarget.View == id ||
		 m_D3D11PipelineState.m_OM.DepthTarget.Resource == id)
			return true;

	return false;
}

APIProperties ProxySerialiser::GetAPIProperties()
{
	APIProperties ret;
	RDCEraseEl(ret);

	if(m_ReplayHost)
	{
		ret = m_Remote->GetAPIProperties();
	}
	else
	{
		if(!SendReplayCommand(eCommand_GetAPIProperties))
			return ret;
	}

	m_FromReplaySerialiser->Serialise("", ret);

	if(!m_ReplayHost)
	{
		m_APIProperties = ret;
	}

	return ret;
}

vector<ResourceId> ProxySerialiser::GetTextures()
{
	vector<ResourceId> ret;

	if(m_ReplayHost)
	{
		ret = m_Remote->GetTextures();
	}
	else
	{
		if(!SendReplayCommand(eCommand_GetTextures))
			return ret;
	}

	m_FromReplaySerialiser->Serialise("", ret);

	return ret;
}

vector<DebugMessage> ProxySerialiser::GetDebugMessages()
{
	vector<DebugMessage> ret;

	if(m_ReplayHost)
	{
		ret = m_Remote->GetDebugMessages();
	}
	else
	{
		if(!SendReplayCommand(eCommand_GetDebugMessages))
			return ret;
	}

	m_FromReplaySerialiser->Serialise("", ret);

	return ret;
}

FetchTexture ProxySerialiser::GetTexture(ResourceId id)
{
	FetchTexture ret;
	
	m_ToReplaySerialiser->Serialise("", id);

	if(m_ReplayHost)
	{
		ret = m_Remote->GetTexture(id);
	}
	else
	{
		if(!SendReplayCommand(eCommand_GetTexture))
			return ret;
	}

	m_FromReplaySerialiser->Serialise("", ret);

	return ret;
}

vector<ResourceId> ProxySerialiser::GetBuffers()
{
	vector<ResourceId> ret;

	if(m_ReplayHost)
	{
		ret = m_Remote->GetBuffers();
	}
	else
	{
		if(!SendReplayCommand(eCommand_GetBuffers))
			return ret;
	}

	m_FromReplaySerialiser->Serialise("", ret);

	return ret;
}

FetchBuffer ProxySerialiser::GetBuffer(ResourceId id)
{
	FetchBuffer ret;
	
	m_ToReplaySerialiser->Serialise("", id);

	if(m_ReplayHost)
	{
		ret = m_Remote->GetBuffer(id);
	}
	else
	{
		if(!SendReplayCommand(eCommand_GetBuffer))
			return ret;
	}

	m_FromReplaySerialiser->Serialise("", ret);

	return ret;
}


void ProxySerialiser::SavePipelineState()
{
	if(m_ReplayHost)
	{
		m_Remote->SavePipelineState();
		m_D3D11PipelineState = m_Remote->GetD3D11PipelineState();
		m_GLPipelineState = m_Remote->GetGLPipelineState();
		m_VulkanPipelineState = m_Remote->GetVulkanPipelineState();
	}
	else
	{
		if(!SendReplayCommand(eCommand_SavePipelineState))
			return;
		
		m_D3D11PipelineState = D3D11PipelineState();
		m_GLPipelineState = GLPipelineState();
		m_VulkanPipelineState = VulkanPipelineState();
	}

	m_FromReplaySerialiser->Serialise("", m_D3D11PipelineState);
	m_FromReplaySerialiser->Serialise("", m_GLPipelineState);
	m_FromReplaySerialiser->Serialise("", m_VulkanPipelineState);
}

void ProxySerialiser::SetContextFilter(ResourceId id, uint32_t firstDefEv, uint32_t lastDefEv)
{
	m_ToReplaySerialiser->Serialise("", id);
	m_ToReplaySerialiser->Serialise("", firstDefEv);
	m_ToReplaySerialiser->Serialise("", lastDefEv);
	
	if(m_ReplayHost)
	{
		m_Remote->SetContextFilter(id, firstDefEv, lastDefEv);
	}
	else
	{
		if(!SendReplayCommand(eCommand_SetCtxFilter))
			return;
	}
}

void ProxySerialiser::ReplayLog(uint32_t endEventID, ReplayLogType replayType)
{
	m_ToReplaySerialiser->Serialise("", endEventID);
	m_ToReplaySerialiser->Serialise("", replayType);
	
	if(m_ReplayHost)
	{
		m_Remote->ReplayLog(endEventID, replayType);
	}
	else
	{
		if(!SendReplayCommand(eCommand_ReplayLog))
			return;

		m_TextureProxyCache.clear();
		m_BufferProxyCache.clear();
	}
}

vector<uint32_t> ProxySerialiser::GetPassEvents(uint32_t eventID)
{
	vector<uint32_t> ret;
	
	m_ToReplaySerialiser->Serialise("", eventID);

	if(m_ReplayHost)
	{
		ret = m_Remote->GetPassEvents(eventID);
	}
	else
	{
		if(!SendReplayCommand(eCommand_GetPassEvents))
			return ret;
	}

	m_FromReplaySerialiser->Serialise("", ret);

	return ret;
}

vector<EventUsage> ProxySerialiser::GetUsage(ResourceId id)
{
	vector<EventUsage> ret;
	
	m_ToReplaySerialiser->Serialise("", id);

	if(m_ReplayHost)
	{
		ret = m_Remote->GetUsage(id);
	}
	else
	{
		if(!SendReplayCommand(eCommand_GetUsage))
			return ret;
	}

	m_FromReplaySerialiser->Serialise("", ret);

	return ret;
}

FetchFrameRecord ProxySerialiser::GetFrameRecord()
{
	FetchFrameRecord ret;

	if(m_ReplayHost)
	{
		ret = m_Remote->GetFrameRecord();
	}
	else
	{
		if(!SendReplayCommand(eCommand_GetFrameRecord))
			return ret;
	}

	m_FromReplaySerialiser->Serialise("", ret);

	return ret;
}

bool ProxySerialiser::HasCallstacks()
{
	bool ret = false;

	RDCASSERT(m_ReplayHost || m_ToReplaySerialiser->GetSize() == 0);

	if(m_ReplayHost)
	{
		ret = m_Remote->HasCallstacks();
	}
	else
	{
		if(!SendReplayCommand(eCommand_HasResolver))
			return ret;
	}
	
	RDCASSERT(!m_ReplayHost || m_FromReplaySerialiser->GetSize() == 0);
	
	m_FromReplaySerialiser->Serialise("", ret);

	return ret;
}

ResourceId ProxySerialiser::GetLiveID(ResourceId id)
{
	if(!m_ReplayHost && m_LiveIDs.find(id) != m_LiveIDs.end())
		return m_LiveIDs[id];
	
	if(!m_ReplayHost && m_LocalTextures.find(id) != m_LocalTextures.end())
		return id;

	ResourceId ret;

	RDCASSERT(m_ReplayHost || m_ToReplaySerialiser->GetSize() == 0);
	
	m_ToReplaySerialiser->Serialise("", id);

	if(m_ReplayHost)
	{
		ret = m_Remote->GetLiveID(id);
	}
	else
	{
		if(!SendReplayCommand(eCommand_GetLiveID))
			return ret;
	}
	
	RDCASSERT(!m_ReplayHost || m_FromReplaySerialiser->GetSize() == 0);
	
	m_FromReplaySerialiser->Serialise("", ret);

	if(!m_ReplayHost)
		m_LiveIDs[id] = ret;

	return ret;
}

vector<CounterResult> ProxySerialiser::FetchCounters(const vector<uint32_t> &counters)
{
	vector<CounterResult> ret;
	
	m_ToReplaySerialiser->Serialise("", (vector<uint32_t> &)counters);

	if(m_ReplayHost)
	{
		ret = m_Remote->FetchCounters(counters);
	}
	else
	{
		if(!SendReplayCommand(eCommand_FetchCounters))
			return ret;
	}
	
	m_FromReplaySerialiser->Serialise("", ret);

	return ret;
}

vector<uint32_t> ProxySerialiser::EnumerateCounters()
{
	vector<uint32_t> ret;
	
	if(m_ReplayHost)
	{
		ret = m_Remote->EnumerateCounters();
	}
	else
	{
		if(!SendReplayCommand(eCommand_EnumerateCounters))
			return ret;
	}

	m_FromReplaySerialiser->Serialise("", ret);

	return ret;
}

void ProxySerialiser::DescribeCounter(uint32_t counterID, CounterDescription &desc)
{
	m_ToReplaySerialiser->Serialise("", counterID);
	
	if(m_ReplayHost)
	{
		m_Remote->DescribeCounter(counterID, desc);
	}
	else
	{
		if(!SendReplayCommand(eCommand_DescribeCounter))
			return;
	}

	m_FromReplaySerialiser->Serialise("", desc);

	return;
}

void ProxySerialiser::FillCBufferVariables(ResourceId shader, string entryPoint, uint32_t cbufSlot, vector<ShaderVariable> &outvars, const vector<byte> &data)
{
	m_ToReplaySerialiser->Serialise("", shader);
	m_ToReplaySerialiser->Serialise("", entryPoint);
	m_ToReplaySerialiser->Serialise("", cbufSlot);
	m_ToReplaySerialiser->Serialise("", outvars);
	m_ToReplaySerialiser->Serialise("", (vector<byte> &)data);

	if(m_ReplayHost)
	{
		m_Remote->FillCBufferVariables(shader, entryPoint, cbufSlot, outvars, data);
	}
	else
	{
		if(!SendReplayCommand(eCommand_FillCBufferVariables))
			return;
	}
	
	m_FromReplaySerialiser->Serialise("", outvars);

	return;
}

void ProxySerialiser::GetBufferData(ResourceId buff, uint64_t offset, uint64_t len, vector<byte> &retData)
{
	m_ToReplaySerialiser->Serialise("", buff);
	m_ToReplaySerialiser->Serialise("", offset);
	m_ToReplaySerialiser->Serialise("", len);
	
	if(m_ReplayHost)
	{
		m_Remote->GetBufferData(buff, offset, len, retData);
	
		size_t sz = retData.size();
		m_FromReplaySerialiser->Serialise("", sz);
		m_FromReplaySerialiser->RawWriteBytes(&retData[0], sz);
	}
	else
	{
		if(!SendReplayCommand(eCommand_GetBufferData))
			return;
	
		size_t sz = 0;
		m_FromReplaySerialiser->Serialise("", sz);
		retData.resize(sz);
		memcpy(&retData[0], m_FromReplaySerialiser->RawReadBytes(sz), sz);
	}
}

byte *ProxySerialiser::GetTextureData(ResourceId tex, uint32_t arrayIdx, uint32_t mip, bool resolve, bool forceRGBA8unorm,
                                      float blackPoint, float whitePoint, size_t &dataSize)
{
	m_ToReplaySerialiser->Serialise("", tex);
	m_ToReplaySerialiser->Serialise("", arrayIdx);
	m_ToReplaySerialiser->Serialise("", mip);
	m_ToReplaySerialiser->Serialise("", resolve);
	m_ToReplaySerialiser->Serialise("", forceRGBA8unorm);
	m_ToReplaySerialiser->Serialise("", blackPoint);
	m_ToReplaySerialiser->Serialise("", whitePoint);

	if(m_ReplayHost)
	{
		byte *data = m_Remote->GetTextureData(tex, arrayIdx, mip, resolve, forceRGBA8unorm, blackPoint, whitePoint, dataSize);

		byte *compressed = new byte[dataSize+512];
		
		size_t compressedSize = (size_t)LZ4_compress((const char *)data, (char *)compressed, (int)dataSize);
	
		m_FromReplaySerialiser->Serialise("", dataSize);
		m_FromReplaySerialiser->Serialise("", compressedSize);
		m_FromReplaySerialiser->RawWriteBytes(compressed, compressedSize);

		delete[] data;
		delete[] compressed;
	}
	else
	{
		if(!SendReplayCommand(eCommand_GetTextureData))
			return NULL;

		size_t compressedSize;

		m_FromReplaySerialiser->Serialise("", dataSize);
		m_FromReplaySerialiser->Serialise("", compressedSize);

		byte *ret = new byte[dataSize+512];

		byte *compressed = (byte *)m_FromReplaySerialiser->RawReadBytes(compressedSize);

		LZ4_decompress_fast((const char *)compressed, (char *)ret, (int)dataSize);

		return ret;
	}

	return NULL;
}

void ProxySerialiser::InitPostVSBuffers(uint32_t eventID)
{
	m_ToReplaySerialiser->Serialise("", eventID);
	
	if(m_ReplayHost)
	{
		m_Remote->InitPostVSBuffers(eventID);
	}
	else
	{
		if(!SendReplayCommand(eCommand_InitPostVS))
			return;
	}
}

void ProxySerialiser::InitPostVSBuffers(const vector<uint32_t> &events)
{
	m_ToReplaySerialiser->Serialise("", (vector<uint32_t> &)events);
	
	if(m_ReplayHost)
	{
		m_Remote->InitPostVSBuffers(events);
	}
	else
	{
		if(!SendReplayCommand(eCommand_InitPostVSVec))
			return;
	}
}

MeshFormat ProxySerialiser::GetPostVSBuffers(uint32_t eventID, uint32_t instID, MeshDataStage stage)
{
	MeshFormat ret;
	
	m_ToReplaySerialiser->Serialise("", eventID);
	m_ToReplaySerialiser->Serialise("", instID);
	m_ToReplaySerialiser->Serialise("", stage);

	if(m_ReplayHost)
	{
		ret = m_Remote->GetPostVSBuffers(eventID, instID, stage);
	}
	else
	{
		if(!SendReplayCommand(eCommand_GetPostVS))
			return ret;
	}

	m_FromReplaySerialiser->Serialise("", ret);

	return ret;
}

ResourceId ProxySerialiser::RenderOverlay(ResourceId texid, TextureDisplayOverlay overlay, uint32_t eventID, const vector<uint32_t> &passEvents)
{
	ResourceId ret;

	vector<uint32_t> events = passEvents;

	m_ToReplaySerialiser->Serialise("", texid);
	m_ToReplaySerialiser->Serialise("", overlay);
	m_ToReplaySerialiser->Serialise("", eventID);
	m_ToReplaySerialiser->Serialise("", events);

	if(m_ReplayHost)
	{
		ret = m_Remote->RenderOverlay(texid, overlay, eventID, events);
	}
	else
	{
		if(!SendReplayCommand(eCommand_RenderOverlay))
			return ret;
	}

	m_FromReplaySerialiser->Serialise("", ret);

	return ret;
}

ShaderReflection *ProxySerialiser::GetShader(ResourceId id, string entryPoint)
{
	if(m_ReplayHost)
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

		if(!SendReplayCommand(eCommand_GetShader))
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

void ProxySerialiser::FreeTargetResource(ResourceId id)
{
	m_ToReplaySerialiser->Serialise("", id);

	if(m_ReplayHost)
	{
		m_Remote->FreeTargetResource(id);
	}
	else
	{
		if(!SendReplayCommand(eCommand_FreeResource))
			return;
	}
}

void ProxySerialiser::InitCallstackResolver()
{
	if(m_ReplayHost)
	{
		m_Remote->InitCallstackResolver();
	}
	else
	{
		if(!SendReplayCommand(eCommand_InitStackResolver))
			return;
	}
}

Callstack::StackResolver *ProxySerialiser::GetCallstackResolver()
{
	if(m_RemoteHasResolver) return this;

	bool remoteHasResolver = false;

	if(m_ReplayHost)
	{
		remoteHasResolver = m_Remote->GetCallstackResolver() != NULL;
	}
	else
	{
		if(!SendReplayCommand(eCommand_HasStackResolver))
			return NULL;
	}

	m_FromReplaySerialiser->Serialise("", remoteHasResolver);

	if(remoteHasResolver)
	{
		if(!m_ReplayHost)
			m_RemoteHasResolver = true;

		return this;
	}

	return NULL;
}

Callstack::AddressDetails ProxySerialiser::GetAddr(uint64_t addr)
{
	Callstack::AddressDetails ret;

	if(m_ReplayHost)
	{
		Callstack::StackResolver *resolv = m_Remote->GetCallstackResolver();
		if(resolv) ret = resolv->GetAddr(addr);
	}
	else
	{
		if(!SendReplayCommand(eCommand_HasStackResolver))
			return ret;
	}

	m_FromReplaySerialiser->Serialise("", ret.filename);
	m_FromReplaySerialiser->Serialise("", ret.function);
	m_FromReplaySerialiser->Serialise("", ret.line);

	return ret;
}

void ProxySerialiser::BuildTargetShader(string source, string entry, const uint32_t compileFlags, ShaderStageType type, ResourceId *id, string *errors)
{
	uint32_t flags = compileFlags;
	m_ToReplaySerialiser->Serialise("", source);
	m_ToReplaySerialiser->Serialise("", entry);
	m_ToReplaySerialiser->Serialise("", flags);
	m_ToReplaySerialiser->Serialise("", type);
	
	ResourceId outId;
	string outErrs;

	if(m_ReplayHost)
	{
		m_Remote->BuildTargetShader(source, entry, flags, type, &outId, &outErrs);
	}
	else
	{
		if(!SendReplayCommand(eCommand_BuildTargetShader))
			return;
	}

	m_FromReplaySerialiser->Serialise("", outId);
	m_FromReplaySerialiser->Serialise("", outErrs);

	if(!m_ReplayHost)
	{
		if(id) *id = outId;
		if(errors) *errors = outErrs;
	}
}

void ProxySerialiser::ReplaceResource(ResourceId from, ResourceId to)
{
	m_ToReplaySerialiser->Serialise("", from);
	m_ToReplaySerialiser->Serialise("", to);

	if(m_ReplayHost)
	{
		m_Remote->ReplaceResource(from, to);
	}
	else
	{
		if(!SendReplayCommand(eCommand_ReplaceResource))
			return;
	}
}

void ProxySerialiser::RemoveReplacement(ResourceId id)
{
	m_ToReplaySerialiser->Serialise("", id);

	if(m_ReplayHost)
	{
		m_Remote->RemoveReplacement(id);
	}
	else
	{
		if(!SendReplayCommand(eCommand_RemoveReplacement))
			return;
	}
}

vector<PixelModification> ProxySerialiser::PixelHistory(vector<EventUsage> events, ResourceId target, uint32_t x, uint32_t y, uint32_t slice, uint32_t mip, uint32_t sampleIdx)
{
	vector<PixelModification> ret;
	
	m_ToReplaySerialiser->Serialise("", events);
	m_ToReplaySerialiser->Serialise("", target);
	m_ToReplaySerialiser->Serialise("", x);
	m_ToReplaySerialiser->Serialise("", y);
	m_ToReplaySerialiser->Serialise("", slice);
	m_ToReplaySerialiser->Serialise("", mip);
	m_ToReplaySerialiser->Serialise("", sampleIdx);

	if(m_ReplayHost)
	{
		ret = m_Remote->PixelHistory(events, target, x, y, slice, mip, sampleIdx);
	}
	else
	{
		if(!SendReplayCommand(eCommand_PixelHistory))
			return ret;
	}

	m_FromReplaySerialiser->Serialise("", ret);

	return ret;
}

ShaderDebugTrace ProxySerialiser::DebugVertex(uint32_t eventID, uint32_t vertid, uint32_t instid, uint32_t idx, uint32_t instOffset, uint32_t vertOffset)
{
	ShaderDebugTrace ret;
	
	m_ToReplaySerialiser->Serialise("", eventID);
	m_ToReplaySerialiser->Serialise("", vertid);
	m_ToReplaySerialiser->Serialise("", instid);
	m_ToReplaySerialiser->Serialise("", idx);
	m_ToReplaySerialiser->Serialise("", instOffset);
	m_ToReplaySerialiser->Serialise("", vertOffset);

	if(m_ReplayHost)
	{
		ret = m_Remote->DebugVertex(eventID, vertid, instid, idx, instOffset, vertOffset);
	}
	else
	{
		if(!SendReplayCommand(eCommand_DebugVertex))
			return ret;
	}

	m_FromReplaySerialiser->Serialise("", ret);

	return ret;
}

ShaderDebugTrace ProxySerialiser::DebugPixel(uint32_t eventID, uint32_t x, uint32_t y, uint32_t sample, uint32_t primitive)
{
	ShaderDebugTrace ret;
	
	m_ToReplaySerialiser->Serialise("", eventID);
	m_ToReplaySerialiser->Serialise("", x);
	m_ToReplaySerialiser->Serialise("", y);
	m_ToReplaySerialiser->Serialise("", sample);
	m_ToReplaySerialiser->Serialise("", primitive);

	if(m_ReplayHost)
	{
		ret = m_Remote->DebugPixel(eventID, x, y, sample, primitive);
	}
	else
	{
		if(!SendReplayCommand(eCommand_DebugPixel))
			return ret;
	}

	m_FromReplaySerialiser->Serialise("", ret);

	return ret;
}

ShaderDebugTrace ProxySerialiser::DebugThread(uint32_t eventID, uint32_t groupid[3], uint32_t threadid[3])
{
	ShaderDebugTrace ret;
	
	m_ToReplaySerialiser->Serialise("", eventID);
	m_ToReplaySerialiser->SerialisePODArray<3>("", groupid);
	m_ToReplaySerialiser->SerialisePODArray<3>("", threadid);

	if(m_ReplayHost)
	{
		ret = m_Remote->DebugThread(eventID, groupid, threadid);
	}
	else
	{
		if(!SendReplayCommand(eCommand_DebugThread))
			return ret;
	}

	m_FromReplaySerialiser->Serialise("", ret);

	return ret;
}
