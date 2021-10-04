/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2021 Baldur Karlsson
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

#include "android.h"
#include <ctype.h>
#include <set>
#include "api/replay/version.h"
#include "common/formatting.h"
#include "common/threading.h"
#include "core/core.h"
#include "core/remote_server.h"
#include "core/settings.h"
#include "replay/replay_driver.h"
#include "strings/string_utils.h"
#include "android_utils.h"

RDOC_CONFIG(uint32_t, Android_MaxConnectTimeout, 30,
            "Maximum time in seconds to try connecting to the target app before giving up. "
            "Useful primarily for apps that take a very long time to start up.");

RDOC_CONFIG(bool, Android_Debug_ProcessLaunch, false,
            "Output verbose debug logging messages when launching android apps.");

namespace Android
{
void adbForwardPorts(uint16_t portbase, const rdcstr &deviceID, uint16_t jdwpPort, int pid,
                     bool silent)
{
  const char *forwardCommand = "forward tcp:%i localabstract:renderdoc_%i";

  adbExecCommand(deviceID,
                 StringFormat::Fmt(forwardCommand, portbase + RenderDoc_ForwardRemoteServerOffset,
                                   RenderDoc_RemoteServerPort),
                 ".", silent);
  adbExecCommand(deviceID,
                 StringFormat::Fmt(forwardCommand, portbase + RenderDoc_ForwardTargetControlOffset,
                                   RenderDoc_FirstTargetControlPort),
                 ".", silent);

  if(jdwpPort && pid)
    adbExecCommand(deviceID, StringFormat::Fmt("forward tcp:%hu jdwp:%i", jdwpPort, pid));
}

uint16_t GetJdwpPort()
{
  // we loop over a number of ports to try and avoid previous failed attempts from leaving sockets
  // open and messing with subsequent attempts
  const uint16_t portBase = 39500;

  static uint16_t portIndex = 0;

  portIndex++;
  portIndex %= 100;

  return portBase + portIndex;
}

rdcstr GetDefaultActivityForPackage(const rdcstr &deviceID, const rdcstr &packageName)
{
  Process::ProcessResult activity =
      adbExecCommand(deviceID, StringFormat::Fmt("shell cmd package resolve-activity"
                                                 " -c android.intent.category.LAUNCHER %s",
                                                 packageName.c_str()));

  if(activity.strStdout.empty())
  {
    RDCERR("Failed to resolve default activity of APK. STDERR: %s", activity.strStderror.c_str());
    return "";
  }

  rdcarray<rdcstr> lines;
  split(activity.strStdout, lines, '\n');

  for(rdcstr &line : lines)
  {
    line.trim();

    if(line.beginsWith("name=") && !line.contains("com.android"))
    {
      return line.substr(5);
    }
  }

  // when failed to find default activiy with cmd package on Android 6.0
  // try using pm dump like in this example:
  // $ adb shell pm dump com.android.gles3jni
  // DUMP OF SERVICE package:
  //  Activity Resolver Table:
  //    Non-Data Actions:
  //        android.intent.action.MAIN:
  //          d97b36a com.android.gles3jni/.GLES3JNIActivity filter fa39fb9
  // ...

  activity = adbExecCommand(deviceID, StringFormat::Fmt("shell pm dump %s", packageName.c_str()));

  lines.clear();
  split(activity.strStdout, lines, '\n');

  size_t numOfLines = lines.size();
  const char *intentFilter = "android.intent.action.MAIN:";

  for(size_t idx = 0; idx < numOfLines; idx++)
  {
    lines[idx].trim();

    if(lines[idx].beginsWith(intentFilter) && idx + 1 < numOfLines)
    {
      rdcstr activityName = lines[idx + 1].trimmed();
      int startPos = activityName.find('/');
      if(startPos < 0)
      {
        RDCWARN("Failed to find default activity");
        return "";
      }
      int endPos = activityName.find(' ', startPos + 1);
      if(endPos < 0)
        endPos = activityName.count();
      return activityName.substr(startPos + 1, endPos - startPos - 1);
    }
  }

  RDCERR("Didn't find default activity in adb output");
  return "";
}

rdcstr GetProcessNameForActivity(const rdcstr &deviceID, const rdcstr &packageName,
                                 const rdcstr &activityName)
{
  Process::ProcessResult activity =
      adbExecCommand(deviceID, StringFormat::Fmt("shell cmd package resolve-activity %s/%s",
                                                 packageName.c_str(), activityName.c_str()));

  if(activity.strStdout.empty())
  {
    RDCERR("Failed to resolve activity %s/%s. STDERR: %s", packageName.c_str(),
           activityName.c_str(), activity.strStderror.c_str());
    return packageName;
  }

  rdcarray<rdcstr> lines;
  split(activity.strStdout, lines, '\n');

  for(rdcstr &line : lines)
  {
    line.trim();

    if(line.beginsWith("processName="))
    {
      return line.substr(12);
    }
  }

  return packageName;
}

int GetCurrentPID(const rdcstr &deviceID, const rdcstr &processName)
{
  if(Android_Debug_ProcessLaunch())
  {
    RDCLOG("Getting PID from device %s for process '%s'", deviceID.c_str(), processName.c_str());
  }

  // try 5 times, 200ms apart to find the pid
  for(int i = 0; i < 5; i++)
  {
    Process::ProcessResult pidOutput =
        adbExecCommand(deviceID, StringFormat::Fmt("shell ps -A | grep %s", processName.c_str()));

    rdcstr &output = pidOutput.strStdout;

    output.trim();
    int space = output.find_first_of("\t ");

    if(Android_Debug_ProcessLaunch())
    {
      RDCLOG("Output from ps -A: '%s'", output.c_str());
    }

    // if we didn't get a response, try without the -A as some android devices don't support that
    // parameter
    if(output.empty() || output.find(processName) == -1 || space == -1)
    {
      pidOutput =
          adbExecCommand(deviceID, StringFormat::Fmt("shell ps | grep %s", processName.c_str()));

      output.trim();
      space = output.find_first_of("\t ");

      if(Android_Debug_ProcessLaunch())
      {
        RDCLOG("Output from ps: '%s'", output.c_str());
      }
    }

    if(Android_Debug_ProcessLaunch())
    {
      RDCLOG("Final output is '%s' and first space is at char %d", output.c_str(), space);
    }

    // if we still didn't get a response, sleep and try again next time
    if(output.empty() || output.find(processName) == -1 || space == -1)
    {
      if(Android_Debug_ProcessLaunch())
      {
        RDCLOG("Didn't get valid PID line, waiting");
      }

      Threading::Sleep(200);
      continue;
    }

    char *pid = &output[space];
    while(*pid == ' ' || *pid == '\t')
      pid++;

    if(Android_Debug_ProcessLaunch())
    {
      RDCLOG("Expecting PID starting at '%s'", pid);
    }

    char *end = pid;
    while(*end >= '0' && *end <= '9')
      end++;

    *end = 0;

    if(Android_Debug_ProcessLaunch())
    {
      RDCLOG("Truncated PID string: '%s'", pid);
    }

    int pidInt = atoi(pid);

    if(Android_Debug_ProcessLaunch())
    {
      RDCLOG("Parsed integer PID: %d", pidInt);
    }

    return pidInt;
  }

  if(Android_Debug_ProcessLaunch())
  {
    RDCLOG("Failed to get a PID after several retries");
  }

  return 0;
}

bool CheckAndroidServerVersion(const rdcstr &deviceID, ABI abi)
{
  // assume all servers are updated at the same rate. Only check first ABI's version
  rdcstr packageName = GetRenderDocPackageForABI(abi);
  RDCLOG("Checking installed version of %s on %s", packageName.c_str(), deviceID.c_str());

  rdcstr dump = adbExecCommand(deviceID, "shell pm dump " + packageName).strStdout;
  if(dump.empty())
    RDCERR("Unable to pm dump %s", packageName.c_str());

  rdcstr versionCode = GetFirstMatchingLine(dump, "versionCode=").trimmed();
  rdcstr versionName = GetFirstMatchingLine(dump, "versionName=").trimmed();

  // versionCode is not alone in this line, isolate it
  if(versionCode != "")
  {
    int32_t spaceOffset = versionCode.find(' ');
    if(spaceOffset >= 0)
      versionCode.erase(spaceOffset, ~0U);

    versionCode.erase(0, strlen("versionCode="));
  }
  else
  {
    RDCERR("Unable to determine versionCode for: %s", packageName.c_str());
  }

  if(versionName != "")
  {
    versionName.erase(0, strlen("versionName="));
  }
  else
  {
    RDCERR("Unable to determine versionName for: %s", packageName.c_str());
  }

  // Compare the server's versionCode and versionName with the host's for compatibility
  rdcstr hostVersionCode =
      rdcstr(STRINGIZE(RENDERDOC_VERSION_MAJOR)) + rdcstr(STRINGIZE(RENDERDOC_VERSION_MINOR));
  rdcstr hostVersionName = GitVersionHash;

  // False positives will hurt us, so check for explicit matches
  if((hostVersionCode == versionCode) && (hostVersionName == versionName))
  {
    RDCLOG("Installed server version (%s:%s) is compatible", versionCode.c_str(),
           versionName.c_str());
    return true;
  }

  RDCWARN("RenderDoc server versionCode:versionName (%s:%s) is incompatible with host (%s:%s)",
          versionCode.c_str(), versionName.c_str(), hostVersionCode.c_str(), hostVersionName.c_str());

  return false;
}

ReplayStatus InstallRenderDocServer(const rdcstr &deviceID)
{
  ReplayStatus status = ReplayStatus::Succeeded;

  rdcarray<ABI> abis = GetSupportedABIs(deviceID);

  if(abis.empty())
  {
    RDCERR("Couldn't determine supported ABIs for %s", deviceID.c_str());
    return ReplayStatus::AndroidABINotFound;
  }

  // Check known paths for RenderDoc server
  rdcstr libPath;
  FileIO::GetLibraryFilename(libPath);
  rdcstr libDir = get_dirname(FileIO::GetFullPathname(libPath));

  rdcarray<rdcstr> paths;

#if defined(RENDERDOC_APK_PATH)
  rdcstr customPath(RENDERDOC_APK_PATH);
#else
  rdcstr customPath;
#endif

  if(!customPath.empty())
  {
    RDCLOG("Custom APK path: %s", customPath.c_str());

    if(FileIO::IsRelativePath(customPath))
      customPath = libDir + "/" + customPath;

    if(customPath.back() != '/')
      customPath += '/';

    paths.push_back(customPath);
  }

  rdcstr suff = GetPlainABIName(abis[0]);

  paths.push_back(libDir + "/plugins/android/");                                  // Windows install
  paths.push_back(libDir + "/../share/renderdoc/plugins/android/");               // Linux install
  paths.push_back(libDir + "/../plugins/android/");                               // macOS install
  paths.push_back(libDir + "/../../build-android/bin/");                          // Local build
  paths.push_back(libDir + "/../../build-android-" + suff + "/bin/");             // Local ABI build
  paths.push_back(libDir + "/../../../../../build-android/bin/");                 // macOS build
  paths.push_back(libDir + "/../../../../../build-android-" + suff + "/bin/");    // macOS ABI build

  // use the first ABI for searching
  rdcstr apk = GetRenderDocPackageForABI(abis[0]);
  rdcstr apksFolder;

  for(uint32_t i = 0; i < paths.size(); i++)
  {
    RDCLOG("Checking for server APK in %s", paths[i].c_str());

    rdcstr apkpath = paths[i] + apk + ".apk";

    if(FileIO::exists(apkpath))
    {
      apksFolder = paths[i];
      RDCLOG("APKs found: %s", apksFolder.c_str());
      break;
    }
  }

  if(apksFolder.empty())
  {
    RDCERR(
        "APK folder missing! RenderDoc for Android will not work without it. "
        "Build your Android ABI in build-android in the root to have it "
        "automatically found and installed.");
    return ReplayStatus::AndroidAPKFolderNotFound;
  }

  for(ABI abi : abis)
  {
    apk = apksFolder;

    int abiSuffix = apk.find(suff);
    if(abiSuffix >= 0)
      apk.replace(abiSuffix, suff.size(), GetPlainABIName(abi));

    apk += GetRenderDocPackageForABI(abi) + ".apk";

    if(!FileIO::exists(apk))
      RDCWARN(
          "%s missing - ensure you build all ABIs your device can support for full compatibility",
          apk.c_str());

    rdcstr api =
        Android::adbExecCommand(deviceID, "shell getprop ro.build.version.sdk").strStdout.trimmed();

    int apiVersion = atoi(api.c_str());

    Process::ProcessResult adbInstall;
    if(apiVersion >= 30)
    {
      adbInstall = adbExecCommand(deviceID, "install -r -g --force-queryable \"" + apk + "\"");
    }
    else
    {
      adbInstall = adbExecCommand(deviceID, "install -r -g \"" + apk + "\"");
    }

    RDCLOG("Installed package '%s', checking for success...", apk.c_str());

    bool success = CheckAndroidServerVersion(deviceID, abi);

    if(!success)
    {
      RDCLOG("Failed to install APK. stdout: %s, stderr: %s",
             adbInstall.strStdout.trimmed().c_str(), adbInstall.strStderror.trimmed().c_str());
      RDCLOG("Retrying...");
      adbExecCommand(deviceID, "install -r \"" + apk + "\"");

      success = CheckAndroidServerVersion(deviceID, abi);

      if(success)
      {
        // if it succeeded this time, then it was the permission grant that failed
        status = ReplayStatus::AndroidGrantPermissionsFailed;
      }
      else
      {
        // otherwise something went wrong with verifying. If the install failed completely we'll
        // return AndroidAPKInstallFailed below, otherwise return a code indicating we couldn't
        // verify the install properly.
        status = ReplayStatus::AndroidAPKVerifyFailed;
      }
    }
  }

  // Ensure installation succeeded. We should have as many lines as abis we installed
  Process::ProcessResult adbCheck =
      adbExecCommand(deviceID, "shell pm list packages " RENDERDOC_ANDROID_PACKAGE_BASE);

  if(adbCheck.strStdout.empty())
  {
    RDCERR("Couldn't find any installed APKs. stderr: %s", adbCheck.strStderror.c_str());
    return ReplayStatus::AndroidAPKInstallFailed;
  }

  size_t lines = adbCheck.strStdout.find('\n') == -1 ? 1 : 2;

  if(lines != abis.size())
    RDCWARN("Installation of some apks failed!");

  return status;
}

bool RemoveRenderDocAndroidServer(const rdcstr &deviceID)
{
  rdcarray<ABI> abis = GetSupportedABIs(deviceID);

  if(abis.empty())
    return false;

  // remove the old package, if it's still there. Ignore any errors
  adbExecCommand(deviceID, "uninstall " RENDERDOC_ANDROID_PACKAGE_BASE);

  for(ABI abi : abis)
  {
    rdcstr packageName = GetRenderDocPackageForABI(abi);

    adbExecCommand(deviceID, "uninstall " + packageName);

    // Ensure uninstall succeeded
    rdcstr adbCheck = adbExecCommand(deviceID, "shell pm list packages " + packageName).strStdout;

    if(!adbCheck.empty())
    {
      RDCERR("Uninstall of %s failed!", packageName.c_str());
      return false;
    }
  }

  return true;
}

void ResetCaptureSettings(const rdcstr &deviceID)
{
  Android::adbExecCommand(deviceID, "shell setprop debug.vulkan.layers :", ".", true);
  Android::adbExecCommand(deviceID, "shell settings delete global enable_gpu_debug_layers", ".",
                          true);
  Android::adbExecCommand(deviceID, "shell settings delete global gpu_debug_app", ".", true);
  Android::adbExecCommand(deviceID, "shell settings delete global gpu_debug_layer_app", ".", true);
  Android::adbExecCommand(deviceID, "shell settings delete global gpu_debug_layers", ".", true);
  Android::adbExecCommand(deviceID, "shell settings delete global gpu_debug_layers_gles", ".", true);
}

rdcarray<rdcstr> EnumerateDevices()
{
  rdcarray<rdcstr> ret;

  rdcstr adbStdout = Android::adbExecCommand("", "devices", ".", true).strStdout;

  rdcarray<rdcstr> lines;
  split(adbStdout, lines, '\n');
  for(const rdcstr &line : lines)
  {
    rdcarray<rdcstr> tokens;
    split(line, tokens, '\t');
    if(tokens.size() == 2 && tokens[1].trimmed() == "device")
      ret.push_back(tokens[0]);
  }

  return ret;
}
};    // namespace Android

