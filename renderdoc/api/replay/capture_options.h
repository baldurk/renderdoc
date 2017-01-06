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

#pragma once

#include <stdint.h>

typedef uint32_t bool32;

// see renderdoc_app.h RENDERDOC_CaptureOption
struct CaptureOptions
{
// for convenience, don't export the constructor but allow it within the module
// for constructing defaults
#ifdef RENDERDOC_EXPORTS
  CaptureOptions();
#endif

  bool32 AllowVSync;
  bool32 AllowFullscreen;
  bool32 APIValidation;
  bool32 CaptureCallstacks;
  bool32 CaptureCallstacksOnlyDraws;
  uint32_t DelayForDebugger;
  bool32 VerifyMapWrites;
  bool32 HookIntoChildren;
  bool32 RefAllResources;
  bool32 SaveAllInitials;
  bool32 CaptureAllCmdLists;
  bool32 DebugOutputMute;
};
