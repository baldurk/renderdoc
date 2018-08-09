/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2018 Google LLC
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

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include <array>
#include <string>
#include <utility>
#include <vector>

#include "common/common.h"
#include "core/core.h"
#include "driver/vulkan/vk_common.h"
#include "driver/vulkan/vk_resources.h"
#include "serialise/rdcfile.h"

namespace vk_cpp_codec
{
class CodeFile
{
protected:
  FILE *cpp = NULL;
  FILE *header = NULL;
  int cpp_lines = 0;
  int header_lines = 0;
  std::string cpp_name = "";
  std::string header_name = "";
  size_t bracket_count = 0;
  std::string spaces = "";
  std::string func_name = "";
  std::string directoryPath = "";

public:
  CodeFile(const std::string &dirPath, const std::string &file_name)
      : directoryPath(dirPath), func_name(file_name)
  {
  }

  virtual ~CodeFile() { CloseAll(); }
  const char *FormatLine(const char *fmt, bool new_line)
  {
    size_t length = strlen(fmt);
    // Assume there is always either one '{' or pairs of '{' and '}'.
    // If one '{' is present this will add two space indention.
    // If a pair is present, nothing will happen.
    // Pairs reflect structured objects like vectors {.x, .y, .z}
    int bracket = 0;
    for(size_t i = 0; i < length; i++)
    {
      if(fmt[i] == '{')
        bracket += 2;
      else if(fmt[i] == '}')
        bracket -= 2;
    }
    // Assert that the above assumption is correct (either braces are balanced, or there is one
    // extra opening or closing one).
    RDCASSERT(bracket == 0 || bracket == 2 || bracket == -2);
    if(bracket < 0)
      bracket_count += bracket;
    if(bracket_count < spaces.length())
    {
      spaces = spaces.substr(0, bracket_count);
    }
    else
    {
      if(bracket_count > spaces.length())
      {
        length = spaces.length();
        for(size_t i = length; i < bracket_count; i += 2)
          spaces += "  ";
      }
    }
    static std::string format;
    format = spaces + std::string(fmt);
    if(new_line)
      format += std::string("\n");
    cpp_lines += new_line ? 1 : 0;
    if(bracket > 0)
      bracket_count += bracket;
    return format.c_str();
  }

  CodeFile &Print(const char *fmt, ...)
  {
    va_list argptr;
    va_start(argptr, fmt);
    vfprintf(cpp, FormatLine(fmt, false), argptr);
    va_end(argptr);
    return *this;
  }

  CodeFile &PrintLn(const char *fmt, ...)
  {
    va_list argptr;
    va_start(argptr, fmt);
    vfprintf(cpp, FormatLine(fmt, true), argptr);
    va_end(argptr);
    return *this;
  }

  CodeFile &PrintLnH(const char *fmt, ...)
  {
    std::string format = std::string(fmt) + std::string("\n");
    va_list argptr;
    va_start(argptr, fmt);
    vfprintf(header, format.c_str(), argptr);
    va_end(argptr);
    header_lines++;
    return *this;
  }

  void CloseAll()
  {
    CloseCPP();
    CloseHeader();
  }

  void CloseHeader()
  {
    fprintf(header, "\n");
    FileIO::fclose(header);
    header = NULL;
    header_lines = 0;
  }

  void CloseCPP()
  {
    if(bracket_count >= 2)
    {
      RDCASSERT(bracket_count == 2);
      PrintLn("}");
    }
    // a good check for the code gen.
    RDCASSERT(bracket_count == 0);
    FileIO::fclose(cpp);
    cpp = NULL;
    cpp_lines = 0;
  }

  virtual void Open(const std::string &file_name)
  {
    std::string name = std::string("gen_") + file_name;
    header_name = name + std::string(".h");
    cpp_name = name + std::string(".cpp");
    std::string header_path = directoryPath + "/" + header_name;
    std::string cpp_path = directoryPath + "/" + cpp_name;
    FileIO::CreateParentDirectory(header_path);
    header = FileIO::fopen(header_path.c_str(), "wt");
    RDCASSERT(header != NULL);
    cpp = FileIO::fopen(cpp_path.c_str(), "wt");
    RDCASSERT(cpp != NULL);

    PrintLnH("#pragma once")
        .PrintLnH("#include \"common.h\"")
        .PrintLn("#include \"%s\"", header_name.c_str());
  }
};

class MultiPartCodeFile : public CodeFile
{
protected:
  int index = 0;

public:
  MultiPartCodeFile(const std::string &dir_path, const std::string &file_name)
      : CodeFile(dir_path, file_name)
  {
  }
  virtual ~MultiPartCodeFile() {}
  virtual void Open(const std::string &file_name)
  {
    std::string name = std::string("gen_") + file_name;
    cpp_name = name + std::string("_") + std::to_string(index) + std::string(".cpp");

    std::string cpp_path = directoryPath + "/" + cpp_name;
    FileIO::CreateParentDirectory(cpp_path);
    cpp = FileIO::fopen(cpp_path.c_str(), "wt");
    RDCASSERT(cpp != NULL);

    if(header == NULL)
    {
      header_name = name + std::string(".h");
      std::string header_path = directoryPath + "/" + header_name;
      header = FileIO::fopen(header_path.c_str(), "wt");
      RDCASSERT(header != NULL);
      PrintLnH("#pragma once")
          .PrintLnH("#include \"common.h\"")
          .PrintLnH("#include \"gen_variables.h\"");
    }

    PrintLnH("void %s_%d();", func_name.c_str(), index)
        .PrintLn("#include \"%s\"", header_name.c_str())
        .PrintLn("void %s_%d() {", func_name.c_str(), index);
  }

  void MultiPartSplit()
  {
    if(cpp_lines > 10000)
    {
      CloseCPP();
      index++;
      cpp_lines = 0;
      Open(func_name);
    }
  }

  int GetIndex() { return index; }
};

}    // namespace vk_cpp_codec