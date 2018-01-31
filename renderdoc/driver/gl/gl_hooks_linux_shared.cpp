/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2017-2018 Baldur Karlsson
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

#include <dlfcn.h>
#include <stdio.h>
#include "common/threading.h"
#include "driver/gl/gl_common.h"
#include "driver/gl/gl_driver.h"
#include "driver/gl/gl_hookset.h"
#include "driver/gl/gl_hookset_defs.h"
#include "hooks/hooks.h"
#include "strings/string_utils.h"

GLHookSet GL;
WrappedOpenGL *m_GLDriver;
Threading::CriticalSection glLock;
void *libGLdlsymHandle =
    RTLD_NEXT;    // default to RTLD_NEXT, but overwritten if app calls dlopen() on real libGL

Threading::CriticalSection &GetGLLock()
{
  return glLock;
}

#define HookInit(function)                                 \
  if(!strcmp(func, STRINGIZE(function)))                   \
  {                                                        \
    if(GL.function == NULL)                                \
      GL.function = (CONCAT(function, _hooktype))realFunc; \
    return (void *)&CONCAT(function, _renderdoc_hooked);   \
  }

#define HookExtension(funcPtrType, function)             \
  if(!strcmp(func, STRINGIZE(function)))                 \
  {                                                      \
    if(GL.function == NULL)                              \
      GL.function = (funcPtrType)realFunc;               \
    return (void *)&CONCAT(function, _renderdoc_hooked); \
  }

#define HookExtensionAlias(funcPtrType, function, alias) \
  if(!strcmp(func, STRINGIZE(alias)))                    \
  {                                                      \
    if(GL.function == NULL)                              \
      GL.function = (funcPtrType)realFunc;               \
    return (void *)&CONCAT(function, _renderdoc_hooked); \
  }

#if 0    // debug print for each unsupported function requested (but not used)
#define HandleUnsupported(funcPtrType, function)                                           \
  if(lowername == STRINGIZE(function))                                                     \
  {                                                                                        \
    CONCAT(unsupported_real_, function) = (CONCAT(function, _hooktype))realFunc;           \
    RDCDEBUG("Requesting function pointer for unsupported function " STRINGIZE(function)); \
    return (void *)&CONCAT(function, _renderdoc_hooked);                                   \
  }
#else
#define HandleUnsupported(funcPtrType, function)                                 \
  if(lowername == STRINGIZE(function))                                           \
  {                                                                              \
    CONCAT(unsupported_real_, function) = (CONCAT(function, _hooktype))realFunc; \
    return (void *)&CONCAT(function, _renderdoc_hooked);                         \
  }
#endif

/*
  in bash:

    function WrapperMacro()
    {
        NAME=$1;
        N=$2;
        ALIAS=$3;
        if [ $ALIAS -eq 1 ]; then
          echo -n "#define $NAME$N(ret, function, realfunc";
        else
          echo -n "#define $NAME$N(ret, function";
        fi
            for I in `seq 1 $N`; do echo -n ", t$I, p$I"; done;
        echo ") \\";

        echo -en "  typedef ret (*CONCAT(function, _hooktype))(";
            for I in `seq 1 $N`; do echo -n "t$I"; if [ $I -ne $N ]; then echo -n ", "; fi; done;
        echo "); \\";

        echo -en "  extern \"C\" __attribute__((visibility (\"default\"))) ret function(";
            for I in `seq 1 $N`; do echo -n "t$I p$I"; if [ $I -ne $N ]; then echo -n ", "; fi;
  done;
        echo ") \\";

        echo -e "  { \\";
        echo -e "    SCOPED_GLCALL(glLock, function); \\";
        echo -e "    gl_CurChunk = GLChunk::function; \\";
        if [ $ALIAS -eq 1 ]; then
          echo -n "    return m_GLDriver->realfunc(";
        else
          echo -n "    return m_GLDriver->function(";
        fi
            for I in `seq 1 $N`; do echo -n "p$I"; if [ $I -ne $N ]; then echo -n ", "; fi; done;
        echo -e "); \\";
        echo -e "  } \\";

        echo -en "  ret CONCAT(function,_renderdoc_hooked)(";
            for I in `seq 1 $N`; do echo -n "t$I p$I"; if [ $I -ne $N ]; then echo -n ", "; fi;
  done;
        echo ") \\";

        echo -e "  { \\";
        echo -e "    SCOPED_GLCALL(glLock, function); \\";
        echo -e "    gl_CurChunk = GLChunk::function; \\";
        if [ $ALIAS -eq 1 ]; then
          echo -n "    return m_GLDriver->realfunc(";
        else
          echo -n "    return m_GLDriver->function(";
        fi
            for I in `seq 1 $N`; do echo -n "p$I"; if [ $I -ne $N ]; then echo -n ", "; fi; done;
        echo -e "); \\";
        echo -e "  }";
    }

  for I in `seq 0 17`; do WrapperMacro HookWrapper $I 0; echo; done
  for I in `seq 0 17`; do WrapperMacro HookAliasWrapper $I 1; echo; done

  */

// don't want these definitions, the only place we'll use these is as parameter/variable names
#ifdef near
#undef near
#endif

#ifdef far
#undef far
#endif

#if ENABLED(RDOC_DEVEL)

struct ScopedPrinter
{
  ScopedPrinter(const char *f) : func(f)
  {
    // RDCLOG("Entering %s", func);
    depth++;
    if(depth > 100)
      RDCFATAL("Infinite recursion detected!");
  }
  ~ScopedPrinter()
  {
    // RDCLOG("Exiting %s", func);
    depth--;
  }
  const char *func = NULL;
  static int depth;
};

int ScopedPrinter::depth = 0;

// This checks that we're not infinite looping by calling our own hooks from ourselves. Mostly
// useful on android where you can only debug by printf and the stack dumps are often corrupted when
// the callstack overflows.
#define SCOPED_GLCALL(lock, funcname) \
  SCOPED_LOCK(lock);                  \
  ScopedPrinter CONCAT(scopedprint, __LINE__)(STRINGIZE(funcname));

#else

#define SCOPED_GLCALL(lock, funcname) SCOPED_LOCK(lock);

#endif

// the _renderdoc_hooked variants are to make sure we always have a function symbol
// exported that we can return from glXGetProcAddress. If another library (or the app)
// creates a symbol called 'glEnable' we'll return the address of that, and break
// badly. Instead we leave the 'naked' versions for applications trying to import those
// symbols, and declare the _renderdoc_hooked for returning as a func pointer.

#define HookWrapper0(ret, function)                                \
  typedef ret (*CONCAT(function, _hooktype))();                    \
  extern "C" __attribute__((visibility("default"))) ret function() \
  {                                                                \
    SCOPED_GLCALL(glLock, function);                               \
    gl_CurChunk = GLChunk::function;                               \
    return m_GLDriver->function();                                 \
  }                                                                \
  ret CONCAT(function, _renderdoc_hooked)()                        \
  {                                                                \
    SCOPED_GLCALL(glLock, function);                               \
    gl_CurChunk = GLChunk::function;                               \
    return m_GLDriver->function();                                 \
  }

#define HookWrapper1(ret, function, t1, p1)                             \
  typedef ret (*CONCAT(function, _hooktype))(t1);                       \
  extern "C" __attribute__((visibility("default"))) ret function(t1 p1) \
  {                                                                     \
    SCOPED_GLCALL(glLock, function);                                    \
    gl_CurChunk = GLChunk::function;                                    \
    return m_GLDriver->function(p1);                                    \
  }                                                                     \
  ret CONCAT(function, _renderdoc_hooked)(t1 p1)                        \
  {                                                                     \
    SCOPED_GLCALL(glLock, function);                                    \
    gl_CurChunk = GLChunk::function;                                    \
    return m_GLDriver->function(p1);                                    \
  }

