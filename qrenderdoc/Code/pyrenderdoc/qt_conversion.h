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

  static PyObject *ConvertToPy(const QString &in)
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

  static PyObject *ConvertToPy(const QDateTime &in)
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
  static PyObject *ConvertToPyInPlace(PyObject *list, const Container &in, int *failIdx)
  {
    for(int i = 0; i < in.size(); i++)
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

  static PyObject *ConvertToPy(const Container &in, int *failIdx)
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

  static PyObject *ConvertToPy(const Container &in) { return ConvertToPy(in, NULL); }
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

  static PyObject *ConvertToPy(const QPair<A, B> &in)
  {
    PyObject *first = TypeConversion<A>::ConvertToPy(in.first);
    if(!first)
      return NULL;

    PyObject *second = TypeConversion<B>::ConvertToPy(in.second);
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
  static PyObject *ConvertToPyInPlace(PyObject *pymap, const QMap<K, V> &in, int *failIdx)
  {
    QList<K> keys = in.keys();

    for(int i = 0; i < keys.size(); i++)
    {
      const K &k = keys[i];
      PyObject *key = TypeConversion<K>::ConvertToPy(k);

      if(key)
      {
        PyObject *value = TypeConversion<V>::ConvertToPy(in[k]);

        if(value)
        {
          PyDict_SetItem(pymap, key, value);
          // release our reference
          Py_DecRef(key);
          // release our reference
          Py_DecRef(value);
          continue;
        }

        // destroy unused key
        Py_DecRef(key);
      }

      if(failIdx)
        *failIdx = i;

      return NULL;
    }

    return pymap;
  }

  static PyObject *ConvertToPy(const QMap<K, V> &in, int *failIdx)
  {
    PyObject *list = PyDict_New();
    if(!list)
      return NULL;

    PyObject *ret = ConvertToPyInPlace(list, in, failIdx);

    // if a failure happened, don't leak the map we created
    if(!ret)
      Py_XDECREF(list);

    return ret;
  }

  static PyObject *ConvertToPy(const QMap<K, V> &in) { return ConvertToPy(in, NULL); }
};

#endif
