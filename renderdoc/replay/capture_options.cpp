/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2019 Baldur Karlsson
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
#include "api/app/renderdoc_app.h"
#include "api/replay/renderdoc_replay.h"
#include "common/common.h"
#include "core/core.h"

int RENDERDOC_CC SetCaptureOptionU32(RENDERDOC_CaptureOption opt, uint32_t val)
{
  CaptureOptions opts = RenderDoc::Inst().GetCaptureOptions();

  switch(opt)
  {
    case eRENDERDOC_Option_AllowVSync: opts.allowVSync = (val != 0); break;
    case eRENDERDOC_Option_AllowFullscreen: opts.allowFullscreen = (val != 0); break;
    case eRENDERDOC_Option_APIValidation: opts.apiValidation = (val != 0); break;
    case eRENDERDOC_Option_CaptureCallstacks: opts.captureCallstacks = (val != 0); break;
    case eRENDERDOC_Option_CaptureCallstacksOnlyDraws:
      opts.captureCallstacksOnlyDraws = (val != 0);
      break;
    case eRENDERDOC_Option_DelayForDebugger: opts.delayForDebugger = val; break;
    case eRENDERDOC_Option_VerifyBufferAccess: opts.verifyBufferAccess = (val != 0); break;
    case eRENDERDOC_Option_HookIntoChildren: opts.hookIntoChildren = (val != 0); break;
    case eRENDERDOC_Option_RefAllResources: opts.refAllResources = (val != 0); break;
    case eRENDERDOC_Option_SaveAllInitials:
      // option is deprecated
      break;
    case eRENDERDOC_Option_CaptureAllCmdLists: opts.captureAllCmdLists = (val != 0); break;
    case eRENDERDOC_Option_DebugOutputMute: opts.debugOutputMute = (val != 0); break;
    case eRENDERDOC_Option_AllowUnsupportedVendorExtensions:
      if(val == 0x10DE)
        RenderDoc::Inst().EnableVendorExtensions(VendorExtensions::NvAPI);
      else
        RDCWARN("AllowUnsupportedVendorExtensions unexpected parameter %x", val);
      break;
    default: RDCLOG("Unrecognised capture option '%d'", opt); return 0;
  }

  RenderDoc::Inst().SetCaptureOptions(opts);
  return 1;
}

int RENDERDOC_CC SetCaptureOptionF32(RENDERDOC_CaptureOption opt, float val)
{
  CaptureOptions opts = RenderDoc::Inst().GetCaptureOptions();

  switch(opt)
  {
    case eRENDERDOC_Option_AllowVSync: opts.allowVSync = (val != 0.0f); break;
    case eRENDERDOC_Option_AllowFullscreen: opts.allowFullscreen = (val != 0.0f); break;
    case eRENDERDOC_Option_APIValidation: opts.apiValidation = (val != 0.0f); break;
    case eRENDERDOC_Option_CaptureCallstacks: opts.captureCallstacks = (val != 0.0f); break;
    case eRENDERDOC_Option_CaptureCallstacksOnlyDraws:
      opts.captureCallstacksOnlyDraws = (val != 0.0f);
      break;
    case eRENDERDOC_Option_DelayForDebugger: opts.delayForDebugger = (uint32_t)val; break;
    case eRENDERDOC_Option_VerifyBufferAccess: opts.verifyBufferAccess = (val != 0.0f); break;
    case eRENDERDOC_Option_HookIntoChildren: opts.hookIntoChildren = (val != 0.0f); break;
    case eRENDERDOC_Option_RefAllResources: opts.refAllResources = (val != 0.0f); break;
    case eRENDERDOC_Option_SaveAllInitials:
      // option is deprecated
      break;
    case eRENDERDOC_Option_CaptureAllCmdLists: opts.captureAllCmdLists = (val != 0.0f); break;
    case eRENDERDOC_Option_DebugOutputMute: opts.debugOutputMute = (val != 0.0f); break;
    case eRENDERDOC_Option_AllowUnsupportedVendorExtensions:
      RDCWARN("AllowUnsupportedVendorExtensions unexpected parameter %f", val);
      break;
    default: RDCLOG("Unrecognised capture option '%d'", opt); return 0;
  }

  RenderDoc::Inst().SetCaptureOptions(opts);
  return 1;
}

