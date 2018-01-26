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

#include <sstream>
#include "core/core.h"
#include "strings/string_utils.h"
#include "android_utils.h"

// we use GIT_COMMIT_HASH here instead of GitVersionHash since the value is actually only used on
// android - where GIT_COMMIT_HASH is available globally. We need to have it available in a static
// initializer at compile time, which wouldn't be possible with GitVersionHash.
#if !defined(GIT_COMMIT_HASH)
#define GIT_COMMIT_HASH "NO_GIT_COMMIT_HASH_DEFINED_AT_BUILD_TIME"
#endif

extern "C" RENDERDOC_API const char RENDERDOC_Version_Tag_String[] =
    "RenderDoc_build_version: " FULL_VERSION_STRING " from git commit " GIT_COMMIT_HASH;

static const char keystoreName[] = "renderdoc.keystore";

namespace Android
{
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

  if(version == FULL_VERSION_STRING && hash == GitVersionHash)
  {
    RDCLOG("RenderDoc layer version (%s) and git hash (%s) match.", version.c_str(), hash.c_str());
    match = true;
  }
  else
  {
    RDCLOG(
        "RenderDoc layer version (%s) and git hash (%s) do NOT match the host version (%s) or git "
        "hash (%s).",
        version.c_str(), hash.c_str(), FULL_VERSION_STRING, GitVersionHash);
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

bool CheckInstalledPermissions(const string &deviceID, const string &packageName)
{
  RDCLOG("Checking installed permissions for %s", packageName.c_str());

  string dump = adbExecCommand(deviceID, "shell pm dump " + packageName).strStdout;
  if(dump.empty())
    RDCERR("Unable to pm dump %s", packageName.c_str());

  return CheckPermissions(dump);
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

std::string GetPathForPackage(const std::string &deviceID, const std::string &packageName)
{
  std::string pkgPath = trim(adbExecCommand(deviceID, "shell pm path " + packageName).strStdout);

  if(pkgPath.empty() || pkgPath.find("package:") != 0 || pkgPath.find("base.apk") == std::string::npos)
    return pkgPath;

  pkgPath.erase(pkgPath.begin(), pkgPath.begin() + strlen("package:"));
  pkgPath.erase(pkgPath.end() - strlen("base.apk"), pkgPath.end());

  return pkgPath;
}
};
using namespace Android;

extern "C" RENDERDOC_API void RENDERDOC_CC RENDERDOC_CheckAndroidPackage(const char *host,
                                                                         const char *exe,
                                                                         AndroidFlags *flags)
{
  string packageName(basename(string(exe)));

  int index = 0;
  std::string deviceID;
  Android::ExtractDeviceIDAndIndex(host, index, deviceID);

  // Find the path to package
  std::string pkgPath = Android::GetPathForPackage(deviceID, packageName) + "lib";

  string layerName = "libVkLayer_GLES_RenderDoc.so";

  // Reset the flags each time we check
  *flags = AndroidFlags::NoFlags;

  bool found = false;
  string layerPath = "";

  // Check a debug location only usable by rooted devices, overriding app's layer
  if(SearchForAndroidLibrary(deviceID, "/data/local/debug/vulkan", layerName, layerPath))
    found = true;

  // See if the application contains the layer
  if(!found && SearchForAndroidLibrary(deviceID, pkgPath, layerName, layerPath))
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
  Android::ExtractDeviceIDAndIndex(host, index, deviceID);

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
  return SearchForAndroidLibrary(deviceID, layerDst, layerName, foundLayer);
}

extern "C" RENDERDOC_API bool RENDERDOC_CC RENDERDOC_AddLayerToAndroidPackage(
    const char *host, const char *exe, RENDERDOC_ProgressCallback progress)
{
  Process::ProcessResult result = {};
  string packageName(basename(string(exe)));

  int index = 0;
  std::string deviceID;
  Android::ExtractDeviceIDAndIndex(host, index, deviceID);

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
  std::string apkPath = Android::GetPathForPackage(deviceID, packageName) + "base.apk";

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
