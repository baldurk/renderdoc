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

#pragma once

struct D3D11PipelineState
{
	D3D11PipelineState() {}
	
	struct InputAssembler
	{
		InputAssembler() : Bytecode(NULL) {}

		struct LayoutInput
		{
			LayoutInput() : SemanticIndex(0), InputSlot(0), ByteOffset(0), PerInstance(false), InstanceDataStepRate(0) {}
			rdctype::str SemanticName;
			uint32_t SemanticIndex;
			ResourceFormat Format;
			uint32_t InputSlot;
			uint32_t ByteOffset;
			bool32 PerInstance;
			uint32_t InstanceDataStepRate;
		};
		rdctype::array<LayoutInput> layouts;
		ResourceId layout;
		ShaderReflection *Bytecode;

		struct VertexBuffer
		{
			VertexBuffer() : Buffer(), Stride(0), Offset(0) {}
			ResourceId Buffer;
			uint32_t Stride;
			uint32_t Offset;
		};
		rdctype::array<VertexBuffer> vbuffers;

		struct IndexBuffer
		{
			IndexBuffer() : Buffer(), Offset(0) {}
			ResourceId Buffer;
			uint32_t Offset;
		} ibuffer;
	} m_IA;

	struct ShaderStage
	{
		ShaderStage() : Shader(), ShaderDetails(NULL) {}
		ResourceId Shader;
		rdctype::str ShaderName;
		bool32 customName;
		ShaderReflection *ShaderDetails;
		ShaderBindpointMapping BindpointMapping;

		ShaderStageType stage;

		struct ResourceView
		{
			ResourceView() : View(), Resource(),
				Format(),
				Structured(false), BufferStructCount(0),
				ElementOffset(0), ElementWidth(0),
				FirstElement(0), NumElements(0),
				Flags(0),
				HighestMip(0), NumMipLevels(0),
				ArraySize(0), FirstArraySlice(0) {}

			ResourceId View;
			ResourceId Resource;
			rdctype::str Type;
			ResourceFormat Format;

			bool32 Structured;
			uint32_t BufferStructCount;

			// Buffer (SRV)
			uint32_t ElementOffset;
			uint32_t ElementWidth;

			// Buffer (UAV)
			uint32_t FirstElement;
			uint32_t NumElements;

			// BufferEx
			uint32_t Flags;

			// Texture
			uint32_t HighestMip;
			uint32_t NumMipLevels;

			// Texture Array
			uint32_t ArraySize;
			uint32_t FirstArraySlice;
		};
		rdctype::array<ResourceView> SRVs;
		rdctype::array<ResourceView> UAVs;

		struct Sampler
		{
			Sampler()
				: Samp()
				, UseBorder(false), UseComparison(false)
			 	, MaxAniso(0), MaxLOD(0.0f), MinLOD(0.0f), MipLODBias(0.0f)
			{ BorderColor[0] = BorderColor[1] = BorderColor[2] = BorderColor[3] = 0.0f; }
			ResourceId Samp;
			rdctype::str AddressU, AddressV, AddressW;
			float BorderColor[4];
			rdctype::str Comparison;
			rdctype::str Filter;
			bool32 UseBorder;
			bool32 UseComparison;
			uint32_t MaxAniso;
			float MaxLOD;
			float MinLOD;
			float MipLODBias;
		};
		rdctype::array<Sampler> Samplers;

		struct CBuffer
		{
			CBuffer() : Buffer(), VecOffset(0), VecCount(0) {}
			ResourceId Buffer;
			uint32_t VecOffset;
			uint32_t VecCount;
		};
		rdctype::array<CBuffer> ConstantBuffers;

		rdctype::array<rdctype::str> ClassInstances;
	} m_VS, m_HS, m_DS, m_GS, m_PS, m_CS;

	struct Streamout
	{
		struct Output
		{
			Output() : Buffer(), Offset(0) { }
			ResourceId Buffer;
			uint32_t Offset;
		};
		rdctype::array<Output> Outputs;
	} m_SO;

	struct Rasterizer
	{
		struct Viewport
		{
			Viewport(float TX=0.0f, float TY=0.0f, float W=0.0f, float H=0.0f, float MN=0.0f, float MX=0.0f)
				: Width(W), Height(H), MinDepth(MN), MaxDepth(MX)
			{ TopLeft[0] = TX; TopLeft[1] = TY; }
			float TopLeft[2];
			float Width, Height;
			float MinDepth, MaxDepth;
		};
		rdctype::array<Viewport> Viewports;

		struct Scissor
		{
			Scissor(int l=0, int t=0, int r=0, int b=0) : left(l), top(t), right(r), bottom(b) {}
			int32_t left, top, right, bottom;
		};
		rdctype::array<Scissor> Scissors;

		struct RasterizerState
		{
			RasterizerState()
				: State(), FillMode(eFill_Solid), CullMode(eCull_None), FrontCCW(false), DepthBias(0),
				  DepthBiasClamp(0.0f), SlopeScaledDepthBias(0.0f), DepthClip(false),
				  ScissorEnable(false), MultisampleEnable(false), AntialiasedLineEnable(false),
				  ForcedSampleCount(0) {}
			ResourceId State;
			TriangleFillMode FillMode;
			TriangleCullMode CullMode;
			bool32 FrontCCW;
			int32_t DepthBias;
			float DepthBiasClamp;
			float SlopeScaledDepthBias;
			bool32 DepthClip;
			bool32 ScissorEnable;
			bool32 MultisampleEnable;
			bool32 AntialiasedLineEnable;
			uint32_t ForcedSampleCount;
		} m_State;
	} m_RS;

	struct OutputMerger
	{
		OutputMerger()
			: UAVStartSlot(0), DepthReadOnly(false), StencilReadOnly(false) {}

		struct DepthStencilState
		{
			DepthStencilState()
				: State(), DepthEnable(false), StencilEnable(false),
				  StencilReadMask(0), StencilWriteMask(0), StencilRef(0) {}
			ResourceId State;
			bool32 DepthEnable;
			rdctype::str DepthFunc;
			bool32 DepthWrites;
			bool32 StencilEnable;
			byte StencilReadMask;
			byte StencilWriteMask;

			struct StencilOp
			{
				rdctype::str FailOp;
				rdctype::str DepthFailOp;
				rdctype::str PassOp;
				rdctype::str Func;
			} m_FrontFace, m_BackFace;

			uint32_t StencilRef;
		} m_State;

		struct BlendState
		{
			BlendState() : AlphaToCoverage(false), IndependentBlend(false), SampleMask(0)
			{ BlendFactor[0] = BlendFactor[1] = BlendFactor[2] = BlendFactor[3] = 0.0f; }

			ResourceId State;

			bool32 AlphaToCoverage;
			bool32 IndependentBlend;

			struct RTBlend
			{
				RTBlend() : Enabled(false), WriteMask(0) {}
				struct BlendOp
				{
					rdctype::str Source;
					rdctype::str Destination;
					rdctype::str Operation;
				} m_Blend, m_AlphaBlend;

				rdctype::str LogicOp;

				bool32 Enabled;
				bool32 LogicEnabled;
				byte WriteMask;
			};
			rdctype::array<RTBlend> Blends;

			float BlendFactor[4];
			uint32_t SampleMask;
		} m_BlendState;

		rdctype::array<ShaderStage::ResourceView> RenderTargets;

		uint32_t UAVStartSlot;
		rdctype::array<ShaderStage::ResourceView> UAVs;

		ShaderStage::ResourceView DepthTarget;
		bool32 DepthReadOnly;
		bool32 StencilReadOnly;
	} m_OM;
};
