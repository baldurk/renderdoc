/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2018-2019 Baldur Karlsson
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

#include "gl_resources.h"

struct VertexAttribInitialData
{
  uint32_t enabled;
  uint32_t vbslot;
  uint32_t offset;
  GLenum type;
  int32_t normalized;
  uint32_t integer;
  uint32_t size;
};

DECLARE_REFLECTION_STRUCT(VertexAttribInitialData);

struct VertexBufferInitialData
{
  GLResource Buffer;
  uint64_t Stride;
  uint64_t Offset;
  uint32_t Divisor;
};

DECLARE_REFLECTION_STRUCT(VertexBufferInitialData);

// note these data structures below contain a 'valid' bool, since due to complexities of
// fetching the state on the right context, we might never be able to fetch the data at
// all. So the valid is set to false to indicate that we shouldn't try to restore it on
// replay.
struct VAOInitialData
{
  bool valid;
  VertexAttribInitialData VertexAttribs[16];
  VertexBufferInitialData VertexBuffers[16];
  GLResource ElementArrayBuffer;
};

DECLARE_REFLECTION_STRUCT(VAOInitialData);

struct FeedbackInitialData
{
  bool valid;
  GLResource Buffer[4];
  uint64_t Offset[4];
  uint64_t Size[4];
};

DECLARE_REFLECTION_STRUCT(FeedbackInitialData);

struct FramebufferAttachmentData
{
  bool layered;
  int32_t layer;
  int32_t level;
  int32_t numVirtualSamples;    // number of samples for mobile MSAA framebuffers, where the tiler
                                // resolves into non-msaa textures
  int32_t numViews;             // number of views for multiview framebuffers
  int32_t startView;            // index of the first view for multiview framebuffers
  GLResource obj;
};

DECLARE_REFLECTION_STRUCT(FramebufferAttachmentData);

struct FramebufferInitialData
{
  bool valid;
  FramebufferAttachmentData Attachments[10];
  GLenum DrawBuffers[8];
  GLenum ReadBuffer;

  static const GLenum attachmentNames[10];
};

DECLARE_REFLECTION_STRUCT(FramebufferInitialData);

struct SamplerInitialData
{
  bool valid;
  float border[4];
  GLenum compareFunc, compareMode;
  float lodBias;
  float minLod, maxLod;
  GLenum minFilter, magFilter;
  float maxAniso;
  GLenum wrap[3];
};

DECLARE_REFLECTION_STRUCT(SamplerInitialData);

struct PipelineInitialData
{
  bool valid;
  GLResource programs[6];
};

DECLARE_REFLECTION_STRUCT(PipelineInitialData);

struct TextureStateInitialData
{
  // these are slightly redundant but convenient to have with the initial state data.
  GLenum internalformat;
  bool isView;
  uint32_t width, height, depth;
  uint32_t samples;
  uint32_t dim;
  GLenum type;
  int mips;

  int32_t baseLevel, maxLevel;
  float minLod, maxLod;
  GLenum srgbDecode;
  GLenum depthMode;
  GLenum compareFunc, compareMode;
  GLenum minFilter, magFilter;
  int32_t seamless;
  GLenum swizzle[4];
  GLenum wrap[3];
  float border[4];
  float lodBias;
  GLResource texBuffer;
  uint32_t texBufOffs;
  uint32_t texBufSize;
};

DECLARE_REFLECTION_STRUCT(TextureStateInitialData);

struct GLInitialContents
{
  GLInitialContents()
  {
    RDCCOMPILE_ASSERT(std::is_standard_layout<GLInitialContents>::value,
                      "GLInitialContents must be POD");
    memset(this, 0, sizeof(*this));
  }

  GLInitialContents(GLResource buf, uint32_t len)
  {
    memset(this, 0, sizeof(*this));
    resource = buf;
    bufferLength = len;
  }

  template <typename Configuration>
  void Free(ResourceManager<Configuration> *rm)
  {
    rm->ResourceTypeRelease(resource);
  }

  // these are all POD and mutually exclusive, so we can union them to save space
  union
  {
    VAOInitialData vao;
    FeedbackInitialData xfb;
    FramebufferInitialData fbo;
    SamplerInitialData samp;
    PipelineInitialData pipe;
    TextureStateInitialData tex;
  };

  GLNamespace type;

  // the GL object containing the contents of a texture, buffer, or program
  GLResource resource;
  uint32_t bufferLength;
};