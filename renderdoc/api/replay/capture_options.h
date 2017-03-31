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

// see renderdoc_app.h RENDERDOC_CaptureOption - make sure any changes here are reflected there, to
// the options or to the documentation
DOCUMENT(R"(Sets up configuration and options for optional features either at capture time or at API
initialisation time that the user can enable or disable at will.
)");
struct CaptureOptions
{
// for convenience, don't export the constructor but allow it within the module
// for constructing defaults
#ifdef RENDERDOC_EXPORTS
  CaptureOptions();
#endif

  DOCUMENT(R"(Allow the application to enable vsync.

Default - enabled

``True`` - The application can enable or disable vsync at will.

``False`` - vsync is force disabled.
)");
  bool32 AllowVSync;

  DOCUMENT(R"(Allow the application to enable fullscreen.

Default - enabled

``True`` - The application can enable or disable fullscreen at will.

``False`` - fullscreen is force disabled.
)");
  bool32 AllowFullscreen;

  DOCUMENT(R"(Record API debugging events and messages

Default - disabled

``True`` - Enable built-in API debugging features and records the results into
the capture logfile, which is matched up with events on replay.

``False`` - no API debugging is forcibly enabled.
)");
  bool32 APIValidation;

  DOCUMENT(R"(Capture CPU callstacks for API events

Default - disabled

``True`` - Enables capturing of callstacks.

``False`` - no callstacks are captured.
)");
  bool32 CaptureCallstacks;

  DOCUMENT(R"(When capturing CPU callstacks, only capture them from drawcalls.
This option does nothing if :data:`CaptureCallstacks` is not enabled.

Default - disabled

``True`` - Only captures callstacks for drawcall type API events.

``False`` - Callstacks, if enabled, are captured for every event.
)");
  bool32 CaptureCallstacksOnlyDraws;

  DOCUMENT(R"(Specify a delay in seconds to wait for a debugger to attach, after
creating or injecting into a process, before continuing to allow it to run.

``0`` indicates no delay, and the process will run immediately after injection.

Default - 0 seconds
)");
  uint32_t DelayForDebugger;

  DOCUMENT(R"(Verify any writes to mapped buffers, by checking the memory after the
bounds of the returned pointer to detect any modification.

Default - disabled

``True`` - Verify any writes to mapped buffers.

``False`` - No verification is performed, and overwriting bounds may cause
crashes or corruption in RenderDoc.
)");
  bool32 VerifyMapWrites;

  DOCUMENT(R"(Hooks any system API calls that create child processes, and injects
RenderDoc into them recursively with the same options.

Default - disabled

``True`` - Hooks into spawned child processes.

``False`` - Child processes are not hooked by RenderDoc.
)");
  bool32 HookIntoChildren;

  DOCUMENT(R"(By default RenderDoc only includes resources in the final logfile necessary
for that frame, this allows you to override that behaviour.

Default - disabled

``True`` - all live resources at the time of capture are included in the log
and available for inspection.

``False`` - only the resources referenced by the captured frame are included.
)");
  bool32 RefAllResources;

  DOCUMENT(R"(By default RenderDoc skips saving initial states for resources where the
previous contents don't appear to be used, assuming that writes before
reads indicate previous contents aren't used.

Default - disabled

``True`` - initial contents at the start of each captured frame are saved, even if
    they are later overwritten or cleared before being used.

``False`` - unless a read is detected, initial contents will not be saved and will
    appear as black or empty data.
)");
  bool32 SaveAllInitials;

  DOCUMENT(R"(In APIs that allow for the recording of command lists to be replayed later,
RenderDoc may choose to not capture command lists before a frame capture is
triggered, to reduce overheads. This means any command lists recorded once
and replayed many times will not be available and may cause a failure to
capture.

.. note:: This is only true for APIs where multithreading is difficult or
  discouraged. Newer APIs like Vulkan and D3D12 will ignore this option
  and always capture all command lists since the API is heavily oriented
  around it and the overheads have been reduced by API design.

``True`` - All command lists are captured from the start of the application.

``False`` - Command lists are only captured if their recording begins during
the period when a frame capture is in progress.
)");
  bool32 CaptureAllCmdLists;

  DOCUMENT(R"(Mute API debugging output when the API validation mode option is enabled.

Default - enabled

``True`` - Mute any API debug messages from being displayed or passed through.

``False`` - API debugging is displayed as normal.
)");
  bool32 DebugOutputMute;
};
