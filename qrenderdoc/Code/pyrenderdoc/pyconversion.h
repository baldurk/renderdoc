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

// specialisation for array<byte>
template <>
struct TypeConversion<rdctype::array<byte>, false>
{
  // we add some extra parameters so the typemaps for array can use these to get
  // nicer failure error messages out with the index that failed
  static int ConvertFromPy(PyObject *in, rdctype::array<byte> &out, int *failIdx)
  {
    if(!PyBytes_Check(in))
      return SWIG_TypeError;

    Py_ssize_t len = PyBytes_Size(in);

    out.create((int)len);
    memcpy(&out[0], PyBytes_AsString(in), out.count);

    return SWIG_OK;
  }

  static int ConvertFromPy(PyObject *in, rdctype::array<byte> &out)
  {
    return ConvertFromPy(in, out, NULL);
  }

  static PyObject *ConvertToPyInPlace(PyObject *self, PyObject *list,
                                      const rdctype::array<byte> &in, int *failIdx)
  {
    // can't modify bytes objects
    return NULL;
  }

  static PyObject *ConvertToPy(PyObject *self, const rdctype::array<byte> &in, int *failIdx)
  {
    return PyBytes_FromStringAndSize((const char *)in.elems, (Py_ssize_t)in.count);
  }

  static PyObject *ConvertToPy(PyObject *self, const rdctype::array<byte> &in)
  {
    return ConvertToPy(self, in, NULL);
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
        out.count = (int)size;
        out.elems = (char *)out.allocate(size + 1);
        memcpy(out.elems, buf, size);
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

#ifdef ENABLE_QT_CONVERT

template <>
struct TypeConversion<QString, false>
{
  static int ConvertFromPy(PyObject *in, QString &out)
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
        out = QString::fromUtf8(buf, (int)size);

        Py_DecRef(bytes);

        return SWIG_OK;
      }

      Py_DecRef(bytes);

      return SWIG_ERROR;
    }

    return SWIG_ERROR;
  }

  static PyObject *ConvertToPy(PyObject *self, const QString &in)
  {
    QByteArray bytes = in.toUtf8();
    return PyUnicode_FromStringAndSize(bytes.data(), bytes.size());
  }
};

template <>
struct TypeConversion<QDateTime, false>
{
  static int ConvertFromPy(PyObject *in, QDateTime &out)
  {
    if(!PyDateTime_Check(in))
      return SWIG_TypeError;

    QDate date(PyDateTime_GET_YEAR(in), PyDateTime_GET_MONTH(in), PyDateTime_GET_DAY(in));
    QTime time(PyDateTime_DATE_GET_HOUR(in), PyDateTime_DATE_GET_MINUTE(in),
               PyDateTime_DATE_GET_SECOND(in), PyDateTime_DATE_GET_MICROSECOND(in) / 1000);

    out = QDateTime(date, time, QTimeZone::utc());

    return SWIG_OK;
  }

  static PyObject *ConvertToPy(PyObject *self, const QDateTime &in)
  {
    QDate date = in.date();
    QTime time = in.time();
    return PyDateTime_FromDateAndTime(date.year(), date.month(), date.day(), time.hour(),
                                      time.minute(), time.second(), time.msec() * 1000);
  }
};

template <typename Container, typename U>
struct ContainerConversion
{
  // we add some extra parameters so the typemaps for array can use these to get
  // nicer failure error messages out with the index that failed
  static int ConvertFromPy(PyObject *in, Container &out, int *failIdx)
  {
    if(!PyList_Check(in))
      return SWIG_TypeError;

    Py_ssize_t len = PyList_Size(in);

    for(Py_ssize_t i = 0; i < len; i++)
    {
      U u;
      int ret = TypeConversion<U>::ConvertFromPy(PyList_GetItem(in, i), u);
      if(!SWIG_IsOK(ret))
      {
        if(failIdx)
          *failIdx = i;
        return ret;
      }
      out.append(u);
    }

    return SWIG_OK;
  }

