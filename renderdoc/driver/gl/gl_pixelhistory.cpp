/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2022 Baldur Karlsson
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
#include "data/glsl_shaders.h"
#include "gl_common.h"
#include "gl_driver.h"
#include "gl_replay.h"

namespace
{
bool isDirectWrite(ResourceUsage usage)
{
  return ((usage >= ResourceUsage::VS_RWResource && usage <= ResourceUsage::CS_RWResource) ||
          usage == ResourceUsage::CopyDst || usage == ResourceUsage::Copy ||
          usage == ResourceUsage::Resolve || usage == ResourceUsage::ResolveDst ||
          usage == ResourceUsage::GenMips);
}

struct GLPixelHistoryResources
{
  // Used for offscreen rendering for draw call events.
  GLuint colorImage;
  GLuint dsImage;
  GLuint frameBuffer;
  GLuint primitiveIdFragmentShader;
  GLuint primitiveIdFragmentShaderSPIRV;
  std::unordered_map<GLuint, GLuint> programs;
};

enum class OpenGLTest
{
  FaceCulling,
  ScissorTest,
  StencilTest,
  DepthTest,
  NumTests
};

enum class PerFragmentQueryType
{
  ShaderOut,
  PostMod,
  PrimitiveId
};

GLuint CreatePrimitiveIdProgram(GLuint Program, GLuint Pipeline, GLuint fragShader,
                                GLuint fragShaderSPIRV, WrappedOpenGL *driver, GLReplay *replay)
{
  WrappedOpenGL &drv = *driver;

  ContextPair &ctx = drv.GetCtx();

  GLuint primitiveIdProgram = drv.glCreateProgram();

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
      ResourceId id = driver->GetResourceManager()->GetResID(ProgramPipeRes(ctx, Pipeline));
      const WrappedOpenGL::PipelineData &pipeDetails = driver->m_Pipelines[id];

      // fetch the corresponding shaders and programs for each stage
      for(size_t i = 0; i < 4; i++)
      {
        if(pipeDetails.stageShaders[i] != ResourceId())
        {
          const WrappedOpenGL::ShaderData &shadDetails =
              driver->m_Shaders[pipeDetails.stageShaders[i]];

          if(shadDetails.reflection->encoding == ShaderEncoding::SPIRV)
            HasSPIRVShaders = true;
          else
            HasGLSLShaders = true;

          programs[i] =
              driver->GetResourceManager()->GetCurrentResource(pipeDetails.stagePrograms[i]).name;
          shaders[i] =
              driver->GetResourceManager()->GetCurrentResource(pipeDetails.stageShaders[i]).name;

          if(pipeDetails.stagePrograms[i] == pipeDetails.stageShaders[i])
          {
            const WrappedOpenGL::ProgramData &progDetails =
                driver->m_Programs[pipeDetails.stagePrograms[i]];

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
                RDCERR("Trying to create primitive id program, couldn't compile shader:\n%s", buffer);
              }
            }
          }

          if(i == 0)
            vsRefl = replay->GetShader(ResourceId(), pipeDetails.stageShaders[i], ShaderEntryPoint());
        }
      }
    }
  }
  else
  {
    const WrappedOpenGL::ProgramData &progDetails =
        driver->m_Programs[driver->GetResourceManager()->GetResID(ProgramRes(ctx, Program))];

    // fetch any and all non-fragment shader shaders
    for(size_t i = 0; i < 4; i++)
    {
      if(progDetails.stageShaders[i] != ResourceId())
      {
        programs[i] = Program;
        shaders[i] =
            driver->GetResourceManager()->GetCurrentResource(progDetails.stageShaders[i]).name;

        const WrappedOpenGL::ShaderData &shadDetails = driver->m_Shaders[progDetails.stageShaders[i]];

        if(shadDetails.reflection->encoding == ShaderEncoding::SPIRV)
          HasSPIRVShaders = true;
        else
          HasGLSLShaders = true;

        if(i == 0)
          vsRefl = replay->GetShader(ResourceId(), progDetails.stageShaders[0], ShaderEntryPoint());
      }
    }
  }

  if(HasGLSLShaders && HasSPIRVShaders)
    RDCERR("Unsupported - mixed GLSL and SPIR-V shaders in pipeline");

  // attach the shaders
  for(size_t i = 0; i < 4; i++)
    if(shaders[i])
      drv.glAttachShader(primitiveIdProgram, shaders[i]);

  if(HasSPIRVShaders)
  {
    RDCASSERT(fragShaderSPIRV);
    drv.glAttachShader(primitiveIdProgram, fragShaderSPIRV);
  }
  else
  {
    drv.glAttachShader(primitiveIdProgram, fragShader);
  }

  // copy the vertex attribs over from the source program
  if(vsRefl && programs[0] && !HasSPIRVShaders)
    CopyProgramAttribBindings(programs[0], primitiveIdProgram, vsRefl);

  // link the overlay program
  drv.glLinkProgram(primitiveIdProgram);

  // detach the shaders
  for(size_t i = 0; i < 4; i++)
    if(shaders[i])
      drv.glDetachShader(primitiveIdProgram, shaders[i]);

  if(HasSPIRVShaders)
    drv.glDetachShader(primitiveIdProgram, fragShaderSPIRV);
  else
    drv.glDetachShader(primitiveIdProgram, fragShader);

  // delete any temporaries
  for(size_t i = 0; i < 4; i++)
    if(tmpShaders[i])
      drv.glDeleteShader(tmpShaders[i]);

  // check that the link succeeded
  char buffer[1024] = {};
  GLint status = 0;
  drv.glGetProgramiv(primitiveIdProgram, eGL_LINK_STATUS, &status);
  if(status == 0)
  {
    drv.glGetProgramInfoLog(primitiveIdProgram, 1024, NULL, buffer);
    RDCERR("Error linking primitive id program: %s", buffer);
    return false;
  }

  // copy the uniform values over from the source program. This is redundant but harmless if the
  // same program is bound to multiple stages. It's just inefficient
  {
    PerStageReflections dstStages;
    driver->FillReflectionArray(ProgramRes(ctx, primitiveIdProgram), dstStages);

    for(size_t i = 0; i < 4; i++)
    {
      if(programs[i])
      {
        PerStageReflections stages;
        driver->FillReflectionArray(ProgramRes(ctx, programs[i]), stages);

        CopyProgramUniforms(stages, programs[i], dstStages, primitiveIdProgram);
      }
    }
  }

  // return HasSPIRVShaders;
  return primitiveIdProgram;
}

