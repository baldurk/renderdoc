/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2017 Baldur Karlsson
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

extern "C" RENDERDOC_API const char RENDERDOC_Version_Tag_String[] =
    "RenderDoc_build_version: " FULL_VERSION_STRING " from git commit " GIT_COMMIT_HASH;

namespace Android
{
enum class ToolDir
{
  None,
  Java,
  BuildTools,
  BuildToolsLib,
  PlatformTools,
};
static const char keystoreName[] = "renderdoc.keystore";
bool toolExists(const std::string &path)
{
  if(path.empty())
    return false;
  return FileIO::exists(path.c_str()) || FileIO::exists((path + ".exe").c_str());
}
std::string getToolInSDK(ToolDir subdir, const std::string &jdkroot, const std::string &sdkroot,
                         const std::string &toolname)
{
  std::string toolpath;

  switch(subdir)
  {
    case ToolDir::None:
    {
      // This indicates the file is not a standard tool and will not exist anywhere but our
      // distributed folder.
      break;
    }
    case ToolDir::Java:
    {
      // if no path is configured, abort
      if(jdkroot.empty())
        break;

      toolpath = jdkroot + "/bin/" + toolname;

      if(toolExists(toolpath))
        return toolpath;

      break;
    }
    case ToolDir::BuildTools:
    case ToolDir::BuildToolsLib:
    case ToolDir::PlatformTools:
    {
      // if no path is configured, abort
      if(sdkroot.empty())
        break;

      // if it's in platform tools it's easy, just concatenate the path
      if(subdir == ToolDir::PlatformTools)
      {
        toolpath = sdkroot + "/platform-tools/" + toolname;
      }
      else
      {
        // otherwise we need to find the build-tools versioned folder
        toolpath = sdkroot + "/build-tools/";

        std::vector<PathEntry> paths = FileIO::GetFilesInDirectory(toolpath.c_str());

        if(paths.empty())
          break;

        uint32_t bestversion = 0;
        std::string bestpath;

        for(const PathEntry &path : paths)
        {
          // skip non-directories
          if(!(path.flags & PathProperty::Directory))
            continue;

          uint32_t version = 0;
          bool valid = true;
          for(char c : path.filename)
          {
            // add digits to the version
            if(c >= '0' && c <= '9')
            {
              int digit = int(c) - int('0');
              version *= 10;
              version += digit;
              continue;
            }

            // ignore .s
            if(c == '.')
              continue;

            // if any char is not in [.0-9] then this filename is invalid
            valid = false;
            break;
          }

          // skip non-valid directories
          if(!valid)
            continue;

          // if this directory is a higher version, prefer it
          if(version > bestversion)
          {
            bestversion = version;
            bestpath = path.filename;
          }
        }

        // if we didn't find a version at all, abort
        if(bestversion == 0)
          break;

        toolpath += bestpath + "/";

        if(subdir == ToolDir::BuildToolsLib)
          toolpath += "lib/";

        toolpath += toolname;
      }

      if(toolExists(toolpath))
        return toolpath;

      break;
    }
  }

  return "";
}
struct ToolPathCache
{
  std::string sdk, jdk;
  std::map<std::string, std::string> paths;
} cache;
std::string getToolPath(ToolDir subdir, const std::string &toolname, bool checkExist)
{
  // search path for tools:
  // 1. First look relative to the configured paths, these come from the user manually setting them
  //    so they always have priority.
  // 2. Next if those paths don't exist or the tool isn't found, we search relative to our
  //    executable looking for an android/ subfolder, and look for the tool in there.
  // 3. If we still don't have that (most likely because it's a local build from a git clone and not
  //    a distributed build with the tools available) then we fall back to trying to auto-locate it.
  //    - First check if the tool is in the path, assuming the user configured it to their system.
  //    - Otherwise check environment variables or default locations

  std::string sdk = RenderDoc::Inst().GetConfigSetting("androidSDKPath");
  std::string jdk = RenderDoc::Inst().GetConfigSetting("androidJDKPath");

  // invalidate the cache when these settings change
  if(sdk != cache.sdk || jdk != cache.jdk)
  {
    cache.paths.clear();
    cache.sdk = sdk;
    cache.jdk = jdk;
  }

  // if we have the path cached and it's still valid, return it
  if(toolExists(cache.paths[toolname]))
    return cache.paths[toolname];

  std::string &toolpath = cache.paths[toolname];

  // first try according to the configured paths
  toolpath = getToolInSDK(subdir, jdk, sdk, toolname);

  if(toolExists(toolpath))
    return toolpath;

  // next try to locate it in our own distributed android subfolder
  {
    std::string exepath;
    FileIO::GetExecutableFilename(exepath);
    std::string exedir = dirname(FileIO::GetFullPathname(exepath));

    toolpath = exedir + "/android/" + toolname;
    if(toolExists(toolpath))
      return toolpath;
  }

  // need to try to auto-guess the tool's location

  // first try in PATH
  if(subdir != ToolDir::None)
  {
    toolpath = FileIO::FindFileInPath(toolname);

    if(toolExists(toolpath))
      return toolpath;

    // if the tool name contains a .jar then try stripping that and look for the non-.jar version in
    // the PATH.
    if(toolname.find(".jar") != std::string::npos)
    {
      toolpath = toolname;
      toolpath.erase(toolpath.rfind(".jar"), 4);
      toolpath = FileIO::FindFileInPath(toolpath);

      if(toolExists(toolpath))
        return toolpath;
    }
  }

  // now try to find it based on heuristics/environment variables
  const char *env = Process::GetEnvVariable("JAVA_HOME");

  jdk = env ? env : "";

  env = Process::GetEnvVariable("ANDROID_HOME");
  sdk = env ? env : "";

  if(sdk.empty() || !FileIO::exists(sdk.c_str()))
  {
    env = Process::GetEnvVariable("ANDROID_SDK_ROOT");
    sdk = env ? env : "";
  }

  if(sdk.empty() || !FileIO::exists(sdk.c_str()))
  {
    env = Process::GetEnvVariable("ANDROID_SDK");
    sdk = env ? env : "";
  }

  // maybe in future we can try to search in common install locations.

  toolpath = getToolInSDK(subdir, jdk, sdk, toolname);

  if(toolExists(toolpath))
    return toolpath;

  toolpath = "";

  // if we're checking for existence, we have failed so return empty string.
  if(checkExist)
    return toolpath;

  // otherwise we at least return the tool name so that there's something to try and run
  return toolname;
}

bool IsHostADB(const char *hostname)
{
  return !strncmp(hostname, "adb:", 4);
}
void extractDeviceIDAndIndex(const string &hostname, int &index, string &deviceID)
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
Process::ProcessResult execScript(const string &script, const string &args,
                                  const string &workDir = ".")
{
  RDCLOG("SCRIPT: %s", script.c_str());

  Process::ProcessResult result;
  Process::LaunchScript(script.c_str(), workDir.c_str(), args.c_str(), &result);
  return result;
}
Process::ProcessResult execCommand(const string &exe, const string &args,
                                   const string &workDir = ".")
{
  RDCLOG("COMMAND: %s '%s'", exe.c_str(), args.c_str());

  Process::ProcessResult result;
  Process::LaunchProcess(exe.c_str(), workDir.c_str(), args.c_str(), &result);
  return result;
}
Process::ProcessResult adbExecCommand(const string &device, const string &args, const string &workDir)
{
  std::string adb = getToolPath(ToolDir::PlatformTools, "adb", false);
  Process::ProcessResult result;
  string deviceArgs;
  if(device.empty())
    deviceArgs = args;
  else
    deviceArgs = StringFormat::Fmt("-s %s %s", device.c_str(), args.c_str());
  return execCommand(adb, deviceArgs, workDir);
}
string adbGetDeviceList()
{
  return adbExecCommand("", "devices").strStdout;
}
void adbForwardPorts(int index, const std::string &deviceID)
{
  const char *forwardCommand = "forward tcp:%i localabstract:renderdoc_%i";
  int offs = RenderDoc_AndroidPortOffset * (index + 1);

  adbExecCommand(deviceID, StringFormat::Fmt(forwardCommand, RenderDoc_RemoteServerPort + offs,
                                             RenderDoc_RemoteServerPort));
  adbExecCommand(deviceID, StringFormat::Fmt(forwardCommand, RenderDoc_FirstTargetControlPort + offs,
                                             RenderDoc_FirstTargetControlPort));
}
uint32_t StartAndroidPackageForCapture(const char *host, const char *package)
{
  int index = 0;
  std::string deviceID;
  Android::extractDeviceIDAndIndex(host, index, deviceID);

  string packageName = basename(string(package));    // Remove leading '/' if any

  adbExecCommand(deviceID, "shell am force-stop " + packageName);
  adbForwardPorts(index, deviceID);
  adbExecCommand(deviceID, "shell setprop debug.vulkan.layers VK_LAYER_RENDERDOC_Capture");
  adbExecCommand(deviceID,
                 "shell monkey -p " + packageName + " -c android.intent.category.LAUNCHER 1");

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

    Threading::Sleep(1000);
    elapsed += 1000;
  }

