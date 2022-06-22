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
};

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

  return true;
}

bool PixelHistoryDestroyResources(WrappedOpenGL *driver, const GLPixelHistoryResources &resources)
{
  driver->glDeleteTextures(1, &resources.colorImage);
  driver->glDeleteTextures(1, &resources.dsImage);
  driver->glDeleteFramebuffers(1, &resources.frameBuffer);

  return true;
}

rdcarray<EventUsage> QueryModifyingEvents(WrappedOpenGL *driver, GLPixelHistoryResources &resources,
                                          const rdcarray<EventUsage> &events, int x, int y)
{
  rdcarray<EventUsage> modEvents;
  rdcarray<GLuint> occlusionQueries(events.size());
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
      driver->glBeginQuery(eGL_ANY_SAMPLES_PASSED, occlusionQueries[i]);
      driver->ReplayLog(events[i].eventId, events[i].eventId, eReplay_OnlyDraw);
      driver->glEndQuery(eGL_ANY_SAMPLES_PASSED);
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
      modEvents.push_back(events[i]);
    }
    else
    {
      int result;
      driver->glGetQueryObjectiv(occlusionQueries[i], eGL_QUERY_RESULT, &result);
      if(result)
      {
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
  driver->glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
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
    PixelModification mod;
    ModificationValue modValue;
    PixelValue pixelValue;
    driver->glReadPixels(x, y, 1, 1, eGL_RGBA, eGL_FLOAT, (void *)pixelValue.floatValue.data());
    driver->glReadPixels(x, y, 1, 1, eGL_DEPTH_COMPONENT, eGL_FLOAT, (void *)&modValue.depth);
    driver->glReadPixels(x, y, 1, 1, eGL_STENCIL_INDEX, eGL_INT, (void *)&modValue.stencil);
    modValue.col = pixelValue;

    RDCEraseEl(mod);
    mod.eventId = modEvents[i].eventId;
    mod.postMod = modValue;
    history.push_back(mod);

    // restore the capture's framebuffer
    driver->glBindFramebuffer(eGL_DRAW_FRAMEBUFFER, savedDrawFramebuffer);
    driver->glBindFramebuffer(eGL_READ_FRAMEBUFFER, savedReadFramebuffer);

    if(i < modEvents.size() - 1)
    {
      driver->ReplayLog(modEvents[i].eventId + 1, modEvents[i + 1].eventId, eReplay_WithoutDraw);
    }
  }
}
};

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

  rdcarray<EventUsage> modEvents = QueryModifyingEvents(m_pDriver, resources, events, x, flippedY);

  if(modEvents.empty())
  {
    PixelHistoryDestroyResources(m_pDriver, resources);
    return history;
  }

  QueryPostModPixelValues(m_pDriver, resources, modEvents, x, flippedY, history);

  // copy the postMod to next history's preMod
  for(size_t i = 1; i < history.size(); ++i)
  {
    history[i].preMod = history[i - 1].postMod;
  }

  PixelHistoryDestroyResources(m_pDriver, resources);
  return history;
}
