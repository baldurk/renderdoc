/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2017-2019 Baldur Karlsson
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

#include <algorithm>
#include "gl_driver.h"
#include "gl_manager.h"

// declare emulated glCopyImageSubData in case we need to force its use when the driver's version is
// buggy
namespace glEmulate
{
void APIENTRY _glCopyImageSubData(GLuint srcName, GLenum srcTarget, GLint srcLevel, GLint srcX,
                                  GLint srcY, GLint srcZ, GLuint dstName, GLenum dstTarget,
                                  GLint dstLevel, GLint dstX, GLint dstY, GLint dstZ,
                                  GLsizei srcWidth, GLsizei srcHeight, GLsizei srcDepth);
};

const GLenum FramebufferInitialData::attachmentNames[10] = {
    eGL_COLOR_ATTACHMENT0, eGL_COLOR_ATTACHMENT1,  eGL_COLOR_ATTACHMENT2, eGL_COLOR_ATTACHMENT3,
    eGL_COLOR_ATTACHMENT4, eGL_COLOR_ATTACHMENT5,  eGL_COLOR_ATTACHMENT6, eGL_COLOR_ATTACHMENT7,
    eGL_DEPTH_ATTACHMENT,  eGL_STENCIL_ATTACHMENT,
};

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VertexAttribInitialData &el)
{
  SERIALISE_MEMBER(enabled);
  SERIALISE_MEMBER(vbslot);
  SERIALISE_MEMBER(offset);
  SERIALISE_MEMBER(type);
  SERIALISE_MEMBER(normalized);
  SERIALISE_MEMBER(integer);
  SERIALISE_MEMBER(size);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VertexBufferInitialData &el)
{
  SERIALISE_MEMBER(Buffer);
  SERIALISE_MEMBER(Stride);
  SERIALISE_MEMBER(Offset);
  SERIALISE_MEMBER(Divisor);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VAOInitialData &el)
{
  SERIALISE_MEMBER(valid);
  SERIALISE_MEMBER(VertexAttribs);
  SERIALISE_MEMBER(VertexBuffers);
  SERIALISE_MEMBER(ElementArrayBuffer);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, FeedbackInitialData &el)
{
  SERIALISE_MEMBER(valid);
  SERIALISE_MEMBER(Buffer);
  SERIALISE_MEMBER(Offset);
  SERIALISE_MEMBER(Size);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, FramebufferAttachmentData &el)
{
  SERIALISE_MEMBER(layered);
  SERIALISE_MEMBER(layer);
  SERIALISE_MEMBER(level);
  if(ser.VersionAtLeast(0x1B))
  {
    SERIALISE_MEMBER(numVirtualSamples);
    SERIALISE_MEMBER(numViews);
    SERIALISE_MEMBER(startView);
  }
  else if(ser.IsReading())
  {
    el.numVirtualSamples = 1;
    el.numViews = 1;
    el.startView = 0;
  }
  SERIALISE_MEMBER(obj);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, FramebufferInitialData &el)
{
  SERIALISE_MEMBER(valid);
  SERIALISE_MEMBER(Attachments);
  SERIALISE_MEMBER(DrawBuffers);
  SERIALISE_MEMBER(ReadBuffer);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, PipelineInitialData &el)
{
  SERIALISE_MEMBER(valid);
  SERIALISE_MEMBER(programs);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, SamplerInitialData &el)
{
  SERIALISE_MEMBER(valid);
  SERIALISE_MEMBER(border);
  SERIALISE_MEMBER(compareFunc);
  SERIALISE_MEMBER(compareMode);
  SERIALISE_MEMBER(lodBias);
  SERIALISE_MEMBER(minLod);
  SERIALISE_MEMBER(maxLod);
  SERIALISE_MEMBER(minFilter);
  SERIALISE_MEMBER(magFilter);
  SERIALISE_MEMBER(maxAniso);
  SERIALISE_MEMBER(wrap);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, TextureStateInitialData &el)
{
  SERIALISE_MEMBER(internalformat);
  SERIALISE_MEMBER(isView);
  SERIALISE_MEMBER(width);
  SERIALISE_MEMBER(height);
  SERIALISE_MEMBER(depth);
  SERIALISE_MEMBER(samples);
  SERIALISE_MEMBER(dim);
  SERIALISE_MEMBER(type);
  SERIALISE_MEMBER(mips);

  SERIALISE_MEMBER(baseLevel);
  SERIALISE_MEMBER(maxLevel);
  SERIALISE_MEMBER(minLod);
  SERIALISE_MEMBER(maxLod);
  SERIALISE_MEMBER(srgbDecode);
  SERIALISE_MEMBER(depthMode);
  SERIALISE_MEMBER(compareFunc);
  SERIALISE_MEMBER(compareMode);
  SERIALISE_MEMBER(minFilter);
  SERIALISE_MEMBER(magFilter);
  SERIALISE_MEMBER(seamless);
  SERIALISE_MEMBER(swizzle);
  SERIALISE_MEMBER(wrap);
  SERIALISE_MEMBER(border);
  SERIALISE_MEMBER(lodBias);
  SERIALISE_MEMBER(texBuffer);
  SERIALISE_MEMBER(texBufOffs);
  SERIALISE_MEMBER(texBufSize);
}

void WrappedOpenGL::TextureData::GetCompressedImageDataGLES(int mip, GLenum target, size_t size,
                                                            byte *buf)
{
  const std::vector<byte> &data = compressedData[mip];

  memset(buf, 0, size);

  size_t startOffs = IsCubeFace(target) ? CubeTargetIndex(target) * size : 0;
  if(data.size() >= startOffs)
  {
    size_t byteSize = RDCMIN(data.size() - startOffs, size);
    if(byteSize > 0)
      memcpy(buf, data.data() + startOffs, byteSize);
  }
}