  // Let the app pickup the setprop before we turn it back off for replaying.
  adbExecCommand(deviceID, "shell setprop debug.vulkan.layers :");

  return ret;
}

bool SearchForAndroidLayer(const string &deviceID, const string &location, const string &layerName,
                           string &foundLayer)
{
  RDCLOG("Checking for layers in: %s", location.c_str());
  foundLayer =
      trim(adbExecCommand(deviceID, "shell find " + location + " -name " + layerName).strStdout);
  if(!foundLayer.empty())
  {
    RDCLOG("Found RenderDoc layer in %s", location.c_str());
    return true;
  }
  return false;
}

bool RemoveAPKSignature(const string &apk)
{
  RDCLOG("Checking for existing signature");

  std::string aapt = getToolPath(ToolDir::BuildTools, "aapt", false);

  // Get the list of files in META-INF
  string fileList = execCommand(aapt, "list \"" + apk + "\"").strStdout;
  if(fileList.empty())
    return false;

  // Walk through the output.  If it starts with META-INF, remove it.
  uint32_t fileCount = 0;
  uint32_t matchCount = 0;
  std::istringstream contents(fileList);
  string line;
  string prefix("META-INF");
  while(std::getline(contents, line))
  {
    line = trim(line);
    fileCount++;
    if(line.compare(0, prefix.size(), prefix) == 0)
    {
      RDCDEBUG("Match found, removing  %s", line.c_str());
      execCommand(aapt, "remove \"" + apk + "\" " + line);
      matchCount++;
    }
  }
  RDCLOG("%d files searched, %d removed", fileCount, matchCount);

  // Ensure no hits on second pass through
  RDCDEBUG("Walk through file list again, ensure signature removed");
  fileList = execCommand(aapt, "list \"" + apk + "\"").strStdout;
  std::istringstream recheck(fileList);
  while(std::getline(recheck, line))
  {
    if(line.compare(0, prefix.size(), prefix) == 0)
    {
      RDCERR("Match found, that means removal failed! %s", line.c_str());
      return false;
    }
  }
  return true;
}