GLuint GetPrimitiveIdProgram(WrappedOpenGL *driver, GLReplay *replay,
                             GLPixelHistoryResources &resources, GLuint currentProgram)
{
  auto programIterator = resources.programs.find(currentProgram);
  if(programIterator != resources.programs.end())
  {
    return programIterator->second;
  }

  GLRenderState rs;
  rs.FetchState(driver);

  GLuint newProgram = CreatePrimitiveIdProgram(
      rs.Program.name, rs.Pipeline.name, resources.primitiveIdFragmentShader,
      resources.primitiveIdFragmentShaderSPIRV, driver, replay);

  return newProgram;
}

bool PixelHistorySetupResources(WrappedOpenGL *driver, GLPixelHistoryResources &resources,
                                const TextureDescription &desc, const Subresource &sub,
                                uint32_t numEvents, GLuint glslVersion)
{
  // Allocate a framebuffer that will render to the textures
  driver->glGenFramebuffers(1, &resources.frameBuffer);
  driver->glBindFramebuffer(eGL_FRAMEBUFFER, resources.frameBuffer);

  // Allocate a texture for the pixel history colour values
  driver->glGenTextures(1, &resources.colorImage);
  driver->glBindTexture(eGL_TEXTURE_2D, resources.colorImage);
  driver->CreateTextureImage(resources.colorImage, eGL_RGBA32F, eGL_NONE, eGL_NONE, eGL_TEXTURE_2D,
                             2, desc.width >> sub.mip, desc.height >> sub.mip, 1, desc.msSamp, 1);
  driver->glFramebufferTexture(eGL_FRAMEBUFFER, eGL_COLOR_ATTACHMENT0, resources.colorImage, 0);

  // Allocate a texture for the pixel history depth/stencil values
  driver->glGenTextures(1, &resources.dsImage);
  driver->glBindTexture(eGL_TEXTURE_2D, resources.dsImage);
  driver->CreateTextureImage(resources.dsImage, eGL_DEPTH32F_STENCIL8, eGL_NONE, eGL_NONE,
                             eGL_TEXTURE_2D, 2, desc.width >> sub.mip, desc.height >> sub.mip, 1,
                             desc.msSamp, 1);
  driver->glFramebufferTexture(eGL_FRAMEBUFFER, eGL_DEPTH_STENCIL_ATTACHMENT, resources.dsImage, 0);

  // If the GLSL version is greater than or equal to 330, we can use IntBitsToFloat, otherwise we
  // need to write the float value directly.
  rdcstr glslSource;
  if(glslVersion >= 330)
  {
    glslSource = GenerateGLSLShader(GetEmbeddedResource(glsl_pixelhistory_primid_frag),
                                    ShaderType::GLSL, glslVersion);
  }
  else
  {
    glslSource = GenerateGLSLShader(GetEmbeddedResource(glsl_pixelhistory_primid_legacy_frag),
                                    ShaderType::GLSL, glslVersion);
  }
  // SPIR-V shaders are always generated as desktop GL 430, for ease
  rdcstr spirvSource = GenerateGLSLShader(GetEmbeddedResource(glsl_pixelhistory_primid_frag),
                                          ShaderType::GLSPIRV, 430);
  resources.primitiveIdFragmentShaderSPIRV = CreateSPIRVShader(eGL_FRAGMENT_SHADER, spirvSource);
  resources.primitiveIdFragmentShader = CreateShader(eGL_FRAGMENT_SHADER, glslSource);

  return true;
}

