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

#pragma once

#include <stdint.h>
#include "apidefs.h"
#include "rdcstr.h"
#include "stringise.h"

typedef uint8_t byte;

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

  DOCUMENT(R"(Encode the current options to a string suitable for passing around between processes.

:return: The encoded string, suitable for passing to :meth:`DecodeFromString`.
:rtype: str
)");
  inline rdcstr EncodeAsString() const
  {
    rdcstr optstr;
    optstr.reserve(sizeof(CaptureOptions) * 2 + 1);
    const byte *b = (const byte *)this;
    for(size_t i = 0; i < sizeof(CaptureOptions); i++)
    {
      optstr.push_back(char('a' + ((b[i] >> 4) & 0xf)));
      optstr.push_back(char('a' + ((b[i]) & 0xf)));
    }

    return optstr;
  }

  DOCUMENT(R"(Decode the options from a string, as returned by :meth:`EncodeAsString`. Updates this
object in place.

:param str encoded: The encoded string, as returned by :meth:`EncodeAsString`.
)");
  inline void DecodeFromString(const rdcstr &encoded)
  {
    if(encoded.size() < sizeof(CaptureOptions))
      return;

    // serialise from string with two chars per byte
    byte *b = (byte *)this;
    for(size_t i = 0; i < sizeof(CaptureOptions); i++)
      *(b++) = byte(((byte(encoded[i * 2 + 0] - 'a') & 0xf) << 4) |
                    (byte(encoded[i * 2 + 1] - 'a') & 0xf));
  }

  DOCUMENT(R"(Allow the application to enable vsync.

Default - enabled

``True`` - The application can enable or disable vsync at will.

``False`` - vsync is force disabled.
)");
  bool allowVSync;

  DOCUMENT(R"(Allow the application to enable fullscreen.

Default - enabled

``True`` - The application can enable or disable fullscreen at will.

``False`` - fullscreen is force disabled.
)");
  bool allowFullscreen;

  DOCUMENT(R"(Record API debugging events and messages

Default - disabled

``True`` - Enable built-in API debugging features and records the results into
the capture logfile, which is matched up with events on replay.

``False`` - no API debugging is forcibly enabled.
)");
  bool apiValidation;

  DOCUMENT(R"(Capture CPU callstacks for API events

Default - disabled

``True`` - Enables capturing of callstacks.

``False`` - no callstacks are captured.
)");
  bool captureCallstacks;

  DOCUMENT(R"(When capturing CPU callstacks, only capture them from actions.
This option does nothing if :data:`captureCallstacks` is not enabled.

Default - disabled

``True`` - Only captures callstacks for actions.

``False`` - Callstacks, if enabled, are captured for every event.
)");
  bool captureCallstacksOnlyActions;

  DOCUMENT(R"(Specify a delay in seconds to wait for a debugger to attach, after
creating or injecting into a process, before continuing to allow it to run.

``0`` indicates no delay, and the process will run immediately after injection.

Default - 0 seconds
)");
  uint32_t delayForDebugger;

  DOCUMENT(R"(Verify buffer access. This includes checking the memory returned by a Map() call to
detect any out-of-bounds modification, as well as initialising buffers with undefined contents to
a marker value to catch use of uninitialised memory.

.. note::

  This option is only valid for OpenGL and D3D11. Explicit APIs such as D3D12 and Vulkan do not
  do the same kind of interception & checking and undefined contents are really undefined.

Default - disabled

``True`` - Verify buffer access.

``False`` - No verification is performed, and overwriting bounds may cause crashes or corruption in
RenderDoc.
)");
  bool verifyBufferAccess;

  DOCUMENT(R"(Hooks any system API calls that create child processes, and injects
RenderDoc into them recursively with the same options.

Default - disabled

``True`` - Hooks into spawned child processes.

``False`` - Child processes are not hooked by RenderDoc.
)");
  bool hookIntoChildren;

  DOCUMENT(R"(By default RenderDoc only includes resources in the final logfile necessary
for that frame, this allows you to override that behaviour.

Default - disabled

``True`` - all live resources at the time of capture are included in the log
and available for inspection.

``False`` - only the resources referenced by the captured frame are included.
)");
  bool refAllResources;

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
  bool captureAllCmdLists;

  DOCUMENT(R"(Mute API debugging output when the API validation mode option is enabled.

Default - enabled

``True`` - Mute any API debug messages from being displayed or passed through.

``False`` - API debugging is displayed as normal.
)");
  bool debugOutputMute;

  DOCUMENT(R"(Define a soft memory limit which some APIs may aim to keep overhead under where
possible. Anything above this limit will where possible be saved directly to disk during capture.
This will cause increased disk space use (which may cause a capture to fail if disk space is
exhausted) as well as slower capture times.

Not all memory allocations may be deferred like this so it is not a guarantee of a memory limit.

Units are in MBs, suggested values would range from 200MB to 1000MB.

Default - 0 Megabytes
)");
  uint32_t softMemoryLimit;
};

DECLARE_REFLECTION_STRUCT(CaptureOptions);
