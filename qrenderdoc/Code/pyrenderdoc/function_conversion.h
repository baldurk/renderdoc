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

#pragma once

#include <frameobject.h>

// this is defined elsewhere for managing the opaque global_handle object
extern "C" PyThreadState *GetExecutingThreadState(PyObject *global_handle);
extern "C" PyObject *GetCurrentGlobalHandle();
extern "C" void HandleException(PyObject *global_handle);
extern "C" bool IsThreadBlocking(PyObject *global_handle);
extern "C" void SetThreadBlocking(PyObject *global_handle, bool block);

struct ExceptionHandling
{
  bool failFlag = false;
  PyObject *exObj = NULL;
  PyObject *valueObj = NULL;
  PyObject *tracebackObj = NULL;
};

// this function handles failures in callback functions. If we're synchronously calling the callback
// from within an execute scope, then we can assign to failflag and let the error propagate upwards.
// If we're not, then the callback is being executed on another thread with no knowledge of python,
// so we need to use the global handle to try and emit the exception through the context. None of
// this is multi-threaded because we're inside the GIL at all times
inline void HandleCallbackFailure(PyObject *global_handle, ExceptionHandling &exHandle)
{
  // if there's no global handle assume we are not running in the usual environment, so there are no
  // external-to-python threads.
  // Specifically this is when we're imported as a module directly into python with none of our
  // harness, so this is running as pure glue code.
  if(!global_handle)
  {
    exHandle.failFlag = true;
    return;
  }

  PyThreadState *current = PyGILState_GetThisThreadState();
  PyThreadState *executing = GetExecutingThreadState(global_handle);

  // we are executing synchronously, set the flag and return
  if(current == executing)
  {
    exHandle.failFlag = true;
    return;
  }

  // if we have the blocking flag set, then we may be on another thread but we can still propagate
  // up the error
  if(IsThreadBlocking(global_handle))
  {
    exHandle.failFlag = true;

    // we need to rethrow the exception to that thread, so fetch (and clear it) on this thread.
    //
    // Note that the exception can only propagate up to one place. However since we know that python
    // is inherently single threaded, so if we're doing this blocking funciton call on another
    // thread then we *know* there isn't python further up the stack. Therefore we're safe to
    // swallow the exception here (since there's nowhere for it to bubble up to anyway) and rethrow
    // on the python thread.
    PyErr_Fetch(&exHandle.exObj, &exHandle.valueObj, &exHandle.tracebackObj);

    return;
  }

  // in this case we are executing asynchronously, and must handle the exception manually as there's
  // nothing above us that knows about python exceptions
  HandleException(global_handle);
}

template <typename T>
inline T get_return(const char *funcname, PyObject *result, PyObject *global_handle,
                    ExceptionHandling &exHandle)
{
  T val = T();

  int res = ConvertFromPy(result, val);

  if(!SWIG_IsOK(res))
  {
    HandleCallbackFailure(global_handle, exHandle);

    PyErr_Format(PyExc_TypeError, "Unexpected type for return value of callback in %s", funcname);
  }

  Py_XDECREF(result);

  return val;
}

template <>
inline void get_return(const char *funcname, PyObject *result, PyObject *global_handle,
                       ExceptionHandling &exHandle)
{
  Py_XDECREF(result);
}

struct PyObjectRefCounter
{
  PyObjectRefCounter(PyObject *o) : obj(o) { Py_INCREF(obj); }
  PyObjectRefCounter(const PyObjectRefCounter &o)
  {
    obj = o.obj;
    Py_INCREF(obj);
  }
  ~PyObjectRefCounter()
  {
// in non-release, check that we're currently executing if we're about to delete the object.
#if !defined(RELEASE)
    if(obj->ob_refcnt == 1 && PyGILState_Check() == 0)
    {
      RENDERDOC_LogMessage(LogType::Error, "QTRD", __FILE__, __LINE__,
                           "Deleting PyObjectRefCounter without python executing on this thread");
      // return and leak the object rather than crashing
      return;
    }
#endif
    Py_DECREF(obj);
  }
  PyObject *obj;
};

template <typename rettype, typename... paramTypes>
struct varfunc
{
  varfunc(const char *funcname, paramTypes... params)
  {
    args = PyTuple_New(sizeof...(paramTypes));

    currentarg = 0;

    // avoid unused parameter errors when calling a parameter-less function
    (void)funcname;

    using expand_type = int[];
    (void)expand_type{0, (push_arg(funcname, params), 0)...};
  }

  template <typename T>
  void push_arg(const char *funcname, const T &arg)
  {
    if(!args)
      return;

    PyObject *obj = ConvertToPy(arg);

    if(!obj)
    {
      Py_DecRef(args);
      args = NULL;

      PyErr_Format(PyExc_TypeError, "Unexpected type for arg %d of callback in %s", currentarg + 1,
                   funcname);

      return;
    }

    PyTuple_SetItem(args, currentarg++, obj);
  }

  ~varfunc() { Py_XDECREF(args); }
  rettype call(const char *funcname, PyObject *func, PyObject *global_handle,
               ExceptionHandling &exHandle)
  {
    if(!func || !PyCallable_Check(func) || !args)
    {
      HandleCallbackFailure(global_handle, exHandle);
      return rettype();
    }

    PyObject *result = PyObject_Call(func, args, 0);

    if(result == NULL)
      HandleCallbackFailure(global_handle, exHandle);

    Py_DECREF(args);

    return get_return<rettype>(funcname, result, global_handle, exHandle);
  }

  int currentarg = 0;
  PyObject *args;
};

struct ScopedFuncCall
{
  ScopedFuncCall(PyObject *h)
  {
    handle = h;
    Py_XINCREF(handle);
    gil = PyGILState_Ensure();
  }

  ~ScopedFuncCall()
  {
    Py_XDECREF(handle);
    PyGILState_Release(gil);
  }

  PyObject *handle;
  PyGILState_STATE gil;
};

template <typename funcType>
funcType ConvertFunc(const char *funcname, PyObject *func, ExceptionHandling &exHandle)
{
  // allow None to indicate no callback
  if(func == Py_None)
    return funcType();

  // add a reference to the global object so it stays alive while we execute, in case this is an
  // async call
  PyObject *global_internal_handle = NULL;

  // walk the frames until we find one with _renderdoc_internal. If we call a function in another
  // module the globals may not have the entry, but the root level is expected to.
  {
    _frame *frame = PyEval_GetFrame();

    while(frame)
    {
      global_internal_handle = PyDict_GetItemString(frame->f_globals, "_renderdoc_internal");

      if(global_internal_handle)
        break;
      frame = frame->f_back;
    }
  }

  if(!global_internal_handle)
    global_internal_handle = GetCurrentGlobalHandle();

  // create a copy that will keep the function object alive as long as the lambda is
  PyObjectRefCounter funcptr(func);

  return [global_internal_handle, funcname, funcptr, &exHandle](auto... param) {
    ScopedFuncCall gil(global_internal_handle);

    varfunc<typename funcType::result_type, decltype(param)...> f(funcname, param...);
    return f.call(funcname, funcptr.obj, global_internal_handle, exHandle);
  };
}
