/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2018-2019 Baldur Karlsson
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
#include <algorithm>
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

std::string GetPackageName(const std::string &packageAndActivity)
{
  if(packageAndActivity.empty())
    return "";

  size_t start = 0;
  if(packageAndActivity[0] == '/')
    start++;

  size_t activitySep = packageAndActivity.find('/', start);

  if(activitySep == std::string::npos)
    return packageAndActivity.substr(start);

  return packageAndActivity.substr(start, activitySep - start);
}

std::string GetActivityName(const std::string &packageAndActivity)
{
  if(packageAndActivity.empty())
    return "";

  size_t start = 0;
  if(packageAndActivity[0] == '/')
    start++;

  size_t activitySep = packageAndActivity.find('/', start);

  if(activitySep == std::string::npos)
    return "";

  return packageAndActivity.substr(activitySep + 1);
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

bool IsSupported(std::string deviceID)
{
  std::string api =
      trim(Android::adbExecCommand(deviceID, "shell getprop ro.build.version.sdk").strStdout);

  int apiVersion = atoi(api.c_str());

  // SDK 23 == Android 6.0, our minimum spec. Only fail if we did parse an SDK string, in case some
  // Android devices don't support the query - we assume they are new enough.
  if(apiVersion >= 0 && apiVersion < 23)
  {
    RDCWARN("Device '%s' is on api version %d which is not supported",
            GetFriendlyName(deviceID).c_str(), apiVersion);
    return false;
  }

  return true;
}

std::string GetFriendlyName(std::string deviceID)
{
  auto it = friendlyNameCache.find(deviceID);
  if(it != friendlyNameCache.end())
    return it->second;

  // run adb root now, so we hit any disconnection that we're going to before trying to connect.
  // If we can't be root, this is cheap, if we're already root, this is cheap, if we can be root
  // and this changes us it will block only the first time - and we expect this function to be
  // slow-ish.
  //
  // We do this here so that we sneakily take advantage of the above caching - otherwise we spam adb
  // root commands into the log
  Android::adbExecCommand(deviceID, "root");

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

  if(!IsSupported(deviceID))
    combined += " - (Android 5.x)";

  return combined;
}

// on android only when we hit this function we write a marker that isn't a standard log. The
// purpose is to always try and have a unique message in the last N lines so that we can detect if
// we ever lose messages.
void TickDeviceLogcat()
{
#if ENABLED(RDOC_ANDROID)
  static uint64_t freq = (uint64_t)Timing::GetTickFrequency();

  const uint64_t timeMS = uint64_t(Timing::GetTick() / freq);

  static uint64_t prevTimeMS = 0;

  // don't spam more than once every 100ms to avoid saturating our log
  if(timeMS > prevTimeMS + 100)
  {
    prevTimeMS = timeMS;
    OSUtility::WriteOutput(OSUtility::Output_DebugMon,
                           StringFormat::Fmt("__rdoc_internal_android_logcat %llu", timeMS).c_str());
  }
#endif
}

struct LogLine
{
  time_t timestamp = 0;
  uint32_t pid = 0;
  LogType logtype = LogType::Comment;
  std::string filename;
  uint32_t line_number = 0;
  std::string message;

  bool parse(const std::string &line)
  {
#define EXPECT_CHAR(c)                       \
  if(idx >= line.length() || line[idx] != c) \
    return false;                            \
  idx++;

    // Parse out mostly our own log files, but also output that looks like crash callstacks
    //
    // Example lines:
    //
    // clang-format off
    //
    // 0        1         2         3         4         5         6         7         8         9         10
    // 1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456
    // I/renderdoc( 1234): @1234567812345678@ RDOC 001234: [01:02:03]         filename.cpp( 123) - Log     - Hello
    //
    // F/libc    (11519): Fatal signal 11 (SIGSEGV), code 1, fault addr 0x4 in tid 11618 (FooBar), pid 11519 (blah)
    // F/DEBUG   (12061): backtrace:
    // F/DEBUG   (12061):     #00 pc 000485ec  /system/lib/libc.so (pthread_mutex_lock+1)
    // F/DEBUG   (12061):     #01 pc 00137449  /data/app/org.renderdoc.renderdoccmd.arm32==/lib/arm/libVkLayer_GLES_RenderDoc.so
    // F/DEBUG   (12061):     #02 pc 0013bbf1  /data/app/org.renderdoc.renderdoccmd.arm32==/lib/arm/libVkLayer_GLES_RenderDoc.so
    //
    // clang-format on
    //

    size_t idx = 0;

    // too short - minimum is 22 for prefix. Could be longer if PID is over 5 digits
    // saves on length checks below
    if(line.length() <= 20)
      return false;

    // skip past priority character
    idx++;

    EXPECT_CHAR('/');

    // we assume that the logcat filters have worked, so ignore the logcat tag here. Just check if
    // it's ours or not
    bool ownLog = !strncmp(&line[idx], "renderdoc", 9);
    while(idx < line.length() && line[idx] != '(')
      idx++;

    size_t tagEnd = idx;

    EXPECT_CHAR('(');

    // skip spaces
    while(idx < line.length() && isspace(line[idx]))
      idx++;

    // process this PID field - we'll override it with our own if this is one of our logs
    pid = 0;
    while(idx < line.length() && isdigit(line[idx]))
    {
      pid *= 10;
      pid += int(line[idx] - '0');
      idx++;
    }

    EXPECT_CHAR(')');
    EXPECT_CHAR(':');
    EXPECT_CHAR(' ');

    if(!ownLog)
    {
      // don't know anything more about the format, this whole thing is the message.
      message = line.substr(idx);

      // use current host time, it's not accurate but it's close enough.
      timestamp = Timing::GetUTCTime();

      filename = line.substr(2, tagEnd - 2);
      line_number = 0;

      logtype = LogType::Comment;
      switch(line[0])
      {
        case 'V':    // VERBOSE
        case 'D':    // DEBUG
          logtype = LogType::Debug;
          break;
        case 'I':    // INFO
          logtype = LogType::Comment;
          break;
        case 'W':    // WARN
          logtype = LogType::Warning;
          break;
        case 'E':    // ERROR
          logtype = LogType::Error;
          break;
        case 'F':    // FATAL
          logtype = LogType::Fatal;
          break;
        case 'S':    // SILENT
        default: logtype = LogType::Comment; break;
      }

      // remove any padding spaces
      while(!filename.empty() && isspace(filename.back()))
        filename.pop_back();

      // if adb gave us DOS newlines, remove the \r
      if(message.back() == '\r')
        message.pop_back();

      return true;
    }

    // skip past digits and '@', this field is only so that we don't ever get duplicates in the
    // output even if the same message is printed
    while(idx < line.length() && (line[idx] == '@' || isalnum(line[idx])))
      idx++;

    EXPECT_CHAR(' ');

    // should be at least 64 more characters
    if(idx + 64 > line.length())
      return false;

    if(strncmp(&line[idx], "RDOC ", 5))
      return false;

    idx += 5;

    pid = 0;
    while(idx < line.length() && isdigit(line[idx]))
    {
      pid *= 10;
      pid += int(line[idx] - '0');
      idx++;
    }

    EXPECT_CHAR(':');
    EXPECT_CHAR(' ');
    EXPECT_CHAR('[');

    // expect HH:MM:SS
    if(idx + 8 >= line.length())
      return false;

    // we only need the time part, so just take it from the epoch
    uint32_t h = 0, m = 0, s = 0;

    h = int(line[idx + 0] - '0') * 10 + int(line[idx + 1] - '0');
    m = int(line[idx + 3] - '0') * 10 + int(line[idx + 4] - '0');
    s = int(line[idx + 6] - '0') * 10 + int(line[idx + 7] - '0');

    if(line[idx + 2] != ':' || line[idx + 5] != ':')
      return false;

    timestamp = (h * 60 + m) * 60 + s;

    idx += 8;

    EXPECT_CHAR(']');

    while(idx < line.length() && line[idx] != '(')
    {
      filename.push_back(line[idx]);
      idx++;
    }

    // strip spaces
    while(!filename.empty() && isspace(filename[0]))
      filename.erase(0, 1);

    while(!filename.empty() && isspace(filename.back()))
      filename.pop_back();

    if(filename.empty())
      return false;

    EXPECT_CHAR('(');

    while(idx < line.length() && isspace(line[idx]))
      idx++;

    line_number = 0;
    while(idx < line.length() && isdigit(line[idx]))
    {
      line_number *= 10;
      line_number += int(line[idx] - '0');
      idx++;
    }

    EXPECT_CHAR(')');
    EXPECT_CHAR(' ');
    EXPECT_CHAR('-');
    EXPECT_CHAR(' ');

    std::string logtype_str;
    while(idx < line.length() && line[idx] != '-')
    {
      logtype_str.push_back(line[idx]);
      idx++;
    }

    // strip spaces
    while(!logtype_str.empty() && isspace(logtype_str[0]))
      logtype_str.erase(0);

    while(!logtype_str.empty() && isspace(logtype_str.back()))
      logtype_str.pop_back();

    if(logtype_str == "Debug")
      logtype = LogType::Debug;
    else if(logtype_str == "Log")
      logtype = LogType::Comment;
    else if(logtype_str == "Warning")
      logtype = LogType::Warning;
    else if(logtype_str == "Error")
      logtype = LogType::Error;
    else if(logtype_str == "Fatal")
      logtype = LogType::Fatal;
    else
      return false;

    EXPECT_CHAR('-');
    EXPECT_CHAR(' ');

    if(idx >= line.length())
      return false;

    message = line.substr(idx);

    // if adb gave us DOS newlines, remove the \r
    if(message.back() == '\r')
      message.pop_back();

#undef EXPECT_CHAR

    return true;
  }
};

// we need to keep track of logcat threads, so that if we start a new one up on a device before the
// old one has finished, we don't start overlapping and double-printing messages.
static Threading::CriticalSection logcatThreadLock;
static std::map<std::string, LogcatThread *> logcatThreads;

LogcatThread *ProcessLogcat(std::string deviceID)
{
  LogcatThread *ret = NULL;

  Threading::ThreadHandle joinThread = 0;

  // ensure any previous thread running is really finished
  {
    SCOPED_LOCK(logcatThreadLock);

    // if this thread is running, kill it immediately
    if(logcatThreads[deviceID])
    {
      joinThread = logcatThreads[deviceID]->thread;

      {
        SCOPED_LOCK(logcatThreads[deviceID]->lock);
        logcatThreads[deviceID]->immediateExit = true;
      }
    }
  }

  // if we had a thread to join, do so now. It will remove itself from the above map and
  // self-delete, but not detach the thread
  if(joinThread)
  {
    Threading::JoinThread(joinThread);
    Threading::CloseThread(joinThread);
  }

  // start a new thread to monitor this device's logcat
  ret = new LogcatThread;

  {
    SCOPED_LOCK(logcatThreadLock);
    logcatThreads[deviceID] = ret;
  }

  ret->deviceID = deviceID;
  ret->thread = Threading::CreateThread([ret]() {
    bool done = false;

    RDCDEBUG("Starting monitoring logcat on %s", ret->deviceID.c_str());

    while(!done)
    {
      // tick the logcat
      ret->Tick();

      // sleep 400ms, but in small chunks to let us respond to immediateExit more quickly
      for(int i = 0; i < 10; i++)
      {
        Threading::Sleep(40);

        time_t now = Timing::GetUTCTime();
        {
          SCOPED_LOCK(ret->lock);
          if(ret->immediateExit || (ret->finishTime && ret->finishTime + 5 < now))
          {
            done = true;
            break;
          }
        }
      }
    }

    RDCDEBUG("Stopping monitoring logcat on %s", ret->deviceID.c_str());

    bool detach = true;

    // we need to exit. Take the logcat thread lock first
    {
      SCOPED_LOCK(logcatThreadLock);

      // if our immediateExit flag is set then we shouldn't detach, as the above code will join
      // with this thread to be sure we're done
      {
        SCOPED_LOCK(ret->lock);
        detach = !ret->immediateExit;
      }

      // remove ourselves from the map now, so that as soon as the lock is released the above code
      // can safely check for no overlap (we won't tick again so even if the threads overlap the
      // processing won't).
      logcatThreads[ret->deviceID] = NULL;
    }

    // if we should detach because no-one is going to join us, do that now
    if(detach)
      Threading::DetachThread(ret->thread);

    ret->thread = 0;

    // finally see if we can delete the struct
    ret->SelfDelete();
  });

  return ret;
}

void LogcatThread::Finish()
{
  SCOPED_LOCK(lock);
  finishTime = Timing::GetUTCTime();
  SelfDelete();
}

void LogcatThread::Tick()
{
  // adb is extremely unreliable, so although it supposedly contains functionality to filter for
  // everything after a certain timestamp, this can actually drop messages. Instead we just always
  // grab the last 750 lines and hope that the device doesn't ever peak over 1 line per millisecond
  // such that we'd miss something. Note another joy of adb - the line count is applied *before*
  // the filter, so if something else spams 1000 lines we won't see our own.
  const uint32_t lineBacklog = 750;

  // logcat
  //    -t N         // always the last N messages, and (implied -d) stop after doing so
  //    -v brief     // print the 'brief' format
  //    -s           // silence everything as a default
  //    renderdoc:*  // print logcats from our tag.
  //    libc:*       // or from libc (prints crash messages)
  //    DEBUG:*      // or from DEBUG (prints crash messages)
  //
  // This gives us all messages from renderdoc since the last timestamp.
  std::string command =
      StringFormat::Fmt("logcat -t %u -v brief -s renderdoc:* libc:* DEBUG:*", lineBacklog);

  std::string logcat = trim(Android::adbExecCommand(deviceID, command, ".", true).strStdout);

  std::vector<std::string> lines;
  split(logcat, lines, '\n');

  // remove \n from any lines right now to prevent it breaking further processing
  for(std::string &line : lines)
    if(!line.empty() && line.back() == '\r')
      line.pop_back();

  // only do any processing if we had a line last time that we know to start from.
  if(!lastLogcatLine.empty())
  {
    auto it = std::find(lines.begin(), lines.end(), lastLogcatLine);

    if(it != lines.end())
    {
      // remove everything up to and including that line
      lines.erase(lines.begin(), it + 1);
    }
    else
    {
      RDCWARN("Couldn't find last line. Potentially missed logcat messages.");
    }

    for(const std::string &line : lines)
    {
      LogLine logline;

      if(logline.parse(line))
      {
        rdclog_direct(logline.timestamp, logline.pid, logline.logtype, "ADRD",
                      logline.filename.c_str(), logline.line_number, "%s", logline.message.c_str());
        rdclog_flush();
      }
    }
  }

  // store the last line (if we have one) to search for and start from next time
  if(!lines.empty())
    lastLogcatLine = lines.back();
}
};

