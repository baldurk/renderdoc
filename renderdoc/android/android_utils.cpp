/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2018 Baldur Karlsson
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

#include "android_utils.h"
#include "core/core.h"
#include "strings/string_utils.h"

static std::map<std::string, std::string> friendlyNameCache;

namespace Android
{
bool IsHostADB(const char *hostname)
{
  return !strncmp(hostname, "adb:", 4);
}

void ExtractDeviceIDAndIndex(const std::string &hostname, int &index, std::string &deviceID)
{
  if(!IsHostADB(hostname.c_str()))
    return;

  const char *c = hostname.c_str();
  c += 4;

  index = atoi(c);

  c = strchr(c, ':');

  if(!c)
  {
    index = 0;
    return;
  }

  c++;

  deviceID = c;
}

std::string GetPathForPackage(const std::string &deviceID, const std::string &packageName)
{
  std::string pkgPath = trim(adbExecCommand(deviceID, "shell pm path " + packageName).strStdout);

  if(pkgPath.empty() || pkgPath.find("package:") != 0 || pkgPath.find("base.apk") == std::string::npos)
    return pkgPath;

  pkgPath.erase(pkgPath.begin(), pkgPath.begin() + strlen("package:"));
  pkgPath.erase(pkgPath.end() - strlen("base.apk"), pkgPath.end());

  return pkgPath;
}

std::string GetFriendlyName(std::string deviceID)
{
  auto it = friendlyNameCache.find(deviceID);
  if(it != friendlyNameCache.end())
    return it->second;

  std::string manuf =
      trim(Android::adbExecCommand(deviceID, "shell getprop ro.product.manufacturer").strStdout);
  std::string model =
      trim(Android::adbExecCommand(deviceID, "shell getprop ro.product.model").strStdout);

  std::string &combined = friendlyNameCache[deviceID];

  if(manuf.empty() && model.empty())
    combined = "";
  else if(manuf.empty() && !model.empty())
    combined = model;
  else if(!manuf.empty() && model.empty())
    combined = manuf + " device";
  else if(!manuf.empty() && !model.empty())
    combined = manuf + " " + model;

  return combined;
}
};