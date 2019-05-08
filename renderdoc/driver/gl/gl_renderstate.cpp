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

#include "gl_renderstate.h"
#include "gl_driver.h"

struct EnableDisableCap
{
  GLenum cap;
  rdcliteral name;
};

static const EnableDisableCap enable_disable_cap[] = {
    {eGL_CLIP_DISTANCE0, "GL_CLIP_DISTANCE0"_lit},
    {eGL_CLIP_DISTANCE1, "GL_CLIP_DISTANCE1"_lit},
    {eGL_CLIP_DISTANCE2, "GL_CLIP_DISTANCE2"_lit},
    {eGL_CLIP_DISTANCE3, "GL_CLIP_DISTANCE3"_lit},
    {eGL_CLIP_DISTANCE4, "GL_CLIP_DISTANCE4"_lit},
    {eGL_CLIP_DISTANCE5, "GL_CLIP_DISTANCE5"_lit},
    {eGL_CLIP_DISTANCE6, "GL_CLIP_DISTANCE6"_lit},
    {eGL_CLIP_DISTANCE7, "GL_CLIP_DISTANCE7"_lit},
    {eGL_COLOR_LOGIC_OP, "GL_COLOR_LOGIC_OP"_lit},
    {eGL_CULL_FACE, "GL_CULL_FACE"_lit},
    {eGL_DEPTH_CLAMP, "GL_DEPTH_CLAMP"_lit},
    {eGL_DEPTH_TEST, "GL_DEPTH_TEST"_lit},
    {eGL_DEPTH_BOUNDS_TEST_EXT, "GL_DEPTH_BOUNDS_TEST_EXT"_lit},
    {eGL_DITHER, "GL_DITHER"_lit},
    {eGL_FRAMEBUFFER_SRGB, "GL_FRAMEBUFFER_SRGB"_lit},
    {eGL_LINE_SMOOTH, "GL_LINE_SMOOTH"_lit},
    {eGL_MULTISAMPLE, "GL_MULTISAMPLE"_lit},
    {eGL_POLYGON_SMOOTH, "GL_POLYGON_SMOOTH"_lit},
    {eGL_POLYGON_OFFSET_FILL, "GL_POLYGON_OFFSET_FILL"_lit},
    {eGL_POLYGON_OFFSET_LINE, "GL_POLYGON_OFFSET_LINE"_lit},
    {eGL_POLYGON_OFFSET_POINT, "GL_POLYGON_OFFSET_POINT"_lit},
    {eGL_PROGRAM_POINT_SIZE, "GL_PROGRAM_POINT_SIZE"_lit},
    {eGL_PRIMITIVE_RESTART, "GL_PRIMITIVE_RESTART"_lit},
    {eGL_PRIMITIVE_RESTART_FIXED_INDEX, "GL_PRIMITIVE_RESTART_FIXED_INDEX"_lit},
    {eGL_SAMPLE_ALPHA_TO_COVERAGE, "GL_SAMPLE_ALPHA_TO_COVERAGE"_lit},
    {eGL_SAMPLE_ALPHA_TO_ONE, "GL_SAMPLE_ALPHA_TO_ONE"_lit},
    {eGL_SAMPLE_COVERAGE, "GL_SAMPLE_COVERAGE"_lit},
    {eGL_SAMPLE_MASK, "GL_SAMPLE_MASK"_lit},
    {eGL_SAMPLE_SHADING, "GL_SAMPLE_SHADING"_lit},
    {eGL_RASTER_MULTISAMPLE_EXT, "GL_RASTER_MULTISAMPLE_EXT"_lit},
    {eGL_STENCIL_TEST, "GL_STENCIL_TEST"_lit},
    {eGL_TEXTURE_CUBE_MAP_SEAMLESS, "GL_TEXTURE_CUBE_MAP_SEAMLESS"_lit},
    {eGL_BLEND_ADVANCED_COHERENT_KHR, "GL_BLEND_ADVANCED_COHERENT_KHR"_lit},
    {eGL_RASTERIZER_DISCARD, "GL_RASTERIZER_DISCARD"_lit},
};

void ResetPixelPackState(bool compressed, GLint alignment)
{
  PixelPackState empty;
  empty.alignment = alignment;
  empty.Apply(compressed);
}

void ResetPixelUnpackState(bool compressed, GLint alignment)
{
  PixelUnpackState empty;
  empty.alignment = alignment;
  empty.Apply(compressed);
}

PixelStorageState::PixelStorageState()
    : swapBytes(),
      lsbFirst(),
      rowlength(),
      imageheight(),
      skipPixels(),
      skipRows(),
      skipImages(),
      alignment(),
      compressedBlockWidth(),
      compressedBlockHeight(),
      compressedBlockDepth(),
      compressedBlockSize()
{
}

void PixelPackState::Fetch(bool compressed)
{
  if(!IsGLES)
  {
    GL.glGetIntegerv(eGL_PACK_SWAP_BYTES, &swapBytes);
    GL.glGetIntegerv(eGL_PACK_LSB_FIRST, &lsbFirst);
    GL.glGetIntegerv(eGL_PACK_IMAGE_HEIGHT, &imageheight);
    GL.glGetIntegerv(eGL_PACK_SKIP_IMAGES, &skipImages);
  }
  GL.glGetIntegerv(eGL_PACK_ROW_LENGTH, &rowlength);
  GL.glGetIntegerv(eGL_PACK_SKIP_PIXELS, &skipPixels);
  GL.glGetIntegerv(eGL_PACK_SKIP_ROWS, &skipRows);
  GL.glGetIntegerv(eGL_PACK_ALIGNMENT, &alignment);

  if(!IsGLES && compressed)
  {
    GL.glGetIntegerv(eGL_PACK_COMPRESSED_BLOCK_WIDTH, &compressedBlockWidth);
    GL.glGetIntegerv(eGL_PACK_COMPRESSED_BLOCK_HEIGHT, &compressedBlockHeight);
    GL.glGetIntegerv(eGL_PACK_COMPRESSED_BLOCK_DEPTH, &compressedBlockDepth);
    GL.glGetIntegerv(eGL_PACK_COMPRESSED_BLOCK_SIZE, &compressedBlockSize);
  }
}

void PixelPackState::Apply(bool compressed)
{
  if(!IsGLES)
  {
    GL.glPixelStorei(eGL_PACK_SWAP_BYTES, swapBytes);
    GL.glPixelStorei(eGL_PACK_LSB_FIRST, lsbFirst);
    GL.glPixelStorei(eGL_PACK_IMAGE_HEIGHT, imageheight);
    GL.glPixelStorei(eGL_PACK_SKIP_IMAGES, skipImages);
  }
  GL.glPixelStorei(eGL_PACK_ROW_LENGTH, rowlength);
  GL.glPixelStorei(eGL_PACK_SKIP_PIXELS, skipPixels);
  GL.glPixelStorei(eGL_PACK_SKIP_ROWS, skipRows);
  GL.glPixelStorei(eGL_PACK_ALIGNMENT, alignment);

  if(!IsGLES && compressed)
  {
    GL.glPixelStorei(eGL_PACK_COMPRESSED_BLOCK_WIDTH, compressedBlockWidth);
    GL.glPixelStorei(eGL_PACK_COMPRESSED_BLOCK_HEIGHT, compressedBlockHeight);
    GL.glPixelStorei(eGL_PACK_COMPRESSED_BLOCK_DEPTH, compressedBlockDepth);
    GL.glPixelStorei(eGL_PACK_COMPRESSED_BLOCK_SIZE, compressedBlockSize);
  }
}

void PixelUnpackState::Fetch(bool compressed)
{
  if(!IsGLES)
  {
    GL.glGetIntegerv(eGL_UNPACK_SWAP_BYTES, &swapBytes);
    GL.glGetIntegerv(eGL_UNPACK_LSB_FIRST, &lsbFirst);
  }
  GL.glGetIntegerv(eGL_UNPACK_ROW_LENGTH, &rowlength);
  GL.glGetIntegerv(eGL_UNPACK_IMAGE_HEIGHT, &imageheight);
  GL.glGetIntegerv(eGL_UNPACK_SKIP_PIXELS, &skipPixels);
  GL.glGetIntegerv(eGL_UNPACK_SKIP_ROWS, &skipRows);
  GL.glGetIntegerv(eGL_UNPACK_SKIP_IMAGES, &skipImages);
  GL.glGetIntegerv(eGL_UNPACK_ALIGNMENT, &alignment);

  if(!IsGLES && compressed)
  {
    GL.glGetIntegerv(eGL_UNPACK_COMPRESSED_BLOCK_WIDTH, &compressedBlockWidth);
    GL.glGetIntegerv(eGL_UNPACK_COMPRESSED_BLOCK_HEIGHT, &compressedBlockHeight);
    GL.glGetIntegerv(eGL_UNPACK_COMPRESSED_BLOCK_DEPTH, &compressedBlockDepth);
    GL.glGetIntegerv(eGL_UNPACK_COMPRESSED_BLOCK_SIZE, &compressedBlockSize);
  }
}

void PixelUnpackState::Apply(bool compressed)
{
  if(!IsGLES)
  {
    GL.glPixelStorei(eGL_UNPACK_SWAP_BYTES, swapBytes);
    GL.glPixelStorei(eGL_UNPACK_LSB_FIRST, lsbFirst);
  }
  GL.glPixelStorei(eGL_UNPACK_ROW_LENGTH, rowlength);
  GL.glPixelStorei(eGL_UNPACK_IMAGE_HEIGHT, imageheight);
  GL.glPixelStorei(eGL_UNPACK_SKIP_PIXELS, skipPixels);
  GL.glPixelStorei(eGL_UNPACK_SKIP_ROWS, skipRows);
  GL.glPixelStorei(eGL_UNPACK_SKIP_IMAGES, skipImages);
  GL.glPixelStorei(eGL_UNPACK_ALIGNMENT, alignment);

  if(!IsGLES && compressed)
  {
    GL.glPixelStorei(eGL_UNPACK_COMPRESSED_BLOCK_WIDTH, compressedBlockWidth);
    GL.glPixelStorei(eGL_UNPACK_COMPRESSED_BLOCK_HEIGHT, compressedBlockHeight);
    GL.glPixelStorei(eGL_UNPACK_COMPRESSED_BLOCK_DEPTH, compressedBlockDepth);
    GL.glPixelStorei(eGL_UNPACK_COMPRESSED_BLOCK_SIZE, compressedBlockSize);
  }
}

bool PixelUnpackState::FastPath(GLsizei width, GLsizei height, GLsizei depth, GLenum dataformat,
                                GLenum basetype)
{
  if(swapBytes)
    return false;

  if(skipPixels)
    return false;

  if(height > 0 && skipRows)
    return false;

  if(depth > 0 && skipImages)
    return false;

  if(width > 0 && rowlength > 0 && width < rowlength)
    return false;

  if(height > 0 && imageheight > 0 && height < imageheight)
    return false;

  if(alignment > (int32_t)GetByteSize(1, 1, 1, dataformat, basetype))
    return false;

  return true;
}

bool PixelUnpackState::FastPathCompressed(GLsizei width, GLsizei height, GLsizei depth)
{
  // compressedBlockSize and compressedBlockWidth must be set for any of the unpack params to be
  // used
  // if they are 0, all of the unpack params are ignored, so we go through the fast path (no
  // unpacking)
  if(compressedBlockSize == 0 || compressedBlockWidth == 0)
    return true;

  if(skipPixels)
    return false;

  if(width > 0 && rowlength > 0 && width < rowlength)
    return false;

  // the below two unpack params require compressedBlockHeight to be set so if we haven't "failed"
  // to
  // hit the fast path, none of the other params make a difference as they're ignored and we go
  // through
  // the fast path (no unpacking)
  if(compressedBlockHeight == 0)
    return true;

  if(height > 0 && skipRows)
    return false;

  if(height > 0 && imageheight > 0 && height < imageheight)
    return false;

  // the final unpack param requires compressedBlockDepth to be set, as above if it's 0 then we can
  // just go straight through the fast path (no unpacking)
  if(compressedBlockDepth == 0)
    return true;

  if(depth > 0 && skipImages)
    return false;

  return true;
}

