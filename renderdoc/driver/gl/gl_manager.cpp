/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2017 Baldur Karlsson
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

#include "driver/gl/gl_manager.h"
#include "driver/gl/gl_driver.h"

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

struct VertexBufferInitialData
{
  ResourceId Buffer;
  uint64_t Stride;
  uint64_t Offset;
  uint32_t Divisor;
};

// note these data structures below contain a 'valid' bool, since due to complexities of
// fetching the state on the right context, we might never be able to fetch the data at
// all. So the valid is set to false to indicate that we shouldn't try to restore it on
// replay.
struct VAOInitialData
{
  bool valid;
  VertexAttribInitialData VertexAttribs[16];
  VertexBufferInitialData VertexBuffers[16];
  ResourceId ElementArrayBuffer;
};

struct FeedbackInitialData
{
  bool valid;
  ResourceId Buffer[4];
  uint64_t Offset[4];
  uint64_t Size[4];
};

struct FramebufferAttachmentData
{
  bool renderbuffer;
  bool layered;
  int32_t layer;
  int32_t level;
  ResourceId obj;
};

struct FramebufferInitialData
{
  bool valid;
  FramebufferAttachmentData Attachments[10];
  GLenum DrawBuffers[8];
  GLenum ReadBuffer;

  static const GLenum attachmentNames[10];
};

const GLenum FramebufferInitialData::attachmentNames[10] = {
    eGL_COLOR_ATTACHMENT0, eGL_COLOR_ATTACHMENT1,  eGL_COLOR_ATTACHMENT2, eGL_COLOR_ATTACHMENT3,
    eGL_COLOR_ATTACHMENT4, eGL_COLOR_ATTACHMENT5,  eGL_COLOR_ATTACHMENT6, eGL_COLOR_ATTACHMENT7,
    eGL_DEPTH_ATTACHMENT,  eGL_STENCIL_ATTACHMENT,
};

template <>
void Serialiser::Serialise(const char *name, VertexAttribInitialData &el)
{
  ScopedContext scope(this, name, "VertexArrayInitialData", 0, true);
  Serialise("enabled", el.enabled);
  Serialise("vbslot", el.vbslot);
  Serialise("offset", el.offset);
  Serialise("type", el.type);
  Serialise("normalized", el.normalized);
  Serialise("integer", el.integer);
  Serialise("size", el.size);
}

template <>
void Serialiser::Serialise(const char *name, VertexBufferInitialData &el)
{
  ScopedContext scope(this, name, "VertexBufferInitialData", 0, true);
  Serialise("Buffer", el.Buffer);
  Serialise("Stride", el.Stride);
  Serialise("Offset", el.Offset);
  Serialise("Divisor", el.Divisor);
}

template <>
void Serialiser::Serialise(const char *name, FeedbackInitialData &el)
{
  ScopedContext scope(this, name, "FeedbackInitialData", 0, true);
  Serialise("valid", el.valid);
  SerialisePODArray<4>("Buffer", el.Buffer);
  SerialisePODArray<4>("Offset", el.Offset);
  SerialisePODArray<4>("Size", el.Size);
}

template <>
void Serialiser::Serialise(const char *name, FramebufferAttachmentData &el)
{
  ScopedContext scope(this, name, "FramebufferAttachmentData", 0, true);
  Serialise("renderbuffer", el.renderbuffer);
  Serialise("layered", el.layered);
  Serialise("layer", el.layer);
  Serialise("level", el.level);
  Serialise("obj", el.obj);
}

template <>
void Serialiser::Serialise(const char *name, FramebufferInitialData &el)
{
  ScopedContext scope(this, name, "FramebufferInitialData", 0, true);
  Serialise("valid", el.valid);
  SerialisePODArray<8>("DrawBuffers", el.DrawBuffers);
  for(size_t i = 0; i < ARRAY_COUNT(el.Attachments); i++)
    Serialise("Attachments", el.Attachments[i]);
  Serialise("ReadBuffer", el.ReadBuffer);
}

struct TextureStateInitialData
{
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
  ResourceId texBuffer;
  uint32_t texBufOffs;
  uint32_t texBufSize;
};

template <>
void Serialiser::Serialise(const char *name, TextureStateInitialData &el)
{
  ScopedContext scope(this, name, "TextureStateInitialData", 0, true);
  Serialise("baseLevel", el.baseLevel);
  Serialise("maxLevel", el.maxLevel);
  Serialise("minLod", el.minLod);
  Serialise("maxLod", el.maxLod);
  Serialise("srgbDecode", el.srgbDecode);
  Serialise("depthMode", el.depthMode);
  Serialise("compareFunc", el.compareFunc);
  Serialise("compareMode", el.compareMode);
  Serialise("seamless", el.seamless);
  Serialise("minFilter", el.minFilter);
  Serialise("magFilter", el.magFilter);
  SerialisePODArray<4>("swizzle", el.swizzle);
  SerialisePODArray<3>("wrap", el.wrap);
  SerialisePODArray<4>("border", el.border);
  Serialise("lodBias", el.lodBias);
  Serialise("texBuffer", el.texBuffer);
  Serialise("texBufOffs", el.texBufOffs);
  Serialise("texBufSize", el.texBufSize);
}

void GLResourceManager::MarkVAOReferenced(GLResource res, FrameRefType ref, bool allowFake0)
{
  const GLHookSet &gl = m_GL->GetHookset();

  if(res.name || allowFake0)
  {
    MarkResourceFrameReferenced(res, ref == eFrameRef_Unknown ? eFrameRef_Unknown : eFrameRef_Read);

    GLint numVBufferBindings = 16;
    gl.glGetIntegerv(eGL_MAX_VERTEX_ATTRIB_BINDINGS, &numVBufferBindings);

    for(GLuint i = 0; i < (GLuint)numVBufferBindings; i++)
    {
      GLuint buffer = GetBoundVertexBuffer(gl, i);

      MarkResourceFrameReferenced(BufferRes(res.Context, buffer), ref);
    }

    GLuint ibuffer = 0;
    gl.glGetIntegerv(eGL_ELEMENT_ARRAY_BUFFER_BINDING, (GLint *)&ibuffer);
    MarkResourceFrameReferenced(BufferRes(res.Context, ibuffer), ref);
  }
}

void GLResourceManager::MarkFBOReferenced(GLResource res, FrameRefType ref)
{
  if(res.name == 0)
    return;

  MarkResourceFrameReferenced(res, ref == eFrameRef_Unknown ? eFrameRef_Unknown : eFrameRef_Read);

  const GLHookSet &gl = m_GL->GetHookset();

  GLint numCols = 8;
  gl.glGetIntegerv(eGL_MAX_COLOR_ATTACHMENTS, &numCols);

  GLenum type = eGL_TEXTURE;
  GLuint name = 0;

  for(int c = 0; c < numCols; c++)
  {
    gl.glGetNamedFramebufferAttachmentParameterivEXT(res.name, GLenum(eGL_COLOR_ATTACHMENT0 + c),
                                                     eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME,
                                                     (GLint *)&name);
    gl.glGetNamedFramebufferAttachmentParameterivEXT(res.name, GLenum(eGL_COLOR_ATTACHMENT0 + c),
                                                     eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE,
                                                     (GLint *)&type);

    if(type == eGL_RENDERBUFFER)
      MarkResourceFrameReferenced(RenderbufferRes(res.Context, name), ref);
    else
      MarkResourceFrameReferenced(TextureRes(res.Context, name), ref);
  }

  gl.glGetNamedFramebufferAttachmentParameterivEXT(
      res.name, eGL_DEPTH_ATTACHMENT, eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, (GLint *)&name);
  gl.glGetNamedFramebufferAttachmentParameterivEXT(
      res.name, eGL_DEPTH_ATTACHMENT, eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, (GLint *)&type);

  if(name)
  {
    if(type == eGL_RENDERBUFFER)
      MarkResourceFrameReferenced(RenderbufferRes(res.Context, name), ref);
    else
      MarkResourceFrameReferenced(TextureRes(res.Context, name), ref);
  }

  gl.glGetNamedFramebufferAttachmentParameterivEXT(
      res.name, eGL_STENCIL_ATTACHMENT, eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, (GLint *)&name);
  gl.glGetNamedFramebufferAttachmentParameterivEXT(
      res.name, eGL_STENCIL_ATTACHMENT, eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, (GLint *)&type);

  if(name)
  {
    if(type == eGL_RENDERBUFFER)
      MarkResourceFrameReferenced(RenderbufferRes(res.Context, name), ref);
    else
      MarkResourceFrameReferenced(TextureRes(res.Context, name), ref);
  }
}

bool GLResourceManager::SerialisableResource(ResourceId id, GLResourceRecord *record)
{
  if(id == m_GL->GetContextResourceID())
    return false;
  return true;
}

bool GLResourceManager::Need_InitialStateChunk(GLResource res)
{
  return true;
}

