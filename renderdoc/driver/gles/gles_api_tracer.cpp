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

#include "gles_driver.h"
#include "gles_hookset_defs.h"
#include <iostream>


std::ostream& operator<<(std::ostream& io, GLboolean b)
{
  return io << (b ? "true" : "false");
}

std::ostream& operator<<(std::ostream& io, GLchar* c)
{
  return io << (void*)c;
}

static GLHookSet originalFunctions;
static bool debugAPI = false;

#define HookWrapper0(ret, function) \
  typedef ret (*CONCAT(function, _hooktype))();              \
  ret CONCAT(function, _debug_hooked)()                      \
  {                                                          \
    if (debugAPI) std::cout << #function << "()" << std::endl;                        \
    return originalFunctions.function();                     \
  }

#define HookWrapper1(ret, function, t1, p1)                             \
  typedef ret (*CONCAT(function, _hooktype))(t1);                       \
  ret CONCAT(function, _debug_hooked)(t1 p1)                            \
  {                                                                     \
    if (debugAPI) std::cout << #function << "(" << p1 <<")"  << std::endl;;                              \
    return originalFunctions.function(p1);                              \
  }

#define HookWrapper2(ret, function, t1, p1, t2, p2) \
  typedef ret (*CONCAT(function, _hooktype))(t1, t2);                   \
  ret CONCAT(function, _debug_hooked)(t1 p1, t2 p2)                     \
  {                                                                     \
    if (debugAPI) std::cout << #function << "(" << p1 << ", " << p2 << ")" << std::endl;                        \
    return originalFunctions.function(p1, p2);                          \
  }

#define HookWrapper3(ret, function, t1, p1, t2, p2, t3, p3) \
  typedef ret (*CONCAT(function, _hooktype))(t1, t2, t3);               \
  ret CONCAT(function, _debug_hooked)(t1 p1, t2 p2, t3 p3)              \
  {                                                                     \
    if (debugAPI) std::cout << #function << "(" << p1 << ", " << p2 << ", " << p3 << ")" << std::endl;                                             \
    return originalFunctions.function(p1, p2, p3);                          \
  }

#define HookWrapper4(ret, function, t1, p1, t2, p2, t3, p3, t4, p4) \
  typedef ret (*CONCAT(function, _hooktype))(t1, t2, t3, t4);               \
  ret CONCAT(function, _debug_hooked)(t1 p1, t2 p2, t3 p3, t4 p4)              \
  {                                                                     \
    if (debugAPI) std::cout << #function << "(" << p1 << ", " << p2 << ", " << p3 << ", " << p4 <<")" << std::endl;                                            \
    return originalFunctions.function(p1, p2, p3, p4);                  \
  }

#define HookWrapper5(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5) \
  typedef ret (*CONCAT(function, _hooktype))(t1, t2, t3, t4, t5);               \
  ret CONCAT(function, _debug_hooked)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5)              \
  {                                                                     \
    if (debugAPI) std::cout << #function << "(" << p1 << ", " << p2 << ", " << p3 << ", " << p4 << ", " << p5 <<")" << std::endl;                                            \
    return originalFunctions.function(p1, p2, p3, p4, p5);                  \
  }


#define HookWrapper6(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6) \
  typedef ret (*CONCAT(function, _hooktype))(t1, t2, t3, t4, t5, t6);               \
  ret CONCAT(function, _debug_hooked)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6)              \
  {                                                                     \
    if (debugAPI) std::cout << #function << "(" << p1 << ", " << p2 << ", " << p3 << ", " << p4 << ", " << p5 << ", " << p6 <<")" << std::endl;                                          \
    return originalFunctions.function(p1, p2, p3, p4, p5, p6);                  \
  }

#define HookWrapper7(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6, t7, p7) \
  typedef ret (*CONCAT(function, _hooktype))(t1, t2, t3, t4, t5, t6, t7);               \
  ret CONCAT(function, _debug_hooked)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7)              \
  {                                                                     \
    if (debugAPI) std::cout << #function << "(" << p1 << ", " << p2 << ", " << p3 << ", " << p4 << ", " << p5 << ", " << p6 << ", " << p7 <<")" << std::endl;                                            \
    return originalFunctions.function(p1, p2, p3, p4, p5, p6, p7);                  \
  }


#define HookWrapper8(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6, t7, p7, t8, p8) \
  typedef ret (*CONCAT(function, _hooktype))(t1, t2, t3, t4, t5, t6, t7, t8);               \
  ret CONCAT(function, _debug_hooked)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8)              \
  {                                                                     \
    if (debugAPI) std::cout << #function << "(" << p1 << ", " << p2 << ", " << p3 << ", " << p4 << ", " << p5 << ", " << p6 << ", " << p7 << ", " << p8 <<")" << std::endl;                                           \
    return originalFunctions.function(p1, p2, p3, p4, p5, p6, p7, p8);                  \
  }


#define HookWrapper9(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6, t7, p7, t8, p8, t9, p9) \
  typedef ret (*CONCAT(function, _hooktype))(t1, t2, t3, t4, t5, t6, t7, t8, t9);               \
  ret CONCAT(function, _debug_hooked)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8, t9 p9)              \
  {                                                                     \
    if (debugAPI) std::cout << #function << "(" << p1 << ", " << p2 << ", " << p3 << ", " << p4 << ", " << p5 << ", " << p6 << ", " << p7 << ", " << p8 << ", " << p9 <<")" << std::endl;                                            \
    return originalFunctions.function(p1, p2, p3, p4, p5, p6, p7, p8, p9);                  \
  }

