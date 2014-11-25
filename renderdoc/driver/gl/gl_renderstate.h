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

#include "gl_common.h"
#include "gl_hookset.h"
#include "gl_manager.h"

#include "maths/vec.h"

struct GLRenderState
{
	GLRenderState(const GLHookSet *funcs, Serialiser *ser, LogState state);
	~GLRenderState();

	void FetchState();
	void ApplyState();
	void Clear();

	enum
	{
		// eEnabled_Blend // handled below with blend values
		eEnabled_ClipDistance0,
		eEnabled_ClipDistance1,
		eEnabled_ClipDistance2,
		eEnabled_ClipDistance3,
		eEnabled_ClipDistance4,
		eEnabled_ClipDistance5,
		eEnabled_ClipDistance6,
		eEnabled_ClipDistance7,
		eEnabled_ColorLogicOp,
		eEnabled_CullFace,
		eEnabled_DepthClamp,
		eEnabled_DepthTest,
		eEnabled_Dither,
		eEnabled_FramebufferSRGB,
		eEnabled_LineSmooth,
		eEnabled_Multisample,
		eEnabled_PolySmooth,
		eEnabled_PolyOffsetFill,
		eEnabled_PolyOffsetLine,
		eEnabled_PolyOffsetPoint,
		eEnabled_ProgramPointSize,
		eEnabled_PrimitiveRestart,
		eEnabled_PrimitiveRestartFixedIndex,
		eEnabled_SampleAlphaToCoverage,
		eEnabled_SampleAlphaToOne,
		eEnabled_SampleCoverage,
		eEnabled_SampleMask,
		//eEnabled_ScissorTest, handled below with scissor values
		eEnabled_StencilTest,
		eEnabled_TexCubeSeamless,
		eEnabled_Count,
	};

	bool Enabled[eEnabled_Count];

	//
	uint32_t Tex2D[128];
	uint32_t Samplers[128];
	GLenum ActiveTexture;

	GLuint Program;
	GLuint Pipeline;

	struct
	{
		GLint numSubroutines;
		GLuint Values[128];
	} Subroutines[6];

	enum
	{
		eBufIdx_Array,
		eBufIdx_Copy_Read,
		eBufIdx_Copy_Write,
		eBufIdx_Draw_Indirect,
		eBufIdx_Dispatch_Indirect,
		eBufIdx_Element_Array,
		eBufIdx_Pixel_Pack,
		eBufIdx_Pixel_Unpack,
		eBufIdx_Query,
		eBufIdx_Texture,
	};
	
	GLuint VAO;

	Vec4f GenericVertexAttribs[32];

	float PointFadeThresholdSize;
	GLenum PointSpriteOrigin;
	float LineWidth;
	float PointSize;

	uint32_t PrimitiveRestartIndex;
	GLenum ClipOrigin, ClipDepth;
	GLenum ProvokingVertex;

	uint32_t BufferBindings[10];
	struct IdxRangeBuffer
	{
		uint32_t name;
		uint64_t start;
		uint64_t size;
	} AtomicCounter[1], ShaderStorage[8], TransformFeedback[4], UniformBinding[84];

	struct BlendState
	{
		GLenum EquationRGB, EquationAlpha;
		GLenum SourceRGB, SourceAlpha;
		GLenum DestinationRGB, DestinationAlpha;
		bool Enabled;
	} Blends[8];
	float BlendColor[4];

	struct Viewport
	{
		float x, y, width, height;
	} Viewports[16];
	
	struct Scissor
	{
		int32_t x, y, width, height;
		bool enabled;
	} Scissors[16];
	
	GLuint ReadFBO, DrawFBO;
	GLenum DrawBuffers[8];

	// TODO:
	// Image state (GL_IMAGE_BINDING_NAME)
	// multisampling
	// provoking vertex
	// other misc state :)
	
	struct
	{
		int32_t numVerts;
		float defaultInnerLevel[2];
		float defaultOuterLevel[4];
	} PatchParams;

	GLenum PolygonMode;
	float PolygonOffset[2]; // Factor, Units

	uint8_t DepthWriteMask;
	float DepthClearValue;
	GLenum DepthFunc;
	struct
	{
		double nearZ, farZ;
	} DepthRanges[16];
	
	struct
	{
		double nearZ, farZ;
	} DepthBounds;

	struct
	{
		GLenum func;
		int32_t ref;
		uint8_t valuemask;
		uint8_t writemask;

		GLenum stencilFail;
		GLenum depthFail;
		GLenum pass;
	} StencilBack, StencilFront;
	uint32_t StencilClearValue;

	struct
	{
		uint8_t red, green, blue, alpha;
	} ColorMasks[8];

	uint32_t SampleMask[2];
	float SampleCoverage;
	bool SampleCoverageInvert;
	float MinSampleShading;

	GLenum LogicOp;

	struct
	{
		float red, green, blue, alpha;
	} ColorClearValue;

	struct
	{
		GLenum Derivatives;
		GLenum LineSmooth;
		GLenum PolySmooth;
		GLenum TexCompression;
	} Hints;

	GLenum FrontFace;
	GLenum CullFace;
	//

	void Serialise(LogState state, void *ctx, WrappedOpenGL *gl);
private:
	Serialiser *m_pSerialiser;
	LogState m_State;
	const GLHookSet *m_Real;
};
