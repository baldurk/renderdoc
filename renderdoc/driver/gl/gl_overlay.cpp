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
#include "data/glsl/glsl_ubos_cpp.h"

bool GLReplay::CreateOverlayProgram(GLuint Program, GLuint Pipeline, GLuint fragShader,
                                    GLuint fragShaderSPIRV)
{
  WrappedOpenGL &drv = *m_pDriver;

  ContextPair &ctx = drv.GetCtx();

  // delete the old program if it exists
  if(DebugData.overlayProg != 0)
    drv.glDeleteProgram(DebugData.overlayProg);

  DebugData.overlayProg = drv.glCreateProgram();

  // these are the shaders to attach, and the programs to copy details from
  GLuint shaders[4] = {0};
  GLuint programs[4] = {0};

  // temporary programs created as needed if the original program was created with
  // glCreateShaderProgramv and we don't have a shader to attach
  GLuint tmpShaders[4] = {0};

  // the reflection for the vertex shader, used to copy vertex bindings
  ShaderReflection *vsRefl = NULL;

  bool HasSPIRVShaders = false;
  bool HasGLSLShaders = false;

  if(Program == 0)
  {
    if(Pipeline == 0)
    {
      return false;
    }
    else
    {
      ResourceId id = m_pDriver->GetResourceManager()->GetID(ProgramPipeRes(ctx, Pipeline));
      const WrappedOpenGL::PipelineData &pipeDetails = m_pDriver->m_Pipelines[id];

      // fetch the corresponding shaders and programs for each stage
      for(size_t i = 0; i < 4; i++)
      {
        if(pipeDetails.stageShaders[i] != ResourceId())
        {
          const WrappedOpenGL::ShaderData &shadDetails =
              m_pDriver->m_Shaders[pipeDetails.stageShaders[i]];

          if(shadDetails.reflection.encoding == ShaderEncoding::SPIRV)
            HasSPIRVShaders = true;
          else
            HasGLSLShaders = true;

          programs[i] =
              m_pDriver->GetResourceManager()->GetCurrentResource(pipeDetails.stagePrograms[i]).name;
          shaders[i] =
              m_pDriver->GetResourceManager()->GetCurrentResource(pipeDetails.stageShaders[i]).name;

          if(pipeDetails.stagePrograms[i] == pipeDetails.stageShaders[i])
          {
            const WrappedOpenGL::ProgramData &progDetails =
                m_pDriver->m_Programs[pipeDetails.stagePrograms[i]];

            if(progDetails.shaderProgramUnlinkable)
            {
              std::vector<const char *> sources;
              sources.reserve(shadDetails.sources.size());

              for(const std::string &s : shadDetails.sources)
                sources.push_back(s.c_str());

              shaders[i] = tmpShaders[i] = drv.glCreateShader(ShaderEnum(i));
              drv.glShaderSource(tmpShaders[i], (GLsizei)sources.size(), sources.data(), NULL);
              drv.glCompileShader(tmpShaders[i]);

              GLint status = 0;
              drv.glGetShaderiv(tmpShaders[i], eGL_COMPILE_STATUS, &status);

              if(status == 0)
              {
                char buffer[1024] = {};
                drv.glGetShaderInfoLog(tmpShaders[i], 1024, NULL, buffer);
                RDCERR("Trying to recreate overlay program, couldn't compile shader:\n%s", buffer);
              }
            }
          }

          if(i == 0)
            vsRefl = GetShader(pipeDetails.stageShaders[i], ShaderEntryPoint());
        }
      }
    }
  }
  else
  {
    const WrappedOpenGL::ProgramData &progDetails =
        m_pDriver->m_Programs[m_pDriver->GetResourceManager()->GetID(ProgramRes(ctx, Program))];

    // fetch any and all non-fragment shader shaders
    for(size_t i = 0; i < 4; i++)
    {
      if(progDetails.stageShaders[i] != ResourceId())
      {
        programs[i] = Program;
        shaders[i] =
            m_pDriver->GetResourceManager()->GetCurrentResource(progDetails.stageShaders[i]).name;

        const WrappedOpenGL::ShaderData &shadDetails =
            m_pDriver->m_Shaders[progDetails.stageShaders[i]];

        if(shadDetails.reflection.encoding == ShaderEncoding::SPIRV)
          HasSPIRVShaders = true;
        else
          HasGLSLShaders = true;

        if(i == 0)
          vsRefl = GetShader(progDetails.stageShaders[0], ShaderEntryPoint());
      }
    }
  }

  if(HasGLSLShaders && HasSPIRVShaders)
    RDCERR("Unsupported - mixed GLSL and SPIR-V shaders in pipeline");

  // attach the shaders
  for(size_t i = 0; i < 4; i++)
    if(shaders[i])
      drv.glAttachShader(DebugData.overlayProg, shaders[i]);

  if(HasSPIRVShaders)
  {
    RDCASSERT(fragShaderSPIRV);
    drv.glAttachShader(DebugData.overlayProg, fragShaderSPIRV);
  }
  else
  {
    drv.glAttachShader(DebugData.overlayProg, fragShader);
  }

  // copy the vertex attribs over from the source program
  if(vsRefl && programs[0] && !HasSPIRVShaders)
    CopyProgramAttribBindings(programs[0], DebugData.overlayProg, vsRefl);

  // link the overlay program
  drv.glLinkProgram(DebugData.overlayProg);

  // detach the shaders
  for(size_t i = 0; i < 4; i++)
    if(shaders[i])
      drv.glDetachShader(DebugData.overlayProg, shaders[i]);

  if(HasSPIRVShaders)
    drv.glDetachShader(DebugData.overlayProg, fragShaderSPIRV);
  else
    drv.glDetachShader(DebugData.overlayProg, fragShader);

  // delete any temporaries
  for(size_t i = 0; i < 4; i++)
    if(tmpShaders[i])
      drv.glDeleteShader(tmpShaders[i]);

  // check that the link succeeded
  char buffer[1024] = {};
  GLint status = 0;
  drv.glGetProgramiv(DebugData.overlayProg, eGL_LINK_STATUS, &status);
  if(status == 0)
  {
    drv.glGetProgramInfoLog(DebugData.overlayProg, 1024, NULL, buffer);
    RDCERR("Error linking overlay program: %s", buffer);
    return false;
  }

  // copy the uniform values over from the source program. This is redundant but harmless if the
  // same program is bound to multiple stages. It's just inefficient
  {
    PerStageReflections dstStages;
    m_pDriver->FillReflectionArray(ProgramRes(ctx, DebugData.overlayProg), dstStages);

    for(size_t i = 0; i < 4; i++)
    {
      if(programs[i])
      {
        PerStageReflections stages;
        m_pDriver->FillReflectionArray(ProgramRes(ctx, programs[i]), stages);

        CopyProgramUniforms(stages, programs[i], dstStages, DebugData.overlayProg);
      }
    }
  }

  return HasSPIRVShaders;
}

