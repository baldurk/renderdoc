/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2020-2024 Baldur Karlsson
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

#include "api/replay/rdcstr.h"
#include "common/common.h"
#include "common/globalconfig.h"
#include "os/os_specific.h"
#include "superluminal/PerformanceAPI_capi.h"

// use registry to locate superluminal DLL
#if ENABLED(RDOC_WIN32)
#include <shlwapi.h>
#endif

namespace Superluminal
{
PerformanceAPI_Functions funcTable = {};

void Init()
{
  void *module = NULL;

#if ENABLED(RDOC_WIN32)
  {
    HKEY key = NULL;
    LSTATUS ret = ERROR_SUCCESS;

    ret = RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Superluminal\\Performance", 0, KEY_READ, &key);

    if(ret == ERROR_SUCCESS)
    {
      DWORD size = 0;
      ret = RegGetValueA(key, NULL, "InstallDir", RRF_RT_ANY, NULL, NULL, &size);

      if(ret == ERROR_SUCCESS)
      {
        rdcstr path;
        path.resize(size);

        ret = RegGetValueA(key, NULL, "InstallDir", RRF_RT_ANY, NULL, path.data(), &size);

        if(ret == ERROR_SUCCESS)
        {
          path.trim();

          path += "\\API\\dll\\";

#if ENABLED(RDOC_X64)
          path += "x64\\";
#else
          path += "x86\\";
#endif

          path += "PerformanceAPI.dll";

          module = Process::LoadModule(path);
        }
      }

      RegCloseKey(key);
    }
  }
#else
// not supported. Yet!
#endif

  if(module == NULL)
    RDCEraseEl(funcTable);

  PerformanceAPI_GetAPI_Func GetAPI =
      (PerformanceAPI_GetAPI_Func)Process::GetFunctionAddress(module, "PerformanceAPI_GetAPI");

  int ret = 0;

  if(GetAPI)
    ret = GetAPI(PERFORMANCEAPI_VERSION, &funcTable);

  if(ret != 1)
    RDCEraseEl(funcTable);
}

void BeginProfileRange(const rdcstr &name)
{
  if(funcTable.BeginEvent)
    funcTable.BeginEvent("RenderDoc", name.c_str(), PERFORMANCEAPI_DEFAULT_COLOR);
}

void EndProfileRange()
{
  if(funcTable.EndEvent)
    funcTable.EndEvent();
}
};