struct AndroidRemoteServer : public RemoteServer
{
  AndroidRemoteServer(Network::Socket *sock, const rdcstr &deviceID, uint16_t portbase)
      : RemoteServer(sock, deviceID), m_portbase(portbase)
  {
  }

  virtual ~AndroidRemoteServer() override
  {
    if(m_LogcatThread)
      m_LogcatThread->Finish();
  }

  virtual void ShutdownConnection() override;

  virtual void ShutdownServerAndConnection() override
  {
    ResetAndroidSettings();
    RemoteServer::ShutdownServerAndConnection();
  }

  virtual bool Ping() override
  {
    if(!Connected())
      return false;

    LazilyStartLogcatThread();

    return RemoteServer::Ping();
  }

  virtual rdcpair<ReplayStatus, IReplayController *> OpenCapture(
      uint32_t proxyid, const rdcstr &filename, const ReplayOptions &opts,
      RENDERDOC_ProgressCallback progress) override
  {
    ResetAndroidSettings();

    // enable profiling to measure hardware counters
    Android::adbExecCommand(m_deviceID, "shell setprop security.perf_harden 0");

    LazilyStartLogcatThread();

    return RemoteServer::OpenCapture(proxyid, filename, opts, progress);
  }

  virtual void CloseCapture(IReplayController *rend) override
  {
    // disable profiling
    Android::adbExecCommand(m_deviceID, "shell setprop security.perf_harden 1");

    RemoteServer::CloseCapture(rend);
  }