#define HookWrapper2(ret, function, t1, p1, t2, p2)                            \
  typedef ret (*CONCAT(function, _hooktype))(t1, t2);                          \
  extern "C" __attribute__((visibility("default"))) ret function(t1 p1, t2 p2) \
  {                                                                            \
    SCOPED_GLCALL(glLock, function);                                           \
    gl_CurChunk = GLChunk::function;                                           \
    return m_GLDriver->function(p1, p2);                                       \
  }                                                                            \
  ret CONCAT(function, _renderdoc_hooked)(t1 p1, t2 p2)                        \
  {                                                                            \
    SCOPED_GLCALL(glLock, function);                                           \
    gl_CurChunk = GLChunk::function;                                           \
    return m_GLDriver->function(p1, p2);                                       \
  }

#define HookWrapper3(ret, function, t1, p1, t2, p2, t3, p3)                           \
  typedef ret (*CONCAT(function, _hooktype))(t1, t2, t3);                             \
  extern "C" __attribute__((visibility("default"))) ret function(t1 p1, t2 p2, t3 p3) \
  {                                                                                   \
    SCOPED_GLCALL(glLock, function);                                                  \
    gl_CurChunk = GLChunk::function;                                                  \
    return m_GLDriver->function(p1, p2, p3);                                          \
  }                                                                                   \
  ret CONCAT(function, _renderdoc_hooked)(t1 p1, t2 p2, t3 p3)                        \
  {                                                                                   \
    SCOPED_GLCALL(glLock, function);                                                  \
    gl_CurChunk = GLChunk::function;                                                  \
    return m_GLDriver->function(p1, p2, p3);                                          \
  }

#define HookWrapper4(ret, function, t1, p1, t2, p2, t3, p3, t4, p4)                          \
  typedef ret (*CONCAT(function, _hooktype))(t1, t2, t3, t4);                                \
  extern "C" __attribute__((visibility("default"))) ret function(t1 p1, t2 p2, t3 p3, t4 p4) \
  {                                                                                          \
    SCOPED_GLCALL(glLock, function);                                                         \
    gl_CurChunk = GLChunk::function;                                                         \
    return m_GLDriver->function(p1, p2, p3, p4);                                             \
  }                                                                                          \
  ret CONCAT(function, _renderdoc_hooked)(t1 p1, t2 p2, t3 p3, t4 p4)                        \
  {                                                                                          \
    SCOPED_GLCALL(glLock, function);                                                         \
    gl_CurChunk = GLChunk::function;                                                         \
    return m_GLDriver->function(p1, p2, p3, p4);                                             \
  }

#define HookWrapper5(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5)                         \
  typedef ret (*CONCAT(function, _hooktype))(t1, t2, t3, t4, t5);                                   \
  extern "C" __attribute__((visibility("default"))) ret function(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5) \
  {                                                                                                 \
    SCOPED_GLCALL(glLock, function);                                                                \
    gl_CurChunk = GLChunk::function;                                                                \
    return m_GLDriver->function(p1, p2, p3, p4, p5);                                                \
  }                                                                                                 \
  ret CONCAT(function, _renderdoc_hooked)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5)                        \
  {                                                                                                 \
    SCOPED_GLCALL(glLock, function);                                                                \
    gl_CurChunk = GLChunk::function;                                                                \
    return m_GLDriver->function(p1, p2, p3, p4, p5);                                                \
  }

#define HookWrapper6(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6)          \
  typedef ret (*CONCAT(function, _hooktype))(t1, t2, t3, t4, t5, t6);                        \
  extern "C" __attribute__((visibility("default"))) ret function(t1 p1, t2 p2, t3 p3, t4 p4, \
                                                                 t5 p5, t6 p6)               \
  {                                                                                          \
    SCOPED_GLCALL(glLock, function);                                                         \
    gl_CurChunk = GLChunk::function;                                                         \
    return m_GLDriver->function(p1, p2, p3, p4, p5, p6);                                     \
  }                                                                                          \
  ret CONCAT(function, _renderdoc_hooked)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6)          \
  {                                                                                          \
    SCOPED_GLCALL(glLock, function);                                                         \
    gl_CurChunk = GLChunk::function;                                                         \
    return m_GLDriver->function(p1, p2, p3, p4, p5, p6);                                     \
  }

#define HookWrapper7(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6, t7, p7)  \
  typedef ret (*CONCAT(function, _hooktype))(t1, t2, t3, t4, t5, t6, t7);                    \
  extern "C" __attribute__((visibility("default"))) ret function(t1 p1, t2 p2, t3 p3, t4 p4, \
                                                                 t5 p5, t6 p6, t7 p7)        \
  {                                                                                          \
    SCOPED_GLCALL(glLock, function);                                                         \
    gl_CurChunk = GLChunk::function;                                                         \
    return m_GLDriver->function(p1, p2, p3, p4, p5, p6, p7);                                 \
  }                                                                                          \
  ret CONCAT(function, _renderdoc_hooked)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7)   \
  {                                                                                          \
    SCOPED_GLCALL(glLock, function);                                                         \
    gl_CurChunk = GLChunk::function;                                                         \
    return m_GLDriver->function(p1, p2, p3, p4, p5, p6, p7);                                 \
  }

#define HookWrapper8(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6, t7, p7, t8, p8) \
  typedef ret (*CONCAT(function, _hooktype))(t1, t2, t3, t4, t5, t6, t7, t8);                       \
  extern "C" __attribute__((visibility("default"))) ret function(t1 p1, t2 p2, t3 p3, t4 p4,        \
                                                                 t5 p5, t6 p6, t7 p7, t8 p8)        \
  {                                                                                                 \
    SCOPED_GLCALL(glLock, function);                                                                \
    gl_CurChunk = GLChunk::function;                                                                \
    return m_GLDriver->function(p1, p2, p3, p4, p5, p6, p7, p8);                                    \
  }                                                                                                 \
  ret CONCAT(function, _renderdoc_hooked)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8)   \
  {                                                                                                 \
    SCOPED_GLCALL(glLock, function);                                                                \
    gl_CurChunk = GLChunk::function;                                                                \
    return m_GLDriver->function(p1, p2, p3, p4, p5, p6, p7, p8);                                    \
  }

#define HookWrapper9(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6, t7, p7, t8,   \
                     p8, t9, p9)                                                                  \
  typedef ret (*CONCAT(function, _hooktype))(t1, t2, t3, t4, t5, t6, t7, t8, t9);                 \
  extern "C" __attribute__((visibility("default"))) ret function(                                 \
      t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8, t9 p9)                              \
  {                                                                                               \
    SCOPED_GLCALL(glLock, function);                                                              \
    gl_CurChunk = GLChunk::function;                                                              \
    return m_GLDriver->function(p1, p2, p3, p4, p5, p6, p7, p8, p9);                              \
  }                                                                                               \
  ret CONCAT(function, _renderdoc_hooked)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8, \
                                          t9 p9)                                                  \
  {                                                                                               \
    SCOPED_GLCALL(glLock, function);                                                              \
    gl_CurChunk = GLChunk::function;                                                              \
    return m_GLDriver->function(p1, p2, p3, p4, p5, p6, p7, p8, p9);                              \
  }

#define HookWrapper10(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6, t7, p7, t8,  \
                      p8, t9, p9, t10, p10)                                                       \
  typedef ret (*CONCAT(function, _hooktype))(t1, t2, t3, t4, t5, t6, t7, t8, t9, t10);            \
  extern "C" __attribute__((visibility("default"))) ret function(                                 \
      t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8, t9 p9, t10 p10)                     \
  {                                                                                               \
    SCOPED_GLCALL(glLock, function);                                                              \
    gl_CurChunk = GLChunk::function;                                                              \
    return m_GLDriver->function(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10);                         \
  }                                                                                               \
  ret CONCAT(function, _renderdoc_hooked)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8, \
                                          t9 p9, t10 p10)                                         \
  {                                                                                               \
    SCOPED_GLCALL(glLock, function);                                                              \
    gl_CurChunk = GLChunk::function;                                                              \
    return m_GLDriver->function(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10);                         \
  }

