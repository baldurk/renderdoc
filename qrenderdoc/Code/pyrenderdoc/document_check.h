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

inline void check_docstrings(swig_type_info **swig_types, size_t numTypes)
{
  std::set<std::string> docstrings;
  for(size_t i = 0; i < numTypes; i++)
  {
    SwigPyClientData *typeinfo = (SwigPyClientData *)swig_types[i]->clientdata;

    // opaque types have no typeinfo, skip these
    if(!typeinfo)
      continue;

    PyTypeObject *typeobj = typeinfo->pytype;

    std::string typedoc = typeobj->tp_doc;

    auto result = docstrings.insert(typedoc);

    if(!result.second)
    {
      snprintf(convert_error, sizeof(convert_error) - 1,
               "Duplicate docstring '%s' found on struct '%s' - are you missing a DOCUMENT()?",
               typedoc.c_str(), typeobj->tp_name);
      RENDERDOC_LogMessage(LogType::Fatal, "QTRD", __FILE__, __LINE__, convert_error);
    }

    PyMethodDef *method = typeobj->tp_methods;

    while(method->ml_doc)
    {
      std::string typedoc = method->ml_doc;

      size_t i = 0;
      while(typedoc[i] == '\n')
        i++;

      // skip the first line as it's autodoc generated
      i = typedoc.find('\n', i);
      if(i != std::string::npos)
      {
        while(typedoc[i] == '\n')
          i++;

        typedoc.erase(0, i);

        result = docstrings.insert(typedoc);

        if(!result.second)
        {
          snprintf(
              convert_error, sizeof(convert_error) - 1,
              "Duplicate docstring '%s' found on method '%s.%s' - are you missing a DOCUMENT()?",
              method_doc.c_str(), typeobj->tp_name, method->ml_name);
          RENDERDOC_LogMessage(LogType::Fatal, "QTRD", __FILE__, __LINE__, convert_error);
        }
      }

      method++;
    }
  }
}
