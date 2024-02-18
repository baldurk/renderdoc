/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2024 Baldur Karlsson
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

#include "3rdparty/pythoncapi_compat.h"

template <typename objType>
inline std::map<const objType *, PyObject *> &obj2py();

template <>
inline std::map<const SDChunk *, PyObject *> &obj2py<SDChunk>()
{
  static std::map<const SDChunk *, PyObject *> mapping;
  return mapping;
}

template <>
inline std::map<const SDObject *, PyObject *> &obj2py<SDObject>()
{
  static std::map<const SDObject *, PyObject *> mapping;
  return mapping;
}

// specialisations for structured data
template <typename refcountedType>
struct ActiveRefcounter
{
  static PyObject *GetPyObject(const refcountedType *c)
  {
    auto it = obj2py<refcountedType>().find(c);
    if(it == obj2py<refcountedType>().end())
    {
      // not recognised - must be C++ side owned. Construct a non-owning PyObject and DON'T insert
      // it into the map. The map is only for objects python owns
      swig_type_info *type_info = TypeConversion<refcountedType>::GetTypeInfo();
      if(type_info == NULL)
        return NULL;

      return SWIG_InternalNewPointerObj((void *)c, type_info, 0);
    }

    // recognised - inc refcount on existing object and return
    Py_IncRef(it->second);
    return it->second;
  }

  static bool HasPyObject(const refcountedType *c)
  {
    return obj2py<refcountedType>().find(c) != obj2py<refcountedType>().end();
  }

  static void NewPyObject(PyObject *py, const refcountedType *c)
  {
    obj2py<refcountedType>()[c] = py;
  }
  static void DelPyObject(PyObject *py, refcountedType *c) { obj2py<refcountedType>().erase(c); }
  static void Dec(const refcountedType *c)
  {
    auto it = obj2py<refcountedType>().find(c);
    if(it != obj2py<refcountedType>().end())
      Py_DecRef(it->second);
  }
  static void Inc(const refcountedType *c)
  {
    auto it = obj2py<refcountedType>().find(c);
    if(it != obj2py<refcountedType>().end())
      Py_IncRef(it->second);
  }
};

template <typename T>
struct ExtRefcount
{
  static void Dec(const T &t) {}
  static void Inc(const T &t) {}
};

template <typename refcountedType>
struct RefcountConverter
{
  static int ConvertFromPy(PyObject *in, refcountedType *&out)
  {
    // We just unbox the PyObject
    void *ptr = NULL;
    int res = 0;
    swig_type_info *typeInfo = TypeConversion<refcountedType>::GetTypeInfo();

    if(!typeInfo)
      return SWIG_RuntimeError;

    res = SWIG_ConvertPtr(in, &ptr, typeInfo, 0);
    if(SWIG_IsOK(res))
      out = (refcountedType *)ptr;

    // increment the refcount to indicate that there's an externally stored reference.
    Py_IncRef(in);

    return res;
  }
  static PyObject *ConvertToPy(refcountedType *const &in)
  {
    return ExtRefcount<refcountedType *>::GetPyObject(in);
  }
};

template <>
struct ExtRefcount<SDChunk *> : public ActiveRefcounter<SDChunk>
{
  static void DelPyObject(PyObject *py, SDChunk *c)
  {
    // dec ref any python-owned objects in the children array, so the default destructor doesn't
    // just delete them.
    for(size_t i = 0; i < c->NumChildren(); i++)
      if(ActiveRefcounter<SDObject>::HasPyObject(c->GetChild(i)))
        ActiveRefcounter<SDObject>::Dec(c->GetChild(i));

    // we clear the array, because anything still left is C++ owned. We're just borrowing a
    // reference to it, so C++ will control the lifetime.
    StructuredObjectList discard;
    c->TakeAllChildren(discard);

    ActiveRefcounter<SDChunk>::DelPyObject(py, c);
  }
};

template <>
struct TypeConversion<SDChunk *, false> : public RefcountConverter<SDChunk>
{
};