  virtual rdcstr GetHomeFolder() override { return ""; }
  virtual rdcarray<PathEntry> ListFolder(const rdcstr &path) override
  {
    if(path.empty() || (path[0] == '/' && path.size() == 1))
    {
      SCOPED_TIMER("Fetching android packages and activities");

      rdcstr adbStdout = Android::adbExecCommand(m_deviceID, "shell pm list packages -3").strStdout;

      rdcarray<rdcstr> lines;
      split(adbStdout, lines, '\n');
      for(rdcstr &line : lines)
        while(!line.empty() && isspace(line.back()))
          line.pop_back();

      rdcarray<PathEntry> packages;
      for(const rdcstr &line : lines)
      {
        // hide our own internal packages
        if(strstr(line.c_str(), "package:org.renderdoc."))
          continue;

        if(!strncmp(line.c_str(), "package:", 8))
        {
          PathEntry pkg;
          pkg.filename = line.substr(8).trimmed();
          pkg.size = 0;
          pkg.lastmod = 0;
          pkg.flags = PathProperty::Directory;

          packages.push_back(pkg);
        }
      }

      // also fetch the system packages but mark them as hidden folders
      adbStdout = Android::adbExecCommand(m_deviceID, "shell pm list packages -s").strStdout;

      split(adbStdout, lines, '\n');
      for(rdcstr &line : lines)
        while(!line.empty() && isspace(line.back()))
          line.pop_back();

      for(const rdcstr &line : lines)
      {
        if(!strncmp(line.c_str(), "package:", 8))
        {
          PathEntry pkg;
          pkg.filename = line.substr(8).trimmed();
          pkg.size = 0;
          pkg.lastmod = 0;
          pkg.flags = PathProperty::Directory | PathProperty::Hidden;

          packages.push_back(pkg);
        }
      }

      adbStdout = Android::adbExecCommand(m_deviceID, "shell dumpsys package").strStdout;

      split(adbStdout, lines, '\n');
      for(rdcstr &line : lines)
        while(!line.empty() && isspace(line.back()))
          line.pop_back();

      // not everything that looks like it's an activity is actually an activity, because of course
      // nothing is ever simple on Android. Watch out for the activity sections and only parse
      // activities found within them.

      bool activitySection = false;
      bool nonDataSection = false;

      for(const rdcstr &line : lines)
      {
        // if this is the start of the activity section, set the flag to true
        if(line.contains("Activity Resolver Table:"))
          activitySection = true;

        // the activity section ends when we reach a line that starts at column 0, which is the
        // start of a section. Reset the flag to false
        else if(!line.empty() && !isspace(line[0]))
          activitySection = false;

        // if this is the start of the non-data action section, set the flag to true
        if(line.contains("Non-Data Actions:"))
          nonDataSection = true;

        // a blank line indicates the end of the non-data action section
        else if(line.empty())
          nonDataSection = false;

        // if the flag is false, skip
        if(!activitySection || !nonDataSection)
          continue;

        // quick check, look for a /
        if(!line.contains('/'))
          continue;

        // line should be something like: '    78f9sba com.package.name/.NameOfActivity .....'

        const char *c = line.c_str();

        // expect whitespace
        while(*c && isspace(*c))
          c++;

        // expect hex
        while(*c && ((*c >= '0' && *c <= '9') || (*c >= 'a' && *c <= 'f')))
          c++;

        // expect space
        if(*c != ' ')
          continue;

        c++;

        // expect the package now. Search to see if it's one of the ones we listed above
        rdcstr package;

        for(const PathEntry &p : packages)
          if(!strncmp(c, p.filename.c_str(), p.filename.size()))
            package = p.filename;

        // didn't find a matching package
        if(package.empty())
          continue;

        c += package.size();

        // expect a /
        if(*c != '/')
          continue;

        c++;

        const char *end = strchr(c, ' ');

        if(end == NULL)
          end = c + strlen(c);

        while(isspace(*(end - 1)))
          end--;

        m_AndroidActivities.insert({package, rdcstr(c, end - c)});
      }

      return packages;
    }
    else
    {
      rdcstr package = path;

      if(!package.empty() && package[0] == '/')
        package.erase(0, 1);

      rdcarray<PathEntry> activities;

      for(const Activity &act : m_AndroidActivities)
      {
        if(act.package == package)
        {
          PathEntry activity;
          if(act.activity[0] == '.')
            activity.filename = package + act.activity;
          else
            activity.filename = act.activity;
          activity.size = 0;
          activity.lastmod = 0;
          activity.flags = PathProperty::Executable;
          activities.push_back(activity);
        }
      }

      PathEntry defaultActivity;
      defaultActivity.filename = "#DefaultActivity";
      defaultActivity.size = 0;
      defaultActivity.lastmod = 0;
      defaultActivity.flags = PathProperty::Executable;

      // if there's only one activity listed, assume it's the default and don't add a virtual
      // entry
      if(activities.size() != 1)
        activities.push_back(defaultActivity);

      return activities;
    }
  }