byte *PixelUnpackState::Unpack(byte *pixels, GLsizei width, GLsizei height, GLsizei depth,
                               GLenum dataformat, GLenum basetype)
{
  size_t pixelSize = GetByteSize(1, 1, 1, dataformat, basetype);

  size_t srcrowstride = pixelSize * RDCMAX(RDCMAX(width, 1), rowlength);
  size_t srcimgstride = srcrowstride * RDCMAX(RDCMAX(height, 1), imageheight);

  size_t destrowstride = pixelSize * width;
  size_t destimgstride = destrowstride * height;

  size_t elemSize = GLTypeSize(basetype);

  size_t allocsize = width * RDCMAX(1, height) * RDCMAX(1, depth) * pixelSize;
  byte *ret = new byte[allocsize];

  byte *source = pixels;

  if(skipPixels > 0)
    source += skipPixels * pixelSize;
  if(skipRows > 0 && height > 0)
    source += skipRows * srcrowstride;
  if(skipImages > 0 && depth > 0)
    source += skipImages * srcimgstride;

  size_t align = 1;
  // "If the number of bits per element is not 1, 2, 4, or 8 times the number of
  // bits in a GL ubyte, then k = nl for all values of a"
  // ie. alignment is only used for pixel formats with N-byte elements.
  if(basetype == eGL_UNSIGNED_BYTE || basetype == eGL_BYTE || basetype == eGL_UNSIGNED_SHORT ||
     basetype == eGL_SHORT || basetype == eGL_UNSIGNED_INT || basetype == eGL_INT ||
     basetype == eGL_HALF_FLOAT || basetype == eGL_FLOAT || basetype == eGL_UNSIGNED_INT_8_8_8_8 ||
     basetype == eGL_UNSIGNED_INT_8_8_8_8_REV)
  {
    align = RDCMAX(align, (size_t)alignment);
  }

  byte *dest = ret;

  for(GLsizei img = 0; img < RDCMAX(1, depth); img++)
  {
    byte *rowsource = source;
    byte *rowdest = dest;

    for(GLsizei row = 0; row < RDCMAX(1, height); row++)
    {
      memcpy(rowdest, rowsource, destrowstride);

      if(swapBytes && elemSize > 1)
      {
        for(size_t el = 0; el < pixelSize * width; el += elemSize)
        {
          byte *element = rowdest + el;

          if(elemSize == 2)
          {
            std::swap(element[0], element[1]);
          }
          else if(elemSize == 4)
          {
            std::swap(element[0], element[3]);
            std::swap(element[1], element[2]);
          }
          else if(elemSize == 8)
          {
            std::swap(element[0], element[7]);
            std::swap(element[1], element[6]);
            std::swap(element[2], element[5]);
            std::swap(element[3], element[4]);
          }
        }
      }

      rowdest += destrowstride;
      rowsource += srcrowstride;
      rowsource = (byte *)AlignUp((size_t)rowsource, align);
    }

    dest += destimgstride;
    source += srcimgstride;
    source = (byte *)AlignUp((size_t)source, align);
  }

  return ret;
}

byte *PixelUnpackState::UnpackCompressed(byte *pixels, GLsizei width, GLsizei height, GLsizei depth,
                                         GLsizei &imageSize)
{
  int blockWidth = RDCMAX(compressedBlockWidth, 1);
  int blockHeight = RDCMAX(compressedBlockHeight, 1);
  int blockDepth = RDCMAX(compressedBlockDepth, 1);
  int blockSize = RDCMAX(compressedBlockSize, 1);

  RDCASSERT(compressedBlockWidth != 0);
  RDCASSERT(compressedBlockSize != 0);

  size_t blocksX = width ? (width + blockWidth - 1) / blockWidth : 0;
  size_t blocksY = height ? (height + blockHeight - 1) / blockHeight : 0;
  size_t blocksZ = depth ? (depth + blockDepth - 1) / blockDepth : 0;

  if(height != 0)
    RDCASSERT(compressedBlockHeight != 0);

  if(depth != 0)
    RDCASSERT(compressedBlockDepth != 0);

  blocksX = RDCMAX((size_t)1, blocksX);
  blocksY = RDCMAX((size_t)1, blocksY);
  blocksZ = RDCMAX((size_t)1, blocksZ);

  size_t srcrowstride = blockSize * RDCMAX(RDCMAX(width, blockWidth), rowlength) / blockWidth;
  size_t srcimgstride = srcrowstride * RDCMAX(RDCMAX(height, blockHeight), imageheight) / blockHeight;

  size_t destrowstride = blockSize * RDCMAX(width, blockWidth) / blockWidth;
  size_t destimgstride = destrowstride * RDCMAX(height, blockHeight) / blockHeight;

  size_t allocsize = blocksX * blocksY * blocksZ * blockSize;
  byte *ret = new byte[allocsize];

  imageSize = (GLsizei)allocsize;

  byte *source = pixels;

  if(skipPixels > 0)
    source += (skipPixels / blockWidth) * blockSize;
  if(skipRows > 0 && height > 0)
    source += (skipRows / compressedBlockHeight) * srcrowstride;
  if(skipImages > 0 && depth > 0)
    source += skipImages * srcimgstride;

  byte *dest = ret;

  for(GLsizei img = 0; img < RDCMAX(1, depth); img++)
  {
    byte *rowsource = source;
    byte *rowdest = dest;

    for(size_t row = 0; row < blocksY; row++)
    {
      memcpy(rowdest, rowsource, destrowstride);

      rowsource += srcrowstride;
      rowdest += destrowstride;
    }

    source += srcimgstride;
    dest += destimgstride;
  }

  return ret;
}

GLRenderState::GLRenderState()
{
  Clear();

  for(GLuint i = 0; i < (GLuint)ARRAY_COUNT(Tex2D); i++)
  {
    Tex1D[i].Namespace = Tex2D[i].Namespace = Tex3D[i].Namespace = Tex1DArray[i].Namespace =
        Tex2DArray[i].Namespace = TexCubeArray[i].Namespace = TexRect[i].Namespace =
            TexBuffer[i].Namespace = TexCube[i].Namespace = Tex2DMS[i].Namespace =
                Tex2DMSArray[i].Namespace = eResTexture;
    Samplers[i].Namespace = eResSampler;
  }

  Program.Namespace = eResProgram;
  Pipeline.Namespace = eResProgramPipe;
  VAO.Namespace = eResVertexArray;

  FeedbackObj.Namespace = eResFeedback;

  for(GLuint i = 0; i < (GLuint)ARRAY_COUNT(BufferBindings); i++)
    BufferBindings[i].Namespace = eResBuffer;

  for(GLuint i = 0; i < (GLuint)ARRAY_COUNT(AtomicCounter); i++)
    AtomicCounter[i].res.Namespace = eResBuffer;
  for(GLuint i = 0; i < (GLuint)ARRAY_COUNT(ShaderStorage); i++)
    ShaderStorage[i].res.Namespace = eResBuffer;
  for(GLuint i = 0; i < (GLuint)ARRAY_COUNT(TransformFeedback); i++)
    TransformFeedback[i].res.Namespace = eResBuffer;
  for(GLuint i = 0; i < (GLuint)ARRAY_COUNT(UniformBinding); i++)
    UniformBinding[i].res.Namespace = eResBuffer;
  for(GLuint i = 0; i < (GLuint)ARRAY_COUNT(Images); i++)
    Images[i].res.Namespace = eResTexture;

  ReadFBO.Namespace = DrawFBO.Namespace = eResFramebuffer;
}

GLRenderState::~GLRenderState()
{
}

void GLRenderState::MarkReferenced(WrappedOpenGL *driver, bool initial) const
{
  GLResourceManager *manager = driver->GetResourceManager();

  for(GLuint i = 0; i < (GLuint)ARRAY_COUNT(Tex2D); i++)
  {
    manager->MarkResourceFrameReferenced(Tex1D[i], initial ? eFrameRef_None : eFrameRef_Read);
    manager->MarkResourceFrameReferenced(Tex2D[i], initial ? eFrameRef_None : eFrameRef_Read);
    manager->MarkResourceFrameReferenced(Tex3D[i], initial ? eFrameRef_None : eFrameRef_Read);
    manager->MarkResourceFrameReferenced(Tex1DArray[i], initial ? eFrameRef_None : eFrameRef_Read);
    manager->MarkResourceFrameReferenced(Tex2DArray[i], initial ? eFrameRef_None : eFrameRef_Read);
    manager->MarkResourceFrameReferenced(TexCubeArray[i], initial ? eFrameRef_None : eFrameRef_Read);
    manager->MarkResourceFrameReferenced(TexRect[i], initial ? eFrameRef_None : eFrameRef_Read);
    manager->MarkResourceFrameReferenced(TexBuffer[i], initial ? eFrameRef_None : eFrameRef_Read);
    manager->MarkResourceFrameReferenced(TexCube[i], initial ? eFrameRef_None : eFrameRef_Read);
    manager->MarkResourceFrameReferenced(Tex2DMS[i], initial ? eFrameRef_None : eFrameRef_Read);
    manager->MarkResourceFrameReferenced(Tex2DMSArray[i], initial ? eFrameRef_None : eFrameRef_Read);
    manager->MarkResourceFrameReferenced(Samplers[i], initial ? eFrameRef_None : eFrameRef_Read);
  }

  for(GLuint i = 0; i < (GLuint)ARRAY_COUNT(Images); i++)
  {
    manager->MarkResourceFrameReferenced(Images[i].res,
                                         initial ? eFrameRef_None : eFrameRef_ReadBeforeWrite);
  }

  manager->MarkVAOReferenced(VAO, initial ? eFrameRef_None : eFrameRef_Read, true);

  manager->MarkResourceFrameReferenced(FeedbackObj, initial ? eFrameRef_None : eFrameRef_Read);

  manager->MarkResourceFrameReferenced(Program, initial ? eFrameRef_None : eFrameRef_Read);
  manager->MarkResourceFrameReferenced(Pipeline, initial ? eFrameRef_None : eFrameRef_Read);

  // the pipeline correctly has program parents, but we must also mark the programs as frame
  // referenced so that their
  // initial contents will be serialised.
  GLResourceRecord *record = manager->GetResourceRecord(Pipeline);
  if(record)
    record->MarkParentsReferenced(manager, initial ? eFrameRef_None : eFrameRef_Read);

  for(size_t i = 0; i < ARRAY_COUNT(BufferBindings); i++)
    manager->MarkResourceFrameReferenced(BufferBindings[i],
                                         initial ? eFrameRef_None : eFrameRef_Read);

  for(size_t i = 0; i < ARRAY_COUNT(AtomicCounter); i++)
    manager->MarkResourceFrameReferenced(AtomicCounter[i].res,
                                         initial ? eFrameRef_None : eFrameRef_ReadBeforeWrite);

  for(size_t i = 0; i < ARRAY_COUNT(ShaderStorage); i++)
    manager->MarkResourceFrameReferenced(ShaderStorage[i].res,
                                         initial ? eFrameRef_None : eFrameRef_ReadBeforeWrite);

  for(size_t i = 0; i < ARRAY_COUNT(TransformFeedback); i++)
    manager->MarkResourceFrameReferenced(TransformFeedback[i].res,
                                         initial ? eFrameRef_None : eFrameRef_ReadBeforeWrite);

  for(size_t i = 0; i < ARRAY_COUNT(UniformBinding); i++)
    manager->MarkResourceFrameReferenced(UniformBinding[i].res,
                                         initial ? eFrameRef_None : eFrameRef_Read);

  manager->MarkFBOReferenced(DrawFBO, initial ? eFrameRef_None : eFrameRef_ReadBeforeWrite);

  // if same FBO is bound to both targets, treat it as draw only
  if(ReadFBO != DrawFBO)
    manager->MarkFBOReferenced(ReadFBO, initial ? eFrameRef_None : eFrameRef_Read);

  MarkDirty(driver);
}