template <>
rdcstr DoStringise(const Android::ABI &el)
{
  BEGIN_ENUM_STRINGISE(Android::ABI);
  {
    STRINGISE_ENUM_CLASS(unknown);
    STRINGISE_ENUM_CLASS(armeabi_v7a);
    STRINGISE_ENUM_CLASS(arm64_v8a);
    STRINGISE_ENUM_CLASS(x86);
    STRINGISE_ENUM_CLASS(x86_64);
  }
  END_ENUM_STRINGISE();
}

#if ENABLED(ENABLE_UNIT_TESTS)

#undef None

#include "3rdparty/catch/catch.hpp"

TEST_CASE("Test that log line parsing is robust", "[android]")
{
  using namespace Android;

  SECTION("Empty string")
  {
    LogLine line;

    CHECK(line.parse("") == false);
  };

  SECTION("Invalid strings")
  {
    LogLine line1;

    CHECK(line1.parse("--------- beginning of main") == false);

    LogLine line2;

    CHECK(
        line2.parse("Lorem ipsum dolor sit amet, consectetur adipiscing elit. Nullam lacus lacus, "
                    "egestas vitae elementum sit amet, venenatis ac nunc.") == false);
  };

  SECTION("Valid strings")
  {
    LogLine line;

    const char *valid_text =
        R"(I/renderdoc( 1234): @1234567812345678@ RDOC 001234: [01:02:03]         filename.cpp( 123) - Warning - Hello)";

    CHECK(line.parse(valid_text) == true);

    CHECK(line.filename == "filename.cpp");
    CHECK(line.line_number == 123);
    CHECK((line.logtype == LogType::Warning));
    CHECK(line.message == "Hello");
    CHECK(line.pid == 1234);
    CHECK(line.timestamp == 3723);

    LogLine highpid;

    const char *highpid_text =
        R"(I/renderdoc(12345678): @1234567812345678@ RDOC 12345678: [01:02:03]         filename.cpp( 123) - Warning - Hello)";

    CHECK(highpid.parse(highpid_text) == true);

    CHECK(highpid.filename == "filename.cpp");
    CHECK(highpid.line_number == 123);
    CHECK((highpid.logtype == LogType::Warning));
    CHECK(highpid.message == "Hello");
    CHECK(highpid.pid == 12345678);
    CHECK(highpid.timestamp == 3723);

    LogLine longname;

    const char *longname_text =
        R"(I/renderdoc( 1234): @1234567812345678@ RDOC 001234: [01:02:03] a_very_long_source_filename.cpp( 123) - Warning - Hello)";

    CHECK(longname.parse(longname_text) == true);

    CHECK(longname.filename == "a_very_long_source_filename.cpp");
    CHECK(longname.line_number == 123);
    CHECK((longname.logtype == LogType::Warning));
    CHECK(longname.message == "Hello");
    CHECK(longname.pid == 1234);
    CHECK(longname.timestamp == 3723);

    LogLine longlinenum;

    const char *longlinenum_text =
        R"(I/renderdoc( 1234): @1234567812345678@ RDOC 001234: [01:02:03]         filename.cpp(12345678) - Warning - Hello)";

    CHECK(longlinenum.parse(longlinenum_text) == true);

    CHECK(longlinenum.filename == "filename.cpp");
    CHECK(longlinenum.line_number == 12345678);
    CHECK((longlinenum.logtype == LogType::Warning));
    CHECK(longlinenum.message == "Hello");
    CHECK(longlinenum.pid == 1234);
    CHECK(longlinenum.timestamp == 3723);
  };

  SECTION("Invalid strings - truncated")
  {
    std::string truncated =
        R"(I/renderdoc( 1234): @1234567812345678@ RDOC 001234: [01:02:03]         filename.cpp( 123) - Warning - H)";

    LogLine working;
    CHECK(working.parse(truncated) == true);

    while(!truncated.empty())
    {
      truncated.pop_back();

      LogLine broken;
      CHECK(broken.parse(truncated) == false);
    }
  };
}

#endif
