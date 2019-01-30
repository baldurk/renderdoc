/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2018-2019 Baldur Karlsson
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

#include "official/cgl.h"
#include "gl_common.h"

// cgl functions
typedef CGLError (*PFN_CGLCreateContext)(CGLPixelFormatObj pix, CGLContextObj share,
                                         CGLContextObj *ctx);
typedef CGLError (*PFN_CGLSetCurrentContext)(CGLContextObj ctx);
typedef CGLError (*PFN_CGLFlushDrawable)(CGLContextObj ctx);
typedef CGLError (*PFN_CGLDestroyContext)(CGLContextObj ctx);
typedef CGLError (*PFN_CGLDescribePixelFormat)(CGLPixelFormatObj pix, GLint pix_num,
                                               CGLPixelFormatAttribute attrib, GLint *value);
typedef CGLError (*PFN_CGLSetSurface)(CGLContextObj gl, CGSConnectionID cid, CGSWindowID wid,
                                      CGSSurfaceID sid);
typedef CGLError (*PFN_CGLGetSurface)(CGLContextObj gl, CGSConnectionID *cid, CGSWindowID *wid,
                                      CGSSurfaceID *sid);
typedef CGLError (*PFN_CGSGetSurfaceBounds)(CGSConnectionID cid, CGSWindowID wid, CGSSurfaceID sid,
                                            CGRect *rect);
typedef CGLError (*PFN_CGLChoosePixelFormat)(const CGLPixelFormatAttribute *attribs,
                                             CGLPixelFormatObj *pix, GLint *npix);
typedef CGLError (*PFN_CGLDestroyPixelFormat)(CGLPixelFormatObj pix);

#define CGL_HOOKED_SYMBOLS(FUNC) \
  FUNC(CGLCreateContext);        \
  FUNC(CGLSetCurrentContext);    \
  FUNC(CGLFlushDrawable);

#define CGL_NONHOOKED_SYMBOLS(FUNC) \
  FUNC(CGLDestroyContext);          \
  FUNC(CGLDescribePixelFormat);     \
  FUNC(CGLSetSurface);              \
  FUNC(CGLGetSurface);              \
  FUNC(CGSGetSurfaceBounds);        \
  FUNC(CGLChoosePixelFormat);       \
  FUNC(CGLDestroyPixelFormat);

struct CGLDispatchTable
{
  // Although not needed on macOS, for consistency we follow the same pattern as EGLDispatchTable
  // and GLXDispatchTable.
  bool PopulateForReplay();

// Generate the CGL function pointers. We need to consider hooked and non-hooked symbols separately
// - non-hooked symbols don't have a function hook to register, or if they do it's a dummy
// pass-through hook that will risk calling itself via trampoline.
#define CGL_PTR_GEN(func) CONCAT(PFN_, func) func;
  CGL_HOOKED_SYMBOLS(CGL_PTR_GEN)
  CGL_NONHOOKED_SYMBOLS(CGL_PTR_GEN)
#undef CGL_PTR_GEN
};

extern CGLDispatchTable CGL;