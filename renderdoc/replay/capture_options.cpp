/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2017 Baldur Karlsson
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
    case eRENDERDOC_Option_AllowVSync: opts.AllowVSync = (val != 0); break;
    case eRENDERDOC_Option_AllowFullscreen: opts.AllowFullscreen = (val != 0); break;
    case eRENDERDOC_Option_APIValidation: opts.APIValidation = (val != 0); break;
    case eRENDERDOC_Option_CaptureCallstacks: opts.CaptureCallstacks = (val != 0); break;
    case eRENDERDOC_Option_CaptureCallstacksOnlyDraws:
      opts.CaptureCallstacksOnlyDraws = (val != 0);
      break;
    case eRENDERDOC_Option_DelayForDebugger: opts.DelayForDebugger = val; break;
    case eRENDERDOC_Option_VerifyMapWrites: opts.VerifyMapWrites = (val != 0); break;
    case eRENDERDOC_Option_HookIntoChildren: opts.HookIntoChildren = (val != 0); break;
    case eRENDERDOC_Option_RefAllResources: opts.RefAllResources = (val != 0); break;
    case eRENDERDOC_Option_SaveAllInitials: opts.SaveAllInitials = (val != 0); break;
    case eRENDERDOC_Option_CaptureAllCmdLists: opts.CaptureAllCmdLists = (val != 0); break;
    case eRENDERDOC_Option_DebugOutputMute: opts.DebugOutputMute = (val != 0); break;
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
    case eRENDERDOC_Option_AllowVSync: opts.AllowVSync = (val != 0.0f); break;
    case eRENDERDOC_Option_AllowFullscreen: opts.AllowFullscreen = (val != 0.0f); break;
    case eRENDERDOC_Option_APIValidation: opts.APIValidation = (val != 0.0f); break;
    case eRENDERDOC_Option_CaptureCallstacks: opts.CaptureCallstacks = (val != 0.0f); break;
    case eRENDERDOC_Option_CaptureCallstacksOnlyDraws:
      opts.CaptureCallstacksOnlyDraws = (val != 0.0f);
      break;
    case eRENDERDOC_Option_DelayForDebugger: opts.DelayForDebugger = (uint32_t)val; break;
    case eRENDERDOC_Option_VerifyMapWrites: opts.VerifyMapWrites = (val != 0.0f); break;
    case eRENDERDOC_Option_HookIntoChildren: opts.HookIntoChildren = (val != 0.0f); break;
    case eRENDERDOC_Option_RefAllResources: opts.RefAllResources = (val != 0.0f); break;
    case eRENDERDOC_Option_SaveAllInitials: opts.SaveAllInitials = (val != 0.0f); break;
    case eRENDERDOC_Option_CaptureAllCmdLists: opts.CaptureAllCmdLists = (val != 0.0f); break;
    case eRENDERDOC_Option_DebugOutputMute: opts.DebugOutputMute = (val != 0.0f); break;
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
      return (RenderDoc::Inst().GetCaptureOptions().AllowVSync ? 1 : 0);
    case eRENDERDOC_Option_AllowFullscreen:
      return (RenderDoc::Inst().GetCaptureOptions().AllowFullscreen ? 1 : 0);
    case eRENDERDOC_Option_APIValidation:
      return (RenderDoc::Inst().GetCaptureOptions().APIValidation ? 1 : 0);
    case eRENDERDOC_Option_CaptureCallstacks:
      return (RenderDoc::Inst().GetCaptureOptions().CaptureCallstacks ? 1 : 0);
    case eRENDERDOC_Option_CaptureCallstacksOnlyDraws:
      return (RenderDoc::Inst().GetCaptureOptions().CaptureCallstacksOnlyDraws ? 1 : 0);
    case eRENDERDOC_Option_DelayForDebugger:
      return (RenderDoc::Inst().GetCaptureOptions().DelayForDebugger);
    case eRENDERDOC_Option_VerifyMapWrites:
      return (RenderDoc::Inst().GetCaptureOptions().VerifyMapWrites ? 1 : 0);
    case eRENDERDOC_Option_HookIntoChildren:
      return (RenderDoc::Inst().GetCaptureOptions().HookIntoChildren ? 1 : 0);
    case eRENDERDOC_Option_RefAllResources:
      return (RenderDoc::Inst().GetCaptureOptions().RefAllResources ? 1 : 0);
    case eRENDERDOC_Option_SaveAllInitials:
      return (RenderDoc::Inst().GetCaptureOptions().SaveAllInitials ? 1 : 0);
    case eRENDERDOC_Option_CaptureAllCmdLists:
      return (RenderDoc::Inst().GetCaptureOptions().CaptureAllCmdLists ? 1 : 0);
    case eRENDERDOC_Option_DebugOutputMute:
      return (RenderDoc::Inst().GetCaptureOptions().DebugOutputMute ? 1 : 0);
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
      return (RenderDoc::Inst().GetCaptureOptions().AllowVSync ? 1.0f : 0.0f);
    case eRENDERDOC_Option_AllowFullscreen:
      return (RenderDoc::Inst().GetCaptureOptions().AllowFullscreen ? 1.0f : 0.0f);
    case eRENDERDOC_Option_APIValidation:
      return (RenderDoc::Inst().GetCaptureOptions().APIValidation ? 1.0f : 0.0f);
    case eRENDERDOC_Option_CaptureCallstacks:
      return (RenderDoc::Inst().GetCaptureOptions().CaptureCallstacks ? 1.0f : 0.0f);
    case eRENDERDOC_Option_CaptureCallstacksOnlyDraws:
      return (RenderDoc::Inst().GetCaptureOptions().CaptureCallstacksOnlyDraws ? 1.0f : 0.0f);
    case eRENDERDOC_Option_DelayForDebugger:
      return (RenderDoc::Inst().GetCaptureOptions().DelayForDebugger * 1.0f);
    case eRENDERDOC_Option_VerifyMapWrites:
      return (RenderDoc::Inst().GetCaptureOptions().VerifyMapWrites ? 1.0f : 0.0f);
    case eRENDERDOC_Option_HookIntoChildren:
      return (RenderDoc::Inst().GetCaptureOptions().HookIntoChildren ? 1.0f : 0.0f);
    case eRENDERDOC_Option_RefAllResources:
      return (RenderDoc::Inst().GetCaptureOptions().RefAllResources ? 1.0f : 0.0f);
    case eRENDERDOC_Option_SaveAllInitials:
      return (RenderDoc::Inst().GetCaptureOptions().SaveAllInitials ? 1.0f : 0.0f);
    case eRENDERDOC_Option_CaptureAllCmdLists:
      return (RenderDoc::Inst().GetCaptureOptions().CaptureAllCmdLists ? 1.0f : 0.0f);
    case eRENDERDOC_Option_DebugOutputMute:
      return (RenderDoc::Inst().GetCaptureOptions().DebugOutputMute ? 1.0f : 0.0f);
    default: break;
  }

  RDCLOG("Unrecognised capture option '%d'", opt);
  return -FLT_MAX;
}

CaptureOptions::CaptureOptions()
{
  AllowVSync = true;
  AllowFullscreen = true;
  APIValidation = false;
  CaptureCallstacks = false;
  CaptureCallstacksOnlyDraws = false;
  DelayForDebugger = 0;
  VerifyMapWrites = false;
  HookIntoChildren = false;
  RefAllResources = false;
  SaveAllInitials = false;
  CaptureAllCmdLists = false;
  DebugOutputMute = true;
}