#define HookWrapper11(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6, t7, p7, t8,  \
                      p8, t9, p9, t10, p10, t11, p11)                                             \
  typedef ret (*CONCAT(function, _hooktype))(t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, t11);       \
  extern "C" __attribute__((visibility("default"))) ret function(                                 \
      t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8, t9 p9, t10 p10, t11 p11)            \
  {                                                                                               \
    SCOPED_GLCALL(glLock, function);                                                              \
    gl_CurChunk = GLChunk::function;                                                              \
    return m_GLDriver->function(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11);                    \
  }                                                                                               \
  ret CONCAT(function, _renderdoc_hooked)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8, \
                                          t9 p9, t10 p10, t11 p11)                                \
  {                                                                                               \
    SCOPED_GLCALL(glLock, function);                                                              \
    gl_CurChunk = GLChunk::function;                                                              \
    return m_GLDriver->function(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11);                    \
  }

#define HookWrapper12(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6, t7, p7, t8,  \
                      p8, t9, p9, t10, p10, t11, p11, t12, p12)                                   \
  typedef ret (*CONCAT(function, _hooktype))(t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, t11, t12);  \
  extern "C" __attribute__((visibility("default"))) ret function(                                 \
      t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8, t9 p9, t10 p10, t11 p11, t12 p12)   \
  {                                                                                               \
    SCOPED_GLCALL(glLock, function);                                                              \
    gl_CurChunk = GLChunk::function;                                                              \
    return m_GLDriver->function(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12);               \
  }                                                                                               \
  ret CONCAT(function, _renderdoc_hooked)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8, \
                                          t9 p9, t10 p10, t11 p11, t12 p12)                       \
  {                                                                                               \
    SCOPED_GLCALL(glLock, function);                                                              \
    gl_CurChunk = GLChunk::function;                                                              \
    return m_GLDriver->function(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12);               \
  }

#define HookWrapper13(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6, t7, p7, t8,  \
                      p8, t9, p9, t10, p10, t11, p11, t12, p12, t13, p13)                         \
  typedef ret (*CONCAT(function, _hooktype))(t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, t11, t12,   \
                                             t13);                                                \
  extern "C" __attribute__((visibility("default"))) ret function(                                 \
      t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8, t9 p9, t10 p10, t11 p11, t12 p12,   \
      t13 p13)                                                                                    \
  {                                                                                               \
    SCOPED_GLCALL(glLock, function);                                                              \
    gl_CurChunk = GLChunk::function;                                                              \
    return m_GLDriver->function(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13);          \
  }                                                                                               \
  ret CONCAT(function, _renderdoc_hooked)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8, \
                                          t9 p9, t10 p10, t11 p11, t12 p12, t13 p13)              \
  {                                                                                               \
    SCOPED_GLCALL(glLock, function);                                                              \
    gl_CurChunk = GLChunk::function;                                                              \
    return m_GLDriver->function(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13);          \
  }

#define HookWrapper14(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6, t7, p7, t8,  \
                      p8, t9, p9, t10, p10, t11, p11, t12, p12, t13, p13, t14, p14)               \
  typedef ret (*CONCAT(function, _hooktype))(t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, t11, t12,   \
                                             t13, t14);                                           \
  extern "C" __attribute__((visibility("default"))) ret function(                                 \
      t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8, t9 p9, t10 p10, t11 p11, t12 p12,   \
      t13 p13, t14 p14)                                                                           \
  {                                                                                               \
    SCOPED_GLCALL(glLock, function);                                                              \
    gl_CurChunk = GLChunk::function;                                                              \
    return m_GLDriver->function(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14);     \
  }                                                                                               \
  ret CONCAT(function, _renderdoc_hooked)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8, \
                                          t9 p9, t10 p10, t11 p11, t12 p12, t13 p13, t14 p14)     \
  {                                                                                               \
    SCOPED_GLCALL(glLock, function);                                                              \
    gl_CurChunk = GLChunk::function;                                                              \
    return m_GLDriver->function(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14);     \
  }

#define HookWrapper15(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6, t7, p7, t8,   \
                      p8, t9, p9, t10, p10, t11, p11, t12, p12, t13, p13, t14, p14, t15, p15)      \
  typedef ret (*CONCAT(function, _hooktype))(t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, t11, t12,    \
                                             t13, t14, t15);                                       \
  extern "C" __attribute__((visibility("default"))) ret function(                                  \
      t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8, t9 p9, t10 p10, t11 p11, t12 p12,    \
      t13 p13, t14 p14, t15 p15)                                                                   \
  {                                                                                                \
    SCOPED_GLCALL(glLock, function);                                                               \
    gl_CurChunk = GLChunk::function;                                                               \
    return m_GLDriver->function(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15); \
  }                                                                                                \
  ret CONCAT(function, _renderdoc_hooked)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8,  \
                                          t9 p9, t10 p10, t11 p11, t12 p12, t13 p13, t14 p14,      \
                                          t15 p15)                                                 \
  {                                                                                                \
    SCOPED_GLCALL(glLock, function);                                                               \
    gl_CurChunk = GLChunk::function;                                                               \
    return m_GLDriver->function(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15); \
  }

#define HookWrapper16(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6, t7, p7, t8,   \
                      p8, t9, p9, t10, p10, t11, p11, t12, p12, t13, p13, t14, p14, t15, p15, t16, \
                      p16)                                                                         \
  typedef ret (*CONCAT(function, _hooktype))(t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, t11, t12,    \
                                             t13, t14, t15, t16);                                  \
  extern "C" __attribute__((visibility("default"))) ret function(                                  \
      t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8, t9 p9, t10 p10, t11 p11, t12 p12,    \
      t13 p13, t14 p14, t15 p15, t16 p16)                                                          \
  {                                                                                                \
    SCOPED_GLCALL(glLock, function);                                                               \
    gl_CurChunk = GLChunk::function;                                                               \
    return m_GLDriver->function(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15,  \
                                p16);                                                              \
  }                                                                                                \
  ret CONCAT(function, _renderdoc_hooked)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8,  \
                                          t9 p9, t10 p10, t11 p11, t12 p12, t13 p13, t14 p14,      \
                                          t15 p15, t16 p16)                                        \
  {                                                                                                \
    SCOPED_GLCALL(glLock, function);                                                               \
    gl_CurChunk = GLChunk::function;                                                               \
    return m_GLDriver->function(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15,  \
                                p16);                                                              \
  }

#define HookWrapper17(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6, t7, p7, t8,   \
                      p8, t9, p9, t10, p10, t11, p11, t12, p12, t13, p13, t14, p14, t15, p15, t16, \
                      p16, t17, p17)                                                               \
  typedef ret (*CONCAT(function, _hooktype))(t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, t11, t12,    \
                                             t13, t14, t15, t16, t17);                             \
  extern "C" __attribute__((visibility("default"))) ret function(                                  \
      t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8, t9 p9, t10 p10, t11 p11, t12 p12,    \
      t13 p13, t14 p14, t15 p15, t16 p16, t17 p17)                                                 \
  {                                                                                                \
    SCOPED_GLCALL(glLock, function);                                                               \
    gl_CurChunk = GLChunk::function;                                                               \
    return m_GLDriver->function(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15,  \
                                p16, p17);                                                         \
  }                                                                                                \
  ret CONCAT(function, _renderdoc_hooked)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8,  \
                                          t9 p9, t10 p10, t11 p11, t12 p12, t13 p13, t14 p14,      \
                                          t15 p15, t16 p16, t17 p17)                               \
  {                                                                                                \
    SCOPED_GLCALL(glLock, function);                                                               \
    gl_CurChunk = GLChunk::function;                                                               \
    return m_GLDriver->function(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15,  \
                                p16, p17);                                                         \
  }

