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

#include <algorithm>
#include <map>
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

    rdcstr baseTypeName = TypeName<T>();
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

  static PyObject *ConvertToPy(const T &in)
  {
    swig_type_info *type_info = GetTypeInfo();
    if(type_info == NULL)
      return NULL;

    T *pyCopy = new T(in);
    return SWIG_InternalNewPointerObj((void *)pyCopy, type_info, SWIG_POINTER_OWN);
  }
};

// specialisations for Python object that just increments the refcount. Only useful if we're doing
// something quite special and manually converting outside from some type we don't want to expose to
// python (this is used for QVariant conversion in python callback arguments).
template <>
struct TypeConversion<PyObject *, false>
{
  static int ConvertFromPy(PyObject *in, PyObject *&out)
  {
    out = in;
    Py_XINCREF(out);

    return 0;
  }

  static PyObject *ConvertToPy(PyObject *in)
  {
    Py_XINCREF(in);
    return in;
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

    rdcstr baseTypeName = TypeName<Opaque>();
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

  static PyObject *ConvertToPy(const Opaque *&in)
  {
    swig_type_info *type_info = GetTypeInfo();
    if(type_info == NULL)
      return NULL;

    return SWIG_InternalNewPointerObj((void *)in, type_info, 0);
  }

  static PyObject *ConvertToPy(Opaque *in) { return ConvertToPy((const Opaque *&)in); }
};

// specialisations for basic types
template <>
struct TypeConversion<bool, false>
{
  static int ConvertFromPy(PyObject *in, bool &out)
  {
    if(!PyBool_Check(in))
      return SWIG_TypeError;

    if(in == Py_True)
      out = true;
    else
      out = false;

    return SWIG_OK;
  }

  static PyObject *ConvertToPy(const bool &in)
  {
    if(in)
    {
      Py_IncRef(Py_True);
      return Py_True;
    }
    else
    {
      Py_IncRef(Py_False);
      return Py_False;
    }
  }
};

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

  static PyObject *ConvertToPy(const uint8_t &in) { return PyLong_FromUnsignedLong(in); }
};

template <>
struct TypeConversion<int8_t, false>
{
  static int ConvertFromPy(PyObject *in, int8_t &out)
  {
    if(!PyLong_Check(in))
      return SWIG_TypeError;

    uint32_t longval = PyLong_AsUnsignedLong(in);

    if(PyErr_Occurred() || longval > 0xff)
      return SWIG_OverflowError;

    out = int8_t(longval & 0xff);

    return SWIG_OK;
  }

  static PyObject *ConvertToPy(const int8_t &in) { return PyLong_FromLong(in); }
};

template <>
struct TypeConversion<uint16_t, false>
{
  static int ConvertFromPy(PyObject *in, uint16_t &out)
  {
    if(!PyLong_Check(in))
      return SWIG_TypeError;

    uint32_t longval = PyLong_AsUnsignedLong(in);

    if(PyErr_Occurred() || longval > 0xffff)
      return SWIG_OverflowError;

    out = uint16_t(longval & 0xff);

    return SWIG_OK;
  }

  static PyObject *ConvertToPy(const uint16_t &in) { return PyLong_FromUnsignedLong(in); }
};

template <>
struct TypeConversion<int16_t, false>
{
  static int ConvertFromPy(PyObject *in, int16_t &out)
  {
    if(!PyLong_Check(in))
      return SWIG_TypeError;

    uint32_t longval = PyLong_AsLong(in);

    if(PyErr_Occurred() || longval > 0xffff)
      return SWIG_OverflowError;

    out = int16_t(longval & 0xff);

    return SWIG_OK;
  }

  static PyObject *ConvertToPy(const int16_t &in) { return PyLong_FromLong(in); }
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

  static PyObject *ConvertToPy(const uint32_t &in) { return PyLong_FromUnsignedLong(in); }
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

  static PyObject *ConvertToPy(const int32_t &in) { return PyLong_FromLong(in); }
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

  static PyObject *ConvertToPy(const uint64_t &in) { return PyLong_FromUnsignedLongLong(in); }
};

template <>
struct TypeConversion<int64_t, false>
{
  static int ConvertFromPy(PyObject *in, int64_t &out)
  {
    if(!PyLong_Check(in))
      return SWIG_TypeError;

    out = PyLong_AsLongLong(in);

    if(PyErr_Occurred())
      return SWIG_OverflowError;

    return SWIG_OK;
  }

  static PyObject *ConvertToPy(const int64_t &in) { return PyLong_FromLongLong(in); }
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

  static PyObject *ConvertToPy(const float &in) { return PyFloat_FromDouble(in); }
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

