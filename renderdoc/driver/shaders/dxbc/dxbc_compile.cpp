/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2020 Baldur Karlsson
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

#include "dxbc_compile.h"
#include "common/common.h"
#include "os/os_specific.h"
#include "strings/string_utils.h"

// gives us an address to identify this dll with
static int dllLocator = 0;

HMODULE GetD3DCompiler()
{
  static HMODULE ret = NULL;
  if(ret != NULL)
    return ret;

  // dlls to try in priority order
  const char *dlls[] = {
      "d3dcompiler_47.dll", "d3dcompiler_46.dll", "d3dcompiler_45.dll",
      "d3dcompiler_44.dll", "d3dcompiler_43.dll",
  };

  for(int i = 0; i < 2; i++)
  {
    for(int d = 0; d < ARRAY_COUNT(dlls); d++)
    {
      if(i == 0)
        ret = GetModuleHandleA(dlls[d]);
      else
        ret = LoadLibraryA(dlls[d]);

      if(ret != NULL)
        return ret;
    }
  }

  // all else failed, couldn't find d3dcompiler loaded,
  // and couldn't even loadlibrary any version!
  // we'll have to loadlibrary the version that ships with
  // RenderDoc.

  rdcstr dllFile;
  FileIO::GetLibraryFilename(dllFile);

  rdcstr dll = get_dirname(dllFile) + "/d3dcompiler_47.dll";

  return LoadLibraryW(StringFormat::UTF82Wide(dll.c_str()).data());
}