void GLResourceManager::ContextPrepare_InitialState(GLResource res)
{
  GLInitialContents initContents;

  initContents.type = res.Namespace;

  ResourceId id = GetID(res);

  if(res.Namespace == eResBuffer)
  {
    // get the length of the buffer
    uint32_t length = 4;
    GL.glGetNamedBufferParameterivEXT(res.name, eGL_BUFFER_SIZE, (GLint *)&length);

    // save old bindings
    GLuint oldbuf1 = 0, oldbuf2 = 0;
    GL.glGetIntegerv(eGL_COPY_READ_BUFFER_BINDING, (GLint *)&oldbuf1);
    GL.glGetIntegerv(eGL_COPY_WRITE_BUFFER_BINDING, (GLint *)&oldbuf2);

    // create a new buffer big enough to hold the contents
    GLuint buf = 0;
    GL.glGenBuffers(1, &buf);
    GL.glBindBuffer(eGL_COPY_WRITE_BUFFER, buf);
    GL.glNamedBufferDataEXT(buf, (GLsizeiptr)RDCMAX(length, 4U), NULL, eGL_STATIC_READ);

    // bind the live buffer for copying
    GL.glBindBuffer(eGL_COPY_READ_BUFFER, res.name);

    // do the actual copy
    if(length > 0)
      GL.glCopyBufferSubData(eGL_COPY_READ_BUFFER, eGL_COPY_WRITE_BUFFER, 0, 0, (GLsizeiptr)length);

    // workaround for some drivers - mapping/unmapping here seems to help avoid problems mapping
    // later.
    GL.glMapNamedBufferEXT(buf, eGL_READ_ONLY);
    GL.glUnmapNamedBufferEXT(buf);

    // restore old bindings
    GL.glBindBuffer(eGL_COPY_READ_BUFFER, oldbuf1);
    GL.glBindBuffer(eGL_COPY_WRITE_BUFFER, oldbuf2);

    initContents.resource = GLResource(res.ContextShareGroup, eResBuffer, buf);
    initContents.bufferLength = length;
  }
  else if(res.Namespace == eResProgram)
  {
    WriteSerialiser ser(new StreamWriter(4 * 1024), Ownership::Stream);

    ser.SetChunkMetadataRecording(m_Driver->GetSerialiser().GetChunkMetadataRecording());

    SCOPED_SERIALISE_CHUNK(SystemChunk::InitialContents);

    SERIALISE_ELEMENT(id).TypedAs("GLResource"_lit);
    SERIALISE_ELEMENT(res.Namespace);

    PerStageReflections stages;
    m_Driver->FillReflectionArray(id, stages);

    SerialiseProgramBindings(ser, CaptureState::ActiveCapturing, stages, res.name);
    SerialiseProgramUniforms(ser, CaptureState::ActiveCapturing, stages, res.name, NULL);

    SetInitialChunk(id, scope.Get());
    return;
  }
  else if(res.Namespace == eResTexture)
  {
    PrepareTextureInitialContents(id, id, res);
    return;
  }
  else if(res.Namespace == eResFramebuffer)
  {
    FramebufferInitialData &data = initContents.fbo;

    ContextPair &ctx = m_Driver->GetCtx();

    RDCASSERT(!data.valid);
    data.valid = true;

    GLuint prevread = 0, prevdraw = 0;
    GL.glGetIntegerv(eGL_DRAW_FRAMEBUFFER_BINDING, (GLint *)&prevdraw);
    GL.glGetIntegerv(eGL_READ_FRAMEBUFFER_BINDING, (GLint *)&prevread);

    GL.glBindFramebuffer(eGL_DRAW_FRAMEBUFFER, res.name);
    GL.glBindFramebuffer(eGL_READ_FRAMEBUFFER, res.name);

    // need to serialise out which objects are bound
    GLenum type = eGL_TEXTURE;
    GLuint object = 0;
    GLint layered = 0;
    for(int i = 0; i < (int)ARRAY_COUNT(data.Attachments); i++)
    {
      FramebufferAttachmentData &a = data.Attachments[i];
      GLenum attachment = FramebufferInitialData::attachmentNames[i];

      GL.glGetNamedFramebufferAttachmentParameterivEXT(
          res.name, attachment, eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, (GLint *)&object);
      GL.glGetNamedFramebufferAttachmentParameterivEXT(
          res.name, attachment, eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, (GLint *)&type);

      layered = 0;
      a.level = 0;
      a.layer = 0;

      if(object && type != eGL_RENDERBUFFER)
      {
        GL.glGetNamedFramebufferAttachmentParameterivEXT(
            res.name, attachment, eGL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL, &a.level);

        if(HasExt[ARB_geometry_shader4])
          GL.glGetNamedFramebufferAttachmentParameterivEXT(
              res.name, attachment, eGL_FRAMEBUFFER_ATTACHMENT_LAYERED, &layered);
        else
          layered = 0;

        if(layered == 0)
          GL.glGetNamedFramebufferAttachmentParameterivEXT(
              res.name, attachment, eGL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LAYER, &a.layer);

        if(HasExt[EXT_multisampled_render_to_texture])
          GL.glGetNamedFramebufferAttachmentParameterivEXT(
              res.name, attachment, eGL_FRAMEBUFFER_ATTACHMENT_TEXTURE_SAMPLES_EXT,
              &a.numVirtualSamples);

        if(HasExt[OVR_multiview])
        {
          GL.glGetNamedFramebufferAttachmentParameterivEXT(
              res.name, attachment, eGL_FRAMEBUFFER_ATTACHMENT_TEXTURE_NUM_VIEWS_OVR, &a.numViews);
          GL.glGetNamedFramebufferAttachmentParameterivEXT(
              res.name, attachment, eGL_FRAMEBUFFER_ATTACHMENT_TEXTURE_BASE_VIEW_INDEX_OVR,
              &a.startView);
        }
      }

      a.layered = (layered != 0);
      a.obj = (type == eGL_RENDERBUFFER) ? RenderbufferRes(ctx, object) : TextureRes(ctx, object);

      if(type != eGL_RENDERBUFFER)
      {
        WrappedOpenGL::TextureData &details = m_Driver->m_Textures[GetID(a.obj)];

        if(details.curType == eGL_TEXTURE_CUBE_MAP)
        {
          GLenum face;
          GL.glGetNamedFramebufferAttachmentParameterivEXT(
              res.name, attachment, eGL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE, (GLint *)&face);

          a.layer = CubeTargetIndex(face);
        }
      }
    }

    GLuint maxDraws = 0;
    GL.glGetIntegerv(eGL_MAX_DRAW_BUFFERS, (GLint *)&maxDraws);

    for(GLuint i = 0; i < (GLuint)ARRAY_COUNT(data.DrawBuffers); i++)
    {
      if(i < maxDraws)
        GL.glGetIntegerv(GLenum(eGL_DRAW_BUFFER0 + i), (GLint *)&data.DrawBuffers[i]);
      else
        data.DrawBuffers[i] = eGL_COLOR_ATTACHMENT0;
    }

    GL.glGetIntegerv(eGL_READ_BUFFER, (GLint *)&data.ReadBuffer);

    GL.glBindFramebuffer(eGL_DRAW_FRAMEBUFFER, prevdraw);
    GL.glBindFramebuffer(eGL_READ_FRAMEBUFFER, prevread);
  }
  else if(res.Namespace == eResProgramPipe)
  {
    PipelineInitialData &data = initContents.pipe;

    RDCASSERT(!data.valid);
    data.valid = true;

    // programs are shared
    void *shareGroup = m_Driver->GetCtx().shareGroup;

    for(GLuint i = 0; i < (GLuint)ARRAY_COUNT(data.programs); i++)
    {
      data.programs[i].Namespace = eResProgram;
      data.programs[i].ContextShareGroup = shareGroup;
    }

    GL.glGetProgramPipelineiv(res.name, eGL_VERTEX_SHADER, (GLint *)&data.programs[0].name);
    GL.glGetProgramPipelineiv(res.name, eGL_FRAGMENT_SHADER, (GLint *)&data.programs[4].name);
    GL.glGetProgramPipelineiv(res.name, eGL_GEOMETRY_SHADER, (GLint *)&data.programs[3].name);
    GL.glGetProgramPipelineiv(res.name, eGL_TESS_CONTROL_SHADER, (GLint *)&data.programs[1].name);
    GL.glGetProgramPipelineiv(res.name, eGL_TESS_EVALUATION_SHADER, (GLint *)&data.programs[2].name);
    GL.glGetProgramPipelineiv(res.name, eGL_COMPUTE_SHADER, (GLint *)&data.programs[5].name);
  }
  else if(res.Namespace == eResSampler)
  {
    SamplerInitialData &data = initContents.samp;

    RDCASSERT(!data.valid);
    data.valid = true;

    GLenum activeTexture = eGL_TEXTURE0;
    GL.glGetIntegerv(eGL_ACTIVE_TEXTURE, (GLint *)&activeTexture);

    GL.glActiveTexture(eGL_TEXTURE0);

    GLuint prevsampler = 0;
    GL.glGetIntegerv(eGL_SAMPLER_BINDING, (GLint *)&prevsampler);

    {
      GL.glGetSamplerParameteriv(res.name, eGL_TEXTURE_COMPARE_FUNC, (GLint *)&data.compareFunc);
      GL.glGetSamplerParameteriv(res.name, eGL_TEXTURE_COMPARE_MODE, (GLint *)&data.compareMode);
      GL.glGetSamplerParameteriv(res.name, eGL_TEXTURE_MIN_FILTER, (GLint *)&data.minFilter);
      GL.glGetSamplerParameteriv(res.name, eGL_TEXTURE_MAG_FILTER, (GLint *)&data.magFilter);
      GL.glGetSamplerParameteriv(res.name, eGL_TEXTURE_WRAP_R, (GLint *)&data.wrap[0]);
      GL.glGetSamplerParameteriv(res.name, eGL_TEXTURE_WRAP_S, (GLint *)&data.wrap[1]);
      GL.glGetSamplerParameteriv(res.name, eGL_TEXTURE_WRAP_T, (GLint *)&data.wrap[2]);
      GL.glGetSamplerParameterfv(res.name, eGL_TEXTURE_MIN_LOD, &data.minLod);
      GL.glGetSamplerParameterfv(res.name, eGL_TEXTURE_MAX_LOD, &data.maxLod);
      if(!IsGLES)
        GL.glGetSamplerParameterfv(res.name, eGL_TEXTURE_LOD_BIAS, &data.lodBias);

      // technically border color has been in since GL 1.0, but since this extension was really
      // early and dovetails nicely with OES_texture_border_color which added both border colors and
      // clamping, we check it.
      if(HasExt[ARB_texture_border_clamp])
        GL.glGetSamplerParameterfv(res.name, eGL_TEXTURE_BORDER_COLOR, &data.border[0]);
      else
        data.border[0] = data.border[1] = data.border[2] = data.border[3] = 1.0f;
    }

    GL.glBindSampler(0, prevsampler);

    GL.glActiveTexture(activeTexture);
  }
  else if(res.Namespace == eResFeedback)
  {
    FeedbackInitialData &data = initContents.xfb;

    RDCASSERT(!data.valid);
    data.valid = true;

    ContextPair &ctx = m_Driver->GetCtx();

    GLuint prevfeedback = 0;
    GL.glGetIntegerv(eGL_TRANSFORM_FEEDBACK_BINDING, (GLint *)&prevfeedback);

    GL.glBindTransformFeedback(eGL_TRANSFORM_FEEDBACK, res.name);

    GLint maxCount = 0;
    GL.glGetIntegerv(eGL_MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS, &maxCount);

    for(int i = 0; i < (int)ARRAY_COUNT(data.Buffer) && i < maxCount; i++)
    {
      GLuint buffer = 0;
      GL.glGetIntegeri_v(eGL_TRANSFORM_FEEDBACK_BUFFER_BINDING, i, (GLint *)&buffer);
      data.Buffer[i] = BufferRes(ctx, buffer);
      GL.glGetInteger64i_v(eGL_TRANSFORM_FEEDBACK_BUFFER_START, i, (GLint64 *)&data.Offset[i]);
      GL.glGetInteger64i_v(eGL_TRANSFORM_FEEDBACK_BUFFER_SIZE, i, (GLint64 *)&data.Size[i]);
    }

    GL.glBindTransformFeedback(eGL_TRANSFORM_FEEDBACK, prevfeedback);
  }
  else if(res.Namespace == eResVertexArray)
  {
    VAOInitialData &data = initContents.vao;

    RDCASSERT(!data.valid);
    data.valid = true;

    ContextPair &ctx = m_Driver->GetCtx();

    GLuint prevVAO = 0;
    GL.glGetIntegerv(eGL_VERTEX_ARRAY_BINDING, (GLint *)&prevVAO);

    GL.glBindVertexArray(res.name);

    for(GLuint i = 0; i < 16; i++)
    {
      GLuint buffer = GetBoundVertexBuffer(i);

      data.VertexBuffers[i].Buffer = BufferRes(ctx, buffer);
    }

    for(GLuint i = 0; i < 16; i++)
    {
      GL.glGetVertexAttribiv(i, eGL_VERTEX_ATTRIB_ARRAY_ENABLED,
                             (GLint *)&data.VertexAttribs[i].enabled);
      GL.glGetVertexAttribiv(i, eGL_VERTEX_ATTRIB_ARRAY_TYPE, (GLint *)&data.VertexAttribs[i].type);
      GL.glGetVertexAttribiv(i, eGL_VERTEX_ATTRIB_ARRAY_NORMALIZED,
                             (GLint *)&data.VertexAttribs[i].normalized);

      // no extension for this, it just appeared in GL & GLES 3.0, along with glVertexAttribIPointer
      if(GLCoreVersion >= 3.0)
        GL.glGetVertexAttribiv(i, eGL_VERTEX_ATTRIB_ARRAY_INTEGER,
                               (GLint *)&data.VertexAttribs[i].integer);
      else
        data.VertexAttribs[i].integer = 0;

      GL.glGetVertexAttribiv(i, eGL_VERTEX_ATTRIB_ARRAY_SIZE, (GLint *)&data.VertexAttribs[i].size);

      if(HasExt[ARB_vertex_attrib_binding])
      {
        GL.glGetIntegeri_v(eGL_VERTEX_BINDING_STRIDE, i, (GLint *)&data.VertexBuffers[i].Stride);
        GL.glGetIntegeri_v(eGL_VERTEX_BINDING_OFFSET, i, (GLint *)&data.VertexBuffers[i].Offset);
        GL.glGetIntegeri_v(eGL_VERTEX_BINDING_DIVISOR, i, (GLint *)&data.VertexBuffers[i].Divisor);

        GL.glGetVertexAttribiv(i, eGL_VERTEX_ATTRIB_RELATIVE_OFFSET,
                               (GLint *)&data.VertexAttribs[i].offset);
        GL.glGetVertexAttribiv(i, eGL_VERTEX_ATTRIB_BINDING, (GLint *)&data.VertexAttribs[i].vbslot);
      }
      else
      {
        GL.glGetVertexAttribiv(i, eGL_VERTEX_ATTRIB_ARRAY_STRIDE,
                               (GLint *)&data.VertexBuffers[i].Stride);
        GL.glGetVertexAttribiv(i, eGL_VERTEX_ATTRIB_ARRAY_DIVISOR,
                               (GLint *)&data.VertexBuffers[i].Divisor);
        data.VertexAttribs[i].vbslot = i;
        data.VertexBuffers[i].Offset = 0;

        void *ptr = NULL;
        GL.glGetVertexAttribPointerv(i, eGL_VERTEX_ATTRIB_ARRAY_POINTER, &ptr);

        data.VertexAttribs[i].offset = (uint32_t)(uintptr_t)ptr;
      }

      // if no buffer is bound, replace any non-zero offset with a marker value. This makes captures
      // more deterministic and ensures that if we ever try to use the invalid offset/pointer then
      // we crash with a known value.
      if(data.VertexBuffers[data.VertexAttribs[i].vbslot].Buffer.name == 0 &&
         data.VertexAttribs[i].offset > 0)
        data.VertexAttribs[i].offset = 0xDEADBEEF;

      if(data.VertexBuffers[i].Buffer.name == 0 && data.VertexBuffers[i].Offset > 0)
        data.VertexBuffers[i].Offset = 0xDEADBEEF;
    }

    GLuint buffer = 0;
    GL.glGetIntegerv(eGL_ELEMENT_ARRAY_BUFFER_BINDING, (GLint *)&buffer);
    data.ElementArrayBuffer = BufferRes(ctx, buffer);

    GL.glBindVertexArray(prevVAO);
  }
  else if(res.Namespace == eResRenderbuffer)
  {
    //
  }
  else
  {
    RDCERR("Unexpected type of resource requiring initial state");
  }

  if(IsReplayMode(m_State))
    SetInitialContents(GetOriginalID(id), initContents);
  else
    SetInitialContents(id, initContents);
}

bool GLResourceManager::Prepare_InitialState(GLResource res)
{
  // We need to fetch the data for this resource on the right context.
  // It's not safe for us to go changing contexts ourselves (the context could be active on
  // another thread), so instead we'll queue this up to fetch when we are on a correct context.
  // The correct context depends on whether the object is shared or not - if it's shared, any
  // context in the same share group will do, otherwise it must be precisely the right context
  //
  // Because we've already allocated and set the blob above, it can be filled in any time
  // before serialising (end of the frame, and if the context is never used before the end of
  // the frame the resource can't be used, so not fetching the initial state doesn't matter).
  //
  // Note we also need to detect the case where the context is already current on another thread
  // and we just start getting commands there, but that case already isn't supported as we don't
  // detect it and insert state-change chunks, we assume all commands will come from a single
  // thread.
  ContextPair &ctx = m_Driver->GetCtx();
  if(res.ContextShareGroup == ctx.ctx || res.ContextShareGroup == ctx.shareGroup)
  {
    // call immediately, we are on the right context or share group
    ContextPrepare_InitialState(res);
  }
  else
  {
    m_Driver->QueuePrepareInitialState(res);
  }

  return true;
}

