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

#include "android.h"
#include <sstream>
#include "api/replay/version.h"
#include "core/core.h"
#include "strings/string_utils.h"
#include "android_utils.h"

namespace Android
{
void adbForwardPorts(int index, const std::string &deviceID, uint16_t jdwpPort, int pid, bool silent)
{
  const char *forwardCommand = "forward tcp:%i localabstract:renderdoc_%i";
  int offs = RenderDoc_AndroidPortOffset * (index + 1);

  adbExecCommand(deviceID, StringFormat::Fmt(forwardCommand, RenderDoc_RemoteServerPort + offs,
                                             RenderDoc_RemoteServerPort),
                 ".", silent);
  adbExecCommand(deviceID, StringFormat::Fmt(forwardCommand, RenderDoc_FirstTargetControlPort + offs,
                                             RenderDoc_FirstTargetControlPort),
                 ".", silent);

  if(jdwpPort && pid)
    adbExecCommand(deviceID, StringFormat::Fmt("forward tcp:%hu jdwp:%i", jdwpPort, pid));
}

uint16_t GetJdwpPort()
{
  // we loop over a number of ports to try and avoid previous failed attempts from leaving sockets
  // open and messing with subsequent attempts
  const uint16_t portBase = RenderDoc_FirstTargetControlPort + RenderDoc_AndroidPortOffset * 2;

  static uint16_t portIndex = 0;

  portIndex++;
  portIndex %= RenderDoc_AndroidPortOffset;

  return portBase + portIndex;
}

std::string GetDefaultActivityForPackage(const std::string &deviceID, const std::string &packageName)
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

  std::vector<std::string> lines;
  split(activity.strStdout, lines, '\n');

