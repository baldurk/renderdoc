/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2022-2024 Baldur Karlsson
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

#include "metal_hook.h"
#include "core/core.h"
#include "hooks/hooks.h"
#include "metal_device.h"
#include "metal_dispatch_table_bridge.h"
#include "metal_types_bridge.h"

#define METAL_EXPORT_NAME(function) CONCAT(interposed_, function)

#define DECL_HOOK_EXPORT(function)                                               \
  __attribute__((used)) static struct                                            \
  {                                                                              \
    const void *replacment;                                                      \
    const void *replacee;                                                        \
  } _interpose_def_##function __attribute__((section("__DATA,__interpose"))) = { \
      (const void *)(unsigned long)&METAL_EXPORT_NAME(function),                 \
      (const void *)(unsigned long)&function,                                    \
  };

#define ForEachMetalSupported() METAL_FUNC(MTLCreateSystemDefaultDevice)

id<MTLDevice> METAL_EXPORT_NAME(MTLCreateSystemDefaultDevice)(void)
{
  if(RenderDoc::Inst().IsReplayApp())
  {
    if(!METAL.MTLCreateSystemDefaultDevice)
      METAL.PopulateForReplay();

    return METAL.MTLCreateSystemDefaultDevice();
  }

  id<MTLDevice> device = METAL.MTLCreateSystemDefaultDevice();
  return id<MTLDevice>(WrappedMTLDevice::MTLCreateSystemDefaultDevice((MTL::Device *)device));
}

/*

 APIs not currently hooked

*** MTLDevice.h ***
NSArray <id<MTLDevice>> *METAL_EXPORT_NAME(MTLCopyAllDevices)(void);
NSArray <id<MTLDevice>> *METAL_EXPORT_NAME(MTLCopyAllDevicesWithObserver)(id<NSObject>* observer,
MTLDeviceNotificationHandler handler);
void METAL_EXPORT_NAME(MTLRemoveDeviceObserver)(id <NSObject> observer);

 *** CGDirectDisplayMetal.h ***
 CG_EXTERN id<MTLDevice> __nullable CGDirectDisplayCopyCurrentMetalDevice(CGDirectDisplayID display)
NS_RETURNS_RETAINED CG_AVAILABLE_STARTING(10.11);

 */

void MetalHook::RegisterGlobalNonHookedMetalFunctions()
{
// fetch non-hooked functions into our dispatch table
#define METAL_FETCH(func) METAL.func = &::func;
  METAL_NONHOOKED_SYMBOLS(METAL_FETCH)
#undef METAL_FETCH
}

extern void AppleRegisterRealSymbol(const char *functionName, void *address);

void MetalHook::RegisterGlobalHookedMetalFunctions()
{
#define METAL_FUNC(func)                                     \
  AppleRegisterRealSymbol(STRINGIZE(func), (void *)&::func); \
  LibraryHooks::RegisterFunctionHook(                        \
      "Metal",                                               \
      FunctionHook(STRINGIZE(func), (void **)&METAL.func, (void *)&METAL_EXPORT_NAME(func)));

  ForEachMetalSupported();
#undef METAL_FUNC
}

#define METAL_FUNC(function) DECL_HOOK_EXPORT(function)
ForEachMetalSupported();
#undef METAL_FUNC
