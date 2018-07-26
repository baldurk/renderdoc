/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2017-2018 Baldur Karlsson
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

#include <string>
#include "os/os_specific.h"

// public interface, for other non-android parts of the code
namespace Android
{
bool IsHostADB(const char *hostname);
ExecuteResult StartAndroidPackageForCapture(const char *host, const char *package,
                                            const char *intentArgs, const CaptureOptions &opts);
void ResetCaptureSettings(const std::string &deviceID);
void ExtractDeviceIDAndIndex(const std::string &hostname, int &index, std::string &deviceID);
Process::ProcessResult adbExecCommand(const std::string &deviceID, const std::string &args,
                                      const string &workDir = ".", bool silent = false);
void shutdownAdb();
bool InjectWithJDWP(const std::string &deviceID, uint16_t jdwpport);
};