void GLResourceManager::CreateTextureImage(GLuint tex, GLenum internalFormat,
                                           GLenum internalFormatHint, GLenum textype, GLint dim,
                                           GLint width, GLint height, GLint depth, GLint samples,
                                           int mips)
{
  if(textype == eGL_TEXTURE_BUFFER)
  {
    return;
  }
  else if(textype == eGL_TEXTURE_2D_MULTISAMPLE)
  {
    GL.glTextureStorage2DMultisampleEXT(tex, textype, samples, internalFormat, width, height,
                                        GL_TRUE);
  }
  else if(textype == eGL_TEXTURE_2D_MULTISAMPLE_ARRAY)
  {
    GL.glTextureStorage3DMultisampleEXT(tex, textype, samples, internalFormat, width, height, depth,
                                        GL_TRUE);
  }
  else
  {
    GL.glTextureParameteriEXT(tex, textype, eGL_TEXTURE_MAX_LEVEL, mips - 1);
    GL.glTextureParameteriEXT(tex, textype, eGL_TEXTURE_MIN_FILTER, eGL_NEAREST);
    GL.glTextureParameteriEXT(tex, textype, eGL_TEXTURE_MAG_FILTER, eGL_NEAREST);
    GL.glTextureParameteriEXT(tex, textype, eGL_TEXTURE_WRAP_S, eGL_CLAMP_TO_EDGE);
    GL.glTextureParameteriEXT(tex, textype, eGL_TEXTURE_WRAP_T, eGL_CLAMP_TO_EDGE);

    bool isCompressed = IsCompressedFormat(internalFormat);

    GLenum baseFormat = eGL_RGBA;
    GLenum dataType = internalFormatHint != eGL_NONE ? internalFormatHint : eGL_UNSIGNED_BYTE;
    if(!isCompressed)
    {
      baseFormat = GetBaseFormat(internalFormat);

      if(internalFormatHint == eGL_NONE)
        dataType = GetDataType(internalFormat);
    }

    GLenum targets[] = {
        eGL_TEXTURE_CUBE_MAP_POSITIVE_X, eGL_TEXTURE_CUBE_MAP_NEGATIVE_X,
        eGL_TEXTURE_CUBE_MAP_POSITIVE_Y, eGL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
        eGL_TEXTURE_CUBE_MAP_POSITIVE_Z, eGL_TEXTURE_CUBE_MAP_NEGATIVE_Z,
    };

    int count = ARRAY_COUNT(targets);

    if(textype != eGL_TEXTURE_CUBE_MAP)
    {
      targets[0] = textype;
      count = 1;
    }

    GLsizei w = (GLsizei)width;
    GLsizei h = (GLsizei)height;
    GLsizei d = (GLsizei)depth;

    for(int m = 0; m < mips; m++)
    {
      for(int t = 0; t < count; t++)
      {
        if(isCompressed)
        {
          GLsizei compSize = (GLsizei)GetCompressedByteSize(w, h, d, internalFormat);

          std::vector<byte> dummy;
          dummy.resize(compSize);

          if(dim == 1)
            GL.glCompressedTextureImage1DEXT(tex, targets[t], m, internalFormat, w, 0, compSize,
                                             &dummy[0]);
          else if(dim == 2)
            GL.glCompressedTextureImage2DEXT(tex, targets[t], m, internalFormat, w, h, 0, compSize,
                                             &dummy[0]);
          else if(dim == 3)
            GL.glCompressedTextureImage3DEXT(tex, targets[t], m, internalFormat, w, h, d, 0,
                                             compSize, &dummy[0]);
        }
        else
        {
          if(dim == 1)
            GL.glTextureImage1DEXT(tex, targets[t], m, internalFormat, w, 0, baseFormat, dataType,
                                   NULL);
          else if(dim == 2)
            GL.glTextureImage2DEXT(tex, targets[t], m, internalFormat, w, h, 0, baseFormat,
                                   dataType, NULL);
          else if(dim == 3)
            GL.glTextureImage3DEXT(tex, targets[t], m, internalFormat, w, h, d, 0, baseFormat,
                                   dataType, NULL);
        }
      }

      w = RDCMAX(1, w >> 1);
      if(textype != eGL_TEXTURE_1D_ARRAY)
        h = RDCMAX(1, h >> 1);
      if(textype != eGL_TEXTURE_2D_ARRAY && textype != eGL_TEXTURE_CUBE_MAP_ARRAY)
        d = RDCMAX(1, d >> 1);
    }
  }

  if(IsCaptureMode(m_State))
  {
    // register this texture and set up its texture details, so it's available for emulation
    // readback.
    GLResource res = TextureRes(m_Driver->GetCtx(), tex);
    ResourceId id = RegisterResource(res);

    WrappedOpenGL::TextureData &details = m_Driver->m_Textures[id];

    details.resource = res;
    details.curType = textype;
    details.dimension = dim;
    details.emulated = details.view = false;
    details.width = width;
    details.height = height;
    details.depth = depth;
    details.samples = samples;
    details.creationFlags = TextureCategory::NoFlags;
    details.internalFormat = internalFormat;
    details.mipsValid = (1 << mips) - 1;
  }
}

void GLResourceManager::PrepareTextureInitialContents(ResourceId liveid, ResourceId origid,
                                                      GLResource res)
{
  WrappedOpenGL::TextureData &details = m_Driver->m_Textures[liveid];

  GLInitialContents initContents;

  initContents.type = eResTexture;

  TextureStateInitialData &state = initContents.tex;

  state.internalformat = details.internalFormat;
  state.isView = details.view;
  state.width = details.width;
  state.height = details.height;
  state.depth = details.depth;
  state.samples = details.samples;
  state.dim = details.dimension;
  state.type = details.curType;
  state.mips = 1;

  if(details.internalFormat == eGL_NONE)
  {
    // textures can get here as GL_NONE if they were created and dirtied (by setting lots of
    // texture parameters) without ever having storage allocated (via glTexStorage or glTexImage).
    // in that case, just ignore as we won't bother with the initial states.
  }
  else if(details.curType != eGL_TEXTURE_BUFFER)
  {
    GLenum binding = TextureBinding(details.curType);

    state.mips = GetNumMips(details.curType, res.name, details.width, details.height, details.depth);

    bool ms = (details.curType == eGL_TEXTURE_2D_MULTISAMPLE ||
               details.curType == eGL_TEXTURE_2D_MULTISAMPLE_ARRAY);

    state.depthMode = eGL_NONE;
    if(IsDepthStencilFormat(details.internalFormat))
    {
      if(HasExt[ARB_stencil_texturing])
        GL.glGetTextureParameterivEXT(res.name, details.curType, eGL_DEPTH_STENCIL_TEXTURE_MODE,
                                      (GLint *)&state.depthMode);
      else
        state.depthMode = eGL_DEPTH_COMPONENT;
    }

    state.seamless = GL_FALSE;
    if((details.curType == eGL_TEXTURE_CUBE_MAP || details.curType == eGL_TEXTURE_CUBE_MAP_ARRAY) &&
       HasExt[ARB_seamless_cubemap_per_texture])
      GL.glGetTextureParameterivEXT(res.name, details.curType, eGL_TEXTURE_CUBE_MAP_SEAMLESS,
                                    (GLint *)&state.seamless);

    GL.glGetTextureParameterivEXT(res.name, details.curType, eGL_TEXTURE_BASE_LEVEL,
                                  (GLint *)&state.baseLevel);
    GL.glGetTextureParameterivEXT(res.name, details.curType, eGL_TEXTURE_MAX_LEVEL,
                                  (GLint *)&state.maxLevel);

    if(HasExt[ARB_texture_swizzle] || HasExt[EXT_texture_swizzle])
    {
      GetTextureSwizzle(res.name, details.curType, state.swizzle);
    }
    else
    {
      state.swizzle[0] = eGL_RED;
      state.swizzle[1] = eGL_GREEN;
      state.swizzle[2] = eGL_BLUE;
      state.swizzle[3] = eGL_ALPHA;
    }

    // only non-ms textures have sampler state
    if(!ms)
    {
      if(HasExt[EXT_texture_sRGB_decode])
        GL.glGetTextureParameterivEXT(res.name, details.curType, eGL_TEXTURE_SRGB_DECODE_EXT,
                                      (GLint *)&state.srgbDecode);
      else
        state.srgbDecode = eGL_DECODE_EXT;
      GL.glGetTextureParameterivEXT(res.name, details.curType, eGL_TEXTURE_COMPARE_FUNC,
                                    (GLint *)&state.compareFunc);
      GL.glGetTextureParameterivEXT(res.name, details.curType, eGL_TEXTURE_COMPARE_MODE,
                                    (GLint *)&state.compareMode);
      GL.glGetTextureParameterivEXT(res.name, details.curType, eGL_TEXTURE_MIN_FILTER,
                                    (GLint *)&state.minFilter);
      GL.glGetTextureParameterivEXT(res.name, details.curType, eGL_TEXTURE_MAG_FILTER,
                                    (GLint *)&state.magFilter);
      GL.glGetTextureParameterivEXT(res.name, details.curType, eGL_TEXTURE_WRAP_R,
                                    (GLint *)&state.wrap[0]);
      GL.glGetTextureParameterivEXT(res.name, details.curType, eGL_TEXTURE_WRAP_S,
                                    (GLint *)&state.wrap[1]);
      GL.glGetTextureParameterivEXT(res.name, details.curType, eGL_TEXTURE_WRAP_T,
                                    (GLint *)&state.wrap[2]);
      GL.glGetTextureParameterfvEXT(res.name, details.curType, eGL_TEXTURE_MIN_LOD, &state.minLod);
      GL.glGetTextureParameterfvEXT(res.name, details.curType, eGL_TEXTURE_MAX_LOD, &state.maxLod);

      // technically border color has been in since GL 1.0, but since this extension was really
      // early and dovetails nicely with OES_texture_border_color which added both border colors and
      // clamping, we check it.
      if(HasExt[ARB_texture_border_clamp])
        GL.glGetTextureParameterfvEXT(res.name, details.curType, eGL_TEXTURE_BORDER_COLOR,
                                      &state.border[0]);
      else
        state.border[0] = state.border[1] = state.border[2] = state.border[3] = 1.0f;

      if(!IsGLES)
        GL.glGetTextureParameterfvEXT(res.name, details.curType, eGL_TEXTURE_LOD_BIAS,
                                      &state.lodBias);

      // CLAMP isn't supported (border texels gone), assume they meant CLAMP_TO_EDGE
      if(state.wrap[0] == eGL_CLAMP)
        state.wrap[0] = eGL_CLAMP_TO_EDGE;
      if(state.wrap[1] == eGL_CLAMP)
        state.wrap[1] = eGL_CLAMP_TO_EDGE;
      if(state.wrap[2] == eGL_CLAMP)
        state.wrap[2] = eGL_CLAMP_TO_EDGE;
    }

    // we only copy contents for non-views
    GLuint tex = 0;

    if(!details.view)
    {
      {
        GLuint oldtex = 0;
        GL.glGetIntegerv(binding, (GLint *)&oldtex);

        GL.glGenTextures(1, &tex);
        GL.glBindTexture(details.curType, tex);

        GL.glBindTexture(details.curType, oldtex);
      }

      int mips = GetNumMips(details.curType, res.name, details.width, details.height, details.depth);

      if(details.curType == eGL_TEXTURE_2D_MULTISAMPLE ||
         details.curType == eGL_TEXTURE_2D_MULTISAMPLE_ARRAY)
        mips = 1;

      // create texture of identical format/size to store initial contents
      CreateTextureImage(tex, details.internalFormat, details.internalFormatHint, details.curType,
                         details.dimension, details.width, details.height, details.depth,
                         details.samples, mips);

      // we need to set maxlevel appropriately for number of mips to force the texture to be
      // complete.
      // This can happen if e.g. a texture is initialised just by default with glTexImage for level
      // 0 and used as a framebuffer attachment, then the implementation is fine with it.
      // Unfortunately glCopyImageSubData requires completeness across all mips, a stricter
      // requirement :(.
      // We set max_level to mips - 1 (so mips=1 means MAX_LEVEL=0). Then restore it to the 'real'
      // value we fetched above
      int maxlevel = mips - 1;
      GL.glTextureParameterivEXT(res.name, details.curType, eGL_TEXTURE_MAX_LEVEL,
                                 (GLint *)&maxlevel);

      // set min/mag filters to NEAREST since we are doing an identity copy. Avoids issues where the
      // spec says that e.g. integer or stencil textures cannot have a LINEAR filter
      if(!ms)
      {
        GLenum nearest = eGL_NEAREST;
        GL.glTextureParameterivEXT(res.name, details.curType, eGL_TEXTURE_MIN_FILTER,
                                   (GLint *)&nearest);
        GL.glTextureParameterivEXT(res.name, details.curType, eGL_TEXTURE_MAG_FILTER,
                                   (GLint *)&nearest);
      }

      bool iscomp = IsCompressedFormat(details.internalFormat);

      bool avoidCopySubImage = false;
      if(iscomp && VendorCheck[VendorCheck_AMD_copy_compressed_tinymips])
        avoidCopySubImage = true;
      if(iscomp && details.curType == eGL_TEXTURE_CUBE_MAP &&
         VendorCheck[VendorCheck_AMD_copy_compressed_cubemaps])
        avoidCopySubImage = true;
      if(iscomp && IsGLES)
        avoidCopySubImage = true;

      PixelPackState pack;
      PixelUnpackState unpack;
      GLuint pixelPackBuffer = 0;
      GLuint pixelUnpackBuffer = 0;

      if(avoidCopySubImage)
      {
        pack.Fetch(false);
        unpack.Fetch(false);

        ResetPixelPackState(false, 1);
        ResetPixelUnpackState(false, 1);

        GL.glGetIntegerv(eGL_PIXEL_PACK_BUFFER_BINDING, (GLint *)&pixelPackBuffer);
        GL.glGetIntegerv(eGL_PIXEL_UNPACK_BUFFER_BINDING, (GLint *)&pixelUnpackBuffer);
        GL.glBindBuffer(eGL_PIXEL_PACK_BUFFER, 0);
        GL.glBindBuffer(eGL_PIXEL_UNPACK_BUFFER, 0);
      }

      // copy over mips
      for(int i = 0; i < mips; i++)
      {
        int w = RDCMAX(details.width >> i, 1);
        int h = RDCMAX(details.height >> i, 1);
        int d = RDCMAX(details.depth >> i, 1);

        if(details.curType == eGL_TEXTURE_CUBE_MAP)
          d *= 6;
        else if(details.curType == eGL_TEXTURE_CUBE_MAP_ARRAY ||
                details.curType == eGL_TEXTURE_1D_ARRAY || details.curType == eGL_TEXTURE_2D_ARRAY)
          d = details.depth;

        // AMD throws an error copying mips that are smaller than the block size in one dimension,
        // so do copy via CPU instead (will be slow, potentially we could optimise this if there's a
        // different GPU-side image copy routine that works on these dimensions. Hopefully there'll
        // only be a couple of such mips).
        // AMD also has issues copying cubemaps
        // glCopyImageSubData does not seem to work at all for compressed textures on GLES (at least
        // with some tested drivers and texture types)
        if((iscomp && VendorCheck[VendorCheck_AMD_copy_compressed_tinymips] && (w < 4 || h < 4)) ||
           (iscomp && VendorCheck[VendorCheck_AMD_copy_compressed_cubemaps] &&
            details.curType == eGL_TEXTURE_CUBE_MAP) ||
           (iscomp && IsGLES))
        {
          GLenum targets[] = {
              eGL_TEXTURE_CUBE_MAP_POSITIVE_X, eGL_TEXTURE_CUBE_MAP_NEGATIVE_X,
              eGL_TEXTURE_CUBE_MAP_POSITIVE_Y, eGL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
              eGL_TEXTURE_CUBE_MAP_POSITIVE_Z, eGL_TEXTURE_CUBE_MAP_NEGATIVE_Z,
          };

          int count = ARRAY_COUNT(targets);

          if(details.curType != eGL_TEXTURE_CUBE_MAP)
          {
            targets[0] = details.curType;
            count = 1;
          }

          for(int trg = 0; trg < count; trg++)
          {
            size_t size = GetCompressedByteSize(w, h, d, details.internalFormat);

            if(details.curType == eGL_TEXTURE_CUBE_MAP)
              size /= 6;

            byte *buf = new byte[size];

            if(IsGLES)
            {
              details.GetCompressedImageDataGLES(i, targets[trg], size, buf);
            }
            else
            {
              // read to CPU
              GL.glGetCompressedTextureImageEXT(res.name, targets[trg], i, buf);
            }

            // write to GPU
            if(details.dimension == 1)
              GL.glCompressedTextureSubImage1DEXT(tex, targets[trg], i, 0, w,
                                                  details.internalFormat, (GLsizei)size, buf);
            else if(details.dimension == 2)
              GL.glCompressedTextureSubImage2DEXT(tex, targets[trg], i, 0, 0, w, h,
                                                  details.internalFormat, (GLsizei)size, buf);
            else if(details.dimension == 3)
              GL.glCompressedTextureSubImage3DEXT(tex, targets[trg], i, 0, 0, 0, w, h, d,
                                                  details.internalFormat, (GLsizei)size, buf);

            delete[] buf;
          }
        }
        else
        {
          // it seems like everything explodes if I do glCopyImageSubData on a D32F_S8 texture -
          // in-program the overlay gets corrupted as one UBO seems to not provide data anymore
          // until it's "refreshed". It seems like a driver bug, nvidia specific. In most cases a
          // program isn't going to rely on the contents of a depth-stencil buffer (shadow maps that
          // it might require would be depth-only formatted).
          if(details.internalFormat == eGL_DEPTH32F_STENCIL8 &&
             VendorCheck[VendorCheck_NV_avoid_D32S8_copy])
          {
            RDCDEBUG("Not fetching initial contents of D32F_S8 texture");
          }
          else
          {
            if(VendorCheck[VendorCheck_Qualcomm_avoid_glCopyImageSubData])
              glEmulate::_glCopyImageSubData(res.name, details.curType, i, 0, 0, 0, tex,
                                             details.curType, i, 0, 0, 0, w, h, d);
            else
              GL.glCopyImageSubData(res.name, details.curType, i, 0, 0, 0, tex, details.curType, i,
                                    0, 0, 0, w, h, d);
          }
        }
      }

      if(avoidCopySubImage)
      {
        pack.Apply(false);
        unpack.Apply(false);

        GL.glBindBuffer(eGL_PIXEL_PACK_BUFFER, pixelPackBuffer);
        GL.glBindBuffer(eGL_PIXEL_UNPACK_BUFFER, pixelUnpackBuffer);
      }

      GL.glTextureParameterivEXT(res.name, details.curType, eGL_TEXTURE_MAX_LEVEL,
                                 (GLint *)&state.maxLevel);

      if(!ms)
      {
        GL.glTextureParameterivEXT(res.name, details.curType, eGL_TEXTURE_MIN_FILTER,
                                   (GLint *)&state.minFilter);
        GL.glTextureParameterivEXT(res.name, details.curType, eGL_TEXTURE_MAG_FILTER,
                                   (GLint *)&state.magFilter);
      }
    }

    initContents.resource = GLResource(res.ContextShareGroup, eResTexture, tex);
  }
  else
  {
    // record texbuffer only state

    GLuint bufName = 0;
    GL.glGetTextureLevelParameterivEXT(res.name, details.curType, 0,
                                       eGL_TEXTURE_BUFFER_DATA_STORE_BINDING, (GLint *)&bufName);
    state.texBuffer = GLResource(res.ContextShareGroup, eResBuffer, bufName);

    GL.glGetTextureLevelParameterivEXT(res.name, details.curType, 0, eGL_TEXTURE_BUFFER_OFFSET,
                                       (GLint *)&state.texBufOffs);
    GL.glGetTextureLevelParameterivEXT(res.name, details.curType, 0, eGL_TEXTURE_BUFFER_SIZE,
                                       (GLint *)&state.texBufSize);
  }

  SetInitialContents(origid, initContents);
}