bool PixelHistoryDestroyResources(WrappedOpenGL *driver, const GLPixelHistoryResources &resources)
{
  driver->glDeleteTextures(1, &resources.colorImage);
  driver->glDeleteTextures(1, &resources.dsImage);
  driver->glDeleteFramebuffers(1, &resources.frameBuffer);
  driver->glDeleteShader(resources.primitiveIdFragmentShader);
  driver->glDeleteShader(resources.primitiveIdFragmentShaderSPIRV);

  for(const std::pair<const GLuint, GLuint> &resourceProgram : resources.programs)
  {
    driver->glDeleteProgram(resourceProgram.second);
  }

  return true;
}

rdcarray<EventUsage> QueryModifyingEvents(WrappedOpenGL *driver, GLPixelHistoryResources &resources,
                                          const rdcarray<EventUsage> &events, int x, int y,
                                          rdcarray<PixelModification> &history)
{
  rdcarray<EventUsage> modEvents;
  rdcarray<GLuint> occlusionQueries;
  occlusionQueries.resize(events.size());
  driver->glGenQueries((GLsizei)occlusionQueries.size(), occlusionQueries.data());

  driver->ReplayLog(0, events[0].eventId, eReplay_WithoutDraw);
  // execute the occlusion queries
  for(size_t i = 0; i < events.size(); i++)
  {
    if(!(events[i].usage == ResourceUsage::Clear || isDirectWrite(events[i].usage)))
    {
      driver->glDisable(eGL_DEPTH_TEST);
      driver->glDisable(eGL_STENCIL_TEST);
      driver->glDisable(eGL_CULL_FACE);
      driver->glDisable(eGL_SAMPLE_MASK);
      driver->glDisable(eGL_DEPTH_CLAMP);
      driver->glEnable(eGL_SCISSOR_TEST);
      driver->glScissor(x, y, 1, 1);

      driver->SetFetchCounters(true);
      driver->glBeginQuery(eGL_SAMPLES_PASSED, occlusionQueries[i]);
      driver->ReplayLog(events[i].eventId, events[i].eventId, eReplay_OnlyDraw);
      driver->glEndQuery(eGL_SAMPLES_PASSED);
      driver->SetFetchCounters(false);
    }

    if(i < events.size() - 1)
    {
      driver->ReplayLog(events[i].eventId + 1, events[i + 1].eventId, eReplay_WithoutDraw);
    }
  }
  // read back the occlusion queries and generate the list of potentially modifying events
  for(size_t i = 0; i < events.size(); i++)
  {
    if(events[i].usage == ResourceUsage::Clear || isDirectWrite(events[i].usage))
    {
      PixelModification mod;
      RDCEraseEl(mod);
      mod.eventId = events[i].eventId;
      history.push_back(mod);

      modEvents.push_back(events[i]);
    }
    else
    {
      uint32_t numSamples;
      driver->glGetQueryObjectuiv(occlusionQueries[i], eGL_QUERY_RESULT, &numSamples);

      if(numSamples > 0)
      {
        PixelModification mod;
        RDCEraseEl(mod);
        mod.eventId = events[i].eventId;
        history.push_back(mod);
        modEvents.push_back(events[i]);
      }
    }
  }

  driver->glDeleteQueries((GLsizei)occlusionQueries.size(), occlusionQueries.data());
  return modEvents;
}