#define HookAliasWrapper0(ret, function, realfunc)                 \
  typedef ret (*CONCAT(function, _hooktype))();                    \
  extern "C" __attribute__((visibility("default"))) ret function() \
  {                                                                \
    SCOPED_GLCALL(glLock, function);                               \
    gl_CurChunk = GLChunk::function;                               \
    return m_GLDriver->realfunc();                                 \
  }                                                                \
  ret CONCAT(function, _renderdoc_hooked)()                        \
  {                                                                \
    SCOPED_GLCALL(glLock, function);                               \
    gl_CurChunk = GLChunk::function;                               \
    return m_GLDriver->realfunc();                                 \
  }

#define HookAliasWrapper1(ret, function, realfunc, t1, p1)              \
  typedef ret (*CONCAT(function, _hooktype))(t1);                       \
  extern "C" __attribute__((visibility("default"))) ret function(t1 p1) \
  {                                                                     \
    SCOPED_GLCALL(glLock, function);                                    \
    gl_CurChunk = GLChunk::function;                                    \
    return m_GLDriver->realfunc(p1);                                    \
  }                                                                     \
  ret CONCAT(function, _renderdoc_hooked)(t1 p1)                        \
  {                                                                     \
    SCOPED_GLCALL(glLock, function);                                    \
    gl_CurChunk = GLChunk::function;                                    \
    return m_GLDriver->realfunc(p1);                                    \
  }

#define HookAliasWrapper2(ret, function, realfunc, t1, p1, t2, p2)             \
  typedef ret (*CONCAT(function, _hooktype))(t1, t2);                          \
  extern "C" __attribute__((visibility("default"))) ret function(t1 p1, t2 p2) \
  {                                                                            \
    SCOPED_GLCALL(glLock, function);                                           \
    gl_CurChunk = GLChunk::function;                                           \
    return m_GLDriver->realfunc(p1, p2);                                       \
  }                                                                            \
  ret CONCAT(function, _renderdoc_hooked)(t1 p1, t2 p2)                        \
  {                                                                            \
    SCOPED_GLCALL(glLock, function);                                           \
    gl_CurChunk = GLChunk::function;                                           \
    return m_GLDriver->realfunc(p1, p2);                                       \
  }

#define HookAliasWrapper3(ret, function, realfunc, t1, p1, t2, p2, t3, p3)            \
  typedef ret (*CONCAT(function, _hooktype))(t1, t2, t3);                             \
  extern "C" __attribute__((visibility("default"))) ret function(t1 p1, t2 p2, t3 p3) \
  {                                                                                   \
    SCOPED_GLCALL(glLock, function);                                                  \
    gl_CurChunk = GLChunk::function;                                                  \
    return m_GLDriver->realfunc(p1, p2, p3);                                          \
  }                                                                                   \
  ret CONCAT(function, _renderdoc_hooked)(t1 p1, t2 p2, t3 p3)                        \
  {                                                                                   \
    SCOPED_GLCALL(glLock, function);                                                  \
    gl_CurChunk = GLChunk::function;                                                  \
    return m_GLDriver->realfunc(p1, p2, p3);                                          \
  }

#define HookAliasWrapper4(ret, function, realfunc, t1, p1, t2, p2, t3, p3, t4, p4)           \
  typedef ret (*CONCAT(function, _hooktype))(t1, t2, t3, t4);                                \
  extern "C" __attribute__((visibility("default"))) ret function(t1 p1, t2 p2, t3 p3, t4 p4) \
  {                                                                                          \
    SCOPED_GLCALL(glLock, function);                                                         \
    gl_CurChunk = GLChunk::function;                                                         \
    return m_GLDriver->realfunc(p1, p2, p3, p4);                                             \
  }                                                                                          \
  ret CONCAT(function, _renderdoc_hooked)(t1 p1, t2 p2, t3 p3, t4 p4)                        \
  {                                                                                          \
    SCOPED_GLCALL(glLock, function);                                                         \
    gl_CurChunk = GLChunk::function;                                                         \
    return m_GLDriver->realfunc(p1, p2, p3, p4);                                             \
  }

#define HookAliasWrapper5(ret, function, realfunc, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5)          \
  typedef ret (*CONCAT(function, _hooktype))(t1, t2, t3, t4, t5);                                   \
  extern "C" __attribute__((visibility("default"))) ret function(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5) \
  {                                                                                                 \
    SCOPED_GLCALL(glLock, function);                                                                \
    gl_CurChunk = GLChunk::function;                                                                \
    return m_GLDriver->realfunc(p1, p2, p3, p4, p5);                                                \
  }                                                                                                 \
  ret CONCAT(function, _renderdoc_hooked)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5)                        \
  {                                                                                                 \
    SCOPED_GLCALL(glLock, function);                                                                \
    gl_CurChunk = GLChunk::function;                                                                \
    return m_GLDriver->realfunc(p1, p2, p3, p4, p5);                                                \
  }

#define HookAliasWrapper6(ret, function, realfunc, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6) \
  typedef ret (*CONCAT(function, _hooktype))(t1, t2, t3, t4, t5, t6);                              \
  extern "C" __attribute__((visibility("default"))) ret function(t1 p1, t2 p2, t3 p3, t4 p4,       \
                                                                 t5 p5, t6 p6)                     \
  {                                                                                                \
    SCOPED_GLCALL(glLock, function);                                                               \
    gl_CurChunk = GLChunk::function;                                                               \
    return m_GLDriver->realfunc(p1, p2, p3, p4, p5, p6);                                           \
  }                                                                                                \
  ret CONCAT(function, _renderdoc_hooked)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6)                \
  {                                                                                                \
    SCOPED_GLCALL(glLock, function);                                                               \
    gl_CurChunk = GLChunk::function;                                                               \
    return m_GLDriver->realfunc(p1, p2, p3, p4, p5, p6);                                           \
  }

#define HookAliasWrapper7(ret, function, realfunc, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6, \
                          t7, p7)                                                                  \
  typedef ret (*CONCAT(function, _hooktype))(t1, t2, t3, t4, t5, t6, t7);                          \
  extern "C" __attribute__((visibility("default"))) ret function(t1 p1, t2 p2, t3 p3, t4 p4,       \
                                                                 t5 p5, t6 p6, t7 p7)              \
  {                                                                                                \
    SCOPED_GLCALL(glLock, function);                                                               \
    gl_CurChunk = GLChunk::function;                                                               \
    return m_GLDriver->realfunc(p1, p2, p3, p4, p5, p6, p7);                                       \
  }                                                                                                \
  ret CONCAT(function, _renderdoc_hooked)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7)         \
  {                                                                                                \
    SCOPED_GLCALL(glLock, function);                                                               \
    gl_CurChunk = GLChunk::function;                                                               \
    return m_GLDriver->realfunc(p1, p2, p3, p4, p5, p6, p7);                                       \
  }