ResourceId GLReplay::RenderOverlay(ResourceId texid, CompType typeHint, DebugOverlay overlay,
                                   uint32_t eventId, const std::vector<uint32_t> &passEvents)
{
  WrappedOpenGL &drv = *m_pDriver;

  MakeCurrentReplayContext(&m_ReplayCtx);

  GLMarkerRegion renderoverlay(StringFormat::Fmt("RenderOverlay %d", overlay));

  ContextPair &ctx = drv.GetCtx();

  GLRenderState rs;
  rs.FetchState(&drv);

  if(rs.Program.name == 0 && rs.Pipeline.name == 0)
    return ResourceId();

  // use our overlay program that we'll fill up with all the right
  // shaders, then replace the fragment shader with our own.
  drv.glBindProgramPipeline(0);

  if(DebugData.fixedcolFragShader)
    drv.glDeleteShader(DebugData.fixedcolFragShader);
  if(DebugData.quadoverdrawFragShader)
    drv.glDeleteShader(DebugData.quadoverdrawFragShader);

  DebugData.fixedcolFragShader = DebugData.quadoverdrawFragShader = 0;

  ShaderType shaderType;
  int glslVer;

  if(IsGLES)
  {
    shaderType = eShaderGLSLES;

    // default to 100 just in case something is broken
    glslVer = 100;

    // GLES requires that versions *precisely* match, so here we must figure out what version the
    // existing shader was using (we pick the vertex shader for simplicity since by definition it
    // must match any others) and recompile our shaders with that version. We've ensured they are
    // compatible with anything after the minimum version
    ResourceId vs;

    if(rs.Program.name)
      vs = m_pDriver->m_Programs[m_pDriver->GetResourceManager()->GetID(rs.Program)].stageShaders[0];
    else
      vs = m_pDriver->m_Pipelines[m_pDriver->GetResourceManager()->GetID(rs.Pipeline)].stageShaders[0];

    if(vs != ResourceId())
    {
      glslVer = m_pDriver->m_Shaders[vs].version;
      if(glslVer == 0)
        glslVer = 100;
    }
  }
  else
  {
    // Desktop GL is vastly better, it can link any program so we just default to 150
    glslVer = 150;
    shaderType = eShaderGLSL;
  }

  // this is always compatible.
  {
    std::string source =
        GenerateGLSLShader(GetEmbeddedResource(glsl_fixedcol_frag), shaderType, glslVer);
    DebugData.fixedcolFragShader = CreateShader(eGL_FRAGMENT_SHADER, source);
  }

  // this is not supported on GLES
  if(shaderType == eShaderGLSL)
  {
    std::string defines = "";

    if(!HasExt[ARB_derivative_control])
    {
      // dFdx fine functions not available before GLSL 450. Use normal dFdx, which might be coarse,
      // so won't show quad overdraw properly
      defines += "#define dFdxFine dFdx\n\n";
      defines += "#define dFdyFine dFdy\n\n";

      RDCWARN("Quad overdraw requires GLSL 4.50 for dFd(xy)fine, using possibly coarse dFd(xy).");
    }

    DebugData.quadoverdrawFragShader = 0;

    // needs these extensions
    if(HasExt[ARB_gpu_shader5] && HasExt[ARB_shader_image_load_store])
    {
      std::string source =
          GenerateGLSLShader(GetEmbeddedResource(glsl_quadwrite_frag), shaderType, glslVer, defines);
      DebugData.quadoverdrawFragShader = CreateShader(eGL_FRAGMENT_SHADER, source);

      // we expect if the SPIR-V extension is present then we've compiled this variant.
      if(HasExt[ARB_gl_spirv])
      {
        RDCASSERT(DebugData.quadoverdrawFragShaderSPIRV);
      }
    }
  }
  else
  {
    if(overlay == DebugOverlay::QuadOverdrawDraw || overlay == DebugOverlay::QuadOverdrawPass)
      RDCWARN("Quad overdraw not supported on GLES", glslVer);
  }

  // we bind the separable program created for each shader, and copy
  // uniforms and attrib bindings from the 'real' programs, wherever
  // they are.
  bool spirvOverlay =
      CreateOverlayProgram(rs.Program.name, rs.Pipeline.name, DebugData.fixedcolFragShader,
                           DebugData.fixedcolFragShaderSPIRV);
  drv.glUseProgram(DebugData.overlayProg);

  GLint overlayFixedColLocation = 0;

  // on SPIR-V overlays we don't query the location, it's baked into the shader
  if(spirvOverlay)
    overlayFixedColLocation = 99;
  else
    overlayFixedColLocation =
        drv.glGetUniformLocation(DebugData.overlayProg, "RENDERDOC_Fixed_Color");

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
      drv.glDeleteFramebuffers(1, &DebugData.overlayFBO);
      drv.glDeleteTextures(1, &DebugData.overlayTex);
    }

    drv.glGenFramebuffers(1, &DebugData.overlayFBO);
    drv.glBindFramebuffer(eGL_FRAMEBUFFER, DebugData.overlayFBO);

    GLuint curTex = 0;
    drv.glGetIntegerv(texQueryEnum, (GLint *)&curTex);

    drv.glGenTextures(1, &DebugData.overlayTex);
    drv.glBindTexture(texBindingEnum, DebugData.overlayTex);

    DebugData.overlayTexWidth = texDetails.width;
    DebugData.overlayTexHeight = texDetails.height;
    DebugData.overlayTexSamples = texDetails.samples;

    if(DebugData.overlayTexSamples > 1)
    {
      drv.glTextureStorage2DMultisampleEXT(DebugData.overlayTex, texBindingEnum, texDetails.samples,
                                           eGL_RGBA16F, texDetails.width, texDetails.height, true);
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

      drv.glTextureImage2DEXT(DebugData.overlayTex, texBindingEnum, 0, internalFormat,
                              texDetails.width, texDetails.height, 0, format, type, NULL);
      drv.glTexParameteri(texBindingEnum, eGL_TEXTURE_MAX_LEVEL, 0);
      drv.glTexParameteri(texBindingEnum, eGL_TEXTURE_MIN_FILTER, eGL_NEAREST);
      drv.glTexParameteri(texBindingEnum, eGL_TEXTURE_MAG_FILTER, eGL_NEAREST);
      drv.glTexParameteri(texBindingEnum, eGL_TEXTURE_WRAP_S, eGL_CLAMP_TO_EDGE);
      drv.glTexParameteri(texBindingEnum, eGL_TEXTURE_WRAP_T, eGL_CLAMP_TO_EDGE);
    }
    drv.glFramebufferTexture2D(eGL_FRAMEBUFFER, eGL_COLOR_ATTACHMENT0, texBindingEnum,
                               DebugData.overlayTex, 0);

    drv.glBindTexture(texBindingEnum, curTex);
  }

  drv.glBindFramebuffer(eGL_FRAMEBUFFER, DebugData.overlayFBO);

  // disable several tests/allow rendering - some overlays will override
  // these states but commonly we don't want to inherit these states from
  // the program's state.
  drv.glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  drv.glDisable(eGL_BLEND);
  drv.glDisable(eGL_SCISSOR_TEST);
  drv.glDepthMask(GL_FALSE);
  drv.glDisable(eGL_CULL_FACE);
  drv.glDisable(eGL_DEPTH_TEST);
  drv.glDisable(eGL_STENCIL_TEST);
  drv.glStencilMask(0);
  if(!IsGLES)
  {
    drv.glPolygonMode(eGL_FRONT_AND_BACK, eGL_FILL);
    drv.glEnable(eGL_DEPTH_CLAMP);
  }

  if(overlay == DebugOverlay::NaN || overlay == DebugOverlay::Clipping)
  {
    // just need the basic texture
    float black[] = {0.0f, 0.0f, 0.0f, 0.0f};
    drv.glClearBufferfv(eGL_COLOR, 0, black);
  }
  else if(overlay == DebugOverlay::Drawcall)
  {
    float black[] = {0.0f, 0.0f, 0.0f, 0.5f};
    drv.glClearBufferfv(eGL_COLOR, 0, black);

    float colVal[] = {0.8f, 0.1f, 0.8f, 1.0f};
    drv.glProgramUniform4fv(DebugData.overlayProg, overlayFixedColLocation, 1, colVal);

    ReplayLog(eventId, eReplay_OnlyDraw);
  }
  else if(overlay == DebugOverlay::Wireframe)
  {
    float wireCol[] = {200.0f / 255.0f, 255.0f / 255.0f, 0.0f / 255.0f, 0.0f};
    drv.glClearBufferfv(eGL_COLOR, 0, wireCol);

    wireCol[3] = 1.0f;
    drv.glProgramUniform4fv(DebugData.overlayProg, overlayFixedColLocation, 1, wireCol);

    if(!IsGLES)
    {
      // desktop GL is simple
      drv.glPolygonMode(eGL_FRONT_AND_BACK, eGL_LINE);

      ReplayLog(eventId, eReplay_OnlyDraw);
    }
    else
    {
      // GLES is hard. We need to readback the index buffer, convert it to a line list, then draw
      // with that. We can at least use a client-side pointer for the index buffer.
      GLint idxbuf = 0;
      drv.glGetIntegerv(eGL_ELEMENT_ARRAY_BUFFER_BINDING, &idxbuf);

      const DrawcallDescription *draw = m_pDriver->GetDrawcall(eventId);

      std::vector<uint32_t> patchedIndices;

      // readback the index buffer data
      if(idxbuf)
      {
        std::vector<byte> idxs;
        uint32_t offset = draw->indexOffset * draw->indexByteWidth;
        uint32_t length = 1;
        drv.glGetNamedBufferParameterivEXT(idxbuf, eGL_BUFFER_SIZE, (GLint *)&length);

        idxs.resize(length);
        drv.glGetBufferSubData(
            eGL_ELEMENT_ARRAY_BUFFER, offset,
            RDCMIN(GLsizeiptr(length - offset),
                   GLsizeiptr(draw->numIndices) * GLsizeiptr(draw->indexByteWidth)),
            &idxs[0]);

        // unbind the real index buffer
        drv.glBindBuffer(eGL_ELEMENT_ARRAY_BUFFER, 0);

        uint32_t expectedSize = draw->numIndices * draw->indexByteWidth;

        if(idxs.size() < expectedSize)
        {
          RDCERR("Index buffer is as large as expected");
          idxs.resize(expectedSize);
        }

        PatchLineStripIndexBuffer(
            draw, draw->indexByteWidth == 1 ? (uint8_t *)idxs.data() : (uint8_t *)NULL,
            draw->indexByteWidth == 2 ? (uint16_t *)idxs.data() : (uint16_t *)NULL,
            draw->indexByteWidth == 4 ? (uint32_t *)idxs.data() : (uint32_t *)NULL, patchedIndices);
      }
      else
      {
        // generate 'index' list
        std::vector<uint32_t> idxs;
        idxs.resize(draw->numIndices);
        for(uint32_t i = 0; i < draw->numIndices; i++)
          idxs[i] = i;
        PatchLineStripIndexBuffer(draw, NULL, NULL, idxs.data(), patchedIndices);
      }

      GLboolean primRestart = drv.glIsEnabled(eGL_PRIMITIVE_RESTART_FIXED_INDEX);
      drv.glEnable(eGL_PRIMITIVE_RESTART_FIXED_INDEX);

      if(draw->flags & DrawFlags::Instanced)
      {
        if(HasExt[ARB_base_instance])
        {
          drv.glDrawElementsInstancedBaseVertexBaseInstance(
              eGL_LINE_STRIP, (GLsizei)patchedIndices.size(), eGL_UNSIGNED_INT,
              patchedIndices.data(), draw->numInstances, 0, draw->instanceOffset);
        }
        else
        {
          drv.glDrawElementsInstancedBaseVertex(eGL_LINE_STRIP, (GLsizei)patchedIndices.size(),
                                                eGL_UNSIGNED_INT, patchedIndices.data(),
                                                draw->numInstances, 0);
        }
      }
      else
      {
        drv.glDrawElementsBaseVertex(eGL_LINE_STRIP, (GLsizei)patchedIndices.size(),
                                     eGL_UNSIGNED_INT, patchedIndices.data(), 0);
      }

      if(!primRestart)
        drv.glDisable(eGL_PRIMITIVE_RESTART_FIXED_INDEX);

      drv.glBindBuffer(eGL_ELEMENT_ARRAY_BUFFER, idxbuf);
    }
  }
  else if(overlay == DebugOverlay::ViewportScissor)
  {
    float col[] = {0.0f, 0.0f, 0.0f, 0.0f};
    drv.glClearBufferfv(eGL_COLOR, 0, col);

    // don't need to use the existing program at all!
    drv.glUseProgram(DebugData.checkerProg);
    drv.glBindProgramPipeline(0);

    if(HasExt[ARB_viewport_array])
    {
      drv.glDisablei(eGL_SCISSOR_TEST, 0);

      drv.glViewportIndexedf(0, rs.Viewports[0].x, rs.Viewports[0].y, rs.Viewports[0].width,
                             rs.Viewports[0].height);
    }
    else
    {
      drv.glDisable(eGL_SCISSOR_TEST);

      drv.glViewport((GLint)rs.Viewports[0].x, (GLint)rs.Viewports[0].y,
                     (GLsizei)rs.Viewports[0].width, (GLsizei)rs.Viewports[0].height);
    }

    if(HasExt[ARB_draw_buffers_blend])
    {
      drv.glEnablei(eGL_BLEND, 0);

      drv.glBlendFuncSeparatei(0, eGL_SRC_ALPHA, eGL_ONE_MINUS_SRC_ALPHA, eGL_SRC_ALPHA,
                               eGL_ONE_MINUS_SRC_ALPHA);
      drv.glBlendEquationSeparatei(0, eGL_FUNC_ADD, eGL_FUNC_ADD);
    }
    else
    {
      drv.glBlendFuncSeparate(eGL_SRC_ALPHA, eGL_ONE_MINUS_SRC_ALPHA, eGL_SRC_ALPHA,
                              eGL_ONE_MINUS_SRC_ALPHA);
      drv.glBlendEquationSeparate(eGL_FUNC_ADD, eGL_FUNC_ADD);

      drv.glEnable(eGL_BLEND);
    }

    drv.glBlendColor(1.0f, 1.0f, 1.0f, 1.0f);

    drv.glBindBufferBase(eGL_UNIFORM_BUFFER, 0, DebugData.UBOs[0]);
    CheckerboardUBOData *cdata = (CheckerboardUBOData *)drv.glMapBufferRange(
        eGL_UNIFORM_BUFFER, 0, sizeof(CheckerboardUBOData),
        GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);

    cdata->BorderWidth = 3;
    cdata->CheckerSquareDimension = 16.0f;

    // set primary/secondary to the same to 'disable' checkerboard
    cdata->PrimaryColor = cdata->SecondaryColor = Vec4f(0.1f, 0.1f, 0.1f, 1.0f);
    cdata->InnerColor = Vec4f(0.2f, 0.2f, 0.9f, 0.7f);

    // set viewport rect
    cdata->RectPosition = Vec2f(rs.Viewports[0].x, rs.Viewports[0].y);
    cdata->RectSize = Vec2f(rs.Viewports[0].width, rs.Viewports[0].height);

    drv.glUnmapBuffer(eGL_UNIFORM_BUFFER);

    drv.glDrawArrays(eGL_TRIANGLE_STRIP, 0, 4);

    if(rs.Scissors[0].enabled)
    {
      Vec4f scissor((float)rs.Scissors[0].x, (float)rs.Scissors[0].y, (float)rs.Scissors[0].width,
                    (float)rs.Scissors[0].height);

      if(HasExt[ARB_viewport_array])
        drv.glViewportIndexedf(0, scissor.x, scissor.y, scissor.z, scissor.w);
      else
        drv.glViewport(rs.Scissors[0].x, rs.Scissors[0].y, rs.Scissors[0].width,
                       rs.Scissors[0].height);

      cdata = (CheckerboardUBOData *)drv.glMapBufferRange(
          eGL_UNIFORM_BUFFER, 0, sizeof(CheckerboardUBOData),
          GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);

      cdata->BorderWidth = 3;
      cdata->CheckerSquareDimension = 16.0f;

      // black/white checkered border
      cdata->PrimaryColor = Vec4f(1.0f, 1.0f, 1.0f, 1.0f);
      cdata->SecondaryColor = Vec4f(0.0f, 0.0f, 0.0f, 1.0f);

      // nothing at all inside
      cdata->InnerColor = Vec4f(0.0f, 0.0f, 0.0f, 0.0f);

      cdata->RectPosition = Vec2f(scissor.x, scissor.y);
      cdata->RectSize = Vec2f(scissor.z, scissor.w);

      drv.glUnmapBuffer(eGL_UNIFORM_BUFFER);

      drv.glDrawArrays(eGL_TRIANGLE_STRIP, 0, 4);
    }
  }
  else if(overlay == DebugOverlay::Depth || overlay == DebugOverlay::Stencil)
  {
    float black[] = {0.0f, 0.0f, 0.0f, 0.0f};
    drv.glClearBufferfv(eGL_COLOR, 0, black);

    float red[] = {1.0f, 0.0f, 0.0f, 1.0f};
    drv.glProgramUniform4fv(DebugData.overlayProg, overlayFixedColLocation, 1, red);

    ReplayLog(eventId, eReplay_OnlyDraw);

    GLuint curDepth = 0, curStencil = 0;

    drv.glGetNamedFramebufferAttachmentParameterivEXT(rs.DrawFBO.name, eGL_DEPTH_ATTACHMENT,
                                                      eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME,
                                                      (GLint *)&curDepth);
    drv.glGetNamedFramebufferAttachmentParameterivEXT(rs.DrawFBO.name, eGL_STENCIL_ATTACHMENT,
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
      drv.glGetNamedFramebufferAttachmentParameterivEXT(
          rs.DrawFBO.name, eGL_DEPTH_ATTACHMENT, eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &type);

      GLenum fmt;

      if(type != eGL_RENDERBUFFER)
      {
        ResourceId id = m_pDriver->GetResourceManager()->GetID(TextureRes(ctx, curDepth));
        WrappedOpenGL::TextureData &details = m_pDriver->m_Textures[id];

        fmt = details.internalFormat;

        drv.glGetNamedFramebufferAttachmentParameterivEXT(
            rs.DrawFBO.name, eGL_DEPTH_ATTACHMENT, eGL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL, &mip);

        if(details.curType == eGL_TEXTURE_CUBE_MAP)
        {
          GLenum face;
          drv.glGetNamedFramebufferAttachmentParameterivEXT(
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
      drv.glGetIntegerv(copyQueryEnum, (GLint *)&curTex);

      drv.glGenTextures(1, &depthCopy);
      drv.glBindTexture(copyBindingEnum, depthCopy);
      if(DebugData.overlayTexSamples > 1)
      {
        drv.glTextureStorage2DMultisampleEXT(depthCopy, copyBindingEnum, DebugData.overlayTexSamples,
                                             fmt, DebugData.overlayTexWidth,
                                             DebugData.overlayTexHeight, true);
      }
      else
      {
        drv.glTextureImage2DEXT(depthCopy, copyBindingEnum, 0, fmt, DebugData.overlayTexWidth,
                                DebugData.overlayTexHeight, 0, GetBaseFormat(fmt), GetDataType(fmt),
                                NULL);
        drv.glTexParameteri(copyBindingEnum, eGL_TEXTURE_MAX_LEVEL, 0);
        drv.glTexParameteri(copyBindingEnum, eGL_TEXTURE_MIN_FILTER, eGL_NEAREST);
        drv.glTexParameteri(copyBindingEnum, eGL_TEXTURE_MAG_FILTER, eGL_NEAREST);
        drv.glTexParameteri(copyBindingEnum, eGL_TEXTURE_WRAP_S, eGL_CLAMP_TO_EDGE);
        drv.glTexParameteri(copyBindingEnum, eGL_TEXTURE_WRAP_T, eGL_CLAMP_TO_EDGE);
      }

      drv.glBindTexture(copyBindingEnum, curTex);
    }

    // create matching separate stencil if relevant
    if(curStencil != curDepth && curStencil != 0)
    {
      GLint type = 0;
      drv.glGetNamedFramebufferAttachmentParameterivEXT(
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
          drv.glGetNamedFramebufferAttachmentParameterivEXT(
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
      drv.glGetIntegerv(copyQueryEnum, (GLint *)&curTex);

      drv.glGenTextures(1, &stencilCopy);
      drv.glBindTexture(copyBindingEnum, stencilCopy);
      if(DebugData.overlayTexSamples > 1)
      {
        drv.glTextureStorage2DMultisampleEXT(
            stencilCopy, copyBindingEnum, DebugData.overlayTexSamples, fmt,
            DebugData.overlayTexWidth, DebugData.overlayTexHeight, true);
      }
      else
      {
        drv.glTextureImage2DEXT(stencilCopy, copyBindingEnum, 0, fmt, DebugData.overlayTexWidth,
                                DebugData.overlayTexHeight, 0, GetBaseFormat(fmt), GetDataType(fmt),
                                NULL);
        drv.glTexParameteri(copyBindingEnum, eGL_TEXTURE_MAX_LEVEL, 0);
        drv.glTexParameteri(copyBindingEnum, eGL_TEXTURE_MIN_FILTER, eGL_NEAREST);
        drv.glTexParameteri(copyBindingEnum, eGL_TEXTURE_MAG_FILTER, eGL_NEAREST);
        drv.glTexParameteri(copyBindingEnum, eGL_TEXTURE_WRAP_S, eGL_CLAMP_TO_EDGE);
        drv.glTexParameteri(copyBindingEnum, eGL_TEXTURE_WRAP_T, eGL_CLAMP_TO_EDGE);
      }

      drv.glBindTexture(copyBindingEnum, curTex);
    }

    // bind depth/stencil to overlay FBO (currently bound to DRAW_FRAMEBUFFER)
    if(curDepth != 0 && curDepth == curStencil)
    {
      if(layer == 0)
        drv.glFramebufferTexture2D(eGL_DRAW_FRAMEBUFFER, eGL_DEPTH_STENCIL_ATTACHMENT,
                                   eGL_TEXTURE_2D, depthCopy, mip);
      else
        drv.glFramebufferTextureLayer(eGL_DRAW_FRAMEBUFFER, eGL_DEPTH_STENCIL_ATTACHMENT, depthCopy,
                                      mip, layer);
    }
    else if(curDepth != 0)
    {
      if(layer == 0)
        drv.glFramebufferTexture2D(eGL_DRAW_FRAMEBUFFER, eGL_DEPTH_ATTACHMENT, eGL_TEXTURE_2D,
                                   depthCopy, mip);
      else
        drv.glFramebufferTextureLayer(eGL_DRAW_FRAMEBUFFER, eGL_DEPTH_ATTACHMENT, depthCopy, mip,
                                      layer);
    }
    else if(curStencil != 0)
    {
      if(layer == 0)
        drv.glFramebufferTexture2D(eGL_DRAW_FRAMEBUFFER, eGL_STENCIL_ATTACHMENT, eGL_TEXTURE_2D,
                                   stencilCopy, mip);
      else
        drv.glFramebufferTextureLayer(eGL_DRAW_FRAMEBUFFER, eGL_STENCIL_ATTACHMENT, stencilCopy,
                                      mip, layer);
    }

    // bind the 'real' fbo to the read framebuffer, so we can blit from it
    drv.glBindFramebuffer(eGL_READ_FRAMEBUFFER, rs.DrawFBO.name);

    float green[] = {0.0f, 1.0f, 0.0f, 1.0f};
    drv.glProgramUniform4fv(DebugData.overlayProg, overlayFixedColLocation, 1, green);

    if(overlay == DebugOverlay::Depth)
    {
      if(rs.Enabled[GLRenderState::eEnabled_DepthTest])
        drv.glEnable(eGL_DEPTH_TEST);
      else
        drv.glDisable(eGL_DEPTH_TEST);

      if(rs.DepthWriteMask)
        drv.glDepthMask(GL_TRUE);
      else
        drv.glDepthMask(GL_FALSE);
    }
    else
    {
      if(rs.Enabled[GLRenderState::eEnabled_StencilTest])
        drv.glEnable(eGL_STENCIL_TEST);
      else
        drv.glDisable(eGL_STENCIL_TEST);

      drv.glStencilMaskSeparate(eGL_FRONT, rs.StencilFront.writemask);
      drv.glStencilMaskSeparate(eGL_BACK, rs.StencilBack.writemask);
    }

    if(rs.Enabled[GLRenderState::eEnabled_CullFace])
      drv.glEnable(eGL_CULL_FACE);
    if(!IsGLES && !rs.Enabled[GLRenderState::eEnabled_DepthClamp])
      drv.glDisable(eGL_DEPTH_CLAMP);

    // get latest depth/stencil from read FBO (existing FBO) into draw FBO (overlay FBO)
    SafeBlitFramebuffer(0, 0, DebugData.overlayTexWidth, DebugData.overlayTexHeight, 0, 0,
                        DebugData.overlayTexWidth, DebugData.overlayTexHeight,
                        GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT, eGL_NEAREST);

    ReplayLog(eventId, eReplay_OnlyDraw);

    // unset depth/stencil textures from overlay FBO and delete temp depth/stencil
    if(curDepth != 0 && curDepth == curStencil)
      drv.glFramebufferTexture2D(eGL_DRAW_FRAMEBUFFER, eGL_DEPTH_STENCIL_ATTACHMENT, eGL_TEXTURE_2D,
                                 0, 0);
    else if(curDepth != 0)
      drv.glFramebufferTexture2D(eGL_DRAW_FRAMEBUFFER, eGL_DEPTH_ATTACHMENT, eGL_TEXTURE_2D, 0, 0);
    else if(curStencil != 0)
      drv.glFramebufferTexture2D(eGL_DRAW_FRAMEBUFFER, eGL_STENCIL_ATTACHMENT, eGL_TEXTURE_2D, 0, 0);
    if(depthCopy != 0)
      drv.glDeleteTextures(1, &depthCopy);
    if(stencilCopy != 0)
      drv.glDeleteTextures(1, &stencilCopy);
  }
  else if(overlay == DebugOverlay::BackfaceCull)
  {
    float col[] = {0.0f, 0.0f, 0.0f, 0.0f};
    drv.glClearBufferfv(eGL_COLOR, 0, col);

    col[0] = 1.0f;
    col[3] = 1.0f;

    drv.glProgramUniform4fv(DebugData.overlayProg, overlayFixedColLocation, 1, col);

    ReplayLog(eventId, eReplay_OnlyDraw);

    // only enable cull face if it was enabled originally (otherwise
    // we just render green over the exact same area, so it shows up "passing")
    if(rs.Enabled[GLRenderState::eEnabled_CullFace])
      drv.glEnable(eGL_CULL_FACE);

    col[0] = 0.0f;
    col[1] = 1.0f;

    drv.glProgramUniform4fv(DebugData.overlayProg, overlayFixedColLocation, 1, col);

    ReplayLog(eventId, eReplay_OnlyDraw);
  }
  else if(overlay == DebugOverlay::ClearBeforeDraw || overlay == DebugOverlay::ClearBeforePass)
  {
    float col[] = {0.0f, 0.0f, 0.0f, 0.0f};
    drv.glClearBufferfv(eGL_COLOR, 0, col);

    std::vector<uint32_t> events = passEvents;

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
        rs.ApplyState(&drv);
      }

      float black[] = {0.0f, 0.0f, 0.0f, 0.0f};
      for(int i = 0; i < 8; i++)
        drv.glClearBufferfv(eGL_COLOR, i, black);

      for(size_t i = 0; i < events.size(); i++)
      {
        m_pDriver->ReplayLog(events[i], events[i], eReplay_OnlyDraw);

        if(overlay == DebugOverlay::ClearBeforePass && i + 1 < events.size())
          m_pDriver->ReplayLog(events[i] + 1, events[i + 1], eReplay_WithoutDraw);
      }
    }
  }
  else if(overlay == DebugOverlay::TriangleSizeDraw || overlay == DebugOverlay::TriangleSizePass)
  {
    SCOPED_TIMER("Triangle Size");

    float black[] = {0.0f, 0.0f, 0.0f, 0.0f};
    drv.glClearBufferfv(eGL_COLOR, 0, black);

    MeshUBOData uboParams = {};
    uboParams.homogenousInput = 1;
    uboParams.invProj = Matrix4f::Identity();
    uboParams.mvp = Matrix4f::Identity();

    drv.glBindBuffer(eGL_COPY_WRITE_BUFFER, DebugData.UBOs[0]);

    MeshUBOData *uboptr =
        (MeshUBOData *)drv.glMapBufferRange(eGL_COPY_WRITE_BUFFER, 0, sizeof(MeshUBOData),
                                            GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
    *uboptr = uboParams;
    drv.glUnmapBuffer(eGL_COPY_WRITE_BUFFER);

    drv.glBindBuffer(eGL_COPY_WRITE_BUFFER, DebugData.UBOs[2]);
    Vec4f *v = (Vec4f *)drv.glMapBufferRange(eGL_COPY_WRITE_BUFFER, 0, sizeof(Vec4f),
                                             GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
    *v = Vec4f(rs.Viewports[0].width, rs.Viewports[0].height);
    drv.glUnmapBuffer(eGL_COPY_WRITE_BUFFER);

    std::vector<uint32_t> events = passEvents;

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
      drv.glGenFramebuffers(1, &overlayFBO);
      drv.glGenVertexArrays(1, &tempVAO);

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
          drv.glGetIntegerv(eGL_DEPTH_WRITEMASK, &depthwritemask);
          drv.glGetIntegerv(eGL_STENCIL_WRITEMASK, &stencilfmask);
          drv.glGetIntegerv(eGL_STENCIL_BACK_WRITEMASK, &stencilbmask);

          drv.glGetIntegerv(eGL_DRAW_FRAMEBUFFER_BINDING, (GLint *)&drawFBO);
          drv.glGetIntegerv(eGL_VERTEX_ARRAY_BINDING, (GLint *)&prevVAO);

          blending = drv.glIsEnabled(eGL_BLEND);

          for(uint32_t u = 0; u < 3; u++)
          {
            drv.glGetIntegeri_v(eGL_UNIFORM_BUFFER_BINDING, u, (GLint *)&ubos[u].buf);
            drv.glGetInteger64i_v(eGL_UNIFORM_BUFFER_START, u, (GLint64 *)&ubos[u].offs);
            drv.glGetInteger64i_v(eGL_UNIFORM_BUFFER_SIZE, u, (GLint64 *)&ubos[u].size);
          }
        }

        // disable depth and stencil writes
        drv.glDepthMask(GL_FALSE);
        drv.glStencilMask(GL_FALSE);

        // disable blending
        drv.glDisable(eGL_BLEND);

        // bind our UBOs
        drv.glBindBufferBase(eGL_UNIFORM_BUFFER, 0, DebugData.UBOs[0]);
        drv.glBindBufferBase(eGL_UNIFORM_BUFFER, 2, DebugData.UBOs[2]);

        GLenum att = eGL_DEPTH_ATTACHMENT;
        GLuint depthObj = 0, stencilObj = 0;
        GLint type = 0, level = 0, layered = 0, layer = 0;

        // do we have a stencil object?
        drv.glGetNamedFramebufferAttachmentParameterivEXT(drawFBO, eGL_STENCIL_ATTACHMENT,
                                                          eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME,
                                                          (GLint *)&stencilObj);

        // fetch the details of the 'real' depth attachment
        drv.glGetNamedFramebufferAttachmentParameterivEXT(
            drawFBO, att, eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, (GLint *)&depthObj);
        drv.glGetNamedFramebufferAttachmentParameterivEXT(
            drawFBO, att, eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &type);

        if(depthObj && stencilObj)
        {
          att = eGL_DEPTH_STENCIL_ATTACHMENT;
        }
        else if(depthObj == 0 && stencilObj)
        {
          att = eGL_STENCIL_ATTACHMENT;
          depthObj = stencilObj;
        }

        if(depthObj && type != eGL_RENDERBUFFER)
        {
          drv.glGetNamedFramebufferAttachmentParameterivEXT(
              drawFBO, att, eGL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL, &level);
          drv.glGetNamedFramebufferAttachmentParameterivEXT(
              drawFBO, att, eGL_FRAMEBUFFER_ATTACHMENT_LAYERED, &layered);

          layered = (layered != 0);

          layer = 0;
          if(layered == 0)
            drv.glGetNamedFramebufferAttachmentParameterivEXT(
                drawFBO, att, eGL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LAYER, &layer);

          ResourceId id = m_pDriver->GetResourceManager()->GetID(TextureRes(ctx, depthObj));
          WrappedOpenGL::TextureData &details = m_pDriver->m_Textures[id];

          if(details.curType == eGL_TEXTURE_CUBE_MAP)
          {
            GLenum face;
            drv.glGetNamedFramebufferAttachmentParameterivEXT(
                drawFBO, att, eGL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE, (GLint *)&face);

            layer = CubeTargetIndex(face);
          }
        }

        // bind our FBO
        drv.glBindFramebuffer(eGL_DRAW_FRAMEBUFFER, overlayFBO);
        drv.glFramebufferTexture2D(eGL_FRAMEBUFFER, eGL_COLOR_ATTACHMENT0, texBindingEnum,
                                   DebugData.overlayTex, 0);

        // now apply the depth texture binding
        if(depthObj)
        {
          if(type == eGL_RENDERBUFFER)
          {
            drv.glNamedFramebufferRenderbufferEXT(overlayFBO, att, eGL_RENDERBUFFER, depthObj);
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
                  drv.glFramebufferTexture2D(eGL_DRAW_FRAMEBUFFER, att, faces[layer], depthObj,
                                             level);
                }
                else
                {
                  RDCWARN(
                      "Invalid layer %u used to bind cubemap to framebuffer. Binding POSITIVE_X");
                  drv.glFramebufferTexture2D(eGL_DRAW_FRAMEBUFFER, att, faces[0], depthObj, level);
                }
              }
              else if(details.curType == eGL_TEXTURE_CUBE_MAP_ARRAY ||
                      details.curType == eGL_TEXTURE_1D_ARRAY ||
                      details.curType == eGL_TEXTURE_2D_ARRAY)
              {
                drv.glFramebufferTextureLayer(eGL_DRAW_FRAMEBUFFER, att, depthObj, level, layer);
              }
              else
              {
                RDCASSERT(layer == 0);
                drv.glNamedFramebufferTextureEXT(overlayFBO, att, depthObj, level);
              }
            }
            else
            {
              drv.glNamedFramebufferTextureEXT(overlayFBO, att, depthObj, level);
            }
          }
        }

        GLuint prog = 0, pipe = 0;
        drv.glGetIntegerv(eGL_CURRENT_PROGRAM, (GLint *)&prog);
        drv.glGetIntegerv(eGL_PROGRAM_PIPELINE_BINDING, (GLint *)&pipe);

        drv.glUseProgram(DebugData.trisizeProg);
        drv.glBindProgramPipeline(0);

        const DrawcallDescription *draw = m_pDriver->GetDrawcall(events[i]);

        for(uint32_t inst = 0; draw && inst < RDCMAX(1U, draw->numInstances); inst++)
        {
          MeshFormat postvs = GetPostVSBuffers(events[i], inst, 0, MeshDataStage::GSOut);
          if(postvs.vertexResourceId == ResourceId())
            postvs = GetPostVSBuffers(events[i], inst, 0, MeshDataStage::VSOut);

          if(postvs.vertexResourceId != ResourceId())
          {
            GLenum topo = MakeGLPrimitiveTopology(postvs.topology);

            drv.glBindVertexArray(tempVAO);

            {
              if(postvs.format.Special())
              {
                if(postvs.format.type == ResourceFormatType::R10G10B10A2)
                {
                  if(postvs.format.compType == CompType::UInt)
                    drv.glVertexAttribIFormat(0, 4, eGL_UNSIGNED_INT_2_10_10_10_REV, 0);
                  if(postvs.format.compType == CompType::SInt)
                    drv.glVertexAttribIFormat(0, 4, eGL_INT_2_10_10_10_REV, 0);
                }
                else if(postvs.format.type == ResourceFormatType::R11G11B10)
                {
                  drv.glVertexAttribFormat(0, 4, eGL_UNSIGNED_INT_10F_11F_11F_REV, GL_FALSE, 0);
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

                drv.glVertexAttribFormat(0, postvs.format.compCount, fmttype,
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

                drv.glVertexAttribIFormat(0, postvs.format.compCount, fmttype, 0);
              }
              else if(postvs.format.compType == CompType::Double)
              {
                drv.glVertexAttribLFormat(0, postvs.format.compCount, eGL_DOUBLE, 0);
              }

              GLuint vb =
                  m_pDriver->GetResourceManager()->GetCurrentResource(postvs.vertexResourceId).name;
              drv.glBindVertexBuffer(0, vb, (GLintptr)postvs.vertexByteOffset,
                                     postvs.vertexByteStride);
            }

            drv.glEnableVertexAttribArray(0);
            drv.glDisableVertexAttribArray(1);

            if(postvs.indexByteStride)
            {
              GLenum idxtype = eGL_UNSIGNED_BYTE;
              if(postvs.indexByteStride == 2)
                idxtype = eGL_UNSIGNED_SHORT;
              else if(postvs.indexByteStride == 4)
                idxtype = eGL_UNSIGNED_INT;

              GLuint ib =
                  m_pDriver->GetResourceManager()->GetCurrentResource(postvs.indexResourceId).name;
              drv.glBindBuffer(eGL_ELEMENT_ARRAY_BUFFER, ib);
              drv.glDrawElementsBaseVertex(topo, postvs.numIndices, idxtype,
                                           (const void *)uintptr_t(postvs.indexByteOffset),
                                           postvs.baseVertex);
            }
            else
            {
              drv.glDrawArrays(topo, 0, postvs.numIndices);
            }
          }
        }

        // pop the state that we messed with
        {
          drv.glBindProgramPipeline(pipe);
          drv.glUseProgram(prog);

          if(blending)
            drv.glEnable(eGL_BLEND);
          else
            drv.glDisable(eGL_BLEND);

          // restore the previous FBO/VAO
          drv.glBindFramebuffer(eGL_DRAW_FRAMEBUFFER, drawFBO);
          drv.glBindVertexArray(prevVAO);

          for(uint32_t u = 0; u < 3; u++)
          {
            if(ubos[u].buf == 0 || (ubos[u].offs == 0 && ubos[u].size == 0))
              drv.glBindBufferBase(eGL_UNIFORM_BUFFER, u, ubos[u].buf);
            else
              drv.glBindBufferRange(eGL_UNIFORM_BUFFER, u, ubos[u].buf, (GLintptr)ubos[u].offs,
                                    (GLsizeiptr)ubos[u].size);
          }

          drv.glDepthMask(depthwritemask ? GL_TRUE : GL_FALSE);
          drv.glStencilMaskSeparate(eGL_FRONT, (GLuint)stencilfmask);
          drv.glStencilMaskSeparate(eGL_BACK, (GLuint)stencilbmask);
        }

        if(overlay == DebugOverlay::TriangleSizePass)
        {
          m_pDriver->ReplayLog(0, events[i], eReplay_OnlyDraw);

          if(i + 1 < events.size())
            m_pDriver->ReplayLog(events[i] + 1, events[i + 1], eReplay_WithoutDraw);
        }
      }

      drv.glDeleteFramebuffers(1, &overlayFBO);
      drv.glDeleteVertexArrays(1, &tempVAO);

      if(overlay == DebugOverlay::TriangleSizePass)
        ReplayLog(eventId, eReplay_WithoutDraw);
    }
  }
  else if(overlay == DebugOverlay::QuadOverdrawDraw || overlay == DebugOverlay::QuadOverdrawPass)
  {
    if(DebugData.quadoverdrawFragShader)
    {
      SCOPED_TIMER("Quad Overdraw");

      float black[] = {0.0f, 0.0f, 0.0f, 0.0f};
      drv.glClearBufferfv(eGL_COLOR, 0, black);

      std::vector<uint32_t> events = passEvents;

      if(overlay == DebugOverlay::QuadOverdrawDraw)
        events.clear();

      events.push_back(eventId);

      if(!events.empty())
      {
        GLuint replacefbo = 0;
        GLuint quadtexs[3] = {0};
        drv.glGenFramebuffers(1, &replacefbo);
        drv.glBindFramebuffer(eGL_FRAMEBUFFER, replacefbo);

        drv.glGenTextures(3, quadtexs);

        // image for quad usage
        drv.glBindTexture(eGL_TEXTURE_2D_ARRAY, quadtexs[2]);
        drv.glTextureImage3DEXT(quadtexs[2], eGL_TEXTURE_2D_ARRAY, 0, eGL_R32UI,
                                RDCMAX(1, texDetails.width >> 1), RDCMAX(1, texDetails.height >> 1),
                                4, 0, eGL_RED_INTEGER, eGL_UNSIGNED_INT, NULL);
        drv.glTexParameteri(eGL_TEXTURE_2D_ARRAY, eGL_TEXTURE_MAX_LEVEL, 0);

        // temporarily attach to FBO to clear it
        GLint zero[4] = {0};
        drv.glFramebufferTextureLayer(eGL_FRAMEBUFFER, eGL_COLOR_ATTACHMENT0, quadtexs[2], 0, 0);
        drv.glClearBufferiv(eGL_COLOR, 0, zero);
        drv.glFramebufferTextureLayer(eGL_FRAMEBUFFER, eGL_COLOR_ATTACHMENT0, quadtexs[2], 0, 1);
        drv.glClearBufferiv(eGL_COLOR, 0, zero);
        drv.glFramebufferTextureLayer(eGL_FRAMEBUFFER, eGL_COLOR_ATTACHMENT0, quadtexs[2], 0, 2);
        drv.glClearBufferiv(eGL_COLOR, 0, zero);
        drv.glFramebufferTextureLayer(eGL_FRAMEBUFFER, eGL_COLOR_ATTACHMENT0, quadtexs[2], 0, 3);
        drv.glClearBufferiv(eGL_COLOR, 0, zero);

        drv.glBindTexture(eGL_TEXTURE_2D, quadtexs[0]);
        drv.glTextureImage2DEXT(quadtexs[0], eGL_TEXTURE_2D, 0, eGL_RGBA8, texDetails.width,
                                texDetails.height, 0, eGL_RGBA, eGL_UNSIGNED_BYTE, NULL);
        drv.glTexParameteri(eGL_TEXTURE_2D, eGL_TEXTURE_MAX_LEVEL, 0);
        drv.glTexParameteri(eGL_TEXTURE_2D, eGL_TEXTURE_MIN_FILTER, eGL_NEAREST);
        drv.glTexParameteri(eGL_TEXTURE_2D, eGL_TEXTURE_MAG_FILTER, eGL_NEAREST);
        drv.glTexParameteri(eGL_TEXTURE_2D, eGL_TEXTURE_WRAP_S, eGL_CLAMP_TO_EDGE);
        drv.glTexParameteri(eGL_TEXTURE_2D, eGL_TEXTURE_WRAP_T, eGL_CLAMP_TO_EDGE);
        drv.glFramebufferTexture2D(eGL_FRAMEBUFFER, eGL_COLOR_ATTACHMENT0, eGL_TEXTURE_2D,
                                   quadtexs[0], 0);

        GLuint curDepth = 0, depthType = 0;

        // TODO handle non-2D depth/stencil attachments and fetch slice or cubemap face
        GLint mip = 0;

        drv.glGetNamedFramebufferAttachmentParameterivEXT(rs.DrawFBO.name, eGL_DEPTH_ATTACHMENT,
                                                          eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME,
                                                          (GLint *)&curDepth);
        drv.glGetNamedFramebufferAttachmentParameterivEXT(rs.DrawFBO.name, eGL_DEPTH_ATTACHMENT,
                                                          eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE,
                                                          (GLint *)&depthType);

        GLenum fmt = eGL_DEPTH32F_STENCIL8;

        if(curDepth)
        {
          if(depthType == eGL_TEXTURE)
          {
            drv.glGetNamedFramebufferAttachmentParameterivEXT(
                rs.DrawFBO.name, eGL_DEPTH_ATTACHMENT, eGL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL,
                &mip);
            drv.glGetTextureLevelParameterivEXT(curDepth, eGL_TEXTURE_2D, mip,
                                                eGL_TEXTURE_INTERNAL_FORMAT, (GLint *)&fmt);
          }
          else
          {
            drv.glGetNamedRenderbufferParameterivEXT(curDepth, eGL_RENDERBUFFER_INTERNAL_FORMAT,
                                                     (GLint *)&fmt);
          }
        }

        drv.glBindTexture(eGL_TEXTURE_2D, quadtexs[1]);
        drv.glTextureImage2DEXT(quadtexs[1], eGL_TEXTURE_2D, 0, fmt, texDetails.width,
                                texDetails.height, 0, GetBaseFormat(fmt), GetDataType(fmt), NULL);
        drv.glTexParameteri(eGL_TEXTURE_2D, eGL_TEXTURE_MAX_LEVEL, 0);
        drv.glTexParameteri(eGL_TEXTURE_2D, eGL_TEXTURE_MIN_FILTER, eGL_NEAREST);
        drv.glTexParameteri(eGL_TEXTURE_2D, eGL_TEXTURE_MAG_FILTER, eGL_NEAREST);
        drv.glTexParameteri(eGL_TEXTURE_2D, eGL_TEXTURE_WRAP_S, eGL_CLAMP_TO_EDGE);
        drv.glTexParameteri(eGL_TEXTURE_2D, eGL_TEXTURE_WRAP_T, eGL_CLAMP_TO_EDGE);

        GLenum dsAttach = eGL_DEPTH_STENCIL_ATTACHMENT;

        if(GetBaseFormat(fmt) == eGL_DEPTH_COMPONENT)
          dsAttach = eGL_DEPTH_ATTACHMENT;

        drv.glFramebufferTexture2D(eGL_FRAMEBUFFER, dsAttach, eGL_TEXTURE_2D, quadtexs[1], 0);

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
            drv.glGetIntegerv(eGL_DEPTH_WRITEMASK, &depthwritemask);
            drv.glGetIntegerv(eGL_STENCIL_WRITEMASK, &stencilfmask);
            drv.glGetIntegerv(eGL_STENCIL_BACK_WRITEMASK, &stencilbmask);

            drv.glGetIntegerv(eGL_DRAW_FRAMEBUFFER_BINDING, (GLint *)&curdrawfbo);
            drv.glGetIntegerv(eGL_READ_FRAMEBUFFER_BINDING, (GLint *)&curreadfbo);

            drv.glGetIntegeri_v(eGL_IMAGE_BINDING_NAME, 0, (GLint *)&curimage0.name);
            drv.glGetIntegeri_v(eGL_IMAGE_BINDING_LEVEL, 0, (GLint *)&curimage0.level);
            drv.glGetIntegeri_v(eGL_IMAGE_BINDING_ACCESS, 0, (GLint *)&curimage0.access);
            drv.glGetIntegeri_v(eGL_IMAGE_BINDING_FORMAT, 0, (GLint *)&curimage0.format);
            drv.glGetBooleani_v(eGL_IMAGE_BINDING_LAYERED, 0, &curimage0.layered);
            if(curimage0.layered)
              drv.glGetIntegeri_v(eGL_IMAGE_BINDING_LAYER, 0, (GLint *)&curimage0.layer);
          }

          // disable depth and stencil writes
          drv.glDepthMask(GL_FALSE);
          drv.glStencilMask(GL_FALSE);

          // bind our FBO
          drv.glBindFramebuffer(eGL_DRAW_FRAMEBUFFER, replacefbo);
          // bind image
          drv.glBindImageTexture(0, quadtexs[2], 0, GL_TRUE, 0, eGL_READ_WRITE, eGL_R32UI);

          GLuint prog = 0, pipe = 0;
          drv.glGetIntegerv(eGL_CURRENT_PROGRAM, (GLint *)&prog);
          drv.glGetIntegerv(eGL_PROGRAM_PIPELINE_BINDING, (GLint *)&pipe);

          // replace fragment shader. This is exactly what we did
          // at the start of this function for the single-event case, but now we have
          // to do it for every event
          spirvOverlay = CreateOverlayProgram(prog, pipe, DebugData.quadoverdrawFragShader,
                                              DebugData.quadoverdrawFragShaderSPIRV);
          drv.glUseProgram(DebugData.overlayProg);
          drv.glBindProgramPipeline(0);

          if(!spirvOverlay)
          {
            GLint loc = drv.glGetUniformLocation(DebugData.overlayProg, "overdrawImage");
            if(loc != -1)
              drv.glUniform1i(loc, 0);
            else
              RDCERR("Couldn't get location of overdrawImage");
          }

          drv.glBindFramebuffer(eGL_READ_FRAMEBUFFER, curdrawfbo);
          SafeBlitFramebuffer(0, 0, texDetails.width, texDetails.height, 0, 0, texDetails.width,
                              texDetails.height, GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT,
                              eGL_NEAREST);

          m_pDriver->ReplayLog(0, events[i], eReplay_OnlyDraw);

          // pop the state that we messed with
          {
            drv.glBindProgramPipeline(pipe);
            drv.glUseProgram(prog);

            if(curimage0.name)
              drv.glBindImageTexture(0, curimage0.name, curimage0.level,
                                     curimage0.layered ? GL_TRUE : GL_FALSE, curimage0.layer,
                                     curimage0.access, curimage0.format);
            else
              drv.glBindImageTexture(0, 0, 0, GL_FALSE, 0, eGL_READ_ONLY, eGL_R32UI);

            drv.glBindFramebuffer(eGL_DRAW_FRAMEBUFFER, curdrawfbo);
            drv.glBindFramebuffer(eGL_READ_FRAMEBUFFER, curreadfbo);

            drv.glDepthMask(depthwritemask ? GL_TRUE : GL_FALSE);
            drv.glStencilMaskSeparate(eGL_FRONT, (GLuint)stencilfmask);
            drv.glStencilMaskSeparate(eGL_BACK, (GLuint)stencilbmask);
          }

          if(overlay == DebugOverlay::QuadOverdrawPass)
          {
            m_pDriver->ReplayLog(0, events[i], eReplay_OnlyDraw);

            if(i + 1 < events.size())
              m_pDriver->ReplayLog(events[i] + 1, events[i + 1], eReplay_WithoutDraw);
          }
        }

        // resolve pass
        {
          drv.glUseProgram(DebugData.quadoverdrawResolveProg);
          drv.glBindProgramPipeline(0);

          // modify our fbo to attach the overlay texture instead
          drv.glBindFramebuffer(eGL_FRAMEBUFFER, replacefbo);
          drv.glFramebufferTexture2D(eGL_FRAMEBUFFER, eGL_COLOR_ATTACHMENT0, texBindingEnum,
                                     DebugData.overlayTex, 0);
          drv.glFramebufferTexture2D(eGL_FRAMEBUFFER, eGL_DEPTH_STENCIL_ATTACHMENT, texBindingEnum,
                                     0, 0);

          drv.glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
          drv.glDisable(eGL_BLEND);
          drv.glDisable(eGL_SCISSOR_TEST);
          drv.glDepthMask(GL_FALSE);
          drv.glDisable(eGL_CULL_FACE);
          if(!IsGLES)
            drv.glPolygonMode(eGL_FRONT_AND_BACK, eGL_FILL);
          drv.glDisable(eGL_DEPTH_TEST);
          drv.glDisable(eGL_STENCIL_TEST);
          drv.glStencilMask(0);
          drv.glViewport(0, 0, texDetails.width, texDetails.height);

          drv.glBindImageTexture(0, quadtexs[2], 0, GL_TRUE, 0, eGL_READ_WRITE, eGL_R32UI);

          GLuint emptyVAO = 0;
          drv.glGenVertexArrays(1, &emptyVAO);
          drv.glBindVertexArray(emptyVAO);
          drv.glDrawArrays(eGL_TRIANGLE_STRIP, 0, 4);
          drv.glBindVertexArray(0);
          drv.glDeleteVertexArrays(1, &emptyVAO);

          drv.glFramebufferTexture2D(eGL_FRAMEBUFFER, eGL_COLOR_ATTACHMENT0, eGL_TEXTURE_2D,
                                     quadtexs[0], 0);
        }

        drv.glDeleteFramebuffers(1, &replacefbo);
        drv.glDeleteTextures(3, quadtexs);

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

  DebugData.overlayTexId =
      m_pDriver->GetResourceManager()->GetID(TextureRes(ctx, DebugData.overlayTex));

  return DebugData.overlayTexId;
}
