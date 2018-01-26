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

#include "android.h"
#include <sstream>
#include "api/replay/version.h"
#include "core/core.h"
#include "strings/string_utils.h"
#include "android_utils.h"

namespace Android
{
void adbForwardPorts(int index, const std::string &deviceID, uint16_t jdwpPort, int pid)
{
  const char *forwardCommand = "forward tcp:%i localabstract:renderdoc_%i";
  int offs = RenderDoc_AndroidPortOffset * (index + 1);

  adbExecCommand(deviceID, StringFormat::Fmt(forwardCommand, RenderDoc_RemoteServerPort + offs,
                                             RenderDoc_RemoteServerPort));
  adbExecCommand(deviceID, StringFormat::Fmt(forwardCommand, RenderDoc_FirstTargetControlPort + offs,
                                             RenderDoc_FirstTargetControlPort));

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

  RDCERR("Didn't find default activity in adb output");
  return "";
}

int GetCurrentPid(const std::string &deviceID, const std::string &packageName)
{
  // try 5 times, 200ms apart to find the pid
  for(int i = 0; i < 5; i++)
  {
    Process::ProcessResult pidOutput =
        adbExecCommand(deviceID, StringFormat::Fmt("shell ps | grep %s", packageName.c_str()));

    std::string output = trim(pidOutput.strStdout);
    size_t space = output.find_first_of("\t ");

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

uint32_t StartAndroidPackageForCapture(const char *host, const char *package)
{
  int index = 0;
  std::string deviceID;
  Android::ExtractDeviceIDAndIndex(host, index, deviceID);

  string packageName = basename(string(package));    // Remove leading '/' if any

  // adb shell cmd package resolve-activity -c android.intent.category.LAUNCHER com.jake.cube1
  string activityName = GetDefaultActivityForPackage(deviceID, packageName);

  uint16_t jdwpPort = GetJdwpPort();

  // remove any previous jdwp port forward on this port
  adbExecCommand(deviceID, StringFormat::Fmt("forward --remove tcp:%i", jdwpPort));
  // force stop the package if it was running before
  adbExecCommand(deviceID, "shell am force-stop " + packageName);
  // enable the vulkan layer (will only be used by vulkan programs)
  adbExecCommand(deviceID, "shell setprop debug.vulkan.layers " RENDERDOC_VULKAN_LAYER_NAME);
  // start the activity in this package with debugging enabled and force-stop after starting
  adbExecCommand(deviceID, StringFormat::Fmt("shell am start -S -D %s/%s", packageName.c_str(),
                                             activityName.c_str()));

  // adb shell ps | grep $PACKAGE | awk '{print $2}')
  int pid = GetCurrentPid(deviceID, packageName);

  adbForwardPorts(index, deviceID, jdwpPort, pid);

  // sleep a little to let the ports initialise
  Threading::Sleep(500);

  // use a JDWP connection to inject our libraries
  InjectWithJDWP(deviceID, jdwpPort);

  uint32_t ret = RenderDoc_FirstTargetControlPort + RenderDoc_AndroidPortOffset * (index + 1);
  uint32_t elapsed = 0,
           timeout = 1000 *
                     RDCMAX(5, atoi(RenderDoc::Inst().GetConfigSetting("MaxConnectTimeout").c_str()));
  while(elapsed < timeout)
  {
    // Check if the target app has started yet and we can connect to it.
    ITargetControl *control = RENDERDOC_CreateTargetControl(host, ret, "testConnection", false);
    if(control)
    {
      control->Shutdown();
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

  // we might open the connection early, when the library is first injected, before the vulkan
  // loader completes .
  Threading::Sleep(1000);

  // Let the app pickup the setprop before we turn it back off for replaying.
  adbExecCommand(deviceID, "shell setprop debug.vulkan.layers :");

  return ret;
}

bool installRenderDocServer(const string &deviceID)
{
  string targetApk = "RenderDocCmd.apk";
  string serverApk;

  // Check known paths for RenderDoc server
  string exePath;
  FileIO::GetExecutableFilename(exePath);
  string exeDir = dirname(FileIO::GetFullPathname(exePath));

  std::vector<std::string> paths;

#if defined(RENDERDOC_APK_PATH)
  string customPath(RENDERDOC_APK_PATH);
  RDCLOG("Custom APK path: %s", customPath.c_str());

  if(FileIO::IsRelativePath(customPath))
    customPath = exeDir + "/" + customPath;

  // Check to see if APK name was included in custom path
  if(!endswith(customPath, targetApk))
  {
    if(customPath.back() != '/')
      customPath += "/";
    customPath += targetApk;
  }

  paths.push_back(customPath);
#endif

  paths.push_back(exeDir + "/android/apk/" + targetApk);                         // Windows install
  paths.push_back(exeDir + "/../share/renderdoc/android/apk/" + targetApk);      // Linux install
  paths.push_back(exeDir + "/../../build-android/bin/" + targetApk);             // Local build
  paths.push_back(exeDir + "/../../../../../build-android/bin/" + targetApk);    // macOS build

  for(uint32_t i = 0; i < paths.size(); i++)
  {
    RDCLOG("Checking for server APK in %s", paths[i].c_str());
    if(FileIO::exists(paths[i].c_str()))
    {
      serverApk = paths[i];
      RDCLOG("APK found!: %s", serverApk.c_str());
      break;
    }
  }

  if(serverApk.empty())
  {
    RDCERR(
        "%s missing! RenderDoc for Android will not work without it. "
        "Build your Android ABI in build-android in the root to have it "
        "automatically found and installed.",
        targetApk.c_str());
    return false;
  }

  // Build a map so we can switch on the string that returns from adb calls
  enum AndroidAbis
  {
    Android_armeabi,
    Android_armeabi_v7a,
    Android_arm64_v8a,
    Android_x86,
    Android_x86_64,
    Android_mips,
    Android_mips64,
    Android_numAbis
  };

  // clang-format off
  static std::map<std::string, AndroidAbis> abi_string_map;
  abi_string_map["armeabi"]     = Android_armeabi;
  abi_string_map["armeabi-v7a"] = Android_armeabi_v7a;
  abi_string_map["arm64-v8a"]   = Android_arm64_v8a;
  abi_string_map["x86"]         = Android_x86;
  abi_string_map["x86_64"]      = Android_x86_64;
  abi_string_map["mips"]        = Android_mips;
  abi_string_map["mips64"]      = Android_mips64;
  // clang-format on

  // 32-bit server works for 32 and 64 bit apps
  // For stable builds of the server, only 32-bit libs will be packaged into APK
  // For local builds, whatever was specified as single ABI will be packaged into APK

  string adbAbi = trim(adbExecCommand(deviceID, "shell getprop ro.product.cpu.abi").strStdout);

  string adbInstall;
  switch(abi_string_map[adbAbi])
  {
    case Android_armeabi_v7a:
    case Android_arm64_v8a:
      adbInstall = adbExecCommand(deviceID, "install -r -g \"" + serverApk + "\"").strStdout;
      break;
    case Android_armeabi:
    case Android_x86:
    case Android_x86_64:
    case Android_mips:
    case Android_mips64:
    default:
    {
      RDCERR("Unsupported target ABI: %s", adbAbi.c_str());
      return false;
    }
  }

  // Ensure installation succeeded
  string adbCheck =
      adbExecCommand(deviceID, "shell pm list packages org.renderdoc.renderdoccmd").strStdout;
  if(adbCheck.empty())
  {
    RDCERR("Installation of RenderDocCmd.apk failed!");
    return false;
  }

  return true;
}
bool RemoveRenderDocAndroidServer(const string &deviceID, const string &packageName)
{
  adbExecCommand(deviceID, "uninstall " + packageName);

  // Ensure uninstall succeeded
  string adbCheck = adbExecCommand(deviceID, "shell pm list packages " + packageName).strStdout;

  if(!adbCheck.empty())
  {
    RDCERR("Uninstall of %s failed!", packageName.c_str());
    return false;
  }

  return true;
}
bool CheckAndroidServerVersion(const string &deviceID)
{
  string packageName = "org.renderdoc.renderdoccmd";
  RDCLOG("Checking installed version of %s on %s", packageName.c_str(), deviceID.c_str());

  string dump = adbExecCommand(deviceID, "shell pm dump " + packageName).strStdout;
  if(dump.empty())
    RDCERR("Unable to pm dump %s", packageName.c_str());

  // Walk through the output and look for versionCode and versionName
  std::istringstream contents(dump);
  string line;
  string versionCode;
  string versionName;
  string prefix1("versionCode=");
  string prefix2("versionName=");
  while(std::getline(contents, line))
  {
    line = trim(line);
    if(line.compare(0, prefix1.size(), prefix1) == 0)
    {
      // versionCode is not alone in this line, isolate it
      std::vector<string> vec;
      split(line, vec, ' ');
      versionCode = vec[0].substr(vec[0].find_last_of("=") + 1);
    }
    if(line.compare(0, prefix2.size(), prefix2) == 0)
    {
      versionName = line.substr(line.find_first_of("=") + 1);
    }
  }

  if(versionCode.empty())
    RDCERR("Unable to determine versionCode for: %s", packageName.c_str());

  if(versionName.empty())
    RDCERR("Unable to determine versionName for: %s", packageName.c_str());

  // Compare the server's versionCode and versionName with the host's for compatibility
  string hostVersionCode =
      string(STRINGIZE(RENDERDOC_VERSION_MAJOR)) + string(STRINGIZE(RENDERDOC_VERSION_MINOR));
  string hostVersionName = RENDERDOC_STABLE_BUILD ? MAJOR_MINOR_VERSION_STRING : GitVersionHash;

  // False positives will hurt us, so check for explicit matches
  if((hostVersionCode == versionCode) && (hostVersionName == versionName))
  {
    RDCLOG("Installed server version (%s:%s) is compatible", versionCode.c_str(),
           versionName.c_str());
    return true;
  }

  RDCWARN("RenderDoc server versionCode:versionName (%s:%s) is incompatible with host (%s:%s)",
          versionCode.c_str(), versionName.c_str(), hostVersionCode.c_str(), hostVersionName.c_str());

  if(RemoveRenderDocAndroidServer(deviceID, packageName))
    RDCLOG("Uninstall of incompatible server succeeded");

  return false;
}

};    // namespace Android
using namespace Android;

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
  string adbStdout = adbExecCommand("", "devices").strStdout;

  int idx = 0;

  using namespace std;
  istringstream stdoutStream(adbStdout);
  string ret;
  string line;
  while(getline(stdoutStream, line))
  {
    vector<string> tokens;
    split(line, tokens, '\t');
    if(tokens.size() == 2 && trim(tokens[1]) == "device")
    {
      if(ret.length())
        ret += ",";

      ret += StringFormat::Fmt("adb:%d:%s", idx, tokens[0].c_str());

      // Forward the ports so we can see if a remoteserver/captured app is already running.
      adbForwardPorts(idx, tokens[0], 0, 0);

      idx++;
    }
  }

  *deviceList = ret;
}

extern "C" RENDERDOC_API void RENDERDOC_CC RENDERDOC_StartAndroidRemoteServer(const char *device)
{
  int index = 0;
  std::string deviceID;

  Android::ExtractDeviceIDAndIndex(device, index, deviceID);

  string adbPackage =
      adbExecCommand(deviceID, "shell pm list packages org.renderdoc.renderdoccmd").strStdout;

  if(adbPackage.empty() || !CheckAndroidServerVersion(deviceID))
  {
    // If server is not detected or has been removed due to incompatibility, install it
    if(!installRenderDocServer(deviceID))
      return;
  }

  adbExecCommand(deviceID, "shell am force-stop org.renderdoc.renderdoccmd");
  adbForwardPorts(index, deviceID, 0, 0);
  adbExecCommand(deviceID, "shell setprop debug.vulkan.layers :");
  adbExecCommand(
      deviceID,
      "shell am start -n org.renderdoc.renderdoccmd/.Loader -e renderdoccmd remoteserver");
}