void GLResourceManager::Force_ReferenceViews()
{
  // don't need to force anything if we're already including all resources
  if(RenderDoc::Inst().GetCaptureOptions().refAllResources)
    return;

  for(auto recordit = m_ResourceRecords.begin(); recordit != m_ResourceRecords.end(); ++recordit)
  {
    GLResourceRecord *record = recordit->second;

    // if this resource has some viewers, check to see if they were referenced by the frame but we
    // weren't, and force our own reference as well so that our initial states are included
    if(record && !record->viewTextures.empty())
    {
      // if this data resource was referenced already, just skip
      if(m_FrameReferencedResources.find(record->GetResourceID()) != m_FrameReferencedResources.end())
        continue;

      // see if any of our viewers were referenced
      for(auto it = record->viewTextures.begin(); it != record->viewTextures.end(); ++it)
      {
        // if so, return true to force our inclusion, for the benefit of the view
        if(m_FrameReferencedResources.find(*it) != m_FrameReferencedResources.end())
        {
          RDCDEBUG("Forcing inclusion of %llu for %llu", record->GetResourceID(), *it);
          MarkResourceFrameReferenced(record->GetResourceID(), eFrameRef_ReadBeforeWrite);
          break;
        }
      }
    }
  }
}

uint64_t GLResourceManager::GetSize_InitialState(ResourceId resid, const GLInitialContents &initial)
{
  if(initial.type == eResBuffer)
  {
    // buffers just have their contents, no metadata needed
    return initial.bufferLength + WriteSerialiser::GetChunkAlignment() + 16;
  }
  else if(initial.type == eResProgram)
  {
    // need to estimate based on how many bindings and uniforms there are. This is a rare path -
    // only happening when a program is created at runtime in the middle of a frameand we didn't
    // prepare its initial contents. So we take a less efficient route by just serialising the
    // current contents and using that as our size estimate, then throwing away the contents.
    WriteSerialiser ser(new StreamWriter(4 * 1024), Ownership::Stream);

    SCOPED_SERIALISE_CHUNK(SystemChunk::InitialContents);

    GLResource res = GetCurrentResource(resid);

    SERIALISE_ELEMENT(resid).TypedAs("GLResource"_lit);
    SERIALISE_ELEMENT(res.Namespace);

    PerStageReflections stages;
    m_Driver->FillReflectionArray(GetID(res), stages);

    SerialiseProgramBindings(ser, CaptureState::ActiveCapturing, stages, res.name);
    SerialiseProgramUniforms(ser, CaptureState::ActiveCapturing, stages, res.name, NULL);

    return ser.GetWriter()->GetOffset() + 256;
  }
  else if(initial.type == eResTexture)
  {
    uint64_t ret = 0;

    ret += sizeof(TextureStateInitialData) + 64;

    const TextureStateInitialData &TextureState = initial.tex;

    // in these cases, no more data is serialised
    if(TextureState.internalformat == eGL_NONE || TextureState.type == eGL_TEXTURE_BUFFER ||
       TextureState.isView)
      return ret;

    bool isCompressed = IsCompressedFormat(TextureState.internalformat);

    GLenum fmt = eGL_NONE;
    GLenum type = eGL_NONE;

    if(!isCompressed)
    {
      fmt = GetBaseFormat(TextureState.internalformat);
      type = GetDataType(TextureState.internalformat);
    }

    // otherwise loop over all the mips and estimate their size
    for(int i = 0; i < TextureState.mips; i++)
    {
      uint32_t w = RDCMAX(TextureState.width >> i, 1U);
      uint32_t h = RDCMAX(TextureState.height >> i, 1U);
      uint32_t d = RDCMAX(TextureState.depth >> i, 1U);

      if(TextureState.type == eGL_TEXTURE_CUBE_MAP_ARRAY ||
         TextureState.type == eGL_TEXTURE_1D_ARRAY || TextureState.type == eGL_TEXTURE_2D_ARRAY)
        d = TextureState.depth;

      uint32_t size = 0;

      // calculate the actual byte size of this mip
      if(isCompressed)
        size = (uint32_t)GetCompressedByteSize(w, h, d, TextureState.internalformat);
      else
        size = (uint32_t)GetByteSize(w, h, d, fmt, type);

      int targetcount = 1;

      if(TextureState.type == eGL_TEXTURE_CUBE_MAP)
        targetcount = 6;

      for(int t = 0; t < targetcount; t++)
        ret += WriteSerialiser::GetChunkAlignment() + size;
    }

    return ret;
  }
  else if(initial.type == eResFramebuffer)
  {
    return sizeof(FramebufferInitialData);
  }
  else if(initial.type == eResSampler)
  {
    // reserve some extra size to account for array count
    return sizeof(SamplerInitialData) + 32;
  }
  else if(initial.type == eResFeedback)
  {
    return sizeof(FeedbackInitialData);
  }
  else if(initial.type == eResProgramPipe)
  {
    return sizeof(PipelineInitialData);
  }
  else if(initial.type == eResVertexArray)
  {
    return sizeof(VAOInitialData);
  }
  else if(initial.type == eResRenderbuffer)
  {
  }
  else
  {
    RDCERR("Unexpected type of resource requiring initial state %d", initial.type);
  }

  return 16;
}

