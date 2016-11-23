/******************************************************************************
 * The MIT License (MIT)
 *
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

#include "gles_hooks_posix.h"

#include "driver/gles/gles_common.h"
#include "driver/gles/gles_driver.h"

/* Create wrapper method definitions */
#include "driver/gles/gles_hookset_defs.h"


// don't want these definitions, the only place we'll use these is as parameter/variable names
#ifdef near
#undef near
#endif

#ifdef far
#undef far
#endif

//#define DUMP_GL_ERRORS

#ifdef DUMP_GL_ERRORS
class GLESError
{
public:
  GLESError(const char *function_arg) : function_arg(function_arg) {}
  ~GLESError()
  {
    GLenum errorResult = OpenGLHook::GetInstance().GetDriver()->glGetError();
    if(errorResult != GL_NO_ERROR)
    {
      RDCLOG("GL ES error: %s : %p", function_arg, errorResult);
    }
  }

private:
  const char *function_arg;
};

#define DEBUG_WRAPPER(function) GLESError __error_test(#function)

#else /* !DUMP_GL_ERRORS */

#define DEBUG_WRAPPER(function)

#endif /* !DUMP_GL_ERRORS */

Threading::CriticalSection glLock;

#define SCOPED_LOCK_GUARD() SCOPED_LOCK(glLock)
#define DEBUG_HOOKED(function)
#define DRIVER() OpenGLHook::GetInstance().GetDriver()

// the _renderdoc_hooked variants are to make sure we always have a function symbol
// exported that we can return from glXGetProcAddress. If another library (or the app)
// creates a symbol called 'glEnable' we'll return the address of that, and break
// badly. Instead we leave the 'naked' versions for applications trying to import those
// symbols, and declare the _renderdoc_hooked for returning as a func pointer.

#include "gles_hooks_posix_defines_supported_impl.h"

DefineDLLExportHooks();
DefineGLExtensionHooks();

#include "gles_hooks_posix_defines_unsupported_impl.h"

DefineUnsupportedDummies();


