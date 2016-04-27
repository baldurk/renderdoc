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

#pragma once

struct GLPipelineState
{
	GLPipelineState() {}

	struct VertexInput
	{
		VertexInput() :
		  ibuffer(), primitiveRestart(false),
		  restartIndex(0), provokingVertexLast(false) {}
		
		struct VertexAttribute
		{
			VertexAttribute() : BufferSlot(0), RelativeOffset(0) {}
			bool32 Enabled;
			ResourceFormat Format;

			union
			{
				float f[4];
				uint32_t u[4];
				int32_t i[4];
			} GenericValue;

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

		bool32 provokingVertexLast;
	} m_VtxIn;

	struct ShaderStage
	{
		ShaderStage()
			: Shader()
			, customShaderName(false), customProgramName(false)
			, customPipelineName(false), ShaderDetails(NULL) {}
		ResourceId Shader;

		rdctype::str ShaderName;
		bool32 customShaderName;

		rdctype::str ProgramName;
		bool32 customProgramName;

		bool32 PipelineActive;
		rdctype::str PipelineName;
		bool32 customPipelineName;

		ShaderReflection *ShaderDetails;
		ShaderBindpointMapping BindpointMapping;

		ShaderStageType stage;

		rdctype::array<uint32_t> Subroutines;
	} m_VS, m_TCS, m_TES, m_GS, m_FS, m_CS;
	
	struct FixedVertexProcessing
	{
		FixedVertexProcessing() : discard(0), clipOriginLowerLeft(0), clipNegativeOneToOne(0)
		{
			defaultInnerLevel[0] = defaultInnerLevel[1] = 0.0f;
			defaultOuterLevel[0] = defaultOuterLevel[1] = defaultOuterLevel[2] = defaultOuterLevel[3] = 0.0f;
			clipPlanes[0] =
				clipPlanes[1] =
				clipPlanes[2] =
				clipPlanes[3] =
				clipPlanes[4] =
				clipPlanes[5] =
				clipPlanes[6] =
				clipPlanes[7] = 0;
		}

		float defaultInnerLevel[2];
		float defaultOuterLevel[4];
		bool32 discard;

		bool32 clipPlanes[8];
		bool32 clipOriginLowerLeft;
		bool32 clipNegativeOneToOne;
	} m_VtxProcess;

	struct Texture
	{
		Texture() : Resource(), FirstSlice(0), ResType(eResType_None), DepthReadChannel(-1)
		{
			Swizzle[0] = eSwizzle_Red;
			Swizzle[1] = eSwizzle_Green;
			Swizzle[2] = eSwizzle_Blue;
			Swizzle[3] = eSwizzle_Alpha;
		}
		ResourceId Resource;
		uint32_t FirstSlice;
		uint32_t HighestMip;
		ShaderResourceType ResType;

		TextureSwizzle Swizzle[4];
		int32_t DepthReadChannel;
	};
	rdctype::array<Texture> Textures;
	
	struct Sampler
	{
		Sampler()
			: Samp()
			, UseBorder(false), UseComparison(false), SeamlessCube(false)
			, MaxAniso(0.0f), MaxLOD(0.0f), MinLOD(0.0f), MipLODBias(0.0f)
		{ BorderColor[0] = BorderColor[1] = BorderColor[2] = BorderColor[3] = 0.0f; }
		ResourceId Samp;
		rdctype::str AddressS, AddressT, AddressR;
		float BorderColor[4];
		rdctype::str Comparison;
		rdctype::str MinFilter;
		rdctype::str MagFilter;
		bool32 UseBorder;
		bool32 UseComparison;
		bool32 SeamlessCube;
		float MaxAniso;
		float MaxLOD;
		float MinLOD;
		float MipLODBias;
	};
	rdctype::array<Sampler> Samplers;

	struct Buffer
	{
		Buffer() : Resource(), Offset(0), Size(0) {}
		ResourceId Resource;
		uint64_t Offset;
		uint64_t Size;
	};
	rdctype::array<Buffer> AtomicBuffers;
	rdctype::array<Buffer> UniformBuffers;
	rdctype::array<Buffer> ShaderStorageBuffers;
	
	struct ImageLoadStore
	{
		ImageLoadStore()
			: Resource(), Level(0), Layered(false), Layer(0),
			  ResType(eResType_None), readAllowed(false), writeAllowed(false)
		{
		}
		ResourceId Resource;
		uint32_t Level;
		bool32 Layered;
		uint32_t Layer;
		ShaderResourceType ResType;
		bool32 readAllowed;
		bool32 writeAllowed;
		ResourceFormat Format;
	};
	rdctype::array<ImageLoadStore> Images;
	
	struct Feedback
	{
		ResourceId Obj;
		ResourceId BufferBinding[4];
		uint64_t Offset[4];
		uint64_t Size[4];
		bool32 Active;
		bool32 Paused;
	} m_Feedback;

	struct Rasterizer
	{
		struct Viewport
		{
			Viewport() : Left(0.0f), Bottom(0.0f), Width(0.0f), Height(0.0f), MinDepth(0.0f), MaxDepth(0.0f), Enabled(true) {}
			float Left, Bottom;
			float Width, Height;
			double MinDepth, MaxDepth;
			bool32 Enabled;
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
				  SlopeScaledDepthBias(0.0f), OffsetClamp(0.0f), DepthClamp(false),
				  MultisampleEnable(false) {}
			TriangleFillMode FillMode;
			TriangleCullMode CullMode;
			bool32 FrontCCW;
			float DepthBias;
			float SlopeScaledDepthBias;
			float OffsetClamp;
			bool32 DepthClamp;

			bool32 MultisampleEnable;
			bool32 SampleShading;
			bool32 SampleMask;
			uint32_t SampleMaskValue;
			bool32 SampleCoverage;
			bool32 SampleCoverageInvert;
			float SampleCoverageValue;
			bool32 SampleAlphaToCoverage;
			bool32 SampleAlphaToOne;
			float MinSampleShadingRate;

			bool32 ProgrammablePointSize;
			float PointSize;
			float LineWidth;
			float PointFadeThreshold;
			bool32 PointOriginUpperLeft;
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
		FrameBuffer() : FramebufferSRGB(false), Dither(false) {}

		bool32 FramebufferSRGB;
		bool32 Dither;

		struct Attachment
		{
			ResourceId Obj;
			uint32_t Layer;
			uint32_t Mip;
		};

		struct FBO
		{
			FBO() : Obj(), Depth(), Stencil() {}
			ResourceId Obj;
			rdctype::array<Attachment> Color;
			Attachment Depth;
			Attachment Stencil;

			rdctype::array<int32_t> DrawBuffers;
			int32_t ReadBuffer;
		} m_DrawFBO, m_ReadFBO;
		
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

	struct Hints 
	{
		Hints()
			: Derivatives(eQuality_DontCare)
			, LineSmooth(eQuality_DontCare)
			, PolySmooth(eQuality_DontCare)
			, TexCompression(eQuality_DontCare)
			, LineSmoothEnabled(0)
			, PolySmoothEnabled(0)
		{}

		QualityHint Derivatives;
		QualityHint LineSmooth;
		QualityHint PolySmooth;
		QualityHint TexCompression;
		bool32 LineSmoothEnabled;
		bool32 PolySmoothEnabled;
	} m_Hints;
};