#define HookWrapper10(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6, t7, p7, t8, p8, t9, p9, t10, p10) \
  typedef ret (*CONCAT(function, _hooktype))(t1, t2, t3, t4, t5, t6, t7, t8, t9, t10);               \
  ret CONCAT(function, _debug_hooked)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8, t9 p9, t10 p10)              \
  {                                                                     \
    if (debugAPI) std::cout << #function << "(" << p1 << ", " << p2 << ", " << p3 << ", " << p4 << ", " << p5 << ", " << p6 << ", " << p7 << ", " << p8 << ", " << p9 << ", " << p10 <<")" << std::endl;                                        \
    return originalFunctions.function(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10);                  \
  }

#define HookWrapper11(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6, t7, p7, t8, p8, t9, p9, t10, p10, t11, p11) \
  typedef ret (*CONCAT(function, _hooktype))(t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, t11);               \
  ret CONCAT(function, _debug_hooked)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8, t9 p9, t10 p10, t11 p11)              \
  {                                                                     \
    if (debugAPI) std::cout << #function << "(" << p1 << ", " << p2 << ", " << p3 << ", " << p4 << ", " << p5 << ", " << p6 << ", " << p7 << ", " << p8 << ", " << p9 << ", " << p10 << ", " << p11 <<")" << std::endl;                                    \
    return originalFunctions.function(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11);                  \
  }

#define HookWrapper12(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6, t7, p7, t8, p8, t9, p9, t10, p10, t11, p11, t12, p12) \
  typedef ret (*CONCAT(function, _hooktype))(t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, t11, t12);               \
  ret CONCAT(function, _debug_hooked)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8, t9 p9, t10 p10, t11 p11, t12 p12)              \
  {                                                                     \
    if (debugAPI) std::cout << #function << "(" << p1 << ", " << p2 << ", " << p3 << ", " << p4 << ", " << p5 << ", " << p6 << ", " << p7 << ", " << p8 << ", " << p9 << ", " << p10 << ", " << p11 << ", " << p12 <<")" << std::endl;                                    \
    return originalFunctions.function(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12);                  \
  }

#define HookWrapper13(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6, t7, p7, t8, p8, t9, p9, t10, p10, t11, p11, t12, p12, t13, p13) \
  typedef ret (*CONCAT(function, _hooktype))(t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, t11, t12, t13);               \
  ret CONCAT(function, _debug_hooked)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8, t9 p9, t10 p10, t11 p11, t12 p12, t13 p13)              \
  {                                                                     \
    if (debugAPI) std::cout << #function << "(" << p1 << ", " << p2 << ", " << p3 << ", " << p4 << ", " << p5 << ", " << p6 << ", " << p7 << ", " << p8 << ", " << p9 << ", " << p10 << ", " << p11 << ", " << p12 << ", " << p13 <<")" << std::endl;                                    \
    return originalFunctions.function(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13);                  \
  }

#define HookWrapper14(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6, t7, p7, t8, p8, t9, p9, t10, p10, t11, p11, t12, p12, t13, p13, t14, p14) \
  typedef ret (*CONCAT(function, _hooktype))(t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, t11, t12, t13, t14);               \
  ret CONCAT(function, _debug_hooked)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8, t9 p9, t10 p10, t11 p11, t12 p12, t13 p13, t14 p14)              \
  {                                                                     \
    if (debugAPI) std::cout << #function << "(" << p1 << ", " << p2 << ", " << p3 << ", " << p4 << ", " << p5 << ", " << p6 << ", " << p7 << ", " << p8 << ", " << p9 << ", " << p10 << ", " << p11 << ", " << p12 << ", " << p13 << ", " << p14 <<")" << std::endl;                                    \
    return originalFunctions.function(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14);                  \
  }


#define HookWrapper15(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6, t7, p7, t8, p8, t9, p9, t10, p10, t11, p11, t12, p12, t13, p13, t14, p14, t15, p15) \
  typedef ret (*CONCAT(function, _hooktype))(t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, t11, t12, t13, t14, t15);               \
  ret CONCAT(function, _debug_hooked)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8, t9 p9, t10 p10, t11 p11, t12 p12, t13 p13, t14 p14, t15 p15)              \
  {                                                                     \
    if (debugAPI) std::cout << #function << "(" << p1 << ", " << p2 << ", " << p3 << ", " << p4 << ", " << p5 << ", " << p6 << ", " << p7 << ", " << p8 << ", " << p9 << ", " << p10 << ", " << p11 << ", " << p12 << ", " << p13 << ", " << p14 << ", " << p15 <<")" << std::endl;                                    \
    return originalFunctions.function(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15);                  \
  }

DefineDLLExportHooks();
DefineGLExtensionHooks();
DefineUnsupportedDummies();

void WrappedGLES::enableAPIDebug(bool enable)
{
  debugAPI = enable;
}

GLHookSet WrappedGLES::initRealWrapper(const GLHookSet& hooks)
{
  originalFunctions = hooks;
  GLHookSet wrapper;
  memset(&wrapper, 0, sizeof(wrapper));

  #define HookInit(function) \
    if (wrapper.function == NULL) \
      wrapper.function = CONCAT(function, _debug_hooked)

  #define HandleUnsupported(funcPtrType, function) \
    if (wrapper.function == NULL) \
      wrapper.function = CONCAT(function, _debug_hooked)

  #define HookExtension(funcPtrType, function) \
    if (wrapper.function == NULL) \
      wrapper.function = CONCAT(function, _debug_hooked)

  DLLExportHooks();
  HookCheckGLExtensions();
  CheckUnsupported();

  return wrapper;
}