#define HookAliasWrapper8(ret, function, realfunc, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6, \
                          t7, p7, t8, p8)                                                          \
  typedef ret (*CONCAT(function, _hooktype))(t1, t2, t3, t4, t5, t6, t7, t8);                      \
  extern "C" __attribute__((visibility("default"))) ret function(t1 p1, t2 p2, t3 p3, t4 p4,       \
                                                                 t5 p5, t6 p6, t7 p7, t8 p8)       \
  {                                                                                                \
    SCOPED_GLCALL(glLock, function);                                                               \
    gl_CurChunk = GLChunk::function;                                                               \
    return m_GLDriver->realfunc(p1, p2, p3, p4, p5, p6, p7, p8);                                   \
  }                                                                                                \
  ret CONCAT(function, _renderdoc_hooked)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8)  \
  {                                                                                                \
    SCOPED_GLCALL(glLock, function);                                                               \
    gl_CurChunk = GLChunk::function;                                                               \
    return m_GLDriver->realfunc(p1, p2, p3, p4, p5, p6, p7, p8);                                   \
  }

#define HookAliasWrapper9(ret, function, realfunc, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6, \
                          t7, p7, t8, p8, t9, p9)                                                  \
  typedef ret (*CONCAT(function, _hooktype))(t1, t2, t3, t4, t5, t6, t7, t8, t9);                  \
  extern "C" __attribute__((visibility("default"))) ret function(                                  \
      t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8, t9 p9)                               \
  {                                                                                                \
    SCOPED_GLCALL(glLock, function);                                                               \
    gl_CurChunk = GLChunk::function;                                                               \
    return m_GLDriver->realfunc(p1, p2, p3, p4, p5, p6, p7, p8, p9);                               \
  }                                                                                                \
  ret CONCAT(function, _renderdoc_hooked)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8,  \
                                          t9 p9)                                                   \
  {                                                                                                \
    SCOPED_GLCALL(glLock, function);                                                               \
    gl_CurChunk = GLChunk::function;                                                               \
    return m_GLDriver->realfunc(p1, p2, p3, p4, p5, p6, p7, p8, p9);                               \
  }

#define HookAliasWrapper10(ret, function, realfunc, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6,   \
                           p6, t7, p7, t8, p8, t9, p9, t10, p10)                                  \
  typedef ret (*CONCAT(function, _hooktype))(t1, t2, t3, t4, t5, t6, t7, t8, t9, t10);            \
  extern "C" __attribute__((visibility("default"))) ret function(                                 \
      t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8, t9 p9, t10 p10)                     \
  {                                                                                               \
    SCOPED_GLCALL(glLock, function);                                                              \
    gl_CurChunk = GLChunk::function;                                                              \
    return m_GLDriver->realfunc(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10);                         \
  }                                                                                               \
  ret CONCAT(function, _renderdoc_hooked)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8, \
                                          t9 p9, t10 p10)                                         \
  {                                                                                               \
    SCOPED_GLCALL(glLock, function);                                                              \
    gl_CurChunk = GLChunk::function;                                                              \
    return m_GLDriver->realfunc(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10);                         \
  }

#define HookAliasWrapper11(ret, function, realfunc, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6,   \
                           p6, t7, p7, t8, p8, t9, p9, t10, p10, t11, p11)                        \
  typedef ret (*CONCAT(function, _hooktype))(t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, t11);       \
  extern "C" __attribute__((visibility("default"))) ret function(                                 \
      t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8, t9 p9, t10 p10, t11 p11)            \
  {                                                                                               \
    SCOPED_GLCALL(glLock, function);                                                              \
    gl_CurChunk = GLChunk::function;                                                              \
    return m_GLDriver->realfunc(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11);                    \
  }                                                                                               \
  ret CONCAT(function, _renderdoc_hooked)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8, \
                                          t9 p9, t10 p10, t11 p11)                                \
  {                                                                                               \
    SCOPED_GLCALL(glLock, function);                                                              \
    gl_CurChunk = GLChunk::function;                                                              \
    return m_GLDriver->realfunc(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11);                    \
  }

#define HookAliasWrapper12(ret, function, realfunc, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6,   \
                           p6, t7, p7, t8, p8, t9, p9, t10, p10, t11, p11, t12, p12)              \
  typedef ret (*CONCAT(function, _hooktype))(t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, t11, t12);  \
  extern "C" __attribute__((visibility("default"))) ret function(                                 \
      t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8, t9 p9, t10 p10, t11 p11, t12 p12)   \
  {                                                                                               \
    SCOPED_GLCALL(glLock, function);                                                              \
    gl_CurChunk = GLChunk::function;                                                              \
    return m_GLDriver->realfunc(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12);               \
  }                                                                                               \
  ret CONCAT(function, _renderdoc_hooked)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8, \
                                          t9 p9, t10 p10, t11 p11, t12 p12)                       \
  {                                                                                               \
    SCOPED_GLCALL(glLock, function);                                                              \
    gl_CurChunk = GLChunk::function;                                                              \
    return m_GLDriver->realfunc(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12);               \
  }

#define HookAliasWrapper13(ret, function, realfunc, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6,   \
                           p6, t7, p7, t8, p8, t9, p9, t10, p10, t11, p11, t12, p12, t13, p13)    \
  typedef ret (*CONCAT(function, _hooktype))(t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, t11, t12,   \
                                             t13);                                                \
  extern "C" __attribute__((visibility("default"))) ret function(                                 \
      t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8, t9 p9, t10 p10, t11 p11, t12 p12,   \
      t13 p13)                                                                                    \
  {                                                                                               \
    SCOPED_GLCALL(glLock, function);                                                              \
    gl_CurChunk = GLChunk::function;                                                              \
    return m_GLDriver->realfunc(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13);          \
  }                                                                                               \
  ret CONCAT(function, _renderdoc_hooked)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8, \
                                          t9 p9, t10 p10, t11 p11, t12 p12, t13 p13)              \
  {                                                                                               \
    SCOPED_GLCALL(glLock, function);                                                              \
    gl_CurChunk = GLChunk::function;                                                              \
    return m_GLDriver->realfunc(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13);          \
  }

#define HookAliasWrapper14(ret, function, realfunc, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6,   \
                           p6, t7, p7, t8, p8, t9, p9, t10, p10, t11, p11, t12, p12, t13, p13,    \
                           t14, p14)                                                              \
  typedef ret (*CONCAT(function, _hooktype))(t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, t11, t12,   \
                                             t13, t14);                                           \
  extern "C" __attribute__((visibility("default"))) ret function(                                 \
      t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8, t9 p9, t10 p10, t11 p11, t12 p12,   \
      t13 p13, t14 p14)                                                                           \
  {                                                                                               \
    SCOPED_GLCALL(glLock, function);                                                              \
    gl_CurChunk = GLChunk::function;                                                              \
    return m_GLDriver->realfunc(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14);     \
  }                                                                                               \
  ret CONCAT(function, _renderdoc_hooked)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8, \
                                          t9 p9, t10 p10, t11 p11, t12 p12, t13 p13, t14 p14)     \
  {                                                                                               \
    SCOPED_GLCALL(glLock, function);                                                              \
    gl_CurChunk = GLChunk::function;                                                              \
    return m_GLDriver->realfunc(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14);     \
  }

#define HookAliasWrapper15(ret, function, realfunc, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6,    \
                           p6, t7, p7, t8, p8, t9, p9, t10, p10, t11, p11, t12, p12, t13, p13,     \
                           t14, p14, t15, p15)                                                     \
  typedef ret (*CONCAT(function, _hooktype))(t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, t11, t12,    \
                                             t13, t14, t15);                                       \
  extern "C" __attribute__((visibility("default"))) ret function(                                  \
      t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8, t9 p9, t10 p10, t11 p11, t12 p12,    \
      t13 p13, t14 p14, t15 p15)                                                                   \
  {                                                                                                \
    SCOPED_GLCALL(glLock, function);                                                               \
    gl_CurChunk = GLChunk::function;                                                               \
    return m_GLDriver->realfunc(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15); \
  }                                                                                                \
  ret CONCAT(function, _renderdoc_hooked)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8,  \
                                          t9 p9, t10 p10, t11 p11, t12 p12, t13 p13, t14 p14,      \
                                          t15 p15)                                                 \
  {                                                                                                \
    SCOPED_GLCALL(glLock, function);                                                               \
    gl_CurChunk = GLChunk::function;                                                               \
    return m_GLDriver->realfunc(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15); \
  }

