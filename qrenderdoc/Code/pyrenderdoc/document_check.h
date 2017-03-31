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
  // track all errors and fatal error at the end, so we see all of the problems at once instead of
  // requiring rebuilds over and over.
  // This does mean that e.g. a duplicated docstring could be reported multiple times but that's not
  // the end of the world.
  bool errors_found = false;

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
      RENDERDOC_LogMessage(LogType::Error, "QTRD", __FILE__, __LINE__, convert_error);
      errors_found = true;
    }

    PyObject *dict = typeobj->tp_dict;

    // check the object's dict to see if this is an enum (or struct with constants).
    // We require ALL constants be documented in a docstring with data:: directives
    if(dict && PyDict_Check(dict))
    {
      PyObject *keys = PyDict_Keys(dict);

      if(keys)
      {
        std::set<std::string> constants;

        Py_ssize_t len = PyList_Size(keys);
        for(Py_ssize_t i = 0; i < len; i++)
        {
          PyObject *key = PyList_GetItem(keys, i);
          PyObject *value = PyDict_GetItem(dict, key);

          // if this key is a string (it should be) and the value is an integer, it's a constant
          if(PyUnicode_Check(key) && PyLong_Check(value))
          {
            char *str = NULL;
            Py_ssize_t len = 0;
            PyObject *bytes = PyUnicode_AsUTF8String(key);
            PyBytes_AsStringAndSize(bytes, &str, &len);

            if(str == NULL || len == 0)
            {
              RENDERDOC_LogMessage(LogType::Error, "QTRD", __FILE__, __LINE__, convert_error);
              errors_found = true;
            }
            else
            {
              constants.insert(std::string(str, str + len));
            }

            Py_DecRef(bytes);
          }
        }

        Py_DecRef(keys);

        if(!constants.empty())
        {
          std::set<std::string> documented;

          const char *docstring = typedoc.data();

          const char identifier[] = ".. data::";

          const char *datadoc = strstr(docstring, identifier);

          while(datadoc)
          {
            datadoc += sizeof(identifier) - 1;

            while(isspace(*datadoc))
              datadoc++;

            const char *eol = strchr(datadoc, '\n');

            if(!eol)
              break;

            documented.insert(std::string(datadoc, eol));

            datadoc = strstr(datadoc, identifier);
          }

          for(auto it = constants.begin(); it != constants.end(); ++it)
          {
            // allow enums with First or Count members to be undocumented
            if(*it == "First" || *it == "Count")
              continue;

            if(documented.find(*it) == documented.end())
            {
              snprintf(convert_error, sizeof(convert_error) - 1,
                       "'%s::%s' is not documented in class docstring", typeobj->tp_name,
                       it->c_str());
              RENDERDOC_LogMessage(LogType::Error, "QTRD", __FILE__, __LINE__, convert_error);
              errors_found = true;
            }
          }
        }
      }
    }

    PyMethodDef *method = typeobj->tp_methods;

    while(method->ml_doc)
    {
      std::string method_doc = method->ml_doc;

      size_t i = 0;
      while(method_doc[i] == '\n')
        i++;

      // skip the first line as it's autodoc generated
      i = method_doc.find('\n', i);
      if(i != std::string::npos)
      {
        while(method_doc[i] == '\n')
          i++;

        method_doc.erase(0, i);

        result = docstrings.insert(method_doc);

        if(!result.second)
        {
          snprintf(
              convert_error, sizeof(convert_error) - 1,
              "Duplicate docstring '%s' found on method '%s.%s' - are you missing a DOCUMENT()?",
              method_doc.c_str(), typeobj->tp_name, method->ml_name);
          RENDERDOC_LogMessage(LogType::Error, "QTRD", __FILE__, __LINE__, convert_error);
          errors_found = true;
        }
      }

      method++;
    }
  }

  if(errors_found)
    RENDERDOC_LogMessage(LogType::Fatal, "QTRD", __FILE__, __LINE__,
                         "Found errors in python binding docstrings. Please fix!");
}