void GLRenderState::MarkDirty(WrappedOpenGL *driver) const
{
  GLResourceManager *manager = driver->GetResourceManager();

  ContextPair &ctx = driver->GetCtx();

  GLint maxCount = 0;
  GLuint name = 0;

  if(HasExt[ARB_transform_feedback2])
  {
    GL.glGetIntegerv(eGL_MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS, &maxCount);

    for(GLint i = 0; i < maxCount; i++)
    {
      name = 0;
      GL.glGetIntegeri_v(eGL_TRANSFORM_FEEDBACK_BUFFER_BINDING, i, (GLint *)&name);

      if(name)
        manager->MarkDirtyResource(BufferRes(ctx, name));
    }
  }

  if(HasExt[ARB_shader_image_load_store])
  {
    GL.glGetIntegerv(eGL_MAX_IMAGE_UNITS, &maxCount);

    for(GLint i = 0; i < maxCount; i++)
    {
      name = 0;
      GL.glGetIntegeri_v(eGL_IMAGE_BINDING_NAME, i, (GLint *)&name);

      if(name)
        manager->MarkDirtyResource(TextureRes(ctx, name));
    }
  }

  if(HasExt[ARB_shader_atomic_counters])
  {
    GL.glGetIntegerv(eGL_MAX_ATOMIC_COUNTER_BUFFER_BINDINGS, &maxCount);

    for(GLint i = 0; i < maxCount; i++)
    {
      name = 0;
      GL.glGetIntegeri_v(eGL_ATOMIC_COUNTER_BUFFER_BINDING, i, (GLint *)&name);

      if(name)
        manager->MarkDirtyResource(BufferRes(ctx, name));
    }
  }

  if(HasExt[ARB_shader_storage_buffer_object])
  {
    GL.glGetIntegerv(eGL_MAX_SHADER_STORAGE_BUFFER_BINDINGS, &maxCount);

    for(GLint i = 0; i < maxCount; i++)
    {
      name = 0;
      GL.glGetIntegeri_v(eGL_SHADER_STORAGE_BUFFER_BINDING, i, (GLint *)&name);

      if(name)
        manager->MarkDirtyResource(BufferRes(ctx, name));
    }
  }

  GL.glGetIntegerv(eGL_MAX_COLOR_ATTACHMENTS, &maxCount);

  GL.glGetIntegerv(eGL_DRAW_FRAMEBUFFER_BINDING, (GLint *)&name);

  if(name)
  {
    GLenum type = eGL_TEXTURE;
    for(GLint i = 0; i < maxCount; i++)
    {
      GL.glGetFramebufferAttachmentParameteriv(
          eGL_DRAW_FRAMEBUFFER, GLenum(eGL_COLOR_ATTACHMENT0 + i),
          eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, (GLint *)&name);
      GL.glGetFramebufferAttachmentParameteriv(
          eGL_DRAW_FRAMEBUFFER, GLenum(eGL_COLOR_ATTACHMENT0 + i),
          eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, (GLint *)&type);

      if(name)
      {
        if(type == eGL_RENDERBUFFER)
          manager->MarkDirtyResource(RenderbufferRes(ctx, name));
        else
          manager->MarkDirtyResource(TextureRes(ctx, name));
      }
    }

    GL.glGetFramebufferAttachmentParameteriv(eGL_DRAW_FRAMEBUFFER, eGL_DEPTH_ATTACHMENT,
                                             eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, (GLint *)&name);
    GL.glGetFramebufferAttachmentParameteriv(eGL_DRAW_FRAMEBUFFER, eGL_DEPTH_ATTACHMENT,
                                             eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, (GLint *)&type);

    if(name)
    {
      if(type == eGL_RENDERBUFFER)
        manager->MarkDirtyResource(RenderbufferRes(ctx, name));
      else
        manager->MarkDirtyResource(TextureRes(ctx, name));
    }

    GL.glGetFramebufferAttachmentParameteriv(eGL_DRAW_FRAMEBUFFER, eGL_STENCIL_ATTACHMENT,
                                             eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, (GLint *)&name);
    GL.glGetFramebufferAttachmentParameteriv(eGL_DRAW_FRAMEBUFFER, eGL_STENCIL_ATTACHMENT,
                                             eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, (GLint *)&type);

    if(name)
    {
      if(type == eGL_RENDERBUFFER)
        manager->MarkDirtyResource(RenderbufferRes(ctx, name));
      else
        manager->MarkDirtyResource(TextureRes(ctx, name));
    }
  }
}

bool GLRenderState::CheckEnableDisableParam(GLenum pname)
{
  RDCCOMPILE_ASSERT(ARRAY_COUNT(enable_disable_cap) == eEnabled_Count,
                    "Wrong number of capabilities");

  if(IsGLES)
  {
    switch(pname)
    {
      case eGL_COLOR_LOGIC_OP:
      case eGL_DEPTH_CLAMP:
      case eGL_DEPTH_BOUNDS_TEST_EXT:
      case eGL_LINE_SMOOTH:
      case eGL_POLYGON_SMOOTH:
      case eGL_PROGRAM_POINT_SIZE:
      case eGL_PRIMITIVE_RESTART:
      case eGL_TEXTURE_CUBE_MAP_SEAMLESS:
        // these are not supported by OpenGL ES
        return false;

      case eGL_POLYGON_OFFSET_LINE:
      case eGL_POLYGON_OFFSET_POINT:
        // these are in GL_NV_polygon_mode, however they are not accepted by the NVIDIA driver
        // see DoVendorChecks()
        return false;

      case eGL_SAMPLE_MASK:
        return HasExt[ARB_texture_multisample_no_array] || HasExt[ARB_texture_multisample];

      case eGL_CLIP_DISTANCE0:
      case eGL_CLIP_DISTANCE1:
      case eGL_CLIP_DISTANCE2:
      case eGL_CLIP_DISTANCE3:
      case eGL_CLIP_DISTANCE4:
      case eGL_CLIP_DISTANCE5:
      case eGL_CLIP_DISTANCE6:
      case eGL_CLIP_DISTANCE7: return HasExt[EXT_clip_cull_distance];

      case eGL_SAMPLE_ALPHA_TO_ONE:
      case eGL_MULTISAMPLE: return HasExt[EXT_multisample_compatibility];

      case eGL_SAMPLE_SHADING: return HasExt[ARB_sample_shading];

      case eGL_FRAMEBUFFER_SRGB: return HasExt[EXT_framebuffer_sRGB];

      default: break;
    }
  }
  else
  {
    switch(pname)
    {
      case eGL_DEPTH_BOUNDS_TEST_EXT: return HasExt[EXT_depth_bounds_test];
      case eGL_SAMPLE_SHADING: return HasExt[ARB_sample_shading];
      case eGL_PRIMITIVE_RESTART_FIXED_INDEX: return HasExt[ARB_ES3_compatibility];
      default: break;
    }
  }

  // both OpenGL and OpenGL ES
  switch(pname)
  {
    case eGL_BLEND_ADVANCED_COHERENT_KHR: return HasExt[KHR_blend_equation_advanced_coherent];
    case eGL_RASTER_MULTISAMPLE_EXT: return HasExt[EXT_raster_multisample];
    case eGL_RASTERIZER_DISCARD: return HasExt[EXT_transform_feedback];
    default: break;
  }

  return true;
}

