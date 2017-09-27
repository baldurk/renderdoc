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

#include <algorithm>

///////////////////////////////////////////////////////////////////////////
// utility function to get the real C++ this pointer out of PyObject* given
// a type, so we can modify it by reference if we want
template <typename arrayType>
arrayType *array_thisptr(PyObject *self)
{
  void *ptr = NULL;
  int res = 0;
  swig_type_info *typeInfo = TypeConversion<arrayType>::GetTypeInfo();

  if(!typeInfo)
    SWIG_exception_fail(SWIG_RuntimeError, "Internal error fetching type info");

  res = SWIG_ConvertPtr(self, &ptr, typeInfo, 0);
  if(!SWIG_IsOK(res))
    SWIG_exception_fail(SWIG_ArgError(res), "Couldn't convert array type");

  return (arrayType *)ptr;

fail:
  return NULL;
}

///////////////////////////////////////////////////////////////////////////
// Bit of a hack - use a dispatch struct templated on a constant bool, so
// we can do a compile-time check if we're converting 'self' and only invoke
// array_thisptr for arrays that we want to be modifying by reference.
template <bool isSelf>
struct self_dispatch
{
  template <typename arrayType>
  static arrayType *getthis(PyObject *self)
  {
    return NULL;
  }
};

template <>
struct self_dispatch<true>
{
  template <typename arrayType>
  static arrayType *getthis(PyObject *self)
  {
    return ::array_thisptr<arrayType>(self);
  }
};
///////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////
// templated implementations of array functions for both slots and named
// functions in python sequences

// returns the index into an array, including reverseing negative indices to refer to elements from
// the end of the array (-1 = last, -2 = second last, etc).
template <typename arrayType>
Py_ssize_t array_revindex(arrayType *thisptr, PyObject *index)
{
  Py_ssize_t idx = 0;

  if(!PyIndex_Check(index))
    SWIG_exception_fail(SWIG_TypeError, "invalid index type");

  idx = PyNumber_AsSsize_t(index, PyExc_IndexError);

  // don't need to fail, it's already been thrown
  if(idx == -1 && PyErr_Occurred())
    return PY_SSIZE_T_MIN;

  // if the index is negative, treat it as an offset from the end, with -1 being not last but
  // next-to-last
  if(idx < 0)
    idx = (Py_ssize_t)thisptr->size() + idx;

  return idx;
fail:
  return PY_SSIZE_T_MIN;
}

template <typename arrayType>
PyObject *array_repr(arrayType *thisptr)
{
  PyObject *result = NULL;
  PyObject *list = NULL;

  // make a copy as a python list
  list = ConvertToPy(*thisptr);

  if(list == NULL)
    SWIG_exception_fail(SWIG_ValueError, "invalid array");

  // use the python list repr function
  result = PyObject_Repr(list);
  Py_DECREF(list);
  return result;
fail:
  return NULL;
}

template <typename arrayType>
PyObject *array_getitem(arrayType *thisptr, Py_ssize_t idx)
{
  if(idx < 0 || (size_t)idx >= thisptr->size())
    SWIG_exception_fail(SWIG_IndexError, "list index out of range");

  return ConvertToPy(thisptr->at(idx));
fail:
  return NULL;
}

template <typename arrayType>
int array_setitem(arrayType *thisptr, Py_ssize_t idx, PyObject *val)
{
  int res = SWIG_OK;

  if(idx < 0 || (size_t)idx >= thisptr->size())
    SWIG_exception_fail(SWIG_IndexError, "list assignment index out of range");

  if(val == NULL)
  {
    thisptr->erase(idx);
    res = SWIG_OK;
  }
  else
  {
    res = ConvertFromPy(val, thisptr->at(idx));
  }

  if(SWIG_IsOK(res))
    return 0;
fail:
  return -1;
}

template <typename arrayType>
Py_ssize_t array_len(arrayType *thisptr)
{
  return thisptr->size();
}

template <typename arrayType>
PyObject *array_clear(arrayType *thisptr)
{
  thisptr->clear();
  return SWIG_Py_Void();
}

template <typename arrayType>
PyObject *array_reverse(arrayType *thisptr)
{
  std::reverse(thisptr->begin(), thisptr->end());
  return SWIG_Py_Void();
}

template <typename arrayType>
PyObject *array_copy(arrayType *thisptr)
{
  PyObject *list = PyList_New(0);
  if(!list)
    return NULL;

  PyObject *ret = NULL;

  for(size_t i = 0; i < thisptr->size(); i++)
  {
    ret = ConvertToPy(thisptr->at(i));

    PyList_Append(list, ret);

    if(!ret)
      SWIG_exception_fail(SWIG_TypeError, "failed to convert element while copying");
  }

  return list;
fail:
  if(list)
    Py_XDECREF(list);

  return NULL;
}

template <typename arrayType>
PyObject *array_sort(arrayType *thisptr, PyObject *key, bool reverse)
{
  // TODO - implement sort
  return SWIG_Py_Void();
}

template <typename arrayType>
PyObject *array_append(arrayType *thisptr, PyObject *value)
{
  typename arrayType::value_type converted;
  int res = ConvertFromPy(value, converted);

  if(SWIG_IsOK(res))
  {
    thisptr->push_back(converted);
    return SWIG_Py_Void();
  }

  SWIG_exception_fail(SWIG_ArgError(res), "failed to convert element while appending");
fail:
  return NULL;
}

template <typename arrayType>
PyObject *array_insert(arrayType *thisptr, PyObject *index, PyObject *value)
{
  typename arrayType::value_type converted;
  int res = 0;
  Py_ssize_t idx = array_revindex(thisptr, index);

  // if an error occurred an exception has been thrown, just return NULL
  if(idx == PY_SSIZE_T_MIN)
    return NULL;

  // insert clamps the index
  if(idx < 0)
    idx = 0;
  if(idx > thisptr->count())
    idx = thisptr->count();

  res = ConvertFromPy(value, converted);

  if(SWIG_IsOK(res))
  {
    thisptr->insert(idx, converted);
    return SWIG_Py_Void();
  }

  SWIG_exception_fail(SWIG_ArgError(res), "failed to convert element while inserting");
fail:
  return NULL;
}

template <typename arrayType>
PyObject *array_pop(arrayType *thisptr, PyObject *index)
{
  PyObject *ret = NULL;
  Py_ssize_t idx = index ? array_revindex(thisptr, index) : thisptr->size() - 1;

  // if an error occurred an exception has been thrown, just return NULL
  if(idx == PY_SSIZE_T_MIN)
    return NULL;

  if(idx < 0 || idx > thisptr->count())
    SWIG_exception_fail(SWIG_IndexError, "pop index out of range");

  if(thisptr->empty())
    SWIG_exception_fail(SWIG_IndexError, "pop from empty list");

  ret = ConvertToPy(thisptr->at(idx));

  if(!ret)
    SWIG_exception_fail(SWIG_TypeError, "failed to convert element while popping");

  thisptr->erase(idx);
  return ret;
fail:
  return NULL;
}