void QueryPostModPixelValues(WrappedOpenGL *driver, GLPixelHistoryResources &resources,
                             const rdcarray<EventUsage> &modEvents, int x, int y,
                             rdcarray<PixelModification> &history)
{
  driver->glBindFramebuffer(eGL_FRAMEBUFFER, resources.frameBuffer);
  driver->glClear(eGL_COLOR_BUFFER_BIT | eGL_DEPTH_BUFFER_BIT | eGL_STENCIL_BUFFER_BIT);
  driver->ReplayLog(0, modEvents[0].eventId, eReplay_WithoutDraw);

  for(size_t i = 0; i < modEvents.size(); i++)
  {
    GLint savedReadFramebuffer, savedDrawFramebuffer;
    driver->glGetIntegerv(eGL_DRAW_FRAMEBUFFER_BINDING, &savedDrawFramebuffer);
    driver->glGetIntegerv(eGL_READ_FRAMEBUFFER_BINDING, &savedReadFramebuffer);
    // bind our own framebuffer to save the pixel values
    driver->glBindFramebuffer(eGL_FRAMEBUFFER, resources.frameBuffer);
    driver->ReplayLog(modEvents[i].eventId, modEvents[i].eventId, eReplay_Full);

    // read the post mod pixel value into the history event
    ModificationValue modValue;
    PixelValue pixelValue;
    driver->glReadPixels(x, y, 1, 1, eGL_RGBA, eGL_FLOAT, (void *)pixelValue.floatValue.data());
    driver->glReadPixels(x, y, 1, 1, eGL_DEPTH_COMPONENT, eGL_FLOAT, (void *)&modValue.depth);
    driver->glReadPixels(x, y, 1, 1, eGL_STENCIL_INDEX, eGL_INT, (void *)&modValue.stencil);
    modValue.col = pixelValue;

    history[i].postMod = modValue;

    // restore the capture's framebuffer
    driver->glBindFramebuffer(eGL_DRAW_FRAMEBUFFER, savedDrawFramebuffer);
    driver->glBindFramebuffer(eGL_READ_FRAMEBUFFER, savedReadFramebuffer);

    if(i < modEvents.size() - 1)
    {
      driver->ReplayLog(modEvents[i].eventId + 1, modEvents[i + 1].eventId, eReplay_WithoutDraw);
    }
  }
}

std::map<uint32_t, uint32_t> QueryNumFragmentsByEvent(WrappedOpenGL *driver,
                                                      GLPixelHistoryResources &resources,
                                                      const rdcarray<EventUsage> &modEvents,
                                                      rdcarray<PixelModification> &history, int x,
                                                      int y)
{
  driver->glBindFramebuffer(eGL_FRAMEBUFFER, resources.frameBuffer);
  driver->glClear(eGL_COLOR_BUFFER_BIT | eGL_DEPTH_BUFFER_BIT | eGL_STENCIL_BUFFER_BIT);
  driver->ReplayLog(0, modEvents[0].eventId, eReplay_WithoutDraw);

  std::map<uint32_t, uint32_t> eventFragments;

  for(size_t i = 0; i < modEvents.size(); ++i)
  {
    GLint savedReadFramebuffer, savedDrawFramebuffer;
    driver->glGetIntegerv(eGL_DRAW_FRAMEBUFFER_BINDING, &savedDrawFramebuffer);
    driver->glGetIntegerv(eGL_READ_FRAMEBUFFER_BINDING, &savedReadFramebuffer);
    // bind our own framebuffer to save the pixel values
    driver->glBindFramebuffer(eGL_FRAMEBUFFER, resources.frameBuffer);
    driver->glStencilOp(eGL_INCR, eGL_INCR, eGL_INCR);
    driver->glStencilMask(0xff);    // default for 1 byte
    driver->glStencilFunc(eGL_ALWAYS, 0, 0xff);
    driver->glClearStencil(0);
    driver->glClear(eGL_STENCIL_BUFFER_BIT);
    driver->glEnable(eGL_STENCIL_TEST);
    // depth test enable
    driver->glEnable(eGL_DEPTH_TEST);
    driver->glDepthFunc(eGL_ALWAYS);
    driver->glDepthMask(GL_TRUE);
    driver->glDisable(eGL_BLEND);
    // replay start
    driver->ReplayLog(modEvents[i].eventId, modEvents[i].eventId, eReplay_OnlyDraw);

    uint32_t numFragments;
    ModificationValue modValue;
    PixelValue pixelValue;

    driver->glReadPixels(x, y, 1, 1, eGL_STENCIL_INDEX, eGL_INT, (void *)&numFragments);
    driver->glReadPixels(x, y, 1, 1, eGL_RGBA, eGL_FLOAT, (void *)pixelValue.floatValue.data());
    driver->glReadPixels(x, y, 1, 1, eGL_DEPTH_COMPONENT, eGL_FLOAT, (void *)&modValue.depth);
    modValue.col = pixelValue;

    // We're not reading the stencil value here, so use the postMod instead.
    // Shaders don't actually output stencil values, those are determined by the stencil op.
    modValue.stencil = history[i].postMod.stencil;

    history[i].shaderOut = modValue;

    eventFragments.emplace(modEvents[i].eventId, numFragments);

    // restore the capture's framebuffer
    driver->glBindFramebuffer(eGL_DRAW_FRAMEBUFFER, savedDrawFramebuffer);
    driver->glBindFramebuffer(eGL_READ_FRAMEBUFFER, savedReadFramebuffer);

    if(i < modEvents.size() - 1)
    {
      driver->ReplayLog(modEvents[i].eventId + 1, modEvents[i + 1].eventId, eReplay_WithoutDraw);
    }
  }

  return eventFragments;
}