  static PyObject *ConvertToPy(const double &in) { return PyFloat_FromDouble(in); }
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

  static PyObject *ConvertToPy(const T &in)
  {
    return TypeConversion<etype>::ConvertToPy(etype(in));
  }
};

// specialisation for datetime
template <>
struct TypeConversion<rdcdatetime, false>
{
  static int ConvertFromPy(PyObject *in, rdcdatetime &out)
  {
    if(!PyDateTime_Check(in))
      return SWIG_TypeError;

    out.year = PyDateTime_GET_YEAR(in);
    out.month = PyDateTime_GET_MONTH(in);
    out.day = PyDateTime_GET_DAY(in);
    out.hour = PyDateTime_DATE_GET_HOUR(in);
    out.minute = PyDateTime_DATE_GET_MINUTE(in);
    out.second = PyDateTime_DATE_GET_SECOND(in);
    out.microsecond = PyDateTime_TIME_GET_MICROSECOND(in);

    return SWIG_OK;
  }

  static PyObject *ConvertToPy(const rdcdatetime &in)
  {
    return PyDateTime_FromDateAndTime(in.year, in.month, in.day, in.hour, in.minute, in.second,
                                      in.microsecond);
  }
};

// specialisation for pair
template <typename A, typename B>
struct TypeConversion<rdcpair<A, B>, false>
{
  static int ConvertFromPy(PyObject *in, rdcpair<A, B> &out, int *failIdx)
  {
    if(!PyTuple_Check(in))
      return SWIG_TypeError;

    Py_ssize_t size = PyTuple_Size(in);

    if(size != 2)
      return SWIG_TypeError;

    int ret = TypeConversion<A>::ConvertFromPy(PyTuple_GetItem(in, 0), out.first);

    if(!SWIG_IsOK(ret))
    {
      if(failIdx)
        *failIdx = 0;
      return ret;
    }

    ret = TypeConversion<B>::ConvertFromPy(PyTuple_GetItem(in, 1), out.second);

    if(!SWIG_IsOK(ret))
    {
      if(failIdx)
        *failIdx = 1;
      return ret;
    }

    return ret;
  }

  static int ConvertFromPy(PyObject *in, rdcpair<A, B> &out)
  {
    return ConvertFromPy(in, out, NULL);
  }

  static PyObject *ConvertToPy(const rdcpair<A, B> &in, int *failIdx)
  {
    PyObject *first = TypeConversion<A>::ConvertToPy(in.first);
    if(!first)
    {
      if(failIdx)
        *failIdx = 0;
      return NULL;
    }

    PyObject *second = TypeConversion<B>::ConvertToPy(in.second);
    if(!second)
    {
      if(failIdx)
        *failIdx = 1;
      return NULL;
    }

    PyObject *ret = PyTuple_New(2);
    if(!ret)
      return NULL;

    PyTuple_SetItem(ret, 0, first);
    PyTuple_SetItem(ret, 1, second);

    return ret;
  }

  static PyObject *ConvertToPy(const rdcpair<A, B> &in) { return ConvertToPy(in, NULL); }
};

// specialisation for bytebuf
template <>
struct TypeConversion<bytebuf, false>
{
  // we add some extra parameters so the typemaps for array can use these to get
  // nicer failure error messages out with the index that failed
  static int ConvertFromPy(PyObject *in, bytebuf &out, int *failIdx)
  {
    if(!PyBytes_Check(in))
      return SWIG_TypeError;

    Py_ssize_t len = PyBytes_Size(in);

    out.resize((size_t)len);
    memcpy(out.data(), PyBytes_AsString(in), out.size());

    return SWIG_OK;
  }

  static int ConvertFromPy(PyObject *in, bytebuf &out) { return ConvertFromPy(in, out, NULL); }
  static PyObject *ConvertToPyInPlace(PyObject *list, const bytebuf &in, int *failIdx)
  {
    // can't modify bytes objects
    return SWIG_Py_Void();
  }

  static PyObject *ConvertToPy(const bytebuf &in, int *failIdx)
  {
    return PyBytes_FromStringAndSize((const char *)in.data(), (Py_ssize_t)in.size());
  }

  static PyObject *ConvertToPy(const bytebuf &in) { return ConvertToPy(in, NULL); }
};

// specialisation for array
template <typename U>
struct TypeConversion<rdcarray<U>, false>
{
  static swig_type_info *GetTypeInfo()
  {
    static swig_type_info *cached_type_info = NULL;
    static rdcstr typeName = "rdcarray < " + TypeName<U>() + " > *";

    if(cached_type_info)
      return cached_type_info;

    cached_type_info = SWIG_TypeQuery(typeName.c_str());

    return cached_type_info;
  }