bool AddLayerToAPK(const string &apk, const string &layerPath, const string &layerName,
                   const string &abi, const string &tmpDir)
{
  RDCLOG("Adding RenderDoc layer");

  std::string aapt = getToolPath(ToolDir::BuildTools, "aapt", false);

  // Run aapt from the directory containing "lib" so the relative paths are good
  string relativeLayer("lib/" + abi + "/" + layerName);
  string workDir = removeFromEnd(layerPath, relativeLayer);

  // If the layer was already present in the APK, we need to remove it first
  Process::ProcessResult contents = execCommand(aapt, "list \"" + apk + "\"", workDir);
  if(contents.strStdout.empty())
  {
    RDCERR("Failed to list contents of APK. STDERR: %s", contents.strStderror.c_str());
    return false;
  }

  if(contents.strStdout.find(relativeLayer) != std::string::npos)
  {
    RDCLOG("Removing existing layer from APK before trying to add");
    Process::ProcessResult remove =
        execCommand(aapt, "remove \"" + apk + "\" " + relativeLayer, workDir);

    if(!remove.strStdout.empty())
    {
      RDCERR("Failed to remove existing layer from APK. STDERR: %s", remove.strStderror.c_str());
      return false;
    }
  }

  // Add the RenderDoc layer
  Process::ProcessResult result = execCommand(aapt, "add \"" + apk + "\" " + relativeLayer, workDir);

  if(result.strStdout.empty())
  {
    RDCERR("Failed to add layer to APK. STDERR: %s", result.strStderror.c_str());
    return false;
  }

  return true;
}

bool RealignAPK(const string &apk, string &alignedAPK, const string &tmpDir)
{
  std::string zipalign = getToolPath(ToolDir::BuildTools, "zipalign", false);

  // Re-align the APK for performance
  RDCLOG("Realigning APK");
  string errOut =
      execCommand(zipalign, "-f 4 \"" + apk + "\" \"" + alignedAPK + "\"", tmpDir).strStderror;

  if(!errOut.empty())
    return false;

  // Wait until the aligned version exists to proceed
  uint32_t elapsed = 0;
  uint32_t timeout = 10000;    // 10 seconds
  while(elapsed < timeout)
  {
    if(FileIO::exists(alignedAPK.c_str()))
    {
      RDCLOG("Aligned APK ready to go, continuing...");
      return true;
    }

    Threading::Sleep(1000);
    elapsed += 1000;
  }

  RDCERR("Timeout reached aligning APK");
  return false;
}