template <>
struct ExtRefcount<SDObject *> : public ActiveRefcounter<SDObject>
{
  static void DelPyObject(PyObject *py, SDObject *o)
  {
    // dec ref any python-owned objects in the children array, so the default destructor doesn't
    // just delete them.
    for(size_t i = 0; i < o->NumChildren(); i++)
      if(ActiveRefcounter<SDObject>::HasPyObject(o->GetChild(i)))
        ActiveRefcounter<SDObject>::Dec(o->GetChild(i));

    // we clear the array, because anything still left is C++ owned. We're just borrowing a
    // reference to it, so C++ will control the lifetime.
    StructuredObjectList discard;
    o->TakeAllChildren(discard);

    ActiveRefcounter<SDObject>::DelPyObject(py, o);
  }
};

// mostly the same as the plain bytebuf conversion, but when converting from py we need to
// allocate. This will only be used when assigning a buffer in an SDFile's StructuredBufferList,
// which then takes ownership of the allocated object so it doesn't leak.
template <>
struct TypeConversion<bytebuf *, false>
{
  static int ConvertFromPy(PyObject *in, bytebuf *&out, int *failIdx)
  {
    out = new bytebuf;
    return TypeConversion<bytebuf>::ConvertFromPy(in, *out, failIdx);
  }

  static int ConvertFromPy(PyObject *in, bytebuf *&out) { return ConvertFromPy(in, out, NULL); }
  static PyObject *ConvertToPyInPlace(PyObject *list, const bytebuf *in, int *failIdx)
  {
    // can't modify bytes objects
    return SWIG_Py_Void();
  }

  static PyObject *ConvertToPy(const bytebuf *in, int *failIdx)
  {
    return TypeConversion<bytebuf>::ConvertToPy(*in, failIdx);
  }

  static PyObject *ConvertToPy(const bytebuf *in) { return ConvertToPy(in, NULL); }
};

template <>
struct ExtRefcount<SDFile *>
{
  static void Dec(const SDFile *t) {}
  static void Inc(const SDFile *t) {}
  static void NewPyObject(PyObject *py, const SDFile *f) {}
  static void DelPyObject(PyObject *py, SDFile *f)
  {
    // dec ref any python-owned objects in the children array, so the default destructor doesn't
    // just delete them.
    for(size_t i = 0; i < f->chunks.size(); i++)
      if(ActiveRefcounter<SDChunk>::HasPyObject(f->chunks[i]))
        ActiveRefcounter<SDChunk>::Dec(f->chunks[i]);

    // we clear the array, because anything still left is C++ owned. We're just borrowing a
    // reference to it, so C++ will control the lifetime.
    f->chunks.clear();
  }
};

template <>
struct TypeConversion<SDObject *, false> : public RefcountConverter<SDObject>
{
};

template <>
struct TypeConversion<StructuredBufferList, false>
{
  static swig_type_info *GetTypeInfo()
  {
    static swig_type_info *cached_type_info = NULL;

    if(cached_type_info)
      return cached_type_info;

    cached_type_info = SWIG_TypeQuery("StructuredBufferList *");

    return cached_type_info;
  }

  // we add some extra parameters so the typemaps for array can use these to get
  // nicer failure error messages out with the index that failed
  static int ConvertFromPy(PyObject *in, StructuredBufferList &out, int *failIdx)
  {
    swig_type_info *own_type = GetTypeInfo();
    if(own_type)
    {
      StructuredBufferList *ptr = NULL;
      int ret = SWIG_ConvertPtr(in, (void **)&ptr, own_type, 0);
      if(SWIG_IsOK(ret))
      {
        // we need to duplicate the objects here, otherwise the owner of both lists will try and
        // delete the same things when they destruct. Avoiding copies must be done another way
        out.resize(ptr->size());
        for(size_t i = 0; i < ptr->size(); i++)
          out[i] = new bytebuf(*ptr->at(i));
        return SWIG_OK;
      }
    }

    if(!PyList_Check(in))
      return SWIG_TypeError;

    out.resize((size_t)PyList_Size(in));

    for(int i = 0; i < out.count(); i++)
    {
      PyObject *elem = PyList_GetItem(in, i);
      if(Py_IsNone(elem))
      {
        out[i] = NULL;
      }
      else
      {
        out[i] = new bytebuf;
        int ret = TypeConversion<bytebuf>::ConvertFromPy(elem, *out[i]);
        if(!SWIG_IsOK(ret))
        {
          if(failIdx)
            *failIdx = i;
          return ret;
        }
      }
    }

    return SWIG_OK;
  }

