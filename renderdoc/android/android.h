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

#include <string>
#include "os/os_specific.h"

// public interface, for other non-android parts of the code
namespace Android
{
bool IsHostADB(const char *hostname);
std::string GetPackageName(const std::string &packageAndActivity);
std::string GetActivityName(const std::string &packageAndActivity);
ExecuteResult StartAndroidPackageForCapture(const char *host, const char *packageAndActivity,
                                            const char *intentArgs, const CaptureOptions &opts);
void ResetCaptureSettings(const std::string &deviceID);
void ExtractDeviceIDAndIndex(const std::string &hostname, int &index, std::string &deviceID);
Process::ProcessResult adbExecCommand(const std::string &deviceID, const std::string &args,
                                      const std::string &workDir = ".", bool silent = false);
void initAdb();
void shutdownAdb();
bool InjectWithJDWP(const std::string &deviceID, uint16_t jdwpport);

struct LogcatThread
{
  void Finish();

private:
  void Tick();

  void SelfDelete()
  {
    // only delete ourselves once the thread has closed and we've called Finish()
    if(finishTime && thread == 0)
      delete this;
  }

  // lock for accessing finishTime or immediateExit
  Threading::CriticalSection lock;

  // the time we were asked to finish - we'll hang around for a few seconds longer to catch any
  // remaining output then exit
  time_t finishTime = 0;

  // immediately exit. This only happens when there's another thread wanting to start monitoring
  // logcat, so we should stop hanging around.
  bool immediateExit = false;

  // the last log line we saw, so we start printing after that point. If we ever have a line here
  // and we don't see it in the backlog.
  std::string lastLogcatLine;

  // the device ID we're monitoring
  std::string deviceID;

  // the thread handle
  Threading::ThreadHandle thread = 0;

  friend LogcatThread *ProcessLogcat(std::string deviceID);
};

void TickDeviceLogcat();
LogcatThread *ProcessLogcat(std::string deviceID);
};