bool QueryScissorTest(WrappedOpenGL *driver, GLPixelHistoryResources &resources,
                      const EventUsage event, int x, int y)
{
  driver->ReplayLog(0, event.eventId, eReplay_WithoutDraw);
  bool scissorTestFailed = false;
  if(driver->glIsEnabled(eGL_SCISSOR_TEST))
  {
    int scissorBox[4];    // [x, y, width, height]
    driver->glGetIntegerv(eGL_SCISSOR_BOX, scissorBox);
    scissorTestFailed = x < scissorBox[0] || x - scissorBox[0] >= scissorBox[2] ||
                        y < scissorBox[1] || y - scissorBox[1] >= scissorBox[3];
  }
  return scissorTestFailed;
}

bool QueryTest(WrappedOpenGL *driver, GLPixelHistoryResources &resources, const EventUsage event,
               int x, int y, OpenGLTest test)
{
  driver->ReplayLog(0, event.eventId - 1, eReplay_Full);
  GLuint samplesPassedQuery;
  driver->glGenQueries(1, &samplesPassedQuery);
  driver->glEnable(eGL_SCISSOR_TEST);
  driver->glScissor(x, y, 1, 1);
  if(test < OpenGLTest::DepthTest)
  {
    driver->glDisable(eGL_DEPTH_TEST);
  }
  if(test < OpenGLTest::StencilTest)
  {
    driver->glDisable(eGL_STENCIL_TEST);
  }
  if(test < OpenGLTest::FaceCulling)
  {
    driver->glDisable(eGL_CULL_FACE);
  }

  driver->SetFetchCounters(true);
  driver->glBeginQuery(eGL_SAMPLES_PASSED, samplesPassedQuery);
  driver->ReplayLog(event.eventId, event.eventId, eReplay_Full);
  driver->glEndQuery(eGL_SAMPLES_PASSED);
  driver->SetFetchCounters(false);
  int numSamplesPassed;
  driver->glGetQueryObjectiv(samplesPassedQuery, eGL_QUERY_RESULT, &numSamplesPassed);
  driver->glDeleteQueries(1, &samplesPassedQuery);
  return numSamplesPassed == 0;
}