#define HookAliasWrapper16(ret, function, realfunc, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6,   \
                           p6, t7, p7, t8, p8, t9, p9, t10, p10, t11, p11, t12, p12, t13, p13,    \
                           t14, p14, t15, p15, t16, p16)                                          \
  typedef ret (*CONCAT(function, _hooktype))(t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, t11, t12,   \
                                             t13, t14, t15, t16);                                 \
  extern "C" __attribute__((visibility("default"))) ret function(                                 \
      t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8, t9 p9, t10 p10, t11 p11, t12 p12,   \
      t13 p13, t14 p14, t15 p15, t16 p16)                                                         \
  {                                                                                               \
    SCOPED_GLCALL(glLock, function);                                                              \
    gl_CurChunk = GLChunk::function;                                                              \
    return m_GLDriver->realfunc(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, \
                                p16);                                                             \
  }                                                                                               \
  ret CONCAT(function, _renderdoc_hooked)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8, \
                                          t9 p9, t10 p10, t11 p11, t12 p12, t13 p13, t14 p14,     \
                                          t15 p15, t16 p16)                                       \
  {                                                                                               \
    SCOPED_GLCALL(glLock, function);                                                              \
    gl_CurChunk = GLChunk::function;                                                              \
    return m_GLDriver->realfunc(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, \
                                p16);                                                             \
  }

#define HookAliasWrapper17(ret, function, realfunc, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6,   \
                           p6, t7, p7, t8, p8, t9, p9, t10, p10, t11, p11, t12, p12, t13, p13,    \
                           t14, p14, t15, p15, t16, p16, t17, p17)                                \
  typedef ret (*CONCAT(function, _hooktype))(t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, t11, t12,   \
                                             t13, t14, t15, t16, t17);                            \
  extern "C" __attribute__((visibility("default"))) ret function(                                 \
      t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8, t9 p9, t10 p10, t11 p11, t12 p12,   \
      t13 p13, t14 p14, t15 p15, t16 p16, t17 p17)                                                \
  {                                                                                               \
    SCOPED_GLCALL(glLock, function);                                                              \
    gl_CurChunk = GLChunk::function;                                                              \
    return m_GLDriver->realfunc(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, \
                                p16, p17);                                                        \
  }                                                                                               \
  ret CONCAT(function, _renderdoc_hooked)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8, \
                                          t9 p9, t10 p10, t11 p11, t12 p12, t13 p13, t14 p14,     \
                                          t15 p15, t16 p16, t17 p17)                              \
  {                                                                                               \
    SCOPED_GLCALL(glLock, function);                                                              \
    gl_CurChunk = GLChunk::function;                                                              \
    return m_GLDriver->realfunc(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, \
                                p16, p17);                                                        \
  }

DefineDLLExportHooks();
DefineGLExtensionHooks();

/*
  in bash:

    function HookWrapper()
    {
        N=$1;
        echo "#undef HookWrapper$N";
        echo -n "#define HookWrapper$N(ret, function";
            for I in `seq 1 $N`; do echo -n ", t$I, p$I"; done;
        echo ") \\";

        echo -en "\ttypedef ret (*CONCAT(function, _hooktype)) (";
            for I in `seq 1 $N`; do echo -n "t$I"; if [ $I -ne $N ]; then echo -n ", "; fi; done;
        echo "); \\";

        echo -en "\tCONCAT(function, _hooktype) CONCAT(unsupported_real_,function) = NULL;";

        echo -en "\tret CONCAT(function,_renderdoc_hooked)(";
            for I in `seq 1 $N`; do echo -n "t$I p$I"; if [ $I -ne $N ]; then echo -n ", "; fi;
  done;
        echo ") \\";

        echo -e "\t{ \\";
        echo -e "\tstatic bool hit = false; if(hit == false) { RDCERR(\"Function \" \\";
        echo -e "\t\tSTRINGIZE(function)\" not supported - capture may be broken\");hit = true;}\\";
        echo -en "\treturn CONCAT(unsupported_real_,function)(";
            for I in `seq 1 $N`; do echo -n "p$I"; if [ $I -ne $N ]; then echo -n ", "; fi; done;
        echo -e "); \\";
        echo -e "\t}";
    }

  for I in `seq 0 17`; do HookWrapper $I; echo; done

  */

// don't want these definitions, the only place we'll use these is as parameter/variable names
#ifdef near
#undef near
#endif

#ifdef far
#undef far
#endif

// the _renderdoc_hooked variants are to make sure we always have a function symbol
// exported that we can return from glXGetProcAddress. If another library (or the app)
// creates a symbol called 'glEnable' we'll return the address of that, and break
// badly. Instead we leave the 'naked' versions for applications trying to import those
// symbols, and declare the _renderdoc_hooked for returning as a func pointer.

#undef HookWrapper0
#define HookWrapper0(ret, function)                                                     \
  typedef ret (*CONCAT(function, _hooktype))();                                         \
  CONCAT(function, _hooktype) CONCAT(unsupported_real_, function) = NULL;               \
  ret CONCAT(function, _renderdoc_hooked)()                                             \
  {                                                                                     \
    static bool hit = false;                                                            \
    if(hit == false)                                                                    \
    {                                                                                   \
      RDCERR("Function " STRINGIZE(function) " not supported - capture may be broken"); \
      hit = true;                                                                       \
    }                                                                                   \
    return CONCAT(unsupported_real_, function)();                                       \
  }

#undef HookWrapper1
#define HookWrapper1(ret, function, t1, p1)                                             \
  typedef ret (*CONCAT(function, _hooktype))(t1);                                       \
  CONCAT(function, _hooktype) CONCAT(unsupported_real_, function) = NULL;               \
  ret CONCAT(function, _renderdoc_hooked)(t1 p1)                                        \
  {                                                                                     \
    static bool hit = false;                                                            \
    if(hit == false)                                                                    \
    {                                                                                   \
      RDCERR("Function " STRINGIZE(function) " not supported - capture may be broken"); \
      hit = true;                                                                       \
    }                                                                                   \
    return CONCAT(unsupported_real_, function)(p1);                                     \
  }

#undef HookWrapper2
#define HookWrapper2(ret, function, t1, p1, t2, p2)                                     \
  typedef ret (*CONCAT(function, _hooktype))(t1, t2);                                   \
  CONCAT(function, _hooktype) CONCAT(unsupported_real_, function) = NULL;               \
  ret CONCAT(function, _renderdoc_hooked)(t1 p1, t2 p2)                                 \
  {                                                                                     \
    static bool hit = false;                                                            \
    if(hit == false)                                                                    \
    {                                                                                   \
      RDCERR("Function " STRINGIZE(function) " not supported - capture may be broken"); \
      hit = true;                                                                       \
    }                                                                                   \
    return CONCAT(unsupported_real_, function)(p1, p2);                                 \
  }

#undef HookWrapper3
#define HookWrapper3(ret, function, t1, p1, t2, p2, t3, p3)                             \
  typedef ret (*CONCAT(function, _hooktype))(t1, t2, t3);                               \
  CONCAT(function, _hooktype) CONCAT(unsupported_real_, function) = NULL;               \
  ret CONCAT(function, _renderdoc_hooked)(t1 p1, t2 p2, t3 p3)                          \
  {                                                                                     \
    static bool hit = false;                                                            \
    if(hit == false)                                                                    \
    {                                                                                   \
      RDCERR("Function " STRINGIZE(function) " not supported - capture may be broken"); \
      hit = true;                                                                       \
    }                                                                                   \
    return CONCAT(unsupported_real_, function)(p1, p2, p3);                             \
  }

