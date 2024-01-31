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

#pragma once

#include "api/replay/stringise.h"
#include "android.h"

// internal functions, shouldn't be used outside the android implementation - anything public goes
// in android.h

namespace Android
{
Process::ProcessResult execScript(const rdcstr &script, const rdcstr &args,
                                  const rdcstr &workDir = ".", bool silent = false);
Process::ProcessResult execCommand(const rdcstr &exe, const rdcstr &args,
                                   const rdcstr &workDir = ".", bool silent = false);

enum class ToolDir
{
  None,
  Java,
  BuildTools,
  BuildToolsLib,
  PlatformTools,
};
rdcstr getToolPath(ToolDir subdir, const rdcstr &toolname, bool checkExist);
bool toolExists(const rdcstr &path);

bool IsDebuggable(const rdcstr &deviceID, const rdcstr &packageName);
bool HasRootAccess(const rdcstr &deviceID);
rdcstr GetFirstMatchingLine(const rdcstr &haystack, const rdcstr &needle);

bool IsSupported(rdcstr deviceID);
bool SupportsNativeLayers(const rdcstr &deviceID);
rdcstr DetermineInstalledABI(const rdcstr &deviceID, const rdcstr &packageName);
rdcstr GetFriendlyName(const rdcstr &deviceID);

// supported ABIs
enum class ABI
{
  unknown,
  armeabi_v7a,
  arm64_v8a,
  x86,
  x86_64,
};

ABI GetABI(const rdcstr &abiName);
rdcstr GetPlainABIName(ABI abi);
rdcarray<ABI> GetSupportedABIs(const rdcstr &deviceID);
rdcstr GetRenderDocPackageForABI(ABI abi);
rdcstr GetPathForPackage(const rdcstr &deviceID, const rdcstr &packageName);
rdcstr GetFolderName(const rdcstr &deviceID);
};

DECLARE_REFLECTION_ENUM(Android::ABI);