  for(std::string &line : lines)
  {
    line = trim(line);

    if(!strncmp(line.c_str(), "name=", 5))
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
  size_t intentFilterSize = strlen(intentFilter);

  for(size_t idx = 0; idx < numOfLines; idx++)
  {
    std::string line = trim(lines[idx]);
    if(!strncmp(line.c_str(), intentFilter, intentFilterSize) && idx + 1 < numOfLines)
    {
      std::string activityName = trim(lines[idx + 1]);
      size_t startPos = activityName.find("/");
      if(startPos == std::string::npos)
      {
        RDCWARN("Failed to find default activity");
        return "";
      }
      size_t endPos = activityName.find(" ", startPos + 1);
      if(endPos == std::string::npos)
      {
        endPos = activityName.length();
      }
      return activityName.substr(startPos + 1, endPos - startPos - 1);
    }
  }

  RDCERR("Didn't find default activity in adb output");
  return "";
}

int GetCurrentPID(const std::string &deviceID, const std::string &packageName)
{
  // try 5 times, 200ms apart to find the pid
  for(int i = 0; i < 5; i++)
  {
    Process::ProcessResult pidOutput =
        adbExecCommand(deviceID, StringFormat::Fmt("shell ps -A | grep %s", packageName.c_str()));

    std::string output = trim(pidOutput.strStdout);
    size_t space = output.find_first_of("\t ");

    // if we didn't get a response, try without the -A as some android devices don't support that
    // parameter
    if(output.empty() || output.find(packageName) == std::string::npos || space == std::string::npos)
    {
      pidOutput =
          adbExecCommand(deviceID, StringFormat::Fmt("shell ps | grep %s", packageName.c_str()));

      output = trim(pidOutput.strStdout);
      space = output.find_first_of("\t ");
    }

    // if we still didn't get a response, sleep and try again next time
    if(output.empty() || output.find(packageName) == std::string::npos || space == std::string::npos)
    {
      Threading::Sleep(200);
      continue;
    }

    char *pid = &output[space];
    while(*pid == ' ' || *pid == '\t')
      pid++;

    char *end = pid;
    while(*end >= '0' && *end <= '9')
      end++;

    *end = 0;

    return atoi(pid);
  }

  return 0;
}

ExecuteResult StartAndroidPackageForCapture(const char *host, const char *packageAndActivity,
                                            const char *intentArgs, const CaptureOptions &opts)
{
  int index = 0;
  std::string deviceID;
  Android::ExtractDeviceIDAndIndex(host, index, deviceID);

  ExecuteResult ret;
  ret.status = ReplayStatus::UnknownError;
  ret.ident = RenderDoc_FirstTargetControlPort + RenderDoc_AndroidPortOffset * (index + 1);

  std::string packageName = GetPackageName(packageAndActivity);    // Remove leading '/' if any

  // adb shell cmd package resolve-activity -c android.intent.category.LAUNCHER com.jake.cube1
  std::string activityName = GetActivityName(packageAndActivity);

  // if the activity name isn't specified, get the default one
  if(activityName.empty() || activityName == "#DefaultActivity")
    activityName = GetDefaultActivityForPackage(deviceID, packageName);

  uint16_t jdwpPort = GetJdwpPort();

  // remove any previous jdwp port forward on this port
  adbExecCommand(deviceID, StringFormat::Fmt("forward --remove tcp:%i", jdwpPort));
  // force stop the package if it was running before
  adbExecCommand(deviceID, "shell am force-stop " + packageName);
  // enable the vulkan layer (will only be used by vulkan programs)
  adbExecCommand(deviceID, "shell setprop debug.vulkan.layers " RENDERDOC_VULKAN_LAYER_NAME);
  // if in VR mode, enable frame delimiter markers
  adbExecCommand(deviceID, "shell setprop debug.vr.profiler 1");
  // create the data directory we will use for storing, in case the application doesn't
  adbExecCommand(deviceID, "shell mkdir -p /sdcard/Android/data/" + packageName);
  // set our property with the capture options encoded, to be picked up by the library on the device
  adbExecCommand(deviceID, StringFormat::Fmt("shell setprop debug.rdoc.RENDERDOC_CAPOPTS %s",
                                             opts.EncodeAsString().c_str()));

  std::string installedPath = GetPathForPackage(deviceID, packageName);

  std::string RDCLib = trim(
      adbExecCommand(deviceID, "shell ls " + installedPath + "/lib/*/" RENDERDOC_ANDROID_LIBRARY)
          .strStdout);

  // some versions of adb/android return the error message on stdout, so try to detect those and
  // clear the output.
  if(RDCLib.size() < installedPath.size() || RDCLib.substr(0, installedPath.size()) != installedPath)
    RDCLib.clear();

  // some versions of adb/android also don't print any error message at all! Look to see if the
  // wildcard glob is still present.
  if(RDCLib.find("/lib/*/" RENDERDOC_ANDROID_LIBRARY) != std::string::npos)
    RDCLib.clear();

  bool injectLibraries = true;

  if(RDCLib.empty())
  {
    RDCLOG("No library found in %s/lib/*/" RENDERDOC_ANDROID_LIBRARY
           " for %s - assuming injection is required.",
           installedPath.c_str(), packageName.c_str());
  }
  else
  {
    injectLibraries = false;
    RDCLOG("Library found, no injection required: %s", RDCLib.c_str());
  }

  int pid = 0;

  RDCLOG("Launching package '%s' with activity '%s'", packageName.c_str(), activityName.c_str());

  if(injectLibraries)
  {
    RDCLOG("Setting up to launch the application as a debugger to inject.");

    // start the activity in this package with debugging enabled and force-stop after starting
    adbExecCommand(deviceID,
                   StringFormat::Fmt("shell am start -S -D -n %s/%s %s", packageName.c_str(),
                                     activityName.c_str(), intentArgs));

    // adb shell ps | grep $PACKAGE | awk '{print $2}')
    pid = GetCurrentPID(deviceID, packageName);

    if(pid == 0)
      RDCERR("Couldn't get PID when launching %s with activity %s", packageName.c_str(),
             activityName.c_str());
  }
  else
  {
    RDCLOG("Not doing any injection - assuming APK is pre-loaded with RenderDoc capture library.");

    // start the activity in this package with debugging enabled and force-stop after starting
    adbExecCommand(deviceID, StringFormat::Fmt("shell am start -n %s/%s %s", packageName.c_str(),
                                               activityName.c_str(), intentArgs));

    // don't connect JDWP
    jdwpPort = 0;
  }

  adbForwardPorts(index, deviceID, jdwpPort, pid, false);

  // sleep a little to let the ports initialise
  Threading::Sleep(500);

  if(jdwpPort)
  {
    // use a JDWP connection to inject our libraries
    bool injected = InjectWithJDWP(deviceID, jdwpPort);
    if(!injected)
    {
      RDCERR("Failed to inject using JDWP");
      ret.status = ReplayStatus::JDWPFailure;
      ret.ident = 0;
      return ret;
    }
  }

  ret.status = ReplayStatus::InjectionFailed;

  uint32_t elapsed = 0,
           timeout = 1000 *
                     RDCMAX(5, atoi(RenderDoc::Inst().GetConfigSetting("MaxConnectTimeout").c_str()));
  while(elapsed < timeout)
  {
    // Check if the target app has started yet and we can connect to it.
    ITargetControl *control = RENDERDOC_CreateTargetControl(host, ret.ident, "testConnection", false);
    if(control)
    {
      control->Shutdown();
      ret.status = ReplayStatus::Succeeded;
      break;
    }

    // check to see if the PID is still there. If it was before and isn't now, the APK has exited
    // without ever opening a connection.
    int curpid = GetCurrentPID(deviceID, packageName);

    if(pid != 0 && curpid == 0)
    {
      RDCERR("APK has crashed or never opened target control connection before closing.");
      break;
    }

    Threading::Sleep(1000);
    elapsed += 1000;
  }

  // we leave the setprop in case the application later initialises a vulkan device. It's impossible
  // to tell if it will or not, since many applications will init and present from GLES and then
  // later use vulkan.

  return ret;
}

bool CheckAndroidServerVersion(const std::string &deviceID, ABI abi)
{
  // assume all servers are updated at the same rate. Only check first ABI's version
  std::string packageName = GetRenderDocPackageForABI(abi);
  RDCLOG("Checking installed version of %s on %s", packageName.c_str(), deviceID.c_str());

  std::string dump = adbExecCommand(deviceID, "shell pm dump " + packageName).strStdout;
  if(dump.empty())
    RDCERR("Unable to pm dump %s", packageName.c_str());

  std::string versionCode = trim(GetFirstMatchingLine(dump, "versionCode="));
  std::string versionName = trim(GetFirstMatchingLine(dump, "versionName="));

  // versionCode is not alone in this line, isolate it
  if(versionCode != "")
  {
    size_t spaceOffset = versionCode.find(' ');
    versionCode.erase(spaceOffset);

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
  std::string hostVersionCode = std::string(STRINGIZE(RENDERDOC_VERSION_MAJOR)) +
                                std::string(STRINGIZE(RENDERDOC_VERSION_MINOR));
  std::string hostVersionName = GitVersionHash;

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

ReplayStatus InstallRenderDocServer(const std::string &deviceID)
{
  ReplayStatus status = ReplayStatus::Succeeded;

  std::vector<ABI> abis = GetSupportedABIs(deviceID);

  if(abis.empty())
  {
    RDCERR("Couldn't determine supported ABIs for %s", deviceID.c_str());
    return ReplayStatus::AndroidABINotFound;
  }

  // Check known paths for RenderDoc server
  std::string libPath;
  FileIO::GetLibraryFilename(libPath);
  std::string libDir = get_dirname(FileIO::GetFullPathname(libPath));

  std::vector<std::string> paths;

#if defined(RENDERDOC_APK_PATH)
  string customPath(RENDERDOC_APK_PATH);
  RDCLOG("Custom APK path: %s", customPath.c_str());

  if(FileIO::IsRelativePath(customPath))
    customPath = libDir + "/" + customPath;

  if(!endswith(customPath, "/"))
    customPath += "/";

  paths.push_back(customPath);
#endif

  std::string suff = GetRenderDocPackageForABI(abis[0], '-');
  suff.erase(0, strlen(RENDERDOC_ANDROID_PACKAGE_BASE));

  paths.push_back(libDir + "/plugins/android/");                                 // Windows install
  paths.push_back(libDir + "/../share/renderdoc/plugins/android/");              // Linux install
  paths.push_back(libDir + "/../plugins/android/");                              // macOS install
  paths.push_back(libDir + "/../../build-android/bin/");                         // Local build
  paths.push_back(libDir + "/../../build-android" + suff + "/bin/");             // Local ABI build
  paths.push_back(libDir + "/../../../../../build-android/bin/");                // macOS build
  paths.push_back(libDir + "/../../../../../build-android" + suff + "/bin/");    // macOS ABI build

  // use the first ABI for searching
  std::string apk = GetRenderDocPackageForABI(abis[0]);
  std::string apksFolder;

  for(uint32_t i = 0; i < paths.size(); i++)
  {
    RDCLOG("Checking for server APK in %s", paths[i].c_str());

    std::string apkpath = paths[i] + apk + ".apk";

    if(FileIO::exists(apkpath.c_str()))
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

    size_t abiSuffix = apk.find(suff);
    if(abiSuffix != std::string::npos)
    {
      std::string abisuff = GetRenderDocPackageForABI(abi, '-');
      abisuff.erase(0, strlen(RENDERDOC_ANDROID_PACKAGE_BASE));
      apk.replace(abiSuffix, suff.size(), abisuff);
    }

    apk += GetRenderDocPackageForABI(abi) + ".apk";

    if(!FileIO::exists(apk.c_str()))
      RDCWARN(
          "%s missing - ensure you build all ABIs your device can support for full compatibility",
          apk.c_str());

    Process::ProcessResult adbInstall = adbExecCommand(deviceID, "install -r -g \"" + apk + "\"");

    RDCLOG("Installed package '%s', checking for success...", apk.c_str());

    bool success = CheckAndroidServerVersion(deviceID, abi);

    if(!success)
    {
      status = ReplayStatus::AndroidGrantPermissionsFailed;
      RDCLOG("Failed to install APK. stdout: %s, stderr: %s", trim(adbInstall.strStdout).c_str(),
             trim(adbInstall.strStderror).c_str());
      RDCLOG("Retrying...");
      adbExecCommand(deviceID, "install -r \"" + apk + "\"");
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

  size_t lines = adbCheck.strStdout.find('\n') == std::string::npos ? 1 : 2;

  if(lines != abis.size())
    RDCWARN("Installation of some apks failed!");

  return status;
}

bool RemoveRenderDocAndroidServer(const std::string &deviceID)
{
  std::vector<ABI> abis = GetSupportedABIs(deviceID);

  if(abis.empty())
    return false;

  // remove the old package, if it's still there. Ignore any errors
  adbExecCommand(deviceID, "uninstall " RENDERDOC_ANDROID_PACKAGE_BASE);

  for(ABI abi : abis)
  {
    std::string packageName = GetRenderDocPackageForABI(abi);

    adbExecCommand(deviceID, "uninstall " + packageName);

    // Ensure uninstall succeeded
    std::string adbCheck =
        adbExecCommand(deviceID, "shell pm list packages " + packageName).strStdout;

    if(!adbCheck.empty())
    {
      RDCERR("Uninstall of %s failed!", packageName.c_str());
      return false;
    }
  }

  return true;
}

void ResetCaptureSettings(const std::string &deviceID)
{
  Android::adbExecCommand(deviceID, "shell setprop debug.vulkan.layers :", ".", true);
}
};    // namespace Android

extern "C" RENDERDOC_API void RENDERDOC_CC RENDERDOC_GetAndroidFriendlyName(const rdcstr &device,
                                                                            rdcstr &friendly)
{
  if(!Android::IsHostADB(device.c_str()))
  {
    RDCERR("Calling RENDERDOC_GetAndroidFriendlyName with non-android device: %s", device.c_str());
    return;
  }

  int index = 0;
  std::string deviceID;
  Android::ExtractDeviceIDAndIndex(device.c_str(), index, deviceID);

  if(deviceID.empty())
  {
    RDCERR("Failed to get android device and index from: %s", device.c_str());
    return;
  }

  friendly = Android::GetFriendlyName(deviceID);
}

extern "C" RENDERDOC_API void RENDERDOC_CC RENDERDOC_EnumerateAndroidDevices(rdcstr *deviceList)
{
  std::string adbStdout = Android::adbExecCommand("", "devices", ".", true).strStdout;

  int idx = 0;

  std::string ret;

  std::vector<std::string> lines;
  split(adbStdout, lines, '\n');
  for(const std::string &line : lines)
  {
    std::vector<std::string> tokens;
    split(line, tokens, '\t');
    if(tokens.size() == 2 && trim(tokens[1]) == "device")
    {
      if(ret.length())
        ret += ",";

      ret += StringFormat::Fmt("adb:%d:%s", idx, tokens[0].c_str());

      // Forward the ports so we can see if a remoteserver/captured app is already running.
      Android::adbForwardPorts(idx, tokens[0], 0, 0, true);

      idx++;
    }
  }

  *deviceList = ret;
}

extern "C" RENDERDOC_API void RENDERDOC_CC RENDERDOC_AndroidInitialise()
{
  Android::initAdb();
}

extern "C" RENDERDOC_API void RENDERDOC_CC RENDERDOC_AndroidShutdown()
{
  Android::shutdownAdb();
}

extern "C" RENDERDOC_API bool RENDERDOC_CC RENDERDOC_IsAndroidSupported(const char *device)
{
  int index = 0;
  std::string deviceID;

  Android::ExtractDeviceIDAndIndex(device, index, deviceID);

  return Android::IsSupported(deviceID);
}

extern "C" RENDERDOC_API ReplayStatus RENDERDOC_CC RENDERDOC_StartAndroidRemoteServer(const char *device)
{
  ReplayStatus status = ReplayStatus::Succeeded;
  int index = 0;
  std::string deviceID;

  Android::ExtractDeviceIDAndIndex(device, index, deviceID);

  std::string packagesOutput = trim(
      Android::adbExecCommand(deviceID, "shell pm list packages " RENDERDOC_ANDROID_PACKAGE_BASE)
          .strStdout);

  std::vector<std::string> packages;
  split(packagesOutput, packages, '\n');

  std::vector<Android::ABI> abis = Android::GetSupportedABIs(deviceID);

  RDCLOG("Starting RenderDoc server, supported ABIs:");
  for(Android::ABI abi : abis)
    RDCLOG("  - %s", ToStr(abi).c_str());

  if(abis.empty())
    return ReplayStatus::UnknownError;

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
    if(status != ReplayStatus::Succeeded && status != ReplayStatus::AndroidGrantPermissionsFailed)
    {
      RDCERR("Failed to install RenderDoc server app");
      return status;
    }
  }

  // stop all servers of any ABI
  for(Android::ABI abi : abis)
    Android::adbExecCommand(deviceID, "shell am force-stop " + GetRenderDocPackageForABI(abi));

  if(abis.empty())
    return ReplayStatus::AndroidABINotFound;

  Android::adbForwardPorts(index, deviceID, 0, 0, false);
  Android::ResetCaptureSettings(deviceID);

  // launch the last ABI, as the 64-bit version where possible, or 32-bit version where not.
  // Captures are portable across bitness and in some cases a 64-bit capture can't replay on a
  // 32-bit remote server.
  Android::adbExecCommand(deviceID, "shell am start -n " + GetRenderDocPackageForABI(abis.back()) +
                                        "/.Loader -e renderdoccmd remoteserver");
  return status;
}