string GetAndroidDebugKey()
{
  std::string keystore = getToolPath(ToolDir::None, keystoreName, false);

  // if we found the keystore, use that.
  if(FileIO::exists(keystore.c_str()))
    return keystore;

  // otherwise, see if we generated a temporary one
  string key = FileIO::GetTempFolderFilename() + keystoreName;

  if(FileIO::exists(key.c_str()))
    return key;

  // locate keytool and use it to generate a keystore
  string create;
  create += " -genkey";
  create += " -keystore \"" + key + "\"";
  create += " -storepass android";
  create += " -alias androiddebugkey";
  create += " -keypass android";
  create += " -keyalg RSA";
  create += " -keysize 2048";
  create += " -validity 10000";
  create += " -dname \"CN=, OU=, O=, L=, S=, C=\"";

  std::string keytool = getToolPath(ToolDir::Java, "keytool", false);

  Process::ProcessResult result = execCommand(keytool, create);

  if(!result.strStderror.empty())
    RDCERR("Failed to create debug key");

  return key;
}
bool DebugSignAPK(const string &apk, const string &workDir)
{
  RDCLOG("Signing with debug key");

  std::string aapt = getToolPath(ToolDir::BuildTools, "aapt", false);
  std::string apksigner = getToolPath(ToolDir::BuildToolsLib, "apksigner.jar", false);

  string debugKey = GetAndroidDebugKey();

  string args;
  args += " sign ";
  args += " --ks \"" + debugKey + "\" ";
  args += " --ks-pass pass:android ";
  args += " --key-pass pass:android ";
  args += " --ks-key-alias androiddebugkey ";
  args += "\"" + apk + "\"";

  if(apksigner.find(".jar") == std::string::npos)
  {
    // if we found the non-jar version, then the jar wasn't located and we found the wrapper script
    // in PATH. Execute it as a script
    execScript(apksigner, args, workDir);
  }
  else
  {
    // otherwise, find and invoke java on the .jar
    std::string java = getToolPath(ToolDir::Java, "java", false);

    std::string signerdir = dirname(FileIO::GetFullPathname(apksigner));

    std::string javaargs;
    javaargs += " \"-Djava.ext.dirs=" + signerdir + "\"";
    javaargs += " -jar \"" + apksigner + "\"";
    javaargs += args;

    execCommand(java, javaargs, workDir);
  }

  // Check for signature
  string list = execCommand(aapt, "list \"" + apk + "\"").strStdout;

  // Walk through the output.  If it starts with META-INF, we're good
  std::istringstream contents(list);
  string line;
  string prefix("META-INF");
  while(std::getline(contents, line))
  {
    if(line.compare(0, prefix.size(), prefix) == 0)
    {
      RDCLOG("Signature found, continuing...");
      return true;
    }
  }

  RDCERR("re-sign of APK failed!");
  return false;
}

bool UninstallOriginalAPK(const string &deviceID, const string &packageName, const string &workDir)
{
  RDCLOG("Uninstalling previous version of application");

  adbExecCommand(deviceID, "uninstall " + packageName, workDir);

  // Wait until uninstall completes
  string uninstallResult;
  uint32_t elapsed = 0;
  uint32_t timeout = 10000;    // 10 seconds
  while(elapsed < timeout)
  {
    uninstallResult = adbExecCommand(deviceID, "shell pm path " + packageName).strStdout;
    if(uninstallResult.empty())
    {
      RDCLOG("Package removed");
      return true;
    }

    Threading::Sleep(1000);
    elapsed += 1000;
  }

  RDCERR("Uninstallation of APK failed!");
  return false;
}

bool ReinstallPatchedAPK(const string &deviceID, const string &apk, const string &abi,
                         const string &packageName, const string &workDir)
{
  RDCLOG("Reinstalling APK");

  adbExecCommand(deviceID, "install --abi " + abi + " \"" + apk + "\"", workDir);

  // Wait until re-install completes
  string reinstallResult;
  uint32_t elapsed = 0;
  uint32_t timeout = 10000;    // 10 seconds
  while(elapsed < timeout)
  {
    reinstallResult = adbExecCommand(deviceID, "shell pm path " + packageName).strStdout;
    if(!reinstallResult.empty())
    {
      RDCLOG("Patched APK reinstalled, continuing...");
      return true;
    }

    Threading::Sleep(1000);
    elapsed += 1000;
  }

  RDCERR("Reinstallation of APK failed!");
  return false;
}