  virtual ExecuteResult ExecuteAndInject(const rdcstr &packageAndActivity, const rdcstr &,
                                         const rdcstr &intentArgs,
                                         const rdcarray<EnvironmentModification> &env,
                                         const CaptureOptions &opts) override;

private:
  void ResetAndroidSettings() { Android::ResetCaptureSettings(m_deviceID); }
  void LazilyStartLogcatThread()
  {
    if(m_LogcatThread)
      return;

    m_LogcatThread = Android::ProcessLogcat(m_deviceID);
  }

  uint16_t m_portbase = 0;
  Android::LogcatThread *m_LogcatThread = NULL;

  struct Activity
  {
    rdcstr package;
    rdcstr activity;

    bool operator<(const Activity &o) const
    {
      if(package != o.package)
        return package < o.package;
      return activity < o.activity;
    }
  };

  std::set<Activity> m_AndroidActivities;
};

struct AndroidController : public IDeviceProtocolHandler
{
  void Start()
  {
    if(running == 0)
    {
      Atomic::Inc32(&running);

      {
        SCOPED_LOCK(lock);
        Android::initAdb();
      }

      thread = Threading::CreateThread([]() { m_Inst.ThreadEntry(); });
      RenderDoc::Inst().RegisterShutdownFunction([]() { m_Inst.Shutdown(); });
    }
  }

