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

#pragma once

#import <Metal/MTLDevice.h>
#include "common/common.h"

// Global scope Metal functions from MTLDevice.h
typedef id<MTLDevice> (*PFN_MTLCreateSystemDefaultDevice)(void);
typedef NSArray<id<MTLDevice>> *(*PFN_MTLCopyAllDevices)(void);
typedef NSArray<id<MTLDevice>> *(*PFN_MTLCopyAllDevicesWithObserver)(
    id<NSObject> *observer, MTLDeviceNotificationHandler handler);
typedef void (*PFN_MTLRemoveDeviceObserver)(id<NSObject> observer);

// TODO: Global scope Metal device method from CGDirectDisplayMetal.h
// CG_EXTERN id<MTLDevice> __nullable CGDirectDisplayCopyCurrentMetalDevice(CGDirectDisplayID
// display)
// NS_RETURNS_RETAINED CG_AVAILABLE_STARTING(10.11);

#define METAL_HOOKED_SYMBOLS(FUNC) FUNC(MTLCreateSystemDefaultDevice);

#define METAL_NONHOOKED_SYMBOLS(FUNC)  \
  FUNC(MTLCopyAllDevices);             \
  FUNC(MTLCopyAllDevicesWithObserver); \
  FUNC(MTLRemoveDeviceObserver);

struct MetalDispatchTable
{
  bool PopulateForReplay();

#define METAL_PTR_GEN(func) CONCAT(PFN_, func) func;
  METAL_HOOKED_SYMBOLS(METAL_PTR_GEN)
  METAL_NONHOOKED_SYMBOLS(METAL_PTR_GEN)
#undef METAL_PTR_GEN
};

extern MetalDispatchTable METAL;