bool GLResourceManager::Prepare_InitialState(GLResource res, byte *blob)
{
  const GLHookSet &gl = m_GL->GetHookset();

  if(res.Namespace == eResFramebuffer)
  {
    FramebufferInitialData *data = (FramebufferInitialData *)blob;

    data->valid = true;

    GLuint prevread = 0, prevdraw = 0;
    gl.glGetIntegerv(eGL_DRAW_FRAMEBUFFER_BINDING, (GLint *)&prevdraw);
    gl.glGetIntegerv(eGL_READ_FRAMEBUFFER_BINDING, (GLint *)&prevread);

    gl.glBindFramebuffer(eGL_DRAW_FRAMEBUFFER, res.name);
    gl.glBindFramebuffer(eGL_READ_FRAMEBUFFER, res.name);

    // need to serialise out which objects are bound
    GLenum type = eGL_TEXTURE;
    GLuint object = 0;
    GLint layered = 0;
    for(int i = 0; i < (int)ARRAY_COUNT(data->Attachments); i++)
    {
      FramebufferAttachmentData &a = data->Attachments[i];
      GLenum attachment = FramebufferInitialData::attachmentNames[i];

      gl.glGetNamedFramebufferAttachmentParameterivEXT(
          res.name, attachment, eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, (GLint *)&object);
      gl.glGetNamedFramebufferAttachmentParameterivEXT(
          res.name, attachment, eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, (GLint *)&type);

      a.renderbuffer = (type == eGL_RENDERBUFFER);

      layered = 0;
      a.level = 0;
      a.layer = 0;

      if(object && !a.renderbuffer)
      {
        gl.glGetNamedFramebufferAttachmentParameterivEXT(
            res.name, attachment, eGL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL, &a.level);
        gl.glGetNamedFramebufferAttachmentParameterivEXT(
            res.name, attachment, eGL_FRAMEBUFFER_ATTACHMENT_LAYERED, &layered);

        if(layered == 0)
          gl.glGetNamedFramebufferAttachmentParameterivEXT(
              res.name, attachment, eGL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LAYER, &a.layer);
      }

      a.layered = (layered != 0);
      a.obj = GetID(a.renderbuffer ? RenderbufferRes(res.Context, object)
                                   : TextureRes(res.Context, object));

      if(!a.renderbuffer)
      {
        WrappedOpenGL::TextureData &details = m_GL->m_Textures[a.obj];

        if(details.curType == eGL_TEXTURE_CUBE_MAP)
        {
          GLenum face;
          gl.glGetNamedFramebufferAttachmentParameterivEXT(
              res.name, attachment, eGL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE, (GLint *)&face);

          a.layer = CubeTargetIndex(face);
        }
      }
    }

    for(int i = 0; i < (int)ARRAY_COUNT(data->DrawBuffers); i++)
      gl.glGetIntegerv(GLenum(eGL_DRAW_BUFFER0 + i), (GLint *)&data->DrawBuffers[i]);

    gl.glGetIntegerv(eGL_READ_BUFFER, (GLint *)&data->ReadBuffer);

    gl.glBindFramebuffer(eGL_DRAW_FRAMEBUFFER, prevdraw);
    gl.glBindFramebuffer(eGL_READ_FRAMEBUFFER, prevread);
  }
  else if(res.Namespace == eResFeedback)
  {
    FeedbackInitialData *data = (FeedbackInitialData *)blob;

    data->valid = true;

    GLuint prevfeedback = 0;
    gl.glGetIntegerv(eGL_TRANSFORM_FEEDBACK_BINDING, (GLint *)&prevfeedback);

    gl.glBindTransformFeedback(eGL_TRANSFORM_FEEDBACK, res.name);

    GLint maxCount = 0;
    gl.glGetIntegerv(eGL_MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS, &maxCount);

    for(int i = 0; i < (int)ARRAY_COUNT(data->Buffer) && i < maxCount; i++)
    {
      GLuint buffer = 0;
      gl.glGetIntegeri_v(eGL_TRANSFORM_FEEDBACK_BUFFER_BINDING, i, (GLint *)&buffer);
      data->Buffer[i] = GetID(BufferRes(res.Context, buffer));
      gl.glGetInteger64i_v(eGL_TRANSFORM_FEEDBACK_BUFFER_START, i, (GLint64 *)&data->Offset[i]);
      gl.glGetInteger64i_v(eGL_TRANSFORM_FEEDBACK_BUFFER_SIZE, i, (GLint64 *)&data->Size[i]);
    }

    gl.glBindTransformFeedback(eGL_TRANSFORM_FEEDBACK, prevfeedback);
  }
  else if(res.Namespace == eResVertexArray)
  {
    VAOInitialData *data = (VAOInitialData *)blob;

    data->valid = true;

    GLuint prevVAO = 0;
    gl.glGetIntegerv(eGL_VERTEX_ARRAY_BINDING, (GLint *)&prevVAO);

    if(res.name == 0)
      gl.glBindVertexArray(m_GL->GetFakeVAO());
    else
      gl.glBindVertexArray(res.name);

    for(GLuint i = 0; i < 16; i++)
    {
      gl.glGetVertexAttribiv(i, eGL_VERTEX_ATTRIB_ARRAY_ENABLED,
                             (GLint *)&data->VertexAttribs[i].enabled);
      gl.glGetVertexAttribiv(i, eGL_VERTEX_ATTRIB_BINDING, (GLint *)&data->VertexAttribs[i].vbslot);
      gl.glGetVertexAttribiv(i, eGL_VERTEX_ATTRIB_RELATIVE_OFFSET,
                             (GLint *)&data->VertexAttribs[i].offset);
      gl.glGetVertexAttribiv(i, eGL_VERTEX_ATTRIB_ARRAY_TYPE, (GLint *)&data->VertexAttribs[i].type);
      gl.glGetVertexAttribiv(i, eGL_VERTEX_ATTRIB_ARRAY_NORMALIZED,
                             (GLint *)&data->VertexAttribs[i].normalized);
      gl.glGetVertexAttribiv(i, eGL_VERTEX_ATTRIB_ARRAY_INTEGER,
                             (GLint *)&data->VertexAttribs[i].integer);
      gl.glGetVertexAttribiv(i, eGL_VERTEX_ATTRIB_ARRAY_SIZE, (GLint *)&data->VertexAttribs[i].size);

      GLuint buffer = GetBoundVertexBuffer(gl, i);

      data->VertexBuffers[i].Buffer = GetID(BufferRes(res.Context, buffer));

      gl.glGetIntegeri_v(eGL_VERTEX_BINDING_STRIDE, i, (GLint *)&data->VertexBuffers[i].Stride);
      gl.glGetIntegeri_v(eGL_VERTEX_BINDING_OFFSET, i, (GLint *)&data->VertexBuffers[i].Offset);
      gl.glGetIntegeri_v(eGL_VERTEX_BINDING_DIVISOR, i, (GLint *)&data->VertexBuffers[i].Divisor);
    }

    GLuint buffer = 0;
    gl.glGetIntegerv(eGL_ELEMENT_ARRAY_BUFFER_BINDING, (GLint *)&buffer);
    data->ElementArrayBuffer = GetID(BufferRes(res.Context, buffer));

    gl.glBindVertexArray(prevVAO);
  }

  return true;
}

bool GLResourceManager::Prepare_InitialState(GLResource res)
{
  // this function needs to be refactored to better deal with multiple
  // contexts and resources that are specific to a particular context

  ResourceId Id = GetID(res);

  const GLHookSet &gl = m_GL->GetHookset();

  if(res.Namespace == eResBuffer)
  {
    // get the length of the buffer
    uint32_t length = 1;
    gl.glGetNamedBufferParameterivEXT(res.name, eGL_BUFFER_SIZE, (GLint *)&length);

    // save old bindings
    GLuint oldbuf1 = 0, oldbuf2 = 0;
    gl.glGetIntegerv(eGL_COPY_READ_BUFFER_BINDING, (GLint *)&oldbuf1);
    gl.glGetIntegerv(eGL_COPY_WRITE_BUFFER_BINDING, (GLint *)&oldbuf2);

    // create a new buffer big enough to hold the contents
    GLuint buf = 0;
    gl.glGenBuffers(1, &buf);
    gl.glBindBuffer(eGL_COPY_WRITE_BUFFER, buf);
    gl.glNamedBufferDataEXT(buf, (GLsizeiptr)length, NULL, eGL_STATIC_READ);

    // bind the live buffer for copying
    gl.glBindBuffer(eGL_COPY_READ_BUFFER, res.name);

    // do the actual copy
    gl.glCopyBufferSubData(eGL_COPY_READ_BUFFER, eGL_COPY_WRITE_BUFFER, 0, 0, (GLsizeiptr)length);

    // restore old bindings
    gl.glBindBuffer(eGL_COPY_READ_BUFFER, oldbuf1);
    gl.glBindBuffer(eGL_COPY_WRITE_BUFFER, oldbuf2);

    SetInitialContents(Id, InitialContentData(BufferRes(res.Context, buf), length, NULL));
  }
  else if(res.Namespace == eResProgram)
  {
    ScopedContext scope(m_pSerialiser, "Initial Contents", "Initial Contents", INITIAL_CONTENTS,
                        false);

    m_pSerialiser->Serialise("Id", Id);

    SerialiseProgramBindings(gl, m_pSerialiser, res.name, true);

    SerialiseProgramUniforms(gl, m_pSerialiser, res.name, NULL, true);

    SetInitialChunk(Id, scope.Get());
  }
  else if(res.Namespace == eResTexture)
  {
    PrepareTextureInitialContents(Id, Id, res);
  }
  else if(res.Namespace == eResFramebuffer)
  {
    byte *data = Serialiser::AllocAlignedBuffer(sizeof(FramebufferInitialData));
    RDCEraseMem(data, sizeof(FramebufferInitialData));

    SetInitialContents(Id, InitialContentData(GLResource(MakeNullResource), 0, data));

    // if FBOs aren't shared we need to fetch the data for this FBO on the right context. It's
    // not safe for us to go changing contexts ourselves (the context could be active on another
    // thread), so instead we'll queue this up to fetch when we are on the correct context.
    //
    // Because we've already allocated and set the blob above, it can be filled in any time
    // before serialising (end of the frame, and if the context is never used before the end of
    // the frame the resource can't be used, so not fetching the initial state doesn't matter).
    //
    // Note we also need to detect the case where the context is already current on another thread
    // and we just start getting commands there, but that case already isn't supported as we don't
    // detect it and insert state-change chunks, we assume all commands will come from a single
    // thread.
    if(!VendorCheck[VendorCheck_EXT_fbo_shared] && res.Context && m_GL->GetCtx() != res.Context)
    {
      m_GL->QueuePrepareInitialState(res, data);
    }
    else
    {
      // call immediately, we are on the right context or for one reason or another the context
      // doesn't matter for fetching this resource (res.Context is NULL or vendorcheck means they're
      // shared).
      Prepare_InitialState(res, (byte *)data);
    }
  }
  else if(res.Namespace == eResFeedback)
  {
    byte *data = Serialiser::AllocAlignedBuffer(sizeof(FeedbackInitialData));
    RDCEraseMem(data, sizeof(FeedbackInitialData));

    SetInitialContents(Id, InitialContentData(GLResource(MakeNullResource), 0, data));

    // queue initial state fetching if we're not on the right context, see above in FBOs for more
    // explanation of this.
    if(res.Context && m_GL->GetCtx() != res.Context)
    {
      m_GL->QueuePrepareInitialState(res, data);
    }
    else
    {
      Prepare_InitialState(res, (byte *)data);
    }
  }
  else if(res.Namespace == eResVertexArray)
  {
    byte *data = Serialiser::AllocAlignedBuffer(sizeof(VAOInitialData));
    RDCEraseMem(data, sizeof(VAOInitialData));

    SetInitialContents(Id, InitialContentData(GLResource(MakeNullResource), 0, data));

    // queue initial state fetching if we're not on the right context, see above in FBOs for more
    // explanation of this.
    if(res.Context && m_GL->GetCtx() != res.Context)
    {
      m_GL->QueuePrepareInitialState(res, data);
    }
    else
    {
      Prepare_InitialState(res, (byte *)data);
    }
  }
  else if(res.Namespace == eResRenderbuffer)
  {
    //
  }
  else
  {
    RDCERR("Unexpected type of resource requiring initial state");
  }

  return true;
}

