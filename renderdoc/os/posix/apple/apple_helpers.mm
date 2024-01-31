/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2024 Baldur Karlsson
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

#include "api/replay/rdcstr.h"
#include "common/common.h"

#import <Cocoa/Cocoa.h>

#define MAX_COUNT_KEYS (65536)
static bool s_keysPressed[MAX_COUNT_KEYS];
static id s_eventMonitor;

void apple_InitKeyboard()
{
  s_eventMonitor =
      [NSEvent addLocalMonitorForEventsMatchingMask:(NSEventMaskKeyDown | NSEventMaskKeyUp)
                                            handler:^(NSEvent *incomingEvent) {
                                              NSEvent *result = incomingEvent;
                                              // NSWindow *targetWindowForEvent = [incomingEvent
                                              // window];
                                              // if (targetWindowForEvent == _window)
                                              {
                                                unsigned short keyCode = [incomingEvent keyCode];
                                                if([incomingEvent type] == NSEventTypeKeyDown)
                                                {
                                                  s_keysPressed[keyCode] = true;
                                                }
                                                if([incomingEvent type] == NSEventTypeKeyUp)
                                                {
                                                  s_keysPressed[keyCode] = false;
                                                }
                                              }
                                              return result;
                                            }];
}

bool apple_IsKeyPressed(int appleKeyCode)
{
  return s_keysPressed[appleKeyCode];
}

rdcstr apple_GetExecutablePathFromAppBundle(const char *appBundlePath)
{
  NSString *path = [NSString stringWithCString:appBundlePath encoding:NSUTF8StringEncoding];

  NSBundle *nsBundle = [NSBundle bundleWithPath:path];
  if(!nsBundle)
  {
    RDCWARN("Failed to open application '%s' as an NSBundle", appBundlePath);
    return rdcstr(appBundlePath);
  }

  NSString *executablePath = nsBundle.executablePath;
  if(!executablePath)
  {
    RDCERR("Failed to get executable path from application '%s'", appBundlePath);
    return rdcstr();
  }

  rdcstr result([executablePath cStringUsingEncoding:NSUTF8StringEncoding]);
  return result;
}