#undef HookWrapper4
#define HookWrapper4(ret, function, t1, p1, t2, p2, t3, p3, t4, p4)                     \
  typedef ret (*CONCAT(function, _hooktype))(t1, t2, t3, t4);                           \
  CONCAT(function, _hooktype) CONCAT(unsupported_real_, function) = NULL;               \
  ret CONCAT(function, _renderdoc_hooked)(t1 p1, t2 p2, t3 p3, t4 p4)                   \
  {                                                                                     \
    static bool hit = false;                                                            \
    if(hit == false)                                                                    \
    {                                                                                   \
      RDCERR("Function " STRINGIZE(function) " not supported - capture may be broken"); \
      hit = true;                                                                       \
    }                                                                                   \
    return CONCAT(unsupported_real_, function)(p1, p2, p3, p4);                         \
  }

#undef HookWrapper5
#define HookWrapper5(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5)             \
  typedef ret (*CONCAT(function, _hooktype))(t1, t2, t3, t4, t5);                       \
  CONCAT(function, _hooktype) CONCAT(unsupported_real_, function) = NULL;               \
  ret CONCAT(function, _renderdoc_hooked)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5)            \
  {                                                                                     \
    static bool hit = false;                                                            \
    if(hit == false)                                                                    \
    {                                                                                   \
      RDCERR("Function " STRINGIZE(function) " not supported - capture may be broken"); \
      hit = true;                                                                       \
    }                                                                                   \
    return CONCAT(unsupported_real_, function)(p1, p2, p3, p4, p5);                     \
  }

#undef HookWrapper6
#define HookWrapper6(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6)     \
  typedef ret (*CONCAT(function, _hooktype))(t1, t2, t3, t4, t5, t6);                   \
  CONCAT(function, _hooktype) CONCAT(unsupported_real_, function) = NULL;               \
  ret CONCAT(function, _renderdoc_hooked)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6)     \
  {                                                                                     \
    static bool hit = false;                                                            \
    if(hit == false)                                                                    \
    {                                                                                   \
      RDCERR("Function " STRINGIZE(function) " not supported - capture may be broken"); \
      hit = true;                                                                       \
    }                                                                                   \
    return CONCAT(unsupported_real_, function)(p1, p2, p3, p4, p5, p6);                 \
  }

#undef HookWrapper7
#define HookWrapper7(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6, t7, p7) \
  typedef ret (*CONCAT(function, _hooktype))(t1, t2, t3, t4, t5, t6, t7);                   \
  CONCAT(function, _hooktype) CONCAT(unsupported_real_, function) = NULL;                   \
  ret CONCAT(function, _renderdoc_hooked)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7)  \
  {                                                                                         \
    static bool hit = false;                                                                \
    if(hit == false)                                                                        \
    {                                                                                       \
      RDCERR("Function " STRINGIZE(function) " not supported - capture may be broken");     \
      hit = true;                                                                           \
    }                                                                                       \
    return CONCAT(unsupported_real_, function)(p1, p2, p3, p4, p5, p6, p7);                 \
  }

#undef HookWrapper8
#define HookWrapper8(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6, t7, p7, t8, p8) \
  typedef ret (*CONCAT(function, _hooktype))(t1, t2, t3, t4, t5, t6, t7, t8);                       \
  CONCAT(function, _hooktype) CONCAT(unsupported_real_, function) = NULL;                           \
  ret CONCAT(function, _renderdoc_hooked)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8)   \
  {                                                                                                 \
    static bool hit = false;                                                                        \
    if(hit == false)                                                                                \
    {                                                                                               \
      RDCERR("Function " STRINGIZE(function) " not supported - capture may be broken");             \
      hit = true;                                                                                   \
    }                                                                                               \
    return CONCAT(unsupported_real_, function)(p1, p2, p3, p4, p5, p6, p7, p8);                     \
  }

#undef HookWrapper9
#define HookWrapper9(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6, t7, p7, t8,   \
                     p8, t9, p9)                                                                  \
  typedef ret (*CONCAT(function, _hooktype))(t1, t2, t3, t4, t5, t6, t7, t8, t9);                 \
  CONCAT(function, _hooktype) CONCAT(unsupported_real_, function) = NULL;                         \
  ret CONCAT(function, _renderdoc_hooked)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8, \
                                          t9 p9)                                                  \
  {                                                                                               \
    static bool hit = false;                                                                      \
    if(hit == false)                                                                              \
    {                                                                                             \
      RDCERR("Function " STRINGIZE(function) " not supported - capture may be broken");           \
      hit = true;                                                                                 \
    }                                                                                             \
    return CONCAT(unsupported_real_, function)(p1, p2, p3, p4, p5, p6, p7, p8, p9);               \
  }

#undef HookWrapper10
#define HookWrapper10(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6, t7, p7, t8,  \
                      p8, t9, p9, t10, p10)                                                       \
  typedef ret (*CONCAT(function, _hooktype))(t1, t2, t3, t4, t5, t6, t7, t8, t9, t10);            \
  CONCAT(function, _hooktype) CONCAT(unsupported_real_, function) = NULL;                         \
  ret CONCAT(function, _renderdoc_hooked)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8, \
                                          t9 p9, t10 p10)                                         \
  {                                                                                               \
    static bool hit = false;                                                                      \
    if(hit == false)                                                                              \
    {                                                                                             \
      RDCERR("Function " STRINGIZE(function) " not supported - capture may be broken");           \
      hit = true;                                                                                 \
    }                                                                                             \
    return CONCAT(unsupported_real_, function)(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10);          \
  }

#undef HookWrapper11
#define HookWrapper11(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6, t7, p7, t8,  \
                      p8, t9, p9, t10, p10, t11, p11)                                             \
  typedef ret (*CONCAT(function, _hooktype))(t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, t11);       \
  CONCAT(function, _hooktype) CONCAT(unsupported_real_, function) = NULL;                         \
  ret CONCAT(function, _renderdoc_hooked)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8, \
                                          t9 p9, t10 p10, t11 p11)                                \
  {                                                                                               \
    static bool hit = false;                                                                      \
    if(hit == false)                                                                              \
    {                                                                                             \
      RDCERR("Function " STRINGIZE(function) " not supported - capture may be broken");           \
      hit = true;                                                                                 \
    }                                                                                             \
    return CONCAT(unsupported_real_, function)(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11);     \
  }

#undef HookWrapper12
#define HookWrapper12(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6, t7, p7, t8,   \
                      p8, t9, p9, t10, p10, t11, p11, t12, p12)                                    \
  typedef ret (*CONCAT(function, _hooktype))(t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, t11, t12);   \
  CONCAT(function, _hooktype) CONCAT(unsupported_real_, function) = NULL;                          \
  ret CONCAT(function, _renderdoc_hooked)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8,  \
                                          t9 p9, t10 p10, t11 p11, t12 p12)                        \
  {                                                                                                \
    static bool hit = false;                                                                       \
    if(hit == false)                                                                               \
    {                                                                                              \
      RDCERR("Function " STRINGIZE(function) " not supported - capture may be broken");            \
      hit = true;                                                                                  \
    }                                                                                              \
    return CONCAT(unsupported_real_, function)(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12); \
  }

#undef HookWrapper13
#define HookWrapper13(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6, t7, p7, t8,  \
                      p8, t9, p9, t10, p10, t11, p11, t12, p12, t13, p13)                         \
  typedef ret (*CONCAT(function, _hooktype))(t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, t11, t12,   \
                                             t13);                                                \
  CONCAT(function, _hooktype) CONCAT(unsupported_real_, function) = NULL;                         \
  ret CONCAT(function, _renderdoc_hooked)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8, \
                                          t9 p9, t10 p10, t11 p11, t12 p12, t13 p13)              \
  {                                                                                               \
    static bool hit = false;                                                                      \
    if(hit == false)                                                                              \
    {                                                                                             \
      RDCERR("Function " STRINGIZE(function) " not supported - capture may be broken");           \
      hit = true;                                                                                 \
    }                                                                                             \
    return CONCAT(unsupported_real_, function)(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, \
                                               p13);                                              \
  }

