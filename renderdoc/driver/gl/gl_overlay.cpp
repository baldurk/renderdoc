/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2018 Baldur Karlsson
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

#include <float.h>
#include <algorithm>
#include "common/common.h"
#include "data/glsl_shaders.h"
#include "maths/camera.h"
#include "maths/formatpacking.h"
#include "maths/matrix.h"
#include "strings/string_utils.h"
#include "gl_driver.h"
#include "gl_replay.h"
#include "gl_resources.h"

#define OPENGL 1
#include "data/glsl/debuguniforms.h"

void GLReplay::SetupOverlayPipeline(GLuint Program, GLuint Pipeline, GLuint fragProgram)
{
  WrappedOpenGL &gl = *m_pDriver;

  void *ctx = m_ReplayCtx.ctx;

  if(Program == 0)
  {
    if(Pipeline == 0)
    {
      return;
    }
    else
    {
      ResourceId id = m_pDriver->GetResourceManager()->GetID(ProgramPipeRes(ctx, Pipeline));
      auto &pipeDetails = m_pDriver->m_Pipelines[id];

      for(size_t i = 0; i < 4; i++)
      {
        if(pipeDetails.stageShaders[i] != ResourceId())
        {
          GLuint progsrc =
              m_pDriver->GetResourceManager()->GetCurrentResource(pipeDetails.stagePrograms[i]).name;
          GLuint progdst = m_pDriver->m_Shaders[pipeDetails.stageShaders[i]].prog;

          gl.glUseProgramStages(DebugData.overlayPipe, ShaderBit(i), progdst);

          CopyProgramUniforms(gl.GetHookset(), progsrc, progdst);

          if(i == 0)
          {
            CopyProgramAttribBindings(gl.GetHookset(), progsrc, progdst,
                                      GetShader(pipeDetails.stageShaders[i], ShaderEntryPoint()));

            gl.glLinkProgram(progdst);
          }
        }
      }
    }
  }
  else
  {
    auto &progDetails =
        m_pDriver->m_Programs[m_pDriver->GetResourceManager()->GetID(ProgramRes(ctx, Program))];

    for(size_t i = 0; i < 4; i++)
    {
      if(progDetails.stageShaders[i] != ResourceId())
      {
        GLuint progdst = m_pDriver->m_Shaders[progDetails.stageShaders[i]].prog;

        gl.glUseProgramStages(DebugData.overlayPipe, ShaderBit(i), progdst);

        // we have to link the program first, as this trashes all uniform values
        if(i == 0)
        {
          CopyProgramAttribBindings(gl.GetHookset(), Program, progdst,
                                    GetShader(progDetails.stageShaders[i], ShaderEntryPoint()));

          gl.glLinkProgram(progdst);
        }

        CopyProgramUniforms(gl.GetHookset(), Program, progdst);
      }
    }
  }

  // use the generic FS program by default, can be overridden for specific overlays if needed
  gl.glUseProgramStages(DebugData.overlayPipe, eGL_FRAGMENT_SHADER_BIT, fragProgram);
}

