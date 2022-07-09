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

GLuint GetPrimitiveIdProgram(WrappedOpenGL *driver, GLReplay *replay,
                             GLPixelHistoryResources &resources, GLuint currentProgram)
{
  auto programIterator = resources.programs.find(currentProgram);
  if(programIterator != resources.programs.end())
  {
    return programIterator->second;
  }

  GLuint newProgram;

  GLint numAttachedShaders;
  driver->glGetProgramiv(currentProgram, eGL_ATTACHED_SHADERS, &numAttachedShaders);

  newProgram = driver->glCreateProgram();

  GLuint *attachedShaders = new GLuint[numAttachedShaders];

  driver->glGetAttachedShaders(currentProgram, numAttachedShaders, &numAttachedShaders,
                               attachedShaders);

  ShaderReflection *vsRefl = NULL;
  for(int i = 0; i < numAttachedShaders; ++i)
  {
    GLint shaderType;
    driver->glGetShaderiv(attachedShaders[i], eGL_SHADER_TYPE, &shaderType);

    if(shaderType != eGL_FRAGMENT_SHADER)
    {
      driver->glAttachShader(newProgram, attachedShaders[i]);
    }

    if(shaderType == eGL_VERTEX_SHADER)
    {
      vsRefl = replay->GetShader(ResourceId(), driver->GetResourceManager()->GetResID(
                                                   ShaderRes(driver->GetCtx(), attachedShaders[i])),
                                 ShaderEntryPoint());
    }
  }
  delete[] attachedShaders;

  driver->glAttachShader(newProgram, resources.primitiveIdFragmentShader);
  if(vsRefl)
  {
    CopyProgramAttribBindings(currentProgram, newProgram, vsRefl);
  }
  driver->glLinkProgram(newProgram);

  char buffer[1024] = {};
  GLint status = 0;
  driver->glGetProgramiv(newProgram, eGL_LINK_STATUS, &status);
  if(status == 0)
  {
    GL.glGetProgramInfoLog(newProgram, 1024, NULL, buffer);
    RDCERR("Shader error: %s", buffer);
  }

  resources.programs[currentProgram] = newProgram;

  PerStageReflections dstStages;
  driver->FillReflectionArray(ProgramRes(driver->GetCtx(), newProgram), dstStages);

  PerStageReflections stages;
  driver->FillReflectionArray(ProgramRes(driver->GetCtx(), newProgram), stages);
  CopyProgramUniforms(stages, currentProgram, dstStages, newProgram);

  return newProgram;
}

bool PixelHistorySetupResources(WrappedOpenGL *driver, GLPixelHistoryResources &resources,
                                const TextureDescription &desc, const Subresource &sub,
                                uint32_t numEvents)
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

  // The pixel history primitive ID Fragment Shader requires at least version 420.
  // This is because it requires scalars to be swizzled something enabled
  // in version 420. We also require intBitsToFloat which is introduced in
  // version 330.
  int GLSLVersion = 420;

  rdcstr fs = GenerateGLSLShader(GetEmbeddedResource(glsl_pixelhistory_primid_frag),
                                 ShaderType::GLSL, GLSLVersion);
  resources.primitiveIdFragmentShader = CreateShader(eGL_FRAGMENT_SHADER, fs);

  return true;
}

