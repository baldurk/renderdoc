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

///////////////////////////////////////////////////////////////////////////
// Helper template used to implement tp_init for a refcounted type, handles
// constructing a new object from a PyObject *argsTuple

template <typename refcountedType>
inline refcountedType *MakeFromArgsTuple(PyObject *args);

template <>
inline SDChunk *MakeFromArgsTuple<SDChunk>(PyObject *args)
{
  int res = 0;
  PyObject *name_obj = NULL;
  SDChunk *result = NULL;
  rdcstr name;

  if(!SWIG_Python_UnpackTuple(args, "new_SDChunk", 1, 1, &name_obj))
    SWIG_fail;

  res = ConvertFromPy(name_obj, name);

  if(!SWIG_IsOK(res))
    SWIG_exception_fail(SWIG_ArgError(res), "invalid name used to create SDChunk, expected string");

  result = new SDChunk(name.c_str());

  return result;
fail:
  return NULL;
}

template <>
inline SDObject *MakeFromArgsTuple<SDObject>(PyObject *args)
{
  int res = 0;
  PyObject *params[2] = {NULL};
  SDObject *result = NULL;
  rdcstr name, typeName;

  if(!SWIG_Python_UnpackTuple(args, "new_SDObject", 2, 2, params))
    SWIG_fail;

  res = ConvertFromPy(params[0], name);

  if(!SWIG_IsOK(res))
    SWIG_exception_fail(SWIG_ArgError(res),
                        "invalid name used to create SDObject, expected string");

  res = ConvertFromPy(params[1], typeName);

  if(!SWIG_IsOK(res))
    SWIG_exception_fail(SWIG_ArgError(res),
                        "invalid type name used to create SDObject, expected string");

  result = new SDObject(name.c_str(), typeName);

  return result;
fail:
  return NULL;
}

template <>
inline SDFile *MakeFromArgsTuple<SDFile>(PyObject *args)
{
  return new SDFile();
}
