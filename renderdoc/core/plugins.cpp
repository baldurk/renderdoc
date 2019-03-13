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

#include "core/plugins.h"
#include "os/os_specific.h"
#include "strings/string_utils.h"

std::string LocatePluginFile(const std::string &path, const std::string &fileName)
{
  std::string ret;

  std::string libpath;
  FileIO::GetLibraryFilename(libpath);
  libpath = get_dirname(libpath);

  std::vector<std::string> paths;

#if defined(RENDERDOC_PLUGINS_PATH)
  string customPath(RENDERDOC_PLUGINS_PATH);

  if(FileIO::IsRelativePath(customPath))
    customPath = libpath + "/" + customPath;

  paths.push_back(customPath);
#endif

  // windows installation
  paths.push_back(libpath + "/plugins");
  // linux installation
  paths.push_back(libpath + "/../share/renderdoc/plugins");
// also search the appropriate OS-specific location in the root
#if ENABLED(RDOC_WIN32) && ENABLED(RDOC_X64)
  paths.push_back(libpath + "/../../plugins-win64");
#endif

#if ENABLED(RDOC_WIN32) && DISABLED(RDOC_X64)
  paths.push_back(libpath + "/../../plugins-win32");
#endif

#if ENABLED(RDOC_LINUX)
  paths.push_back(libpath + "/../../plugins-linux64");
#endif

  // there is no standard path for local builds as we don't provide these plugins in the repository
  // directly. As a courtesy we search the root of the build, from the executable. The user can
  // always put the plugins folder relative to the exe where it would be in an installation too.
  paths.push_back(libpath + "/../../plugins");

  // in future maybe we want to search a user-specific plugins folder? Like ~/.renderdoc/ on linux
  // or %APPDATA%/renderdoc on windows?

  for(uint32_t i = 0; i < paths.size(); i++)
  {
    std::string check = paths[i] + "/" + path + "/" + fileName;
    if(FileIO::exists(check.c_str()))
    {
      ret = check;
      break;
    }
  }

  // if we didn't find it anywhere, just try running it directly in case it's in the PATH
  if(ret.empty())
    ret = fileName;

  return ret;
}