bool PixelHistoryDestroyResources(WrappedOpenGL *driver, const GLPixelHistoryResources &resources)
{
  driver->glDeleteTextures(1, &resources.colorImage);
  driver->glDeleteTextures(1, &resources.dsImage);
  driver->glDeleteFramebuffers(1, &resources.frameBuffer);
  driver->glDeleteShader(resources.primitiveIdFragmentShader);

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
  for(size_t i = 0; i < events.size(); ++i)
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
  for(size_t i = 0; i < events.size(); ++i)
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

std::map<uint32_t, uint32_t> QueryNumFragmentsByEvent(WrappedOpenGL *driver,
                                                      GLPixelHistoryResources &resources,
                                                      const rdcarray<EventUsage> &modEvents, int x,
                                                      int y)
{
  driver->glBindFramebuffer(eGL_FRAMEBUFFER, resources.frameBuffer);
  driver->glClear(eGL_COLOR_BUFFER_BIT | eGL_DEPTH_BUFFER_BIT | eGL_STENCIL_BUFFER_BIT);
  driver->ReplayLog(0, modEvents[0].eventId, eReplay_WithoutDraw);

  std::map<uint32_t, uint32_t> eventFragments;

  for(size_t i = 0; i < modEvents.size(); ++i)
  {
    if(!isDirectWrite(modEvents[i].usage) && modEvents[i].usage != ResourceUsage::Clear)
    {
      GLint savedReadFramebuffer, savedDrawFramebuffer;
      driver->glGetIntegerv(eGL_DRAW_FRAMEBUFFER_BINDING, &savedDrawFramebuffer);
      driver->glGetIntegerv(eGL_READ_FRAMEBUFFER_BINDING, &savedReadFramebuffer);
      // bind our own framebuffer to save the pixel values
      driver->glBindFramebuffer(eGL_FRAMEBUFFER, resources.frameBuffer);
      driver->glStencilOp(eGL_INCR, eGL_INCR, eGL_INCR);
      driver->glStencilMask(0xff);
      driver->glClearStencil(0);
      driver->glClear(eGL_STENCIL_BUFFER_BIT);
      driver->glEnable(eGL_STENCIL_TEST);
      driver->ReplayLog(modEvents[i].eventId, modEvents[i].eventId, eReplay_Full);

      // read and get number of fragments
      uint32_t numFragments;
      driver->glReadPixels(x, y, 1, 1, eGL_STENCIL_INDEX, eGL_INT, (void *)&numFragments);

      eventFragments.emplace(modEvents[i].eventId, numFragments);

      // restore the capture's framebuffer
      driver->glBindFramebuffer(eGL_DRAW_FRAMEBUFFER, savedDrawFramebuffer);
      driver->glBindFramebuffer(eGL_READ_FRAMEBUFFER, savedReadFramebuffer);
    }

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
                            PerFragmentQueryType queryType)
{
  driver->ReplayLog(0, modEvent.eventId - 1, eReplay_Full);
  driver->glBindFramebuffer(eGL_FRAMEBUFFER, resources.frameBuffer);
  driver->glReadBuffer(eGL_COLOR_ATTACHMENT0);

  auto it = eventFragments.find(modEvent.eventId);
  uint32_t numFragments = (it != eventFragments.end()) ? it->second : 0;

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

    GLint currentProgram;

    driver->glGetIntegerv(eGL_CURRENT_PROGRAM, &currentProgram);

    driver->glUseProgram(GetPrimitiveIdProgram(driver, replay, resources, currentProgram));
  }

  for(size_t j = 0; j < std::max(numFragments, 1u); ++j)
  {
    ModificationValue modValue;
    PixelValue pixelValue;
    modValue.stencil = 0;
    driver->glStencilFunc(eGL_EQUAL, (int)j, 0xff);
    driver->glClear(eGL_STENCIL_BUFFER_BIT);

    driver->ReplayLog(modEvent.eventId, modEvent.eventId, eReplay_Full);

    if(queryType == PerFragmentQueryType::PrimitiveId)
    {
      int primitiveIds[4];
      driver->glReadPixels(x, y, 1, 1, eGL_RGBA, eGL_FLOAT, (void *)primitiveIds);

      RDCASSERT(primitiveIds[0] == primitiveIds[1] && primitiveIds[0] == primitiveIds[2] &&
                primitiveIds[0] == primitiveIds[3]);
      historyIndex->primitiveID = primitiveIds[0];
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
}

void WriteClearValues(WrappedOpenGL *driver, GLPixelHistoryResources &resources,
                      const EventUsage clearEvent, int x, int y, rdcarray<PixelModification> &history)
{
  driver->glBindFramebuffer(eGL_FRAMEBUFFER, resources.frameBuffer);
  driver->glClear(eGL_COLOR_BUFFER_BIT | eGL_DEPTH_BUFFER_BIT | eGL_STENCIL_BUFFER_BIT);
  driver->ReplayLog(0, clearEvent.eventId - 1, eReplay_WithoutDraw);

  GLint savedReadFramebuffer, savedDrawFramebuffer;
  driver->glGetIntegerv(eGL_DRAW_FRAMEBUFFER_BINDING, &savedDrawFramebuffer);
  driver->glGetIntegerv(eGL_READ_FRAMEBUFFER_BINDING, &savedReadFramebuffer);
  // bind our own framebuffer to save the pixel values
  driver->glBindFramebuffer(eGL_FRAMEBUFFER, resources.frameBuffer);
  driver->ReplayLog(clearEvent.eventId, clearEvent.eventId, eReplay_Full);

  // read the post mod pixel value into the history event
  ModificationValue modValue;
  PixelValue pixelValue;
  driver->glReadPixels(x, y, 1, 1, eGL_RGBA, eGL_FLOAT, (void *)pixelValue.floatValue.data());
  driver->glReadPixels(x, y, 1, 1, eGL_DEPTH_COMPONENT, eGL_FLOAT, (void *)&modValue.depth);
  driver->glReadPixels(x, y, 1, 1, eGL_STENCIL_INDEX, eGL_INT, (void *)&modValue.stencil);
  modValue.col = pixelValue;

  PixelModification referenceHistory;
  referenceHistory.eventId = clearEvent.eventId;
  auto historyIndex =
      std::lower_bound(history.begin(), history.end(), referenceHistory,
                       [](const PixelModification &h1, const PixelModification &h2) -> bool {
                         return h1.eventId < h2.eventId;
                       });

  historyIndex->postMod = modValue;

  // restore the capture's framebuffer
  driver->glBindFramebuffer(eGL_DRAW_FRAMEBUFFER, savedDrawFramebuffer);
  driver->glBindFramebuffer(eGL_READ_FRAMEBUFFER, savedReadFramebuffer);
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

    default: RDCASSERT(0);
  }
  return false;
}

void CalculateFragmentDepthTests(WrappedOpenGL *driver, GLPixelHistoryResources &resources,
                                 const rdcarray<EventUsage> &modEvents,
                                 rdcarray<PixelModification> &history)
{
  driver->ReplayLog(0, modEvents[0].eventId, eReplay_WithoutDraw);
  size_t historyIndex = 0;
  for(size_t i = 0; i < modEvents.size(); ++i)
  {
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

  PixelHistorySetupResources(m_pDriver, resources, textureDesc, sub, (uint32_t)events.size());

  rdcarray<EventUsage> modEvents =
      QueryModifyingEvents(m_pDriver, resources, events, x, flippedY, history);

  if(modEvents.empty())
  {
    PixelHistoryDestroyResources(m_pDriver, resources);
    return history;
  }

  QueryFailedTests(m_pDriver, resources, modEvents, x, flippedY, history);
  std::map<uint32_t, uint32_t> eventFragments =
      QueryNumFragmentsByEvent(m_pDriver, resources, modEvents, x, flippedY);

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
    if(modEvent.usage == ResourceUsage::Clear)
    {
      WriteClearValues(m_pDriver, resources, modEvent, x, flippedY, history);
    }
    else
    {
      QueryPerFragmentValues(m_pDriver, this, resources, modEvent, x, flippedY, history,
                             eventFragments, PerFragmentQueryType::ShaderOut);
      QueryPerFragmentValues(m_pDriver, this, resources, modEvent, x, flippedY, history,
                             eventFragments, PerFragmentQueryType::PrimitiveId);
      QueryPerFragmentValues(m_pDriver, this, resources, modEvent, x, flippedY, history,
                             eventFragments, PerFragmentQueryType::PostMod);
    }
  }

  // copy the postMod to next history's preMod
  for(size_t i = 1; i < history.size(); ++i)
  {
    history[i].preMod = history[i - 1].postMod;
  }

  CalculateFragmentDepthTests(m_pDriver, resources, modEvents, history);

  PixelHistoryDestroyResources(m_pDriver, resources);
  return history;
}