  void Shutdown()
  {
    Atomic::Dec32(&running);
    Threading::JoinThread(thread);
    Threading::CloseThread(thread);
    thread = 0;

    {
      SCOPED_LOCK(lock);
      Android::shutdownAdb();
    }
  }

  struct Command
  {
    std::function<void()> meth;
    int32_t done = 0;
    bool selfdelete = false;
  };

  rdcarray<Command *> cmdqueue;

  void ThreadEntry()
  {
    Threading::SetCurrentThreadName("AndroidController");

    while(Atomic::CmpExch32(&running, 1, 1) == 1)
    {
      Threading::Sleep(5);
      Command *cmd = NULL;

      {
        SCOPED_LOCK(lock);
        if(cmdqueue.empty())
          continue;

        cmd = cmdqueue[0];
        cmdqueue.erase(0);
      }

      cmd->meth();

      Atomic::Inc32(&cmd->done);

      if(cmd->selfdelete)
        delete cmd;
    }
  }

  void Invoke(std::function<void()> method)
  {
    Command cmd;
    cmd.meth = method;

    {
      SCOPED_LOCK(lock);
      cmdqueue.push_back(&cmd);
    }

    while(Atomic::CmpExch32(&cmd.done, 0, 0) == 0)
      Threading::Sleep(5);
  }

  void AsyncInvoke(std::function<void()> method)
  {
    Command *cmd = new Command;
    cmd->meth = method;
    cmd->selfdelete = true;

    {
      SCOPED_LOCK(lock);
      cmdqueue.push_back(cmd);
    }
  }

  rdcstr GetProtocolName() override { return "adb"; }
  rdcarray<rdcstr> GetDevices() override
  {
    rdcarray<rdcstr> ret;

    Invoke([this, &ret]() {
      rdcarray<rdcstr> activedevices = Android::EnumerateDevices();

      // reset all devices to inactive
      for(auto it = devices.begin(); it != devices.end(); ++it)
        it->second.active = false;

      // process the list of active devices, find matches and activate them, or add a new entry
      for(const rdcstr &d : activedevices)
      {
        auto it = devices.find(d);
        if(it != devices.end())
        {
          it->second.active = true;

          // silently forward the ports now. These may be refreshed but this will allow us to
          // connect
          Android::adbForwardPorts(it->second.portbase, d, 0, 0, true);
          continue;
        }

        // not found - add a new device
        Device dev;
        dev.active = true;
        dev.name = Android::GetFriendlyName(d);
        if(!Android::IsSupported(d))
          dev.name += " - (Android 5.x)";
        dev.portbase =
            uint16_t(RenderDoc_ForwardPortBase +
                     RenderDoc::Inst().GetForwardedPortSlot() * RenderDoc_ForwardPortStride);

        // silently forward the ports now. These may be refreshed but this will allow us to connect
        Android::adbForwardPorts(dev.portbase, d, 0, 0, true);

        devices[d] = dev;
      }

      for(auto it = devices.begin(); it != devices.end(); ++it)
      {
        if(it->second.active)
          ret.push_back(it->first);
      }
    });

    return ret;
  }

  rdcstr GetFriendlyName(const rdcstr &URL) override
  {
    rdcstr ret;

    {
      SCOPED_LOCK(lock);
      ret = devices[GetDeviceID(URL)].name;
    }

    return ret;
  }

  bool SupportsMultiplePrograms(const rdcstr &URL) override { return false; }
  bool IsSupported(const rdcstr &URL) override
  {
    bool ret = false;

    {
      SCOPED_LOCK(lock);
      ret = Android::IsSupported(GetDeviceID(URL));
    }

    return ret;
  }