void QueryFailedTests(WrappedOpenGL *driver, GLPixelHistoryResources &resources,
                      const rdcarray<EventUsage> &modEvents, int x, int y,
                      rdcarray<PixelModification> &history)
{
  for(size_t i = 0; i < modEvents.size(); ++i)
  {
    OpenGLTest failedTest = OpenGLTest::NumTests;
    if(!isDirectWrite(modEvents[i].usage))
    {
      if(modEvents[i].usage == ResourceUsage::Clear)
      {
        bool failed = QueryScissorTest(driver, resources, modEvents[i], x, y);
        if(failed)
        {
          failedTest = OpenGLTest::ScissorTest;
        }
      }
      else
      {
        for(int test = 0; test < int(OpenGLTest::NumTests); ++test)
        {
          bool failed;
          if(test == int(OpenGLTest::ScissorTest))
          {
            failed = QueryScissorTest(driver, resources, modEvents[i], x, y);
          }
          else
          {
            failed = QueryTest(driver, resources, modEvents[i], x, y, OpenGLTest(test));
          }
          if(failed)
          {
            failedTest = OpenGLTest(test);
            break;
          }
        }
      }
    }

    // ensure that history objects are one-to-one mapped with modEvent objects
    RDCASSERT(history[i].eventId == modEvents[i].eventId);
    history[i].scissorClipped = failedTest == OpenGLTest::ScissorTest;
    history[i].stencilTestFailed = failedTest == OpenGLTest::StencilTest;
    history[i].depthTestFailed = failedTest == OpenGLTest::DepthTest;
    history[i].backfaceCulled = failedTest == OpenGLTest::FaceCulling;
  }
}

void QueryPerFragmentValues(WrappedOpenGL *driver, GLReplay *replay,
                            GLPixelHistoryResources &resources, const EventUsage &modEvent, int x,
                            int y, rdcarray<PixelModification> &history,
                            const std::map<uint32_t, uint32_t> &eventFragments,
                            PerFragmentQueryType queryType, bool usingLegacyPrimitiveId,
                            uint32_t numFragments)
{
  driver->ReplayLog(0, modEvent.eventId - 1, eReplay_Full);
  driver->glBindFramebuffer(eGL_FRAMEBUFFER, resources.frameBuffer);
  driver->glReadBuffer(eGL_COLOR_ATTACHMENT0);

  driver->glEnable(eGL_SCISSOR_TEST);
  driver->glScissor(x, y, 1, 1);

  driver->glClearStencil(0);

  if(queryType == PerFragmentQueryType::ShaderOut || queryType == PerFragmentQueryType::PrimitiveId)
  {
    driver->glEnable(eGL_DEPTH_TEST);
    driver->glDepthFunc(eGL_ALWAYS);
    driver->glDepthMask(GL_TRUE);
    driver->glDisable(eGL_BLEND);
  }

  driver->glStencilMask(0xff);
  driver->glStencilOp(eGL_INCR, eGL_INCR, eGL_INCR);
  driver->glEnable(eGL_STENCIL_TEST);

  PixelModification referenceHistory;
  referenceHistory.eventId = modEvent.eventId;

  auto historyIndex =
      std::lower_bound(history.begin(), history.end(), referenceHistory,
                       [](const PixelModification &h1, const PixelModification &h2) -> bool {
                         return h1.eventId < h2.eventId;
                       });
  RDCASSERT(historyIndex != history.end());

  GLint currentProgram = 0;

  if(queryType == PerFragmentQueryType::PostMod)
  {
    if(historyIndex != history.begin())
    {
      --historyIndex;

      // Because we are replaying this draw into our own framebuffer
      // We need to set the values to the ones that they would be
      // before this draw call.
      driver->glClearColor(
          historyIndex->postMod.col.floatValue[0], historyIndex->postMod.col.floatValue[1],
          historyIndex->postMod.col.floatValue[2], historyIndex->postMod.col.floatValue[3]);

      GLboolean depthWriteMask = false;
      driver->glGetBooleanv(eGL_DEPTH_WRITEMASK, &depthWriteMask);
      driver->glDepthMask(true);
      driver->glClearDepth(historyIndex->postMod.depth);

      driver->glClear(eGL_COLOR_BUFFER_BIT | eGL_DEPTH_BUFFER_BIT);

      driver->glDepthMask(depthWriteMask);
      ++historyIndex;
    }
  }
  else if(queryType == PerFragmentQueryType::PrimitiveId)
  {
    // we expect this value to be overwritten by the primitive id shader. if not
    // it will cause an assertion that all color values are the same to fail
    driver->glClearColor(0.84f, 0.17f, 0.2f, 0.49f);
    driver->glClear(eGL_COLOR_BUFFER_BIT);

    driver->glGetIntegerv(eGL_CURRENT_PROGRAM, &currentProgram);

    driver->glUseProgram(GetPrimitiveIdProgram(driver, replay, resources, currentProgram));
  }

  for(size_t j = 0; j < std::max(numFragments, 1u); ++j)
  {
    ModificationValue modValue;
    PixelValue pixelValue;
    modValue.stencil = 0;
    // Set the stencil function so only jth fragment will pass.
    driver->glStencilFunc(eGL_EQUAL, (int)j, 0xff);
    driver->glClear(eGL_STENCIL_BUFFER_BIT);

    driver->ReplayLog(modEvent.eventId, modEvent.eventId, eReplay_OnlyDraw);

    if(queryType == PerFragmentQueryType::PrimitiveId)
    {
      if(usingLegacyPrimitiveId)
      {
        float primitiveIds[4];
        driver->glReadPixels(x, y, 1, 1, eGL_RGBA, eGL_FLOAT, (void *)primitiveIds);

        RDCASSERT(primitiveIds[0] == primitiveIds[1] && primitiveIds[0] == primitiveIds[2] &&
                  primitiveIds[0] == primitiveIds[3]);
        historyIndex->primitiveID = int(primitiveIds[0]);
      }
      else
      {
        int primitiveIds[4];
        driver->glReadPixels(x, y, 1, 1, eGL_RGBA, eGL_FLOAT, (void *)primitiveIds);

        RDCASSERT(primitiveIds[0] == primitiveIds[1] && primitiveIds[0] == primitiveIds[2] &&
                  primitiveIds[0] == primitiveIds[3]);
        historyIndex->primitiveID = primitiveIds[0];
      }
    }
    else
    {
      driver->glReadPixels(x, y, 1, 1, eGL_RGBA, eGL_FLOAT, (void *)pixelValue.floatValue.data());
      driver->glReadPixels(x, y, 1, 1, eGL_DEPTH_COMPONENT, eGL_FLOAT, (void *)&modValue.depth);

      modValue.col = pixelValue;

      if(queryType == PerFragmentQueryType::ShaderOut)
      {
        historyIndex->shaderOut = modValue;
      }
      else if(queryType == PerFragmentQueryType::PostMod)
      {
        historyIndex->postMod = modValue;
      }
    }
    ++historyIndex;
  }

  if(queryType == PerFragmentQueryType::PrimitiveId)
  {
    driver->glUseProgram(currentProgram);
  }
}

