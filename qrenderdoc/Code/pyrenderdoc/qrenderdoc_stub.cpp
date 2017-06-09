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

#include <Python.h>
#include "Code/QRDUtils.h"
#include "PythonContext.h"

// we only support the qrenderdoc module for docs generation, so it doesn't matter that these stub
// functions aren't valid

void Formatter::setParams(const PersistantConfig &config)
{
}

bool SaveToJSON(QVariantMap &data, QIODevice &f, const char *magicIdentifier, uint32_t magicVersion)
{
  return false;
}

bool LoadFromJSON(QVariantMap &data, QIODevice &f, const char *magicIdentifier, uint32_t magicVersion)
{
  return false;
}

QWidget *PythonContext::QWidgetFromPy(PyObject *widget)
{
  return NULL;
}

PyObject *PythonContext::QtObjectToPython(PyObject *self, const char *typeName, QObject *object)
{
  return NULL;
}
