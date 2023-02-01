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
  GLuint dsImage = 0;
  GLuint frameBuffer;
  GLuint fullPrecisionColorImage;
  GLuint fullPrecisionDsImage;
  GLuint fullPrecisionFrameBuffer;
  GLuint primitiveIdFragmentShader;
  GLuint primitiveIdFragmentShaderSPIRV;
  std::unordered_map<GLuint, GLuint> programs;
  bool depthTextureAttachedToFrameBuffer;
  bool stencilTextureAttachedToFrameBuffer;
  GLuint depthImage = 0;
  GLuint stencilImage = 0;
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

  GLRenderState rs;
  rs.FetchState(driver);

  GLuint primitiveIdProgram = driver->glCreateProgram();
  replay->CreateFragmentShaderReplacementProgram(
      rs.Program.name, primitiveIdProgram, rs.Pipeline.name, resources.primitiveIdFragmentShader,
      resources.primitiveIdFragmentShaderSPIRV);

  return primitiveIdProgram;
}

bool PixelHistorySetupResources(WrappedOpenGL *driver, GLPixelHistoryResources &resources,
                                const TextureDescription &desc, const Subresource &sub,
                                uint32_t numEvents, GLuint glslVersion)
{
  GLuint curDepth;
  GLint depthType;
  GLuint curStencil;
  GLint stencilType;

  driver->glGetFramebufferAttachmentParameteriv(eGL_DRAW_FRAMEBUFFER, eGL_DEPTH_ATTACHMENT,
                                                eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME,
                                                (GLint *)&curDepth);

  driver->glGetFramebufferAttachmentParameteriv(eGL_DRAW_FRAMEBUFFER, eGL_DEPTH_ATTACHMENT,
                                                eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &depthType);

  driver->glGetFramebufferAttachmentParameteriv(eGL_DRAW_FRAMEBUFFER, eGL_STENCIL_ATTACHMENT,
                                                eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME,
                                                (GLint *)&curStencil);

  driver->glGetFramebufferAttachmentParameteriv(eGL_DRAW_FRAMEBUFFER, eGL_STENCIL_ATTACHMENT,
                                                eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &stencilType);

  GLenum depthFormat = eGL_DEPTH_COMPONENT16;

  resources.depthTextureAttachedToFrameBuffer = false;
  resources.stencilTextureAttachedToFrameBuffer = false;

  if(curDepth != 0)
  {
    resources.depthTextureAttachedToFrameBuffer = true;
    ResourceId id;
    if(depthType != eGL_RENDERBUFFER)
    {
      id = driver->GetResourceManager()->GetResID(TextureRes(driver->GetCtx(), curDepth));
    }
    else
    {
      id = driver->GetResourceManager()->GetResID(RenderbufferRes(driver->GetCtx(), curDepth));
    }
    depthFormat = driver->m_Textures[id].internalFormat;
  }

  GLenum stencilFormat = eGL_STENCIL_INDEX8;
  if(curStencil != 0)
  {
    resources.stencilTextureAttachedToFrameBuffer = true;
    ResourceId id;
    if(stencilType != eGL_RENDERBUFFER)
    {
      id = driver->GetResourceManager()->GetResID(TextureRes(driver->GetCtx(), curStencil));
    }
    else
    {
      id = driver->GetResourceManager()->GetResID(RenderbufferRes(driver->GetCtx(), curStencil));
    }
    stencilFormat = driver->m_Textures[id].internalFormat;
  }

  if(curStencil == curDepth)
  {
    driver->CreateTextureImage(resources.dsImage, depthFormat, eGL_NONE, eGL_NONE, eGL_TEXTURE_2D,
                               2, numEvents, 1, 1, 1, 1);
    driver->glFramebufferTexture(eGL_FRAMEBUFFER, eGL_DEPTH_STENCIL_ATTACHMENT, resources.dsImage, 0);
  }
  else
  {
    if(curDepth != 0)
    {
      driver->CreateTextureImage(resources.depthImage, depthFormat, eGL_NONE, eGL_NONE,
                                 eGL_TEXTURE_2D, 2, numEvents, 1, 1, 1, 1);
      driver->glFramebufferTexture(eGL_FRAMEBUFFER, eGL_DEPTH_ATTACHMENT, resources.depthImage, 0);
    }
    if(curStencil != 0)
    {
      driver->CreateTextureImage(resources.stencilImage, stencilFormat, eGL_NONE, eGL_NONE,
                                 eGL_TEXTURE_2D, 2, numEvents, 1, 1, 1, 1);
      driver->glFramebufferTexture(eGL_FRAMEBUFFER, eGL_STENCIL_ATTACHMENT, resources.stencilImage,
                                   0);
    }
  }

  // Allocate a framebuffer that will render to the textures
  driver->glGenFramebuffers(1, &resources.frameBuffer);
  driver->glBindFramebuffer(eGL_FRAMEBUFFER, resources.frameBuffer);

  // Allocate a texture for the pixel history colour values
  driver->glGenTextures(1, &resources.colorImage);
  driver->glBindTexture(eGL_TEXTURE_2D, resources.colorImage);
  driver->CreateTextureImage(resources.colorImage, eGL_RGBA32F, eGL_NONE, eGL_NONE, eGL_TEXTURE_2D,
                             2, numEvents, 1, 1, 1, 1);
  driver->glFramebufferTexture(eGL_FRAMEBUFFER, eGL_COLOR_ATTACHMENT0, resources.colorImage, 0);

  // Allocate a texture for the pixel history depth/stencil values
  driver->glGenTextures(1, &resources.dsImage);
  driver->glBindTexture(eGL_TEXTURE_2D, resources.dsImage);

  GLResource depthResource;

  driver->CreateTextureImage(resources.dsImage, eGL_DEPTH24_STENCIL8, eGL_NONE, eGL_NONE,
                             eGL_TEXTURE_2D, 2, numEvents, 1, 1, 1, 1);
  driver->glFramebufferTexture(eGL_FRAMEBUFFER, eGL_DEPTH_STENCIL_ATTACHMENT, resources.dsImage, 0);

  // Allocate a framebuffer that will render to the textures
  driver->glGenFramebuffers(1, &resources.fullPrecisionFrameBuffer);
  driver->glBindFramebuffer(eGL_FRAMEBUFFER, resources.fullPrecisionFrameBuffer);

  // Allocate a texture for the pixel history colour values
  driver->glGenTextures(1, &resources.fullPrecisionColorImage);
  driver->glBindTexture(eGL_TEXTURE_2D, resources.fullPrecisionColorImage);
  driver->CreateTextureImage(resources.fullPrecisionColorImage, eGL_RGBA32F, eGL_NONE, eGL_NONE,
                             eGL_TEXTURE_2D, 2, desc.width >> sub.mip, desc.height >> sub.mip, 1, 1,
                             1);
  driver->glFramebufferTexture(eGL_FRAMEBUFFER, eGL_COLOR_ATTACHMENT0,
                               resources.fullPrecisionColorImage, 0);

  // Allocate a texture for the pixel history depth/stencil values
  driver->glGenTextures(1, &resources.fullPrecisionDsImage);
  driver->glBindTexture(eGL_TEXTURE_2D, resources.fullPrecisionDsImage);
  driver->CreateTextureImage(resources.fullPrecisionDsImage, eGL_DEPTH24_STENCIL8, eGL_NONE,
                             eGL_NONE, eGL_TEXTURE_2D, 2, desc.width >> sub.mip,
                             desc.height >> sub.mip, 1, 1, 1);
  driver->glFramebufferTexture(eGL_FRAMEBUFFER, eGL_DEPTH_STENCIL_ATTACHMENT,
                               resources.fullPrecisionDsImage, 0);

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
    glslSource =
        GenerateGLSLShader(GetEmbeddedResource(glsl_pixelhistory_primid_frag), ShaderType::GLSL,
                           glslVersion, "#define INT_BITS_TO_FLOAT_NOT_SUPPORTED\n");
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
  driver->glDeleteTextures(1, &resources.fullPrecisionColorImage);
  driver->glDeleteTextures(1, &resources.fullPrecisionDsImage);
  driver->glDeleteFramebuffers(1, &resources.fullPrecisionFrameBuffer);
  driver->glDeleteShader(resources.primitiveIdFragmentShader);
  driver->glDeleteShader(resources.primitiveIdFragmentShaderSPIRV);
  driver->glDeleteTextures(1, &resources.depthImage);
  driver->glDeleteTextures(1, &resources.stencilImage);

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
  driver->ReplayLog(0, modEvents[0].eventId, eReplay_WithoutDraw);

  for(size_t i = 0; i < modEvents.size(); i++)
  {
    driver->ReplayLog(modEvents[i].eventId, modEvents[i].eventId, eReplay_OnlyDraw);

    // Blit the values into our framebuffer
    GLint savedReadFramebuffer, savedDrawFramebuffer;
    driver->glGetIntegerv(eGL_DRAW_FRAMEBUFFER_BINDING, &savedDrawFramebuffer);
    driver->glGetIntegerv(eGL_READ_FRAMEBUFFER_BINDING, &savedReadFramebuffer);
    driver->glBindFramebuffer(eGL_DRAW_FRAMEBUFFER, resources.frameBuffer);
    driver->glBindFramebuffer(eGL_READ_FRAMEBUFFER, savedDrawFramebuffer);

    SafeBlitFramebuffer(
        x, y, x + 1, y + 1, GLint(i), 0, GLint(i) + 1, 1,
        eGL_COLOR_BUFFER_BIT |
            (resources.depthTextureAttachedToFrameBuffer ? eGL_DEPTH_BUFFER_BIT : eGL_NONE) |
            (resources.stencilTextureAttachedToFrameBuffer ? eGL_STENCIL_BUFFER_BIT : eGL_NONE),
        eGL_NEAREST);

    // restore the capture's framebuffer
    driver->glBindFramebuffer(eGL_DRAW_FRAMEBUFFER, savedDrawFramebuffer);
    driver->glBindFramebuffer(eGL_READ_FRAMEBUFFER, savedReadFramebuffer);

    if(i < modEvents.size() - 1)
    {
      driver->ReplayLog(modEvents[i].eventId + 1, modEvents[i + 1].eventId, eReplay_WithoutDraw);
    }
  }

  driver->glBindFramebuffer(eGL_READ_FRAMEBUFFER, resources.frameBuffer);
  rdcarray<float> colourValues;
  colourValues.resize(4 * modEvents.size());
  rdcarray<float> depthValues;
  depthValues.resize(modEvents.size());
  rdcarray<int> stencilValues;
  stencilValues.resize(modEvents.size());
  driver->glReadPixels(0, 0, GLint(modEvents.size()), 1, eGL_RGBA, eGL_FLOAT,
                       (void *)colourValues.data());
  if(resources.depthTextureAttachedToFrameBuffer)
  {
    driver->glReadPixels(0, 0, GLint(modEvents.size()), 1, eGL_DEPTH_COMPONENT, eGL_FLOAT,
                         (void *)depthValues.data());
  }

  if(resources.stencilTextureAttachedToFrameBuffer)
  {
    driver->glReadPixels(0, 0, GLint(modEvents.size()), 1, eGL_STENCIL_INDEX, eGL_INT,
                         (void *)stencilValues.data());
  }

  for(size_t i = 0; i < modEvents.size(); i++)
  {
    ModificationValue modValue;

    for(int j = 0; j < 4; ++j)
    {
      modValue.col.floatValue[j] = colourValues[i * 4 + j];
    }
    modValue.depth = depthValues[i];
    modValue.stencil = stencilValues[i];

    history[i].postMod = modValue;
  }
}