void GLRenderState::FetchState(WrappedOpenGL *driver)
{
  ContextPair &ctx = driver->GetCtx();

  if(ctx.ctx == NULL)
  {
    ContextPresent = false;
    return;
  }

  for(GLuint i = 0; i < eEnabled_Count; i++)
  {
    if(!CheckEnableDisableParam(enable_disable_cap[i].cap))
    {
      Enabled[i] = false;
      continue;
    }

    Enabled[i] = (GL.glIsEnabled(enable_disable_cap[i].cap) == GL_TRUE);
  }

  GL.glGetIntegerv(eGL_ACTIVE_TEXTURE, (GLint *)&ActiveTexture);

  GLuint maxTextures = 0;
  GL.glGetIntegerv(eGL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, (GLint *)&maxTextures);

  RDCCOMPILE_ASSERT(
      sizeof(Tex1D) == sizeof(Tex2D) && sizeof(Tex2D) == sizeof(Tex3D) &&
          sizeof(Tex3D) == sizeof(Tex1DArray) && sizeof(Tex1DArray) == sizeof(Tex2DArray) &&
          sizeof(Tex2DArray) == sizeof(TexCubeArray) && sizeof(TexCubeArray) == sizeof(TexRect) &&
          sizeof(TexRect) == sizeof(TexBuffer) && sizeof(TexBuffer) == sizeof(TexCube) &&
          sizeof(TexCube) == sizeof(Tex2DMS) && sizeof(Tex2DMS) == sizeof(Tex2DMSArray) &&
          sizeof(Tex2DMSArray) == sizeof(Samplers),
      "All texture arrays should be identically sized");

  for(GLuint i = 0; i < RDCMIN(maxTextures, (GLuint)ARRAY_COUNT(Tex2D)); i++)
  {
    GL.glActiveTexture(GLenum(eGL_TEXTURE0 + i));

    // textures are always shared
    Tex1D[i].ContextShareGroup = ctx.shareGroup;
    Tex2D[i].ContextShareGroup = ctx.shareGroup;
    Tex3D[i].ContextShareGroup = ctx.shareGroup;
    Tex1DArray[i].ContextShareGroup = ctx.shareGroup;
    Tex2DArray[i].ContextShareGroup = ctx.shareGroup;
    TexCube[i].ContextShareGroup = ctx.shareGroup;
    TexRect[i].ContextShareGroup = ctx.shareGroup;
    TexBuffer[i].ContextShareGroup = ctx.shareGroup;
    Tex2DMS[i].ContextShareGroup = ctx.shareGroup;
    Tex2DMSArray[i].ContextShareGroup = ctx.shareGroup;
    TexCubeArray[i].ContextShareGroup = ctx.shareGroup;
    Samplers[i].ContextShareGroup = ctx.shareGroup;

    if(!IsGLES)
      GL.glGetIntegerv(eGL_TEXTURE_BINDING_1D, (GLint *)&Tex1D[i].name);
    else
      Tex1D[i].name = 0;

    GL.glGetIntegerv(eGL_TEXTURE_BINDING_2D, (GLint *)&Tex2D[i].name);
    GL.glGetIntegerv(eGL_TEXTURE_BINDING_3D, (GLint *)&Tex3D[i].name);

    if(!IsGLES)
      GL.glGetIntegerv(eGL_TEXTURE_BINDING_1D_ARRAY, (GLint *)&Tex1DArray[i].name);
    else
      Tex1DArray[i].name = 0;

    GL.glGetIntegerv(eGL_TEXTURE_BINDING_2D_ARRAY, (GLint *)&Tex2DArray[i].name);
    GL.glGetIntegerv(eGL_TEXTURE_BINDING_CUBE_MAP, (GLint *)&TexCube[i].name);

    if(!IsGLES)
      GL.glGetIntegerv(eGL_TEXTURE_BINDING_RECTANGLE, (GLint *)&TexRect[i].name);
    else
      TexRect[i].name = 0;

    if(HasExt[ARB_texture_buffer_object])
      GL.glGetIntegerv(eGL_TEXTURE_BINDING_BUFFER, (GLint *)&TexBuffer[i].name);
    else
      TexBuffer[i].name = 0;

    if(HasExt[ARB_texture_multisample_no_array] || HasExt[ARB_texture_multisample])
      GL.glGetIntegerv(eGL_TEXTURE_BINDING_2D_MULTISAMPLE, (GLint *)&Tex2DMS[i].name);
    else
      Tex2DMS[i].name = 0;

    if(HasExt[ARB_texture_multisample])
      GL.glGetIntegerv(eGL_TEXTURE_BINDING_2D_MULTISAMPLE_ARRAY, (GLint *)&Tex2DMSArray[i].name);
    else
      Tex2DMSArray[i].name = 0;

    if(HasExt[ARB_texture_cube_map_array])
      GL.glGetIntegerv(eGL_TEXTURE_BINDING_CUBE_MAP_ARRAY, (GLint *)&TexCubeArray[i].name);
    else
      TexCubeArray[i].name = 0;

    if(HasExt[ARB_sampler_objects])
      GL.glGetIntegerv(eGL_SAMPLER_BINDING, (GLint *)&Samplers[i].name);
    else
      Samplers[i].name = 0;
  }

  if(HasExt[ARB_shader_image_load_store])
  {
    GLuint maxImages = 0;
    GL.glGetIntegerv(eGL_MAX_IMAGE_UNITS, (GLint *)&maxImages);

    for(GLuint i = 0; i < RDCMIN(maxImages, (GLuint)ARRAY_COUNT(Images)); i++)
    {
      GLboolean layered = GL_FALSE;

      // textures are always shared
      Images[i].res.ContextShareGroup = ctx.shareGroup;

      GL.glGetIntegeri_v(eGL_IMAGE_BINDING_NAME, i, (GLint *)&Images[i].res.name);
      GL.glGetIntegeri_v(eGL_IMAGE_BINDING_LEVEL, i, (GLint *)&Images[i].level);
      GL.glGetIntegeri_v(eGL_IMAGE_BINDING_ACCESS, i, (GLint *)&Images[i].access);
      GL.glGetIntegeri_v(eGL_IMAGE_BINDING_FORMAT, i, (GLint *)&Images[i].format);
      GL.glGetBooleani_v(eGL_IMAGE_BINDING_LAYERED, i, &layered);
      Images[i].layered = (layered == GL_TRUE);
      if(layered)
        GL.glGetIntegeri_v(eGL_IMAGE_BINDING_LAYER, i, (GLint *)&Images[i].layer);
    }
  }

  GL.glActiveTexture(ActiveTexture);

  {
    GLuint name = 0;
    GL.glGetIntegerv(eGL_VERTEX_ARRAY_BINDING, (GLint *)&name);
    VAO = VertexArrayRes(ctx, name);
  }

  if(HasExt[ARB_transform_feedback2])
  {
    GLuint name = 0;
    GL.glGetIntegerv(eGL_TRANSFORM_FEEDBACK_BINDING, (GLint *)&name);
    FeedbackObj = FeedbackRes(ctx, name);
  }

  // the spec says that you can only query for the format that was previously set, or you get
  // undefined results. Ie. if someone set ints, this might return anything. However there's also
  // no way to query for the type so we just have to hope for the best and hope most people are
  // sane and don't use these except for a default "all 0s" attrib.

  GLuint maxNumAttribs = 0;
  GL.glGetIntegerv(eGL_MAX_VERTEX_ATTRIBS, (GLint *)&maxNumAttribs);
  for(GLuint i = 0; i < RDCMIN(maxNumAttribs, (GLuint)ARRAY_COUNT(GenericVertexAttribs)); i++)
    GL.glGetVertexAttribfv(i, eGL_CURRENT_VERTEX_ATTRIB, &GenericVertexAttribs[i].x);

  GL.glGetFloatv(eGL_LINE_WIDTH, &LineWidth);
  if(!IsGLES)
  {
    GL.glGetFloatv(eGL_POINT_FADE_THRESHOLD_SIZE, &PointFadeThresholdSize);
    GL.glGetIntegerv(eGL_POINT_SPRITE_COORD_ORIGIN, (GLint *)&PointSpriteOrigin);
    GL.glGetFloatv(eGL_POINT_SIZE, &PointSize);
  }

  if(!IsGLES)
    GL.glGetIntegerv(eGL_PRIMITIVE_RESTART_INDEX, (GLint *)&PrimitiveRestartIndex);
  if(HasExt[ARB_clip_control])
  {
    GL.glGetIntegerv(eGL_CLIP_ORIGIN, (GLint *)&ClipOrigin);
    GL.glGetIntegerv(eGL_CLIP_DEPTH_MODE, (GLint *)&ClipDepth);
  }
  else
  {
    ClipOrigin = eGL_LOWER_LEFT;
    ClipDepth = eGL_NEGATIVE_ONE_TO_ONE;
  }
  if(!IsGLES)
    GL.glGetIntegerv(eGL_PROVOKING_VERTEX, (GLint *)&ProvokingVertex);

  {
    GLuint name = 0;
    GL.glGetIntegerv(eGL_CURRENT_PROGRAM, (GLint *)&name);
    Program = ProgramRes(ctx, name);
  }

  if(HasExt[ARB_separate_shader_objects])
  {
    GLuint name = 0;
    GL.glGetIntegerv(eGL_PROGRAM_PIPELINE_BINDING, (GLint *)&name);
    Pipeline = ProgramPipeRes(ctx, name);
  }
  else
  {
    Pipeline = ProgramRes(ctx, 0);
  }

  const GLenum shs[] = {
      eGL_VERTEX_SHADER,   eGL_TESS_CONTROL_SHADER, eGL_TESS_EVALUATION_SHADER,
      eGL_GEOMETRY_SHADER, eGL_FRAGMENT_SHADER,     eGL_COMPUTE_SHADER,
  };

  if(HasExt[ARB_shader_subroutine])
  {
    RDCCOMPILE_ASSERT(ARRAY_COUNT(shs) == ARRAY_COUNT(Subroutines),
                      "Subroutine array not the right size");

    for(size_t s = 0; s < ARRAY_COUNT(shs); s++)
    {
      if(shs[s] == eGL_COMPUTE_SHADER && !HasExt[ARB_compute_shader])
        continue;

      if((shs[s] == eGL_TESS_CONTROL_SHADER || shs[s] == eGL_TESS_EVALUATION_SHADER) &&
         !HasExt[ARB_tessellation_shader])
        continue;

      GLuint prog = Program.name;
      if(prog == 0 && Pipeline.name != 0)
      {
        // can't query for GL_COMPUTE_SHADER on some AMD cards
        if(shs[s] != eGL_COMPUTE_SHADER || !VendorCheck[VendorCheck_AMD_pipeline_compute_query])
          GL.glGetProgramPipelineiv(Pipeline.name, shs[s], (GLint *)&prog);
      }

      if(prog == 0)
        continue;

      GLint numSubroutines = 0;
      GL.glGetProgramStageiv(prog, shs[s], eGL_ACTIVE_SUBROUTINES, &numSubroutines);

      if(numSubroutines == 0)
        continue;

      GL.glGetProgramStageiv(prog, shs[s], eGL_ACTIVE_SUBROUTINE_UNIFORM_LOCATIONS,
                             &Subroutines[s].numSubroutines);

      for(GLint i = 0; i < Subroutines[s].numSubroutines; i++)
        GL.glGetUniformSubroutineuiv(shs[s], i, &Subroutines[s].Values[0]);
    }
  }
  else
  {
    RDCEraseEl(Subroutines);
  }

  // buffers are always shared
  for(size_t i = 0; i < ARRAY_COUNT(BufferBindings); i++)
    BufferBindings[i].ContextShareGroup = ctx.shareGroup;

  GL.glGetIntegerv(eGL_ARRAY_BUFFER_BINDING, (GLint *)&BufferBindings[eBufIdx_Array].name);
  GL.glGetIntegerv(eGL_COPY_READ_BUFFER_BINDING, (GLint *)&BufferBindings[eBufIdx_Copy_Read].name);
  GL.glGetIntegerv(eGL_COPY_WRITE_BUFFER_BINDING, (GLint *)&BufferBindings[eBufIdx_Copy_Write].name);
  GL.glGetIntegerv(eGL_PIXEL_PACK_BUFFER_BINDING, (GLint *)&BufferBindings[eBufIdx_Pixel_Pack].name);
  GL.glGetIntegerv(eGL_PIXEL_UNPACK_BUFFER_BINDING,
                   (GLint *)&BufferBindings[eBufIdx_Pixel_Unpack].name);
  if(HasExt[ARB_texture_buffer_object])
    GL.glGetIntegerv(eGL_TEXTURE_BUFFER_BINDING, (GLint *)&BufferBindings[eBufIdx_Texture].name);

  if(HasExt[ARB_draw_indirect])
    GL.glGetIntegerv(eGL_DRAW_INDIRECT_BUFFER_BINDING,
                     (GLint *)&BufferBindings[eBufIdx_Draw_Indirect].name);
  if(HasExt[ARB_compute_shader])
    GL.glGetIntegerv(eGL_DISPATCH_INDIRECT_BUFFER_BINDING,
                     (GLint *)&BufferBindings[eBufIdx_Dispatch_Indirect].name);
  if(HasExt[ARB_query_buffer_object])
    GL.glGetIntegerv(eGL_QUERY_BUFFER_BINDING, (GLint *)&BufferBindings[eBufIdx_Query].name);
  if(HasExt[ARB_indirect_parameters])
    GL.glGetIntegerv(eGL_PARAMETER_BUFFER_BINDING_ARB,
                     (GLint *)&BufferBindings[eBufIdx_Parameter].name);

  struct
  {
    IdxRangeBuffer *bufs;
    int count;
    GLenum binding;
    GLenum start;
    GLenum size;
    GLenum maxcount;
  } idxBufs[] = {
      {
          AtomicCounter, ARRAY_COUNT(AtomicCounter), eGL_ATOMIC_COUNTER_BUFFER_BINDING,
          eGL_ATOMIC_COUNTER_BUFFER_START, eGL_ATOMIC_COUNTER_BUFFER_SIZE,
          eGL_MAX_ATOMIC_COUNTER_BUFFER_BINDINGS,
      },
      {
          ShaderStorage, ARRAY_COUNT(ShaderStorage), eGL_SHADER_STORAGE_BUFFER_BINDING,
          eGL_SHADER_STORAGE_BUFFER_START, eGL_SHADER_STORAGE_BUFFER_SIZE,
          eGL_MAX_SHADER_STORAGE_BUFFER_BINDINGS,
      },
      {
          TransformFeedback, ARRAY_COUNT(TransformFeedback), eGL_TRANSFORM_FEEDBACK_BUFFER_BINDING,
          eGL_TRANSFORM_FEEDBACK_BUFFER_START, eGL_TRANSFORM_FEEDBACK_BUFFER_SIZE,
          eGL_MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS,
      },
      {
          UniformBinding, ARRAY_COUNT(UniformBinding), eGL_UNIFORM_BUFFER_BINDING,
          eGL_UNIFORM_BUFFER_START, eGL_UNIFORM_BUFFER_SIZE, eGL_MAX_UNIFORM_BUFFER_BINDINGS,
      },
  };

  for(GLuint b = 0; b < (GLuint)ARRAY_COUNT(idxBufs); b++)
  {
    if(idxBufs[b].binding == eGL_ATOMIC_COUNTER_BUFFER_BINDING && !HasExt[ARB_shader_atomic_counters])
      continue;

    if(idxBufs[b].binding == eGL_SHADER_STORAGE_BUFFER_BINDING &&
       !HasExt[ARB_shader_storage_buffer_object])
      continue;

    if(idxBufs[b].binding == eGL_TRANSFORM_FEEDBACK_BUFFER_BINDING && !HasExt[ARB_transform_feedback2])
      continue;

    GLint maxCount = 0;
    GL.glGetIntegerv(idxBufs[b].maxcount, &maxCount);
    for(int i = 0; i < idxBufs[b].count && i < maxCount; i++)
    {
      // buffers are always shared
      idxBufs[b].bufs[i].res.ContextShareGroup = ctx.shareGroup;

      GL.glGetIntegeri_v(idxBufs[b].binding, i, (GLint *)&idxBufs[b].bufs[i].res.name);
      GL.glGetInteger64i_v(idxBufs[b].start, i, (GLint64 *)&idxBufs[b].bufs[i].start);
      GL.glGetInteger64i_v(idxBufs[b].size, i, (GLint64 *)&idxBufs[b].bufs[i].size);
    }
  }

  GLuint maxDraws = 0;
  GL.glGetIntegerv(eGL_MAX_DRAW_BUFFERS, (GLint *)&maxDraws);

  if(HasExt[ARB_draw_buffers_blend])
  {
    for(GLuint i = 0; i < RDCMIN(maxDraws, (GLuint)ARRAY_COUNT(Blends)); i++)
    {
      GL.glGetIntegeri_v(eGL_BLEND_EQUATION_RGB, i, (GLint *)&Blends[i].EquationRGB);
      GL.glGetIntegeri_v(eGL_BLEND_EQUATION_ALPHA, i, (GLint *)&Blends[i].EquationAlpha);

      GL.glGetIntegeri_v(eGL_BLEND_SRC_RGB, i, (GLint *)&Blends[i].SourceRGB);
      GL.glGetIntegeri_v(eGL_BLEND_SRC_ALPHA, i, (GLint *)&Blends[i].SourceAlpha);

      GL.glGetIntegeri_v(eGL_BLEND_DST_RGB, i, (GLint *)&Blends[i].DestinationRGB);
      GL.glGetIntegeri_v(eGL_BLEND_DST_ALPHA, i, (GLint *)&Blends[i].DestinationAlpha);

      Blends[i].Enabled = (GL.glIsEnabledi(eGL_BLEND, i) == GL_TRUE);
    }
  }
  else
  {
    // if we don't have separate blending, then replicate across all from 0

    GL.glGetIntegerv(eGL_BLEND_EQUATION_RGB, (GLint *)&Blends[0].EquationRGB);
    GL.glGetIntegerv(eGL_BLEND_EQUATION_ALPHA, (GLint *)&Blends[0].EquationAlpha);

    GL.glGetIntegerv(eGL_BLEND_SRC_RGB, (GLint *)&Blends[0].SourceRGB);
    GL.glGetIntegerv(eGL_BLEND_SRC_ALPHA, (GLint *)&Blends[0].SourceAlpha);

    GL.glGetIntegerv(eGL_BLEND_DST_RGB, (GLint *)&Blends[0].DestinationRGB);
    GL.glGetIntegerv(eGL_BLEND_DST_ALPHA, (GLint *)&Blends[0].DestinationAlpha);

    Blends[0].Enabled = (GL.glIsEnabled(eGL_BLEND) == GL_TRUE);

    for(GLuint i = 1; i < (GLuint)ARRAY_COUNT(Blends); i++)
      memcpy(&Blends[i], &Blends[0], sizeof(Blends[i]));
  }

  GL.glGetFloatv(eGL_BLEND_COLOR, &BlendColor[0]);

  if(HasExt[ARB_viewport_array])
  {
    GLuint maxViews = 0;
    GL.glGetIntegerv(eGL_MAX_VIEWPORTS, (GLint *)&maxViews);

    for(GLuint i = 0; i < RDCMIN(maxViews, (GLuint)ARRAY_COUNT(Viewports)); i++)
      GL.glGetFloati_v(eGL_VIEWPORT, i, &Viewports[i].x);

    for(GLuint i = 0; i < RDCMIN(maxViews, (GLuint)ARRAY_COUNT(Scissors)); i++)
    {
      GL.glGetIntegeri_v(eGL_SCISSOR_BOX, i, &Scissors[i].x);
      Scissors[i].enabled = (GL.glIsEnabledi(eGL_SCISSOR_TEST, i) == GL_TRUE);
    }

    for(GLuint i = 0; i < RDCMIN(maxViews, (GLuint)ARRAY_COUNT(DepthRanges)); i++)
    {
      if(IsGLES)
      {
        float v[2];
        GL.glGetFloati_v(eGL_DEPTH_RANGE, i, v);
        DepthRanges[i] = {(double)v[0], (double)v[1]};
      }
      else
      {
        GL.glGetDoublei_v(eGL_DEPTH_RANGE, i, &DepthRanges[i].nearZ);
      }
    }
  }
  else
  {
    // if we don't have separate viewport/etc, then replicate across all from 0
    // note that the same extension introduced indexed viewports, scissors and
    // depth ranges. Convenient!

    GL.glGetFloatv(eGL_VIEWPORT, &Viewports[0].x);
    GL.glGetIntegerv(eGL_SCISSOR_BOX, &Scissors[0].x);
    Scissors[0].enabled = (GL.glIsEnabled(eGL_SCISSOR_TEST) == GL_TRUE);
    if(IsGLES)
    {
      float v[2];
      GL.glGetFloatv(eGL_DEPTH_RANGE, v);
      DepthRanges[0] = {(double)v[0], (double)v[1]};
    }
    else
    {
      GL.glGetDoublev(eGL_DEPTH_RANGE, &DepthRanges[0].nearZ);
    }

    for(GLuint i = 1; i < (GLuint)ARRAY_COUNT(Viewports); i++)
      memcpy(&Viewports[i], &Viewports[0], sizeof(Viewports[i]));

    for(GLuint i = 1; i < (GLuint)ARRAY_COUNT(Scissors); i++)
      memcpy(&Scissors[i], &Scissors[0], sizeof(Scissors[i]));

    for(GLuint i = 1; i < (GLuint)ARRAY_COUNT(DepthRanges); i++)
      memcpy(&DepthRanges[i], &DepthRanges[0], sizeof(DepthRanges[i]));
  }

  {
    GLuint draw, read;
    GL.glGetIntegerv(eGL_DRAW_FRAMEBUFFER_BINDING, (GLint *)&draw);
    GL.glGetIntegerv(eGL_READ_FRAMEBUFFER_BINDING, (GLint *)&read);
    DrawFBO = FramebufferRes(ctx, draw);
    ReadFBO = FramebufferRes(ctx, read);

    // if the default FBO is bound, we must force the use of the context itself, rather than the
    // sharegroup (if FBOs are normally shared).
    if(draw == 0)
      DrawFBO = FramebufferRes({ctx.ctx, ctx.ctx}, draw);
    if(read == 0)
      ReadFBO = FramebufferRes({ctx.ctx, ctx.ctx}, read);
  }

  GL.glBindFramebuffer(eGL_DRAW_FRAMEBUFFER, 0);
  GL.glBindFramebuffer(eGL_READ_FRAMEBUFFER, 0);

  for(GLuint i = 0; i < RDCMIN(maxDraws, (GLuint)ARRAY_COUNT(DrawBuffers)); i++)
    GL.glGetIntegerv(GLenum(eGL_DRAW_BUFFER0 + i), (GLint *)&DrawBuffers[i]);

  GL.glGetIntegerv(eGL_READ_BUFFER, (GLint *)&ReadBuffer);

  GL.glBindFramebuffer(eGL_DRAW_FRAMEBUFFER, DrawFBO.name);
  GL.glBindFramebuffer(eGL_READ_FRAMEBUFFER, ReadFBO.name);

  GL.glGetIntegerv(eGL_FRAGMENT_SHADER_DERIVATIVE_HINT, (GLint *)&Hints.Derivatives);
  if(!IsGLES)
  {
    GL.glGetIntegerv(eGL_LINE_SMOOTH_HINT, (GLint *)&Hints.LineSmooth);
    GL.glGetIntegerv(eGL_POLYGON_SMOOTH_HINT, (GLint *)&Hints.PolySmooth);
    GL.glGetIntegerv(eGL_TEXTURE_COMPRESSION_HINT, (GLint *)&Hints.TexCompression);
  }

  GL.glGetBooleanv(eGL_DEPTH_WRITEMASK, &DepthWriteMask);
  GL.glGetFloatv(eGL_DEPTH_CLEAR_VALUE, &DepthClearValue);
  GL.glGetIntegerv(eGL_DEPTH_FUNC, (GLint *)&DepthFunc);

  if(HasExt[EXT_depth_bounds_test])
  {
    GL.glGetDoublev(eGL_DEPTH_BOUNDS_EXT, &DepthBounds.nearZ);
  }
  else
  {
    DepthBounds.nearZ = 0.0f;
    DepthBounds.farZ = 1.0f;
  }

  {
    GL.glGetIntegerv(eGL_STENCIL_FUNC, (GLint *)&StencilFront.func);
    GL.glGetIntegerv(eGL_STENCIL_BACK_FUNC, (GLint *)&StencilBack.func);

    GL.glGetIntegerv(eGL_STENCIL_REF, (GLint *)&StencilFront.ref);
    GL.glGetIntegerv(eGL_STENCIL_BACK_REF, (GLint *)&StencilBack.ref);

    GLint maskval;
    GL.glGetIntegerv(eGL_STENCIL_VALUE_MASK, &maskval);
    StencilFront.valuemask = uint8_t(maskval & 0xff);
    GL.glGetIntegerv(eGL_STENCIL_BACK_VALUE_MASK, &maskval);
    StencilBack.valuemask = uint8_t(maskval & 0xff);

    GL.glGetIntegerv(eGL_STENCIL_WRITEMASK, &maskval);
    StencilFront.writemask = uint8_t(maskval & 0xff);
    GL.glGetIntegerv(eGL_STENCIL_BACK_WRITEMASK, &maskval);
    StencilBack.writemask = uint8_t(maskval & 0xff);

    GL.glGetIntegerv(eGL_STENCIL_FAIL, (GLint *)&StencilFront.stencilFail);
    GL.glGetIntegerv(eGL_STENCIL_BACK_FAIL, (GLint *)&StencilBack.stencilFail);

    GL.glGetIntegerv(eGL_STENCIL_PASS_DEPTH_FAIL, (GLint *)&StencilFront.depthFail);
    GL.glGetIntegerv(eGL_STENCIL_BACK_PASS_DEPTH_FAIL, (GLint *)&StencilBack.depthFail);

    GL.glGetIntegerv(eGL_STENCIL_PASS_DEPTH_PASS, (GLint *)&StencilFront.pass);
    GL.glGetIntegerv(eGL_STENCIL_BACK_PASS_DEPTH_PASS, (GLint *)&StencilBack.pass);
  }

  GL.glGetIntegerv(eGL_STENCIL_CLEAR_VALUE, (GLint *)&StencilClearValue);

  if(HasExt[EXT_draw_buffers2] || HasExt[ARB_draw_buffers_blend])
  {
    for(GLuint i = 0; i < RDCMIN(maxDraws, (GLuint)ARRAY_COUNT(ColorMasks)); i++)
      GL.glGetBooleani_v(eGL_COLOR_WRITEMASK, i, &ColorMasks[i].red);
  }
  else
  {
    GL.glGetBooleanv(eGL_COLOR_WRITEMASK, &ColorMasks[0].red);
    for(size_t i = 1; i < ARRAY_COUNT(ColorMasks); i++)
      memcpy(&ColorMasks[i], &ColorMasks[0], sizeof(ColorMask));
  }

  if(HasExt[ARB_texture_multisample_no_array] || HasExt[ARB_texture_multisample])
    GL.glGetIntegeri_v(eGL_SAMPLE_MASK_VALUE, 0, (GLint *)&SampleMask[0]);

  GL.glGetFloatv(eGL_SAMPLE_COVERAGE_VALUE, &SampleCoverage);

  {
    GLint invert = 0;
    GL.glGetIntegerv(eGL_SAMPLE_COVERAGE_INVERT, (GLint *)&invert);
    SampleCoverageInvert = (invert != 0);
  }

  if(HasExt[ARB_sample_shading])
    GL.glGetFloatv(eGL_MIN_SAMPLE_SHADING_VALUE, &MinSampleShading);
  else
    MinSampleShading = 0;

  if(HasExt[EXT_raster_multisample])
    GL.glGetIntegerv(eGL_RASTER_SAMPLES_EXT, (GLint *)&RasterSamples);
  else
    RasterSamples = 0;

  if(HasExt[EXT_raster_multisample])
    GL.glGetIntegerv(eGL_RASTER_FIXED_SAMPLE_LOCATIONS_EXT, (GLint *)&RasterFixed);
  else
    RasterFixed = false;

  if(!IsGLES)
    GL.glGetIntegerv(eGL_LOGIC_OP_MODE, (GLint *)&LogicOp);

  GL.glGetFloatv(eGL_COLOR_CLEAR_VALUE, &ColorClearValue.red);

  if(HasExt[ARB_tessellation_shader])
    GL.glGetIntegerv(eGL_PATCH_VERTICES, &PatchParams.numVerts);
  else
    PatchParams.numVerts = 3;

  if(!IsGLES && HasExt[ARB_tessellation_shader])
  {
    GL.glGetFloatv(eGL_PATCH_DEFAULT_INNER_LEVEL, &PatchParams.defaultInnerLevel[0]);
    GL.glGetFloatv(eGL_PATCH_DEFAULT_OUTER_LEVEL, &PatchParams.defaultOuterLevel[0]);
  }
  else
  {
    PatchParams.defaultInnerLevel[0] = PatchParams.defaultInnerLevel[1] = 1.0f;
    PatchParams.defaultOuterLevel[0] = PatchParams.defaultOuterLevel[1] =
        PatchParams.defaultOuterLevel[2] = PatchParams.defaultOuterLevel[3] = 1.0f;
  }

  if(!VendorCheck[VendorCheck_AMD_polygon_mode_query] && !IsGLES)
  {
    // This was listed in docs as enumeration[2] even though polygon mode can't be set independently
    // for front
    // and back faces for a while, so pass large enough array to be sure.
    // AMD driver claims this doesn't exist anymore in core, so don't return any value, set to
    // default GL_FILL to be safe
    GLenum dummy[2] = {eGL_FILL, eGL_FILL};
    GL.glGetIntegerv(eGL_POLYGON_MODE, (GLint *)&dummy);
    PolygonMode = dummy[0];
  }
  else
  {
    PolygonMode = eGL_FILL;
  }

  GL.glGetFloatv(eGL_POLYGON_OFFSET_FACTOR, &PolygonOffset[0]);
  GL.glGetFloatv(eGL_POLYGON_OFFSET_UNITS, &PolygonOffset[1]);
  if(HasExt[ARB_polygon_offset_clamp])
    GL.glGetFloatv(eGL_POLYGON_OFFSET_CLAMP, &PolygonOffset[2]);
  else
    PolygonOffset[2] = 0.0f;

  GL.glGetIntegerv(eGL_FRONT_FACE, (GLint *)&FrontFace);
  GL.glGetIntegerv(eGL_CULL_FACE_MODE, (GLint *)&CullFace);

  if(IsGLES && (HasExt[EXT_primitive_bounding_box] || HasExt[OES_primitive_bounding_box]))
    GL.glGetFloatv(eGL_PRIMITIVE_BOUNDING_BOX_EXT, (GLfloat *)&PrimitiveBoundingBox);

  Unpack.Fetch(true);

  ClearGLErrors();
}

