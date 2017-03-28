/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2017 Baldur Karlsson
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

#include <type_traits>

// struct to allow partial specialisation for enums
template <typename T, bool isEnum = std::is_enum<T>::value>
struct TypeConversion
{
  static swig_type_info *GetTypeInfo()
  {
    static swig_type_info *cached_type_info = NULL;

    if(cached_type_info)
      return cached_type_info;

    std::string baseTypeName = TypeName<T>();
    baseTypeName += " *";
    cached_type_info = SWIG_TypeQuery(baseTypeName.c_str());

    return cached_type_info;
  }

  static int ConvertFromPy(PyObject *in, T &out)
  {
    swig_type_info *type_info = GetTypeInfo();
    if(type_info == NULL)
      return SWIG_ERROR;

    T *ptr = NULL;
    int res = SWIG_ConvertPtr(in, (void **)&ptr, type_info, 0);
    if(SWIG_IsOK(res))
      out = *ptr;

    return res;
  }

  static PyObject *ConvertToPy(PyObject *self, const T &in)
  {
    swig_type_info *type_info = GetTypeInfo();
    if(type_info == NULL)
      return NULL;

    T *pyCopy = new T(in);
    return SWIG_NewPointerObj((void *)pyCopy, type_info, SWIG_BUILTIN_INIT);
  }
};

// specialisations for pointer types (opaque handles to be moved not copied)
template <typename Opaque>
struct TypeConversion<Opaque *, false>
{
  static swig_type_info *GetTypeInfo()
  {
    static swig_type_info *cached_type_info = NULL;

    if(cached_type_info)
      return cached_type_info;

    std::string baseTypeName = TypeName<Opaque>();
    baseTypeName += " *";
    cached_type_info = SWIG_TypeQuery(baseTypeName.c_str());

    return cached_type_info;
  }

  static int ConvertFromPy(PyObject *in, Opaque *&out)
  {
    swig_type_info *type_info = GetTypeInfo();
    if(type_info == NULL)
      return SWIG_ERROR;

    Opaque *ptr = NULL;
    int res = SWIG_ConvertPtr(in, (void **)&ptr, type_info, 0);
    if(SWIG_IsOK(res))
      out = ptr;

    return res;
  }

  static PyObject *ConvertToPy(PyObject *self, const Opaque *&in)
  {
    swig_type_info *type_info = GetTypeInfo();
    if(type_info == NULL)
      return NULL;

    return SWIG_InternalNewPointerObj((void *)in, type_info, 0);
  }

  static PyObject *ConvertToPy(PyObject *self, Opaque *in)
  {
    return ConvertToPy(self, (const Opaque *&)in);
  }
};

// specialisations for basic types
template <>
struct TypeConversion<uint8_t, false>
{
  static int ConvertFromPy(PyObject *in, uint8_t &out)
  {
    if(!PyLong_Check(in))
      return SWIG_TypeError;

    uint32_t longval = PyLong_AsUnsignedLong(in);

    if(PyErr_Occurred() || longval > 0xff)
      return SWIG_OverflowError;

    out = uint8_t(longval & 0xff);

    return SWIG_OK;
  }

  static PyObject *ConvertToPy(PyObject *self, const uint8_t &in)
  {
    return PyLong_FromUnsignedLong(in);
  }
};

template <>
struct TypeConversion<uint32_t, false>
{
  static int ConvertFromPy(PyObject *in, uint32_t &out)
  {
    if(!PyLong_Check(in))
      return SWIG_TypeError;

    out = PyLong_AsUnsignedLong(in);

    if(PyErr_Occurred())
      return SWIG_OverflowError;

    return SWIG_OK;
  }

  static PyObject *ConvertToPy(PyObject *self, const uint32_t &in)
  {
    return PyLong_FromUnsignedLong(in);
  }
};

template <>
struct TypeConversion<int32_t, false>
{
  static int ConvertFromPy(PyObject *in, int32_t &out)
  {
    if(!PyLong_Check(in))
      return SWIG_TypeError;

    out = PyLong_AsLong(in);

    if(PyErr_Occurred())
      return SWIG_OverflowError;

    return SWIG_OK;
  }

  static PyObject *ConvertToPy(PyObject *self, const int32_t &in) { return PyLong_FromLong(in); }
};

template <>
struct TypeConversion<uint64_t, false>
{
  static int ConvertFromPy(PyObject *in, uint64_t &out)
  {
    if(!PyLong_Check(in))
      return SWIG_TypeError;

    out = PyLong_AsUnsignedLongLong(in);

    if(PyErr_Occurred())
      return SWIG_OverflowError;

    return SWIG_OK;
  }