  ReplayStatus StartRemoteServer(const rdcstr &URL) override
  {
    ReplayStatus status = ReplayStatus::Succeeded;

    Invoke([this, &status, URL]() {
      rdcstr deviceID = GetDeviceID(URL);

      Device &dev = devices[deviceID];

      if(!dev.active)
      {
        status = ReplayStatus::InternalError;
        return;
      }

      rdcstr packagesOutput =
          Android::adbExecCommand(deviceID,
                                  "shell pm list packages " RENDERDOC_ANDROID_PACKAGE_BASE)
              .strStdout.trimmed();

      rdcarray<rdcstr> packages;
      split(packagesOutput, packages, '\n');

      rdcarray<Android::ABI> abis = Android::GetSupportedABIs(deviceID);

      RDCLOG("Starting RenderDoc server, supported ABIs:");
      for(Android::ABI abi : abis)
        RDCLOG("  - %s", ToStr(abi).c_str());

      if(abis.empty())
      {
        status = ReplayStatus::AndroidABINotFound;
        return;
      }

      // assume all servers are updated at the same rate. Only check first ABI's version
      if(packages.size() != abis.size() || !Android::CheckAndroidServerVersion(deviceID, abis[0]))
      {
        // if there was any existing package, remove it
        if(!packages.empty())
        {
          if(Android::RemoveRenderDocAndroidServer(deviceID))
            RDCLOG("Uninstall of old server succeeded");
          else
            RDCERR("Uninstall of old server failed");
        }

        // If server is not detected or has been removed due to incompatibility, install it
        status = Android::InstallRenderDocServer(deviceID);
        if(status != ReplayStatus::Succeeded &&
           status != ReplayStatus::AndroidGrantPermissionsFailed &&
           status != ReplayStatus::AndroidAPKVerifyFailed)
        {
          RDCERR("Failed to install RenderDoc server app");
          return;
        }
      }

      // stop all servers of any ABI
      for(Android::ABI abi : abis)
        Android::adbExecCommand(deviceID, "shell am force-stop " + GetRenderDocPackageForABI(abi));

      Android::adbForwardPorts(dev.portbase, deviceID, 0, 0, false);
      Android::ResetCaptureSettings(deviceID);

      // make Oculus' on device vulkan validation layer available for load
      Android::adbExecCommand(
          deviceID,
          "shell setprop debug.oculus.usepackagedvvl." RENDERDOC_ANDROID_PACKAGE_BASE ".arm32 1");
      Android::adbExecCommand(
          deviceID,
          "shell setprop debug.oculus.usepackagedvvl." RENDERDOC_ANDROID_PACKAGE_BASE ".arm64 1");

      rdcstr package = GetRenderDocPackageForABI(abis.back());

      rdcstr folderName = Android::GetFolderName(deviceID);

      // push settings file into our folder
      Android::adbExecCommand(deviceID, "push \"" + FileIO::GetAppFolderFilename("renderdoc.conf") +
                                            "\" /sdcard/Android/" + folderName + package +
                                            "/files/renderdoc.conf");

      // launch the last ABI, as the 64-bit version where possible, or 32-bit version where not.
      // Captures are portable across bitness and in some cases a 64-bit capture can't replay on a
      // 32-bit remote server.
      Android::adbExecCommand(
          deviceID, "shell am start -n " + package + "/.Loader -e renderdoccmd remoteserver");
    });

    // allow the package to start and begin listening before we return
    Threading::Sleep(1500);

    return status;
  }

  rdcstr RemapHostname(const rdcstr &deviceID) override
  {
    // we always connect to localhost
    return "127.0.0.1";
  }

  uint16_t RemapPort(const rdcstr &deviceID, uint16_t srcPort) override
  {
    uint16_t portbase = 0;

    {
      SCOPED_LOCK(lock);
      portbase = devices[deviceID].portbase;
    }

    if(portbase == 0)
      return 0;

    if(srcPort == RenderDoc_RemoteServerPort)
      return portbase + RenderDoc_ForwardRemoteServerOffset;
    // we only support a single target control connection on android
    else if(srcPort == RenderDoc_FirstTargetControlPort)
      return portbase + RenderDoc_ForwardTargetControlOffset;

    return 0;
  }

  IRemoteServer *CreateRemoteServer(Network::Socket *sock, const rdcstr &deviceID) override
  {
    uint16_t portbase = 0;

    {
      SCOPED_LOCK(lock);
      portbase = devices[deviceID].portbase;
    }

    return new AndroidRemoteServer(sock, deviceID, portbase);
  }

  int32_t running = 0;
  struct Device
  {
    rdcstr name;
    uint16_t portbase;
    bool active;
  };
  std::map<rdcstr, Device> devices;
  Threading::CriticalSection lock;
  Threading::ThreadHandle thread;
  static AndroidController m_Inst;

  static IDeviceProtocolHandler *Get()
  {
    m_Inst.Start();
    return &m_Inst;
  };
};

void AndroidRemoteServer::ShutdownConnection()
{
  rdcstr deviceID = m_deviceID;
  AndroidController::m_Inst.AsyncInvoke([deviceID]() { Android::ResetCaptureSettings(deviceID); });
  RemoteServer::ShutdownConnection();
}