bool CheckPatchingRequirements()
{
  // check for required tools for patching
  std::vector<std::pair<ToolDir, std::string>> requirements;
  std::vector<std::string> missingTools;
  requirements.push_back(std::make_pair(ToolDir::BuildTools, "aapt"));
  requirements.push_back(std::make_pair(ToolDir::BuildTools, "zipalign"));
  requirements.push_back(std::make_pair(ToolDir::BuildToolsLib, "apksigner.jar"));
  requirements.push_back(std::make_pair(ToolDir::Java, "java"));

  for(uint32_t i = 0; i < requirements.size(); i++)
  {
    std::string tool = getToolPath(requirements[i].first, requirements[i].second, true);

    // if we located the tool, we're fine.
    if(toolExists(tool))
      continue;

    // didn't find it.
    missingTools.push_back(requirements[i].second);
  }

  // keytool is special - we look for a debug key first
  {
    std::string key = getToolPath(ToolDir::None, keystoreName, true);

    if(key.empty())
    {
      // if we don't have the debug key, check that we can find keytool. First in our normal search
      std::string tool = getToolPath(ToolDir::Java, "keytool", true);

      if(tool.empty())
      {
        // if not, it's missing too
        missingTools.push_back("keytool");
      }
    }
  }

  if(missingTools.size() > 0)
  {
    for(uint32_t i = 0; i < missingTools.size(); i++)
      RDCERR("Missing %s", missingTools[i].c_str());
    return false;
  }

  return true;
}

bool PullAPK(const string &deviceID, const string &pkgPath, const string &apk)
{
  RDCLOG("Pulling APK to patch");

  adbExecCommand(deviceID, "pull " + pkgPath + " \"" + apk + "\"");

  // Wait until the apk lands
  uint32_t elapsed = 0;
  uint32_t timeout = 10000;    // 10 seconds
  while(elapsed < timeout)
  {
    if(FileIO::exists(apk.c_str()))
    {
      RDCLOG("Original APK ready to go, continuing...");
      return true;
    }

    Threading::Sleep(1000);
    elapsed += 1000;
  }

  RDCERR("Failed to pull APK");
  return false;
}

bool CheckLayerVersion(const string &deviceID, const string &layerName, const string &remoteLayer)
{
  RDCDEBUG("Checking layer version of: %s", layerName.c_str());

  bool match = false;

  // Use 'strings' command on the device to find the layer's build version
  // i.e. strings -n <tag length> <layer> | grep <tag marker>
  // Subtract 5 to provide a bit of wiggle room on version length
  Process::ProcessResult result = adbExecCommand(
      deviceID, "shell strings -n " +
                    StringFormat::Fmt("%u", strlen(RENDERDOC_Version_Tag_String) - 5) + " " +
                    remoteLayer + " | grep RenderDoc_build_version");

  string line = trim(result.strStdout);

  if(line.empty())
  {
    RDCLOG("RenderDoc layer is not versioned, so cannot be checked for compatibility.");
    return false;
  }

  std::vector<string> vec;
  split(line, vec, ' ');
  string version = vec[1];
  string hash = vec[5];

  if(version == FULL_VERSION_STRING && hash == GIT_COMMIT_HASH)
  {
    RDCLOG("RenderDoc layer version (%s) and git hash (%s) match.", version.c_str(), hash.c_str());
    match = true;
  }
  else
  {
    RDCLOG(
        "RenderDoc layer version (%s) and git hash (%s) do NOT match the host version (%s) or git "
        "hash (%s).",
        version.c_str(), hash.c_str(), FULL_VERSION_STRING, GIT_COMMIT_HASH);
  }

  return match;
}

bool CheckPermissions(const string &dump)
{
  // TODO: remove this if we are sure that there are no permissions to check.
  return true;
}