  static PyObject *ConvertToPy(PyObject *self, const uint64_t &in)
  {
    return PyLong_FromUnsignedLongLong(in);
  }
};

template <>
struct TypeConversion<float, false>
{
  static int ConvertFromPy(PyObject *in, float &out)
  {
    if(!PyFloat_Check(in))
      return SWIG_TypeError;

    out = (float)PyFloat_AsDouble(in);

    if(PyErr_Occurred())
      return SWIG_OverflowError;

    return SWIG_OK;
  }

  static PyObject *ConvertToPy(PyObject *self, const float &in) { return PyFloat_FromDouble(in); }
};

template <>
struct TypeConversion<double, false>
{
  static int ConvertFromPy(PyObject *in, double &out)
  {
    if(!PyFloat_Check(in))
      return SWIG_TypeError;

    out = PyFloat_AsDouble(in);

    if(PyErr_Occurred())
      return SWIG_OverflowError;

    return SWIG_OK;
  }

  static PyObject *ConvertToPy(PyObject *self, const double &in) { return PyFloat_FromDouble(in); }
};

// partial specialisation for enums, we just convert as their underlying type,
// whatever integer size that happens to be
template <typename T>
struct TypeConversion<T, true>
{
  typedef typename std::underlying_type<T>::type etype;

  static int ConvertFromPy(PyObject *in, T &out)
  {
    etype int_out = 0;
    int ret = TypeConversion<etype>::ConvertFromPy(in, int_out);
    out = T(int_out);
    return ret;
  }

  static PyObject *ConvertToPy(PyObject *self, const T &in)
  {
    return TypeConversion<etype>::ConvertToPy(self, etype(in));
  }
};

// specialisation for pair
template <typename A, typename B>
struct TypeConversion<rdctype::pair<A, B>, false>
{
  static int ConvertFromPy(PyObject *in, rdctype::pair<A, B> &out)
  {
    if(!PyTuple_Check(in))
      return SWIG_TypeError;

    Py_ssize_t size = PyTuple_Size(in);

    if(size != 2)
      return SWIG_TypeError;

    int ret = TypeConversion<A>::ConvertFromPy(PyTuple_GetItem(in, 0), out.first);
    if(SWIG_IsOK(ret))
      ret = TypeConversion<B>::ConvertFromPy(PyTuple_GetItem(in, 1), out.second);

    return ret;
  }

  static PyObject *ConvertToPy(PyObject *self, const rdctype::pair<A, B> &in)
  {
    PyObject *first = TypeConversion<A>::ConvertToPy(self, in.first);
    if(!first)
      return NULL;

    PyObject *second = TypeConversion<B>::ConvertToPy(self, in.second);
    if(!second)
      return NULL;

    PyObject *ret = PyTuple_New(2);
    if(!ret)
      return NULL;

    PyTuple_SetItem(ret, 0, first);
    PyTuple_SetItem(ret, 1, second);

    return ret;
  }
};

// specialisation for array
template <typename U>
struct TypeConversion<rdctype::array<U>, false>
{
  // we add some extra parameters so the typemaps for array can use these to get
  // nicer failure error messages out with the index that failed
  static int ConvertFromPy(PyObject *in, rdctype::array<U> &out, int *failIdx)
  {
    if(!PyList_Check(in))
      return SWIG_TypeError;

    out.create((int)PyList_Size(in));

    for(int i = 0; i < out.count; i++)
    {
      int ret = TypeConversion<U>::ConvertFromPy(PyList_GetItem(in, i), out.elems[i]);
      if(!SWIG_IsOK(ret))
      {
        if(failIdx)
          *failIdx = i;
        return ret;
      }
    }

    return SWIG_OK;
  }

  static int ConvertFromPy(PyObject *in, rdctype::array<U> &out)
  {
    return ConvertFromPy(in, out, NULL);
  }
  static PyObject *ConvertToPyInPlace(PyObject *self, PyObject *list, const rdctype::array<U> &in,
                                      int *failIdx)
  {
    for(int i = 0; i < in.count; i++)
    {
      PyObject *elem = TypeConversion<U>::ConvertToPy(self, in.elems[i]);

      if(elem)
      {
        PyList_Append(list, elem);
      }
      else
      {
        if(failIdx)
          *failIdx = i;

        return NULL;
      }
    }

    return list;
  }

  static PyObject *ConvertToPy(PyObject *self, const rdctype::array<U> &in, int *failIdx)
  {
    PyObject *list = PyList_New(0);
    if(!list)
      return NULL;

    PyObject *ret = ConvertToPyInPlace(self, list, in, failIdx);

    // if a failure happened, don't leak the list we created
    if(!ret)
      Py_XDECREF(list);

    return ret;
  }

