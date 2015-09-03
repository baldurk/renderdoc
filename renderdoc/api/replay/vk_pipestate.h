/******************************************************************************
 * The MIT License (MIT)
 * 
 * Copyright (c) 2015 Baldur Karlsson
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

struct VulkanPipelineState
{
	VulkanPipelineState() : pipelineFlags(0) {}

	ResourceId computePipeline, graphicsPipeline;
	uint32_t pipelineFlags;

	// VKTODOMED renderpass/subpass?
	
	struct InputAssembly
	{
		InputAssembly() : primitiveRestartEnable(false) {}

		bool32 primitiveRestartEnable;

		struct IndexBuffer
		{
			IndexBuffer() : offs(0) {}
			ResourceId buf;
			uint64_t offs;
			// byte width is latched per-draw for GL
		} ibuffer;
	} IA;

	struct VertexInput
	{
		struct Attribute
		{
			Attribute() : binding(0), format(), byteoffset(0) {}
			uint32_t binding;
			ResourceFormat format;
			uint32_t byteoffset;
		};
		rdctype::array<Attribute> attrs;

		struct Binding
		{
			Binding() : vbufferBinding(0), bytestride(0), perInstance(false) {}
			uint32_t vbufferBinding; // VKTODO I believe this is the meaning
			uint32_t bytestride;
			bool32 perInstance;
		};
		rdctype::array<Binding> binds;

		struct VertexBuffer
		{
			VertexBuffer() : offset(0) {}
			ResourceId buffer;
			uint64_t offset;
		};
		rdctype::array<VertexBuffer> vbuffers;
	} VI;

	struct ShaderStage
	{
		ShaderStage() : Shader(), ShaderDetails(NULL), customName(false) {}
		ResourceId Shader;
		rdctype::str ShaderName;
		bool32 customName;
		ShaderReflection *ShaderDetails;
		// VKTODOMED this might no longer make sense
		ShaderBindpointMapping BindpointMapping;

		ShaderStageType stage;

		// VKTODOMED specialization info
	} VS, TCS, TES, GS, FS, CS;

	// VKTODOHIGH descriptor sets

	struct Tessellation
	{
		Tessellation() : numControlPoints(0) { }
		uint32_t numControlPoints;
	} Tess;

	struct ViewState
	{
		ResourceId state;

		struct ViewportScissor
		{
			struct Viewport
			{
				Viewport() : x(0), y(0), width(0), height(0), mindepth(0), maxdepth(0) {}
				float x, y, width, height, mindepth, maxdepth;
			} vp;

			struct Scissor
			{
				Scissor() : x(0), y(0), width(0), height(0) {}
				int32_t x, y, width, height;
			} scissor;
		};
	
		rdctype::array<ViewportScissor> viewportScissors;
	} VP;

	struct Raster
	{
		Raster()
			: depthClipEnable(false), rasterizerDiscardEnable(false), FrontCCW(false), FillMode(eFill_Solid), CullMode(eCull_None)
			, depthBias(0), depthBiasClamp(0), slopeScaledDepthBias(0), lineWidth(0) {}

		bool32 depthClipEnable, rasterizerDiscardEnable, FrontCCW;
		TriangleFillMode FillMode;
		TriangleCullMode CullMode;

		// from dynamic state
		ResourceId state;
		float depthBias, depthBiasClamp, slopeScaledDepthBias, lineWidth;
	} RS;

	struct MultiSample
	{
		MultiSample() : rasterSamples(0), sampleShadingEnable(false), minSampleShading(0), sampleMask(~0U) {}
		uint32_t rasterSamples;
		bool32 sampleShadingEnable;
		float minSampleShading;
		uint32_t sampleMask;
	} MSAA;

	struct ColorBlend
	{
		ColorBlend()
		{
			blendConst[0] = blendConst[1] = blendConst[2] = blendConst[3] = 0.0f;
		}

		bool32 alphaToCoverageEnable, logicOpEnable;
		rdctype::str logicOp;

		struct Attachment
		{
			Attachment() : blendEnable(false), writeMask(0) {}

			bool32 blendEnable;

			struct BlendOp
			{
				rdctype::str Source;
				rdctype::str Destination;
				rdctype::str Operation;
			} blend, alphaBlend;

			uint8_t writeMask;
		};
		rdctype::array<Attachment> attachments;

		ResourceId state;
		float blendConst[4];
	} CB;

	struct DepthStencil
	{
		DepthStencil()
			: testEnable(false), writeEnable(false), boundsEnable(false), stencilTestEnable(false)
			, minDepthBounds(0), maxDepthBounds(0) {}

		bool32 testEnable, writeEnable, boundsEnable;
		rdctype::str compareOp;

		bool32 stencilTestEnable;
		struct StencilOp
		{
			StencilOp() : mask(0), ref(0) {}
			rdctype::str failOp;
			rdctype::str depthFailOp;
			rdctype::str passOp;
			rdctype::str func;
			uint32_t mask;
			uint32_t ref;
		} front, back;

		ResourceId state;
		float minDepthBounds, maxDepthBounds;
	} DS;

	struct CurrentPass
	{
		struct RenderPass
		{
			ResourceId obj;
			// VKTODOMED renderpass and subpass information here
		} renderpass;

		struct Framebuffer
		{
			Framebuffer() : width(0), height(0), layers(0) {}
			ResourceId obj;

			struct Attachment
			{
				ResourceId view;
				// VKTODOLOW need layout here?
			};
			rdctype::array<Attachment> attachments;

			uint32_t width, height, layers;
		} framebuffer;

		struct RenderArea
		{
			RenderArea() : x(0), y(0), width(0), height(0) {}
			int32_t x, y, width, height;
		} renderArea;
	} Pass;
};