void GLRenderState::ApplyState(WrappedOpenGL *driver)
{
  ContextPair &ctx = driver->GetCtx();

  if(!ContextPresent || ctx.ctx == NULL)
    return;

  for(GLuint i = 0; i < eEnabled_Count; i++)
  {
    if(!CheckEnableDisableParam(enable_disable_cap[i].cap))
      continue;

    if(Enabled[i])
      GL.glEnable(enable_disable_cap[i].cap);
    else
      GL.glDisable(enable_disable_cap[i].cap);
  }

  GLuint maxTextures = 0;
  GL.glGetIntegerv(eGL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, (GLint *)&maxTextures);

  for(GLuint i = 0; i < RDCMIN(maxTextures, (GLuint)ARRAY_COUNT(Tex2D)); i++)
  {
    GL.glActiveTexture(GLenum(eGL_TEXTURE0 + i));

    if(!IsGLES)
      GL.glBindTexture(eGL_TEXTURE_1D, Tex1D[i].name);

    GL.glBindTexture(eGL_TEXTURE_2D, Tex2D[i].name);
    GL.glBindTexture(eGL_TEXTURE_3D, Tex3D[i].name);

    if(!IsGLES)
      GL.glBindTexture(eGL_TEXTURE_1D_ARRAY, Tex1DArray[i].name);

    GL.glBindTexture(eGL_TEXTURE_2D_ARRAY, Tex2DArray[i].name);

    if(!IsGLES)
      GL.glBindTexture(eGL_TEXTURE_RECTANGLE, TexRect[i].name);

    if(HasExt[ARB_texture_buffer_object])
      GL.glBindTexture(eGL_TEXTURE_BUFFER, TexBuffer[i].name);

    GL.glBindTexture(eGL_TEXTURE_CUBE_MAP, TexCube[i].name);

    if(HasExt[ARB_texture_multisample_no_array] || HasExt[ARB_texture_multisample])
      GL.glBindTexture(eGL_TEXTURE_2D_MULTISAMPLE, Tex2DMS[i].name);

    if(HasExt[ARB_texture_multisample])
      GL.glBindTexture(eGL_TEXTURE_2D_MULTISAMPLE_ARRAY, Tex2DMSArray[i].name);

    if(HasExt[ARB_sampler_objects])
      GL.glBindSampler(i, Samplers[i].name);

    if(HasExt[ARB_texture_cube_map_array])
      GL.glBindTexture(eGL_TEXTURE_CUBE_MAP_ARRAY, TexCubeArray[i].name);
  }

  if(HasExt[ARB_shader_image_load_store])
  {
    GLuint maxImages = 0;
    GL.glGetIntegerv(eGL_MAX_IMAGE_UNITS, (GLint *)&maxImages);

    for(GLuint i = 0; i < RDCMIN(maxImages, (GLuint)ARRAY_COUNT(Images)); i++)
    {
      // use sanitised parameters when no image is bound
      if(Images[i].res.name == 0)
        GL.glBindImageTexture(i, 0, 0, GL_FALSE, 0, eGL_READ_ONLY, eGL_RGBA8);
      else
        GL.glBindImageTexture(i, Images[i].res.name, (GLint)Images[i].level, Images[i].layered,
                              (GLint)Images[i].layer, Images[i].access, Images[i].format);
    }
  }

  GL.glActiveTexture(ActiveTexture);

  if(VAO.name)
    GL.glBindVertexArray(VAO.name);
  else
    GL.glBindVertexArray(driver->GetFakeVAO0());

  if(HasExt[ARB_transform_feedback2])
    GL.glBindTransformFeedback(eGL_TRANSFORM_FEEDBACK, FeedbackObj.name);

  // See FetchState(). The spec says that you have to SET the right format for the shader too,
  // but we couldn't query for the format so we can't set it here.
  GLuint maxNumAttribs = 0;
  GL.glGetIntegerv(eGL_MAX_VERTEX_ATTRIBS, (GLint *)&maxNumAttribs);
  for(GLuint i = 0; i < RDCMIN(maxNumAttribs, (GLuint)ARRAY_COUNT(GenericVertexAttribs)); i++)
    GL.glVertexAttrib4fv(i, &GenericVertexAttribs[i].x);

  GL.glLineWidth(LineWidth);
  if(!IsGLES)
  {
    GL.glPointParameterf(eGL_POINT_FADE_THRESHOLD_SIZE, PointFadeThresholdSize);
    GL.glPointParameteri(eGL_POINT_SPRITE_COORD_ORIGIN, (GLint)PointSpriteOrigin);
    GL.glPointSize(PointSize);
  }

  if(!IsGLES)
    GL.glPrimitiveRestartIndex(PrimitiveRestartIndex);
  if(GL.glClipControl && HasExt[ARB_clip_control])
    GL.glClipControl(ClipOrigin, ClipDepth);
  if(!IsGLES)
    GL.glProvokingVertex(ProvokingVertex);

  GL.glUseProgram(Program.name);
  if(HasExt[ARB_separate_shader_objects])
    GL.glBindProgramPipeline(Pipeline.name);

  GLenum shs[] = {eGL_VERTEX_SHADER,   eGL_TESS_CONTROL_SHADER, eGL_TESS_EVALUATION_SHADER,
                  eGL_GEOMETRY_SHADER, eGL_FRAGMENT_SHADER,     eGL_COMPUTE_SHADER};

  RDCCOMPILE_ASSERT(ARRAY_COUNT(shs) == ARRAY_COUNT(Subroutines),
                    "Subroutine array not the right size");
  for(size_t s = 0; s < ARRAY_COUNT(shs); s++)
  {
    if(shs[s] == eGL_COMPUTE_SHADER && !HasExt[ARB_compute_shader])
      continue;

    if((shs[s] == eGL_TESS_CONTROL_SHADER || shs[s] == eGL_TESS_EVALUATION_SHADER) &&
       !HasExt[ARB_tessellation_shader])
      continue;

    if(Subroutines[s].numSubroutines > 0)
      GL.glUniformSubroutinesuiv(shs[s], Subroutines[s].numSubroutines, Subroutines[s].Values);
  }

  GL.glBindBuffer(eGL_ARRAY_BUFFER, BufferBindings[eBufIdx_Array].name);
  GL.glBindBuffer(eGL_COPY_READ_BUFFER, BufferBindings[eBufIdx_Copy_Read].name);
  GL.glBindBuffer(eGL_COPY_WRITE_BUFFER, BufferBindings[eBufIdx_Copy_Write].name);
  GL.glBindBuffer(eGL_PIXEL_PACK_BUFFER, BufferBindings[eBufIdx_Pixel_Pack].name);
  GL.glBindBuffer(eGL_PIXEL_UNPACK_BUFFER, BufferBindings[eBufIdx_Pixel_Unpack].name);
  if(HasExt[ARB_texture_buffer_object])
    GL.glBindBuffer(eGL_TEXTURE_BUFFER, BufferBindings[eBufIdx_Texture].name);
  if(HasExt[ARB_draw_indirect])
    GL.glBindBuffer(eGL_DRAW_INDIRECT_BUFFER, BufferBindings[eBufIdx_Draw_Indirect].name);
  if(HasExt[ARB_compute_shader])
    GL.glBindBuffer(eGL_DISPATCH_INDIRECT_BUFFER, BufferBindings[eBufIdx_Dispatch_Indirect].name);
  if(HasExt[ARB_query_buffer_object])
    GL.glBindBuffer(eGL_QUERY_BUFFER, BufferBindings[eBufIdx_Query].name);
  if(HasExt[ARB_indirect_parameters])
    GL.glBindBuffer(eGL_PARAMETER_BUFFER_ARB, BufferBindings[eBufIdx_Parameter].name);

  struct
  {
    IdxRangeBuffer *bufs;
    int count;
    GLenum binding;
    GLenum maxcount;
  } idxBufs[] = {
      {
          AtomicCounter, ARRAY_COUNT(AtomicCounter), eGL_ATOMIC_COUNTER_BUFFER,
          eGL_MAX_ATOMIC_COUNTER_BUFFER_BINDINGS,
      },
      {
          ShaderStorage, ARRAY_COUNT(ShaderStorage), eGL_SHADER_STORAGE_BUFFER,
          eGL_MAX_SHADER_STORAGE_BUFFER_BINDINGS,
      },
      {
          TransformFeedback, ARRAY_COUNT(TransformFeedback), eGL_TRANSFORM_FEEDBACK_BUFFER,
          eGL_MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS,
      },
      {
          UniformBinding, ARRAY_COUNT(UniformBinding), eGL_UNIFORM_BUFFER,
          eGL_MAX_UNIFORM_BUFFER_BINDINGS,
      },
  };

  for(size_t b = 0; b < ARRAY_COUNT(idxBufs); b++)
  {
    // only restore buffer bindings here if we were using the default transform feedback object
    if(idxBufs[b].binding == eGL_TRANSFORM_FEEDBACK_BUFFER && FeedbackObj.name)
      continue;

    if(idxBufs[b].binding == eGL_ATOMIC_COUNTER_BUFFER && !HasExt[ARB_shader_atomic_counters])
      continue;

    if(idxBufs[b].binding == eGL_SHADER_STORAGE_BUFFER && !HasExt[ARB_shader_storage_buffer_object])
      continue;

    if(idxBufs[b].binding == eGL_TRANSFORM_FEEDBACK_BUFFER && !HasExt[ARB_transform_feedback2])
      continue;

    GLint maxCount = 0;
    GL.glGetIntegerv(idxBufs[b].maxcount, &maxCount);
    for(int i = 0; i < idxBufs[b].count && i < maxCount; i++)
    {
      if(idxBufs[b].bufs[i].res.name == 0 ||
         (idxBufs[b].bufs[i].start == 0 && idxBufs[b].bufs[i].size == 0))
        GL.glBindBufferBase(idxBufs[b].binding, i, idxBufs[b].bufs[i].res.name);
      else
        GL.glBindBufferRange(idxBufs[b].binding, i, idxBufs[b].bufs[i].res.name,
                             (GLintptr)idxBufs[b].bufs[i].start, (GLsizeiptr)idxBufs[b].bufs[i].size);
    }
  }

  GLuint maxDraws = 0;
  GL.glGetIntegerv(eGL_MAX_DRAW_BUFFERS, (GLint *)&maxDraws);

  if(HasExt[ARB_draw_buffers_blend])
  {
    for(GLuint i = 0; i < RDCMIN(maxDraws, (GLuint)ARRAY_COUNT(Blends)); i++)
    {
      // not set, possibly there were lesser draw buffers during capture
      if(Blends[i].EquationRGB == eGL_NONE)
        continue;

      GL.glBlendFuncSeparatei(i, Blends[i].SourceRGB, Blends[i].DestinationRGB,
                              Blends[i].SourceAlpha, Blends[i].DestinationAlpha);
      GL.glBlendEquationSeparatei(i, Blends[i].EquationRGB, Blends[i].EquationAlpha);

      if(Blends[i].Enabled)
        GL.glEnablei(eGL_BLEND, i);
      else
        GL.glDisablei(eGL_BLEND, i);
    }
  }
  else
  {
    // not set, possibly there were lesser draw buffers during capture
    if(Blends[0].EquationRGB != eGL_NONE)
    {
      GL.glBlendFuncSeparate(Blends[0].SourceRGB, Blends[0].DestinationRGB, Blends[0].SourceAlpha,
                             Blends[0].DestinationAlpha);
      GL.glBlendEquationSeparate(Blends[0].EquationRGB, Blends[0].EquationAlpha);

      if(Blends[0].Enabled)
        GL.glEnable(eGL_BLEND);
      else
        GL.glDisable(eGL_BLEND);
    }
  }

  GL.glBlendColor(BlendColor[0], BlendColor[1], BlendColor[2], BlendColor[3]);

  if(HasExt[ARB_viewport_array])
  {
    GLuint maxViews = 0;
    GL.glGetIntegerv(eGL_MAX_VIEWPORTS, (GLint *)&maxViews);

    GL.glViewportArrayv(0, RDCMIN(maxViews, (GLuint)ARRAY_COUNT(Viewports)), &Viewports[0].x);

    for(GLuint s = 0; s < RDCMIN(maxViews, (GLuint)ARRAY_COUNT(Scissors)); ++s)
    {
      GL.glScissorIndexedv(s, &Scissors[s].x);

      if(Scissors[s].enabled)
        GL.glEnablei(eGL_SCISSOR_TEST, s);
      else
        GL.glDisablei(eGL_SCISSOR_TEST, s);
    }

    for(GLuint i = 0; i < RDCMIN(maxViews, (GLuint)ARRAY_COUNT(DepthRanges)); i++)
    {
      if(IsGLES)
      {
        float v[2] = {(float)DepthRanges[i].nearZ, (float)DepthRanges[i].farZ};
        GL.glDepthRangeArrayfvOES(i, 1, v);
      }
      else
      {
        GL.glDepthRangeArrayv(i, 1, &DepthRanges[i].nearZ);
      }
    }
  }
  else
  {
    GL.glViewport((GLint)Viewports[0].x, (GLint)Viewports[0].y, (GLsizei)Viewports[0].width,
                  (GLsizei)Viewports[0].height);

    GL.glScissor(Scissors[0].x, Scissors[0].y, Scissors[0].width, Scissors[0].height);

    if(Scissors[0].enabled)
      GL.glEnable(eGL_SCISSOR_TEST);
    else
      GL.glDisable(eGL_SCISSOR_TEST);

    if(IsGLES)
      GL.glDepthRangef((float)DepthRanges[0].nearZ, (float)DepthRanges[0].farZ);
    else
      GL.glDepthRange(DepthRanges[0].nearZ, DepthRanges[0].farZ);
  }

  GLenum DBs[8] = {eGL_NONE};
  uint32_t numDBs = 0;
  for(GLuint i = 0; i < RDCMIN(maxDraws, (GLuint)ARRAY_COUNT(DrawBuffers)); i++)
  {
    if(DrawBuffers[i] != eGL_NONE)
    {
      numDBs++;
      DBs[i] = DrawBuffers[i];

      if(IsReplayMode(driver->GetState()))
      {
        // since we are faking the default framebuffer with our own
        // to see the results, replace back/front/left/right with color attachment 0
        if(DBs[i] == eGL_BACK_LEFT || DBs[i] == eGL_BACK_RIGHT || DBs[i] == eGL_FRONT_LEFT ||
           DBs[i] == eGL_FRONT_RIGHT)
          DBs[i] = eGL_COLOR_ATTACHMENT0;

        // These aren't valid for glDrawBuffers but can be returned when we call glGet,
        // assume they mean left implicitly
        if(DBs[i] == eGL_BACK || DBs[i] == eGL_FRONT || DBs[i] == eGL_FRONT_AND_BACK)
          DBs[i] = eGL_COLOR_ATTACHMENT0;
      }
    }
    else
    {
      break;
    }
  }

  // this will always return true during capture, but on replay we only do
  // this work if we're on the replay context
  if(driver->GetReplay()->IsReplayContext(ctx.ctx))
  {
    GLuint fbo = driver->GetCurrentDefaultFBO();

    if(fbo)
    {
      // apply drawbuffers/readbuffer to default framebuffer
      GL.glBindFramebuffer(eGL_READ_FRAMEBUFFER, driver->GetCurrentDefaultFBO());
      GL.glBindFramebuffer(eGL_DRAW_FRAMEBUFFER, driver->GetCurrentDefaultFBO());
      GL.glDrawBuffers(numDBs, DBs);

      // see above for reasoning for this
      GL.glReadBuffer(eGL_COLOR_ATTACHMENT0);
    }

    if(ReadFBO.name)
      GL.glBindFramebuffer(eGL_READ_FRAMEBUFFER, ReadFBO.name);
    else if(fbo)
      GL.glBindFramebuffer(eGL_READ_FRAMEBUFFER, fbo);

    if(DrawFBO.name)
      GL.glBindFramebuffer(eGL_DRAW_FRAMEBUFFER, DrawFBO.name);
    else if(fbo)
      GL.glBindFramebuffer(eGL_DRAW_FRAMEBUFFER, fbo);
  }

  GL.glHint(eGL_FRAGMENT_SHADER_DERIVATIVE_HINT, Hints.Derivatives);
  if(!IsGLES)
  {
    GL.glHint(eGL_LINE_SMOOTH_HINT, Hints.LineSmooth);
    GL.glHint(eGL_POLYGON_SMOOTH_HINT, Hints.PolySmooth);
    GL.glHint(eGL_TEXTURE_COMPRESSION_HINT, Hints.TexCompression);
  }

  GL.glDepthMask(DepthWriteMask);
  GL.glClearDepthf(DepthClearValue);
  GL.glDepthFunc(DepthFunc);

  if(HasExt[EXT_depth_bounds_test] && GL.glDepthBoundsEXT)
    GL.glDepthBoundsEXT(DepthBounds.nearZ, DepthBounds.farZ);

  {
    GL.glStencilFuncSeparate(eGL_FRONT, StencilFront.func, StencilFront.ref, StencilFront.valuemask);
    GL.glStencilFuncSeparate(eGL_BACK, StencilBack.func, StencilBack.ref, StencilBack.valuemask);

    GL.glStencilMaskSeparate(eGL_FRONT, StencilFront.writemask);
    GL.glStencilMaskSeparate(eGL_BACK, StencilBack.writemask);

    GL.glStencilOpSeparate(eGL_FRONT, StencilFront.stencilFail, StencilFront.depthFail,
                           StencilFront.pass);
    GL.glStencilOpSeparate(eGL_BACK, StencilBack.stencilFail, StencilBack.depthFail,
                           StencilBack.pass);
  }

  GL.glClearStencil((GLint)StencilClearValue);

  if(HasExt[EXT_draw_buffers2] || HasExt[ARB_draw_buffers_blend])
  {
    for(GLuint i = 0; i < RDCMIN(maxDraws, (GLuint)ARRAY_COUNT(ColorMasks)); i++)
      GL.glColorMaski(i, ColorMasks[i].red, ColorMasks[i].green, ColorMasks[i].blue,
                      ColorMasks[i].alpha);
  }
  else
  {
    GL.glColorMask(ColorMasks[0].red, ColorMasks[0].green, ColorMasks[0].blue, ColorMasks[0].alpha);
  }

  if(HasExt[ARB_texture_multisample_no_array] || HasExt[ARB_texture_multisample])
    GL.glSampleMaski(0, (GLbitfield)SampleMask[0]);

  GL.glSampleCoverage(SampleCoverage, SampleCoverageInvert ? GL_TRUE : GL_FALSE);

  if(HasExt[ARB_sample_shading])
    GL.glMinSampleShading(MinSampleShading);

  if(HasExt[EXT_raster_multisample] && GL.glRasterSamplesEXT)
    GL.glRasterSamplesEXT(RasterSamples, RasterFixed);

  if(!IsGLES)
    GL.glLogicOp(LogicOp);

  GL.glClearColor(ColorClearValue.red, ColorClearValue.green, ColorClearValue.blue,
                  ColorClearValue.alpha);

  if(HasExt[ARB_tessellation_shader])
  {
    GL.glPatchParameteri(eGL_PATCH_VERTICES, PatchParams.numVerts);
    if(!IsGLES)
    {
      GL.glPatchParameterfv(eGL_PATCH_DEFAULT_INNER_LEVEL, PatchParams.defaultInnerLevel);
      GL.glPatchParameterfv(eGL_PATCH_DEFAULT_OUTER_LEVEL, PatchParams.defaultOuterLevel);
    }
  }

  if(!IsGLES)
    GL.glPolygonMode(eGL_FRONT_AND_BACK, PolygonMode);

  if(HasExt[ARB_polygon_offset_clamp])
    GL.glPolygonOffsetClamp(PolygonOffset[0], PolygonOffset[1], PolygonOffset[2]);
  else
    GL.glPolygonOffset(PolygonOffset[0], PolygonOffset[1]);

  GL.glFrontFace(FrontFace);
  GL.glCullFace(CullFace);

  if(IsGLES && (HasExt[EXT_primitive_bounding_box] || HasExt[OES_primitive_bounding_box]))
    GL.glPrimitiveBoundingBox(PrimitiveBoundingBox.minX, PrimitiveBoundingBox.minY,
                              PrimitiveBoundingBox.minZ, PrimitiveBoundingBox.minW,
                              PrimitiveBoundingBox.maxX, PrimitiveBoundingBox.maxY,
                              PrimitiveBoundingBox.maxZ, PrimitiveBoundingBox.maxW);

  Unpack.Apply(true);

  ClearGLErrors();
}