bool depthTestPassed(int depthFunc, float shaderOutputDepth, float depthInBuffer)
{
  switch(depthFunc)
  {
    case eGL_NEVER: return false;
    case eGL_LESS: return shaderOutputDepth < depthInBuffer;
    case eGL_EQUAL: return shaderOutputDepth == depthInBuffer;
    case eGL_LEQUAL: return shaderOutputDepth <= depthInBuffer;
    case eGL_GREATER: return shaderOutputDepth > depthInBuffer;
    case eGL_NOTEQUAL: return shaderOutputDepth != depthInBuffer;
    case eGL_GEQUAL: return shaderOutputDepth >= depthInBuffer;
    case eGL_ALWAYS: return true;

    default: RDCERR("Unexpected depth function: %d", depthFunc);
  }
  return false;
}

void CalculateFragmentDepthTests(WrappedOpenGL *driver, GLPixelHistoryResources &resources,
                                 const rdcarray<EventUsage> &modEvents,
                                 rdcarray<PixelModification> &history,
                                 const std::map<uint32_t, uint32_t> &eventFragments)
{
  driver->ReplayLog(0, modEvents[0].eventId, eReplay_WithoutDraw);
  size_t historyIndex = 0;
  for(size_t i = 0; i < modEvents.size(); ++i)
  {
    // We only need to calculate the depth test for events with multiple fragments.
    auto it = eventFragments.find(modEvents[i].eventId);
    uint32_t numFragments = (it != eventFragments.end()) ? it->second : 0;
    if(numFragments <= 1)
    {
      continue;
    }

    for(; historyIndex < history.size() && modEvents[i].eventId == history[historyIndex].eventId;
        ++historyIndex)
    {
      if(historyIndex == 0)
      {
        continue;
      }

      if(driver->glIsEnabled(eGL_DEPTH_TEST))
      {
        int depthFunc;
        driver->glGetIntegerv(eGL_DEPTH_FUNC, &depthFunc);

        history[historyIndex].depthTestFailed = !depthTestPassed(
            depthFunc, history[historyIndex].shaderOut.depth, history[historyIndex].preMod.depth);
      }
      else
      {
        // since there is no depth test, there is no failure.
        history[historyIndex].depthTestFailed = false;
      }
    }

    if(i < modEvents.size() - 1)
    {
      driver->ReplayLog(modEvents[i].eventId + 1, modEvents[i + 1].eventId, eReplay_WithoutDraw);
    }
  }
}

};    // end of anonymous namespace