bool CheckAPKPermissions(const string &apk)
{
  RDCLOG("Checking that APK can be can write to sdcard");

  std::string aapt = getToolPath(ToolDir::BuildTools, "aapt", false);

  string badging = execCommand(aapt, "dump badging \"" + apk + "\"").strStdout;

  if(badging.empty())
  {
    RDCERR("Unable to aapt dump %s", apk.c_str());
    return false;
  }

  return CheckPermissions(badging);
}
bool CheckDebuggable(const string &apk)
{
  RDCLOG("Checking that APK s debuggable");

  std::string aapt = getToolPath(ToolDir::BuildTools, "aapt", false);

  string badging = execCommand(aapt, "dump badging \"" + apk + "\"").strStdout;

  if(badging.find("application-debuggable") == string::npos)
  {
    RDCERR("APK is not debuggable");
    return false;
  }

  return true;
}
}    // namespace Android

using namespace Android;
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
  string hostVersionName = RENDERDOC_STABLE_BUILD ? MAJOR_MINOR_VERSION_STRING : GIT_COMMIT_HASH;

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

bool CheckInstalledPermissions(const string &deviceID, const string &packageName)
{
  RDCLOG("Checking installed permissions for %s", packageName.c_str());

  string dump = adbExecCommand(deviceID, "shell pm dump " + packageName).strStdout;
  if(dump.empty())
    RDCERR("Unable to pm dump %s", packageName.c_str());

  return CheckPermissions(dump);
}

bool CheckRootAccess(const string &deviceID)
{
  RDCLOG("Checking for root access on %s", deviceID.c_str());

  Process::ProcessResult result = {};

  // Try switching adb to root and check a few indicators for success
  // Nothing will fall over if we get a false positive here, it just enables
  // additional methods of getting things set up.

  result = adbExecCommand(deviceID, "root");

  string whoami = trim(adbExecCommand(deviceID, "shell whoami").strStdout);
  if(whoami == "root")
    return true;

  string checksu =
      trim(adbExecCommand(deviceID, "shell test -e /system/xbin/su && echo found").strStdout);
  if(checksu == "found")
    return true;

  return false;
}

string DetermineInstalledABI(const string &deviceID, const string &packageName)
{
  RDCLOG("Checking installed ABI for %s", packageName.c_str());
  string abi;

  string dump = adbExecCommand(deviceID, "shell pm dump " + packageName).strStdout;
  if(dump.empty())
    RDCERR("Unable to pm dump %s", packageName.c_str());

  // Walk through the output and look for primaryCpuAbi
  std::istringstream contents(dump);
  string line;
  string prefix("primaryCpuAbi=");
  while(std::getline(contents, line))
  {
    line = trim(line);
    if(line.compare(0, prefix.size(), prefix) == 0)
    {
      // Extract the abi
      abi = line.substr(line.find_last_of("=") + 1);
      RDCLOG("primaryCpuAbi found: %s", abi.c_str());
      break;
    }
  }

  if(abi.empty())
    RDCERR("Unable to determine installed abi for: %s", packageName.c_str());

  return abi;
}

string FindAndroidLayer(const string &abi, const string &layerName)
{
  string layer;

  // Check known paths for RenderDoc layer
  string exePath;
  FileIO::GetExecutableFilename(exePath);
  string exeDir = dirname(FileIO::GetFullPathname(exePath));

  std::vector<std::string> paths;

#if defined(RENDERDOC_LAYER_PATH)
  string customPath(RENDERDOC_LAYER_PATH);
  RDCLOG("Custom layer path: %s", customPath.c_str());

  if(FileIO::IsRelativePath(customPath))
    customPath = exeDir + "/" + customPath;

  if(!endswith(customPath, "/"))
    customPath += "/";

  // Custom path must point to directory containing ABI folders
  customPath += abi;
  if(!FileIO::exists(customPath.c_str()))
  {
    RDCWARN("Custom layer path does not contain required ABI");
  }
  paths.push_back(customPath + "/" + layerName);
#endif

  string windows = "/android/lib/";
  string linux = "/../share/renderdoc/android/lib/";
  string local = "/../../build-android/renderdoccmd/libs/lib/";
  string macOS = "/../../../../../build-android/renderdoccmd/libs/lib/";

  paths.push_back(exeDir + windows + abi + "/" + layerName);
  paths.push_back(exeDir + linux + abi + "/" + layerName);
  paths.push_back(exeDir + local + abi + "/" + layerName);
  paths.push_back(exeDir + macOS + abi + "/" + layerName);

  for(uint32_t i = 0; i < paths.size(); i++)
  {
    RDCLOG("Checking for layer in %s", paths[i].c_str());
    if(FileIO::exists(paths[i].c_str()))
    {
      layer = paths[i];
      RDCLOG("Layer found!: %s", layer.c_str());
      break;
    }
  }

  if(layer.empty())
  {
    RDCERR(
        "%s missing! RenderDoc for Android will not work without it. "
        "Build your Android ABI in build-android in the root to have it "
        "automatically found and installed.",
        layerName.c_str());
  }

  return layer;
}