uint32_t RENDERDOC_CC GetCaptureOptionU32(RENDERDOC_CaptureOption opt)
{
  switch(opt)
  {
    case eRENDERDOC_Option_AllowVSync:
      return (RenderDoc::Inst().GetCaptureOptions().allowVSync ? 1 : 0);
    case eRENDERDOC_Option_AllowFullscreen:
      return (RenderDoc::Inst().GetCaptureOptions().allowFullscreen ? 1 : 0);
    case eRENDERDOC_Option_APIValidation:
      return (RenderDoc::Inst().GetCaptureOptions().apiValidation ? 1 : 0);
    case eRENDERDOC_Option_CaptureCallstacks:
      return (RenderDoc::Inst().GetCaptureOptions().captureCallstacks ? 1 : 0);
    case eRENDERDOC_Option_CaptureCallstacksOnlyDraws:
      return (RenderDoc::Inst().GetCaptureOptions().captureCallstacksOnlyDraws ? 1 : 0);
    case eRENDERDOC_Option_DelayForDebugger:
      return (RenderDoc::Inst().GetCaptureOptions().delayForDebugger);
    case eRENDERDOC_Option_VerifyBufferAccess:
      return (RenderDoc::Inst().GetCaptureOptions().verifyBufferAccess ? 1 : 0);
    case eRENDERDOC_Option_HookIntoChildren:
      return (RenderDoc::Inst().GetCaptureOptions().hookIntoChildren ? 1 : 0);
    case eRENDERDOC_Option_RefAllResources:
      return (RenderDoc::Inst().GetCaptureOptions().refAllResources ? 1 : 0);
    case eRENDERDOC_Option_SaveAllInitials:
      // option is deprecated - always enabled
      return 1;
    case eRENDERDOC_Option_CaptureAllCmdLists:
      return (RenderDoc::Inst().GetCaptureOptions().captureAllCmdLists ? 1 : 0);
    case eRENDERDOC_Option_DebugOutputMute:
      return (RenderDoc::Inst().GetCaptureOptions().debugOutputMute ? 1 : 0);
    case eRENDERDOC_Option_AllowUnsupportedVendorExtensions: return 0;
    default: break;
  }

  RDCLOG("Unrecognised capture option '%d'", opt);
  return 0xffffffff;
}

float RENDERDOC_CC GetCaptureOptionF32(RENDERDOC_CaptureOption opt)
{
  switch(opt)
  {
    case eRENDERDOC_Option_AllowVSync:
      return (RenderDoc::Inst().GetCaptureOptions().allowVSync ? 1.0f : 0.0f);
    case eRENDERDOC_Option_AllowFullscreen:
      return (RenderDoc::Inst().GetCaptureOptions().allowFullscreen ? 1.0f : 0.0f);
    case eRENDERDOC_Option_APIValidation:
      return (RenderDoc::Inst().GetCaptureOptions().apiValidation ? 1.0f : 0.0f);
    case eRENDERDOC_Option_CaptureCallstacks:
      return (RenderDoc::Inst().GetCaptureOptions().captureCallstacks ? 1.0f : 0.0f);
    case eRENDERDOC_Option_CaptureCallstacksOnlyDraws:
      return (RenderDoc::Inst().GetCaptureOptions().captureCallstacksOnlyDraws ? 1.0f : 0.0f);
    case eRENDERDOC_Option_DelayForDebugger:
      return (RenderDoc::Inst().GetCaptureOptions().delayForDebugger * 1.0f);
    case eRENDERDOC_Option_VerifyBufferAccess:
      return (RenderDoc::Inst().GetCaptureOptions().verifyBufferAccess ? 1.0f : 0.0f);
    case eRENDERDOC_Option_HookIntoChildren:
      return (RenderDoc::Inst().GetCaptureOptions().hookIntoChildren ? 1.0f : 0.0f);
    case eRENDERDOC_Option_RefAllResources:
      return (RenderDoc::Inst().GetCaptureOptions().refAllResources ? 1.0f : 0.0f);
    case eRENDERDOC_Option_SaveAllInitials:
      // option is deprecated - always enabled
      return 1.0f;
    case eRENDERDOC_Option_CaptureAllCmdLists:
      return (RenderDoc::Inst().GetCaptureOptions().captureAllCmdLists ? 1.0f : 0.0f);
    case eRENDERDOC_Option_DebugOutputMute:
      return (RenderDoc::Inst().GetCaptureOptions().debugOutputMute ? 1.0f : 0.0f);
    case eRENDERDOC_Option_AllowUnsupportedVendorExtensions: return 0.0f;
    default: break;
  }

  RDCLOG("Unrecognised capture option '%d'", opt);
  return -FLT_MAX;
}

CaptureOptions::CaptureOptions()
{
  allowVSync = true;
  allowFullscreen = true;
  apiValidation = false;
  captureCallstacks = false;
  captureCallstacksOnlyDraws = false;
  delayForDebugger = 0;
  verifyBufferAccess = false;
  hookIntoChildren = false;
  refAllResources = false;
  captureAllCmdLists = false;
  debugOutputMute = true;
}
