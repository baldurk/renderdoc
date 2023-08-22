/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2023 Baldur Karlsson
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
#include "maths/formatpacking.h"
#include "maths/matrix.h"
#include "strings/string_utils.h"
#include "gl_driver.h"
#include "gl_replay.h"
#include "gl_resources.h"

#define OPENGL 1
#include "data/glsl/glsl_ubos_cpp.h"

bool GLReplay::CreateFragmentShaderReplacementProgram(GLuint program, GLuint replacementProgram,
                                                      GLuint pipeline, GLuint fragShader,
                                                      GLuint fragShaderSPIRV)
{
  WrappedOpenGL &drv = *m_pDriver;

  ContextPair &ctx = drv.GetCtx();

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

  if(program == 0)
  {
    if(pipeline == 0)
    {
      return false;
    }
    else
    {
      ResourceId id = m_pDriver->GetResourceManager()->GetResID(ProgramPipeRes(ctx, pipeline));
      const WrappedOpenGL::PipelineData &pipeDetails = m_pDriver->m_Pipelines[id];

      // fetch the corresponding shaders and programs for each stage
      for(size_t i = 0; i < 4; i++)
      {
        if(pipeDetails.stageShaders[i] != ResourceId())
        {
          const WrappedOpenGL::ShaderData &shadDetails =
              m_pDriver->m_Shaders[pipeDetails.stageShaders[i]];

          if(shadDetails.reflection->encoding == ShaderEncoding::OpenGLSPIRV)
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
              rdcarray<const char *> sources;
              sources.reserve(shadDetails.sources.size());

              for(const rdcstr &s : shadDetails.sources)
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
                RDCERR("Trying to recreate replacement program, couldn't compile shader:\n%s",
                       buffer);
              }
            }
          }

          if(i == 0)
            vsRefl = GetShader(ResourceId(), pipeDetails.stageShaders[i], ShaderEntryPoint());
        }
      }
    }
  }
  else
  {
    const WrappedOpenGL::ProgramData &progDetails =
        m_pDriver->m_Programs[m_pDriver->GetResourceManager()->GetResID(ProgramRes(ctx, program))];

    // fetch any and all non-fragment shader shaders
    for(size_t i = 0; i < 4; i++)
    {
      if(progDetails.stageShaders[i] != ResourceId())
      {
        programs[i] = program;
        shaders[i] =
            m_pDriver->GetResourceManager()->GetCurrentResource(progDetails.stageShaders[i]).name;

        const WrappedOpenGL::ShaderData &shadDetails =
            m_pDriver->m_Shaders[progDetails.stageShaders[i]];

        if(shadDetails.reflection->encoding == ShaderEncoding::OpenGLSPIRV)
          HasSPIRVShaders = true;
        else
          HasGLSLShaders = true;

        if(i == 0)
          vsRefl = GetShader(ResourceId(), progDetails.stageShaders[0], ShaderEntryPoint());
      }
    }
  }

  if(HasGLSLShaders && HasSPIRVShaders)
    RDCERR("Unsupported - mixed GLSL and SPIR-V shaders in pipeline");

  // attach the shaders
  for(size_t i = 0; i < 4; i++)
    if(shaders[i])
      drv.glAttachShader(replacementProgram, shaders[i]);

  if(HasSPIRVShaders)
  {
    RDCASSERT(fragShaderSPIRV);
    drv.glAttachShader(replacementProgram, fragShaderSPIRV);
  }
  else
  {
    drv.glAttachShader(replacementProgram, fragShader);
  }

  // copy the vertex attribs over from the source program
  if(vsRefl && programs[0] && !HasSPIRVShaders)
    CopyProgramAttribBindings(programs[0], replacementProgram, vsRefl);

  // link the overlay program
  drv.glLinkProgram(replacementProgram);

  // detach the shaders
  for(size_t i = 0; i < 4; i++)
    if(shaders[i])
      drv.glDetachShader(replacementProgram, shaders[i]);

  if(HasSPIRVShaders)
    drv.glDetachShader(replacementProgram, fragShaderSPIRV);
  else
    drv.glDetachShader(replacementProgram, fragShader);

  // delete any temporaries
  for(size_t i = 0; i < 4; i++)
    if(tmpShaders[i])
      drv.glDeleteShader(tmpShaders[i]);

  // check that the link succeeded
  char buffer[1024] = {};
  GLint status = 0;
  drv.glGetProgramiv(replacementProgram, eGL_LINK_STATUS, &status);
  if(status == 0)
  {
    drv.glGetProgramInfoLog(replacementProgram, 1024, NULL, buffer);
    RDCERR("Error linking overlay program: %s", buffer);
    return false;
  }

  // copy the uniform values over from the source program. This is redundant but harmless if the
  // same program is bound to multiple stages. It's just inefficient
  {
    PerStageReflections dstStages;
    m_pDriver->FillReflectionArray(ProgramRes(ctx, replacementProgram), dstStages);

    for(size_t i = 0; i < 4; i++)
    {
      if(programs[i])
      {
        PerStageReflections stages;
        m_pDriver->FillReflectionArray(ProgramRes(ctx, programs[i]), stages);

        CopyProgramUniforms(stages, programs[i], dstStages, replacementProgram);
      }
    }
  }

  return HasSPIRVShaders;
}

RenderOutputSubresource GLReplay::GetRenderOutputSubresource(ResourceId id)
{
  MakeCurrentReplayContext(&m_ReplayCtx);

  WrappedOpenGL &drv = *m_pDriver;
  ContextPair &ctx = drv.GetCtx();

  RenderOutputSubresource ret(~0U, ~0U, 0);

  GLint numCols = 8;
  drv.glGetIntegerv(eGL_MAX_COLOR_ATTACHMENTS, &numCols);

  WrappedOpenGL::TextureData &details = m_pDriver->m_Textures[id];

  GLuint curDrawFBO = 0;
  drv.glGetIntegerv(eGL_DRAW_FRAMEBUFFER_BINDING, (GLint *)&curDrawFBO);

  GLuint name = 0;
  GLenum type = eGL_TEXTURE;
  for(GLint i = 0; i < numCols + 2; i++)
  {
    GLenum att = GLenum(eGL_COLOR_ATTACHMENT0 + i);

    if(i == numCols)
      att = eGL_DEPTH_ATTACHMENT;
    else if(i == numCols + 1)
      att = eGL_STENCIL_ATTACHMENT;

    drv.glGetFramebufferAttachmentParameteriv(
        eGL_DRAW_FRAMEBUFFER, att, eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, (GLint *)&name);
    drv.glGetFramebufferAttachmentParameteriv(
        eGL_DRAW_FRAMEBUFFER, att, eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, (GLint *)&type);

    GLResource res;
    if(type == eGL_RENDERBUFFER)
      res = RenderbufferRes(ctx, name);
    else
      res = TextureRes(ctx, name);

    if(res == details.resource)
    {
      ret.numSlices = 1;

      if(type == eGL_TEXTURE)
      {
        GetFramebufferMipAndLayer(curDrawFBO, att, (GLint *)&ret.mip, (GLint *)&ret.slice);

        // desktop GL allows layered attachments which attach all slices from 0 to N
        if(!IsGLES)
        {
          GLint layered = 0;
          GL.glGetNamedFramebufferAttachmentParameterivEXT(
              curDrawFBO, att, eGL_FRAMEBUFFER_ATTACHMENT_LAYERED, &layered);

          if(layered)
            ret.numSlices = details.depth;
        }
        else
        {
          // on GLES there's an OVR extension that allows attaching multiple layers
          if(HasExt[OVR_multiview])
          {
            GLint numViews = 0;
            GL.glGetNamedFramebufferAttachmentParameterivEXT(
                curDrawFBO, att, eGL_FRAMEBUFFER_ATTACHMENT_TEXTURE_NUM_VIEWS_OVR, &numViews);

            if(numViews > 1)
              ret.numSlices = numViews;
          }
        }
      }
      else
      {
        ret.mip = 0;
        ret.slice = 0;
      }
    }
  }

  return ret;
}