ResourceId GLReplay::RenderOverlay(ResourceId texid, CompType typeHint, DebugOverlay overlay,
                                   uint32_t eventId, const vector<uint32_t> &passEvents)
{
  WrappedOpenGL &gl = *m_pDriver;

  MakeCurrentReplayContext(&m_ReplayCtx);

  GLMarkerRegion renderoverlay(StringFormat::Fmt("RenderOverlay %d", overlay));

  void *ctx = m_ReplayCtx.ctx;

  GLRenderState rs(&gl.GetHookset());
  rs.FetchState(&gl);

  // use our overlay pipeline that we'll fill up with all the right
  // shaders, then replace the fragment shader with our own.
  gl.glUseProgram(0);
  gl.glBindProgramPipeline(DebugData.overlayPipe);

  // we bind the separable program created for each shader, and copy
  // uniforms and attrib bindings from the 'real' programs, wherever
  // they are.
  SetupOverlayPipeline(rs.Program.name, rs.Pipeline.name, DebugData.fixedcolFSProg);

  auto &texDetails = m_pDriver->m_Textures[texid];

  GLenum texBindingEnum = eGL_TEXTURE_2D;
  GLenum texQueryEnum = eGL_TEXTURE_BINDING_2D;

  if(texDetails.samples > 1)
  {
    texBindingEnum = eGL_TEXTURE_2D_MULTISAMPLE;
    texQueryEnum = eGL_TEXTURE_BINDING_2D_MULTISAMPLE;
  }

  // resize (or create) the overlay texture and FBO if necessary
  if(DebugData.overlayTexWidth != texDetails.width ||
     DebugData.overlayTexHeight != texDetails.height ||
     DebugData.overlayTexSamples != texDetails.samples)
  {
    if(DebugData.overlayFBO)
    {
      gl.glDeleteFramebuffers(1, &DebugData.overlayFBO);
      gl.glDeleteTextures(1, &DebugData.overlayTex);
    }

    gl.glGenFramebuffers(1, &DebugData.overlayFBO);
    gl.glBindFramebuffer(eGL_FRAMEBUFFER, DebugData.overlayFBO);

    GLuint curTex = 0;
    gl.glGetIntegerv(texQueryEnum, (GLint *)&curTex);

    gl.glGenTextures(1, &DebugData.overlayTex);
    gl.glBindTexture(texBindingEnum, DebugData.overlayTex);

    DebugData.overlayTexWidth = texDetails.width;
    DebugData.overlayTexHeight = texDetails.height;
    DebugData.overlayTexSamples = texDetails.samples;

    if(DebugData.overlayTexSamples > 1)
    {
      gl.glTextureStorage2DMultisampleEXT(DebugData.overlayTex, texBindingEnum, texDetails.samples,
                                          eGL_RGBA16, texDetails.width, texDetails.height, true);
    }
    else
    {
      GLint internalFormat = eGL_RGBA16F;
      GLenum format = eGL_RGBA;
      GLenum type = eGL_FLOAT;

      if(IsGLES && !HasExt[EXT_color_buffer_float])
      {
        internalFormat = eGL_RGBA8;
        type = eGL_UNSIGNED_BYTE;
      }

      gl.glTextureImage2DEXT(DebugData.overlayTex, texBindingEnum, 0, internalFormat,
                             texDetails.width, texDetails.height, 0, format, type, NULL);
      gl.glTexParameteri(texBindingEnum, eGL_TEXTURE_MAX_LEVEL, 0);
      gl.glTexParameteri(texBindingEnum, eGL_TEXTURE_MIN_FILTER, eGL_NEAREST);
      gl.glTexParameteri(texBindingEnum, eGL_TEXTURE_MAG_FILTER, eGL_NEAREST);
      gl.glTexParameteri(texBindingEnum, eGL_TEXTURE_WRAP_S, eGL_CLAMP_TO_EDGE);
      gl.glTexParameteri(texBindingEnum, eGL_TEXTURE_WRAP_T, eGL_CLAMP_TO_EDGE);
    }
    gl.glFramebufferTexture(eGL_FRAMEBUFFER, eGL_COLOR_ATTACHMENT0, DebugData.overlayTex, 0);

    gl.glBindTexture(texBindingEnum, curTex);
  }

  gl.glBindFramebuffer(eGL_FRAMEBUFFER, DebugData.overlayFBO);

  // disable several tests/allow rendering - some overlays will override
  // these states but commonly we don't want to inherit these states from
  // the program's state.
  gl.glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  gl.glDisable(eGL_BLEND);
  gl.glDisable(eGL_SCISSOR_TEST);
  gl.glDepthMask(GL_FALSE);
  gl.glDisable(eGL_CULL_FACE);
  if(!IsGLES)
    gl.glPolygonMode(eGL_FRONT_AND_BACK, eGL_FILL);
  gl.glDisable(eGL_DEPTH_TEST);
  gl.glDisable(eGL_STENCIL_TEST);
  gl.glStencilMask(0);

  if(overlay == DebugOverlay::NaN || overlay == DebugOverlay::Clipping)
  {
    // just need the basic texture
    float black[] = {0.0f, 0.0f, 0.0f, 0.0f};
    gl.glClearBufferfv(eGL_COLOR, 0, black);
  }
  else if(overlay == DebugOverlay::Drawcall)
  {
    float black[] = {0.0f, 0.0f, 0.0f, 0.5f};
    gl.glClearBufferfv(eGL_COLOR, 0, black);

    GLint colLoc = gl.glGetUniformLocation(DebugData.fixedcolFSProg, "RENDERDOC_Fixed_Color");
    float colVal[] = {0.8f, 0.1f, 0.8f, 1.0f};
    gl.glProgramUniform4fv(DebugData.fixedcolFSProg, colLoc, 1, colVal);

    ReplayLog(eventId, eReplay_OnlyDraw);
  }
  else if(overlay == DebugOverlay::Wireframe)
  {
    float wireCol[] = {200.0f / 255.0f, 255.0f / 255.0f, 0.0f / 255.0f, 0.0f};
    gl.glClearBufferfv(eGL_COLOR, 0, wireCol);

    GLint colLoc = gl.glGetUniformLocation(DebugData.fixedcolFSProg, "RENDERDOC_Fixed_Color");
    wireCol[3] = 1.0f;
    gl.glProgramUniform4fv(DebugData.fixedcolFSProg, colLoc, 1, wireCol);

    if(!IsGLES)
      gl.glPolygonMode(eGL_FRONT_AND_BACK, eGL_LINE);

    ReplayLog(eventId, eReplay_OnlyDraw);
  }
  else if(overlay == DebugOverlay::ViewportScissor)
  {
    float col[] = {0.0f, 0.0f, 0.0f, 0.0f};
    gl.glClearBufferfv(eGL_COLOR, 0, col);

    // don't need to use the existing program at all!
    gl.glUseProgram(DebugData.outlineQuadProg);
    gl.glBindProgramPipeline(0);

    gl.glDisablei(eGL_SCISSOR_TEST, 0);

    if(HasExt[ARB_viewport_array])
      gl.glViewportIndexedf(0, rs.Viewports[0].x, rs.Viewports[0].y, rs.Viewports[0].width,
                            rs.Viewports[0].height);
    else
      gl.glViewport((GLint)rs.Viewports[0].x, (GLint)rs.Viewports[0].y,
                    (GLsizei)rs.Viewports[0].width, (GLsizei)rs.Viewports[0].height);

    gl.glBindBufferBase(eGL_UNIFORM_BUFFER, 0, DebugData.UBOs[0]);
    OutlineUBOData *cdata =
        (OutlineUBOData *)gl.glMapBufferRange(eGL_UNIFORM_BUFFER, 0, sizeof(OutlineUBOData),
                                              GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);

    cdata->Inner_Color = Vec4f(0.2f, 0.2f, 0.9f, 0.7f);
    cdata->Border_Color = Vec4f(0.1f, 0.1f, 0.1f, 1.0f);
    cdata->ViewRect =
        Vec4f(rs.Viewports[0].x, rs.Viewports[0].y, rs.Viewports[0].width, rs.Viewports[0].height);
    cdata->Scissor = 0;

    gl.glUnmapBuffer(eGL_UNIFORM_BUFFER);

    gl.glDrawArrays(eGL_TRIANGLE_STRIP, 0, 4);

    if(rs.Scissors[0].enabled)
    {
      Vec4f scissor((float)rs.Scissors[0].x, (float)rs.Scissors[0].y, (float)rs.Scissors[0].width,
                    (float)rs.Scissors[0].height);

      if(HasExt[ARB_viewport_array])
        gl.glViewportIndexedf(0, scissor.x, scissor.y, scissor.z, scissor.w);
      else
        gl.glViewport(rs.Scissors[0].x, rs.Scissors[0].y, rs.Scissors[0].width,
                      rs.Scissors[0].height);

      cdata = (OutlineUBOData *)gl.glMapBufferRange(eGL_UNIFORM_BUFFER, 0, sizeof(OutlineUBOData),
                                                    GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);

      cdata->Inner_Color = Vec4f(0.2f, 0.2f, 0.9f, 0.7f);
      cdata->Border_Color = Vec4f(0.1f, 0.1f, 0.1f, 1.0f);
      cdata->ViewRect = scissor;
      cdata->Scissor = 1;

      gl.glUnmapBuffer(eGL_UNIFORM_BUFFER);

      gl.glDrawArrays(eGL_TRIANGLE_STRIP, 0, 4);
    }
  }
  else if(overlay == DebugOverlay::Depth || overlay == DebugOverlay::Stencil)
  {
    float black[] = {0.0f, 0.0f, 0.0f, 0.0f};
    gl.glClearBufferfv(eGL_COLOR, 0, black);

    GLint colLoc = gl.glGetUniformLocation(DebugData.fixedcolFSProg, "RENDERDOC_Fixed_Color");
    float red[] = {1.0f, 0.0f, 0.0f, 1.0f};
    gl.glProgramUniform4fv(DebugData.fixedcolFSProg, colLoc, 1, red);

    ReplayLog(eventId, eReplay_OnlyDraw);

    GLuint curDepth = 0, curStencil = 0;

    gl.glGetNamedFramebufferAttachmentParameterivEXT(rs.DrawFBO.name, eGL_DEPTH_ATTACHMENT,
                                                     eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME,
                                                     (GLint *)&curDepth);
    gl.glGetNamedFramebufferAttachmentParameterivEXT(rs.DrawFBO.name, eGL_STENCIL_ATTACHMENT,
                                                     eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME,
                                                     (GLint *)&curStencil);

    GLenum copyBindingEnum = texBindingEnum;
    GLenum copyQueryEnum = texQueryEnum;

    GLuint depthCopy = 0, stencilCopy = 0;

    GLint mip = 0;
    GLint layer = 0;

    // create matching depth for existing FBO
    if(curDepth != 0)
    {
      GLint type = 0;
      gl.glGetNamedFramebufferAttachmentParameterivEXT(
          rs.DrawFBO.name, eGL_DEPTH_ATTACHMENT, eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &type);

      GLenum fmt;

      if(type != eGL_RENDERBUFFER)
      {
        ResourceId id = m_pDriver->GetResourceManager()->GetID(TextureRes(ctx, curDepth));
        WrappedOpenGL::TextureData &details = m_pDriver->m_Textures[id];

        fmt = details.internalFormat;

        gl.glGetNamedFramebufferAttachmentParameterivEXT(
            rs.DrawFBO.name, eGL_DEPTH_ATTACHMENT, eGL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL, &mip);

        if(details.curType == eGL_TEXTURE_CUBE_MAP)
        {
          GLenum face;
          gl.glGetNamedFramebufferAttachmentParameterivEXT(
              rs.DrawFBO.name, eGL_DEPTH_ATTACHMENT,
              eGL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE, (GLint *)&face);

          layer = CubeTargetIndex(face);
        }
      }
      else
      {
        ResourceId id = m_pDriver->GetResourceManager()->GetID(RenderbufferRes(ctx, curDepth));
        WrappedOpenGL::TextureData &details = m_pDriver->m_Textures[id];

        fmt = details.internalFormat;
      }

      if(copyBindingEnum == eGL_TEXTURE_CUBE_MAP)
      {
        copyBindingEnum = eGL_TEXTURE_2D;
        copyQueryEnum = eGL_TEXTURE_BINDING_2D;
      }

      GLuint curTex = 0;
      gl.glGetIntegerv(copyQueryEnum, (GLint *)&curTex);

      gl.glGenTextures(1, &depthCopy);
      gl.glBindTexture(copyBindingEnum, depthCopy);
      if(DebugData.overlayTexSamples > 1)
      {
        gl.glTextureStorage2DMultisampleEXT(depthCopy, copyBindingEnum, DebugData.overlayTexSamples,
                                            fmt, DebugData.overlayTexWidth,
                                            DebugData.overlayTexHeight, true);
      }
      else
      {
        gl.glTextureImage2DEXT(depthCopy, copyBindingEnum, 0, fmt, DebugData.overlayTexWidth,
                               DebugData.overlayTexHeight, 0, GetBaseFormat(fmt), GetDataType(fmt),
                               NULL);
        gl.glTexParameteri(copyBindingEnum, eGL_TEXTURE_MAX_LEVEL, 0);
        gl.glTexParameteri(copyBindingEnum, eGL_TEXTURE_MIN_FILTER, eGL_NEAREST);
        gl.glTexParameteri(copyBindingEnum, eGL_TEXTURE_MAG_FILTER, eGL_NEAREST);
        gl.glTexParameteri(copyBindingEnum, eGL_TEXTURE_WRAP_S, eGL_CLAMP_TO_EDGE);
        gl.glTexParameteri(copyBindingEnum, eGL_TEXTURE_WRAP_T, eGL_CLAMP_TO_EDGE);
      }

      gl.glBindTexture(copyBindingEnum, curTex);
    }

    // create matching separate stencil if relevant
    if(curStencil != curDepth && curStencil != 0)
    {
      GLint type = 0;
      gl.glGetNamedFramebufferAttachmentParameterivEXT(
          rs.DrawFBO.name, eGL_STENCIL_ATTACHMENT, eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &type);

      GLenum fmt;

      if(type != eGL_RENDERBUFFER)
      {
        ResourceId id = m_pDriver->GetResourceManager()->GetID(TextureRes(ctx, curDepth));
        WrappedOpenGL::TextureData &details = m_pDriver->m_Textures[id];

        fmt = details.internalFormat;

        if(details.curType == eGL_TEXTURE_CUBE_MAP)
        {
          GLenum face;
          gl.glGetNamedFramebufferAttachmentParameterivEXT(
              rs.DrawFBO.name, eGL_DEPTH_ATTACHMENT,
              eGL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE, (GLint *)&face);

          layer = CubeTargetIndex(face);
        }
      }
      else
      {
        ResourceId id = m_pDriver->GetResourceManager()->GetID(RenderbufferRes(ctx, curDepth));
        WrappedOpenGL::TextureData &details = m_pDriver->m_Textures[id];

        fmt = details.internalFormat;
      }

      GLuint curTex = 0;
      gl.glGetIntegerv(copyQueryEnum, (GLint *)&curTex);

      gl.glGenTextures(1, &stencilCopy);
      gl.glBindTexture(copyBindingEnum, stencilCopy);
      if(DebugData.overlayTexSamples > 1)
      {
        gl.glTextureStorage2DMultisampleEXT(
            stencilCopy, copyBindingEnum, DebugData.overlayTexSamples, fmt,
            DebugData.overlayTexWidth, DebugData.overlayTexHeight, true);
      }
      else
      {
        gl.glTextureImage2DEXT(stencilCopy, copyBindingEnum, 0, fmt, DebugData.overlayTexWidth,
                               DebugData.overlayTexHeight, 0, GetBaseFormat(fmt), GetDataType(fmt),
                               NULL);
        gl.glTexParameteri(copyBindingEnum, eGL_TEXTURE_MAX_LEVEL, 0);
        gl.glTexParameteri(copyBindingEnum, eGL_TEXTURE_MIN_FILTER, eGL_NEAREST);
        gl.glTexParameteri(copyBindingEnum, eGL_TEXTURE_MAG_FILTER, eGL_NEAREST);
        gl.glTexParameteri(copyBindingEnum, eGL_TEXTURE_WRAP_S, eGL_CLAMP_TO_EDGE);
        gl.glTexParameteri(copyBindingEnum, eGL_TEXTURE_WRAP_T, eGL_CLAMP_TO_EDGE);
      }

      gl.glBindTexture(copyBindingEnum, curTex);
    }

    // bind depth/stencil to overlay FBO (currently bound to DRAW_FRAMEBUFFER)
    if(curDepth != 0 && curDepth == curStencil)
    {
      if(layer == 0)
        gl.glFramebufferTexture(eGL_DRAW_FRAMEBUFFER, eGL_DEPTH_STENCIL_ATTACHMENT, depthCopy, mip);
      else
        gl.glFramebufferTextureLayer(eGL_DRAW_FRAMEBUFFER, eGL_DEPTH_STENCIL_ATTACHMENT, depthCopy,
                                     mip, layer);
    }
    else if(curDepth != 0)
    {
      if(layer == 0)
        gl.glFramebufferTexture(eGL_DRAW_FRAMEBUFFER, eGL_DEPTH_ATTACHMENT, depthCopy, mip);
      else
        gl.glFramebufferTextureLayer(eGL_DRAW_FRAMEBUFFER, eGL_DEPTH_ATTACHMENT, depthCopy, mip,
                                     layer);
    }
    else if(curStencil != 0)
    {
      if(layer == 0)
        gl.glFramebufferTexture(eGL_DRAW_FRAMEBUFFER, eGL_STENCIL_ATTACHMENT, stencilCopy, mip);
      else
        gl.glFramebufferTextureLayer(eGL_DRAW_FRAMEBUFFER, eGL_STENCIL_ATTACHMENT, stencilCopy, mip,
                                     layer);
    }

    // bind the 'real' fbo to the read framebuffer, so we can blit from it
    gl.glBindFramebuffer(eGL_READ_FRAMEBUFFER, rs.DrawFBO.name);

    float green[] = {0.0f, 1.0f, 0.0f, 1.0f};
    gl.glProgramUniform4fv(DebugData.fixedcolFSProg, colLoc, 1, green);

    if(overlay == DebugOverlay::Depth)
    {
      if(rs.Enabled[GLRenderState::eEnabled_DepthTest])
        gl.glEnable(eGL_DEPTH_TEST);
      else
        gl.glDisable(eGL_DEPTH_TEST);

      if(rs.DepthWriteMask)
        gl.glDepthMask(GL_TRUE);
      else
        gl.glDepthMask(GL_FALSE);
    }
    else
    {
      if(rs.Enabled[GLRenderState::eEnabled_StencilTest])
        gl.glEnable(eGL_STENCIL_TEST);
      else
        gl.glDisable(eGL_STENCIL_TEST);

      gl.glStencilMaskSeparate(eGL_FRONT, rs.StencilFront.writemask);
      gl.glStencilMaskSeparate(eGL_BACK, rs.StencilBack.writemask);
    }

    // get latest depth/stencil from read FBO (existing FBO) into draw FBO (overlay FBO)
    gl.glBlitFramebuffer(0, 0, DebugData.overlayTexWidth, DebugData.overlayTexHeight, 0, 0,
                         DebugData.overlayTexWidth, DebugData.overlayTexHeight,
                         GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT, eGL_NEAREST);

    ReplayLog(eventId, eReplay_OnlyDraw);

    // unset depth/stencil textures from overlay FBO and delete temp depth/stencil
    if(curDepth != 0 && curDepth == curStencil)
      gl.glFramebufferTexture(eGL_DRAW_FRAMEBUFFER, eGL_DEPTH_STENCIL_ATTACHMENT, 0, 0);
    else if(curDepth != 0)
      gl.glFramebufferTexture(eGL_DRAW_FRAMEBUFFER, eGL_DEPTH_ATTACHMENT, 0, 0);
    else if(curStencil != 0)
      gl.glFramebufferTexture(eGL_DRAW_FRAMEBUFFER, eGL_STENCIL_ATTACHMENT, 0, 0);
    if(depthCopy != 0)
      gl.glDeleteTextures(1, &depthCopy);
    if(stencilCopy != 0)
      gl.glDeleteTextures(1, &stencilCopy);
  }
  else if(overlay == DebugOverlay::BackfaceCull)
  {
    float col[] = {0.0f, 0.0f, 0.0f, 0.0f};
    gl.glClearBufferfv(eGL_COLOR, 0, col);

    col[0] = 1.0f;
    col[3] = 1.0f;

    GLint colLoc = gl.glGetUniformLocation(DebugData.fixedcolFSProg, "RENDERDOC_Fixed_Color");
    gl.glProgramUniform4fv(DebugData.fixedcolFSProg, colLoc, 1, col);

    ReplayLog(eventId, eReplay_OnlyDraw);

    // only enable cull face if it was enabled originally (otherwise
    // we just render green over the exact same area, so it shows up "passing")
    if(rs.Enabled[GLRenderState::eEnabled_CullFace])
      gl.glEnable(eGL_CULL_FACE);

    col[0] = 0.0f;
    col[1] = 1.0f;

    gl.glProgramUniform4fv(DebugData.fixedcolFSProg, colLoc, 1, col);

    ReplayLog(eventId, eReplay_OnlyDraw);
  }
  else if(overlay == DebugOverlay::ClearBeforeDraw || overlay == DebugOverlay::ClearBeforePass)
  {
    float col[] = {0.0f, 0.0f, 0.0f, 0.0f};
    gl.glClearBufferfv(eGL_COLOR, 0, col);

    vector<uint32_t> events = passEvents;

    if(overlay == DebugOverlay::ClearBeforeDraw)
      events.clear();

    events.push_back(eventId);

    if(!events.empty())
    {
      if(overlay == DebugOverlay::ClearBeforePass)
      {
        m_pDriver->ReplayLog(0, events[0], eReplay_WithoutDraw);
      }
      else
      {
        // if we don't replay the real state, restore what we've changed
        rs.ApplyState(&gl);
      }

      float black[] = {0.0f, 0.0f, 0.0f, 0.0f};
      for(int i = 0; i < 8; i++)
        gl.glClearBufferfv(eGL_COLOR, i, black);

      for(size_t i = 0; i < events.size(); i++)
      {
        m_pDriver->ReplayLog(events[i], events[i], eReplay_OnlyDraw);

        if(overlay == DebugOverlay::ClearBeforePass && i + 1 < events.size())
          m_pDriver->ReplayLog(events[i], events[i + 1], eReplay_WithoutDraw);
      }
    }
  }
  else if(overlay == DebugOverlay::TriangleSizeDraw || overlay == DebugOverlay::TriangleSizePass)
  {
    SCOPED_TIMER("Triangle Size");

    float black[] = {0.0f, 0.0f, 0.0f, 0.0f};
    gl.glClearBufferfv(eGL_COLOR, 0, black);

    MeshUBOData uboParams = {};
    uboParams.homogenousInput = 1;
    uboParams.invProj = Matrix4f::Identity();
    uboParams.mvp = Matrix4f::Identity();

    gl.glBindBuffer(eGL_COPY_WRITE_BUFFER, DebugData.UBOs[0]);

    MeshUBOData *uboptr =
        (MeshUBOData *)gl.glMapBufferRange(eGL_COPY_WRITE_BUFFER, 0, sizeof(MeshUBOData),
                                           GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
    *uboptr = uboParams;
    gl.glUnmapBuffer(eGL_COPY_WRITE_BUFFER);

    gl.glBindBuffer(eGL_COPY_WRITE_BUFFER, DebugData.UBOs[1]);
    Vec4f *v = (Vec4f *)gl.glMapBufferRange(eGL_COPY_WRITE_BUFFER, 0, sizeof(overdrawRamp),
                                            GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
    memcpy(v, overdrawRamp, sizeof(overdrawRamp));
    gl.glUnmapBuffer(eGL_COPY_WRITE_BUFFER);

    gl.glBindBuffer(eGL_COPY_WRITE_BUFFER, DebugData.UBOs[2]);
    v = (Vec4f *)gl.glMapBufferRange(eGL_COPY_WRITE_BUFFER, 0, sizeof(Vec4f),
                                     GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
    *v = Vec4f((float)texDetails.width, (float)texDetails.height);
    gl.glUnmapBuffer(eGL_COPY_WRITE_BUFFER);

    vector<uint32_t> events = passEvents;

    if(overlay == DebugOverlay::TriangleSizeDraw)
      events.clear();

    events.push_back(eventId);

    if(!events.empty() && DebugData.trisizeProg)
    {
      if(overlay == DebugOverlay::TriangleSizePass)
        ReplayLog(events[0], eReplay_WithoutDraw);
      else
        rs.ApplyState(m_pDriver);

      // this all happens on the replay context so we need a temp FBO/VAO
      GLuint overlayFBO = 0, tempVAO = 0;
      gl.glGenFramebuffers(1, &overlayFBO);
      gl.glGenVertexArrays(1, &tempVAO);

      for(size_t i = 0; i < events.size(); i++)
      {
        GLboolean blending = GL_FALSE;
        GLint depthwritemask = 1;
        GLint stencilfmask = 0xff, stencilbmask = 0xff;
        GLuint drawFBO = 0, prevVAO = 0;
        struct UBO
        {
          GLuint buf;
          GLint64 offs;
          GLint64 size;
        } ubos[3];

        // save the state we're going to mess with
        {
          gl.glGetIntegerv(eGL_DEPTH_WRITEMASK, &depthwritemask);
          gl.glGetIntegerv(eGL_STENCIL_WRITEMASK, &stencilfmask);
          gl.glGetIntegerv(eGL_STENCIL_BACK_WRITEMASK, &stencilbmask);

          gl.glGetIntegerv(eGL_DRAW_FRAMEBUFFER_BINDING, (GLint *)&drawFBO);
          gl.glGetIntegerv(eGL_VERTEX_ARRAY_BINDING, (GLint *)&prevVAO);

          blending = gl.glIsEnabled(eGL_BLEND);

          for(uint32_t u = 0; u < 3; u++)
          {
            gl.glGetIntegeri_v(eGL_UNIFORM_BUFFER_BINDING, u, (GLint *)&ubos[u].buf);
            gl.glGetInteger64i_v(eGL_UNIFORM_BUFFER_START, u, (GLint64 *)&ubos[u].offs);
            gl.glGetInteger64i_v(eGL_UNIFORM_BUFFER_SIZE, u, (GLint64 *)&ubos[u].size);
          }
        }

        // disable depth and stencil writes
        gl.glDepthMask(GL_FALSE);
        gl.glStencilMask(GL_FALSE);

        // disable blending
        gl.glDisable(eGL_BLEND);

        // bind our UBOs
        gl.glBindBufferBase(eGL_UNIFORM_BUFFER, 0, DebugData.UBOs[0]);
        gl.glBindBufferBase(eGL_UNIFORM_BUFFER, 1, DebugData.UBOs[1]);
        gl.glBindBufferBase(eGL_UNIFORM_BUFFER, 2, DebugData.UBOs[2]);

        const GLenum att = eGL_DEPTH_ATTACHMENT;
        GLuint depthObj = 0;
        GLint type = 0, level = 0, layered = 0, layer = 0;

        // fetch the details of the 'real' depth attachment
        gl.glGetNamedFramebufferAttachmentParameterivEXT(
            drawFBO, att, eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, (GLint *)&depthObj);
        gl.glGetNamedFramebufferAttachmentParameterivEXT(
            drawFBO, att, eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &type);

        if(depthObj)
        {
          gl.glGetNamedFramebufferAttachmentParameterivEXT(
              drawFBO, att, eGL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL, &level);
          gl.glGetNamedFramebufferAttachmentParameterivEXT(
              drawFBO, att, eGL_FRAMEBUFFER_ATTACHMENT_LAYERED, &layered);

          layered = (layered != 0);

          layer = 0;
          if(layered == 0)
            gl.glGetNamedFramebufferAttachmentParameterivEXT(
                drawFBO, att, eGL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LAYER, &layer);

          if(type != eGL_RENDERBUFFER)
          {
            ResourceId id = m_pDriver->GetResourceManager()->GetID(TextureRes(ctx, depthObj));
            WrappedOpenGL::TextureData &details = m_pDriver->m_Textures[id];

            if(details.curType == eGL_TEXTURE_CUBE_MAP)
            {
              GLenum face;
              gl.glGetNamedFramebufferAttachmentParameterivEXT(
                  drawFBO, att, eGL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE, (GLint *)&face);

              layer = CubeTargetIndex(face);
            }
          }
        }

        // bind our FBO
        gl.glBindFramebuffer(eGL_DRAW_FRAMEBUFFER, overlayFBO);
        gl.glFramebufferTexture(eGL_FRAMEBUFFER, eGL_COLOR_ATTACHMENT0, DebugData.overlayTex, 0);

        // now apply the depth texture binding
        if(depthObj)
        {
          if(type == eGL_RENDERBUFFER)
          {
            gl.glNamedFramebufferRenderbufferEXT(overlayFBO, att, eGL_RENDERBUFFER, depthObj);
          }
          else
          {
            if(!layered)
            {
              // we use old-style non-DSA for this because binding cubemap faces with EXT_dsa
              // is completely messed up and broken

              // if obj is a cubemap use face-specific targets
              ResourceId id = m_pDriver->GetResourceManager()->GetID(TextureRes(ctx, depthObj));
              WrappedOpenGL::TextureData &details = m_pDriver->m_Textures[id];

              if(details.curType == eGL_TEXTURE_CUBE_MAP)
              {
                GLenum faces[] = {
                    eGL_TEXTURE_CUBE_MAP_POSITIVE_X, eGL_TEXTURE_CUBE_MAP_NEGATIVE_X,
                    eGL_TEXTURE_CUBE_MAP_POSITIVE_Y, eGL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
                    eGL_TEXTURE_CUBE_MAP_POSITIVE_Z, eGL_TEXTURE_CUBE_MAP_NEGATIVE_Z,
                };

                if(layer < 6)
                {
                  gl.glFramebufferTexture2D(eGL_DRAW_FRAMEBUFFER, att, faces[layer], depthObj, level);
                }
                else
                {
                  RDCWARN(
                      "Invalid layer %u used to bind cubemap to framebuffer. Binding POSITIVE_X");
                  gl.glFramebufferTexture2D(eGL_DRAW_FRAMEBUFFER, att, faces[0], depthObj, level);
                }
              }
              else if(details.curType == eGL_TEXTURE_CUBE_MAP_ARRAY ||
                      details.curType == eGL_TEXTURE_1D_ARRAY ||
                      details.curType == eGL_TEXTURE_2D_ARRAY)
              {
                gl.glFramebufferTextureLayer(eGL_DRAW_FRAMEBUFFER, att, depthObj, level, layer);
              }
              else
              {
                RDCASSERT(layer == 0);
                gl.glNamedFramebufferTextureEXT(overlayFBO, att, depthObj, level);
              }
            }
            else
            {
              gl.glNamedFramebufferTextureEXT(overlayFBO, att, depthObj, level);
            }
          }
        }

        GLuint prog = 0, pipe = 0;
        gl.glGetIntegerv(eGL_CURRENT_PROGRAM, (GLint *)&prog);
        gl.glGetIntegerv(eGL_PROGRAM_PIPELINE_BINDING, (GLint *)&pipe);

        gl.glUseProgram(DebugData.trisizeProg);
        gl.glBindProgramPipeline(0);

        const DrawcallDescription *draw = m_pDriver->GetDrawcall(events[i]);

        for(uint32_t inst = 0; draw && inst < RDCMAX(1U, draw->numInstances); inst++)
        {
          MeshFormat postvs = GetPostVSBuffers(events[i], inst, MeshDataStage::GSOut);
          if(postvs.vertexResourceId == ResourceId())
            postvs = GetPostVSBuffers(events[i], inst, MeshDataStage::VSOut);

          if(postvs.vertexResourceId != ResourceId())
          {
            GLenum topo = MakeGLPrimitiveTopology(postvs.topology);

            gl.glBindVertexArray(tempVAO);

            {
              if(postvs.format.Special())
              {
                if(postvs.format.type == ResourceFormatType::R10G10B10A2)
                {
                  if(postvs.format.compType == CompType::UInt)
                    gl.glVertexAttribIFormat(0, 4, eGL_UNSIGNED_INT_2_10_10_10_REV, 0);
                  if(postvs.format.compType == CompType::SInt)
                    gl.glVertexAttribIFormat(0, 4, eGL_INT_2_10_10_10_REV, 0);
                }
                else if(postvs.format.type == ResourceFormatType::R11G11B10)
                {
                  gl.glVertexAttribFormat(0, 4, eGL_UNSIGNED_INT_10F_11F_11F_REV, GL_FALSE, 0);
                }
                else
                {
                  RDCWARN("Unsupported vertex attribute format: %x", postvs.format.type);
                }
              }
              else if(postvs.format.compType == CompType::Float ||
                      postvs.format.compType == CompType::UNorm ||
                      postvs.format.compType == CompType::SNorm)
              {
                GLenum fmttype = eGL_UNSIGNED_INT;

                if(postvs.format.compByteWidth == 4)
                {
                  if(postvs.format.compType == CompType::Float)
                    fmttype = eGL_FLOAT;
                  else if(postvs.format.compType == CompType::UNorm)
                    fmttype = eGL_UNSIGNED_INT;
                  else if(postvs.format.compType == CompType::SNorm)
                    fmttype = eGL_INT;
                }
                else if(postvs.format.compByteWidth == 2)
                {
                  if(postvs.format.compType == CompType::Float)
                    fmttype = eGL_HALF_FLOAT;
                  else if(postvs.format.compType == CompType::UNorm)
                    fmttype = eGL_UNSIGNED_SHORT;
                  else if(postvs.format.compType == CompType::SNorm)
                    fmttype = eGL_SHORT;
                }
                else if(postvs.format.compByteWidth == 1)
                {
                  if(postvs.format.compType == CompType::UNorm)
                    fmttype = eGL_UNSIGNED_BYTE;
                  else if(postvs.format.compType == CompType::SNorm)
                    fmttype = eGL_BYTE;
                }

                gl.glVertexAttribFormat(0, postvs.format.compCount, fmttype,
                                        postvs.format.compType != CompType::Float, 0);
              }
              else if(postvs.format.compType == CompType::UInt ||
                      postvs.format.compType == CompType::SInt)
              {
                GLenum fmttype = eGL_UNSIGNED_INT;

                if(postvs.format.compByteWidth == 4)
                {
                  if(postvs.format.compType == CompType::UInt)
                    fmttype = eGL_UNSIGNED_INT;
                  else if(postvs.format.compType == CompType::SInt)
                    fmttype = eGL_INT;
                }
                else if(postvs.format.compByteWidth == 2)
                {
                  if(postvs.format.compType == CompType::UInt)
                    fmttype = eGL_UNSIGNED_SHORT;
                  else if(postvs.format.compType == CompType::SInt)
                    fmttype = eGL_SHORT;
                }
                else if(postvs.format.compByteWidth == 1)
                {
                  if(postvs.format.compType == CompType::UInt)
                    fmttype = eGL_UNSIGNED_BYTE;
                  else if(postvs.format.compType == CompType::SInt)
                    fmttype = eGL_BYTE;
                }

                gl.glVertexAttribIFormat(0, postvs.format.compCount, fmttype, 0);
              }
              else if(postvs.format.compType == CompType::Double)
              {
                gl.glVertexAttribLFormat(0, postvs.format.compCount, eGL_DOUBLE, 0);
              }

              GLuint vb =
                  m_pDriver->GetResourceManager()->GetCurrentResource(postvs.vertexResourceId).name;
              gl.glBindVertexBuffer(0, vb, (GLintptr)postvs.vertexByteOffset,
                                    postvs.vertexByteStride);
            }

            gl.glEnableVertexAttribArray(0);
            gl.glDisableVertexAttribArray(1);

            if(postvs.indexByteStride)
            {
              GLenum idxtype = eGL_UNSIGNED_BYTE;
              if(postvs.indexByteStride == 2)
                idxtype = eGL_UNSIGNED_SHORT;
              else if(postvs.indexByteStride == 4)
                idxtype = eGL_UNSIGNED_INT;

              GLuint ib =
                  m_pDriver->GetResourceManager()->GetCurrentResource(postvs.indexResourceId).name;
              gl.glBindBuffer(eGL_ELEMENT_ARRAY_BUFFER, ib);
              gl.glDrawElementsBaseVertex(topo, postvs.numIndices, idxtype,
                                          (const void *)uintptr_t(postvs.indexByteOffset),
                                          postvs.baseVertex);
            }
            else
            {
              gl.glDrawArrays(topo, 0, postvs.numIndices);
            }
          }
        }

        // pop the state that we messed with
        {
          gl.glBindProgramPipeline(pipe);
          gl.glUseProgram(prog);

          if(blending)
            gl.glEnable(eGL_BLEND);
          else
            gl.glDisable(eGL_BLEND);

          // restore the previous FBO/VAO
          gl.glBindFramebuffer(eGL_DRAW_FRAMEBUFFER, drawFBO);
          gl.glBindVertexArray(prevVAO);

          for(uint32_t u = 0; u < 3; u++)
          {
            if(ubos[u].buf == 0 || (ubos[u].offs == 0 && ubos[u].size == 0))
              gl.glBindBufferBase(eGL_UNIFORM_BUFFER, u, ubos[u].buf);
            else
              gl.glBindBufferRange(eGL_UNIFORM_BUFFER, u, ubos[u].buf, (GLintptr)ubos[u].offs,
                                   (GLsizeiptr)ubos[u].size);
          }

          gl.glDepthMask(depthwritemask ? GL_TRUE : GL_FALSE);
          gl.glStencilMaskSeparate(eGL_FRONT, (GLuint)stencilfmask);
          gl.glStencilMaskSeparate(eGL_BACK, (GLuint)stencilbmask);
        }

        if(overlay == DebugOverlay::TriangleSizePass)
        {
          m_pDriver->ReplayLog(0, events[i], eReplay_OnlyDraw);

          if(i + 1 < events.size())
            m_pDriver->ReplayLog(events[i], events[i + 1], eReplay_WithoutDraw);
        }
      }

      gl.glDeleteFramebuffers(1, &overlayFBO);
      gl.glDeleteVertexArrays(1, &tempVAO);

      if(overlay == DebugOverlay::TriangleSizePass)
        ReplayLog(eventId, eReplay_WithoutDraw);
    }
  }
  else if(overlay == DebugOverlay::QuadOverdrawDraw || overlay == DebugOverlay::QuadOverdrawPass)
  {
    if(DebugData.quadoverdrawFSProg)
    {
      SCOPED_TIMER("Quad Overdraw");

      float black[] = {0.0f, 0.0f, 0.0f, 0.0f};
      gl.glClearBufferfv(eGL_COLOR, 0, black);

      vector<uint32_t> events = passEvents;

      if(overlay == DebugOverlay::QuadOverdrawDraw)
        events.clear();

      events.push_back(eventId);

      if(!events.empty())
      {
        GLuint replacefbo = 0;
        GLuint quadtexs[3] = {0};
        gl.glGenFramebuffers(1, &replacefbo);
        gl.glBindFramebuffer(eGL_FRAMEBUFFER, replacefbo);

        gl.glGenTextures(3, quadtexs);

        // image for quad usage
        gl.glBindTexture(eGL_TEXTURE_2D_ARRAY, quadtexs[2]);
        gl.glTextureImage3DEXT(quadtexs[2], eGL_TEXTURE_2D_ARRAY, 0, eGL_R32UI,
                               RDCMAX(1, texDetails.width >> 1), RDCMAX(1, texDetails.height >> 1),
                               4, 0, eGL_RED_INTEGER, eGL_UNSIGNED_INT, NULL);
        gl.glTexParameteri(eGL_TEXTURE_2D_ARRAY, eGL_TEXTURE_MAX_LEVEL, 0);

        // temporarily attach to FBO to clear it
        GLint zero[4] = {0};
        gl.glFramebufferTextureLayer(eGL_FRAMEBUFFER, eGL_COLOR_ATTACHMENT0, quadtexs[2], 0, 0);
        gl.glClearBufferiv(eGL_COLOR, 0, zero);
        gl.glFramebufferTextureLayer(eGL_FRAMEBUFFER, eGL_COLOR_ATTACHMENT0, quadtexs[2], 0, 1);
        gl.glClearBufferiv(eGL_COLOR, 0, zero);
        gl.glFramebufferTextureLayer(eGL_FRAMEBUFFER, eGL_COLOR_ATTACHMENT0, quadtexs[2], 0, 2);
        gl.glClearBufferiv(eGL_COLOR, 0, zero);
        gl.glFramebufferTextureLayer(eGL_FRAMEBUFFER, eGL_COLOR_ATTACHMENT0, quadtexs[2], 0, 3);
        gl.glClearBufferiv(eGL_COLOR, 0, zero);

        gl.glBindTexture(eGL_TEXTURE_2D, quadtexs[0]);
        gl.glTextureImage2DEXT(quadtexs[0], eGL_TEXTURE_2D, 0, eGL_RGBA8, texDetails.width,
                               texDetails.height, 0, eGL_RGBA, eGL_UNSIGNED_BYTE, NULL);
        gl.glTexParameteri(eGL_TEXTURE_2D, eGL_TEXTURE_MAX_LEVEL, 0);
        gl.glTexParameteri(eGL_TEXTURE_2D, eGL_TEXTURE_MIN_FILTER, eGL_NEAREST);
        gl.glTexParameteri(eGL_TEXTURE_2D, eGL_TEXTURE_MAG_FILTER, eGL_NEAREST);
        gl.glTexParameteri(eGL_TEXTURE_2D, eGL_TEXTURE_WRAP_S, eGL_CLAMP_TO_EDGE);
        gl.glTexParameteri(eGL_TEXTURE_2D, eGL_TEXTURE_WRAP_T, eGL_CLAMP_TO_EDGE);
        gl.glFramebufferTexture(eGL_FRAMEBUFFER, eGL_COLOR_ATTACHMENT0, quadtexs[0], 0);

        GLuint curDepth = 0, depthType = 0;

        // TODO handle non-2D depth/stencil attachments and fetch slice or cubemap face
        GLint mip = 0;

        gl.glGetNamedFramebufferAttachmentParameterivEXT(rs.DrawFBO.name, eGL_DEPTH_ATTACHMENT,
                                                         eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME,
                                                         (GLint *)&curDepth);
        gl.glGetNamedFramebufferAttachmentParameterivEXT(rs.DrawFBO.name, eGL_DEPTH_ATTACHMENT,
                                                         eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE,
                                                         (GLint *)&depthType);
        gl.glGetNamedFramebufferAttachmentParameterivEXT(
            rs.DrawFBO.name, eGL_DEPTH_ATTACHMENT, eGL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL, &mip);

        GLenum fmt = eGL_DEPTH32F_STENCIL8;

        if(depthType == eGL_TEXTURE)
        {
          gl.glGetTextureLevelParameterivEXT(curDepth, texBindingEnum, mip,
                                             eGL_TEXTURE_INTERNAL_FORMAT, (GLint *)&fmt);
        }
        else
        {
          gl.glGetNamedRenderbufferParameterivEXT(curDepth, eGL_RENDERBUFFER_INTERNAL_FORMAT,
                                                  (GLint *)&fmt);
        }

        gl.glBindTexture(eGL_TEXTURE_2D, quadtexs[1]);
        gl.glTextureImage2DEXT(quadtexs[1], eGL_TEXTURE_2D, 0, fmt, texDetails.width,
                               texDetails.height, 0, GetBaseFormat(fmt), GetDataType(fmt), NULL);
        gl.glTexParameteri(eGL_TEXTURE_2D, eGL_TEXTURE_MAX_LEVEL, 0);
        gl.glTexParameteri(eGL_TEXTURE_2D, eGL_TEXTURE_MIN_FILTER, eGL_NEAREST);
        gl.glTexParameteri(eGL_TEXTURE_2D, eGL_TEXTURE_MAG_FILTER, eGL_NEAREST);
        gl.glTexParameteri(eGL_TEXTURE_2D, eGL_TEXTURE_WRAP_S, eGL_CLAMP_TO_EDGE);
        gl.glTexParameteri(eGL_TEXTURE_2D, eGL_TEXTURE_WRAP_T, eGL_CLAMP_TO_EDGE);

        GLenum dsAttach = eGL_DEPTH_STENCIL_ATTACHMENT;

        if(GetBaseFormat(fmt) == eGL_DEPTH_COMPONENT)
          dsAttach = eGL_DEPTH_ATTACHMENT;

        gl.glFramebufferTexture(eGL_FRAMEBUFFER, dsAttach, quadtexs[1], 0);

        if(overlay == DebugOverlay::QuadOverdrawPass)
          ReplayLog(events[0], eReplay_WithoutDraw);
        else
          rs.ApplyState(m_pDriver);

        for(size_t i = 0; i < events.size(); i++)
        {
          GLint depthwritemask = 1;
          GLint stencilfmask = 0xff, stencilbmask = 0xff;
          GLuint curdrawfbo = 0, curreadfbo = 0;
          struct
          {
            GLuint name;
            GLuint level;
            GLboolean layered;
            GLuint layer;
            GLenum access;
            GLenum format;
          } curimage0 = {0};

          // save the state we're going to mess with
          {
            gl.glGetIntegerv(eGL_DEPTH_WRITEMASK, &depthwritemask);
            gl.glGetIntegerv(eGL_STENCIL_WRITEMASK, &stencilfmask);
            gl.glGetIntegerv(eGL_STENCIL_BACK_WRITEMASK, &stencilbmask);

            gl.glGetIntegerv(eGL_DRAW_FRAMEBUFFER_BINDING, (GLint *)&curdrawfbo);
            gl.glGetIntegerv(eGL_READ_FRAMEBUFFER_BINDING, (GLint *)&curreadfbo);

            gl.glGetIntegeri_v(eGL_IMAGE_BINDING_NAME, 0, (GLint *)&curimage0.name);
            gl.glGetIntegeri_v(eGL_IMAGE_BINDING_LEVEL, 0, (GLint *)&curimage0.level);
            gl.glGetIntegeri_v(eGL_IMAGE_BINDING_ACCESS, 0, (GLint *)&curimage0.access);
            gl.glGetIntegeri_v(eGL_IMAGE_BINDING_FORMAT, 0, (GLint *)&curimage0.format);
            gl.glGetBooleani_v(eGL_IMAGE_BINDING_LAYERED, 0, &curimage0.layered);
            if(curimage0.layered)
              gl.glGetIntegeri_v(eGL_IMAGE_BINDING_LAYER, 0, (GLint *)&curimage0.layer);
          }

          // disable depth and stencil writes
          gl.glDepthMask(GL_FALSE);
          gl.glStencilMask(GL_FALSE);

          // bind our FBO
          gl.glBindFramebuffer(eGL_DRAW_FRAMEBUFFER, replacefbo);
          // bind image
          gl.glBindImageTexture(0, quadtexs[2], 0, GL_TRUE, 0, eGL_READ_WRITE, eGL_R32UI);

          GLuint prog = 0, pipe = 0;
          gl.glGetIntegerv(eGL_CURRENT_PROGRAM, (GLint *)&prog);
          gl.glGetIntegerv(eGL_PROGRAM_PIPELINE_BINDING, (GLint *)&pipe);

          // replace fragment shader. This is exactly what we did
          // at the start of this function for the single-event case, but now we have
          // to do it for every event
          SetupOverlayPipeline(prog, pipe, DebugData.quadoverdrawFSProg);
          gl.glUseProgram(0);
          gl.glBindProgramPipeline(DebugData.overlayPipe);

          gl.glBindFramebuffer(eGL_READ_FRAMEBUFFER, curdrawfbo);
          gl.glBlitFramebuffer(0, 0, texDetails.width, texDetails.height, 0, 0, texDetails.width,
                               texDetails.height, GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT,
                               eGL_NEAREST);

          m_pDriver->ReplayLog(0, events[i], eReplay_OnlyDraw);

          // pop the state that we messed with
          {
            gl.glBindProgramPipeline(pipe);
            gl.glUseProgram(prog);

            if(curimage0.name)
              gl.glBindImageTexture(0, curimage0.name, curimage0.level,
                                    curimage0.layered ? GL_TRUE : GL_FALSE, curimage0.layer,
                                    curimage0.access, curimage0.format);
            else
              gl.glBindImageTexture(0, 0, 0, GL_FALSE, 0, eGL_READ_ONLY, eGL_R32UI);

            gl.glBindFramebuffer(eGL_DRAW_FRAMEBUFFER, curdrawfbo);
            gl.glBindFramebuffer(eGL_READ_FRAMEBUFFER, curreadfbo);

            gl.glDepthMask(depthwritemask ? GL_TRUE : GL_FALSE);
            gl.glStencilMaskSeparate(eGL_FRONT, (GLuint)stencilfmask);
            gl.glStencilMaskSeparate(eGL_BACK, (GLuint)stencilbmask);
          }

          if(overlay == DebugOverlay::QuadOverdrawPass)
          {
            m_pDriver->ReplayLog(0, events[i], eReplay_OnlyDraw);

            if(i + 1 < events.size())
              m_pDriver->ReplayLog(events[i], events[i + 1], eReplay_WithoutDraw);
          }
        }

        // resolve pass
        {
          gl.glUseProgram(DebugData.quadoverdrawResolveProg);
          gl.glBindProgramPipeline(0);

          gl.glBindBufferBase(eGL_UNIFORM_BUFFER, 1, DebugData.UBOs[0]);

          Vec4f *v = (Vec4f *)gl.glMapBufferRange(eGL_UNIFORM_BUFFER, 0, sizeof(overdrawRamp),
                                                  GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
          memcpy(v, overdrawRamp, sizeof(overdrawRamp));
          gl.glUnmapBuffer(eGL_UNIFORM_BUFFER);

          // modify our fbo to attach the overlay texture instead
          gl.glBindFramebuffer(eGL_FRAMEBUFFER, replacefbo);
          gl.glFramebufferTexture(eGL_FRAMEBUFFER, eGL_COLOR_ATTACHMENT0, DebugData.overlayTex, 0);
          gl.glFramebufferTexture(eGL_FRAMEBUFFER, eGL_DEPTH_STENCIL_ATTACHMENT, 0, 0);

          gl.glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
          gl.glDisable(eGL_BLEND);
          gl.glDisable(eGL_SCISSOR_TEST);
          gl.glDepthMask(GL_FALSE);
          gl.glDisable(eGL_CULL_FACE);
          if(!IsGLES)
            gl.glPolygonMode(eGL_FRONT_AND_BACK, eGL_FILL);
          gl.glDisable(eGL_DEPTH_TEST);
          gl.glDisable(eGL_STENCIL_TEST);
          gl.glStencilMask(0);
          gl.glViewport(0, 0, texDetails.width, texDetails.height);

          gl.glBindImageTexture(0, quadtexs[2], 0, GL_FALSE, 0, eGL_READ_WRITE, eGL_R32UI);

          GLuint emptyVAO = 0;
          gl.glGenVertexArrays(1, &emptyVAO);
          gl.glBindVertexArray(emptyVAO);
          gl.glDrawArrays(eGL_TRIANGLE_STRIP, 0, 4);
          gl.glBindVertexArray(0);
          gl.glDeleteVertexArrays(1, &emptyVAO);

          gl.glFramebufferTexture(eGL_FRAMEBUFFER, eGL_COLOR_ATTACHMENT0, quadtexs[0], 0);
        }

        gl.glDeleteFramebuffers(1, &replacefbo);
        gl.glDeleteTextures(3, quadtexs);

        if(overlay == DebugOverlay::QuadOverdrawPass)
          ReplayLog(eventId, eReplay_WithoutDraw);
      }
    }
  }
  else
  {
    RDCERR(
        "Unexpected/unimplemented overlay type - should implement a placeholder overlay for all "
        "types");
  }

  rs.ApplyState(m_pDriver);

  return m_pDriver->GetResourceManager()->GetID(TextureRes(ctx, DebugData.overlayTex));
}
