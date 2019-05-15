/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2019 Baldur Karlsson
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

#include "maths/vec.h"
#include "gl_common.h"
#include "gl_dispatch_table.h"
#include "gl_manager.h"

struct PixelStorageState
{
  int32_t swapBytes;
  int32_t lsbFirst;    // deprecated since OpenGL 4.3 core profile
  int32_t rowlength, imageheight;
  int32_t skipPixels, skipRows, skipImages;
  int32_t alignment;

  int32_t compressedBlockWidth, compressedBlockHeight, compressedBlockDepth;
  int32_t compressedBlockSize;

protected:
  PixelStorageState();
};

struct PixelPackState : public PixelStorageState
{
  void Fetch(bool compressed);
  void Apply(bool compressed);
};

struct PixelUnpackState : public PixelStorageState
{
  void Fetch(bool compressed);
  void Apply(bool compressed);

  bool FastPath(GLsizei width, GLsizei height, GLsizei depth, GLenum dataformat, GLenum basetype);
  bool FastPathCompressed(GLsizei width, GLsizei height, GLsizei depth);

  byte *Unpack(byte *pixels, GLsizei width, GLsizei height, GLsizei depth, GLenum dataformat,
               GLenum basetype);
  byte *UnpackCompressed(byte *pixels, GLsizei width, GLsizei height, GLsizei depth,
                         GLsizei &imageSize);
};

void ResetPixelPackState(bool compressed, GLint alignment);
void ResetPixelUnpackState(bool compressed, GLint alignment);

struct GLRenderState
{
  GLRenderState();
  ~GLRenderState();

  void FetchState(WrappedOpenGL *driver);
  void ApplyState(WrappedOpenGL *driver);
  void Clear();

  void MarkReferenced(WrappedOpenGL *driver, bool initial) const;
  void MarkDirty(WrappedOpenGL *driver) const;

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
    eEnabled_DepthBoundsEXT,
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
    eEnabled_SampleShading,
    eEnabled_RasterMultisample,
    // eEnabled_ScissorTest, handled below with scissor values
    eEnabled_StencilTest,
    eEnabled_TexCubeSeamless,
    eEnabled_BlendCoherent,
    eEnabled_RasterizerDiscard,
    eEnabled_Count,
  };

  bool ContextPresent;

  bool Enabled[eEnabled_Count];

  GLResource Tex1D[128];
  GLResource Tex2D[128];
  GLResource Tex3D[128];
  GLResource Tex1DArray[128];
  GLResource Tex2DArray[128];
  GLResource TexCubeArray[128];
  GLResource TexRect[128];
  GLResource TexBuffer[128];
  GLResource TexCube[128];
  GLResource Tex2DMS[128];
  GLResource Tex2DMSArray[128];
  GLResource Samplers[128];
  GLenum ActiveTexture;

  struct Image
  {
    GLResource res;
    uint32_t level;
    bool layered;
    uint32_t layer;
    GLenum access;
    GLenum format;
  } Images[8];

  GLResource Program;
  GLResource Pipeline;

  struct Subroutine
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
    eBufIdx_Pixel_Pack,
    eBufIdx_Pixel_Unpack,
    eBufIdx_Query,
    eBufIdx_Texture,
    eBufIdx_Parameter,
    eBufIdx_Count,
  };

  GLResource VAO;

  GLResource FeedbackObj;

  FloatVector GenericVertexAttribs[32];

  float PointFadeThresholdSize;
  GLenum PointSpriteOrigin;
  float LineWidth;
  float PointSize;

  uint32_t PrimitiveRestartIndex;

  struct BoundingBox
  {
    float minX, minY, minZ, minW;
    float maxX, maxY, maxZ, maxW;
  } PrimitiveBoundingBox;

  GLenum ClipOrigin, ClipDepth;
  GLenum ProvokingVertex;

  GLResource BufferBindings[eBufIdx_Count];
  struct IdxRangeBuffer
  {
    GLResource res;
    uint64_t start;
    uint64_t size;
  } AtomicCounter[8], ShaderStorage[96], TransformFeedback[4], UniformBinding[84];

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

  struct DepthRange
  {
    double nearZ, farZ;
  } DepthRanges[16];

  GLResource ReadFBO, DrawFBO;

  // these refer to the states on the default framebuffer.
  // Other FBOs serialise them in their resource records.
  GLenum ReadBuffer;
  GLenum DrawBuffers[8];

  struct
  {
    int32_t numVerts;
    float defaultInnerLevel[2];
    float defaultOuterLevel[4];
  } PatchParams;

  GLenum PolygonMode;
  float PolygonOffset[3];    // Factor, Units, (extension) Clamp

  uint8_t DepthWriteMask;
  float DepthClearValue;
  GLenum DepthFunc;

  struct DepthBound
  {
    double nearZ, farZ;
  } DepthBounds;

  struct StencilFace
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

  struct ColorMask
  {
    uint8_t red, green, blue, alpha;
  } ColorMasks[8];

  uint32_t RasterSamples;
  bool RasterFixed;
  uint32_t SampleMask[2];
  float SampleCoverage;
  bool SampleCoverageInvert;
  float MinSampleShading;

  GLenum LogicOp;

  struct ClearValue
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

  PixelUnpackState Unpack;

private:
  bool CheckEnableDisableParam(GLenum pname);
};

DECLARE_REFLECTION_STRUCT(GLRenderState::Image);
DECLARE_REFLECTION_STRUCT(GLRenderState::Subroutine);
DECLARE_REFLECTION_STRUCT(GLRenderState::IdxRangeBuffer);
DECLARE_REFLECTION_STRUCT(GLRenderState::BlendState);
DECLARE_REFLECTION_STRUCT(GLRenderState::Viewport);
DECLARE_REFLECTION_STRUCT(GLRenderState::Scissor);
DECLARE_REFLECTION_STRUCT(GLRenderState::DepthRange);
DECLARE_REFLECTION_STRUCT(GLRenderState::DepthBound);
DECLARE_REFLECTION_STRUCT(GLRenderState::ColorMask);
DECLARE_REFLECTION_STRUCT(GLRenderState::ClearValue);
DECLARE_REFLECTION_STRUCT(GLRenderState);