void GLResourceManager::CreateTextureImage(GLuint tex, GLenum internalFormat, GLenum textype,
                                           GLint dim, GLint width, GLint height, GLint depth,
                                           GLint samples, int mips)
{
  const GLHookSet &gl = m_GL->GetHookset();

  if(textype == eGL_TEXTURE_BUFFER)
  {
    return;
  }
  else if(textype == eGL_TEXTURE_2D_MULTISAMPLE)
  {
    gl.glTextureStorage2DMultisampleEXT(tex, textype, samples, internalFormat, width, height,
                                        GL_TRUE);
  }
  else if(textype == eGL_TEXTURE_2D_MULTISAMPLE_ARRAY)
  {
    gl.glTextureStorage3DMultisampleEXT(tex, textype, samples, internalFormat, width, height, depth,
                                        GL_TRUE);
  }
  else
  {
    gl.glTextureParameteriEXT(tex, textype, eGL_TEXTURE_MAX_LEVEL, mips - 1);

    bool isCompressed = IsCompressedFormat(internalFormat);

    GLenum baseFormat = eGL_RGBA;
    GLenum dataType = eGL_UNSIGNED_BYTE;
    if(!isCompressed)
    {
      baseFormat = GetBaseFormat(internalFormat);
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

          vector<byte> dummy;
          dummy.resize(compSize);

          if(dim == 1)
            gl.glCompressedTextureImage1DEXT(tex, targets[t], m, internalFormat, w, 0, compSize,
                                             &dummy[0]);
          else if(dim == 2)
            gl.glCompressedTextureImage2DEXT(tex, targets[t], m, internalFormat, w, h, 0, compSize,
                                             &dummy[0]);
          else if(dim == 3)
            gl.glCompressedTextureImage3DEXT(tex, targets[t], m, internalFormat, w, h, d, 0,
                                             compSize, &dummy[0]);
        }
        else
        {
          if(dim == 1)
            gl.glTextureImage1DEXT(tex, targets[t], m, internalFormat, w, 0, baseFormat, dataType,
                                   NULL);
          else if(dim == 2)
            gl.glTextureImage2DEXT(tex, targets[t], m, internalFormat, w, h, 0, baseFormat,
                                   dataType, NULL);
          else if(dim == 3)
            gl.glTextureImage3DEXT(tex, targets[t], m, internalFormat, w, h, d, 0, baseFormat,
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
}

void GLResourceManager::PrepareTextureInitialContents(ResourceId liveid, ResourceId origid,
                                                      GLResource res)
{
  const GLHookSet &gl = m_GL->GetHookset();

  WrappedOpenGL::TextureData &details = m_GL->m_Textures[liveid];

  TextureStateInitialData *state =
      (TextureStateInitialData *)Serialiser::AllocAlignedBuffer(sizeof(TextureStateInitialData));
  RDCEraseMem(state, sizeof(TextureStateInitialData));

  if(details.internalFormat == eGL_NONE)
  {
    // textures can get here as GL_NONE if they were created and dirtied (by setting lots of
    // texture parameters) without ever having storage allocated (via glTexStorage or glTexImage).
    // in that case, just ignore as we won't bother with the initial states.
    SetInitialContents(origid, InitialContentData(GLResource(MakeNullResource), 0, (byte *)state));
  }
  else if(details.curType != eGL_TEXTURE_BUFFER)
  {
    GLenum binding = TextureBinding(details.curType);

    bool ms = (details.curType == eGL_TEXTURE_2D_MULTISAMPLE ||
               details.curType == eGL_TEXTURE_2D_MULTISAMPLE_ARRAY);

    state->depthMode = eGL_NONE;
    if(IsDepthStencilFormat(details.internalFormat))
    {
      if(HasExt[ARB_stencil_texturing])
        gl.glGetTextureParameterivEXT(res.name, details.curType, eGL_DEPTH_STENCIL_TEXTURE_MODE,
                                      (GLint *)&state->depthMode);
      else
        state->depthMode = eGL_DEPTH_COMPONENT;
    }

    state->seamless = GL_FALSE;
    if((details.curType == eGL_TEXTURE_CUBE_MAP || details.curType == eGL_TEXTURE_CUBE_MAP_ARRAY) &&
       HasExt[ARB_seamless_cubemap_per_texture])
      gl.glGetTextureParameterivEXT(res.name, details.curType, eGL_TEXTURE_CUBE_MAP_SEAMLESS,
                                    (GLint *)&state->seamless);

    gl.glGetTextureParameterivEXT(res.name, details.curType, eGL_TEXTURE_BASE_LEVEL,
                                  (GLint *)&state->baseLevel);
    gl.glGetTextureParameterivEXT(res.name, details.curType, eGL_TEXTURE_MAX_LEVEL,
                                  (GLint *)&state->maxLevel);

    if(HasExt[ARB_texture_swizzle] || HasExt[EXT_texture_swizzle])
    {
      GetTextureSwizzle(gl, res.name, details.curType, state->swizzle);
    }
    else
    {
      state->swizzle[0] = eGL_RED;
      state->swizzle[1] = eGL_GREEN;
      state->swizzle[2] = eGL_BLUE;
      state->swizzle[3] = eGL_ALPHA;
    }

    // only non-ms textures have sampler state
    if(!ms)
    {
      gl.glGetTextureParameterivEXT(res.name, details.curType, eGL_TEXTURE_SRGB_DECODE_EXT,
                                    (GLint *)&state->srgbDecode);
      gl.glGetTextureParameterivEXT(res.name, details.curType, eGL_TEXTURE_COMPARE_FUNC,
                                    (GLint *)&state->compareFunc);
      gl.glGetTextureParameterivEXT(res.name, details.curType, eGL_TEXTURE_COMPARE_MODE,
                                    (GLint *)&state->compareMode);
      gl.glGetTextureParameterivEXT(res.name, details.curType, eGL_TEXTURE_MIN_FILTER,
                                    (GLint *)&state->minFilter);
      gl.glGetTextureParameterivEXT(res.name, details.curType, eGL_TEXTURE_MAG_FILTER,
                                    (GLint *)&state->magFilter);
      gl.glGetTextureParameterivEXT(res.name, details.curType, eGL_TEXTURE_WRAP_R,
                                    (GLint *)&state->wrap[0]);
      gl.glGetTextureParameterivEXT(res.name, details.curType, eGL_TEXTURE_WRAP_S,
                                    (GLint *)&state->wrap[1]);
      gl.glGetTextureParameterivEXT(res.name, details.curType, eGL_TEXTURE_WRAP_T,
                                    (GLint *)&state->wrap[2]);
      gl.glGetTextureParameterfvEXT(res.name, details.curType, eGL_TEXTURE_MIN_LOD, &state->minLod);
      gl.glGetTextureParameterfvEXT(res.name, details.curType, eGL_TEXTURE_MAX_LOD, &state->maxLod);
      gl.glGetTextureParameterfvEXT(res.name, details.curType, eGL_TEXTURE_BORDER_COLOR,
                                    &state->border[0]);
      if(!IsGLES)
        gl.glGetTextureParameterfvEXT(res.name, details.curType, eGL_TEXTURE_LOD_BIAS,
                                      &state->lodBias);

      // CLAMP isn't supported (border texels gone), assume they meant CLAMP_TO_EDGE
      if(state->wrap[0] == eGL_CLAMP)
        state->wrap[0] = eGL_CLAMP_TO_EDGE;
      if(state->wrap[1] == eGL_CLAMP)
        state->wrap[1] = eGL_CLAMP_TO_EDGE;
      if(state->wrap[2] == eGL_CLAMP)
        state->wrap[2] = eGL_CLAMP_TO_EDGE;
    }

    // we only copy contents for non-views
    GLuint tex = 0;

    if(!details.view)
    {
      {
        GLuint oldtex = 0;
        gl.glGetIntegerv(binding, (GLint *)&oldtex);

        gl.glGenTextures(1, &tex);
        gl.glBindTexture(details.curType, tex);

        gl.glBindTexture(details.curType, oldtex);
      }

      int mips =
          GetNumMips(gl, details.curType, res.name, details.width, details.height, details.depth);

      if(details.curType == eGL_TEXTURE_2D_MULTISAMPLE ||
         details.curType == eGL_TEXTURE_2D_MULTISAMPLE_ARRAY)
        mips = 1;

      // create texture of identical format/size to store initial contents
      CreateTextureImage(tex, details.internalFormat, details.curType, details.dimension,
                         details.width, details.height, details.depth, details.samples, mips);

      // we need to set maxlevel appropriately for number of mips to force the texture to be
      // complete.
      // This can happen if e.g. a texture is initialised just by default with glTexImage for level
      // 0 and used as a framebuffer attachment, then the implementation is fine with it.
      // Unfortunately glCopyImageSubData requires completeness across all mips, a stricter
      // requirement :(.
      // We set max_level to mips - 1 (so mips=1 means MAX_LEVEL=0). Then restore it to the 'real'
      // value we fetched above
      int maxlevel = mips - 1;
      gl.glTextureParameterivEXT(res.name, details.curType, eGL_TEXTURE_MAX_LEVEL,
                                 (GLint *)&maxlevel);

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
        pack.Fetch(&gl, false);
        unpack.Fetch(&gl, false);

        ResetPixelPackState(gl, false, 1);
        ResetPixelUnpackState(gl, false, 1);

        gl.glGetIntegerv(eGL_PIXEL_PACK_BUFFER_BINDING, (GLint *)&pixelPackBuffer);
        gl.glGetIntegerv(eGL_PIXEL_UNPACK_BUFFER_BINDING, (GLint *)&pixelUnpackBuffer);
        gl.glBindBuffer(eGL_PIXEL_PACK_BUFFER, 0);
        gl.glBindBuffer(eGL_PIXEL_UNPACK_BUFFER, 0);
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

            byte *buf = new byte[size];

            if(IsGLES)
            {
              const vector<byte> &data = details.compressedData[i];
              const byte *src =
                  (count == 1) ? data.data() : data.data() + CubeTargetIndex(targets[trg]) * size;
              size_t storedSize = data.size() / count;
              if(storedSize == size)
                memcpy(buf, src, size);
              else
                RDCERR("Different expected and stored compressed texture sizes!");
            }
            else
            {
              // read to CPU
              gl.glGetCompressedTextureImageEXT(res.name, targets[trg], i, buf);
            }

            // write to GPU
            if(details.dimension == 1)
              gl.glCompressedTextureSubImage1DEXT(tex, targets[trg], i, 0, w,
                                                  details.internalFormat, (GLsizei)size, buf);
            else if(details.dimension == 2)
              gl.glCompressedTextureSubImage2DEXT(tex, targets[trg], i, 0, 0, w, h,
                                                  details.internalFormat, (GLsizei)size, buf);
            else if(details.dimension == 3)
              gl.glCompressedTextureSubImage3DEXT(tex, targets[trg], i, 0, 0, 0, w, h, d,
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
            RDCDEBUG("Not fetching initial contents of D32F_S8 texture");
          else
            gl.glCopyImageSubData(res.name, details.curType, i, 0, 0, 0, tex, details.curType, i, 0,
                                  0, 0, w, h, d);
        }
      }

      if(avoidCopySubImage)
      {
        pack.Apply(&gl, false);
        unpack.Apply(&gl, false);

        gl.glBindBuffer(eGL_PIXEL_PACK_BUFFER, pixelPackBuffer);
        gl.glBindBuffer(eGL_PIXEL_UNPACK_BUFFER, pixelUnpackBuffer);
      }

      gl.glTextureParameterivEXT(res.name, details.curType, eGL_TEXTURE_MAX_LEVEL,
                                 (GLint *)&state->maxLevel);
    }

    SetInitialContents(origid, InitialContentData(TextureRes(res.Context, tex), 0, (byte *)state));
  }
  else
  {
    // record texbuffer only state

    GLuint bufName = 0;
    gl.glGetTextureLevelParameterivEXT(res.name, details.curType, 0,
                                       eGL_TEXTURE_BUFFER_DATA_STORE_BINDING, (GLint *)&bufName);
    state->texBuffer = GetID(BufferRes(res.Context, bufName));

    gl.glGetTextureLevelParameterivEXT(res.name, details.curType, 0, eGL_TEXTURE_BUFFER_OFFSET,
                                       (GLint *)&state->texBufOffs);
    gl.glGetTextureLevelParameterivEXT(res.name, details.curType, 0, eGL_TEXTURE_BUFFER_SIZE,
                                       (GLint *)&state->texBufSize);

    SetInitialContents(origid, InitialContentData(GLResource(MakeNullResource), 0, (byte *)state));
  }
}

bool GLResourceManager::Force_InitialState(GLResource res, bool prepare)
{
  if(res.Namespace != eResBuffer && res.Namespace != eResTexture)
    return false;

  // don't need to force anything if we're already including all resources
  if(RenderDoc::Inst().GetCaptureOptions().RefAllResources)
    return false;

  GLResourceRecord *record = GetResourceRecord(res);

  // if we have some viewers, check to see if they were referenced but we weren't, and force our own
  // initial state inclusion.
  if(record && !record->viewTextures.empty())
  {
    // need to prepare all such resources, just in case for the worst case.
    if(prepare)
      return true;

    // if this data resource was referenced already, just skip
    if(m_FrameReferencedResources.find(record->GetResourceID()) != m_FrameReferencedResources.end())
      return false;

    // see if any of our viewers were referenced
    for(auto it = record->viewTextures.begin(); it != record->viewTextures.end(); ++it)
    {
      // if so, return true to force our inclusion, for the benefit of the view
      if(m_FrameReferencedResources.find(*it) != m_FrameReferencedResources.end())
      {
        RDCDEBUG("Forcing inclusion of %llu for %llu", record->GetResourceID(), *it);
        return true;
      }
    }
  }

  return false;
}

bool GLResourceManager::Serialise_InitialState(ResourceId resid, GLResource res)
{
  SERIALISE_ELEMENT(ResourceId, Id, GetID(res));

  if(m_State < WRITING)
  {
    if(HasLiveResource(Id))
      res = GetLiveResource(Id);
    else
      res = GLResource(MakeNullResource);
  }

  const GLHookSet &gl = m_GL->GetHookset();

  if(res.Namespace == eResBuffer)
  {
    // Buffers didn't have serialised initial contents before at all, so although
    // this is newly added it's also backwards compatible.
    if(m_State >= WRITING)
    {
      InitialContentData contents = GetInitialContents(Id);
      GLuint buf = contents.resource.name;
      uint32_t len = contents.num;

      m_pSerialiser->Serialise("len", len);

      byte *readback = (byte *)gl.glMapNamedBufferEXT(buf, eGL_READ_ONLY);

      size_t sz = (size_t)len;

      // readback if possible and serialise
      if(readback)
      {
        m_pSerialiser->SerialiseBuffer("buf", readback, sz);

        gl.glUnmapNamedBufferEXT(buf);
      }
      else
      {
        RDCERR("Couldn't map initial contents buffer for readback!");

        byte *dummy = new byte[len];
        memset(dummy, 0xfe, len);

        m_pSerialiser->SerialiseBuffer("buf", dummy, sz);

        SAFE_DELETE_ARRAY(dummy);
      }
    }
    else
    {
      SERIALISE_ELEMENT(uint32_t, len, 0);

      size_t size = 0;
      byte *data = NULL;

      m_pSerialiser->SerialiseBuffer("buf", data, size);

      // create a new buffer big enough to hold the contents
      GLuint buf = 0;
      gl.glGenBuffers(1, &buf);
      gl.glBindBuffer(eGL_COPY_WRITE_BUFFER, buf);
      gl.glNamedBufferDataEXT(buf, (GLsizeiptr)len, data, eGL_STATIC_DRAW);

      SAFE_DELETE_ARRAY(data);

      SetInitialContents(Id, InitialContentData(BufferRes(m_GL->GetCtx(), buf), len, NULL));
    }
  }
  else if(res.Namespace == eResProgram)
  {
    // most of the time Prepare_InitialState sets the serialise chunk directly on write, but if a
    // program is newly created within a frame we won't have prepared its initial contents, so we
    // need to be ready to write it out here.
    if(m_State >= WRITING)
    {
      SerialiseProgramBindings(gl, m_pSerialiser, res.name, true);

      SerialiseProgramUniforms(gl, m_pSerialiser, res.name, NULL, true);
    }
    else
    {
      WrappedOpenGL::ProgramData &details = m_GL->m_Programs[GetLiveID(Id)];

      GLuint initProg = gl.glCreateProgram();

      for(size_t i = 0; i < details.shaders.size(); i++)
      {
        const auto &shadDetails = m_GL->m_Shaders[details.shaders[i]];

        GLuint shad = gl.glCreateShader(shadDetails.type);

        char **srcs = new char *[shadDetails.sources.size()];
        for(size_t s = 0; s < shadDetails.sources.size(); s++)
          srcs[s] = (char *)shadDetails.sources[s].c_str();
        gl.glShaderSource(shad, (GLsizei)shadDetails.sources.size(), srcs, NULL);

        SAFE_DELETE_ARRAY(srcs);
        gl.glCompileShader(shad);
        gl.glAttachShader(initProg, shad);
        gl.glDeleteShader(shad);
      }

      gl.glLinkProgram(initProg);

      GLint status = 0;
      gl.glGetProgramiv(initProg, eGL_LINK_STATUS, &status);

      // if it failed to link, try again as a separable program.
      // we can't do this by default because of the silly rules meaning
      // shaders need fixup to be separable-compatible.
      if(status == 0)
      {
        gl.glProgramParameteri(initProg, eGL_PROGRAM_SEPARABLE, 1);
        gl.glLinkProgram(initProg);

        gl.glGetProgramiv(initProg, eGL_LINK_STATUS, &status);
      }

      if(status == 0)
      {
        if(details.shaders.size() == 0)
        {
          RDCWARN("No shaders attached to program");
        }
        else
        {
          char buffer[1025] = {0};
          gl.glGetProgramInfoLog(initProg, 1024, NULL, buffer);
          RDCERR("Link error: %s", buffer);
        }
      }

      if(m_GL->GetLogVersion() >= 0x0000014)
      {
        SerialiseProgramBindings(gl, m_pSerialiser, initProg, false);

        // re-link the program to set the new attrib bindings
        gl.glLinkProgram(initProg);
      }

      SerialiseProgramUniforms(gl, m_pSerialiser, initProg, &details.locationTranslate, false);

      SetInitialContents(Id, InitialContentData(ProgramRes(m_GL->GetCtx(), initProg), 0, NULL));
    }
  }
  else if(res.Namespace == eResTexture)
  {
    if(m_State >= WRITING)
    {
      WrappedOpenGL::TextureData &details = m_GL->m_Textures[Id];

      SERIALISE_ELEMENT(GLenum, f, details.internalFormat);

      // only continue with the rest if the format is valid (storage allocated)
      if(f != eGL_NONE)
      {
        GLuint tex = GetInitialContents(Id).resource.name;

        GLuint ppb = 0;
        gl.glGetIntegerv(eGL_PIXEL_PACK_BUFFER_BINDING, (GLint *)&ppb);
        gl.glBindBuffer(eGL_PIXEL_PACK_BUFFER, 0);

        PixelPackState pack;
        pack.Fetch(&gl, false);

        ResetPixelPackState(gl, false, 1);

        int imgmips = 1;

        if(details.curType != eGL_TEXTURE_BUFFER)
          imgmips =
              GetNumMips(gl, details.curType, tex, details.width, details.height, details.depth);

        TextureStateInitialData *state = (TextureStateInitialData *)GetInitialContents(Id).blob;

        SERIALISE_ELEMENT(TextureStateInitialData, stateData, *state);

        SERIALISE_ELEMENT(uint32_t, width, details.width);
        SERIALISE_ELEMENT(uint32_t, height, details.height);
        SERIALISE_ELEMENT(uint32_t, depth, details.depth);
        SERIALISE_ELEMENT(uint32_t, samples, details.samples);
        SERIALISE_ELEMENT(uint32_t, dim, details.dimension);
        SERIALISE_ELEMENT(GLenum, t, details.curType);
        SERIALISE_ELEMENT(int, mips, imgmips);

        SERIALISE_ELEMENT(bool, isCompressed, IsCompressedFormat(details.internalFormat));

        if(details.curType == eGL_TEXTURE_BUFFER || details.view)
        {
          // no contents to copy for texture buffer (it's copied under the buffer)
          // same applies for texture views, their data is copies under the aliased texture
        }
        else if(isCompressed)
        {
          GLint w = details.width;
          GLint h = details.height;
          GLint d = details.depth;

          for(int i = 0; i < mips; i++)
          {
            GLenum targets[] = {
                eGL_TEXTURE_CUBE_MAP_POSITIVE_X, eGL_TEXTURE_CUBE_MAP_NEGATIVE_X,
                eGL_TEXTURE_CUBE_MAP_POSITIVE_Y, eGL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
                eGL_TEXTURE_CUBE_MAP_POSITIVE_Z, eGL_TEXTURE_CUBE_MAP_NEGATIVE_Z,
            };

            int count = ARRAY_COUNT(targets);

            if(t != eGL_TEXTURE_CUBE_MAP)
            {
              targets[0] = details.curType;
              count = 1;
            }

            if(t == eGL_TEXTURE_CUBE_MAP_ARRAY || t == eGL_TEXTURE_1D_ARRAY ||
               t == eGL_TEXTURE_2D_ARRAY)
              d = details.depth;

            for(int trg = 0; trg < count; trg++)
            {
              size_t size = GetCompressedByteSize(w, h, d, details.internalFormat);

              byte *buf = new byte[size];

              if(IsGLES)
              {
                const vector<byte> &data = details.compressedData[i];
                const byte *src =
                    (count == 1) ? data.data() : data.data() + CubeTargetIndex(targets[trg]) * size;
                size_t storedSize = data.size() / count;
                if(storedSize == size)
                  memcpy(buf, src, size);
                else
                  RDCERR("Different expected and stored compressed texture sizes!");
              }
              else
              {
                gl.glGetCompressedTextureImageEXT(tex, targets[trg], i, buf);
              }

              m_pSerialiser->SerialiseBuffer("image", buf, size);

              delete[] buf;
            }

            if(w > 0)
              w = RDCMAX(1, w >> 1);
            if(h > 0)
              h = RDCMAX(1, h >> 1);
            if(d > 0)
              d = RDCMAX(1, d >> 1);
          }
        }
        else if(samples > 1)
        {
          GLNOTIMP("Not implemented - initial states of multisampled textures");
        }
        else
        {
          GLenum fmt = GetBaseFormat(details.internalFormat);
          GLenum type = GetDataType(details.internalFormat);

          size_t size = GetByteSize(details.width, details.height, details.depth, fmt, type);

          byte *buf = new byte[size];

          GLenum binding = TextureBinding(t);

          GLuint prevtex = 0;
          gl.glGetIntegerv(binding, (GLint *)&prevtex);

          gl.glBindTexture(t, tex);

          for(int i = 0; i < mips; i++)
          {
            int w = RDCMAX(details.width >> i, 1);
            int h = RDCMAX(details.height >> i, 1);
            int d = RDCMAX(details.depth >> i, 1);

            if(t == eGL_TEXTURE_CUBE_MAP_ARRAY || t == eGL_TEXTURE_1D_ARRAY ||
               t == eGL_TEXTURE_2D_ARRAY)
              d = details.depth;

            size = GetByteSize(w, h, d, fmt, type);

            GLenum targets[] = {
                eGL_TEXTURE_CUBE_MAP_POSITIVE_X, eGL_TEXTURE_CUBE_MAP_NEGATIVE_X,
                eGL_TEXTURE_CUBE_MAP_POSITIVE_Y, eGL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
                eGL_TEXTURE_CUBE_MAP_POSITIVE_Z, eGL_TEXTURE_CUBE_MAP_NEGATIVE_Z,
            };

            int count = ARRAY_COUNT(targets);

            if(t != eGL_TEXTURE_CUBE_MAP)
            {
              targets[0] = t;
              count = 1;
            }

            for(int trg = 0; trg < count; trg++)
            {
              // we avoid glGetTextureImageEXT as it seems buggy for cubemap faces
              gl.glGetTexImage(targets[trg], i, fmt, type, buf);

              m_pSerialiser->SerialiseBuffer("image", buf, size);
            }
          }

          gl.glBindTexture(t, prevtex);

          SAFE_DELETE_ARRAY(buf);
        }

        gl.glBindBuffer(eGL_PIXEL_PACK_BUFFER, ppb);

        pack.Apply(&gl, false);
      }
    }
    else
    {
      WrappedOpenGL::TextureData &details = m_GL->m_Textures[GetLiveID(Id)];

      SERIALISE_ELEMENT(GLenum, internalformat, eGL_NONE);

      if(internalformat != eGL_NONE)
      {
        GLuint pub = 0;
        gl.glGetIntegerv(eGL_PIXEL_UNPACK_BUFFER_BINDING, (GLint *)&pub);
        gl.glBindBuffer(eGL_PIXEL_UNPACK_BUFFER, 0);

        PixelUnpackState unpack;
        unpack.Fetch(&gl, false);

        ResetPixelUnpackState(gl, false, 1);

        TextureStateInitialData *state = (TextureStateInitialData *)Serialiser::AllocAlignedBuffer(
            sizeof(TextureStateInitialData));
        RDCEraseMem(state, sizeof(TextureStateInitialData));

        m_pSerialiser->Serialise("state", *state);

        SERIALISE_ELEMENT(uint32_t, width, 0);
        SERIALISE_ELEMENT(uint32_t, height, 0);
        SERIALISE_ELEMENT(uint32_t, depth, 0);
        SERIALISE_ELEMENT(uint32_t, samples, 0);
        SERIALISE_ELEMENT(uint32_t, dim, 0);
        SERIALISE_ELEMENT(GLenum, textype, eGL_NONE);
        SERIALISE_ELEMENT(int, mips, 0);
        SERIALISE_ELEMENT(bool, isCompressed, false);

        // if number of mips isn't sufficient, make sure to initialise
        // the lower levels - this could happen if e.g. a texture is
        // init'd with glTexImage(level = 0), then after we stop tracking
        // it glGenerateMipmap is called

        GLuint live = GetLiveResource(Id).name;

        // this is only relevant for non-immutable textures though
        GLint immut = 0;

        if(textype != eGL_TEXTURE_BUFFER)
        {
          gl.glGetTextureParameterivEXT(live, textype, eGL_TEXTURE_IMMUTABLE_FORMAT, &immut);

          GLenum dummy;
          EmulateLuminanceFormat(gl, live, textype, internalformat, dummy);
        }

        if(textype != eGL_TEXTURE_BUFFER && immut == 0)
        {
          GLsizei w = (GLsizei)width;
          GLsizei h = (GLsizei)height;
          GLsizei d = (GLsizei)depth;

          int liveMips = GetNumMips(gl, textype, live, width, height, depth);

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

          for(int m = 1; m < mips; m++)
          {
            w = RDCMAX(1, w >> 1);
            h = RDCMAX(1, h >> 1);
            d = RDCMAX(1, d >> 1);

            if(textype == eGL_TEXTURE_CUBE_MAP_ARRAY || textype == eGL_TEXTURE_1D_ARRAY ||
               textype == eGL_TEXTURE_2D_ARRAY)
              d = (GLsizei)depth;

            if(m >= liveMips)
            {
              for(int t = 0; t < count; t++)
              {
                if(isCompressed)
                {
                  GLsizei compSize = (GLsizei)GetCompressedByteSize(w, h, d, internalformat);

                  vector<byte> dummy;
                  dummy.resize(compSize);

                  if(dim == 1)
                    gl.glCompressedTextureImage1DEXT(live, targets[t], m, internalformat, w, 0,
                                                     compSize, &dummy[0]);
                  else if(dim == 2)
                    gl.glCompressedTextureImage2DEXT(live, targets[t], m, internalformat, w, h, 0,
                                                     compSize, &dummy[0]);
                  else if(dim == 3)
                    gl.glCompressedTextureImage3DEXT(live, targets[t], m, internalformat, w, h, d,
                                                     0, compSize, &dummy[0]);
                }
                else
                {
                  if(dim == 1)
                    gl.glTextureImage1DEXT(live, targets[t], m, internalformat, (GLsizei)w, 0,
                                           GetBaseFormat(internalformat),
                                           GetDataType(internalformat), NULL);
                  else if(dim == 2)
                    gl.glTextureImage2DEXT(live, targets[t], m, internalformat, (GLsizei)w,
                                           (GLsizei)h, 0, GetBaseFormat(internalformat),
                                           GetDataType(internalformat), NULL);
                  else if(dim == 3)
                    gl.glTextureImage3DEXT(live, targets[t], m, internalformat, (GLsizei)w,
                                           (GLsizei)h, (GLsizei)d, 0, GetBaseFormat(internalformat),
                                           GetDataType(internalformat), NULL);
                }
              }
            }
          }
        }

        GLuint tex = 0;

        if(textype != eGL_TEXTURE_BUFFER && !details.view)
        {
          GLuint prevtex = 0;
          gl.glGetIntegerv(TextureBinding(textype), (GLint *)&prevtex);

          gl.glGenTextures(1, &tex);
          gl.glBindTexture(textype, tex);

          gl.glBindTexture(textype, prevtex);
        }

        // create texture of identical format/size to store initial contents
        if(textype == eGL_TEXTURE_BUFFER || details.view)
        {
          // no 'contents' texture to create
        }
        else
        {
          CreateTextureImage(tex, internalformat, textype, dim, width, height, depth, samples, mips);
        }

        if(textype == eGL_TEXTURE_2D_MULTISAMPLE || textype == eGL_TEXTURE_2D_MULTISAMPLE_ARRAY)
          mips = 1;

        if(textype == eGL_TEXTURE_BUFFER || details.view)
        {
          // no contents to serialise
        }
        else if(isCompressed)
        {
          for(int i = 0; i < mips; i++)
          {
            uint32_t w = RDCMAX(width >> i, 1U);
            uint32_t h = RDCMAX(height >> i, 1U);
            uint32_t d = RDCMAX(depth >> i, 1U);

            if(textype == eGL_TEXTURE_CUBE_MAP_ARRAY || textype == eGL_TEXTURE_1D_ARRAY ||
               textype == eGL_TEXTURE_2D_ARRAY)
              d = depth;

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

            for(int trg = 0; trg < count; trg++)
            {
              size_t size = 0;
              byte *buf = NULL;

              m_pSerialiser->SerialiseBuffer("image", buf, size);

              if(IsGLES)
              {
                details.compressedData[i].resize(size);
                memcpy(details.compressedData[i].data(), buf, size);
              }

              if(dim == 1)
                gl.glCompressedTextureSubImage1DEXT(tex, targets[trg], i, 0, w, internalformat,
                                                    (GLsizei)size, buf);
              else if(dim == 2)
                gl.glCompressedTextureSubImage2DEXT(tex, targets[trg], i, 0, 0, w, h,
                                                    internalformat, (GLsizei)size, buf);
              else if(dim == 3)
                gl.glCompressedTextureSubImage3DEXT(tex, targets[trg], i, 0, 0, 0, w, h, d,
                                                    internalformat, (GLsizei)size, buf);

              delete[] buf;
            }
          }
        }
        else if(samples > 1)
        {
          GLNOTIMP("Not implemented - initial states of multisampled textures");
        }
        else
        {
          GLenum fmt = GetBaseFormat(internalformat);
          GLenum type = GetDataType(internalformat);

          for(int i = 0; i < mips; i++)
          {
            uint32_t w = RDCMAX(width >> i, 1U);
            uint32_t h = RDCMAX(height >> i, 1U);
            uint32_t d = RDCMAX(depth >> i, 1U);

            if(textype == eGL_TEXTURE_CUBE_MAP_ARRAY || textype == eGL_TEXTURE_1D_ARRAY ||
               textype == eGL_TEXTURE_2D_ARRAY)
              d = depth;

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

            for(int trg = 0; trg < count; trg++)
            {
              size_t size = 0;
              byte *buf = NULL;
              m_pSerialiser->SerialiseBuffer("image", buf, size);

              if(dim == 1)
                gl.glTextureSubImage1DEXT(tex, targets[trg], i, 0, w, fmt, type, buf);
              else if(dim == 2)
                gl.glTextureSubImage2DEXT(tex, targets[trg], i, 0, 0, w, h, fmt, type, buf);
              else if(dim == 3)
                gl.glTextureSubImage3DEXT(tex, targets[trg], i, 0, 0, 0, w, h, d, fmt, type, buf);

              delete[] buf;
            }
          }
        }

        if(textype != eGL_TEXTURE_BUFFER && !details.view)
          SetInitialContents(Id,
                             InitialContentData(TextureRes(m_GL->GetCtx(), tex), 0, (byte *)state));
        else
          SetInitialContents(Id, InitialContentData(GLResource(MakeNullResource), 0, (byte *)state));

        gl.glBindBuffer(eGL_PIXEL_UNPACK_BUFFER, pub);

        unpack.Apply(&gl, false);
      }
    }
  }
  else if(res.Namespace == eResFramebuffer)
  {
    FramebufferInitialData data;

    if(m_State >= WRITING)
    {
      FramebufferInitialData *initialdata = (FramebufferInitialData *)GetInitialContents(Id).blob;
      memcpy(&data, initialdata, sizeof(data));
    }

    m_pSerialiser->Serialise("Framebuffer object Buffers", data);

    if(m_State < WRITING)
    {
      byte *blob = Serialiser::AllocAlignedBuffer(sizeof(data));
      memcpy(blob, &data, sizeof(data));

      SetInitialContents(Id, InitialContentData(GLResource(MakeNullResource), 0, blob));
    }
  }
  else if(res.Namespace == eResFeedback)
  {
    FeedbackInitialData data;

    if(m_State >= WRITING)
    {
      FeedbackInitialData *initialdata = (FeedbackInitialData *)GetInitialContents(Id).blob;
      memcpy(&data, initialdata, sizeof(data));
    }

    m_pSerialiser->Serialise("Transform Feedback Buffers", data);

    if(m_State < WRITING)
    {
      byte *blob = Serialiser::AllocAlignedBuffer(sizeof(data));
      memcpy(blob, &data, sizeof(data));

      SetInitialContents(Id, InitialContentData(GLResource(MakeNullResource), 0, blob));
    }
  }
  else if(res.Namespace == eResVertexArray)
  {
    VAOInitialData data;

    if(m_State >= WRITING)
    {
      VAOInitialData *initialdata = (VAOInitialData *)GetInitialContents(Id).blob;
      memcpy(&data, initialdata, sizeof(data));
    }

    m_pSerialiser->Serialise("valid", data.valid);
    for(GLuint i = 0; i < 16; i++)
    {
      m_pSerialiser->Serialise("VertexAttrib[]", data.VertexAttribs[i]);
      m_pSerialiser->Serialise("VertexBuffer[]", data.VertexBuffers[i]);
    }
    m_pSerialiser->Serialise("ElementArrayBuffer", data.ElementArrayBuffer);

    if(m_State < WRITING)
    {
      byte *blob = Serialiser::AllocAlignedBuffer(sizeof(data));
      memcpy(blob, &data, sizeof(data));

      SetInitialContents(Id, InitialContentData(GLResource(MakeNullResource), 0, blob));
    }
  }
  else if(res.Namespace == eResRenderbuffer)
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

void GLResourceManager::Create_InitialState(ResourceId id, GLResource live, bool hasData)
{
  if(live.Namespace == eResTexture)
  {
    // we basically need to do exactly the same as Prepare_InitialState -
    // save current texture state, create a duplicate object, and save
    // the current contents into that duplicate object

    // in future if we skip RT contents for write-before-read RTs, we could mark
    // textures to be cleared instead of copied.
    PrepareTextureInitialContents(GetID(live), id, live);
  }
  else if(live.Namespace == eResVertexArray)
  {
    byte *data = Serialiser::AllocAlignedBuffer(sizeof(VAOInitialData));
    RDCEraseMem(data, sizeof(VAOInitialData));

    SetInitialContents(id, InitialContentData(GLResource(MakeNullResource), 0, data));

    Prepare_InitialState(live, data);
  }
  else if(live.Namespace != eResBuffer && live.Namespace != eResProgram &&
          live.Namespace != eResRenderbuffer)
  {
    RDCUNIMPLEMENTED("Expect all initial states to be created & not skipped, presently");
  }
}

void GLResourceManager::Apply_InitialState(GLResource live, InitialContentData initial)
{
  const GLHookSet &gl = m_GL->GetHookset();

  if(live.Namespace == eResBuffer)
  {
    // save old bindings
    GLuint oldbuf1 = 0, oldbuf2 = 0;
    gl.glGetIntegerv(eGL_COPY_READ_BUFFER_BINDING, (GLint *)&oldbuf1);
    gl.glGetIntegerv(eGL_COPY_WRITE_BUFFER_BINDING, (GLint *)&oldbuf2);

    // bind the immutable contents for copying
    gl.glBindBuffer(eGL_COPY_READ_BUFFER, initial.resource.name);

    // bind the live buffer for copying
    gl.glBindBuffer(eGL_COPY_WRITE_BUFFER, live.name);

    // do the actual copy
    gl.glCopyBufferSubData(eGL_COPY_READ_BUFFER, eGL_COPY_WRITE_BUFFER, 0, 0,
                           (GLsizeiptr)initial.num);

    // restore old bindings
    gl.glBindBuffer(eGL_COPY_READ_BUFFER, oldbuf1);
    gl.glBindBuffer(eGL_COPY_WRITE_BUFFER, oldbuf2);
  }
  else if(live.Namespace == eResTexture)
  {
    ResourceId Id = GetID(live);
    WrappedOpenGL::TextureData &details = m_GL->m_Textures[Id];

    TextureStateInitialData *state = (TextureStateInitialData *)initial.blob;

    if(details.curType != eGL_TEXTURE_BUFFER)
    {
      GLuint tex = initial.resource.name;

      if(initial.resource != GLResource(MakeNullResource) && tex != 0)
      {
        int mips = GetNumMips(gl, details.curType, tex, details.width, details.height, details.depth);

        // we need to set maxlevel appropriately for number of mips to force the texture to be
        // complete. This can happen if e.g. a texture is initialised just by default with
        // glTexImage for level 0 and used as a framebuffer attachment, then the implementation is
        // fine with it.
        // Unfortunately glCopyImageSubData requires completeness across all mips, a stricter
        // requirement :(.
        // We set max_level to mips - 1 (so mips=1 means MAX_LEVEL=0). Then below where we set the
        // texture state, the correct MAX_LEVEL is set to whatever the program had.
        int maxlevel = mips - 1;
        gl.glTextureParameterivEXT(live.name, details.curType, eGL_TEXTURE_MAX_LEVEL,
                                   (GLint *)&maxlevel);

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
          pack.Fetch(&gl, false);
          unpack.Fetch(&gl, false);

          ResetPixelPackState(gl, false, 1);
          ResetPixelUnpackState(gl, false, 1);
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
                const vector<byte> &data = details.compressedData[i];
                const byte *src =
                    (count == 1) ? data.data() : data.data() + CubeTargetIndex(targets[trg]) * size;
                size_t storedSize = data.size() / count;
                if(storedSize == size)
                  memcpy(buf, src, size);
                else
                  RDCERR("Different expected and stored compressed texture sizes!");
              }
              else
              {
                // read to CPU
                gl.glGetCompressedTextureImageEXT(tex, targets[trg], i, buf);
              }

              // write to GPU
              if(details.dimension == 1)
                gl.glCompressedTextureSubImage1DEXT(live.name, targets[trg], i, 0, w,
                                                    details.internalFormat, (GLsizei)size, buf);
              else if(details.dimension == 2)
                gl.glCompressedTextureSubImage2DEXT(live.name, targets[trg], i, 0, 0, w, h,
                                                    details.internalFormat, (GLsizei)size, buf);
              else if(details.dimension == 3)
                gl.glCompressedTextureSubImage3DEXT(live.name, targets[trg], i, 0, 0, 0, w, h, d,
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
              RDCDEBUG("Not fetching initial contents of D32F_S8 texture");
            else
              gl.glCopyImageSubData(tex, details.curType, i, 0, 0, 0, live.name, details.curType, i,
                                    0, 0, 0, w, h, d);
          }
        }

        if(avoidCopySubImage)
        {
          pack.Apply(&gl, false);
          unpack.Apply(&gl, false);
        }
      }

      bool ms = (details.curType == eGL_TEXTURE_2D_MULTISAMPLE ||
                 details.curType == eGL_TEXTURE_2D_MULTISAMPLE_ARRAY);

      if((state->depthMode == eGL_DEPTH_COMPONENT || state->depthMode == eGL_STENCIL_INDEX) &&
         HasExt[ARB_stencil_texturing])
        gl.glTextureParameterivEXT(live.name, details.curType, eGL_DEPTH_STENCIL_TEXTURE_MODE,
                                   (GLint *)&state->depthMode);

      if((details.curType == eGL_TEXTURE_CUBE_MAP || details.curType == eGL_TEXTURE_CUBE_MAP_ARRAY) &&
         HasExt[ARB_seamless_cubemap_per_texture])
        gl.glTextureParameterivEXT(live.name, details.curType, eGL_TEXTURE_CUBE_MAP_SEAMLESS,
                                   (GLint *)&state->seamless);

      gl.glTextureParameterivEXT(live.name, details.curType, eGL_TEXTURE_BASE_LEVEL,
                                 (GLint *)&state->baseLevel);
      gl.glTextureParameterivEXT(live.name, details.curType, eGL_TEXTURE_MAX_LEVEL,
                                 (GLint *)&state->maxLevel);

      // assume that emulated (luminance, alpha-only etc) textures are not swizzled
      if(!details.emulated && (HasExt[ARB_texture_swizzle] || HasExt[EXT_texture_swizzle]))
      {
        SetTextureSwizzle(gl, live.name, details.curType, state->swizzle);
      }

      if(!ms)
      {
        gl.glTextureParameterivEXT(live.name, details.curType, eGL_TEXTURE_SRGB_DECODE_EXT,
                                   (GLint *)&state->srgbDecode);
        gl.glTextureParameterivEXT(live.name, details.curType, eGL_TEXTURE_COMPARE_FUNC,
                                   (GLint *)&state->compareFunc);
        gl.glTextureParameterivEXT(live.name, details.curType, eGL_TEXTURE_COMPARE_MODE,
                                   (GLint *)&state->compareMode);
        gl.glTextureParameterivEXT(live.name, details.curType, eGL_TEXTURE_MIN_FILTER,
                                   (GLint *)&state->minFilter);
        gl.glTextureParameterivEXT(live.name, details.curType, eGL_TEXTURE_MAG_FILTER,
                                   (GLint *)&state->magFilter);
        gl.glTextureParameterivEXT(live.name, details.curType, eGL_TEXTURE_WRAP_R,
                                   (GLint *)&state->wrap[0]);
        gl.glTextureParameterivEXT(live.name, details.curType, eGL_TEXTURE_WRAP_S,
                                   (GLint *)&state->wrap[1]);
        gl.glTextureParameterivEXT(live.name, details.curType, eGL_TEXTURE_WRAP_T,
                                   (GLint *)&state->wrap[2]);
        gl.glTextureParameterfvEXT(live.name, details.curType, eGL_TEXTURE_BORDER_COLOR,
                                   state->border);
        if(!IsGLES)
          gl.glTextureParameterfvEXT(live.name, details.curType, eGL_TEXTURE_LOD_BIAS,
                                     &state->lodBias);
        if(details.curType != eGL_TEXTURE_RECTANGLE)
        {
          gl.glTextureParameterfvEXT(live.name, details.curType, eGL_TEXTURE_MIN_LOD, &state->minLod);
          gl.glTextureParameterfvEXT(live.name, details.curType, eGL_TEXTURE_MAX_LOD, &state->maxLod);
        }
      }
    }
    else
    {
      GLuint buffer = 0;

      if(HasLiveResource(state->texBuffer))
        buffer = GetLiveResource(state->texBuffer).name;

      GLenum fmt = details.internalFormat;

      // update width from here as it's authoratitive - the texture might have been resized in
      // multiple rebinds that we will not have serialised before.
      details.width =
          state->texBufSize / uint32_t(GetByteSize(1, 1, 1, GetBaseFormat(fmt), GetDataType(fmt)));

      if(gl.glTextureBufferRangeEXT)
      {
        // restore texbuffer only state
        gl.glTextureBufferRangeEXT(live.name, eGL_TEXTURE_BUFFER, details.internalFormat, buffer,
                                   state->texBufOffs, state->texBufSize);
      }
      else
      {
        uint32_t bufSize = 0;
        gl.glGetNamedBufferParameterivEXT(buffer, eGL_BUFFER_SIZE, (GLint *)&bufSize);
        if(state->texBufOffs > 0 || state->texBufSize > bufSize)
        {
          const char *msg =
              "glTextureBufferRangeEXT is not supported on your GL implementation, but is needed "
              "for correct replay.\n"
              "The original capture created a texture buffer with a range - replay will use the "
              "whole buffer, which is likely incorrect.";
          RDCERR("%s", msg);
          m_GL->AddDebugMessage(MessageCategory::Resource_Manipulation, MessageSeverity::High,
                                MessageSource::IncorrectAPIUse, msg);
        }

        gl.glTextureBufferEXT(live.name, eGL_TEXTURE_BUFFER, details.internalFormat, buffer);
      }
    }
  }
  else if(live.Namespace == eResProgram)
  {
    if(m_GL->GetLogVersion() >= 0x0000014)
    {
      ResourceId Id = GetID(live);

      const WrappedOpenGL::ProgramData &prog = m_GL->m_Programs[Id];

      if(prog.stageShaders[0] != ResourceId())
        CopyProgramAttribBindings(gl, initial.resource.name, live.name,
                                  &m_GL->m_Shaders[prog.stageShaders[0]].reflection);

      if(prog.stageShaders[4] != ResourceId())
        CopyProgramFragDataBindings(gl, initial.resource.name, live.name,
                                    &m_GL->m_Shaders[prog.stageShaders[4]].reflection);

      // we need to re-link the program to apply the bindings, as long as it's linkable.
      // See the comment on shaderProgramUnlinkable for more information.
      if(!prog.shaderProgramUnlinkable)
        gl.glLinkProgram(live.name);
    }

    CopyProgramUniforms(gl, initial.resource.name, live.name);
  }
  else if(live.Namespace == eResFramebuffer)
  {
    FramebufferInitialData *data = (FramebufferInitialData *)initial.blob;

    if(data->valid)
    {
      GLuint prevread = 0, prevdraw = 0;
      gl.glGetIntegerv(eGL_DRAW_FRAMEBUFFER_BINDING, (GLint *)&prevdraw);
      gl.glGetIntegerv(eGL_READ_FRAMEBUFFER_BINDING, (GLint *)&prevread);

      gl.glBindFramebuffer(eGL_DRAW_FRAMEBUFFER, live.name);
      gl.glBindFramebuffer(eGL_READ_FRAMEBUFFER, live.name);

      GLint numCols = 8;
      gl.glGetIntegerv(eGL_MAX_COLOR_ATTACHMENTS, &numCols);

      for(int i = 0; i < (int)ARRAY_COUNT(data->Attachments); i++)
      {
        FramebufferAttachmentData &a = data->Attachments[i];
        GLenum attachment = FramebufferInitialData::attachmentNames[i];

        if(attachment != eGL_DEPTH_ATTACHMENT && attachment != eGL_STENCIL_ATTACHMENT &&
           attachment != eGL_DEPTH_STENCIL_ATTACHMENT)
        {
          // color attachment
          int attachNum = attachment - eGL_COLOR_ATTACHMENT0;
          if(attachNum >= numCols)    // attachment is invalid on this device
            continue;
        }

        GLuint obj = a.obj == ResourceId() ? 0 : GetLiveResource(a.obj).name;

        if(a.renderbuffer && obj)
        {
          gl.glNamedFramebufferRenderbufferEXT(live.name, attachment, eGL_RENDERBUFFER, obj);
        }
        else
        {
          if(!a.layered && obj)
          {
            // we use old-style non-DSA for this because binding cubemap faces with EXT_dsa
            // is completely messed up and broken

            // if obj is a cubemap use face-specific targets
            WrappedOpenGL::TextureData &details = m_GL->m_Textures[GetLiveID(a.obj)];

            if(details.curType == eGL_TEXTURE_CUBE_MAP)
            {
              GLenum faces[] = {
                  eGL_TEXTURE_CUBE_MAP_POSITIVE_X, eGL_TEXTURE_CUBE_MAP_NEGATIVE_X,
                  eGL_TEXTURE_CUBE_MAP_POSITIVE_Y, eGL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
                  eGL_TEXTURE_CUBE_MAP_POSITIVE_Z, eGL_TEXTURE_CUBE_MAP_NEGATIVE_Z,
              };

              if(a.layer < 6)
              {
                gl.glFramebufferTexture2D(eGL_DRAW_FRAMEBUFFER, attachment, faces[a.layer], obj,
                                          a.level);
              }
              else
              {
                RDCWARN("Invalid layer %u used to bind cubemap to framebuffer. Binding POSITIVE_X");
                gl.glFramebufferTexture2D(eGL_DRAW_FRAMEBUFFER, attachment, faces[0], obj, a.level);
              }
            }
            else if(details.curType == eGL_TEXTURE_CUBE_MAP_ARRAY ||
                    details.curType == eGL_TEXTURE_1D_ARRAY ||
                    details.curType == eGL_TEXTURE_2D_ARRAY)
            {
              gl.glFramebufferTextureLayer(eGL_DRAW_FRAMEBUFFER, attachment, obj, a.level, a.layer);
            }
            else
            {
              RDCASSERT(a.layer == 0);
              gl.glNamedFramebufferTextureEXT(live.name, attachment, obj, a.level);
            }
          }
          else
          {
            gl.glNamedFramebufferTextureEXT(live.name, attachment, obj, a.level);
          }
        }
      }

      // set invalid caps to GL_COLOR_ATTACHMENT0
      for(int i = 0; i < (int)ARRAY_COUNT(data->DrawBuffers); i++)
        if(data->DrawBuffers[i] == eGL_BACK || data->DrawBuffers[i] == eGL_FRONT)
          data->DrawBuffers[i] = eGL_COLOR_ATTACHMENT0;
      if(data->ReadBuffer == eGL_BACK || data->ReadBuffer == eGL_FRONT)
        data->ReadBuffer = eGL_COLOR_ATTACHMENT0;

      GLuint maxDraws = 0;
      gl.glGetIntegerv(eGL_MAX_DRAW_BUFFERS, (GLint *)&maxDraws);

      gl.glDrawBuffers(RDCMIN(maxDraws, (GLuint)ARRAY_COUNT(data->DrawBuffers)), data->DrawBuffers);

      gl.glReadBuffer(data->ReadBuffer);

      gl.glBindFramebuffer(eGL_DRAW_FRAMEBUFFER, prevdraw);
      gl.glBindFramebuffer(eGL_READ_FRAMEBUFFER, prevread);
    }
  }
  else if(live.Namespace == eResFeedback)
  {
    FeedbackInitialData *data = (FeedbackInitialData *)initial.blob;

    if(data->valid)
    {
      GLuint prevfeedback = 0;
      gl.glGetIntegerv(eGL_TRANSFORM_FEEDBACK_BINDING, (GLint *)&prevfeedback);

      gl.glBindTransformFeedback(eGL_TRANSFORM_FEEDBACK, live.name);

      GLint maxCount = 0;
      gl.glGetIntegerv(eGL_MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS, &maxCount);

      for(int i = 0; i < (int)ARRAY_COUNT(data->Buffer) && i < maxCount; i++)
      {
        GLuint buffer = data->Buffer[i] == ResourceId() ? 0 : GetLiveResource(data->Buffer[i]).name;
        gl.glBindBufferRange(eGL_TRANSFORM_FEEDBACK_BUFFER, i, buffer, (GLintptr)data->Offset[i],
                             (GLsizei)data->Size[i]);
      }

      gl.glBindTransformFeedback(eGL_TRANSFORM_FEEDBACK, prevfeedback);
    }
  }
  else if(live.Namespace == eResVertexArray)
  {
    VAOInitialData *initialdata = (VAOInitialData *)initial.blob;

    if(initialdata->valid)
    {
      GLuint VAO = 0;
      gl.glGetIntegerv(eGL_VERTEX_ARRAY_BINDING, (GLint *)&VAO);

      if(live.name == 0)
        gl.glBindVertexArray(m_GL->GetFakeVAO());
      else
        gl.glBindVertexArray(live.name);

      for(GLuint i = 0; i < 16; i++)
      {
        VertexAttribInitialData &attrib = initialdata->VertexAttribs[i];

        if(attrib.enabled)
          gl.glEnableVertexAttribArray(i);
        else
          gl.glDisableVertexAttribArray(i);

        gl.glVertexAttribBinding(i, attrib.vbslot);

        if(attrib.size != 0)
        {
          if(attrib.type == eGL_DOUBLE)
            gl.glVertexAttribLFormat(i, attrib.size, attrib.type, attrib.offset);
          else if(attrib.integer == 0)
            gl.glVertexAttribFormat(i, attrib.size, attrib.type, (GLboolean)attrib.normalized,
                                    attrib.offset);
          else
            gl.glVertexAttribIFormat(i, attrib.size, attrib.type, attrib.offset);
        }

        VertexBufferInitialData &buf = initialdata->VertexBuffers[i];

        GLuint buffer = buf.Buffer == ResourceId() ? 0 : GetLiveResource(buf.Buffer).name;

        gl.glBindVertexBuffer(i, buffer, (GLintptr)buf.Offset, (GLsizei)buf.Stride);
        gl.glVertexBindingDivisor(i, buf.Divisor);
      }

      GLuint buffer = initialdata->ElementArrayBuffer == ResourceId()
                          ? 0
                          : GetLiveResource(initialdata->ElementArrayBuffer).name;
      gl.glBindBuffer(eGL_ELEMENT_ARRAY_BUFFER, buffer);

      gl.glBindVertexArray(VAO);
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