void GLReplay::BindFramebufferTexture(RenderOutputSubresource &sub, GLenum texBindingEnum,
                                      GLint numSamples)
{
  WrappedOpenGL &drv = *m_pDriver;

  if(sub.numSlices > 1)
  {
    if(IsGLES)
    {
      if(HasExt[OVR_multiview])
      {
        if(texBindingEnum == eGL_TEXTURE_2D_MULTISAMPLE_ARRAY)
        {
          drv.glFramebufferTextureMultisampleMultiviewOVR(eGL_FRAMEBUFFER, eGL_COLOR_ATTACHMENT0,
                                                          DebugData.overlayTex, sub.mip, numSamples,
                                                          sub.slice, sub.numSlices);
        }
        else
        {
          drv.glFramebufferTextureMultiviewOVR(eGL_FRAMEBUFFER, eGL_COLOR_ATTACHMENT0,
                                               DebugData.overlayTex, sub.mip, sub.slice,
                                               sub.numSlices);
        }
      }
      else
      {
        RDCERR("Multiple slices bound without OVR_multiview");
        // without OVR_multiview we can't bind the whole array, so just bind slice 0
        drv.glFramebufferTextureLayer(eGL_FRAMEBUFFER, eGL_COLOR_ATTACHMENT0, DebugData.overlayTex,
                                      sub.mip, sub.slice);
      }
    }
    else
    {
      drv.glFramebufferTexture(eGL_FRAMEBUFFER, eGL_COLOR_ATTACHMENT0, DebugData.overlayTex, sub.mip);
    }
  }
  else
  {
    if(texBindingEnum == eGL_TEXTURE_2D_MULTISAMPLE_ARRAY || texBindingEnum == eGL_TEXTURE_2D_ARRAY)
    {
      drv.glFramebufferTextureLayer(eGL_FRAMEBUFFER, eGL_COLOR_ATTACHMENT0, DebugData.overlayTex,
                                    sub.mip, sub.slice);
    }
    else
    {
      drv.glFramebufferTexture2D(eGL_FRAMEBUFFER, eGL_COLOR_ATTACHMENT0, texBindingEnum,
                                 DebugData.overlayTex, sub.mip);
    }
  }
}

