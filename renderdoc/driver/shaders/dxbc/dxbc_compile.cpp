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

#include "dxbc_compile.h"
#include "common/common.h"
#include "core/core.h"
#include "os/os_specific.h"
#include "strings/string_utils.h"

static HMODULE GetLocalD3DCompiler()
{
  rdcstr dllFile;
  FileIO::GetLibraryFilename(dllFile);

  rdcstr dll = get_dirname(dllFile) + "/d3dcompiler_47.dll";

  return LoadLibraryW(StringFormat::UTF82Wide(dll).data());
}

HMODULE GetD3DCompiler()
{
  static HMODULE ret = NULL;
  if(ret != NULL)
    return ret;

  // during replay, try to load our local one to get the newest possible.
  if(RenderDoc::Inst().IsReplayApp())
  {
    ret = GetLocalD3DCompiler();
    if(ret)
      return ret;
  }

  // now dlls to try in priority order
  const char *dlls[] = {
      "d3dcompiler_47.dll", "d3dcompiler_46.dll", "d3dcompiler_45.dll",
      "d3dcompiler_44.dll", "d3dcompiler_43.dll",
  };

  for(int i = 0; i < 2; i++)
  {
    for(int d = 0; d < ARRAY_COUNT(dlls); d++)
    {
      // first time around, try to load one that already exists. Second time around try to load it
      // in the default search path.
      if(i == 0)
        ret = GetModuleHandleA(dlls[d]);
      else
        ret = LoadLibraryA(dlls[d]);

      if(ret != NULL)
        return ret;
    }
  }

  // finally if we couldn't load a library anywhere from the system while capturing, load our local
  // compiler.
  ret = GetLocalD3DCompiler();

  return ret;
}
