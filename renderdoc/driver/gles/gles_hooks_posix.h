/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2016 Baldur Karlsson
 * Copyright (c) 2014 Crytek
 * Copyright (c) 2016 University of Szeged
 * Copyright (c) 2016 Samsung Electronics Co., Ltd.
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

#include "hooks/hooks.h"

#include "driver/gles/gles_hookset.h"
#include "official/egl_func_typedefs.h"

class OpenGLHook : LibraryHook
{
public:
  OpenGLHook();
  ~OpenGLHook();

  const GLHookSet &GetRealGLFunctions();

  static void libHooked(void *realLib);

  bool CreateHooks(const char *libName);

  void EnableHooks(const char *libName, bool enable) { m_EnabledHooks = enable; }
  void OptionsUpdated(const char *libName) {}

  WrappedGLES *GetDriver();

  bool SetupHooks(GLHookSet &GL);
  bool PopulateHooks();

  void MakeContextCurrent(GLESWindowingData data);
  GLESWindowingData MakeContext(GLESWindowingData share);

  void *GetDLHandle() { return m_libGLdlsymHandle; }

  static OpenGLHook glhooks;

  GLHookSet GL;

  std::set<EGLContext> m_Contexts;

  PFN_eglGetProcAddress m_eglGetProcAddress_real;
  PFN_eglSwapBuffers m_eglSwapBuffers_real;
  PFN_eglMakeCurrent m_eglMakeCurrent_real;
  PFN_eglQuerySurface m_eglQuerySurface_real;

private:
  WrappedGLES *m_GLESDriver;

  bool m_PopulatedHooks;
  bool m_HasHooks;
  bool m_EnabledHooks;

  void *m_libGLdlsymHandle =
    RTLD_NEXT; // default to RTLD_NEXT, but overwritten if app calls dlopen() on real libGL
};

