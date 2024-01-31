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

#import <Cocoa/Cocoa.h>
#import <QuartzCore/CAMetalLayer.h>

struct cocoa_Window
{
  NSWindow *nsWindow;
  NSView *nsView;
  bool shouldClose;
};

@interface cocoa_WindowApplicationDelegate : NSObject
@end

@implementation cocoa_WindowApplicationDelegate

- (void)applicationDidFinishLaunching:(NSNotification *)notification
{
  @autoreleasepool
  {
    NSEvent *event = [NSEvent otherEventWithType:NSEventTypeApplicationDefined
                                        location:NSMakePoint(0, 0)
                                   modifierFlags:0
                                       timestamp:0
                                    windowNumber:0
                                         context:nil
                                         subtype:0
                                           data1:0
                                           data2:0];
    [NSApp postEvent:event atStart:YES];
    [NSApp stop:nil];
  }
}

@end

@interface cocoa_WindowDelegate : NSObject
{
  cocoa_Window *window;
}

- (instancetype)initWithCocoaWindow:(cocoa_Window *)initWindow;

@end

@implementation cocoa_WindowDelegate

- (instancetype)initWithCocoaWindow:(cocoa_Window *)initWindow
{
  self = [super init];
  assert(self);
  window = initWindow;
  return self;
}

- (BOOL)windowShouldClose:(id)sender
{
  window->shouldClose = true;
  return NO;
}
@end

void *cocoa_windowCreate(int width, int height, const char *title)
{
  cocoa_Window *window = (cocoa_Window *)calloc(1, sizeof(cocoa_Window));

  @autoreleasepool
  {
    [NSApplication sharedApplication];
    [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
    [NSApp activateIgnoringOtherApps:YES];

    id appDelegate = [[cocoa_WindowApplicationDelegate alloc] init];
    [NSApp setDelegate:appDelegate];

    if(![[NSRunningApplication currentApplication] isFinishedLaunching])
      [NSApp run];
  }

  NSRect contentRect;
  contentRect = NSMakeRect(0, 0, width, height);

  window->nsWindow =
      [[NSWindow alloc] initWithContentRect:contentRect
                                  styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                                            NSWindowStyleMaskMiniaturizable
                                    backing:NSBackingStoreBuffered
                                      defer:NO];
  assert(window->nsWindow);

  window->nsView = [[NSView alloc] initWithFrame:contentRect];
  assert(window->nsView);
  [window->nsView setLayer:[CAMetalLayer layer]];
  [window->nsView setWantsLayer:YES];

  id windowDelegate = [[cocoa_WindowDelegate alloc] initWithCocoaWindow:window];
  assert(windowDelegate);

  [window->nsWindow center];
  [window->nsWindow setContentView:window->nsView];
  [window->nsWindow makeFirstResponder:window->nsView];
  [window->nsWindow setTitle:@(title)];
  [window->nsWindow setDelegate:windowDelegate];

  [window->nsWindow orderFront:nil];
  [window->nsWindow makeKeyAndOrderFront:nil];
  return (void *)window;
}

void *cocoa_windowGetView(void *cocoaWindow)
{
  cocoa_Window *window = (cocoa_Window *)cocoaWindow;
  assert(window);
  return (void *)(window->nsView);
}

void *cocoa_windowGetLayer(void *cocoaWindow)
{
  cocoa_Window *window = (cocoa_Window *)cocoaWindow;
  assert(window);
  return (void *)(window->nsView.layer);
}

bool cocoa_windowShouldClose(void *cocoaWindow)
{
  cocoa_Window *window = (cocoa_Window *)cocoaWindow;
  assert(window);
  return window->shouldClose;
}

bool cocoa_windowPoll(unsigned short &appleKeyCode)
{
  bool keyUp = false;
  @autoreleasepool
  {
    for(;;)
    {
      NSEvent *event = [NSApp nextEventMatchingMask:NSEventMaskAny
                                          untilDate:[NSDate distantPast]
                                             inMode:NSDefaultRunLoopMode
                                            dequeue:YES];
      if(event == nil)
        break;

      if([event type] == NSEventTypeKeyUp)
      {
        appleKeyCode = [event keyCode];
        keyUp = true;
      }

      [NSApp sendEvent:event];
    }
  }
  return keyUp;
}