ResourceId GLReplay::RenderOverlay(ResourceId texid, FloatVector clearCol, DebugOverlay overlay,
                                   uint32_t eventId, const rdcarray<uint32_t> &passEvents)
{
  WrappedOpenGL &drv = *m_pDriver;

  MakeCurrentReplayContext(&m_ReplayCtx);

  RenderOutputSubresource sub = GetRenderOutputSubresource(texid);

  if(sub.slice == ~0U)
  {
    RDCERR("Rendering overlay for %s couldn't find output to get subresource.", ToStr(texid).c_str());
    sub = RenderOutputSubresource(0, 0, 1);
  }

  GLMarkerRegion renderoverlay(StringFormat::Fmt("RenderOverlay %d", overlay));

  ContextPair &ctx = drv.GetCtx();

  GLRenderState rs;
  rs.FetchState(&drv);

  if(rs.Program.name == 0 && rs.Pipeline.name == 0)
    return ResourceId();

  GL.glBindBuffer(eGL_PIXEL_UNPACK_BUFFER, 0);

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
    shaderType = ShaderType::GLSLES;

    // default to 100 just in case something is broken
    glslVer = 100;

    // GLES requires that versions *precisely* match, so here we must figure out what version the
    // existing shader was using (we pick the vertex shader for simplicity since by definition it
    // must match any others) and recompile our shaders with that version. We've ensured they are
    // compatible with anything after the minimum version
    ResourceId vs;

    if(rs.Program.name)
      vs =
          m_pDriver->m_Programs[m_pDriver->GetResourceManager()->GetResID(rs.Program)].stageShaders[0];
    else
      vs = m_pDriver->m_Pipelines[m_pDriver->GetResourceManager()->GetResID(rs.Pipeline)]
               .stageShaders[0];

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
    shaderType = ShaderType::GLSL;
  }

  // this is always compatible.
  {
    rdcstr source = GenerateGLSLShader(GetEmbeddedResource(glsl_fixedcol_frag), shaderType, glslVer);
    DebugData.fixedcolFragShader = CreateShader(eGL_FRAGMENT_SHADER, source);
  }

  // this is not supported on GLES
  if(shaderType == ShaderType::GLSL)
  {
    rdcstr defines = "";

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
      rdcstr source =
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

  GLuint prog = 0, pipe = 0;
  drv.glGetIntegerv(eGL_CURRENT_PROGRAM, (GLint *)&prog);
  drv.glGetIntegerv(eGL_PROGRAM_PIPELINE_BINDING, (GLint *)&pipe);
  // delete the old program if it exists
  if(DebugData.overlayProg != 0)
    drv.glDeleteProgram(DebugData.overlayProg);

  DebugData.overlayProg = drv.glCreateProgram();

  // we bind the separable program created for each shader, and copy
  // uniforms and attrib bindings from the 'real' programs, wherever
  // they are.
  bool spirvOverlay = CreateFragmentShaderReplacementProgram(
      rs.Program.name, DebugData.overlayProg, rs.Pipeline.name, DebugData.fixedcolFragShader,
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

  GLint texSlices = texDetails.depth;
  if(TextureTarget(texDetails.curType) == eGL_TEXTURE_3D)
    texSlices = 1;

  if(texDetails.samples > 1)
  {
    texBindingEnum = eGL_TEXTURE_2D_MULTISAMPLE;
    texQueryEnum = eGL_TEXTURE_BINDING_2D_MULTISAMPLE;

    if(texSlices > 1)
    {
      texBindingEnum = eGL_TEXTURE_2D_MULTISAMPLE_ARRAY;
      texQueryEnum = eGL_TEXTURE_BINDING_2D_MULTISAMPLE_ARRAY;
    }
  }

  if(texSlices > 1)
  {
    texBindingEnum = eGL_TEXTURE_2D_ARRAY;
    texQueryEnum = eGL_TEXTURE_BINDING_2D_ARRAY;
  }

  GLint texMips = GetNumMips(texDetails.curType, texDetails.resource.name, texDetails.width,
                             texDetails.height, texDetails.depth);

  // resize (or create) the overlay texture and FBO if necessary
  if(DebugData.overlayTexWidth != texDetails.width ||
     DebugData.overlayTexHeight != texDetails.height ||
     DebugData.overlayTexSamples != texDetails.samples || DebugData.overlayTexMips != texMips ||
     DebugData.overlayTexSlices != texSlices)
  {
    if(DebugData.overlayFBO)
    {
      drv.glDeleteFramebuffers(1, &DebugData.overlayFBO);
      drv.glDeleteTextures(1, &DebugData.overlayTex);
    }

    drv.glGenFramebuffers(1, &DebugData.overlayFBO);
    drv.glBindFramebuffer(eGL_FRAMEBUFFER, DebugData.overlayFBO);

    drv.glObjectLabel(eGL_FRAMEBUFFER, DebugData.overlayFBO, -1, "FBO for overlay");

    GLuint curTex = 0;
    drv.glGetIntegerv(texQueryEnum, (GLint *)&curTex);

    drv.glGenTextures(1, &DebugData.overlayTex);
    drv.glBindTexture(texBindingEnum, DebugData.overlayTex);

    drv.glObjectLabel(eGL_TEXTURE, DebugData.overlayTex, -1, "Colour tex for overlay");

    DebugData.overlayTexWidth = texDetails.width;
    DebugData.overlayTexHeight = texDetails.height;
    DebugData.overlayTexSamples = texDetails.samples;
    DebugData.overlayTexMips = texMips;
    DebugData.overlayTexSlices = texSlices;

    if(DebugData.overlayTexSamples > 1)
    {
      if(DebugData.overlayTexSlices > 1)
      {
        drv.glTextureStorage3DMultisampleEXT(DebugData.overlayTex, texBindingEnum,
                                             texDetails.samples, eGL_RGBA16F, texDetails.width,
                                             texDetails.height, texSlices, true);
      }
      else
      {
        drv.glTextureStorage2DMultisampleEXT(DebugData.overlayTex, texBindingEnum, texDetails.samples,
                                             eGL_RGBA16F, texDetails.width, texDetails.height, true);
      }
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

      if(texSlices > 1)
      {
        drv.glTextureImage3DEXT(DebugData.overlayTex, texBindingEnum, 0, internalFormat,
                                texDetails.width, texDetails.height, texSlices, 0, format, type,
                                NULL);
        for(GLint i = 1; i < texMips; i++)
          drv.glTextureImage3DEXT(DebugData.overlayTex, texBindingEnum, i, internalFormat,
                                  RDCMAX(1, texDetails.width >> i), RDCMAX(1, texDetails.height >> i),
                                  texSlices, 0, format, type, NULL);
      }
      else
      {
        drv.glTextureImage2DEXT(DebugData.overlayTex, texBindingEnum, 0, internalFormat,
                                texDetails.width, texDetails.height, 0, format, type, NULL);
        for(GLint i = 1; i < texMips; i++)
          drv.glTextureImage2DEXT(DebugData.overlayTex, texBindingEnum, i, internalFormat,
                                  RDCMAX(1, texDetails.width >> i),
                                  RDCMAX(1, texDetails.height >> i), 0, format, type, NULL);
      }

      drv.glTextureParameteriEXT(DebugData.overlayTex, texBindingEnum, eGL_TEXTURE_MAX_LEVEL,
                                 texMips - 1);
      drv.glTextureParameteriEXT(DebugData.overlayTex, texBindingEnum, eGL_TEXTURE_MIN_FILTER,
                                 eGL_NEAREST);
      drv.glTextureParameteriEXT(DebugData.overlayTex, texBindingEnum, eGL_TEXTURE_MAG_FILTER,
                                 eGL_NEAREST);
      drv.glTextureParameteriEXT(DebugData.overlayTex, texBindingEnum, eGL_TEXTURE_WRAP_S,
                                 eGL_CLAMP_TO_EDGE);
      drv.glTextureParameteriEXT(DebugData.overlayTex, texBindingEnum, eGL_TEXTURE_WRAP_T,
                                 eGL_CLAMP_TO_EDGE);
    }

    // clear all mips first
    drv.glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    drv.glDisable(eGL_BLEND);
    drv.glDepthMask(GL_FALSE);
    drv.glDisable(eGL_CULL_FACE);
    drv.glDisable(eGL_DEPTH_TEST);
    drv.glDisable(eGL_STENCIL_TEST);
    drv.glStencilMask(0);

    drv.glBindTexture(texBindingEnum, curTex);
  }

  GLint outWidth = RDCMAX(1, texDetails.width >> sub.mip);
  GLint outHeight = RDCMAX(1, texDetails.height >> sub.mip);

  drv.glBindFramebuffer(eGL_FRAMEBUFFER, DebugData.overlayFBO);

  drv.glDisable(eGL_SCISSOR_TEST);

  // clear the overlay texture to black
  {
    GLfloat black[4] = {};
    if(texSlices > 1)
    {
      for(GLint s = 0; s < texSlices; s++)
      {
        for(GLint m = 0; m < texMips; m++)
        {
          drv.glFramebufferTextureLayer(eGL_FRAMEBUFFER, eGL_COLOR_ATTACHMENT0,
                                        DebugData.overlayTex, m, s);
          drv.glClearBufferfv(eGL_COLOR, 0, black);
        }
      }
    }
    else
    {
      for(GLint m = 0; m < texMips; m++)
      {
        drv.glFramebufferTexture2D(eGL_FRAMEBUFFER, eGL_COLOR_ATTACHMENT0, texBindingEnum,
                                   DebugData.overlayTex, m);
        drv.glClearBufferfv(eGL_COLOR, 0, black);
      }
    }
  }

  // bind the desired mip/slice/slices
  BindFramebufferTexture(sub, texBindingEnum, texDetails.samples);

  // disable several tests/allow rendering - some overlays will override
  // these states but commonly we don't want to inherit these states from
  // the program's state.
  drv.glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  drv.glDisable(eGL_BLEND);
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

  if(HasExt[ARB_texture_multisample_no_array] || HasExt[ARB_texture_multisample])
    drv.glDisable(eGL_SAMPLE_MASK);

  if(HasExt[ARB_viewport_array])
  {
    for(size_t s = 0; s < ARRAY_COUNT(rs.Scissors); s++)
    {
      if(rs.Scissors[s].enabled)
        drv.glEnablei(eGL_SCISSOR_TEST, (GLuint)s);
      else
        drv.glDisablei(eGL_SCISSOR_TEST, (GLuint)s);
    }
  }
  else
  {
    if(rs.Scissors[0].enabled)
      drv.glEnable(eGL_SCISSOR_TEST);
    else
      drv.glDisable(eGL_SCISSOR_TEST);
  }

  if(overlay == DebugOverlay::NaN || overlay == DebugOverlay::Clipping)
  {
    // just need the basic texture
    float black[] = {0.0f, 0.0f, 0.0f, 0.0f};
    drv.glClearBufferfv(eGL_COLOR, 0, black);
  }
  else if(overlay == DebugOverlay::Drawcall)
  {
    if(HasExt[ARB_viewport_array])
      drv.glDisablei(eGL_SCISSOR_TEST, 0);
    else
      drv.glDisable(eGL_SCISSOR_TEST);

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

      const ActionDescription *action = m_pDriver->GetAction(eventId);
      const GLDrawParams &drawParams = m_pDriver->GetDrawParameters(eventId);

      rdcarray<uint32_t> patchedIndices;

      // readback the index buffer data
      if(idxbuf)
      {
        rdcarray<byte> idxs;
        uint32_t offset = action->indexOffset * drawParams.indexWidth;
        uint32_t length = 1;
        drv.glGetNamedBufferParameterivEXT(idxbuf, eGL_BUFFER_SIZE, (GLint *)&length);

        idxs.resize(length);
        drv.glGetBufferSubData(
            eGL_ELEMENT_ARRAY_BUFFER, offset,
            RDCMIN(GLsizeiptr(length - offset),
                   GLsizeiptr(action->numIndices) * GLsizeiptr(drawParams.indexWidth)),
            &idxs[0]);

        // unbind the real index buffer
        drv.glBindBuffer(eGL_ELEMENT_ARRAY_BUFFER, 0);

        uint32_t expectedSize = action->numIndices * drawParams.indexWidth;

        if(idxs.size() < expectedSize)
        {
          RDCERR("Index buffer is as large as expected");
          idxs.resize(expectedSize);
        }

        PatchLineStripIndexBuffer(
            action, drawParams.topo,
            drawParams.indexWidth == 1 ? (uint8_t *)idxs.data() : (uint8_t *)NULL,
            drawParams.indexWidth == 2 ? (uint16_t *)idxs.data() : (uint16_t *)NULL,
            drawParams.indexWidth == 4 ? (uint32_t *)idxs.data() : (uint32_t *)NULL, patchedIndices);
      }
      else
      {
        // generate 'index' list
        rdcarray<uint32_t> idxs;
        idxs.resize(action->numIndices);
        for(uint32_t i = 0; i < action->numIndices; i++)
          idxs[i] = i;
        PatchLineStripIndexBuffer(action, drawParams.topo, NULL, NULL, idxs.data(), patchedIndices);
      }

      GLboolean primRestart = drv.glIsEnabled(eGL_PRIMITIVE_RESTART_FIXED_INDEX);
      drv.glEnable(eGL_PRIMITIVE_RESTART_FIXED_INDEX);

      if(action->flags & ActionFlags::Instanced)
      {
        if(HasExt[ARB_base_instance])
        {
          drv.glDrawElementsInstancedBaseVertexBaseInstance(
              eGL_LINE_STRIP, (GLsizei)patchedIndices.size(), eGL_UNSIGNED_INT,
              patchedIndices.data(), action->numInstances, 0, action->instanceOffset);
        }
        else
        {
          drv.glDrawElementsInstancedBaseVertex(eGL_LINE_STRIP, (GLsizei)patchedIndices.size(),
                                                eGL_UNSIGNED_INT, patchedIndices.data(),
                                                action->numInstances, 0);
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

    if(HasExt[ARB_viewport_array])
      drv.glDisablei(eGL_SCISSOR_TEST, 0);
    else
      drv.glDisable(eGL_SCISSOR_TEST);

    col[0] = 1.0f;
    col[1] = 0.0f;
    col[3] = 1.0f;

    drv.glProgramUniform4fv(DebugData.overlayProg, overlayFixedColLocation, 1, col);

    ReplayLog(eventId, eReplay_OnlyDraw);

    if(HasExt[ARB_viewport_array])
    {
      if(rs.Scissors[0].enabled)
        drv.glEnablei(eGL_SCISSOR_TEST, 0);
      else
        drv.glDisablei(eGL_SCISSOR_TEST, 0);
    }
    else
    {
      if(rs.Scissors[0].enabled)
        drv.glEnable(eGL_SCISSOR_TEST);
      else
        drv.glDisable(eGL_SCISSOR_TEST);
    }

    col[0] = 0.0f;
    col[1] = 1.0f;

    drv.glProgramUniform4fv(DebugData.overlayProg, overlayFixedColLocation, 1, col);

    ReplayLog(eventId, eReplay_OnlyDraw);

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

    if(!cdata)
    {
      RDCERR("Map buffer failed %d", drv.glGetError());
      return ResourceId();
    }

    cdata->BorderWidth = 3;
    cdata->CheckerSquareDimension = 16.0f;

    // set primary/secondary to the same to 'disable' checkerboard
    cdata->PrimaryColor = cdata->SecondaryColor = Vec4f(0.1f, 0.1f, 0.1f, 1.0f);
    cdata->InnerColor = Vec4f(0.2f, 0.2f, 0.9f, 0.4f);

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

      if(!cdata)
      {
        RDCERR("Map buffer failed %d", drv.glGetError());
        return ResourceId();
      }

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
    if(HasExt[ARB_viewport_array])
      drv.glDisablei(eGL_SCISSOR_TEST, 0);
    else
      drv.glDisable(eGL_SCISSOR_TEST);

    float backCol[] = {0.0f, 0.0f, 0.0f, 0.0f};
    drv.glClearBufferfv(eGL_COLOR, 0, backCol);

    if(HasExt[ARB_viewport_array])
    {
      if(rs.Scissors[0].enabled)
        drv.glEnablei(eGL_SCISSOR_TEST, 0);
      else
        drv.glDisablei(eGL_SCISSOR_TEST, 0);
    }
    else
    {
      if(rs.Scissors[0].enabled)
        drv.glEnable(eGL_SCISSOR_TEST);
      else
        drv.glDisable(eGL_SCISSOR_TEST);
    }

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
    bool useBlitFramebuffer = true;

    // create matching depth for existing FBO
    if(curDepth != 0)
    {
      GLint type = 0;
      drv.glGetNamedFramebufferAttachmentParameterivEXT(
          rs.DrawFBO.name, eGL_DEPTH_ATTACHMENT, eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &type);

      GLenum fmt;

      if(type != eGL_RENDERBUFFER)
      {
        ResourceId id = m_pDriver->GetResourceManager()->GetResID(TextureRes(ctx, curDepth));
        fmt = m_pDriver->m_Textures[id].internalFormat;
      }
      else
      {
        ResourceId id = m_pDriver->GetResourceManager()->GetResID(RenderbufferRes(ctx, curDepth));
        fmt = m_pDriver->m_Textures[id].internalFormat;

        GLint depth = 0;
        GLint stencil = 0;
        GL.glGetNamedRenderbufferParameterivEXT(curDepth, eGL_RENDERBUFFER_DEPTH_SIZE, &depth);
        GL.glGetNamedRenderbufferParameterivEXT(curDepth, eGL_RENDERBUFFER_STENCIL_SIZE, &stencil);

        if(depth == 16 && stencil == 0)
          fmt = eGL_DEPTH_COMPONENT16;
        else if(depth == 24 && stencil == 0)
          fmt = eGL_DEPTH_COMPONENT24;
        else if(depth == 24 && stencil == 8)
          fmt = eGL_DEPTH24_STENCIL8;
        else if(depth == 32 && stencil == 0)
          fmt = eGL_DEPTH_COMPONENT32F;
        else if(depth == 32 && stencil == 8)
          fmt = eGL_DEPTH32F_STENCIL8;
        else if(depth == 0 && stencil == 8)
          fmt = eGL_STENCIL_INDEX8;
      }
      // For depth overlay : need a stencil buffer
      if(overlay == DebugOverlay::Depth)
      {
        GLenum oldFmt = fmt;
        if((oldFmt == eGL_DEPTH_COMPONENT16) || (oldFmt == eGL_DEPTH_COMPONENT24) ||
           (oldFmt == eGL_DEPTH24_STENCIL8))
          fmt = eGL_DEPTH24_STENCIL8;
        else
          fmt = eGL_DEPTH32F_STENCIL8;

        if(oldFmt != fmt)
        {
          useBlitFramebuffer = false;
          curStencil = curDepth;
        }
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
      if(DebugData.overlayTexSlices > 1)
      {
        if(DebugData.overlayTexSamples > 1)
        {
          drv.glTextureStorage3DMultisampleEXT(
              depthCopy, copyBindingEnum, DebugData.overlayTexSamples, fmt, DebugData.overlayTexWidth,
              DebugData.overlayTexHeight, DebugData.overlayTexSlices, true);
        }
        else
        {
          drv.glTextureImage3DEXT(depthCopy, copyBindingEnum, 0, fmt, DebugData.overlayTexWidth,
                                  DebugData.overlayTexHeight, DebugData.overlayTexSlices, 0,
                                  GetBaseFormat(fmt), GetDataType(fmt), NULL);
          for(GLint i = 1; i < texMips; i++)
            drv.glTextureImage3DEXT(
                depthCopy, copyBindingEnum, i, fmt, RDCMAX(1, DebugData.overlayTexWidth >> i),
                RDCMAX(1, DebugData.overlayTexHeight >> i), DebugData.overlayTexSlices, 0,
                GetBaseFormat(fmt), GetDataType(fmt), NULL);
          drv.glTextureParameteriEXT(depthCopy, copyBindingEnum, eGL_TEXTURE_MAX_LEVEL, 0);
          drv.glTextureParameteriEXT(depthCopy, copyBindingEnum, eGL_TEXTURE_MIN_FILTER, eGL_NEAREST);
          drv.glTextureParameteriEXT(depthCopy, copyBindingEnum, eGL_TEXTURE_MAG_FILTER, eGL_NEAREST);
          drv.glTextureParameteriEXT(depthCopy, copyBindingEnum, eGL_TEXTURE_WRAP_S,
                                     eGL_CLAMP_TO_EDGE);
          drv.glTextureParameteriEXT(depthCopy, copyBindingEnum, eGL_TEXTURE_WRAP_T,
                                     eGL_CLAMP_TO_EDGE);
        }
      }
      else
      {
        if(DebugData.overlayTexSamples > 1)
        {
          drv.glTextureStorage2DMultisampleEXT(
              depthCopy, copyBindingEnum, DebugData.overlayTexSamples, fmt,
              DebugData.overlayTexWidth, DebugData.overlayTexHeight, true);
        }
        else
        {
          drv.glTextureImage2DEXT(depthCopy, copyBindingEnum, 0, fmt, DebugData.overlayTexWidth,
                                  DebugData.overlayTexHeight, 0, GetBaseFormat(fmt),
                                  GetDataType(fmt), NULL);
          for(GLint i = 1; i < texMips; i++)
            drv.glTextureImage2DEXT(depthCopy, copyBindingEnum, i, fmt,
                                    RDCMAX(1, DebugData.overlayTexWidth >> i),
                                    RDCMAX(1, DebugData.overlayTexHeight >> i), 0,
                                    GetBaseFormat(fmt), GetDataType(fmt), NULL);
          drv.glTextureParameteriEXT(depthCopy, copyBindingEnum, eGL_TEXTURE_MAX_LEVEL, 0);
          drv.glTextureParameteriEXT(depthCopy, copyBindingEnum, eGL_TEXTURE_MIN_FILTER, eGL_NEAREST);
          drv.glTextureParameteriEXT(depthCopy, copyBindingEnum, eGL_TEXTURE_MAG_FILTER, eGL_NEAREST);
          drv.glTextureParameteriEXT(depthCopy, copyBindingEnum, eGL_TEXTURE_WRAP_S,
                                     eGL_CLAMP_TO_EDGE);
          drv.glTextureParameteriEXT(depthCopy, copyBindingEnum, eGL_TEXTURE_WRAP_T,
                                     eGL_CLAMP_TO_EDGE);
        }
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
        ResourceId id = m_pDriver->GetResourceManager()->GetResID(TextureRes(ctx, curStencil));
        fmt = m_pDriver->m_Textures[id].internalFormat;
      }
      else
      {
        ResourceId id = m_pDriver->GetResourceManager()->GetResID(RenderbufferRes(ctx, curStencil));
        fmt = m_pDriver->m_Textures[id].internalFormat;

        GLint depth = 0;
        GLint stencil = 0;
        GL.glGetNamedRenderbufferParameterivEXT(curStencil, eGL_RENDERBUFFER_DEPTH_SIZE, &depth);
        GL.glGetNamedRenderbufferParameterivEXT(curStencil, eGL_RENDERBUFFER_STENCIL_SIZE, &stencil);

        if(depth == 16 && stencil == 0)
          fmt = eGL_DEPTH_COMPONENT16;
        else if(depth == 24 && stencil == 0)
          fmt = eGL_DEPTH_COMPONENT24;
        else if(depth == 24 && stencil == 8)
          fmt = eGL_DEPTH24_STENCIL8;
        else if(depth == 32 && stencil == 0)
          fmt = eGL_DEPTH_COMPONENT32F;
        else if(depth == 32 && stencil == 8)
          fmt = eGL_DEPTH32F_STENCIL8;
        else if(depth == 0 && stencil == 8)
          fmt = eGL_STENCIL_INDEX8;
      }

      GLuint curTex = 0;
      drv.glGetIntegerv(copyQueryEnum, (GLint *)&curTex);

      drv.glGenTextures(1, &stencilCopy);
      drv.glBindTexture(copyBindingEnum, stencilCopy);
      if(DebugData.overlayTexSlices > 1)
      {
        if(DebugData.overlayTexSamples > 1)
        {
          drv.glTextureStorage3DMultisampleEXT(stencilCopy, copyBindingEnum,
                                               DebugData.overlayTexSamples, fmt,
                                               DebugData.overlayTexWidth, DebugData.overlayTexHeight,
                                               DebugData.overlayTexSlices, true);
        }
        else
        {
          drv.glTextureImage3DEXT(stencilCopy, copyBindingEnum, 0, fmt, DebugData.overlayTexWidth,
                                  DebugData.overlayTexHeight, DebugData.overlayTexSlices, 0,
                                  GetBaseFormat(fmt), GetDataType(fmt), NULL);
          for(GLint i = 1; i < texMips; i++)
            drv.glTextureImage3DEXT(
                stencilCopy, copyBindingEnum, i, fmt, RDCMAX(1, DebugData.overlayTexWidth >> i),
                RDCMAX(1, DebugData.overlayTexHeight >> i), DebugData.overlayTexSlices, 0,
                GetBaseFormat(fmt), GetDataType(fmt), NULL);
          drv.glTextureParameteriEXT(stencilCopy, copyBindingEnum, eGL_TEXTURE_MAX_LEVEL, 0);
          drv.glTextureParameteriEXT(stencilCopy, copyBindingEnum, eGL_TEXTURE_MIN_FILTER,
                                     eGL_NEAREST);
          drv.glTextureParameteriEXT(stencilCopy, copyBindingEnum, eGL_TEXTURE_MAG_FILTER,
                                     eGL_NEAREST);
          drv.glTextureParameteriEXT(stencilCopy, copyBindingEnum, eGL_TEXTURE_WRAP_S,
                                     eGL_CLAMP_TO_EDGE);
          drv.glTextureParameteriEXT(stencilCopy, copyBindingEnum, eGL_TEXTURE_WRAP_T,
                                     eGL_CLAMP_TO_EDGE);
        }
      }
      else
      {
        if(DebugData.overlayTexSamples > 1)
        {
          drv.glTextureStorage2DMultisampleEXT(
              stencilCopy, copyBindingEnum, DebugData.overlayTexSamples, fmt,
              DebugData.overlayTexWidth, DebugData.overlayTexHeight, true);
        }
        else
        {
          drv.glTextureImage2DEXT(stencilCopy, copyBindingEnum, 0, fmt, DebugData.overlayTexWidth,
                                  DebugData.overlayTexHeight, 0, GetBaseFormat(fmt),
                                  GetDataType(fmt), NULL);
          for(GLint i = 1; i < texMips; i++)
            drv.glTextureImage2DEXT(stencilCopy, copyBindingEnum, i, fmt,
                                    RDCMAX(1, DebugData.overlayTexWidth >> i),
                                    RDCMAX(1, DebugData.overlayTexHeight >> i), 0,
                                    GetBaseFormat(fmt), GetDataType(fmt), NULL);
          drv.glTextureParameteriEXT(stencilCopy, copyBindingEnum, eGL_TEXTURE_MAX_LEVEL, 0);
          drv.glTextureParameteriEXT(stencilCopy, copyBindingEnum, eGL_TEXTURE_MIN_FILTER,
                                     eGL_NEAREST);
          drv.glTextureParameteriEXT(stencilCopy, copyBindingEnum, eGL_TEXTURE_MAG_FILTER,
                                     eGL_NEAREST);
          drv.glTextureParameteriEXT(stencilCopy, copyBindingEnum, eGL_TEXTURE_WRAP_S,
                                     eGL_CLAMP_TO_EDGE);
          drv.glTextureParameteriEXT(stencilCopy, copyBindingEnum, eGL_TEXTURE_WRAP_T,
                                     eGL_CLAMP_TO_EDGE);
        }
      }

      drv.glBindTexture(copyBindingEnum, curTex);
    }

    // bind the 'real' fbo to the read framebuffer, so we can blit from it
    drv.glBindFramebuffer(eGL_READ_FRAMEBUFFER, rs.DrawFBO.name);

    // bind depth/stencil to overlay FBO (currently bound to DRAW_FRAMEBUFFER)
    if(curDepth != 0 && curDepth == curStencil)
    {
      drv.glFramebufferTexture2D(eGL_DRAW_FRAMEBUFFER, eGL_DEPTH_STENCIL_ATTACHMENT,
                                 copyBindingEnum, depthCopy, sub.mip);
    }
    else if(curDepth != 0)
    {
      drv.glFramebufferTexture2D(eGL_DRAW_FRAMEBUFFER, eGL_DEPTH_ATTACHMENT, copyBindingEnum,
                                 depthCopy, sub.mip);
    }
    else if(curStencil != 0)
    {
      drv.glFramebufferTexture2D(eGL_DRAW_FRAMEBUFFER, eGL_STENCIL_ATTACHMENT, copyBindingEnum,
                                 stencilCopy, sub.mip);
    }

    if(useBlitFramebuffer)
    {
      // get latest depth/stencil from read FBO (existing FBO) into draw FBO (overlay FBO)
      SafeBlitFramebuffer(0, 0, outWidth, outHeight, 0, 0, outWidth, outHeight,
                          GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT, eGL_NEAREST);
    }
    else
    {
      GLRenderState savedRS;
      savedRS.FetchState(&drv);

      // Fullscreen pass with shader to read from old depth buffer and write to new depth buffer
      drv.glDisable(eGL_BLEND);
      drv.glDisable(eGL_SCISSOR_TEST);
      drv.glDisable(eGL_STENCIL_TEST);
      drv.glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
      drv.glEnable(eGL_DEPTH_TEST);
      drv.glDepthFunc(eGL_ALWAYS);
      drv.glDepthMask(GL_TRUE);
      drv.glDisable(eGL_CULL_FACE);
      if(!IsGLES)
        drv.glPolygonMode(eGL_FRONT_AND_BACK, eGL_FILL);

      if(DebugData.overlayTexSamples > 1)
        drv.glUseProgram(DebugData.fullScreenCopyDepthMS);
      else
        drv.glUseProgram(DebugData.fullScreenCopyDepth);
      drv.glBindProgramPipeline(0);

      drv.glActiveTexture(eGL_TEXTURE0);
      drv.glBindTexture(copyBindingEnum, curDepth);

      GLuint emptyVAO = 0;
      drv.glGenVertexArrays(1, &emptyVAO);
      drv.glBindVertexArray(emptyVAO);
      drv.glDrawArrays(eGL_TRIANGLE_STRIP, 0, 4);
      drv.glDeleteVertexArrays(1, &emptyVAO);

      savedRS.ApplyState(&drv);
    }

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

    bool useDepthStencilMask = (overlay == DebugOverlay::Depth) && (curDepth != 0);
    if(useDepthStencilMask)
    {
      drv.glStencilMask(0xff);
      GLint stencilClear = 0x0;
      drv.glClearBufferiv(eGL_STENCIL, 0, &stencilClear);
      drv.glEnable(eGL_STENCIL_TEST);
      drv.glStencilFunc(eGL_ALWAYS, 1, 0xff);
      drv.glStencilOp(eGL_KEEP, eGL_KEEP, eGL_REPLACE);

      drv.glBindProgramPipeline(pipe);
      drv.glUseProgram(prog);

      drv.glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    }
    else
    {
      float green[] = {0.0f, 1.0f, 0.0f, 1.0f};
      drv.glProgramUniform4fv(DebugData.overlayProg, overlayFixedColLocation, 1, green);
    }

    ReplayLog(eventId, eReplay_OnlyDraw);

    if(useDepthStencilMask)
    {
      float green[] = {0.0f, 1.0f, 0.0f, 1.0f};
      GLint fixedColLocation = 99;
      if(!spirvOverlay)
        fixedColLocation =
            drv.glGetUniformLocation(DebugData.fullScreenFixedColProg, "RENDERDOC_Fixed_Color");
      drv.glProgramUniform4fv(DebugData.fullScreenFixedColProg, fixedColLocation, 1, green);

      drv.glDisable(eGL_BLEND);
      drv.glDisable(eGL_SCISSOR_TEST);
      drv.glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
      drv.glDepthMask(GL_FALSE);
      drv.glDisable(eGL_CULL_FACE);
      if(!IsGLES)
        drv.glPolygonMode(eGL_FRONT_AND_BACK, eGL_FILL);
      drv.glDisable(eGL_DEPTH_TEST);
      drv.glEnable(eGL_STENCIL_TEST);
      drv.glStencilMask(0x0);
      drv.glStencilFunc(eGL_EQUAL, 1, 0xff);

      drv.glUseProgram(DebugData.fullScreenFixedColProg);
      drv.glBindProgramPipeline(0);

      GLuint emptyVAO = 0;
      drv.glGenVertexArrays(1, &emptyVAO);
      drv.glBindVertexArray(emptyVAO);
      drv.glDrawArrays(eGL_TRIANGLE_STRIP, 0, 4);
      drv.glBindVertexArray(0);
      drv.glDeleteVertexArrays(1, &emptyVAO);
    }

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
    if(HasExt[ARB_viewport_array])
      drv.glDisablei(eGL_SCISSOR_TEST, 0);
    else
      drv.glDisable(eGL_SCISSOR_TEST);

    float col[] = {0.0f, 0.0f, 0.0f, 0.0f};
    drv.glClearBufferfv(eGL_COLOR, 0, col);

    if(HasExt[ARB_viewport_array])
    {
      if(rs.Scissors[0].enabled)
        drv.glEnablei(eGL_SCISSOR_TEST, 0);
      else
        drv.glDisablei(eGL_SCISSOR_TEST, 0);
    }
    else
    {
      if(rs.Scissors[0].enabled)
        drv.glEnable(eGL_SCISSOR_TEST);
      else
        drv.glDisable(eGL_SCISSOR_TEST);
    }

    col[0] = 1.0f;
    col[1] = 0.0f;
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
    if(HasExt[ARB_viewport_array])
      drv.glDisablei(eGL_SCISSOR_TEST, 0);
    else
      drv.glDisable(eGL_SCISSOR_TEST);

    float col[] = {0.0f, 0.0f, 0.0f, 0.0f};
    drv.glClearBufferfv(eGL_COLOR, 0, col);

    if(HasExt[ARB_viewport_array])
    {
      for(size_t s = 0; s < ARRAY_COUNT(rs.Scissors); s++)
      {
        if(rs.Scissors[s].enabled)
          drv.glEnablei(eGL_SCISSOR_TEST, (GLuint)s);
        else
          drv.glDisablei(eGL_SCISSOR_TEST, (GLuint)s);
      }
    }
    else
    {
      if(rs.Scissors[0].enabled)
        drv.glEnable(eGL_SCISSOR_TEST);
      else
        drv.glDisable(eGL_SCISSOR_TEST);
    }

    rdcarray<uint32_t> events = passEvents;

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

      GLboolean scissor = HasExt[ARB_viewport_array] ? GL.glIsEnabledi(eGL_SCISSOR_TEST, 0)
                                                     : GL.glIsEnabled(eGL_SCISSOR_TEST);

      if(HasExt[ARB_viewport_array])
        drv.glDisablei(eGL_SCISSOR_TEST, 0);
      else
        drv.glDisable(eGL_SCISSOR_TEST);

      for(int i = 0; i < 8; i++)
        drv.glClearBufferfv(eGL_COLOR, i, &clearCol.x);

      if(HasExt[ARB_viewport_array])
      {
        if(scissor == GL_TRUE)
          drv.glEnablei(eGL_SCISSOR_TEST, 0);
        else
          drv.glDisablei(eGL_SCISSOR_TEST, 0);
      }
      else
      {
        if(scissor == GL_TRUE)
          drv.glEnable(eGL_SCISSOR_TEST);
        else
          drv.glDisable(eGL_SCISSOR_TEST);
      }

      // Try to clear depth as well, to help debug shadow rendering
      if(IsDepthStencilFormat(texDetails.internalFormat))
      {
        // If the depth func is equal or not equal, don't clear at all since the output would be
        // altered in an way that would cause replay to produce mostly incorrect results.
        // Similarly, skip if the depth func is always, as we'd have a 50% chance of guessing the
        // wrong clear value.
        if(rs.DepthFunc != eGL_EQUAL && rs.DepthFunc != eGL_NOTEQUAL && rs.DepthFunc != eGL_ALWAYS)
        {
          // Don't use the render state's depth clear value, as this overlay should show as much
          // information as possible. Instead, clear to the value that would cause the most depth
          // writes to happen.
          bool depthFuncLess = rs.DepthFunc == eGL_LESS || rs.DepthFunc == eGL_LEQUAL;
          float depthClear = depthFuncLess ? 1.0f : 0.0f;
          drv.glClearBufferfi(eGL_DEPTH_STENCIL, 0, depthClear, 0);
        }
      }

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

    if(HasExt[ARB_viewport_array])
      drv.glDisablei(eGL_SCISSOR_TEST, 0);
    else
      drv.glDisable(eGL_SCISSOR_TEST);

    float black[] = {0.0f, 0.0f, 0.0f, 0.0f};
    drv.glClearBufferfv(eGL_COLOR, 0, black);

    if(HasExt[ARB_viewport_array])
    {
      for(size_t s = 0; s < ARRAY_COUNT(rs.Scissors); s++)
      {
        if(rs.Scissors[s].enabled)
          drv.glEnablei(eGL_SCISSOR_TEST, (GLuint)s);
        else
          drv.glDisablei(eGL_SCISSOR_TEST, (GLuint)s);
      }
    }
    else
    {
      if(rs.Scissors[0].enabled)
        drv.glEnable(eGL_SCISSOR_TEST);
      else
        drv.glDisable(eGL_SCISSOR_TEST);
    }

    MeshUBOData uboParams = {};
    uboParams.homogenousInput = 1;
    uboParams.invProj = Matrix4f::Identity();
    uboParams.mvp = Matrix4f::Identity();

    drv.glBindBuffer(eGL_COPY_WRITE_BUFFER, DebugData.UBOs[0]);

    MeshUBOData *uboptr =
        (MeshUBOData *)drv.glMapBufferRange(eGL_COPY_WRITE_BUFFER, 0, sizeof(MeshUBOData),
                                            GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);

    if(!uboptr)
    {
      RDCERR("Map buffer failed %d", drv.glGetError());
      return ResourceId();
    }

    *uboptr = uboParams;
    drv.glUnmapBuffer(eGL_COPY_WRITE_BUFFER);

    drv.glBindBuffer(eGL_COPY_WRITE_BUFFER, DebugData.UBOs[2]);
    Vec4f *v = (Vec4f *)drv.glMapBufferRange(eGL_COPY_WRITE_BUFFER, 0, sizeof(Vec4f),
                                             GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);

    if(!v)
    {
      RDCERR("Map buffer failed %d", drv.glGetError());
      return ResourceId();
    }

    *v = Vec4f(rs.Viewports[0].width, rs.Viewports[0].height);
    drv.glUnmapBuffer(eGL_COPY_WRITE_BUFFER);

    rdcarray<uint32_t> events = passEvents;

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

          ResourceId id = m_pDriver->GetResourceManager()->GetResID(TextureRes(ctx, depthObj));
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
        if(texBindingEnum == eGL_TEXTURE_2D_ARRAY ||
           texBindingEnum == eGL_TEXTURE_2D_MULTISAMPLE_ARRAY)
          drv.glFramebufferTextureLayer(eGL_FRAMEBUFFER, eGL_COLOR_ATTACHMENT0,
                                        DebugData.overlayTex, sub.mip, sub.slice);
        else
          drv.glFramebufferTexture2D(eGL_FRAMEBUFFER, eGL_COLOR_ATTACHMENT0, texBindingEnum,
                                     DebugData.overlayTex, sub.mip);

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
              ResourceId id = m_pDriver->GetResourceManager()->GetResID(TextureRes(ctx, depthObj));
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

        prog = 0;
        pipe = 0;
        drv.glGetIntegerv(eGL_CURRENT_PROGRAM, (GLint *)&prog);
        drv.glGetIntegerv(eGL_PROGRAM_PIPELINE_BINDING, (GLint *)&pipe);

        drv.glUseProgram(DebugData.trisizeProg);
        drv.glBindProgramPipeline(0);

        const ActionDescription *action = m_pDriver->GetAction(events[i]);

        for(uint32_t inst = 0; action && inst < RDCMAX(1U, action->numInstances); inst++)
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
                if(postvs.format.compByteWidth == 8)
                {
                  drv.glVertexAttribLFormat(0, postvs.format.compCount, eGL_DOUBLE, 0);
                }
                else
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

      if(HasExt[ARB_viewport_array])
        drv.glDisablei(eGL_SCISSOR_TEST, 0);
      else
        drv.glDisable(eGL_SCISSOR_TEST);

      float black[] = {0.0f, 0.0f, 0.0f, 0.0f};
      drv.glClearBufferfv(eGL_COLOR, 0, black);

      rdcarray<uint32_t> events = passEvents;

      if(overlay == DebugOverlay::QuadOverdrawDraw)
        events.clear();

      events.push_back(eventId);

      if(!events.empty())
      {
        GLuint replacefbo = 0;
        GLuint overridedepth = 0;
        GLuint quadtexs[2] = {0};
        drv.glGenFramebuffers(1, &replacefbo);
        drv.glBindFramebuffer(eGL_FRAMEBUFFER, replacefbo);

        drv.glGenTextures(2, quadtexs);

        // image for quad usage
        drv.glBindTexture(eGL_TEXTURE_2D_ARRAY, quadtexs[1]);
        drv.glTextureImage3DEXT(quadtexs[1], eGL_TEXTURE_2D_ARRAY, 0, eGL_R32UI,
                                RDCMAX(1, outWidth >> 1), RDCMAX(1, outHeight >> 1), 4, 0,
                                eGL_RED_INTEGER, eGL_UNSIGNED_INT, NULL);
        drv.glTextureParameteriEXT(quadtexs[1], eGL_TEXTURE_2D_ARRAY, eGL_TEXTURE_MAX_LEVEL, 0);

        // temporarily attach to FBO to clear it
        GLint zero[4] = {0};
        drv.glFramebufferTextureLayer(eGL_FRAMEBUFFER, eGL_COLOR_ATTACHMENT0, quadtexs[1], 0, 0);
        drv.glClearBufferiv(eGL_COLOR, 0, zero);
        drv.glFramebufferTextureLayer(eGL_FRAMEBUFFER, eGL_COLOR_ATTACHMENT0, quadtexs[1], 0, 1);
        drv.glClearBufferiv(eGL_COLOR, 0, zero);
        drv.glFramebufferTextureLayer(eGL_FRAMEBUFFER, eGL_COLOR_ATTACHMENT0, quadtexs[1], 0, 2);
        drv.glClearBufferiv(eGL_COLOR, 0, zero);
        drv.glFramebufferTextureLayer(eGL_FRAMEBUFFER, eGL_COLOR_ATTACHMENT0, quadtexs[1], 0, 3);
        drv.glClearBufferiv(eGL_COLOR, 0, zero);

        drv.glBindTexture(eGL_TEXTURE_2D, quadtexs[0]);
        drv.glTextureImage2DEXT(quadtexs[0], eGL_TEXTURE_2D, 0, eGL_RGBA8, outWidth, outHeight, 0,
                                eGL_RGBA, eGL_UNSIGNED_BYTE, NULL);
        drv.glTextureParameteriEXT(quadtexs[0], eGL_TEXTURE_2D, eGL_TEXTURE_MAX_LEVEL, 0);
        drv.glTextureParameteriEXT(quadtexs[0], eGL_TEXTURE_2D, eGL_TEXTURE_MIN_FILTER, eGL_NEAREST);
        drv.glTextureParameteriEXT(quadtexs[0], eGL_TEXTURE_2D, eGL_TEXTURE_MAG_FILTER, eGL_NEAREST);
        drv.glTextureParameteriEXT(quadtexs[0], eGL_TEXTURE_2D, eGL_TEXTURE_WRAP_S,
                                   eGL_CLAMP_TO_EDGE);
        drv.glTextureParameteriEXT(quadtexs[0], eGL_TEXTURE_2D, eGL_TEXTURE_WRAP_T,
                                   eGL_CLAMP_TO_EDGE);
        drv.glFramebufferTexture2D(eGL_FRAMEBUFFER, eGL_COLOR_ATTACHMENT0, eGL_TEXTURE_2D,
                                   quadtexs[0], 0);

        GLuint curdrawfbo = 0, curreadfbo = 0;

        GLuint curDepth = 0, depthType = 0;

        if(overlay == DebugOverlay::QuadOverdrawPass)
          ReplayLog(events[0], eReplay_WithoutDraw);
        else
          rs.ApplyState(m_pDriver);

        drv.glGetIntegerv(eGL_DRAW_FRAMEBUFFER_BINDING, (GLint *)&curdrawfbo);
        drv.glGetIntegerv(eGL_READ_FRAMEBUFFER_BINDING, (GLint *)&curreadfbo);

        // TODO handle non-2D depth/stencil attachments and fetch slice or cubemap face
        GLint mip = 0;

        drv.glGetNamedFramebufferAttachmentParameterivEXT(curdrawfbo, eGL_DEPTH_ATTACHMENT,
                                                          eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME,
                                                          (GLint *)&curDepth);
        drv.glGetNamedFramebufferAttachmentParameterivEXT(curdrawfbo, eGL_DEPTH_ATTACHMENT,
                                                          eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE,
                                                          (GLint *)&depthType);

        GLenum fmt = eGL_DEPTH32F_STENCIL8;

        GLenum depthEnum = eGL_TEXTURE_2D;
        uint32_t depthSlices = 1, depthSamples = 1;

        if(curDepth)
        {
          if(depthType == eGL_TEXTURE)
          {
            ResourceId id = m_pDriver->GetResourceManager()->GetResID(TextureRes(ctx, curDepth));
            WrappedOpenGL::TextureData &depthdetails = m_pDriver->m_Textures[id];

            depthSamples = depthdetails.samples;
            depthSlices = depthdetails.depth;

            if(depthdetails.samples > 1)
              depthEnum = depthdetails.depth > 1 ? eGL_TEXTURE_2D_MULTISAMPLE_ARRAY
                                                 : eGL_TEXTURE_2D_MULTISAMPLE;
            else
              depthEnum = depthdetails.depth > 1 ? eGL_TEXTURE_2D_ARRAY : eGL_TEXTURE_2D;

            if(depthEnum == eGL_TEXTURE_2D_MULTISAMPLE ||
               depthEnum == eGL_TEXTURE_2D_MULTISAMPLE_ARRAY)
              mip = 0;
            else
              drv.glGetNamedFramebufferAttachmentParameterivEXT(
                  curdrawfbo, eGL_DEPTH_ATTACHMENT, eGL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL, &mip);
            drv.glGetTextureLevelParameterivEXT(curDepth, depthEnum, mip,
                                                eGL_TEXTURE_INTERNAL_FORMAT, (GLint *)&fmt);
          }
          else
          {
            drv.glGetNamedRenderbufferParameterivEXT(curDepth, eGL_RENDERBUFFER_INTERNAL_FORMAT,
                                                     (GLint *)&fmt);
          }
        }

        GLenum dsAttach = eGL_DEPTH_STENCIL_ATTACHMENT;

        if(GetBaseFormat(fmt) == eGL_DEPTH_COMPONENT)
          dsAttach = eGL_DEPTH_ATTACHMENT;

        drv.glBindFramebuffer(eGL_FRAMEBUFFER, replacefbo);

        drv.glFramebufferTexture2D(eGL_FRAMEBUFFER, dsAttach, depthEnum, curDepth, 0);

        if(depthEnum == eGL_TEXTURE_2D_MULTISAMPLE || depthEnum == eGL_TEXTURE_2D_MULTISAMPLE_ARRAY)
        {
          drv.CopyTex2DMSToArray(overridedepth, curDepth, outWidth, outHeight, depthSlices,
                                 depthSamples, fmt);

          drv.glTextureParameteriEXT(overridedepth, eGL_TEXTURE_2D_ARRAY, eGL_TEXTURE_MAX_LEVEL, 0);
          drv.glTextureParameteriEXT(overridedepth, eGL_TEXTURE_2D_ARRAY, eGL_TEXTURE_MIN_FILTER,
                                     eGL_NEAREST);
          drv.glTextureParameteriEXT(overridedepth, eGL_TEXTURE_2D_ARRAY, eGL_TEXTURE_MAG_FILTER,
                                     eGL_NEAREST);
          drv.glTextureParameteriEXT(overridedepth, eGL_TEXTURE_2D_ARRAY, eGL_TEXTURE_WRAP_S,
                                     eGL_CLAMP_TO_EDGE);
          drv.glTextureParameteriEXT(overridedepth, eGL_TEXTURE_2D_ARRAY, eGL_TEXTURE_WRAP_T,
                                     eGL_CLAMP_TO_EDGE);

          drv.glFramebufferTextureLayer(eGL_FRAMEBUFFER, dsAttach, overridedepth, 0, 0);
        }

        drv.glBindFramebuffer(eGL_DRAW_FRAMEBUFFER, curdrawfbo);
        drv.glBindFramebuffer(eGL_READ_FRAMEBUFFER, curreadfbo);

        for(size_t i = 0; i < events.size(); i++)
        {
          GLint depthwritemask = 1;
          GLint stencilfmask = 0xff, stencilbmask = 0xff;
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
          drv.glBindImageTexture(0, quadtexs[1], 0, GL_TRUE, 0, eGL_READ_WRITE, eGL_R32UI);

          prog = 0;
          pipe = 0;
          drv.glGetIntegerv(eGL_CURRENT_PROGRAM, (GLint *)&prog);
          drv.glGetIntegerv(eGL_PROGRAM_PIPELINE_BINDING, (GLint *)&pipe);

          // delete the old program if it exists
          if(DebugData.overlayProg != 0)
            drv.glDeleteProgram(DebugData.overlayProg);

          DebugData.overlayProg = drv.glCreateProgram();

          // replace fragment shader. This is exactly what we did
          // at the start of this function for the single-event case, but now we have
          // to do it for every event
          spirvOverlay = CreateFragmentShaderReplacementProgram(
              prog, DebugData.overlayProg, pipe, DebugData.quadoverdrawFragShader,
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

          if(overlay == DebugOverlay::QuadOverdrawPass && overridedepth)
            drv.CopyTex2DMSToArray(overridedepth, curDepth, outWidth, outHeight, depthSlices,
                                   depthSamples, fmt);

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
          if(texBindingEnum == eGL_TEXTURE_2D_ARRAY ||
             texBindingEnum == eGL_TEXTURE_2D_MULTISAMPLE_ARRAY)
          {
            drv.glFramebufferTextureLayer(eGL_FRAMEBUFFER, eGL_COLOR_ATTACHMENT0,
                                          DebugData.overlayTex, sub.mip, sub.slice);
            drv.glFramebufferTextureLayer(eGL_FRAMEBUFFER, eGL_DEPTH_STENCIL_ATTACHMENT, 0, 0, 0);
          }
          else
          {
            drv.glFramebufferTexture2D(eGL_FRAMEBUFFER, eGL_COLOR_ATTACHMENT0, texBindingEnum,
                                       DebugData.overlayTex, sub.mip);
            drv.glFramebufferTexture2D(eGL_FRAMEBUFFER, eGL_DEPTH_STENCIL_ATTACHMENT,
                                       texBindingEnum, 0, 0);
          }

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
          drv.glViewport(0, 0, outWidth, outHeight);

          drv.glBindImageTexture(0, quadtexs[1], 0, GL_TRUE, 0, eGL_READ_WRITE, eGL_R32UI);

          GLuint emptyVAO = 0;
          drv.glGenVertexArrays(1, &emptyVAO);
          drv.glBindVertexArray(emptyVAO);
          drv.glDrawArrays(eGL_TRIANGLE_STRIP, 0, 4);
          drv.glBindVertexArray(0);
          drv.glDeleteVertexArrays(1, &emptyVAO);
        }

        drv.glDeleteFramebuffers(1, &replacefbo);
        drv.glDeleteTextures(2, quadtexs);
        drv.glDeleteTextures(1, &overridedepth);

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
      m_pDriver->GetResourceManager()->GetResID(TextureRes(ctx, DebugData.overlayTex));

  return DebugData.overlayTexId;
}