  // we add some extra parameters so the typemaps for array can use these to get
  // nicer failure error messages out with the index that failed
  static int ConvertFromPy(PyObject *in, rdcarray<U> &out, int *failIdx)
  {
    if(!PyList_Check(in))
      return SWIG_TypeError;

    out.resize((size_t)PyList_Size(in));

    for(int i = 0; i < out.count(); i++)
    {
      int ret = TypeConversion<U>::ConvertFromPy(PyList_GetItem(in, i), out[i]);
      if(!SWIG_IsOK(ret))
      {
        if(failIdx)
          *failIdx = i;
        return ret;
      }
    }

    return SWIG_OK;
  }

  static int ConvertFromPy(PyObject *in, rdcarray<U> &out) { return ConvertFromPy(in, out, NULL); }
  static PyObject *ConvertToPyInPlace(PyObject *list, const rdcarray<U> &in, int *failIdx)
  {
    for(int i = 0; i < in.count(); i++)
    {
      PyObject *elem = TypeConversion<U>::ConvertToPy(in[i]);

      if(elem)
      {
        PyList_Append(list, elem);
        // release our reference
        Py_DecRef(elem);
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

  static PyObject *ConvertToPy(const rdcarray<U> &in, int *failIdx)
  {
    PyObject *list = PyList_New(0);
    if(!list)
      return NULL;

    PyObject *ret = ConvertToPyInPlace(list, in, failIdx);

    // if a failure happened, don't leak the list we created
    if(!ret)
      Py_XDECREF(list);

    return ret;
  }

  static PyObject *ConvertToPy(const rdcarray<U> &in) { return ConvertToPy(in, NULL); }
};

// specialisation for string
template <>
struct TypeConversion<rdcstr, false>
{
  static swig_type_info *GetTypeInfo()
  {
    static swig_type_info *cached_type_info = NULL;

    if(cached_type_info)
      return cached_type_info;

    cached_type_info = SWIG_TypeQuery("rdcstr *");

    return cached_type_info;
  }

  static int ConvertFromPy(PyObject *in, rdcstr &out)
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
        out.assign(buf, size);

        Py_DecRef(bytes);

        return SWIG_OK;
      }

      Py_DecRef(bytes);

      return SWIG_ERROR;
    }

    swig_type_info *type_info = GetTypeInfo();
    if(!type_info)
      return SWIG_ERROR;

    rdcstr *ptr = NULL;
    int res = SWIG_ConvertPtr(in, (void **)&ptr, type_info, 0);
    if(SWIG_IsOK(res))
      out = *ptr;

    return res;
  }

  static PyObject *ConvertToPy(const rdcstr &in)
  {
    return PyUnicode_FromStringAndSize(in.c_str(), in.size());
  }
};

#include "structured_conversion.h"

// free functions forward to struct
template <typename T>
int ConvertFromPy(PyObject *in, T &out)
{
  return TypeConversion<T>::ConvertFromPy(in, out);
}

template <typename T>
PyObject *ConvertToPy(const T &in)
{
  return TypeConversion<T>::ConvertToPy(in);
}

namespace
{
template <typename T, bool is_pointer = std::is_pointer<T>::value>
struct pointer_unwrap;

template <typename T>
struct pointer_unwrap<T, false>
{
  static void tempset(T &ptr, T *tempobj) {}
  static void tempalloc(T &ptr, unsigned char *tempmem) {}
  static void tempdealloc(T &ptr) {}
  static T &indirect(T &ptr) { return ptr; }
};

template <typename T>
struct pointer_unwrap<T, true>
{
  typedef typename std::remove_pointer<T>::type U;

  static void tempset(U *&ptr, U *tempobj) { ptr = tempobj; }
  static void tempalloc(U *&ptr, unsigned char *tempmem) { ptr = new(tempmem) U; }
  static void tempdealloc(U *ptr)
  {
    if(ptr)
      ptr->~U();
  }
  static U &indirect(U *ptr) { return *ptr; }
};
};

template <typename T>
inline void tempalloc(T &ptr, unsigned char *tempmem)
{
  pointer_unwrap<T>::tempalloc(ptr, tempmem);
}

template <typename T, typename U>
inline void tempset(T &ptr, U *tempobj)
{
  pointer_unwrap<T>::tempset(ptr, tempobj);
}

template <typename T>
inline void tempdealloc(T ptr)
{
  pointer_unwrap<T>::tempdealloc(ptr);
}

template <typename T>
inline typename std::remove_pointer<T>::type &indirect(T &ptr)
{
  return pointer_unwrap<T>::indirect(ptr);
}

#include "function_conversion.h"

#include "container_handling.h"

#include "ext_refcounts.h"