  static PyObject *ConvertToPy(PyObject *self, const rdctype::array<U> &in)
  {
    return ConvertToPy(self, in, NULL);
  }
};

// specialisation for string
template <>
struct TypeConversion<rdctype::str, false>
{
  static swig_type_info *GetTypeInfo()
  {
    static swig_type_info *cached_type_info = NULL;

    if(cached_type_info)
      return cached_type_info;

    cached_type_info = SWIG_TypeQuery("rdctype::str *");

    return cached_type_info;
  }

  static int ConvertFromPy(PyObject *in, rdctype::str &out)
  {
    if(PyUnicode_Check(in))
    {
      PyObject *bytes = PyUnicode_AsUTF8String(in);

      if(!bytes)
        return SWIG_ERROR;

      char *buf = NULL;
      Py_ssize_t size = 0;

      int ret = PyBytes_AsStringAndSize(bytes, &buf, &size);

      if(ret == 0)
      {
        out.count = (int)size - 1;
        out.elems = (char *)out.allocate(size);
        memcpy(out.elems, buf, size - 1);
        out.elems[size] = 0;

        Py_DecRef(bytes);

        return SWIG_OK;
      }

      Py_DecRef(bytes);

      return SWIG_ERROR;
    }

    swig_type_info *type_info = GetTypeInfo();
    if(!type_info)
      return SWIG_ERROR;

    rdctype::str *ptr = NULL;
    int res = SWIG_ConvertPtr(in, (void **)&ptr, type_info, 0);
    if(SWIG_IsOK(res))
      out = *ptr;

    return res;
  }

  static PyObject *ConvertToPy(PyObject *self, const rdctype::str &in)
  {
    return PyUnicode_FromStringAndSize(in.elems, in.count);
  }
};
// free functions forward to struct
template <typename T>
int ConvertFromPy(PyObject *in, T &out)
{
  return TypeConversion<T>::ConvertFromPy(in, out);
}

template <typename T>
PyObject *ConvertToPy(PyObject *self, const T &in)
{
  return TypeConversion<T>::ConvertToPy(self, in);
}

// See renderdoc.i for implementation & explanation
extern "C" void HandleCallbackFailure(PyObject *global_handle, bool &failflag);

template <typename T>
inline T get_return(const char *funcname, PyObject *result, PyObject *global_handle, bool &failflag)
{
  T val = T();

  int res = ConvertToPy(result, val);

  if(!SWIG_IsOK(res))
  {
    HandleCallbackFailure(global_handle, failflag);

    PyErr_Format(PyExc_TypeError, "Expected a '%s' for return value of callback in %s",
                 TypeName<T>(), funcname);
  }

  Py_XDECREF(result);

  return val;
}

template <>
inline void get_return(const char *funcname, PyObject *result, PyObject *global_handle, bool &failflag)
{
  Py_XDECREF(result);
}

template <typename rettype, typename... paramTypes>
struct varfunc
{
  varfunc(PyObject *self, const char *funcname, paramTypes... params)
  {
    args = PyTuple_New(sizeof...(paramTypes));

    currentarg = 0;

    using expand_type = int[];
    (void)expand_type{0, (push_arg(self, funcname, params), 0)...};
  }

  template <typename T>
  void push_arg(PyObject *self, const char *funcname, const T &arg)
  {
    if(!args)
      return;

    PyObject *obj = ConvertToPy(self, arg);

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
  rettype call(const char *funcname, PyObject *func, PyObject *global_handle, bool &failflag)
  {
    if(!func || func == Py_None || !PyCallable_Check(func) || !args)
    {
      HandleCallbackFailure(global_handle, failflag);
      return rettype();
    }

    PyObject *result = PyObject_Call(func, args, 0);

    if(result == NULL)
      HandleCallbackFailure(global_handle, failflag);

    Py_DECREF(args);

    return get_return<rettype>(funcname, result, global_handle, failflag);
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
funcType ConvertFunc(PyObject *self, const char *funcname, PyObject *func, bool &failflag)
{
  // add a reference to the global object so it stays alive while we execute, in case this is an
  // async call
  PyObject *global_internal_handle = NULL;

  PyObject *globals = PyEval_GetGlobals();
  if(globals)
    global_internal_handle = PyDict_GetItemString(globals, "_renderdoc_internal");

  return [global_internal_handle, self, funcname, func, &failflag](auto... param) {
    ScopedFuncCall gil(global_internal_handle);

    varfunc<typename funcType::result_type, decltype(param)...> f(self, funcname, param...);
    return f.call(funcname, func, global_internal_handle, failflag);
  };
}