void GLRenderState::Clear()
{
  ContextPresent = true;

  RDCEraseEl(Enabled);

  RDCEraseEl(Tex1D);
  RDCEraseEl(Tex2D);
  RDCEraseEl(Tex3D);
  RDCEraseEl(Tex1DArray);
  RDCEraseEl(Tex2DArray);
  RDCEraseEl(TexCubeArray);
  RDCEraseEl(TexRect);
  RDCEraseEl(TexBuffer);
  RDCEraseEl(TexCube);
  RDCEraseEl(Tex2DMS);
  RDCEraseEl(Tex2DMSArray);
  RDCEraseEl(Samplers);
  RDCEraseEl(ActiveTexture);

  RDCEraseEl(Images);
  for(GLuint i = 0; i < (GLuint)ARRAY_COUNT(Images); i++)
  {
    Images[i].access = eGL_READ_ONLY;
    Images[i].format = eGL_RGBA8;
  }

  RDCEraseEl(Program);
  RDCEraseEl(Pipeline);

  RDCEraseEl(Subroutines);

  RDCEraseEl(VAO);
  RDCEraseEl(FeedbackObj);

  RDCEraseEl(GenericVertexAttribs);

  RDCEraseEl(PointFadeThresholdSize);
  RDCEraseEl(PointSpriteOrigin);
  RDCEraseEl(LineWidth);
  RDCEraseEl(PointSize);

  RDCEraseEl(PrimitiveRestartIndex);
  RDCEraseEl(PrimitiveBoundingBox);
  RDCEraseEl(ClipOrigin);
  RDCEraseEl(ClipDepth);
  RDCEraseEl(ProvokingVertex);

  RDCEraseEl(BufferBindings);
  RDCEraseEl(AtomicCounter);
  RDCEraseEl(ShaderStorage);
  RDCEraseEl(TransformFeedback);
  RDCEraseEl(UniformBinding);
  RDCEraseEl(Blends);
  RDCEraseEl(BlendColor);
  RDCEraseEl(Viewports);
  RDCEraseEl(Scissors);

  RDCEraseEl(DrawFBO);
  RDCEraseEl(ReadFBO);
  RDCEraseEl(DrawBuffers);
  RDCEraseEl(ReadBuffer);

  RDCEraseEl(PatchParams);
  RDCEraseEl(PolygonMode);
  RDCEraseEl(PolygonOffset);

  RDCEraseEl(DepthWriteMask);
  RDCEraseEl(DepthClearValue);
  RDCEraseEl(DepthRanges);
  RDCEraseEl(DepthBounds);
  RDCEraseEl(DepthFunc);
  RDCEraseEl(StencilFront);
  RDCEraseEl(StencilBack);
  RDCEraseEl(StencilClearValue);
  RDCEraseEl(ColorMasks);
  RDCEraseEl(SampleMask);
  RDCEraseEl(RasterSamples);
  RDCEraseEl(RasterFixed);
  RDCEraseEl(SampleCoverage);
  RDCEraseEl(SampleCoverageInvert);
  RDCEraseEl(MinSampleShading);
  RDCEraseEl(LogicOp);
  RDCEraseEl(ColorClearValue);

  RDCEraseEl(Hints);
  RDCEraseEl(FrontFace);
  RDCEraseEl(CullFace);

  RDCEraseEl(Unpack);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, GLRenderState::Image &el)
{
  SERIALISE_MEMBER(res);
  SERIALISE_MEMBER(level);
  SERIALISE_MEMBER(layered);
  SERIALISE_MEMBER(layer);
  SERIALISE_MEMBER(access);
  SERIALISE_MEMBER(format);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, GLRenderState::Subroutine &el)
{
  SERIALISE_MEMBER(numSubroutines);
  SERIALISE_MEMBER(Values);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, GLRenderState::IdxRangeBuffer &el)
{
  SERIALISE_MEMBER(res);
  SERIALISE_MEMBER(start);
  SERIALISE_MEMBER(size);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, GLRenderState::BlendState &el)
{
  SERIALISE_MEMBER(EquationRGB);
  SERIALISE_MEMBER(EquationAlpha);
  SERIALISE_MEMBER(SourceRGB);
  SERIALISE_MEMBER(SourceAlpha);
  SERIALISE_MEMBER(DestinationRGB);
  SERIALISE_MEMBER(DestinationAlpha);
  SERIALISE_MEMBER(Enabled);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, GLRenderState::Viewport &el)
{
  SERIALISE_MEMBER(x);
  SERIALISE_MEMBER(y);
  SERIALISE_MEMBER(width);
  SERIALISE_MEMBER(height);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, GLRenderState::Scissor &el)
{
  SERIALISE_MEMBER(x);
  SERIALISE_MEMBER(y);
  SERIALISE_MEMBER(width);
  SERIALISE_MEMBER(height);
  SERIALISE_MEMBER(enabled);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, GLRenderState::DepthRange &el)
{
  SERIALISE_MEMBER(nearZ);
  SERIALISE_MEMBER(farZ);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, GLRenderState::DepthBound &el)
{
  SERIALISE_MEMBER(nearZ);
  SERIALISE_MEMBER(farZ);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, GLRenderState::ColorMask &el)
{
  SERIALISE_MEMBER(red);
  SERIALISE_MEMBER(green);
  SERIALISE_MEMBER(blue);
  SERIALISE_MEMBER(alpha);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, GLRenderState::ClearValue &el)
{
  SERIALISE_MEMBER(red);
  SERIALISE_MEMBER(green);
  SERIALISE_MEMBER(blue);
  SERIALISE_MEMBER(alpha);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, GLRenderState &el)
{
  SERIALISE_MEMBER(ContextPresent);

  if(!el.ContextPresent)
    return;

  RDCCOMPILE_ASSERT(GLRenderState::eEnabled_Count == 34, "Update cosmetic enabled serialisation");

  for(int i = 0; i < GLRenderState::eEnabled_Count; i++)
    ser.Serialise(enable_disable_cap[i].name, el.Enabled[i]);

  ser.Serialise("GL_TEXTURE_BINDING_1D"_lit, el.Tex1D);
  ser.Serialise("GL_TEXTURE_BINDING_2D"_lit, el.Tex2D);
  ser.Serialise("GL_TEXTURE_BINDING_3D"_lit, el.Tex3D);
  ser.Serialise("GL_TEXTURE_BINDING_1D_ARRAY"_lit, el.Tex1DArray);
  ser.Serialise("GL_TEXTURE_BINDING_2D_ARRAY"_lit, el.Tex2DArray);
  ser.Serialise("GL_TEXTURE_BINDING_CUBE_MAP_ARRAY"_lit, el.TexCubeArray);
  ser.Serialise("GL_TEXTURE_BINDING_RECTANGLE"_lit, el.TexRect);
  ser.Serialise("GL_TEXTURE_BINDING_BUFFER"_lit, el.TexBuffer);
  ser.Serialise("GL_TEXTURE_BINDING_CUBE_MAP"_lit, el.TexCube);
  ser.Serialise("GL_TEXTURE_BINDING_2D_MULTISAMPLE"_lit, el.Tex2DMS);
  ser.Serialise("GL_TEXTURE_BINDING_2D_MULTISAMPLE_ARRAY"_lit, el.Tex2DMSArray);

  ser.Serialise("GL_SAMPLER_BINDING"_lit, el.Samplers);

  ser.Serialise("GL_ACTIVE_TEXTURE"_lit, el.ActiveTexture);

  ser.Serialise("GL_IMAGE_BINDING"_lit, el.Images);

  ser.Serialise("GL_CURRENT_PROGRAM"_lit, el.Program);
  ser.Serialise("GL_PROGRAM_PIPELINE_BINDING"_lit, el.Pipeline);

  ser.Serialise("GL_SUBROUTINES"_lit, el.Subroutines);

  ser.Serialise("GL_VERTEX_ARRAY_BINDING"_lit, el.VAO);

  ser.Serialise("GL_TRANSFORM_FEEDBACK_BINDING"_lit, el.FeedbackObj);

  ser.Serialise("GL_CURRENT_VERTEX_ATTRIB"_lit, el.GenericVertexAttribs);

  ser.Serialise("GL_POINT_FADE_THRESHOLD_SIZE"_lit, el.PointFadeThresholdSize);
  ser.Serialise("GL_POINT_SPRITE_COORD_ORIGIN"_lit, el.PointSpriteOrigin);
  ser.Serialise("GL_LINE_WIDTH"_lit, el.LineWidth);
  ser.Serialise("GL_POINT_SIZE"_lit, el.PointSize);

  ser.Serialise("GL_PRIMITIVE_RESTART_INDEX"_lit, el.PrimitiveRestartIndex);

  {
    ser.Serialise("GL_PRIMITIVE_BOUNDING_BOX_MINX"_lit, el.PrimitiveBoundingBox.minX);
    ser.Serialise("GL_PRIMITIVE_BOUNDING_BOX_MINY"_lit, el.PrimitiveBoundingBox.minY);
    ser.Serialise("GL_PRIMITIVE_BOUNDING_BOX_MINZ"_lit, el.PrimitiveBoundingBox.minZ);
    ser.Serialise("GL_PRIMITIVE_BOUNDING_BOX_MINW"_lit, el.PrimitiveBoundingBox.minW);
    ser.Serialise("GL_PRIMITIVE_BOUNDING_BOX_MAXX"_lit, el.PrimitiveBoundingBox.maxX);
    ser.Serialise("GL_PRIMITIVE_BOUNDING_BOX_MAXY"_lit, el.PrimitiveBoundingBox.maxY);
    ser.Serialise("GL_PRIMITIVE_BOUNDING_BOX_MAXZ"_lit, el.PrimitiveBoundingBox.maxZ);
    ser.Serialise("GL_PRIMITIVE_BOUNDING_BOX_MAXW"_lit, el.PrimitiveBoundingBox.maxW);
  }

  ser.Serialise("GL_CLIP_ORIGIN"_lit, el.ClipOrigin);
  ser.Serialise("GL_CLIP_DEPTH_MODE"_lit, el.ClipDepth);
  ser.Serialise("GL_PROVOKING_VERTEX"_lit, el.ProvokingVertex);

  ser.Serialise("GL_ARRAY_BUFFER_BINDING"_lit, el.BufferBindings[GLRenderState::eBufIdx_Array]);
  ser.Serialise("GL_COPY_READ_BUFFER_BINDING"_lit,
                el.BufferBindings[GLRenderState::eBufIdx_Copy_Read]);
  ser.Serialise("GL_COPY_WRITE_BUFFER_BINDING"_lit,
                el.BufferBindings[GLRenderState::eBufIdx_Copy_Write]);
  ser.Serialise("GL_PIXEL_PACK_BUFFER_BINDING"_lit,
                el.BufferBindings[GLRenderState::eBufIdx_Pixel_Pack]);
  ser.Serialise("GL_PIXEL_UNPACK_BUFFER_BINDING"_lit,
                el.BufferBindings[GLRenderState::eBufIdx_Pixel_Unpack]);
  ser.Serialise("GL_TEXTURE_BUFFER_BINDING"_lit, el.BufferBindings[GLRenderState::eBufIdx_Texture]);
  ser.Serialise("GL_DRAW_INDIRECT_BUFFER_BINDING"_lit,
                el.BufferBindings[GLRenderState::eBufIdx_Draw_Indirect]);
  ser.Serialise("GL_DISPATCH_INDIRECT_BUFFER_BINDING"_lit,
                el.BufferBindings[GLRenderState::eBufIdx_Dispatch_Indirect]);
  ser.Serialise("GL_QUERY_BUFFER_BINDING"_lit, el.BufferBindings[GLRenderState::eBufIdx_Query]);
  ser.Serialise("GL_PARAMETER_BUFFER_ARB_BINDING"_lit,
                el.BufferBindings[GLRenderState::eBufIdx_Parameter]);

  ser.Serialise("GL_ATOMIC_COUNTER_BUFFER_BINDING"_lit, el.AtomicCounter);
  ser.Serialise("GL_SHADER_STORAGE_BUFFER_BINDING"_lit, el.ShaderStorage);
  ser.Serialise("GL_TRANSFORM_FEEDBACK_BUFFER_BINDING"_lit, el.TransformFeedback);
  ser.Serialise("GL_UNIFORM_BUFFER_BINDING"_lit, el.UniformBinding);

  ser.Serialise("GL_BLENDS"_lit, el.Blends);
  ser.Serialise("GL_BLEND_COLOR"_lit, el.BlendColor);

  ser.Serialise("GL_VIEWPORT"_lit, el.Viewports);
  ser.Serialise("GL_SCISSOR"_lit, el.Scissors);

  ser.Serialise("GL_DEPTH_RANGE"_lit, el.DepthRanges);

  ser.Serialise("GL_READ_FRAMEBUFFER_BINDING"_lit, el.ReadFBO);
  ser.Serialise("GL_DRAW_FRAMEBUFFER_BINDING"_lit, el.DrawFBO);

  ser.Serialise("GL_READ_BUFFER"_lit, el.ReadBuffer);
  ser.Serialise("GL_DRAW_BUFFERS"_lit, el.DrawBuffers);

  {
    ser.Serialise("GL_PATCH_VERTICES"_lit, el.PatchParams.numVerts);
    ser.Serialise("GL_PATCH_DEFAULT_INNER_LEVEL"_lit, el.PatchParams.defaultInnerLevel);
    ser.Serialise("GL_PATCH_DEFAULT_OUTER_LEVEL"_lit, el.PatchParams.defaultOuterLevel);
  }

  ser.Serialise("GL_POLYGON_MODE"_lit, el.PolygonMode);
  ser.Serialise("GL_POLYGON_OFFSET_FACTOR"_lit, el.PolygonOffset[0]);
  ser.Serialise("GL_POLYGON_OFFSET_UNITS"_lit, el.PolygonOffset[1]);
  ser.Serialise("GL_POLYGON_OFFSET_CLAMP_EXT"_lit, el.PolygonOffset[2]);

  ser.Serialise("GL_DEPTH_WRITEMASK"_lit, el.DepthWriteMask);
  ser.Serialise("GL_DEPTH_CLEAR_VALUE"_lit, el.DepthClearValue);
  ser.Serialise("GL_DEPTH_FUNC"_lit, el.DepthFunc);

  ser.Serialise("GL_DEPTH_BOUNDS_EXT"_lit, el.DepthBounds);

  {
    ser.Serialise("GL_STENCIL_FUNC"_lit, el.StencilFront.func);
    ser.Serialise("GL_STENCIL_BACK_FUNC"_lit, el.StencilBack.func);

    ser.Serialise("GL_STENCIL_REF"_lit, el.StencilFront.ref);
    ser.Serialise("GL_STENCIL_BACK_REF"_lit, el.StencilBack.ref);

    ser.Serialise("GL_STENCIL_VALUE_MASK"_lit, el.StencilFront.valuemask);
    ser.Serialise("GL_STENCIL_BACK_VALUE_MASK"_lit, el.StencilBack.valuemask);

    ser.Serialise("GL_STENCIL_WRITEMASK"_lit, el.StencilFront.writemask);
    ser.Serialise("GL_STENCIL_BACK_WRITEMASK"_lit, el.StencilBack.writemask);

    ser.Serialise("GL_STENCIL_FAIL"_lit, el.StencilFront.stencilFail);
    ser.Serialise("GL_STENCIL_BACK_FAIL"_lit, el.StencilBack.stencilFail);

    ser.Serialise("GL_STENCIL_PASS_DEPTH_FAIL"_lit, el.StencilFront.depthFail);
    ser.Serialise("GL_STENCIL_BACK_PASS_DEPTH_FAIL"_lit, el.StencilBack.depthFail);

    ser.Serialise("GL_STENCIL_PASS_DEPTH_PASS"_lit, el.StencilFront.pass);
    ser.Serialise("GL_STENCIL_BACK_PASS_DEPTH_PASS"_lit, el.StencilBack.pass);
  }

  ser.Serialise("GL_STENCIL_CLEAR_VALUE"_lit, el.StencilClearValue);

  ser.Serialise("GL_COLOR_WRITEMASK"_lit, el.ColorMasks);

  ser.Serialise("GL_RASTER_SAMPLES_EXT"_lit, el.RasterSamples);
  ser.Serialise("GL_RASTER_FIXED_SAMPLE_LOCATIONS_EXT"_lit, el.RasterFixed);

  ser.Serialise("GL_SAMPLE_MASK_VALUE"_lit, el.SampleMask);
  ser.Serialise("GL_SAMPLE_COVERAGE_VALUE"_lit, el.SampleCoverage);
  ser.Serialise("GL_SAMPLE_COVERAGE_INVERT"_lit, el.SampleCoverageInvert);
  ser.Serialise("GL_MIN_SAMPLE_SHADING"_lit, el.MinSampleShading);

  ser.Serialise("GL_LOGIC_OP_MODE"_lit, el.LogicOp);

  ser.Serialise("GL_COLOR_CLEAR_VALUE"_lit, el.ColorClearValue);

  ser.Serialise("GL_FRAGMENT_SHADER_DERIVATIVE_HINT"_lit, el.Hints.Derivatives);
  ser.Serialise("GL_LINE_SMOOTH_HINT"_lit, el.Hints.LineSmooth);
  ser.Serialise("GL_POLYGON_SMOOTH_HINT"_lit, el.Hints.PolySmooth);
  ser.Serialise("GL_TEXTURE_COMPRESSION_HINT"_lit, el.Hints.TexCompression);

  ser.Serialise("GL_FRONT_FACE"_lit, el.FrontFace);
  ser.Serialise("GL_CULL_FACE_MODE"_lit, el.CullFace);

  ser.Serialise("GL_UNPACK_SWAP_BYTES"_lit, el.Unpack.swapBytes);
  // TODO serialise GL_UNPACK_LSB_FIRST?
  ser.Serialise("GL_UNPACK_ROW_LENGTH"_lit, el.Unpack.rowlength);
  ser.Serialise("GL_UNPACK_IMAGE_HEIGHT"_lit, el.Unpack.imageheight);
  ser.Serialise("GL_UNPACK_SKIP_PIXELS"_lit, el.Unpack.skipPixels);
  ser.Serialise("GL_UNPACK_SKIP_ROWS"_lit, el.Unpack.skipRows);
  ser.Serialise("GL_UNPACK_SKIP_IMAGES"_lit, el.Unpack.skipImages);
  ser.Serialise("GL_UNPACK_ALIGNMENT"_lit, el.Unpack.alignment);
  ser.Serialise("GL_UNPACK_COMPRESSED_BLOCK_WIDTH"_lit, el.Unpack.compressedBlockWidth);
  ser.Serialise("GL_UNPACK_COMPRESSED_BLOCK_HEIGHT"_lit, el.Unpack.compressedBlockHeight);
  ser.Serialise("GL_UNPACK_COMPRESSED_BLOCK_DEPTH"_lit, el.Unpack.compressedBlockDepth);
  ser.Serialise("GL_UNPACK_COMPRESSED_BLOCK_SIZE"_lit, el.Unpack.compressedBlockSize);
}

INSTANTIATE_SERIALISE_TYPE(GLRenderState);