ExecuteResult AndroidRemoteServer::ExecuteAndInject(const rdcstr &packageAndActivity,
                                                    const rdcstr &, const rdcstr &intentArgs,
                                                    const rdcarray<EnvironmentModification> &env,
                                                    const CaptureOptions &opts)
{
  LazilyStartLogcatThread();

  // we spin up a thread to Ping() every second, since starting a package can block for a long time.
  int32_t done = 0;
  Threading::ThreadHandle pingThread = Threading::CreateThread([&done, this]() {
    Threading::SetCurrentThreadName("Android Ping");

    bool ok = true;
    while(ok && Atomic::CmpExch32(&done, 0, 0) == 0)
      ok = Ping();
  });

  ExecuteResult ret;

  AndroidController::m_Inst.Invoke([this, &ret, packageAndActivity, intentArgs, opts]() {
    ret.status = ReplayStatus::UnknownError;
    ret.ident = RenderDoc_FirstTargetControlPort;

    rdcstr packageName =
        Android::GetPackageName(packageAndActivity);    // Remove leading '/' if any

    // adb shell cmd package resolve-activity -c android.intent.category.LAUNCHER com.jake.cube1
    rdcstr activityName = Android::GetActivityName(packageAndActivity);

    // if the activity name isn't specified, get the default one
    if(activityName.empty() || activityName == "#DefaultActivity")
      activityName = Android::GetDefaultActivityForPackage(m_deviceID, packageName);

    rdcstr processName = Android::GetProcessNameForActivity(m_deviceID, packageName, activityName);

    if(Android_Debug_ProcessLaunch())
    {
      RDCLOG("Launching package '%s' with activity '%s' and process name '%s'", packageName.c_str(),
             activityName.c_str(), processName.c_str());
    }

    uint16_t jdwpPort = Android::GetJdwpPort();

    // remove any previous jdwp port forward on this port
    Android::adbExecCommand(m_deviceID, StringFormat::Fmt("forward --remove tcp:%i", jdwpPort));
    // force stop the package if it was running before
    Android::adbExecCommand(m_deviceID, "shell am force-stop " + processName);
    Android::adbExecCommand(m_deviceID, "shell setprop debug.vulkan.layers :", ".", true);

    bool hookWithJDWP = true;

    if(Android::SupportsNativeLayers(m_deviceID))
    {
      RDCLOG("Using Android 10 native GPU layering");

      // if we have Android 10 native layering, don't use JDWP hooking
      hookWithJDWP = false;

      // set up environment variables for the package, and point to ourselves for vulkan and GLES
      // layers
      rdcstr installedABI = Android::DetermineInstalledABI(m_deviceID, packageName);

      Android::ABI abi = Android::ABI::unknown;

      if(installedABI == "null" || installedABI.empty())
      {
        RDCLOG("Can't determine installed ABI, falling back to device preferred ABI");

        // pick the last ABI
        rdcarray<Android::ABI> abis = Android::GetSupportedABIs(m_deviceID);

        if(abis.empty())
          RDCWARN("No ABIs listed as supported");
        else
          abi = abis.back();
      }
      else
      {
        abi = Android::GetABI(installedABI);
      }

      rdcstr layerPackage = GetRenderDocPackageForABI(abi);
      Android::adbExecCommand(m_deviceID, "shell settings put global enable_gpu_debug_layers 1");
      Android::adbExecCommand(m_deviceID, "shell settings put global gpu_debug_app " + packageName);
      Android::adbExecCommand(m_deviceID,
                              "shell settings put global gpu_debug_layer_app " + layerPackage);
      Android::adbExecCommand(
          m_deviceID, "shell settings put global gpu_debug_layers " RENDERDOC_VULKAN_LAYER_NAME);
      Android::adbExecCommand(
          m_deviceID, "shell settings put global gpu_debug_layers_gles " RENDERDOC_ANDROID_LIBRARY);

      // don't ignore the layers by default, only if we encounter an error
      Android::adbExecCommand(m_deviceID, "shell setprop debug.rdoc.IGNORE_LAYERS 0");

      Process::ProcessResult check =
          Android::adbExecCommand(m_deviceID, "shell settings list global");

      // check both since we don't know which one it will come out in
      rdcstr inString = check.strStdout + check.strStderror;

      // remove all whitespace. Our package and layer doesn't contain spaces, and the user's package
      // name can't contain spaces. This makes what we're searching for less subject to change (e.g.
      // if some adb versions print 'setting = value' instead of 'setting=value'
      // This will even work if there are new lines
      rdcstr checkString;
      checkString.reserve(inString.size());
      for(const char &c : inString)
      {
        if(c == ' ' || c == '\t' || c == '\r' || c == '\n')
          continue;

        checkString.push_back(c);
      }

      if(!checkString.contains("enable_gpu_debug_layers=1") ||
         !checkString.contains("gpu_debug_app=" + packageName) ||
         !checkString.contains("gpu_debug_layer_app=" + layerPackage) ||
         !checkString.contains("gpu_debug_layers=" RENDERDOC_VULKAN_LAYER_NAME) ||
         !checkString.contains("gpu_debug_layers_gles=" RENDERDOC_ANDROID_LIBRARY))
      {
        RDCERR(
            "Couldn't verify that debug settings are set:\n%s"
            "Do you have a strange device that requires extra setup?\n"
            "E.g. Xiaomi requires a developer account and \"USB debugging (Security Settings)\"\n",
            inString.c_str());
        hookWithJDWP = true;

        // need to tell the hooks to ignore the fact that layers are present because they're not
        // working.
        Android::adbExecCommand(m_deviceID, "shell setprop debug.rdoc.IGNORE_LAYERS 1");
      }
    }

    if(hookWithJDWP)
    {
      RDCLOG("Using pre-Android 10 Vulkan layering and JDWP injection");

      // enable the vulkan layer (will only be used by vulkan programs)
      Android::adbExecCommand(m_deviceID,
                              "shell setprop debug.vulkan.layers " RENDERDOC_VULKAN_LAYER_NAME);
    }

    rdcstr folderName = Android::GetFolderName(m_deviceID);

    // if in VR mode, enable frame delimiter markers
    Android::adbExecCommand(m_deviceID, "shell setprop debug.vr.profiler 1");

    // create the data directory we will use for storing, in case the application doesn't
    // NOTE: if processName != packageName, process may not be able to write to this directory
    // unless
    // it also has the WRITE_EXTERNAL_STORAGE permission. Under sdcardfs, only
    // Android/data/<package>
    // has the permissions set correctly, and we don't have a convenient way to get the package name
    // from native code.
    Android::adbExecCommand(m_deviceID, "shell mkdir -p /sdcard/Android/" + folderName + processName);
    Android::adbExecCommand(
        m_deviceID, "shell mkdir -p /sdcard/Android/" + folderName + processName + "/files");
    // set our property with the capture options encoded, to be picked up by the library on the
    // device
    Android::adbExecCommand(m_deviceID,
                            StringFormat::Fmt("shell setprop debug.rdoc.RENDERDOC_CAPOPTS %s",
                                              opts.EncodeAsString().c_str()));

    // try to push our settings file into the appdata folder
    Android::adbExecCommand(m_deviceID, "push \"" + FileIO::GetAppFolderFilename("renderdoc.conf") +
                                            "\" /sdcard/Android/" + folderName + processName +
                                            "/files/renderdoc.conf");

    rdcstr installedPath = Android::GetPathForPackage(m_deviceID, packageName);

    rdcstr RDCLib = Android::adbExecCommand(m_deviceID, "shell ls " + installedPath +
                                                            "/lib/*/" RENDERDOC_ANDROID_LIBRARY)
                        .strStdout.trimmed();

    if(Android_Debug_ProcessLaunch())
    {
      RDCLOG("Checking for existing library, found '%s'", RDCLib.c_str());
    }
    // some versions of adb/android return the error message on stdout, so try to detect those and
    // clear the output.
    if(RDCLib.size() < installedPath.size() || RDCLib.substr(0, installedPath.size()) != installedPath)
      RDCLib.clear();

    // some versions of adb/android also don't print any error message at all! Look to see if the
    // wildcard glob is still present.
    if(RDCLib.find("/lib/*/" RENDERDOC_ANDROID_LIBRARY) >= 0)
      RDCLib.clear();

    if(RDCLib.empty())
    {
      RDCLOG("No library found in %s/lib/*/" RENDERDOC_ANDROID_LIBRARY
             " for %s - assuming injection is required.",
             installedPath.c_str(), packageName.c_str());
    }
    else
    {
      hookWithJDWP = false;
      RDCLOG("Library found, no injection required: %s", RDCLib.c_str());
    }

    int pid = 0;

    RDCLOG("Launching package '%s' with activity '%s'", packageName.c_str(), activityName.c_str());

    if(hookWithJDWP)
    {
      RDCLOG("Setting up to launch the application as a debugger to inject.");

      // start the activity in this package with debugging enabled and force-stop after starting
      Android::adbExecCommand(
          m_deviceID, StringFormat::Fmt("shell am start -S -D -n %s/%s %s", packageName.c_str(),
                                        activityName.c_str(), intentArgs.c_str()));
    }
    else
    {
      RDCLOG("Launching APK with no debugger or direct injection.");

      // start the activity in this package with debugging enabled and force-stop after starting
      Android::adbExecCommand(
          m_deviceID, StringFormat::Fmt("shell am start -S -n %s/%s %s", packageName.c_str(),
                                        activityName.c_str(), intentArgs.c_str()));

      // don't connect JDWP
      jdwpPort = 0;
    }

    // adb shell ps | grep $PACKAGE | awk '{print $2}')
    pid = Android::GetCurrentPID(m_deviceID, processName);

    if(pid == 0)
    {
      RDCERR("Couldn't get PID when launching %s with activity %s and intent args %s",
             packageName.c_str(), activityName.c_str(), intentArgs.c_str());
      ret.status = ReplayStatus::InjectionFailed;
      ret.ident = 0;
      return;
    }

    Android::adbForwardPorts(m_portbase, m_deviceID, jdwpPort, pid, false);

    // sleep a little to let the ports initialise
    Threading::Sleep(500);

    if(jdwpPort)
    {
      // use a JDWP connection to inject our libraries
      bool injected = Android::InjectWithJDWP(m_deviceID, jdwpPort);
      if(!injected)
      {
        RDCERR("Failed to inject using JDWP");
        ret.status = ReplayStatus::JDWPFailure;
        ret.ident = 0;
        return;
      }
    }

    ret.status = ReplayStatus::InjectionFailed;

    uint32_t elapsed = 0, timeout = 1000 * RDCMAX(5U, Android_MaxConnectTimeout());
    while(elapsed < timeout)
    {
      // Check if the target app has started yet and we can connect to it.
      ITargetControl *control = RENDERDOC_CreateTargetControl(
          AndroidController::m_Inst.GetProtocolName() + "://" + m_deviceID, ret.ident,
          "testConnection", false);
      if(control)
      {
        control->Shutdown();
        ret.status = ReplayStatus::Succeeded;
        break;
      }

      // check to see if the PID is still there. If it was before and isn't now, the APK has
      // exited
      // without ever opening a connection.
      int curpid = Android::GetCurrentPID(m_deviceID, processName);

      if(curpid == 0)
      {
        RDCERR("APK has crashed or never opened target control connection before closing.");
        break;
      }

      Threading::Sleep(1000);
      elapsed += 1000;
    }

    // we leave the setprop in case the application later initialises a vulkan device. It's
    // impossible to tell if it will or not, since many applications will init and present from GLES
    // and then later use vulkan.

    return;
  });

  Atomic::Inc32(&done);

  Threading::JoinThread(pingThread);
  Threading::CloseThread(pingThread);

  return ret;
}

AndroidController AndroidController::m_Inst;

DeviceProtocolRegistration androidProtocol("adb", &AndroidController::Get);