template <typename SerialiserType>
bool GLResourceManager::Serialise_InitialState(SerialiserType &ser, ResourceId id,
                                               GLResourceRecord *record,
                                               const GLInitialContents *initial)
{
  m_State = m_Driver->GetState();

  GLInitialContents initContents;
  if(initial)
    initContents = *initial;

  SERIALISE_ELEMENT(id).TypedAs("GLResource"_lit);
  SERIALISE_ELEMENT_LOCAL(Type, initial->type);

  if(IsReplayingAndReading())
  {
    m_Driver->AddResourceCurChunk(id);
  }

  if(Type == eResBuffer)
  {
    GLResource mappedBuffer = GLResource(MakeNullResource);
    uint32_t BufferContentsSize = 0;
    byte *BufferContents = NULL;

    if(ser.IsWriting())
    {
      mappedBuffer = initial->resource;
      BufferContentsSize = initial->bufferLength;
      BufferContents = (byte *)GL.glMapNamedBufferEXT(mappedBuffer.name, eGL_READ_ONLY);

      if(!BufferContents)
        RDCERR("Couldn't map initial contents buffer for readback!");
    }

    // Serialise this separately so that it can be used on reading to prepare the upload memory
    SERIALISE_ELEMENT(BufferContentsSize);

    if(IsReplayingAndReading())
    {
      if(!ser.IsErrored())
      {
        GL.glGenBuffers(1, &mappedBuffer.name);
        GL.glBindBuffer(eGL_COPY_WRITE_BUFFER, mappedBuffer.name);
        GL.glNamedBufferDataEXT(mappedBuffer.name, (GLsizeiptr)RDCMAX(BufferContentsSize, 4U), NULL,
                                eGL_STATIC_DRAW);
        BufferContents = (byte *)GL.glMapNamedBufferEXT(mappedBuffer.name, eGL_WRITE_ONLY);

        SetInitialContents(id, GLInitialContents(BufferRes(m_Driver->GetCtx(), mappedBuffer.name),
                                                 BufferContentsSize));
      }
    }

    // not using SERIALISE_ELEMENT_ARRAY so we can deliberately avoid allocation - we serialise
    // directly into upload memory
    ser.Serialise("BufferContents"_lit, BufferContents, BufferContentsSize, SerialiserFlags::NoFlags);

    if(mappedBuffer.name)
      GL.glUnmapNamedBufferEXT(mappedBuffer.name);

    SERIALISE_CHECK_READ_ERRORS();
  }
  else if(Type == eResProgram)
  {
    WrappedOpenGL &drv = *m_Driver;

    GLuint bindingsProgram = 0, uniformsProgram = 0;
    std::map<GLint, GLint> *translationTable = NULL;

    PerStageReflections stages;

    bool IsProgramSPIRV = false;

    if(IsReplayingAndReading())
    {
      WrappedOpenGL::ProgramData &details = m_Driver->m_Programs[GetLiveID(id)];

      m_Driver->FillReflectionArray(GetLiveID(id), stages);

      GLuint initProg = drv.glCreateProgram();

      uint32_t numShaders = 0;

      std::vector<std::string> vertexOutputs;
      for(size_t i = 0; i < ARRAY_COUNT(details.stageShaders); i++)
      {
        if(details.stageShaders[i] == ResourceId())
          continue;

        numShaders++;

        const auto &shadDetails = m_Driver->m_Shaders[details.stageShaders[i]];

        IsProgramSPIRV |= shadDetails.reflection.encoding == ShaderEncoding::SPIRV;

        GLuint shad = drv.glCreateShader(shadDetails.type);

        if(shadDetails.type == eGL_VERTEX_SHADER)
        {
          for(const SigParameter &sig : shadDetails.reflection.outputSignature)
          {
            std::string name = sig.varName;

            // look for :row added to split up matrix variables
            size_t colon = name.find(":row");

            // remove it, if present
            if(colon != std::string::npos)
              name.resize(colon);

            // only push matrix variables once
            if(std::find(vertexOutputs.begin(), vertexOutputs.end(), name) == vertexOutputs.end())
              vertexOutputs.push_back(name);
          }
        }

        if(!shadDetails.sources.empty())
        {
          char **srcs = new char *[shadDetails.sources.size()];
          for(size_t s = 0; s < shadDetails.sources.size(); s++)
            srcs[s] = (char *)shadDetails.sources[s].c_str();
          drv.glShaderSource(shad, (GLsizei)shadDetails.sources.size(), srcs, NULL);

          SAFE_DELETE_ARRAY(srcs);
          drv.glCompileShader(shad);
          drv.glAttachShader(initProg, shad);
          drv.glDeleteShader(shad);
        }
        else if(!shadDetails.spirvWords.empty())
        {
          drv.glShaderBinary(1, &shad, eGL_SHADER_BINARY_FORMAT_SPIR_V, shadDetails.spirvWords.data(),
                             (GLsizei)shadDetails.spirvWords.size() * sizeof(uint32_t));

          drv.glSpecializeShader(shad, shadDetails.entryPoint.c_str(),
                                 (GLuint)shadDetails.specIDs.size(), shadDetails.specIDs.data(),
                                 shadDetails.specValues.data());

          drv.glAttachShader(initProg, shad);
          drv.glDeleteShader(shad);
        }
        else
        {
          RDCERR("Unexpectedly empty shader in program initial state!");
        }
      }

      // Some drivers optimize out uniforms if they dont change any active vertex shader outputs.
      // This resulted in initProg locationTranslate table being -1 for a particular shader where
      // some uniforms were only intended to affect TF. Therefore set a TF mode for all varyings.
      // As the initial state program is never used for TF, this wont adversely affect anything.

      // don't print debug messages from these links - we know some might fail but as long as we
      // eventually get one to work that's fine.
      m_Driver->SuppressDebugMessages(true);

      std::vector<const char *> vertexOutputsPtr;
      vertexOutputsPtr.resize(vertexOutputs.size());
      for(size_t i = 0; i < vertexOutputs.size(); i++)
        vertexOutputsPtr[i] = vertexOutputs[i].c_str();

      if(!IsProgramSPIRV)
        drv.glTransformFeedbackVaryings(initProg, (GLsizei)vertexOutputsPtr.size(),
                                        &vertexOutputsPtr[0], eGL_INTERLEAVED_ATTRIBS);
      drv.glLinkProgram(initProg);

      GLint status = 0;
      drv.glGetProgramiv(initProg, eGL_LINK_STATUS, &status);

      // if it failed to link, first remove the varyings hack above as maybe the driver is barfing
      // on trying to make some output a varying
      if(status == 0 && !IsProgramSPIRV)
      {
        drv.glTransformFeedbackVaryings(initProg, 0, NULL, eGL_INTERLEAVED_ATTRIBS);
        drv.glLinkProgram(initProg);

        drv.glGetProgramiv(initProg, eGL_LINK_STATUS, &status);
      }

      // if it failed to link, try again as a separable program.
      // we can't do this by default because of the silly rules meaning
      // shaders need fixup to be separable-compatible.
      if(status == 0)
      {
        drv.glProgramParameteri(initProg, eGL_PROGRAM_SEPARABLE, 1);
        drv.glLinkProgram(initProg);

        drv.glGetProgramiv(initProg, eGL_LINK_STATUS, &status);
      }

      m_Driver->SuppressDebugMessages(false);

      if(status == 0)
      {
        if(numShaders == 0)
        {
          RDCWARN("No shaders attached to program");
        }
        else
        {
          char buffer[1025] = {0};
          drv.glGetProgramInfoLog(initProg, 1024, NULL, buffer);
          RDCERR("Link error: %s", buffer);
        }
      }

      // normally we'd serialise programs and uniforms into the initial state program, but on some
      // drivers uniform locations can change between it and the live program, so we serialise the
      // uniforms directly into the live program, then copy back to the initial state so that we
      // have a pristine copy of them for later use.
      bindingsProgram = initProg;
      uniformsProgram = GetLiveResource(id).name;

      translationTable = &details.locationTranslate;
    }
    else
    {
      m_Driver->FillReflectionArray(id, stages);
    }

    if(ser.IsWriting())
    {
      // most of the time Prepare_InitialState sets the serialise chunk directly on write, but if a
      // program is newly created within a frame we won't have prepared its initial contents, so we
      // need to be ready to write it out here.
      bindingsProgram = uniformsProgram = GetCurrentResource(id).name;
    }

    bool changedBindings = SerialiseProgramBindings(ser, m_State, stages, bindingsProgram);

    // re-link the program to set the new attrib bindings
    if(IsReplayingAndReading() && !ser.IsErrored() && changedBindings)
      GL.glLinkProgram(bindingsProgram);

    SerialiseProgramUniforms(ser, m_State, stages, uniformsProgram, translationTable);

    SERIALISE_CHECK_READ_ERRORS();

    if(IsReplayingAndReading())
    {
      // see above for why we're copying this back
      // we can pass in the same stages array, it's the same program essentially (reflection is
      // identical)
      CopyProgramUniforms(stages, uniformsProgram, stages, bindingsProgram);

      SetInitialContents(id, GLInitialContents(ProgramRes(m_Driver->GetCtx(), bindingsProgram), 0));
    }
  }
  else if(Type == eResTexture)
  {
    GLuint ppb = 0, pub = 0;
    PixelPackState pack;
    PixelUnpackState unpack;

    // save and restore pixel pack/unpack state. We only need one or the other but for clarity we
    // push and pop both always.
    if(ser.IsWriting() || !IsStructuredExporting(m_State))
    {
      GL.glGetIntegerv(eGL_PIXEL_PACK_BUFFER_BINDING, (GLint *)&ppb);
      GL.glGetIntegerv(eGL_PIXEL_UNPACK_BUFFER_BINDING, (GLint *)&pub);
      GL.glBindBuffer(eGL_PIXEL_PACK_BUFFER, 0);
      GL.glBindBuffer(eGL_PIXEL_UNPACK_BUFFER, 0);

      pack.Fetch(false);
      unpack.Fetch(false);

      ResetPixelPackState(false, 1);
      ResetPixelUnpackState(false, 1);
    }

    TextureStateInitialData &TextureState = initContents.tex;

    if(initial)
      TextureState = initial->tex;

    // serialise the texture metadata which was fetched during state preparation
    SERIALISE_ELEMENT(TextureState);

    // only continue with serialising the contents if the format is valid (storage allocated).
    // Otherwise this texture has no initial state to apply
    if(TextureState.internalformat != eGL_NONE && !ser.IsErrored())
    {
      WrappedOpenGL::TextureData &details =
          ser.IsWriting() ? m_Driver->m_Textures[id] : m_Driver->m_Textures[GetLiveID(id)];

      if(TextureState.type == eGL_TEXTURE_BUFFER || TextureState.isView)
      {
        // no contents to copy for texture buffer (it's copied under the buffer)
        // same applies for texture views, their data is copies under the aliased texture.
        // We just set the metadata blob.
      }
      else
      {
        // we need to treat compressed textures differently, so check it
        bool isCompressed = IsCompressedFormat(TextureState.internalformat);

        // this array will be used to iterate over cubemap faces. If we're *not* uploading a
        // cubemap, we change the targetcount to 1 below and overwrite the first element in the
        // array with the proper target.
        GLenum targets[] = {
            eGL_TEXTURE_CUBE_MAP_POSITIVE_X, eGL_TEXTURE_CUBE_MAP_NEGATIVE_X,
            eGL_TEXTURE_CUBE_MAP_POSITIVE_Y, eGL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
            eGL_TEXTURE_CUBE_MAP_POSITIVE_Z, eGL_TEXTURE_CUBE_MAP_NEGATIVE_Z,
        };

        int targetcount = ARRAY_COUNT(targets);

        if(TextureState.type != eGL_TEXTURE_CUBE_MAP)
        {
          targets[0] = TextureState.type;
          targetcount = 1;
        }

        // For real textures, if number of mips isn't sufficient, make sure to initialise the lower
        // levels. This could happen if e.g. a texture is init'd with glTexImage(level = 0), then
        // after we stop tracking it glGenerateMipmap is called.
        if(IsReplayingAndReading() && !ser.IsErrored())
        {
          GLResource liveRes = GetLiveResource(id);

          // this is only relevant for non-immutable textures
          GLint immut = 0;

          GL.glGetTextureParameterivEXT(liveRes.name, TextureState.type,
                                        eGL_TEXTURE_IMMUTABLE_FORMAT, &immut);

          GLenum dummy = eGL_RGBA;
          EmulateLuminanceFormat(liveRes.name, TextureState.type, TextureState.internalformat, dummy);

          if(immut == 0)
          {
            GLsizei w = (GLsizei)TextureState.width;
            GLsizei h = (GLsizei)TextureState.height;
            GLsizei d = (GLsizei)TextureState.depth;

            // see how many mips we actually have available
            int liveMips = GetNumMips(TextureState.type, liveRes.name, w, h, d);

            std::vector<byte> scratchBuf;

            // loop over the number of mips we should have
            for(int m = 1; m < TextureState.mips; m++)
            {
              w = RDCMAX(1, w >> 1);
              h = RDCMAX(1, h >> 1);
              d = RDCMAX(1, d >> 1);

              if(TextureState.type == eGL_TEXTURE_CUBE_MAP_ARRAY ||
                 TextureState.type == eGL_TEXTURE_1D_ARRAY ||
                 TextureState.type == eGL_TEXTURE_2D_ARRAY)
                d = (GLsizei)TextureState.depth;

              // if this mip doesn't exist yet, we must create it with dummy data.
              if(m >= liveMips)
              {
                for(int t = 0; t < targetcount; t++)
                {
                  if(isCompressed)
                  {
                    GLsizei compSize =
                        (GLsizei)GetCompressedByteSize(w, h, d, TextureState.internalformat);

                    scratchBuf.resize(compSize);

                    if(TextureState.dim == 1)
                      GL.glCompressedTextureImage1DEXT(liveRes.name, targets[t], m,
                                                       TextureState.internalformat, w, 0, compSize,
                                                       &scratchBuf[0]);
                    else if(TextureState.dim == 2)
                      GL.glCompressedTextureImage2DEXT(liveRes.name, targets[t], m,
                                                       TextureState.internalformat, w, h, 0,
                                                       compSize, &scratchBuf[0]);
                    else if(TextureState.dim == 3)
                      GL.glCompressedTextureImage3DEXT(liveRes.name, targets[t], m,
                                                       TextureState.internalformat, w, h, d, 0,
                                                       compSize, &scratchBuf[0]);
                  }
                  else
                  {
                    if(TextureState.dim == 1)
                      GL.glTextureImage1DEXT(liveRes.name, targets[t], m,
                                             TextureState.internalformat, (GLsizei)w, 0,
                                             GetBaseFormat(TextureState.internalformat),
                                             GetDataType(TextureState.internalformat), NULL);
                    else if(TextureState.dim == 2)
                      GL.glTextureImage2DEXT(liveRes.name, targets[t], m,
                                             TextureState.internalformat, (GLsizei)w, (GLsizei)h, 0,
                                             GetBaseFormat(TextureState.internalformat),
                                             GetDataType(TextureState.internalformat), NULL);
                    else if(TextureState.dim == 3)
                      GL.glTextureImage3DEXT(
                          liveRes.name, targets[t], m, TextureState.internalformat, (GLsizei)w,
                          (GLsizei)h, (GLsizei)d, 0, GetBaseFormat(TextureState.internalformat),
                          GetDataType(TextureState.internalformat), NULL);
                  }
                }
              }
            }
          }
        }
        // finished ensuring the texture has the right number of mip levels.

        GLuint tex = 0;
        GLuint prevtex = 0;

        // push the texture binding
        if(!IsStructuredExporting(m_State) && !ser.IsErrored())
          GL.glGetIntegerv(TextureBinding(TextureState.type), (GLint *)&prevtex);

        // create texture of identical format/size as the live resource to store initial contents
        if(IsReplayingAndReading() && !ser.IsErrored())
        {
          GL.glGenTextures(1, &tex);
          GL.glBindTexture(TextureState.type, tex);

          CreateTextureImage(tex, TextureState.internalformat, details.internalFormatHint,
                             TextureState.type, TextureState.dim, TextureState.width,
                             TextureState.height, TextureState.depth, TextureState.samples,
                             TextureState.mips);
        }
        else if(ser.IsWriting())
        {
          // on writing, bind the prepared texture with initial contents to grab
          tex = initial->resource.name;

          GL.glBindTexture(TextureState.type, tex);
        }

        // multisample textures have no mips
        if(TextureState.type == eGL_TEXTURE_2D_MULTISAMPLE ||
           TextureState.type == eGL_TEXTURE_2D_MULTISAMPLE_ARRAY)
          TextureState.mips = 1;

        if(TextureState.samples > 1)
        {
          GLNOTIMP("Not implemented - initial states of multisampled textures");
        }
        else
        {
          GLenum fmt = eGL_NONE;
          GLenum type = eGL_NONE;
          uint32_t size = 0;

          // fetch the maximum possible size that any mip/slice could take, so we can allocate
          // scratch memory.
          if(isCompressed)
          {
            size = (uint32_t)GetCompressedByteSize(TextureState.width, TextureState.height,
                                                   TextureState.depth, TextureState.internalformat);
          }
          else
          {
            fmt = GetBaseFormat(TextureState.internalformat);
            type = GetDataType(TextureState.internalformat);
            size = (uint32_t)GetByteSize(TextureState.width, TextureState.height,
                                         TextureState.depth, fmt, type);
          }

          // on read and write, we allocate a single buffer big enough for all mips and re-use it
          // to avoid repeated new/free.
          byte *scratchBuf = AllocAlignedBuffer(size);

          // loop over all the available mips
          for(int i = 0; i < TextureState.mips; i++)
          {
            uint32_t w = RDCMAX(TextureState.width >> i, 1U);
            uint32_t h = RDCMAX(TextureState.height >> i, 1U);
            uint32_t d = RDCMAX(TextureState.depth >> i, 1U);

            if(TextureState.type == eGL_TEXTURE_CUBE_MAP_ARRAY ||
               TextureState.type == eGL_TEXTURE_1D_ARRAY || TextureState.type == eGL_TEXTURE_2D_ARRAY)
              d = TextureState.depth;

            // calculate the actual byte size of this mip
            if(isCompressed)
              size = (uint32_t)GetCompressedByteSize(w, h, d, TextureState.internalformat);
            else
              size = (uint32_t)GetByteSize(w, h, d, fmt, type);

            // loop over the number of targets (this will only ever be >1 for cubemaps)
            for(int trg = 0; trg < targetcount; trg++)
            {
              // when writing, fetch the source data out of the texture
              if(ser.IsWriting())
              {
                if(isCompressed)
                {
                  if(IsGLES)
                    details.GetCompressedImageDataGLES(i, targets[trg], size, scratchBuf);
                  else
                    GL.glGetCompressedTextureImageEXT(tex, targets[trg], i, scratchBuf);
                }
                else
                {
                  // we avoid glGetTextureImageEXT as it seems buggy for cubemap faces
                  GL.glGetTexImage(targets[trg], i, fmt, type, scratchBuf);
                }
              }

              // serialise without allocating memory as we already have our scratch buf sized.
              ser.Serialise("SubresourceContents"_lit, scratchBuf, size, SerialiserFlags::NoFlags);

              // on replay, restore the data into the initial contents texture
              if(IsReplayingAndReading() && !ser.IsErrored())
              {
                if(isCompressed)
                {
                  if(IsGLES)
                  {
                    size_t startOffs =
                        IsCubeFace(targets[trg]) ? CubeTargetIndex(targets[trg]) * size : 0;

                    details.compressedData[i].resize(startOffs + size);
                    memcpy(details.compressedData[i].data() + startOffs, scratchBuf, size);
                  }

                  if(TextureState.dim == 1)
                    GL.glCompressedTextureSubImage1DEXT(tex, targets[trg], i, 0, w,
                                                        TextureState.internalformat, (GLsizei)size,
                                                        scratchBuf);
                  else if(TextureState.dim == 2)
                    GL.glCompressedTextureSubImage2DEXT(tex, targets[trg], i, 0, 0, w, h,
                                                        TextureState.internalformat, (GLsizei)size,
                                                        scratchBuf);
                  else if(TextureState.dim == 3)
                    GL.glCompressedTextureSubImage3DEXT(tex, targets[trg], i, 0, 0, 0, w, h, d,
                                                        TextureState.internalformat, (GLsizei)size,
                                                        scratchBuf);
                }
                else
                {
                  if(TextureState.dim == 1)
                    GL.glTextureSubImage1DEXT(tex, targets[trg], i, 0, w, fmt, type, scratchBuf);
                  else if(TextureState.dim == 2)
                    GL.glTextureSubImage2DEXT(tex, targets[trg], i, 0, 0, w, h, fmt, type,
                                              scratchBuf);
                  else if(TextureState.dim == 3)
                    GL.glTextureSubImage3DEXT(tex, targets[trg], i, 0, 0, 0, w, h, d, fmt, type,
                                              scratchBuf);
                }
              }
            }
          }

          // free our scratch buffer
          FreeAlignedBuffer(scratchBuf);
        }

        // restore the previous texture binding
        if(!IsStructuredExporting(m_State) && !ser.IsErrored())
          GL.glBindTexture(TextureState.type, prevtex);

        initContents.resource = TextureRes(m_Driver->GetCtx(), tex);
      }

      if(IsReplayingAndReading() && !ser.IsErrored())
      {
        SetInitialContents(id, initContents);
      }
    }

    // restore pixel (un)packing state
    if(ser.IsWriting() || !IsStructuredExporting(m_State))
    {
      GL.glBindBuffer(eGL_PIXEL_PACK_BUFFER, ppb);
      GL.glBindBuffer(eGL_PIXEL_UNPACK_BUFFER, pub);
      pack.Apply(false);
      unpack.Apply(false);
    }

    SERIALISE_CHECK_READ_ERRORS();
  }
  else if(Type == eResFramebuffer)
  {
    FramebufferInitialData &FramebufferState = initContents.fbo;

    SERIALISE_ELEMENT(FramebufferState);

    SERIALISE_CHECK_READ_ERRORS();

    if(IsReplayingAndReading())
    {
      SetInitialContents(id, initContents);
    }
  }
  else if(Type == eResSampler)
  {
    SamplerInitialData &SamplerState = initContents.samp;

    SERIALISE_ELEMENT(SamplerState);

    SERIALISE_CHECK_READ_ERRORS();

    if(IsReplayingAndReading())
    {
      SetInitialContents(id, initContents);
    }
  }
  else if(Type == eResFeedback)
  {
    FeedbackInitialData &TransformFeedbackState = initContents.xfb;

    SERIALISE_ELEMENT(TransformFeedbackState);

    SERIALISE_CHECK_READ_ERRORS();

    if(IsReplayingAndReading())
    {
      SetInitialContents(id, initContents);
    }
  }
  else if(Type == eResProgramPipe)
  {
    PipelineInitialData &ProgramPipelineState = initContents.pipe;

    SERIALISE_ELEMENT(ProgramPipelineState);

    SERIALISE_CHECK_READ_ERRORS();

    if(IsReplayingAndReading())
    {
      SetInitialContents(id, initContents);
    }
  }
  else if(Type == eResVertexArray)
  {
    VAOInitialData &VAOState = initContents.vao;

    SERIALISE_ELEMENT(VAOState);

    SERIALISE_CHECK_READ_ERRORS();

    if(IsReplayingAndReading())
    {
      SetInitialContents(id, initContents);
    }
  }
  else if(Type == eResRenderbuffer)
  {
    RDCWARN(
        "Technically you could try and readback the contents of a RenderBuffer via pixel copy.");
    RDCWARN("Currently we don't support that though, and initial contents will be uninitialised.");
  }
  else
  {
    RDCERR("Unexpected type of resource requiring initial state");
  }

  return true;
}