rdcarray<PixelModification> GLReplay::PixelHistory(rdcarray<EventUsage> events, ResourceId target,
                                                   uint32_t x, uint32_t y, const Subresource &sub,
                                                   CompType typeCast)
{
  rdcarray<PixelModification> history;

  if(events.empty())
    return history;

  TextureDescription textureDesc = GetTexture(target);
  if(textureDesc.format.type == ResourceFormatType::Undefined)
    return history;

  // When RenderDoc passes in the y, the value being passed in is with the Y axis starting from the
  // top
  // However, we need to have it starting from the bottom, so flip it by subtracting y from the
  // height.
  uint32_t flippedY = textureDesc.height - y - 1;

  rdcstr regionName = StringFormat::Fmt(
      "PixelHistory: pixel: (%u, %u) on %s subresource (%u, %u, %u) cast to %s with %zu events", x,
      flippedY, ToStr(target).c_str(), sub.mip, sub.slice, sub.sample, ToStr(typeCast).c_str(),
      events.size());

  RDCDEBUG("%s", regionName.c_str());

  uint32_t sampleIdx = sub.sample;

  if(sampleIdx > textureDesc.msSamp)
    sampleIdx = 0;

  uint32_t sampleMask = ~0U;
  if(sampleIdx < 32)
    sampleMask = 1U << sampleIdx;

  bool multisampled = (textureDesc.msSamp > 1);

  if(sampleIdx == ~0U || !multisampled)
    sampleIdx = 0;

  GLPixelHistoryResources resources;

  MakeCurrentReplayContext(&m_ReplayCtx);

  int glslVersion = DebugData.glslVersion;
  bool usingLegacyPrimitveId = glslVersion < 330;

  PixelHistorySetupResources(m_pDriver, resources, textureDesc, sub, (uint32_t)events.size(),
                             glslVersion);

  rdcarray<EventUsage> modEvents =
      QueryModifyingEvents(m_pDriver, resources, events, x, flippedY, history);

  if(modEvents.empty())
  {
    PixelHistoryDestroyResources(m_pDriver, resources);
    return history;
  }

  QueryFailedTests(m_pDriver, resources, modEvents, x, flippedY, history);
  QueryPostModPixelValues(m_pDriver, resources, modEvents, x, flippedY, history);

  std::map<uint32_t, uint32_t> eventFragments =
      QueryNumFragmentsByEvent(m_pDriver, resources, modEvents, history, x, flippedY);

  // copy history entries to create one history per fragment
  for(size_t h = 0; h < history.size();)
  {
    uint32_t frags = 1;
    auto it = eventFragments.find(history[h].eventId);
    if(it != eventFragments.end())
    {
      frags = it->second;
    }
    for(uint32_t f = 1; f < frags; ++f)
    {
      history.insert(h + 1, history[h]);
    }
    for(uint32_t f = 0; f < frags; ++f)
    {
      history[h + f].fragIndex = f;
    }
    h += RDCMAX(1u, frags);
  }

  for(const EventUsage &modEvent : modEvents)
  {
    auto it = eventFragments.find(modEvent.eventId);
    uint32_t numFragments = (it != eventFragments.end()) ? it->second : 0;
    if(numFragments > 1)
    {
      QueryPerFragmentValues(m_pDriver, this, resources, modEvent, x, flippedY, history,
                             eventFragments, PerFragmentQueryType::ShaderOut, usingLegacyPrimitveId,
                             numFragments);
      QueryPerFragmentValues(m_pDriver, this, resources, modEvent, x, flippedY, history,
                             eventFragments, PerFragmentQueryType::PrimitiveId,
                             usingLegacyPrimitveId, numFragments);
      QueryPerFragmentValues(m_pDriver, this, resources, modEvent, x, flippedY, history,
                             eventFragments, PerFragmentQueryType::PostMod, usingLegacyPrimitveId,
                             numFragments);
    }
  }

  // copy the postMod to next history's preMod
  for(size_t i = 1; i < history.size(); ++i)
  {
    history[i].preMod = history[i - 1].postMod;
  }

  CalculateFragmentDepthTests(m_pDriver, resources, modEvents, history, eventFragments);

  PixelHistoryDestroyResources(m_pDriver, resources);
  return history;
}