extern "C" RENDERDOC_API void RENDERDOC_CC RENDERDOC_GetAndroidFriendlyName(const rdcstr &device,
                                                                            rdcstr &friendly)
{
  if(!IsHostADB(device.c_str()))
  {
    RDCERR("Calling RENDERDOC_GetAndroidFriendlyName with non-android device: %s", device.c_str());
    return;
  }

  int index = 0;
  std::string deviceID;
  Android::extractDeviceIDAndIndex(device.c_str(), index, deviceID);

  if(deviceID.empty())
  {
    RDCERR("Failed to get android device and index from: %s", device.c_str());
    return;
  }

  string manuf = trim(adbExecCommand(deviceID, "shell getprop ro.product.manufacturer").strStdout);
  string model = trim(adbExecCommand(deviceID, "shell getprop ro.product.model").strStdout);

  std::string combined;

  if(manuf.empty() && model.empty())
    combined = "";
  else if(manuf.empty() && !model.empty())
    combined = model;
  else if(!manuf.empty() && model.empty())
    combined = manuf + " device";
  else if(!manuf.empty() && !model.empty())
    combined = manuf + " " + model;

  if(combined.empty())
    friendly = "";
  else
    friendly = combined;
}

extern "C" RENDERDOC_API void RENDERDOC_CC RENDERDOC_EnumerateAndroidDevices(rdcstr *deviceList)
{
  string adbStdout = adbGetDeviceList();

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
      adbForwardPorts(idx, tokens[0]);

      idx++;
    }
  }

  *deviceList = ret;
}

extern "C" RENDERDOC_API void RENDERDOC_CC RENDERDOC_StartAndroidRemoteServer(const char *device)
{
  int index = 0;
  std::string deviceID;

  Android::extractDeviceIDAndIndex(device, index, deviceID);

  string adbPackage =
      adbExecCommand(deviceID, "shell pm list packages org.renderdoc.renderdoccmd").strStdout;

  if(adbPackage.empty() || !CheckAndroidServerVersion(deviceID))
  {
    // If server is not detected or has been removed due to incompatibility, install it
    if(!installRenderDocServer(deviceID))
      return;
  }

  adbExecCommand(deviceID, "shell am force-stop org.renderdoc.renderdoccmd");
  adbForwardPorts(index, deviceID);
  adbExecCommand(deviceID, "shell setprop debug.vulkan.layers :");
  adbExecCommand(
      deviceID,
      "shell am start -n org.renderdoc.renderdoccmd/.Loader -e renderdoccmd remoteserver");
}

extern "C" RENDERDOC_API void RENDERDOC_CC RENDERDOC_CheckAndroidPackage(const char *host,
                                                                         const char *exe,
                                                                         AndroidFlags *flags)
{
  string packageName(basename(string(exe)));

  int index = 0;
  std::string deviceID;
  Android::extractDeviceIDAndIndex(host, index, deviceID);

  // Find the path to package
  string pkgPath = trim(adbExecCommand(deviceID, "shell pm path " + packageName).strStdout);
  pkgPath.erase(pkgPath.begin(), pkgPath.begin() + strlen("package:"));
  pkgPath.erase(pkgPath.end() - strlen("base.apk"), pkgPath.end());
  pkgPath += "lib";

  string layerName = "libVkLayer_GLES_RenderDoc.so";

  // Reset the flags each time we check
  *flags = AndroidFlags::NoFlags;

  bool found = false;
  string layerPath = "";

  // Check a debug location only usable by rooted devices, overriding app's layer
  if(SearchForAndroidLayer(deviceID, "/data/local/debug/vulkan", layerName, layerPath))
    found = true;

  // See if the application contains the layer
  if(!found && SearchForAndroidLayer(deviceID, pkgPath, layerName, layerPath))
    found = true;

  // TODO: Add any future layer locations

  if(found)
  {
#if ENABLED(RDOC_DEVEL)
    // Check the version of the layer found
    if(!CheckLayerVersion(deviceID, layerName, layerPath))
    {
      RDCWARN("RenderDoc layer found, but version does not match");
      *flags |= AndroidFlags::WrongLayerVersion;
    }
#endif
  }
  else
  {
    RDCWARN("No RenderDoc layer for Vulkan or GLES was found");
    *flags |= AndroidFlags::MissingLibrary;
  }

  // Next check permissions of the installed application (without pulling the APK)
  if(!CheckInstalledPermissions(deviceID, packageName))
  {
    RDCWARN("Android application does not have required permissions");
    *flags |= AndroidFlags::MissingPermissions;
  }

  if(CheckRootAccess(deviceID))
  {
    RDCLOG("Root access detected");
    *flags |= AndroidFlags::RootAccess;
  }

  return;
}

