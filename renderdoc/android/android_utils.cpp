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

ABI GetABI(const std::string &abiName)
{
  if(abiName == "armeabi-v7a")
    return ABI::armeabi_v7a;
  else if(abiName == "arm64-v8a")
    return ABI::arm64_v8a;
  else if(abiName == "x86-v7a")
    return ABI::x86;
  else if(abiName == "x86_64")
    return ABI::x86_64;

  RDCWARN("Unknown or unsupported ABI %s", abiName.c_str());

  return ABI::unknown;
}

std::vector<ABI> GetSupportedABIs(const std::string &deviceID)
{
  std::string adbAbi = trim(adbExecCommand(deviceID, "shell getprop ro.product.cpu.abi").strStdout);

  // these returned lists should be such that the first entry is the 'lowest command denominator' -
  // typically 32-bit.
  switch(GetABI(adbAbi))
  {
    case ABI::arm64_v8a: return {ABI::armeabi_v7a, ABI::arm64_v8a};
    case ABI::armeabi_v7a: return {ABI::armeabi_v7a};
    case ABI::x86_64: return {ABI::x86, ABI::x86_64};
    case ABI::x86: return {ABI::x86};
    default: break;
  }

  return {};
}

std::string GetRenderDocPackageForABI(ABI abi, char sep)
{
  std::string ret = RENDERDOC_ANDROID_PACKAGE_BASE;
  ret += sep;

  switch(abi)
  {
    case ABI::arm64_v8a: return ret + "arm64";
    case ABI::armeabi_v7a: return ret + "arm32";
    case ABI::x86_64: return ret + "x64";
    case ABI::x86: return ret + "x86";
    default: break;
  }

  return ret + "unknown";
}

std::string GetPathForPackage(const std::string &deviceID, const std::string &packageName)
{
  std::string pkgPath = trim(adbExecCommand(deviceID, "shell pm path " + packageName).strStdout);

  // if there are multiple slices, the path will be returned on many lines. Take only the first
  // line, assuming all of the apks are in the same directory
  if(pkgPath.find("\n") != std::string::npos)
  {
    std::vector<std::string> lines;
    split(pkgPath, lines, '\n');
    pkgPath = lines[0];
  }

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