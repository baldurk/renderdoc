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

struct GLPipelineState
{
	GLPipelineState() {}

	struct VertexInput
	{
		VertexInput() {}
		
		struct VertexAttribute
		{
			VertexAttribute() : BufferSlot(0), RelativeOffset(0) {}
			bool32 Enabled;
			ResourceFormat Format;
			FloatVector GenericValue;
			uint32_t BufferSlot;
			uint32_t RelativeOffset;
		};
		rdctype::array<VertexAttribute> attributes;

		struct VertexBuffer
		{
			VertexBuffer() : Buffer(), Stride(0), Offset(0), Divisor(0) {}
			ResourceId Buffer;
			uint32_t Stride;
			uint32_t Offset;
			uint32_t Divisor;
		};
		rdctype::array<VertexBuffer> vbuffers;

		ResourceId ibuffer;
		bool32 primitiveRestart;
		uint32_t restartIndex;
	} m_VtxIn;

	struct ShaderStage
	{
		ShaderStage() : Shader(), ShaderDetails(NULL) {}
		ResourceId Shader;
		ShaderReflection *ShaderDetails;
		ShaderBindpointMapping BindpointMapping;

		ShaderStageType stage;
	} m_VS, m_TCS, m_TES, m_GS, m_FS, m_CS;

	struct Texture
	{
		ResourceId Resource;
		uint32_t FirstSlice;
	};
	rdctype::array<Texture> Textures;

	struct Buffer
	{
		ResourceId Resource;
		uint64_t Offset;
		uint64_t Size;
	};
	rdctype::array<Buffer> UniformBuffers;

	struct Rasterizer
	{
		struct Viewport
		{
			Viewport() : Left(0.0f), Bottom(0.0f), Width(0.0f), Height(0.0f), MinDepth(0.0f), MaxDepth(0.0f) {}
			float Left, Bottom;
			float Width, Height;
			double MinDepth, MaxDepth;
		};
		rdctype::array<Viewport> Viewports;

		struct Scissor
		{
			Scissor() : Left(0), Bottom(0), Width(0), Height(0), Enabled(false) {}
			int32_t Left, Bottom;
			int32_t Width, Height;
			bool32 Enabled;
		};
		rdctype::array<Scissor> Scissors;

		struct RasterizerState
		{
			RasterizerState()
				: FillMode(eFill_Solid), CullMode(eCull_None), FrontCCW(false), DepthBias(0),
				  SlopeScaledDepthBias(0.0f), DepthClamp(false),
				  MultisampleEnable(false), AntialiasedLineEnable(false) {}
			TriangleFillMode FillMode;
			TriangleCullMode CullMode;
			bool32 FrontCCW;
			float DepthBias;
			float SlopeScaledDepthBias;
			bool32 DepthClamp;
			bool32 MultisampleEnable;
			bool32 AntialiasedLineEnable;
		} m_State;
	} m_Rasterizer;
	
	struct DepthState
	{
		DepthState()
			: DepthEnable(false), DepthWrites(false), DepthBounds(false),
				NearBound(0), FarBound(0) {}
		bool32 DepthEnable;
		rdctype::str DepthFunc;
		bool32 DepthWrites;
		bool32 DepthBounds;
		double NearBound, FarBound;
	} m_DepthState;

	struct StencilState
	{
		StencilState()
			: StencilEnable(false) {}
		bool32 StencilEnable;

		struct StencilOp
		{
			StencilOp() : Ref(0), ValueMask(0), WriteMask(0) {}
			rdctype::str FailOp;
			rdctype::str DepthFailOp;
			rdctype::str PassOp;
			rdctype::str Func;
			uint32_t Ref;
			uint32_t ValueMask;
			uint32_t WriteMask;
		} m_FrontFace, m_BackFace;
	} m_StencilState;

	struct FrameBuffer
	{
		FrameBuffer() : FBO(), Depth(), Stencil() {}

		ResourceId FBO;

		bool32 FramebufferSRGB;

		rdctype::array<ResourceId> Color;
		ResourceId Depth;
		ResourceId Stencil;
		
		struct BlendState
		{
			BlendState()
			{ BlendFactor[0] = BlendFactor[1] = BlendFactor[2] = BlendFactor[3] = 0.0f; }

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
				byte WriteMask;
			};
			rdctype::array<RTBlend> Blends;

			float BlendFactor[4];
		} m_Blending;

	} m_FB;
};