template bool GLResourceManager::Serialise_InitialState<>(ReadSerialiser &ser, ResourceId id,
                                                          GLResourceRecord *record,
                                                          const GLInitialContents *initial);
template bool GLResourceManager::Serialise_InitialState<>(WriteSerialiser &ser, ResourceId id,
                                                          GLResourceRecord *record,
                                                          const GLInitialContents *initial);

void GLResourceManager::Create_InitialState(ResourceId id, GLResource live, bool hasData)
{
  if(IsStructuredExporting(m_State))
    return;

  if(live.Namespace == eResTexture)
  {
    // we basically need to do exactly the same as Prepare_InitialState -
    // save current texture state, create a duplicate object, and save
    // the current contents into that duplicate object

    // in future if we skip RT contents for write-before-read RTs, we could mark
    // textures to be cleared instead of copied.
    PrepareTextureInitialContents(GetID(live), id, live);
  }
  else if(live.Namespace == eResBuffer)
  {
    ContextPrepare_InitialState(live);
  }
  else if(live.Namespace == eResVertexArray || live.Namespace == eResFramebuffer ||
          live.Namespace == eResFeedback || live.Namespace == eResSampler ||
          live.Namespace == eResProgramPipe)
  {
    ContextPrepare_InitialState(live);
  }
  else if(live.Namespace == eResRenderbuffer)
  {
  }
  else
  {
    RDCUNIMPLEMENTED("Unhandled type of resource needing initial states created");
  }
}

