/******************************************************************************
 * The MIT License (MIT)
 * 
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

#if defined(WIN32) && defined(_M_IX86)
template<size_t a, size_t e> class oversized { int check[int(a)-int(e)+1]; };
template<size_t a, size_t e> class undersized { int check[int(e)-int(a)+1]; };

#define SIZE_CHECK(T, expected) undersized<sizeof(T), expected>(); oversized<sizeof(T), expected>();
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
	Serialise("", el.srgbCorrected);

	SIZE_CHECK(ResourceFormat, 36);
}

template<>
void Serialiser::Serialise(const char *name, BindpointMap &el)
{
	Serialise("", el.bind);
	Serialise("", el.used);

	SIZE_CHECK(BindpointMap, 8);
}

template<>
void Serialiser::Serialise(const char *name, ShaderBindpointMapping &el)
{
	Serialise("", el.InputAttributes);
	Serialise("", el.ConstantBlocks);
	Serialise("", el.Resources);

	SIZE_CHECK(ShaderBindpointMapping, 24);
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

	SIZE_CHECK(SigParameter, 56);
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
	Serialise("", el.members);

	SIZE_CHECK(ShaderVariableType, 36);
}

template<>
void Serialiser::Serialise(const char *name, ShaderConstant &el)
{
	Serialise("", el.name);
	Serialise("", el.reg.vec);
	Serialise("", el.reg.comp);
	Serialise("", el.type);

	SIZE_CHECK(ShaderConstant, 52);
}

template<>
void Serialiser::Serialise(const char *name, ConstantBlock &el)
{
	Serialise("", el.name);
	Serialise("", el.variables);
	Serialise("", el.bufferBacked);
	Serialise("", el.bindPoint);

	SIZE_CHECK(ConstantBlock, 24);
}

template<>
void Serialiser::Serialise(const char *name, ShaderResource &el)
{
	Serialise("", el.IsSampler);
	Serialise("", el.IsTexture);
	Serialise("", el.IsSRV);
	Serialise("", el.IsReadWrite);
	Serialise("", el.resType);
	Serialise("", el.name);
	Serialise("", el.variableType);
	Serialise("", el.bindPoint);

	SIZE_CHECK(ShaderResource, 68);
}

template<>
void Serialiser::Serialise(const char *name, ShaderReflection &el)
{
	Serialise("", el.DebugInfo.compileFlags);
	Serialise("", el.DebugInfo.entryFunc);
	Serialise("", el.DebugInfo.files);

	Serialise<3>("", el.DispatchThreadsDimension);

	Serialise("", el.Disassembly);
	
	Serialise("", el.InputSig);
	Serialise("", el.OutputSig);

	Serialise("", el.ConstantBlocks);

	Serialise("", el.Resources);

	Serialise("", el.Interfaces);

	SIZE_CHECK(ShaderReflection, 84);
}

template<>
void Serialiser::Serialise(const char *name, ShaderVariable &el)
{
	Serialise("", el.rows);
	Serialise("", el.columns);
	Serialise("", el.name);
	Serialise("", el.type);

	Serialise<16>("", el.value.dv);
	
	Serialise("", el.isStruct);
	
	Serialise("", el.members);

	SIZE_CHECK(ShaderVariable, 168);
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

	SIZE_CHECK(ShaderDebugState, 28);
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

	SIZE_CHECK(ShaderDebugTrace, 24);
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

	SIZE_CHECK(D3D11PipelineState::InputAssembler::LayoutInput, 64);
}

template<>
void Serialiser::Serialise(const char *name, D3D11PipelineState::InputAssembler &el)
{
	Serialise("", el.ibuffer.Buffer);
	Serialise("", el.ibuffer.Offset);
	
	Serialise("", el.vbuffers);
	Serialise("", el.layouts);

	SIZE_CHECK(D3D11PipelineState::InputAssembler, 48);
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

	SIZE_CHECK(D3D11PipelineState::ShaderStage::ResourceView, 104);
}

template<>
void Serialiser::Serialise(const char *name, D3D11PipelineState::ShaderStage::Sampler &el)
{
	Serialise("", el.Samp);
	Serialise("", el.AddressU);
	Serialise("", el.AddressV);
	Serialise("", el.AddressW);
	Serialise<4>("", el.BorderColor);
	Serialise("", el.Comparison);
	Serialise("", el.Filter);
	Serialise("", el.UseBorder);
	Serialise("", el.UseComparison);
	Serialise("", el.MaxAniso);
	Serialise("", el.MaxLOD);
	Serialise("", el.MinLOD);
	Serialise("", el.MipLODBias);

	SIZE_CHECK(D3D11PipelineState::ShaderStage::Sampler, 88);
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

	SIZE_CHECK(D3D11PipelineState::ShaderStage, 96);
}

template<>
void Serialiser::Serialise(const char *name, D3D11PipelineState::Rasterizer &el)
{
	Serialise("", el.m_State);
	Serialise("", el.Scissors);
	Serialise("", el.Viewports);

	SIZE_CHECK(D3D11PipelineState::Rasterizer, 72);
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

	SIZE_CHECK(D3D11PipelineState::OutputMerger::BlendState::RTBlend, 68);
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
		Serialise<4>("", el.m_BlendState.BlendFactor);

		Serialise("", el.m_BlendState.SampleMask);
	}

	Serialise("", el.RenderTargets);
	Serialise("", el.UAVStartSlot);
	Serialise("", el.UAVs);
	Serialise("", el.DepthTarget);
	Serialise("", el.DepthReadOnly);
	Serialise("", el.StencilReadOnly);

	SIZE_CHECK(D3D11PipelineState::OutputMerger, 288);
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

	SIZE_CHECK(D3D11PipelineState, 992);
}

#pragma endregion D3D11 pipeline state

#pragma region OpenGL pipeline state

template<>
void Serialiser::Serialise(const char *name, GLPipelineState::VertexInput::VertexAttribute &el)
{
	Serialise("", el.Enabled);
	Serialise("", el.Format);
	Serialise<4>("", el.GenericValue.f);
	Serialise("", el.BufferSlot);
	Serialise("", el.RelativeOffset);

	SIZE_CHECK(GLPipelineState::VertexInput::VertexAttribute, 64);
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

	SIZE_CHECK(GLPipelineState::VertexInput, 40);
}

template<>
void Serialiser::Serialise(const char *name, GLPipelineState::ShaderStage &el)
{
	Serialise("", el.Shader);
	Serialise("", el.stage);
	Serialise("", el.BindpointMapping);
	Serialise("", el.Subroutines);

	if(m_Mode == READING)
		el.ShaderDetails = NULL;

	SIZE_CHECK(GLPipelineState::ShaderStage, 48);
}

template<>
void Serialiser::Serialise(const char *name, GLPipelineState::Sampler &el)
{
	Serialise("", el.Samp);
	Serialise("", el.AddressS);
	Serialise("", el.AddressT);
	Serialise("", el.AddressR);
	Serialise<4>("", el.BorderColor);
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

	SIZE_CHECK(GLPipelineState::Sampler, 104);
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

	SIZE_CHECK(GLPipelineState::ImageLoadStore, 72);
}

template<>
void Serialiser::Serialise(const char *name, GLPipelineState::Rasterizer &el)
{
	Serialise("", el.Viewports);
	Serialise("", el.Scissors);
	Serialise("", el.m_State);

	SIZE_CHECK(GLPipelineState::Rasterizer, 104);
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

	SIZE_CHECK(GLPipelineState::DepthState, 40);
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

	SIZE_CHECK(GLPipelineState::StencilState, 92);
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

	SIZE_CHECK(GLPipelineState::FrameBuffer::BlendState::RTBlend, 64);
}

template<>
void Serialiser::Serialise(const char *name, GLPipelineState::FrameBuffer::BlendState &el)
{
	Serialise<4>("", el.BlendFactor);
	Serialise("", el.Blends);

	SIZE_CHECK(GLPipelineState::FrameBuffer::BlendState, 24);
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

	SIZE_CHECK(GLPipelineState::FrameBuffer, 160);
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

	SIZE_CHECK(GLPipelineState, 984);
}

#pragma endregion OpenGL pipeline state

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

	SIZE_CHECK(FetchTexture, 120);
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

	SIZE_CHECK(FetchBuffer, 40);
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

	SIZE_CHECK(DebugMessage, 28);
}

template<>
void Serialiser::Serialise(const char *name, FetchAPIEvent &el)
{
	Serialise("", el.eventID);
	Serialise("", el.context);
	Serialise("", el.callstack);
	Serialise("", el.eventDesc);
	Serialise("", el.fileOffset);

	SIZE_CHECK(FetchAPIEvent, 40);
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
	Serialise("", el.indexOffset);
	Serialise("", el.vertexOffset);
	Serialise("", el.instanceOffset);

	Serialise<3>("", el.dispatchDimension);
	Serialise<3>("", el.dispatchThreadsDimension);

	Serialise("", el.indexByteWidth);
	Serialise("", el.topology);

	Serialise("", el.context);
	
	Serialise("", el.parent);
	Serialise("", el.previous);
	Serialise("", el.next);
	
	Serialise<8>("", el.outputs);
	Serialise("", el.depthOut);
	
	Serialise("", el.events);
	Serialise("", el.children);

	SIZE_CHECK(FetchDrawcall, 192);
}

template<>
void Serialiser::Serialise(const char *name, FetchFrameInfo &el)
{
	Serialise("", el.frameNumber);
	Serialise("", el.firstEvent);
	Serialise("", el.fileOffset);
	Serialise("", el.captureTime);
	Serialise("", el.immContextId);
	Serialise("", el.debugMessages);

	SIZE_CHECK(FetchFrameInfo, 40);
}

template<>
void Serialiser::Serialise(const char *name, FetchFrameRecord &el)
{
	Serialise("", el.frameInfo);
	Serialise("", el.drawcallList);

	SIZE_CHECK(FetchFrameRecord, 56);
}

template<>
void Serialiser::Serialise(const char *name, MeshFormat &el)
{
	Serialise("", el.idxbuf);
	Serialise("", el.idxoffs);
	Serialise("", el.idxByteWidth);
	Serialise("", el.buf);
	Serialise("", el.offset);
	Serialise("", el.stride);
	Serialise("", el.compCount);
	Serialise("", el.compByteWidth);
	Serialise("", el.compType);
	Serialise("", el.specialFormat);
	Serialise("", el.showAlpha);
	Serialise("", el.topo);
	Serialise("", el.numVerts);
	Serialise("", el.unproject);
	Serialise("", el.nearPlane);
	Serialise("", el.farPlane);

	SIZE_CHECK(MeshFormat, 72);
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

	SIZE_CHECK(CounterDescription, 32);
}

template<>
void Serialiser::Serialise(const char *name, PixelModification &el)
{
	Serialise("", el.eventID);
	
	Serialise("", el.uavWrite);
	Serialise("", el.fragIndex);
	Serialise("", el.primitiveID);

	Serialise<4>("", el.preMod.col.value_u);
	Serialise("", el.preMod.depth);
	Serialise("", el.preMod.stencil);
	Serialise<4>("", el.shaderOut.col.value_u);
	Serialise("", el.shaderOut.depth);
	Serialise("", el.shaderOut.stencil);
	Serialise<4>("", el.postMod.col.value_u);
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

		vector<byte> data = GetBufferData(bufid, 0, 0);

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
			ReplayLog(0, 0, 0, (ReplayLogType)0);
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
			GetShader(ResourceId());
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
			FetchCounters(0, 0, 0, counters);
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
			FillCBufferVariables(ResourceId(), 0, vars, data);
			break;
		}
		case eCommand_GetBufferData:
			GetBufferData(ResourceId(), 0, 0);
			break;
		case eCommand_GetTextureData:
		{
			size_t dummy;
			GetTextureData(ResourceId(), 0, 0, false, false, 0.0f, 0.0f, dummy);
			break;
		}
		case eCommand_InitPostVS:
			InitPostVSBuffers(0, 0);
			break;
		case eCommand_GetPostVS:
			GetPostVSBuffers(0, 0, 0, eMeshDataStage_Unknown);
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
			RenderOverlay(ResourceId(), eTexOverlay_None, 0, 0, vector<uint32_t>());
			break;
		case eCommand_PixelHistory:
			PixelHistory(0, vector<EventUsage>(), ResourceId(), 0, 0, 0, 0, 0);
			break;
		case eCommand_DebugVertex:
			DebugVertex(0, 0, 0, 0, 0, 0, 0);
			break;
		case eCommand_DebugPixel:
			DebugPixel(0, 0, 0, 0, 0, 0);
			break;
		case eCommand_DebugThread:
		{
			uint32_t dummy1[3] = {0};
			uint32_t dummy2[3] = {0};
			DebugThread(0, 0, dummy1, dummy2);
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
	}
	else
	{
		if(!SendReplayCommand(eCommand_SavePipelineState))
			return;
		
		m_D3D11PipelineState = D3D11PipelineState();
		m_GLPipelineState = GLPipelineState();
	}

	m_FromReplaySerialiser->Serialise("", m_D3D11PipelineState);
	m_FromReplaySerialiser->Serialise("", m_GLPipelineState);
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

void ProxySerialiser::ReplayLog(uint32_t frameID, uint32_t startEventID, uint32_t endEventID, ReplayLogType replayType)
{
	m_ToReplaySerialiser->Serialise("", frameID);
	m_ToReplaySerialiser->Serialise("", startEventID);
	m_ToReplaySerialiser->Serialise("", endEventID);
	m_ToReplaySerialiser->Serialise("", replayType);
	
	if(m_ReplayHost)
	{
		m_Remote->ReplayLog(frameID, startEventID, endEventID, replayType);
	}
	else
	{
		if(!SendReplayCommand(eCommand_ReplayLog))
			return;

		m_TextureProxyCache.clear();
		m_BufferProxyCache.clear();
	}
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

vector<FetchFrameRecord> ProxySerialiser::GetFrameRecord()
{
	vector<FetchFrameRecord> ret;

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

vector<CounterResult> ProxySerialiser::FetchCounters(uint32_t frameID, uint32_t minEventID, uint32_t maxEventID, const vector<uint32_t> &counters)
{
	vector<CounterResult> ret;
	
	m_ToReplaySerialiser->Serialise("", frameID);
	m_ToReplaySerialiser->Serialise("", minEventID);
	m_ToReplaySerialiser->Serialise("", maxEventID);
	m_ToReplaySerialiser->Serialise("", (vector<uint32_t> &)counters);

	if(m_ReplayHost)
	{
		ret = m_Remote->FetchCounters(frameID, minEventID, maxEventID, counters);
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

void ProxySerialiser::FillCBufferVariables(ResourceId shader, uint32_t cbufSlot, vector<ShaderVariable> &outvars, const vector<byte> &data)
{
	m_ToReplaySerialiser->Serialise("", shader);
	m_ToReplaySerialiser->Serialise("", cbufSlot);
	m_ToReplaySerialiser->Serialise("", outvars);
	m_ToReplaySerialiser->Serialise("", (vector<byte> &)data);

	if(m_ReplayHost)
	{
		m_Remote->FillCBufferVariables(shader, cbufSlot, outvars, data);
	}
	else
	{
		if(!SendReplayCommand(eCommand_FillCBufferVariables))
			return;
	}
	
	m_FromReplaySerialiser->Serialise("", shader);
	m_FromReplaySerialiser->Serialise("", cbufSlot);
	m_FromReplaySerialiser->Serialise("", outvars);

	return;
}

vector<byte> ProxySerialiser::GetBufferData(ResourceId buff, uint32_t offset, uint32_t len)
{
	vector<byte> ret;
	
	m_ToReplaySerialiser->Serialise("", buff);
	m_ToReplaySerialiser->Serialise("", offset);
	m_ToReplaySerialiser->Serialise("", len);
	
	if(m_ReplayHost)
	{
		ret = m_Remote->GetBufferData(buff, offset, len);
	
		size_t sz = ret.size();
		m_FromReplaySerialiser->Serialise("", sz);
		m_FromReplaySerialiser->RawWriteBytes(&ret[0], sz);
	}
	else
	{
		if(!SendReplayCommand(eCommand_GetBufferData))
			return ret;
	
		size_t sz = 0;
		m_FromReplaySerialiser->Serialise("", sz);
		ret.resize(sz);
		memcpy(&ret[0], m_FromReplaySerialiser->RawReadBytes(sz), sz);
	}

	return ret;
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

		size_t uncompSize = (size_t)LZ4_decompress_fast((const char *)compressed, (char *)ret, (int)dataSize);

		return ret;
	}

	return NULL;
}

void ProxySerialiser::InitPostVSBuffers(uint32_t frameID, uint32_t eventID)
{
	m_ToReplaySerialiser->Serialise("", frameID);
	m_ToReplaySerialiser->Serialise("", eventID);
	
	if(m_ReplayHost)
	{
		m_Remote->InitPostVSBuffers(frameID, eventID);
	}
	else
	{
		if(!SendReplayCommand(eCommand_InitPostVS))
			return;
	}
}

MeshFormat ProxySerialiser::GetPostVSBuffers(uint32_t frameID, uint32_t eventID, uint32_t instID, MeshDataStage stage)
{
	MeshFormat ret;
	
	m_ToReplaySerialiser->Serialise("", frameID);
	m_ToReplaySerialiser->Serialise("", eventID);
	m_ToReplaySerialiser->Serialise("", instID);
	m_ToReplaySerialiser->Serialise("", stage);

	if(m_ReplayHost)
	{
		ret = m_Remote->GetPostVSBuffers(frameID, eventID, instID, stage);
	}
	else
	{
		if(!SendReplayCommand(eCommand_GetPostVS))
			return ret;
	}

	m_FromReplaySerialiser->Serialise("", ret);

	return ret;
}

ResourceId ProxySerialiser::RenderOverlay(ResourceId texid, TextureDisplayOverlay overlay, uint32_t frameID, uint32_t eventID, const vector<uint32_t> &passEvents)
{
	ResourceId ret;

	vector<uint32_t> events = passEvents;

	m_ToReplaySerialiser->Serialise("", texid);
	m_ToReplaySerialiser->Serialise("", overlay);
	m_ToReplaySerialiser->Serialise("", frameID);
	m_ToReplaySerialiser->Serialise("", eventID);
	m_ToReplaySerialiser->Serialise("", events);

	if(m_ReplayHost)
	{
		ret = m_Remote->RenderOverlay(texid, overlay, frameID, eventID, events);
	}
	else
	{
		if(!SendReplayCommand(eCommand_RenderOverlay))
			return ret;
	}

	m_FromReplaySerialiser->Serialise("", ret);

	return ret;
}

ShaderReflection *ProxySerialiser::GetShader(ResourceId id)
{
	if(m_ReplayHost)
	{
		m_ToReplaySerialiser->Serialise("", id);

		ShaderReflection *refl = m_Remote->GetShader(id);

		bool hasrefl = (refl != NULL);
		m_FromReplaySerialiser->Serialise("", hasrefl);

		if(hasrefl)
			m_FromReplaySerialiser->Serialise("", *refl);

		return NULL;
	}

	if(m_ShaderReflectionCache.find(id) == m_ShaderReflectionCache.end())
	{
		m_ToReplaySerialiser->Serialise("", id);

		if(!SendReplayCommand(eCommand_GetShader))
			return NULL;

		bool hasrefl = false;
		m_FromReplaySerialiser->Serialise("", hasrefl);

		if(hasrefl)
		{
			m_ShaderReflectionCache[id] = new ShaderReflection();

			m_FromReplaySerialiser->Serialise("", *m_ShaderReflectionCache[id]);
		}
		else
		{
			m_ShaderReflectionCache[id] = NULL;
		}
	}

	return m_ShaderReflectionCache[id];
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

vector<PixelModification> ProxySerialiser::PixelHistory(uint32_t frameID, vector<EventUsage> events, ResourceId target, uint32_t x, uint32_t y, uint32_t slice, uint32_t mip, uint32_t sampleIdx)
{
	vector<PixelModification> ret;
	
	m_ToReplaySerialiser->Serialise("", frameID);
	m_ToReplaySerialiser->Serialise("", events);
	m_ToReplaySerialiser->Serialise("", target);
	m_ToReplaySerialiser->Serialise("", x);
	m_ToReplaySerialiser->Serialise("", y);
	m_ToReplaySerialiser->Serialise("", slice);
	m_ToReplaySerialiser->Serialise("", mip);
	m_ToReplaySerialiser->Serialise("", sampleIdx);

	if(m_ReplayHost)
	{
		ret = m_Remote->PixelHistory(frameID, events, target, x, y, slice, mip, sampleIdx);
	}
	else
	{
		if(!SendReplayCommand(eCommand_PixelHistory))
			return ret;
	}

	m_FromReplaySerialiser->Serialise("", ret);

	return ret;
}

ShaderDebugTrace ProxySerialiser::DebugVertex(uint32_t frameID, uint32_t eventID, uint32_t vertid, uint32_t instid, uint32_t idx, uint32_t instOffset, uint32_t vertOffset)
{
	ShaderDebugTrace ret;
	
	m_ToReplaySerialiser->Serialise("", frameID);
	m_ToReplaySerialiser->Serialise("", eventID);
	m_ToReplaySerialiser->Serialise("", vertid);
	m_ToReplaySerialiser->Serialise("", instid);
	m_ToReplaySerialiser->Serialise("", idx);
	m_ToReplaySerialiser->Serialise("", instOffset);
	m_ToReplaySerialiser->Serialise("", vertOffset);

	if(m_ReplayHost)
	{
		ret = m_Remote->DebugVertex(frameID, eventID, vertid, instid, idx, instOffset, vertOffset);
	}
	else
	{
		if(!SendReplayCommand(eCommand_DebugVertex))
			return ret;
	}

	m_FromReplaySerialiser->Serialise("", ret);

	return ret;
}

ShaderDebugTrace ProxySerialiser::DebugPixel(uint32_t frameID, uint32_t eventID, uint32_t x, uint32_t y, uint32_t sample, uint32_t primitive)
{
	ShaderDebugTrace ret;
	
	m_ToReplaySerialiser->Serialise("", frameID);
	m_ToReplaySerialiser->Serialise("", eventID);
	m_ToReplaySerialiser->Serialise("", x);
	m_ToReplaySerialiser->Serialise("", y);
	m_ToReplaySerialiser->Serialise("", sample);
	m_ToReplaySerialiser->Serialise("", primitive);

	if(m_ReplayHost)
	{
		ret = m_Remote->DebugPixel(frameID, eventID, x, y, sample, primitive);
	}
	else
	{
		if(!SendReplayCommand(eCommand_DebugPixel))
			return ret;
	}

	m_FromReplaySerialiser->Serialise("", ret);

	return ret;
}

ShaderDebugTrace ProxySerialiser::DebugThread(uint32_t frameID, uint32_t eventID, uint32_t groupid[3], uint32_t threadid[3])
{
	ShaderDebugTrace ret;
	
	m_ToReplaySerialiser->Serialise("", frameID);
	m_ToReplaySerialiser->Serialise("", eventID);
	m_ToReplaySerialiser->Serialise<3>("", groupid);
	m_ToReplaySerialiser->Serialise<3>("", threadid);

	if(m_ReplayHost)
	{
		ret = m_Remote->DebugThread(frameID, eventID, groupid, threadid);
	}
	else
	{
		if(!SendReplayCommand(eCommand_DebugThread))
			return ret;
	}

	m_FromReplaySerialiser->Serialise("", ret);

	return ret;
}
