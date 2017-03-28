// this file is included from renderdoc.i, it's not a module in itself

%header %{
  #include <set>
%}

%init %{
  // verify that docstrings aren't duplicated, which is a symptom of missing DOCUMENT()
  // macros around newly added classes/members.
  #if !defined(RELEASE)
  static bool doc_checked = false;

  if(!doc_checked)
  {
    doc_checked = true;

    std::set<std::string> docstrings;
    for(size_t i=0; i < sizeof(swig_type_initial)/sizeof(swig_type_initial[0]); i++)
    {
      SwigPyClientData *typeinfo = (SwigPyClientData *)swig_type_initial[i]->clientdata;

      // opaque types have no typeinfo, skip these
      if(!typeinfo) continue;

      PyTypeObject *typeobj = typeinfo->pytype;

      std::string typedoc = typeobj->tp_doc;

      auto result = docstrings.insert(typedoc);

      if(!result.second)
      {
        snprintf(convert_error, sizeof(convert_error)-1, "Duplicate docstring '%s' found on struct '%s' - are you missing a DOCUMENT()?", typedoc.c_str(), typeobj->tp_name);
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
            snprintf(convert_error, sizeof(convert_error)-1, "Duplicate docstring '%s' found on method '%s' - are you missing a DOCUMENT()?", typedoc.c_str(), method->ml_name);
            RENDERDOC_LogMessage(LogType::Fatal, "QTRD", __FILE__, __LINE__, convert_error);
          }
        }

        method++;
      }
    }
  }
  #endif
%}