void GLResourceManager::Apply_InitialState(GLResource live, const GLInitialContents &initial)
{
  if(live.Namespace == eResBuffer)
  {
    // save old bindings
    GLuint oldbuf1 = 0, oldbuf2 = 0;
    GL.glGetIntegerv(eGL_COPY_READ_BUFFER_BINDING, (GLint *)&oldbuf1);
    GL.glGetIntegerv(eGL_COPY_WRITE_BUFFER_BINDING, (GLint *)&oldbuf2);

    // bind the immutable contents for copying
    GL.glBindBuffer(eGL_COPY_READ_BUFFER, initial.resource.name);

    // bind the live buffer for copying
    GL.glBindBuffer(eGL_COPY_WRITE_BUFFER, live.name);

    // do the actual copy
    if(initial.bufferLength > 0)
      GL.glCopyBufferSubData(eGL_COPY_READ_BUFFER, eGL_COPY_WRITE_BUFFER, 0, 0,
                             (GLsizeiptr)initial.bufferLength);

    // restore old bindings
    GL.glBindBuffer(eGL_COPY_READ_BUFFER, oldbuf1);
    GL.glBindBuffer(eGL_COPY_WRITE_BUFFER, oldbuf2);
  }
  else if(live.Namespace == eResTexture)
  {
    ResourceId Id = GetID(live);
    WrappedOpenGL::TextureData &details = m_Driver->m_Textures[Id];

    const TextureStateInitialData &state = initial.tex;

    if(details.curType != eGL_TEXTURE_BUFFER)
    {
      GLuint tex = initial.resource.name;

      bool ms = (details.curType == eGL_TEXTURE_2D_MULTISAMPLE ||
                 details.curType == eGL_TEXTURE_2D_MULTISAMPLE_ARRAY);

      if(initial.resource != GLResource(MakeNullResource) && tex != 0)
      {
        int mips = GetNumMips(details.curType, tex, details.width, details.height, details.depth);

        // we need to set maxlevel appropriately for number of mips to force the texture to be
        // complete. This can happen if e.g. a texture is initialised just by default with
        // glTexImage for level 0 and used as a framebuffer attachment, then the implementation is
        // fine with it.
        // Unfortunately glCopyImageSubData requires completeness across all mips, a stricter
        // requirement :(.
        // We set max_level to mips - 1 (so mips=1 means MAX_LEVEL=0). Then below where we set the
        // texture state, the correct MAX_LEVEL is set to whatever the program had.
        int maxlevel = mips - 1;
        GL.glTextureParameterivEXT(live.name, details.curType, eGL_TEXTURE_MAX_LEVEL,
                                   (GLint *)&maxlevel);

        // set min/mag filters to NEAREST since we are doing an identity copy. Avoids issues where
        // the spec says that e.g. integer or stencil textures cannot have a LINEAR filter
        if(!ms)
        {
          GLenum nearest = eGL_NEAREST;
          GL.glTextureParameterivEXT(live.name, details.curType, eGL_TEXTURE_MIN_FILTER,
                                     (GLint *)&nearest);
          GL.glTextureParameterivEXT(live.name, details.curType, eGL_TEXTURE_MAG_FILTER,
                                     (GLint *)&nearest);
        }

        bool iscomp = IsCompressedFormat(details.internalFormat);

        bool avoidCopySubImage = false;
        if(iscomp && VendorCheck[VendorCheck_AMD_copy_compressed_tinymips])
          avoidCopySubImage = true;
        if(iscomp && details.curType == eGL_TEXTURE_CUBE_MAP &&
           VendorCheck[VendorCheck_AMD_copy_compressed_cubemaps])
          avoidCopySubImage = true;
        if(iscomp && IsGLES)
          avoidCopySubImage = true;

        PixelPackState pack;
        PixelUnpackState unpack;

        if(avoidCopySubImage)
        {
          pack.Fetch(false);
          unpack.Fetch(false);

          ResetPixelPackState(false, 1);
          ResetPixelUnpackState(false, 1);
        }

        // copy over mips
        for(int i = 0; i < mips; i++)
        {
          int w = RDCMAX(details.width >> i, 1);
          int h = RDCMAX(details.height >> i, 1);
          int d = RDCMAX(details.depth >> i, 1);

          if(details.curType == eGL_TEXTURE_CUBE_MAP)
            d *= 6;
          else if(details.curType == eGL_TEXTURE_CUBE_MAP_ARRAY ||
                  details.curType == eGL_TEXTURE_1D_ARRAY || details.curType == eGL_TEXTURE_2D_ARRAY)
            d = details.depth;

          // AMD throws an error copying mips that are smaller than the block size in one dimension,
          // so do copy via CPU instead (will be slow, potentially we could optimise this if there's
          // a different GPU-side image copy routine that works on these dimensions. Hopefully
          // there'll only be a couple of such mips).
          // AMD also has issues copying cubemaps
          if((iscomp && VendorCheck[VendorCheck_AMD_copy_compressed_tinymips] && (w < 4 || h < 4)) ||
             (iscomp && VendorCheck[VendorCheck_AMD_copy_compressed_cubemaps] &&
              details.curType == eGL_TEXTURE_CUBE_MAP) ||
             (iscomp && IsGLES))
          {
            GLenum targets[] = {
                eGL_TEXTURE_CUBE_MAP_POSITIVE_X, eGL_TEXTURE_CUBE_MAP_NEGATIVE_X,
                eGL_TEXTURE_CUBE_MAP_POSITIVE_Y, eGL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
                eGL_TEXTURE_CUBE_MAP_POSITIVE_Z, eGL_TEXTURE_CUBE_MAP_NEGATIVE_Z,
            };

            int count = ARRAY_COUNT(targets);

            if(details.curType != eGL_TEXTURE_CUBE_MAP)
            {
              targets[0] = details.curType;
              count = 1;
            }

            for(int trg = 0; trg < count; trg++)
            {
              size_t size = GetCompressedByteSize(w, h, d, details.internalFormat);

              if(details.curType == eGL_TEXTURE_CUBE_MAP)
                size /= 6;

              byte *buf = new byte[size];

              if(IsGLES)
              {
                details.GetCompressedImageDataGLES(i, targets[trg], size, buf);
              }
              else
              {
                // read to CPU
                GL.glGetCompressedTextureImageEXT(tex, targets[trg], i, buf);
              }

              // write to GPU
              if(details.dimension == 1)
                GL.glCompressedTextureSubImage1DEXT(live.name, targets[trg], i, 0, w,
                                                    details.internalFormat, (GLsizei)size, buf);
              else if(details.dimension == 2)
                GL.glCompressedTextureSubImage2DEXT(live.name, targets[trg], i, 0, 0, w, h,
                                                    details.internalFormat, (GLsizei)size, buf);
              else if(details.dimension == 3)
                GL.glCompressedTextureSubImage3DEXT(live.name, targets[trg], i, 0, 0, 0, w, h, d,
                                                    details.internalFormat, (GLsizei)size, buf);

              delete[] buf;
            }
          }
          else
          {
            // it seems like everything explodes if I do glCopyImageSubData on a D32F_S8 texture -
            // on replay loads of things get heavily corrupted - probably the same as the problems
            // we get in-program, but magnified. It seems like a driver bug, nvidia specific.
            // In most cases a program isn't going to rely on the contents of a depth-stencil buffer
            // (shadow maps that it might require would be depth-only formatted).
            if(details.internalFormat == eGL_DEPTH32F_STENCIL8 &&
               VendorCheck[VendorCheck_NV_avoid_D32S8_copy])
            {
              RDCDEBUG("Not fetching initial contents of D32F_S8 texture");
            }
            else
            {
              if(VendorCheck[VendorCheck_Qualcomm_avoid_glCopyImageSubData])
                glEmulate::_glCopyImageSubData(tex, details.curType, i, 0, 0, 0, live.name,
                                               details.curType, i, 0, 0, 0, w, h, d);
              else
                GL.glCopyImageSubData(tex, details.curType, i, 0, 0, 0, live.name, details.curType,
                                      i, 0, 0, 0, w, h, d);
            }
          }
        }

        if(avoidCopySubImage)
        {
          pack.Apply(false);
          unpack.Apply(false);
        }
      }

      if((state.depthMode == eGL_DEPTH_COMPONENT || state.depthMode == eGL_STENCIL_INDEX) &&
         HasExt[ARB_stencil_texturing])
        GL.glTextureParameterivEXT(live.name, details.curType, eGL_DEPTH_STENCIL_TEXTURE_MODE,
                                   (GLint *)&state.depthMode);

      if((details.curType == eGL_TEXTURE_CUBE_MAP || details.curType == eGL_TEXTURE_CUBE_MAP_ARRAY) &&
         HasExt[ARB_seamless_cubemap_per_texture])
        GL.glTextureParameterivEXT(live.name, details.curType, eGL_TEXTURE_CUBE_MAP_SEAMLESS,
                                   (GLint *)&state.seamless);

      GL.glTextureParameterivEXT(live.name, details.curType, eGL_TEXTURE_BASE_LEVEL,
                                 (GLint *)&state.baseLevel);
      GL.glTextureParameterivEXT(live.name, details.curType, eGL_TEXTURE_MAX_LEVEL,
                                 (GLint *)&state.maxLevel);

      // assume that emulated (luminance, alpha-only etc) textures are not swizzled
      if(!details.emulated && (HasExt[ARB_texture_swizzle] || HasExt[EXT_texture_swizzle]))
      {
        SetTextureSwizzle(live.name, details.curType, state.swizzle);
      }

      if(!ms)
      {
        if(HasExt[EXT_texture_sRGB_decode])
          GL.glTextureParameterivEXT(live.name, details.curType, eGL_TEXTURE_SRGB_DECODE_EXT,
                                     (GLint *)&state.srgbDecode);
        GL.glTextureParameterivEXT(live.name, details.curType, eGL_TEXTURE_COMPARE_FUNC,
                                   (GLint *)&state.compareFunc);
        GL.glTextureParameterivEXT(live.name, details.curType, eGL_TEXTURE_COMPARE_MODE,
                                   (GLint *)&state.compareMode);
        GL.glTextureParameterivEXT(live.name, details.curType, eGL_TEXTURE_MIN_FILTER,
                                   (GLint *)&state.minFilter);
        GL.glTextureParameterivEXT(live.name, details.curType, eGL_TEXTURE_MAG_FILTER,
                                   (GLint *)&state.magFilter);
        GL.glTextureParameterivEXT(live.name, details.curType, eGL_TEXTURE_WRAP_R,
                                   (GLint *)&state.wrap[0]);
        GL.glTextureParameterivEXT(live.name, details.curType, eGL_TEXTURE_WRAP_S,
                                   (GLint *)&state.wrap[1]);
        GL.glTextureParameterivEXT(live.name, details.curType, eGL_TEXTURE_WRAP_T,
                                   (GLint *)&state.wrap[2]);

        // see fetch in PrepareTextureInitialContents
        if(HasExt[ARB_texture_border_clamp])
          GL.glTextureParameterfvEXT(live.name, details.curType, eGL_TEXTURE_BORDER_COLOR,
                                     state.border);

        if(!IsGLES)
          GL.glTextureParameterfvEXT(live.name, details.curType, eGL_TEXTURE_LOD_BIAS,
                                     &state.lodBias);
        if(details.curType != eGL_TEXTURE_RECTANGLE)
        {
          GL.glTextureParameterfvEXT(live.name, details.curType, eGL_TEXTURE_MIN_LOD, &state.minLod);
          GL.glTextureParameterfvEXT(live.name, details.curType, eGL_TEXTURE_MAX_LOD, &state.maxLod);
        }
      }
    }
    else
    {
      GLuint buffer = state.texBuffer.name;

      GLenum fmt = details.internalFormat;

      // update width from here as it's authoratitive - the texture might have been resized in
      // multiple rebinds that we will not have serialised before.
      details.width =
          state.texBufSize / uint32_t(GetByteSize(1, 1, 1, GetBaseFormat(fmt), GetDataType(fmt)));

      if(GL.glTextureBufferRangeEXT)
      {
        // restore texbuffer only state
        GL.glTextureBufferRangeEXT(live.name, eGL_TEXTURE_BUFFER, details.internalFormat, buffer,
                                   state.texBufOffs, state.texBufSize);
      }
      else
      {
        uint32_t bufSize = 0;
        GL.glGetNamedBufferParameterivEXT(buffer, eGL_BUFFER_SIZE, (GLint *)&bufSize);
        if(state.texBufOffs > 0 || state.texBufSize > bufSize)
        {
          const char *msg =
              "glTextureBufferRangeEXT is not supported on your GL implementation, but is needed "
              "for correct replay.\n"
              "The original capture created a texture buffer with a range - replay will use the "
              "whole buffer, which is likely incorrect.";
          RDCERR("%s", msg);
          m_Driver->AddDebugMessage(MessageCategory::Resource_Manipulation, MessageSeverity::High,
                                    MessageSource::IncorrectAPIUse, msg);
        }

        GL.glTextureBufferEXT(live.name, eGL_TEXTURE_BUFFER, details.internalFormat, buffer);
      }
    }
  }
  else if(live.Namespace == eResProgram)
  {
    ResourceId Id = GetID(live);

    const WrappedOpenGL::ProgramData &prog = m_Driver->m_Programs[Id];

    bool changedBindings = false;

    if(prog.stageShaders[0] != ResourceId())
      changedBindings |= CopyProgramAttribBindings(
          initial.resource.name, live.name, &m_Driver->m_Shaders[prog.stageShaders[0]].reflection);

    if(prog.stageShaders[4] != ResourceId())
      changedBindings |= CopyProgramFragDataBindings(
          initial.resource.name, live.name, &m_Driver->m_Shaders[prog.stageShaders[4]].reflection);

    // we need to re-link the program to apply the bindings, as long as it's linkable.
    // See the comment on shaderProgramUnlinkable for more information.
    if(!prog.shaderProgramUnlinkable && changedBindings)
      GL.glLinkProgram(live.name);

    PerStageReflections stages;
    m_Driver->FillReflectionArray(Id, stages);

    // we can pass in the same stages array, it's the same program essentially (reflection is
    // identical)
    CopyProgramUniforms(stages, initial.resource.name, stages, live.name);
  }
  else if(live.Namespace == eResFramebuffer)
  {
    const FramebufferInitialData &data = initial.fbo;

    if(data.valid)
    {
      GLuint prevread = 0, prevdraw = 0;
      GL.glGetIntegerv(eGL_DRAW_FRAMEBUFFER_BINDING, (GLint *)&prevdraw);
      GL.glGetIntegerv(eGL_READ_FRAMEBUFFER_BINDING, (GLint *)&prevread);

      GL.glBindFramebuffer(eGL_DRAW_FRAMEBUFFER, live.name);
      GL.glBindFramebuffer(eGL_READ_FRAMEBUFFER, live.name);

      GLint numCols = 8;
      GL.glGetIntegerv(eGL_MAX_COLOR_ATTACHMENTS, &numCols);

      for(int i = 0; i < (int)ARRAY_COUNT(data.Attachments); i++)
      {
        const FramebufferAttachmentData &a = data.Attachments[i];
        GLenum attachment = FramebufferInitialData::attachmentNames[i];

        if(attachment != eGL_DEPTH_ATTACHMENT && attachment != eGL_STENCIL_ATTACHMENT &&
           attachment != eGL_DEPTH_STENCIL_ATTACHMENT)
        {
          // color attachment
          int attachNum = attachment - eGL_COLOR_ATTACHMENT0;
          if(attachNum >= numCols)    // attachment is invalid on this device
            continue;
        }

        GLuint obj = a.obj.name;

        if(a.obj.Namespace == eResRenderbuffer && obj)
        {
          GL.glNamedFramebufferRenderbufferEXT(live.name, attachment, eGL_RENDERBUFFER, obj);
        }
        else
        {
          if(!a.layered && obj)
          {
            // we use old-style non-DSA for this because binding cubemap faces with EXT_dsa
            // is completely messed up and broken

            // if obj is a cubemap use face-specific targets
            WrappedOpenGL::TextureData &details = m_Driver->m_Textures[GetID(a.obj)];

            if(details.curType == eGL_TEXTURE_CUBE_MAP)
            {
              GLenum faces[] = {
                  eGL_TEXTURE_CUBE_MAP_POSITIVE_X, eGL_TEXTURE_CUBE_MAP_NEGATIVE_X,
                  eGL_TEXTURE_CUBE_MAP_POSITIVE_Y, eGL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
                  eGL_TEXTURE_CUBE_MAP_POSITIVE_Z, eGL_TEXTURE_CUBE_MAP_NEGATIVE_Z,
              };

              if(a.layer < 6)
              {
                GL.glFramebufferTexture2D(eGL_DRAW_FRAMEBUFFER, attachment, faces[a.layer], obj,
                                          a.level);
              }
              else
              {
                RDCWARN("Invalid layer %u used to bind cubemap to framebuffer. Binding POSITIVE_X");
                GL.glFramebufferTexture2D(eGL_DRAW_FRAMEBUFFER, attachment, faces[0], obj, a.level);
              }
            }
            else if(details.curType == eGL_TEXTURE_CUBE_MAP_ARRAY ||
                    details.curType == eGL_TEXTURE_1D_ARRAY ||
                    details.curType == eGL_TEXTURE_2D_ARRAY)
            {
              if(a.numViews > 1)
              {
                if(a.numVirtualSamples > 1)
                {
                  GL.glFramebufferTextureMultisampleMultiviewOVR(eGL_DRAW_FRAMEBUFFER, attachment,
                                                                 obj, a.level, a.numVirtualSamples,
                                                                 a.startView, a.numViews);
                }
                else
                {
                  GL.glFramebufferTextureMultiviewOVR(eGL_DRAW_FRAMEBUFFER, attachment, obj,
                                                      a.level, a.startView, a.numViews);
                }
              }
              else
              {
                GL.glFramebufferTextureLayer(eGL_DRAW_FRAMEBUFFER, attachment, obj, a.level, a.layer);
              }
            }
            else if(a.numVirtualSamples > 1)
            {
              GL.glFramebufferTexture2DMultisampleEXT(eGL_DRAW_FRAMEBUFFER, attachment,
                                                      details.curType, obj, a.level,
                                                      a.numVirtualSamples);
            }
            else
            {
              RDCASSERT(a.layer == 0);
              GL.glNamedFramebufferTextureEXT(live.name, attachment, obj, a.level);
            }
          }
          else
          {
            GL.glNamedFramebufferTextureEXT(live.name, attachment, obj, a.level);
          }
        }
      }

      GLenum drawbuffers[8];
      memcpy(drawbuffers, data.DrawBuffers, sizeof(drawbuffers));
      RDCCOMPILE_ASSERT(sizeof(drawbuffers) == sizeof(data.DrawBuffers),
                        "Update drawbuffers array");

      // set invalid caps to GL_COLOR_ATTACHMENT0
      for(int i = 0; i < (int)ARRAY_COUNT(drawbuffers); i++)
        if(drawbuffers[i] == eGL_BACK || drawbuffers[i] == eGL_FRONT)
          drawbuffers[i] = eGL_COLOR_ATTACHMENT0;

      GLenum readbuffer = data.ReadBuffer;
      if(readbuffer == eGL_BACK || readbuffer == eGL_FRONT)
        readbuffer = eGL_COLOR_ATTACHMENT0;

      GLuint maxDraws = 0;
      GL.glGetIntegerv(eGL_MAX_DRAW_BUFFERS, (GLint *)&maxDraws);

      GL.glDrawBuffers(RDCMIN(maxDraws, (GLuint)ARRAY_COUNT(drawbuffers)), drawbuffers);

      GL.glReadBuffer(readbuffer);

      GL.glBindFramebuffer(eGL_DRAW_FRAMEBUFFER, prevdraw);
      GL.glBindFramebuffer(eGL_READ_FRAMEBUFFER, prevread);
    }
  }
  else if(live.Namespace == eResSampler)
  {
    const SamplerInitialData &data = initial.samp;

    if(data.valid)
    {
      GLenum activeTexture = eGL_TEXTURE0;
      GL.glGetIntegerv(eGL_ACTIVE_TEXTURE, (GLint *)&activeTexture);

      GL.glActiveTexture(eGL_TEXTURE0);

      GLuint prevsampler = 0;
      GL.glGetIntegerv(eGL_SAMPLER_BINDING, (GLint *)&prevsampler);

      {
        GL.glSamplerParameteri(live.name, eGL_TEXTURE_COMPARE_FUNC, (GLint)data.compareFunc);
        GL.glSamplerParameteri(live.name, eGL_TEXTURE_COMPARE_MODE, (GLint)data.compareMode);
        GL.glSamplerParameteri(live.name, eGL_TEXTURE_MIN_FILTER, (GLint)data.minFilter);
        GL.glSamplerParameteri(live.name, eGL_TEXTURE_MAG_FILTER, (GLint)data.magFilter);
        GL.glSamplerParameteri(live.name, eGL_TEXTURE_WRAP_R, (GLint)data.wrap[0]);
        GL.glSamplerParameteri(live.name, eGL_TEXTURE_WRAP_S, (GLint)data.wrap[1]);
        GL.glSamplerParameteri(live.name, eGL_TEXTURE_WRAP_T, (GLint)data.wrap[2]);
        GL.glSamplerParameterf(live.name, eGL_TEXTURE_MIN_LOD, data.minLod);
        GL.glSamplerParameterf(live.name, eGL_TEXTURE_MAX_LOD, data.maxLod);
        if(!IsGLES)
          GL.glSamplerParameterf(live.name, eGL_TEXTURE_LOD_BIAS, data.lodBias);

        // see fetch in PrepareTextureInitialContents
        if(HasExt[ARB_texture_border_clamp])
          GL.glSamplerParameterfv(live.name, eGL_TEXTURE_BORDER_COLOR, &data.border[0]);
      }

      GL.glBindSampler(0, prevsampler);

      GL.glActiveTexture(activeTexture);
    }
  }
  else if(live.Namespace == eResFeedback)
  {
    const FeedbackInitialData &data = initial.xfb;

    if(data.valid)
    {
      GLuint prevfeedback = 0;
      GL.glGetIntegerv(eGL_TRANSFORM_FEEDBACK_BINDING, (GLint *)&prevfeedback);

      GL.glBindTransformFeedback(eGL_TRANSFORM_FEEDBACK, live.name);

      GLint maxCount = 0;
      GL.glGetIntegerv(eGL_MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS, &maxCount);

      for(int i = 0; i < (int)ARRAY_COUNT(data.Buffer) && i < maxCount; i++)
        GL.glBindBufferRange(eGL_TRANSFORM_FEEDBACK_BUFFER, i, data.Buffer[i].name,
                             (GLintptr)data.Offset[i], (GLsizei)data.Size[i]);

      GL.glBindTransformFeedback(eGL_TRANSFORM_FEEDBACK, prevfeedback);
    }
  }
  else if(live.Namespace == eResProgramPipe)
  {
    const PipelineInitialData &data = initial.pipe;

    if(data.valid)
    {
      // we need to bind the same program to all relevant stages at once. So since there's only 5
      // stages to worry about (compute can't be shared) we just do an O(N^2) search
      for(int a = 0; a < 5; a++)
      {
        // ignore any empty binds
        if(data.programs[a].name == 0)
          continue;

        // this bit has a program. First search backwards to see if it was already bound previously.
        bool previous = false;
        for(int b = 0; b < a; b++)
          if(data.programs[a].name == data.programs[b].name)
            previous = true;

        // if we found a match behind us, that means we already bound this program back then -
        // continue
        if(previous)
          continue;

        // now build up the bitmask that we'll bind with. Starting with the current bit, searching
        // forwards
        GLbitfield stages = ShaderBit(a);
        for(int b = a + 1; b < 5; b++)
          if(data.programs[a].name == data.programs[b].name)
            stages |= ShaderBit(b);

        // bind the program on all relevant stages
        m_Driver->glUseProgramStages(live.name, stages, data.programs[a].name);

        // now we can continue - any of the stages we just bound will discard themselves with the
        // 'previous' check above.
      }

      // if we have a compute program, bind that. It's outside of the others since it can't be
      // shared
      if(data.programs[5].name)
        m_Driver->glUseProgramStages(live.name, eGL_COMPUTE_SHADER_BIT, data.programs[5].name);
    }
  }
  else if(live.Namespace == eResVertexArray)
  {
    const VAOInitialData &data = initial.vao;

    if(data.valid)
    {
      GLuint VAO = 0;
      GL.glGetIntegerv(eGL_VERTEX_ARRAY_BINDING, (GLint *)&VAO);

      GL.glBindVertexArray(live.name);

      for(GLuint i = 0; i < 16; i++)
      {
        const VertexAttribInitialData &attrib = data.VertexAttribs[i];

        if(attrib.enabled)
          GL.glEnableVertexAttribArray(i);
        else
          GL.glDisableVertexAttribArray(i);

        GL.glVertexAttribBinding(i, attrib.vbslot);

        if(attrib.size != 0)
        {
          if(attrib.type == eGL_DOUBLE)
            GL.glVertexAttribLFormat(i, attrib.size, attrib.type, attrib.offset);
          else if(attrib.integer == 0)
            GL.glVertexAttribFormat(i, attrib.size, attrib.type, (GLboolean)attrib.normalized,
                                    attrib.offset);
          else
            GL.glVertexAttribIFormat(i, attrib.size, attrib.type, attrib.offset);
        }

        const VertexBufferInitialData &buf = data.VertexBuffers[i];

        GL.glBindVertexBuffer(i, buf.Buffer.name, (GLintptr)buf.Offset, (GLsizei)buf.Stride);
        GL.glVertexBindingDivisor(i, buf.Divisor);
      }

      GLuint buffer = data.ElementArrayBuffer.name;
      GL.glBindBuffer(eGL_ELEMENT_ARRAY_BUFFER, buffer);

      GL.glBindVertexArray(VAO);
    }
  }
  else if(live.Namespace == eResRenderbuffer)
  {
  }
  else
  {
    RDCERR("Unexpected type of resource requiring initial state");
  }
}