#undef HookWrapper14
#define HookWrapper14(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6, t7, p7, t8,  \
                      p8, t9, p9, t10, p10, t11, p11, t12, p12, t13, p13, t14, p14)               \
  typedef ret (*CONCAT(function, _hooktype))(t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, t11, t12,   \
                                             t13, t14);                                           \
  CONCAT(function, _hooktype) CONCAT(unsupported_real_, function) = NULL;                         \
  ret CONCAT(function, _renderdoc_hooked)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8, \
                                          t9 p9, t10 p10, t11 p11, t12 p12, t13 p13, t14 p14)     \
  {                                                                                               \
    static bool hit = false;                                                                      \
    if(hit == false)                                                                              \
    {                                                                                             \
      RDCERR("Function " STRINGIZE(function) " not supported - capture may be broken");           \
      hit = true;                                                                                 \
    }                                                                                             \
    return CONCAT(unsupported_real_, function)(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, \
                                               p13, p14);                                         \
  }

#undef HookWrapper15
#define HookWrapper15(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6, t7, p7, t8,  \
                      p8, t9, p9, t10, p10, t11, p11, t12, p12, t13, p13, t14, p14, t15, p15)     \
  typedef ret (*CONCAT(function, _hooktype))(t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, t11, t12,   \
                                             t13, t14, t15);                                      \
  CONCAT(function, _hooktype) CONCAT(unsupported_real_, function) = NULL;                         \
  ret CONCAT(function, _renderdoc_hooked)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8, \
                                          t9 p9, t10 p10, t11 p11, t12 p12, t13 p13, t14 p14,     \
                                          t15 p15)                                                \
  {                                                                                               \
    static bool hit = false;                                                                      \
    if(hit == false)                                                                              \
    {                                                                                             \
      RDCERR("Function " STRINGIZE(function) " not supported - capture may be broken");           \
      hit = true;                                                                                 \
    }                                                                                             \
    return CONCAT(unsupported_real_, function)(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, \
                                               p13, p14, p15);                                    \
  }

#undef HookWrapper16
#define HookWrapper16(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6, t7, p7, t8,   \
                      p8, t9, p9, t10, p10, t11, p11, t12, p12, t13, p13, t14, p14, t15, p15, t16, \
                      p16)                                                                         \
  typedef ret (*CONCAT(function, _hooktype))(t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, t11, t12,    \
                                             t13, t14, t15, t16);                                  \
  CONCAT(function, _hooktype) CONCAT(unsupported_real_, function) = NULL;                          \
  ret CONCAT(function, _renderdoc_hooked)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8,  \
                                          t9 p9, t10 p10, t11 p11, t12 p12, t13 p13, t14 p14,      \
                                          t15 p15, t16 p16)                                        \
  {                                                                                                \
    static bool hit = false;                                                                       \
    if(hit == false)                                                                               \
    {                                                                                              \
      RDCERR("Function " STRINGIZE(function) " not supported - capture may be broken");            \
      hit = true;                                                                                  \
    }                                                                                              \
    return CONCAT(unsupported_real_, function)(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12,  \
                                               p13, p14, p15, p16);                                \
  }

#undef HookWrapper17
#define HookWrapper17(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6, t7, p7, t8,   \
                      p8, t9, p9, t10, p10, t11, p11, t12, p12, t13, p13, t14, p14, t15, p15, t16, \
                      p16, t17, p17)                                                               \
  typedef ret (*CONCAT(function, _hooktype))(t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, t11, t12,    \
                                             t13, t14, t15, t16, t17);                             \
  CONCAT(function, _hooktype) CONCAT(unsupported_real_, function) = NULL;                          \
  ret CONCAT(function, _renderdoc_hooked)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8,  \
                                          t9 p9, t10 p10, t11 p11, t12 p12, t13 p13, t14 p14,      \
                                          t15 p15, t16 p16, t17 p17)                               \
  {                                                                                                \
    static bool hit = false;                                                                       \
    if(hit == false)                                                                               \
    {                                                                                              \
      RDCERR("Function " STRINGIZE(function) " not supported - capture may be broken");            \
      hit = true;                                                                                  \
    }                                                                                              \
    return CONCAT(unsupported_real_, function)(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12,  \
                                               p13, p14, p15, p16, p17);                           \
  }

DefineUnsupportedDummies();

void *SharedLookupFuncPtr(const char *func, void *realFunc)
{
  DLLExportHooks();
  HookCheckGLExtensions();

  // at the moment the unsupported functions are all lowercase (as their name is generated from the
  // typedef name).
  string lowername = strlower(string(func));

  CheckUnsupported();

  // for any other function, if it's not a core or extension function we know about,
  // just return NULL
  return NULL;
}

bool SharedPopulateHooks(bool dlsymFirst, void *(*lookupFunc)(const char *))
{
#undef HookInit
#define HookInit(function)                                                                      \
  if(GL.function == NULL)                                                                       \
  {                                                                                             \
    PosixScopedSuppressHooking suppress;                                                        \
    if(dlsymFirst && GL.function == NULL)                                                       \
    {                                                                                           \
      GL.function =                                                                             \
          (CONCAT(function, _hooktype))PosixGetFunction(libGLdlsymHandle, STRINGIZE(function)); \
    }                                                                                           \
    lookupFunc((const char *)STRINGIZE(function));                                              \
  }

// cheeky
#undef HookExtension
#define HookExtension(funcPtrType, function)                                              \
  if(GL.function == NULL)                                                                 \
  {                                                                                       \
    PosixScopedSuppressHooking suppress;                                                  \
    if(dlsymFirst && GL.function == NULL)                                                 \
    {                                                                                     \
      GL.function = (funcPtrType)PosixGetFunction(libGLdlsymHandle, STRINGIZE(function)); \
    }                                                                                     \
    lookupFunc((const char *)STRINGIZE(function));                                        \
  }
#undef HookExtensionAlias
#define HookExtensionAlias(funcPtrType, function, alias)                               \
  if(GL.function == NULL)                                                              \
  {                                                                                    \
    PosixScopedSuppressHooking suppress;                                               \
    if(dlsymFirst && GL.function == NULL)                                              \
    {                                                                                  \
      GL.function = (funcPtrType)PosixGetFunction(libGLdlsymHandle, STRINGIZE(alias)); \
    }                                                                                  \
    lookupFunc((const char *)STRINGIZE(alias));                                        \
  }

  DLLExportHooks();
  HookCheckGLExtensions();


  return true;
}

void SharedCheckContext()
{
  CheckExtensions(GL);

  // see gl_emulated.cpp
  glEmulate::EmulateUnsupportedFunctions(&GL);

  glEmulate::EmulateRequiredExtensions(&GL);
}

void PosixHookFunctions()
{
#undef HookInit
#define HookInit(function) \
  PosixHookFunction(STRINGIZE(function), (void *)&CONCAT(function, _renderdoc_hooked))
#undef HookExtension
#define HookExtension(funcPtrType, function) \
  PosixHookFunction(STRINGIZE(function), (void *)&CONCAT(function, _renderdoc_hooked))
#undef HookExtensionAlias
#define HookExtensionAlias(funcPtrType, function, alias) \
  PosixHookFunction(STRINGIZE(alias), (void *)&CONCAT(function, _renderdoc_hooked))

  DLLExportHooks();
  HookCheckGLExtensions();
}