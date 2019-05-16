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

#pragma once

#include "android.h"

// internal functions, shouldn't be used outside the android implementation - anything public goes
// in android.h

namespace Android
{
Process::ProcessResult execScript(const std::string &script, const std::string &args,
                                  const std::string &workDir = ".", bool silent = false);
Process::ProcessResult execCommand(const std::string &exe, const std::string &args,
                                   const std::string &workDir = ".", bool silent = false);

enum class ToolDir
{
  None,
  Java,
  BuildTools,
  BuildToolsLib,
  PlatformTools,
};
std::string getToolPath(ToolDir subdir, const std::string &toolname, bool checkExist);
bool toolExists(const std::string &path);

std::string GetFirstMatchingLine(const std::string &haystack, const std::string &needle);

bool IsSupported(std::string deviceID);
std::string GetFriendlyName(std::string deviceID);

// supported ABIs
enum class ABI
{
  unknown,
  armeabi_v7a,
  arm64_v8a,
  x86,
  x86_64,
};

ABI GetABI(const std::string &abiName);
std::vector<ABI> GetSupportedABIs(const std::string &deviceID);
std::string GetRenderDocPackageForABI(ABI abi, char sep = '.');
std::string GetPathForPackage(const std::string &deviceID, const std::string &packageName);

bool PatchManifest(std::vector<byte> &manifest);
};

DECLARE_REFLECTION_ENUM(Android::ABI);