  static int ConvertFromPy(PyObject *in, Container &out) { return ConvertFromPy(in, out, NULL); }
  static PyObject *ConvertToPyInPlace(PyObject *self, PyObject *list, const Container &in,
                                      int *failIdx)
  {
    for(int i = 0; i < in.size(); i++)
    {
      PyObject *elem = TypeConversion<U>::ConvertToPy(self, in[i]);

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

  static PyObject *ConvertToPy(PyObject *self, const Container &in, int *failIdx)
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

  static PyObject *ConvertToPy(PyObject *self, const Container &in)
  {
    return ConvertToPy(self, in, NULL);
  }
};

template <typename U>
struct TypeConversion<QList<U>, false> : ContainerConversion<QList<U>, U>
{
};

template <>
struct TypeConversion<QStringList, false> : ContainerConversion<QList<QString>, QString>
{
};

template <typename U>
struct TypeConversion<QVector<U>, false> : ContainerConversion<QVector<U>, U>
{
};

// specialisation for pair
template <typename A, typename B>
struct TypeConversion<QPair<A, B>, false>
{
  static int ConvertFromPy(PyObject *in, QPair<A, B> &out)
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

  static PyObject *ConvertToPy(PyObject *self, const QPair<A, B> &in)
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

template <typename K, typename V>
struct TypeConversion<QMap<K, V>, false>
{
  // we add some extra parameters so the typemaps for array can use these to get
  // nicer failure error messages out with the index that failed
  static int ConvertFromPy(PyObject *in, QMap<K, V> &out, int *failIdx)
  {
    if(!PyDict_Check(in))
      return SWIG_TypeError;

    PyObject *keys = PyDict_Keys(in);

    if(!keys)
      return SWIG_TypeError;

    Py_ssize_t len = PyList_Size(keys);

    for(Py_ssize_t i = 0; i < len; i++)
    {
      K k;
      V v;

      PyObject *key = PyList_GetItem(keys, i);
      PyObject *value = PyDict_GetItem(in, key);
      int ret = TypeConversion<K>::ConvertFromPy(key, k);
      int ret2 = TypeConversion<V>::ConvertFromPy(value, v);
      if(!SWIG_IsOK(ret) || !SWIG_IsOK(ret2))
      {
        if(failIdx)
          *failIdx = i;
        Py_DecRef(keys);
        if(!SWIG_IsOK(ret))
          return ret;
        else
          return ret2;
      }
      out.insert(k, v);
    }

    Py_DecRef(keys);

    return SWIG_OK;
  }

  static int ConvertFromPy(PyObject *in, QMap<K, V> &out) { return ConvertFromPy(in, out, NULL); }
  static PyObject *ConvertToPyInPlace(PyObject *self, PyObject *pymap, const QMap<K, V> &in,
                                      int *failIdx)
  {
    QList<K> keys = in.keys();

    for(int i = 0; i < keys.size(); i++)
    {
      const K &k = keys[i];
      PyObject *key = TypeConversion<K>::ConvertToPy(self, k);

      if(key)
      {
        PyObject *value = TypeConversion<V>::ConvertToPy(self, in[k]);

        if(value)
        {
          PyDict_SetItem(pymap, key, value);
          continue;
        }
      }

      if(failIdx)
        *failIdx = i;

      return NULL;
    }

    return pymap;
  }

  static PyObject *ConvertToPy(PyObject *self, const QMap<K, V> &in, int *failIdx)
  {
    PyObject *list = PyDict_New();
    if(!list)
      return NULL;

    PyObject *ret = ConvertToPyInPlace(self, list, in, failIdx);

    // if a failure happened, don't leak the map we created
    if(!ret)
      Py_XDECREF(list);

    return ret;
  }

  static PyObject *ConvertToPy(PyObject *self, const QMap<K, V> &in)
  {
    return ConvertToPy(self, in, NULL);
  }
};

#endif

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

// this is defined elsewhere for managing the opaque global_handle object
extern "C" PyThreadState *GetExecutingThreadState(PyObject *global_handle);
extern "C" void HandleException(PyObject *global_handle);

// this function handles failures in callback functions. If we're synchronously calling the callback
// from within an execute scope, then we can assign to failflag and let the error propagate upwards.
// If we're not, then the callback is being executed on another thread with no knowledge of python,
// so we need to use the global handle to try and emit the exception through the context. None of
// this is multi-threaded because we're inside the GIL at all times
inline void HandleCallbackFailure(PyObject *global_handle, bool &fail_flag)
{
  // if there's no global handle assume we are not running in the usual environment, so there are no
  // external-to-python threads
  if(!global_handle)
  {
    fail_flag = true;
    return;
  }

  PyThreadState *current = PyGILState_GetThisThreadState();
  PyThreadState *executing = GetExecutingThreadState(global_handle);

  // we are executing synchronously, set the flag and return
  if(current == executing)
  {
    fail_flag = true;
    return;
  }

  // in this case we are executing asynchronously, and must handle the exception manually as there's
  // nothing above us that knows about python exceptions
  HandleException(global_handle);
}

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

    // avoid unused parameter errors when calling a parameter-less function
    (void)self;
    (void)funcname;

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