  static int ConvertFromPy(PyObject *in, StructuredBufferList &out)
  {
    return ConvertFromPy(in, out, NULL);
  }
  static PyObject *ConvertToPyInPlace(PyObject *list, const StructuredBufferList &in, int *failIdx)
  {
    for(int i = 0; i < in.count(); i++)
    {
      PyObject *elem = SWIG_Py_Void();
      if(in[i])
        elem = TypeConversion<bytebuf>::ConvertToPy(*in[i]);

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

  static PyObject *ConvertToPy(const StructuredBufferList &in, int *failIdx)
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

  static PyObject *ConvertToPy(const StructuredBufferList &in) { return ConvertToPy(in, NULL); }
};

template <>
struct TypeConversion<StructuredObjectList, false>
{
  static swig_type_info *GetTypeInfo()
  {
    static swig_type_info *cached_type_info = NULL;

    if(cached_type_info)
      return cached_type_info;

    cached_type_info = SWIG_TypeQuery("StructuredObjectList *");

    return cached_type_info;
  }

  // we add some extra parameters so the typemaps for array can use these to get
  // nicer failure error messages out with the index that failed
  static int ConvertFromPy(PyObject *in, StructuredObjectList &out, int *failIdx)
  {
    swig_type_info *own_type = GetTypeInfo();
    if(own_type)
    {
      StructuredObjectList *ptr = NULL;
      int ret = SWIG_ConvertPtr(in, (void **)&ptr, own_type, 0);
      if(SWIG_IsOK(ret))
      {
        // we need to duplicate the objects here, otherwise the owner of both lists will try and
        // delete the same things when they destruct. Avoiding copies must be done another way
        out.resize(ptr->size());
        for(size_t i = 0; i < ptr->size(); i++)
        {
          SDObject *obj = ptr->at(i);
          if(ActiveRefcounter<SDObject>::HasPyObject(obj))
          {
            out[i] = obj;
            ActiveRefcounter<SDObject>::Inc(obj);
          }
          else
          {
            out[i] = obj->Duplicate();
          }
        }
        return SWIG_OK;
      }
    }

    swig_type_info *type_info = TypeConversion<SDObject>::GetTypeInfo();
    if(type_info == NULL)
      return SWIG_RuntimeError;

    if(!PyList_Check(in))
      return SWIG_TypeError;

    out.resize((size_t)PyList_Size(in));

    for(int i = 0; i < out.count(); i++)
    {
      PyObject *elem = PyList_GetItem(in, i);
      if(Py_IsNone(elem))
      {
        out[i] = NULL;
      }
      else
      {
        SDObject *ptr = NULL;
        int ret = SWIG_ConvertPtr(elem, (void **)&ptr, type_info, 0);
        if(SWIG_IsOK(ret))
        {
          if(ActiveRefcounter<SDObject>::HasPyObject(ptr))
          {
            out[i] = ptr;
            Py_IncRef(elem);
          }
          else
          {
            out[i] = ptr->Duplicate();
          }
        }
        else
        {
          if(failIdx)
            *failIdx = i;
          return ret;
        }
      }
    }

    return SWIG_OK;
  }

  static int ConvertFromPy(PyObject *in, StructuredObjectList &out)
  {
    return ConvertFromPy(in, out, NULL);
  }
  static PyObject *ConvertToPyInPlace(PyObject *list, const StructuredObjectList &in, int *failIdx)
  {
    swig_type_info *type_info = TypeConversion<SDObject>::GetTypeInfo();
    if(type_info == NULL)
      return NULL;

    for(int i = 0; i < in.count(); i++)
    {
      PyObject *elem = SWIG_Py_Void();
      if(in[i])
      {
        SDObject *pyCopy = in[i]->Duplicate();

        elem = SWIG_InternalNewPointerObj((void *)pyCopy, type_info, SWIG_POINTER_OWN);
      }

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

  static PyObject *ConvertToPy(const StructuredObjectList &in, int *failIdx)
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

  static PyObject *ConvertToPy(const StructuredObjectList &in) { return ConvertToPy(in, NULL); }
};

template <>
struct TypeConversion<StructuredChunkList, false>
{
  static swig_type_info *GetTypeInfo()
  {
    static swig_type_info *cached_type_info = NULL;

    if(cached_type_info)
      return cached_type_info;

    cached_type_info = SWIG_TypeQuery("StructuredChunkList *");

    return cached_type_info;
  }

  // we add some extra parameters so the typemaps for array can use these to get
  // nicer failure error messages out with the index that failed
  static int ConvertFromPy(PyObject *in, StructuredChunkList &out, int *failIdx)
  {
    swig_type_info *own_type = GetTypeInfo();
    if(own_type)
    {
      StructuredChunkList *ptr = NULL;
      int ret = SWIG_ConvertPtr(in, (void **)&ptr, own_type, 0);
      if(SWIG_IsOK(ret))
      {
        // we need to duplicate the objects here, otherwise the owner of both lists will try and
        // delete the same things when they destruct. Avoiding copies must be done another way
        out.resize(ptr->size());
        for(size_t i = 0; i < ptr->size(); i++)
        {
          SDChunk *obj = ptr->at(i);
          if(ActiveRefcounter<SDChunk>::HasPyObject(obj))
          {
            out[i] = obj;
            ActiveRefcounter<SDChunk>::Inc(obj);
          }
          else
          {
            out[i] = obj->Duplicate();
          }
        }
        return SWIG_OK;
      }
    }

    swig_type_info *type_info = TypeConversion<SDChunk>::GetTypeInfo();
    if(type_info == NULL)
      return SWIG_RuntimeError;

    if(!PyList_Check(in))
      return SWIG_TypeError;

    out.resize((size_t)PyList_Size(in));

    for(int i = 0; i < out.count(); i++)
    {
      PyObject *elem = PyList_GetItem(in, i);
      if(Py_IsNone(elem))
      {
        out[i] = NULL;
      }
      else
      {
        SDChunk *ptr = NULL;
        int ret = SWIG_ConvertPtr(elem, (void **)&ptr, type_info, 0);
        if(SWIG_IsOK(ret))
        {
          if(ActiveRefcounter<SDChunk>::HasPyObject(ptr))
          {
            out[i] = ptr;
            Py_IncRef(elem);
          }
          else
          {
            out[i] = ptr->Duplicate();
          }
        }
        else
        {
          if(failIdx)
            *failIdx = i;
          return ret;
        }
      }
    }

    return SWIG_OK;
  }

  static int ConvertFromPy(PyObject *in, StructuredChunkList &out)
  {
    return ConvertFromPy(in, out, NULL);
  }
  static PyObject *ConvertToPyInPlace(PyObject *list, const StructuredChunkList &in, int *failIdx)
  {
    swig_type_info *type_info = TypeConversion<SDChunk>::GetTypeInfo();
    if(type_info == NULL)
      return NULL;

    for(int i = 0; i < in.count(); i++)
    {
      PyObject *elem = SWIG_Py_Void();
      if(in[i])
      {
        SDChunk *pyCopy = in[i]->Duplicate();

        elem = SWIG_InternalNewPointerObj((void *)pyCopy, type_info, SWIG_POINTER_OWN);
      }

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

  static PyObject *ConvertToPy(const StructuredChunkList &in, int *failIdx)
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

  static PyObject *ConvertToPy(const StructuredChunkList &in) { return ConvertToPy(in, NULL); }
};