extern "C" RENDERDOC_API bool RENDERDOC_CC RENDERDOC_PushLayerToInstalledAndroidApp(const char *host,
                                                                                    const char *exe)
{
  Process::ProcessResult result = {};
  string packageName(basename(string(exe)));

  RDCLOG("Attempting to push RenderDoc layer to %s", packageName.c_str());

  int index = 0;
  std::string deviceID;
  Android::extractDeviceIDAndIndex(host, index, deviceID);

  // Detect which ABI was installed on the device
  string abi = DetermineInstalledABI(deviceID, packageName);

  // Find the layer on host
  string layerName("libVkLayer_GLES_RenderDoc.so");
  string layerPath = FindAndroidLayer(abi, layerName);
  if(layerPath.empty())
    return false;

  // Determine where to push the layer
  string pkgPath = trim(adbExecCommand(deviceID, "shell pm path " + packageName).strStdout);

  // Isolate the app's lib dir
  pkgPath.erase(pkgPath.begin(), pkgPath.begin() + strlen("package:"));
  string libDir = removeFromEnd(pkgPath, "base.apk") + "lib/";

  // There will only be one ABI in the lib dir
  string libsAbi = trim(adbExecCommand(deviceID, "shell ls " + libDir).strStdout);
  string layerDst = libDir + libsAbi + "/";
  result = adbExecCommand(deviceID, "push " + layerPath + " " + layerDst);

  // Ensure the push succeeded
  string foundLayer;
  return SearchForAndroidLayer(deviceID, layerDst, layerName, foundLayer);
}

extern "C" RENDERDOC_API bool RENDERDOC_CC RENDERDOC_AddLayerToAndroidPackage(
    const char *host, const char *exe, RENDERDOC_ProgressCallback progress)
{
  Process::ProcessResult result = {};
  string packageName(basename(string(exe)));

  int index = 0;
  std::string deviceID;
  Android::extractDeviceIDAndIndex(host, index, deviceID);

  // make sure progress is valid so we don't have to check it everywhere
  if(!progress)
    progress = [](float) {};

  progress(0.0f);

  if(!CheckPatchingRequirements())
    return false;

  progress(0.11f);

  // Detect which ABI was installed on the device
  string abi = DetermineInstalledABI(deviceID, packageName);

  // Find the layer on host
  string layerName("libVkLayer_GLES_RenderDoc.so");
  string layerPath = FindAndroidLayer(abi, layerName);
  if(layerPath.empty())
    return false;

  // Find the APK on the device
  string pkgPath = trim(adbExecCommand(deviceID, "shell pm path " + packageName).strStdout);
  pkgPath.erase(pkgPath.begin(), pkgPath.begin() + strlen("package:"));

  string tmpDir = FileIO::GetTempFolderFilename();
  string origAPK(tmpDir + packageName + ".orig.apk");
  string alignedAPK(origAPK + ".aligned.apk");

  progress(0.21f);

  // Try the following steps, bailing if anything fails
  if(!PullAPK(deviceID, pkgPath, origAPK))
    return false;

  progress(0.31f);

  if(!CheckAPKPermissions(origAPK))
    return false;

  progress(0.41f);

  if(!RemoveAPKSignature(origAPK))
    return false;

  progress(0.51f);

  if(!AddLayerToAPK(origAPK, layerPath, layerName, abi, tmpDir))
    return false;

  progress(0.61f);

  if(!RealignAPK(origAPK, alignedAPK, tmpDir))
    return false;

  progress(0.71f);

  if(!DebugSignAPK(alignedAPK, tmpDir))
    return false;

  progress(0.81f);

  if(!UninstallOriginalAPK(deviceID, packageName, tmpDir))
    return false;

  progress(0.91f);

  if(!ReinstallPatchedAPK(deviceID, alignedAPK, abi, packageName, tmpDir))
    return false;

  progress(1.0f);

  // All clean!
  return true;
}