// This function a) calculates the number of fagments per event
//           and b) calculates the shader output values per event
std::map<uint32_t, uint32_t> QueryNumFragmentsByEvent(WrappedOpenGL *driver,
                                                      GLPixelHistoryResources &resources,
                                                      const rdcarray<EventUsage> &modEvents,
                                                      rdcarray<PixelModification> &history, int x,
                                                      int y)
{
  driver->ReplayLog(0, modEvents[0].eventId, eReplay_WithoutDraw);

  std::map<uint32_t, uint32_t> eventFragments;

  for(size_t i = 0; i < modEvents.size(); ++i)
  {
    GLint savedReadFramebuffer, savedDrawFramebuffer;
    driver->glGetIntegerv(eGL_DRAW_FRAMEBUFFER_BINDING, &savedDrawFramebuffer);
    driver->glGetIntegerv(eGL_READ_FRAMEBUFFER_BINDING, &savedReadFramebuffer);
    driver->glBindFramebuffer(eGL_DRAW_FRAMEBUFFER, resources.fullPrecisionFrameBuffer);
    driver->glBindFramebuffer(eGL_READ_FRAMEBUFFER, resources.fullPrecisionFrameBuffer);

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

    // replay event
    driver->ReplayLog(modEvents[i].eventId, modEvents[i].eventId, eReplay_OnlyDraw);

    ModificationValue modValue;
    driver->glReadPixels(x, y, 1, 1, eGL_RGBA, eGL_FLOAT, (void *)modValue.col.floatValue.data());
    driver->glReadPixels(x, y, 1, 1, eGL_DEPTH_COMPONENT, eGL_FLOAT, (void *)&modValue.depth);
    uint32_t numFragments = 0;
    driver->glReadPixels(x, y, 1, 1, eGL_STENCIL_INDEX, eGL_UNSIGNED_INT, (void *)&numFragments);
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

void QueryShaderOutPerFragment(WrappedOpenGL *driver, GLReplay *replay,
                               GLPixelHistoryResources &resources,
                               const rdcarray<EventUsage> &modEvents, int x, int y,
                               rdcarray<PixelModification> &history,
                               const std::map<uint32_t, uint32_t> &eventFragments)
{
  driver->ReplayLog(0, modEvents[0].eventId, eReplay_WithoutDraw);
  for(size_t i = 0; i < modEvents.size(); ++i)
  {
    auto it = eventFragments.find(modEvents[i].eventId);
    uint32_t numFragments = (it != eventFragments.end()) ? it->second : 0;

    if(numFragments <= 1)
    {
      if(i < modEvents.size() - 1)
      {
        driver->ReplayLog(modEvents[i].eventId, modEvents[i + 1].eventId, eReplay_WithoutDraw);
      }
      continue;
    }

    driver->glEnable(eGL_SCISSOR_TEST);
    driver->glScissor(x, y, 1, 1);

    driver->glClearStencil(0);

    driver->glEnable(eGL_DEPTH_TEST);
    driver->glDepthFunc(eGL_ALWAYS);
    driver->glDepthMask(GL_TRUE);
    driver->glDisable(eGL_BLEND);

    driver->glStencilMask(0xff);
    driver->glStencilOp(eGL_INCR, eGL_INCR, eGL_INCR);
    driver->glEnable(eGL_STENCIL_TEST);

    PixelModification referenceHistory;
    referenceHistory.eventId = modEvents[i].eventId;

    auto historyIndex =
        std::lower_bound(history.begin(), history.end(), referenceHistory,
                         [](const PixelModification &h1, const PixelModification &h2) -> bool {
                           return h1.eventId < h2.eventId;
                         });

    RDCASSERT(historyIndex != history.end());

    GLint savedReadFramebuffer, savedDrawFramebuffer;
    driver->glGetIntegerv(eGL_DRAW_FRAMEBUFFER_BINDING, &savedDrawFramebuffer);
    driver->glGetIntegerv(eGL_READ_FRAMEBUFFER_BINDING, &savedReadFramebuffer);
    driver->glBindFramebuffer(eGL_DRAW_FRAMEBUFFER, resources.fullPrecisionFrameBuffer);
    driver->glBindFramebuffer(eGL_READ_FRAMEBUFFER, resources.fullPrecisionFrameBuffer);

    for(size_t j = 0; j < RDCMAX(numFragments, 1u); ++j)
    {
      //  Set the stencil function so only jth fragment will pass.
      driver->glStencilFunc(eGL_EQUAL, (int)j, 0xff);
      driver->glClear(eGL_STENCIL_BUFFER_BIT);

      driver->ReplayLog(modEvents[i].eventId, modEvents[i].eventId, eReplay_OnlyDraw);

      ModificationValue modValue;
      driver->glReadPixels(x, y, 1, 1, eGL_RGBA, eGL_FLOAT, (void *)modValue.col.floatValue.data());
      driver->glReadPixels(x, y, 1, 1, eGL_DEPTH_COMPONENT, eGL_FLOAT, (void *)&modValue.depth);
      modValue.stencil = historyIndex->shaderOut.stencil;

      historyIndex->shaderOut = modValue;
      historyIndex++;
    }

    // restore the capture's framebuffer
    driver->glBindFramebuffer(eGL_DRAW_FRAMEBUFFER, savedDrawFramebuffer);
    driver->glBindFramebuffer(eGL_READ_FRAMEBUFFER, savedReadFramebuffer);

    if(i < modEvents.size() - 1)
    {
      driver->ReplayLog(modEvents[i].eventId + 1, modEvents[i + 1].eventId, eReplay_WithoutDraw);
    }
  }
}

void QueryPostModPerFragment(WrappedOpenGL *driver, GLReplay *replay,
                             GLPixelHistoryResources &resources,
                             const rdcarray<EventUsage> &modEvents, int x, int y,
                             rdcarray<PixelModification> &history,
                             const std::map<uint32_t, uint32_t> &eventFragments)
{
  driver->ReplayLog(0, modEvents[0].eventId - 1, eReplay_WithoutDraw);

  for(size_t i = 0; i < modEvents.size(); i++)
  {
    auto it = eventFragments.find(modEvents[i].eventId);
    uint32_t numFragments = (it != eventFragments.end()) ? it->second : 0;

    if(numFragments <= 1)
    {
      if(i < modEvents.size() - 1)
      {
        driver->ReplayLog(modEvents[i].eventId, modEvents[i + 1].eventId, eReplay_WithoutDraw);
      }
      continue;
    }

    driver->glEnable(eGL_SCISSOR_TEST);
    driver->glScissor(x, y, 1, 1);

    driver->glClearStencil(0);

    driver->glStencilMask(0xff);
    driver->glStencilOp(eGL_INCR, eGL_INCR, eGL_INCR);
    driver->glEnable(eGL_STENCIL_TEST);

    PixelModification referenceHistory;
    referenceHistory.eventId = modEvents[i].eventId;

    auto historyIndex =
        std::lower_bound(history.begin(), history.end(), referenceHistory,
                         [](const PixelModification &h1, const PixelModification &h2) -> bool {
                           return h1.eventId < h2.eventId;
                         });
    RDCASSERT(historyIndex != history.end());

    GLint savedReadFramebuffer, savedDrawFramebuffer;
    driver->glGetIntegerv(eGL_DRAW_FRAMEBUFFER_BINDING, &savedDrawFramebuffer);
    driver->glGetIntegerv(eGL_READ_FRAMEBUFFER_BINDING, &savedReadFramebuffer);

    for(size_t j = 0; j < std::max(numFragments, 1u); ++j)
    {
      // Set the stencil function so only jth fragment will pass.
      driver->glStencilFunc(eGL_EQUAL, (int)j, 0xff);
      driver->glClear(eGL_STENCIL_BUFFER_BIT);

      driver->ReplayLog(modEvents[i].eventId, modEvents[i].eventId, eReplay_OnlyDraw);

      // Blit the values into out framebuffer
      driver->glBindFramebuffer(eGL_DRAW_FRAMEBUFFER, resources.frameBuffer);
      driver->glBindFramebuffer(eGL_READ_FRAMEBUFFER, savedDrawFramebuffer);

      SafeBlitFramebuffer(
          x, y, x + 1, y + 1, GLint(j), 0, GLint(j) + 1, 1,
          eGL_COLOR_BUFFER_BIT |
              (resources.depthTextureAttachedToFrameBuffer ? eGL_DEPTH_BUFFER_BIT : eGL_NONE) |
              (resources.stencilTextureAttachedToFrameBuffer ? eGL_STENCIL_BUFFER_BIT : eGL_NONE),
          eGL_NEAREST);

      // restore the capture's framebuffer
      driver->glBindFramebuffer(eGL_DRAW_FRAMEBUFFER, savedDrawFramebuffer);
      driver->glBindFramebuffer(eGL_READ_FRAMEBUFFER, savedReadFramebuffer);
    }
    driver->glBindFramebuffer(eGL_READ_FRAMEBUFFER, resources.frameBuffer);
    rdcarray<float> colourValues;
    colourValues.resize(4 * numFragments);
    rdcarray<float> depthValues;
    depthValues.resize(numFragments);
    driver->glReadPixels(0, 0, numFragments, 1, eGL_RGBA, eGL_FLOAT, (void *)colourValues.data());
    driver->glReadPixels(0, 0, numFragments, 1, eGL_DEPTH_COMPONENT, eGL_FLOAT,
                         (void *)depthValues.data());
    for(size_t j = 0; j < numFragments; j++)
    {
      ModificationValue modValue;

      for(int k = 0; k < 4; ++k)
      {
        modValue.col.floatValue[k] = colourValues[j * 4 + k];
      }
      modValue.depth = depthValues[j];
      modValue.stencil = historyIndex->postMod.stencil;

      historyIndex->postMod = modValue;
      historyIndex++;
    }

    driver->glBindFramebuffer(eGL_READ_FRAMEBUFFER, savedReadFramebuffer);
    if(i < modEvents.size() - 1)
    {
      driver->ReplayLog(modEvents[i].eventId + 1, modEvents[i + 1].eventId, eReplay_WithoutDraw);
    }
  }
}

void QueryPrimitiveIdPerFragment(WrappedOpenGL *driver, GLReplay *replay,
                                 GLPixelHistoryResources &resources,
                                 const rdcarray<EventUsage> &modEvents, int x, int y,
                                 rdcarray<PixelModification> &history,
                                 const std::map<uint32_t, uint32_t> &eventFragments,
                                 bool usingFloatForPrimitiveId)
{
  driver->ReplayLog(0, modEvents[0].eventId - 1, eReplay_WithoutDraw);

  for(size_t i = 0; i < modEvents.size(); i++)
  {
    auto it = eventFragments.find(modEvents[i].eventId);
    uint32_t numFragments = (it != eventFragments.end()) ? it->second : 0;

    if(numFragments == 0)
    {
      if(i < modEvents.size() - 1)
      {
        driver->ReplayLog(modEvents[i].eventId, modEvents[i + 1].eventId, eReplay_WithoutDraw);
      }
      continue;
    }

    driver->glBindFramebuffer(eGL_FRAMEBUFFER, resources.fullPrecisionFrameBuffer);
    driver->glReadBuffer(eGL_COLOR_ATTACHMENT0);

    driver->glEnable(eGL_SCISSOR_TEST);
    driver->glScissor(x, y, 1, 1);

    driver->glClearStencil(0);
    driver->glEnable(eGL_DEPTH_TEST);
    driver->glDepthFunc(eGL_ALWAYS);
    driver->glDepthMask(GL_TRUE);
    driver->glDisable(eGL_BLEND);

    driver->glStencilMask(0xff);
    driver->glStencilOp(eGL_INCR, eGL_INCR, eGL_INCR);
    driver->glEnable(eGL_STENCIL_TEST);

    PixelModification referenceHistory;
    referenceHistory.eventId = modEvents[i].eventId;

    auto historyIndex =
        std::lower_bound(history.begin(), history.end(), referenceHistory,
                         [](const PixelModification &h1, const PixelModification &h2) -> bool {
                           return h1.eventId < h2.eventId;
                         });
    RDCASSERT(historyIndex != history.end());

    GLint currentProgram = 0;

    // we expect this value to be overwritten by the primitive id shader. if not
    // it will cause an assertion that all color values are the same to fail
    driver->glClearColor(0.84f, 0.17f, 0.2f, 0.49f);
    driver->glClear(eGL_COLOR_BUFFER_BIT);

    driver->glGetIntegerv(eGL_CURRENT_PROGRAM, &currentProgram);

    driver->glUseProgram(GetPrimitiveIdProgram(driver, replay, resources, currentProgram));

    for(size_t j = 0; j < std::max(numFragments, 1u); ++j)
    {
      ModificationValue modValue;
      modValue.stencil = 0;
      // Set the stencil function so only jth fragment will pass.
      driver->glStencilFunc(eGL_EQUAL, (int)j, 0xff);
      driver->glClear(eGL_STENCIL_BUFFER_BIT);

      driver->ReplayLog(modEvents[i].eventId, modEvents[i].eventId, eReplay_OnlyDraw);

      if(usingFloatForPrimitiveId)
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

      ++historyIndex;
    }
    driver->glUseProgram(currentProgram);

    if(i < modEvents.size() - 1)
    {
      driver->ReplayLog(modEvents[i].eventId + 1, modEvents[i + 1].eventId, eReplay_WithoutDraw);
    }
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
      // keep the historyIndex in sync with the event index
      while(historyIndex < history.size() && modEvents[i].eventId == history[historyIndex].eventId)
      {
        ++historyIndex;
      }
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

  // When RenderDoc passed y, the value being passed in is with the Y axis starting from the top
  // However, we need to have it starting from the bottom, so flip it by subtracting y from the
  // height.
  uint32_t flippedY = (textureDesc.height >> sub.mip) - y - 1;

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
  bool usingFloatForPrimitiveId = glslVersion < 330;

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

  QueryShaderOutPerFragment(m_pDriver, this, resources, modEvents, x, flippedY, history,
                            eventFragments);
  QueryPrimitiveIdPerFragment(m_pDriver, this, resources, modEvents, x, flippedY, history,
                              eventFragments, usingFloatForPrimitiveId);
  QueryPostModPerFragment(m_pDriver, this, resources, modEvents, x, flippedY, history,
                          eventFragments);

  // copy the postMod to next history's preMod
  for(size_t i = 1; i < history.size(); ++i)
  {
    history[i].preMod = history[i - 1].postMod;
  }

  CalculateFragmentDepthTests(m_pDriver, resources, modEvents, history, eventFragments);

  PixelHistoryDestroyResources(m_pDriver, resources);
  return history;
}
