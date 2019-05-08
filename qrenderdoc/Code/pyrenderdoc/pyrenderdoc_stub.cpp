/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2017-2019 Baldur Karlsson
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

#include <Python.h>
#include <stdlib.h>
#include <algorithm>

#include "renderdoc_replay.h"

template <>
rdcstr DoStringise(const uint32_t &el)
{
  static char tmp[16];
  memset(tmp, 0, sizeof(tmp));
  snprintf(tmp, 15, "%u", el);
  return tmp;
}

#include "renderdoc_tostr.inl"

#include "pipestate.inl"

extern "C" PyThreadState *GetExecutingThreadState(PyObject *global_handle)
{
  return NULL;
}

extern "C" PyObject *GetCurrentGlobalHandle()
{
  return NULL;
}

extern "C" void HandleException(PyObject *global_handle)
{
}

extern "C" bool IsThreadBlocking(PyObject *global_handle)
{
  return false;
}

extern "C" void SetThreadBlocking(PyObject *global_handle, bool block)
{
}

REPLAY_PROGRAM_MARKER()