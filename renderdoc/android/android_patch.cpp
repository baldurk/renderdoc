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
#include "api/replay/version.h"
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

bool HasRootAccess(const std::string &deviceID)
{
  RDCLOG("Checking for root access on %s", deviceID.c_str());

  Process::ProcessResult result = {};

  // Try switching adb to root and check a few indicators for success
  // Nothing will fall over if we get a false positive here, it just enables
  // additional methods of getting things set up.

  result = adbExecCommand(deviceID, "root");

  std::string whoami = trim(adbExecCommand(deviceID, "shell whoami").strStdout);
  if(whoami == "root")
    return true;

  std::string checksu =
      trim(adbExecCommand(deviceID, "shell test -e /system/xbin/su && echo found").strStdout);
  if(checksu == "found")
    return true;

  return false;
}

std::string GetFirstMatchingLine(const std::string &haystack, const std::string &needle)
{
  size_t needleOffset = haystack.find(needle);

  if(needleOffset == std::string::npos)
  {
    RDCERR("Couldn't get pkgFlags from adb");
    return "";
  }

  size_t nextLine = haystack.find('\n', needleOffset + 1);

  return haystack.substr(needleOffset,
                         nextLine == std::string::npos ? nextLine : nextLine - needleOffset);
}

bool IsDebuggable(const std::string &deviceID, const std::string &packageName)
{
  RDCLOG("Checking that APK is debuggable");

  std::string info = adbExecCommand(deviceID, "shell dumpsys package " + packageName).strStdout;

  std::string pkgFlags = GetFirstMatchingLine(info, "pkgFlags=[");

  if(pkgFlags == "")
  {
    RDCERR("Couldn't get pkgFlags from adb");
    return false;
  }

  return pkgFlags.find("DEBUGGABLE") != std::string::npos;
}
};

extern "C" RENDERDOC_API void RENDERDOC_CC RENDERDOC_CheckAndroidPackage(const char *hostname,
                                                                         const char *packageName,
                                                                         AndroidFlags *flags)
{
  int index = 0;
  std::string deviceID;
  Android::ExtractDeviceIDAndIndex(hostname, index, deviceID);

  // Reset the flags each time we check
  *flags = AndroidFlags::NoFlags;

  if(Android::IsDebuggable(deviceID, basename(std::string(packageName))))
  {
    *flags |= AndroidFlags::Debuggable;
  }
  else
  {
    RDCLOG("%s is not debuggable", packageName);
  }

  if(Android::HasRootAccess(deviceID))
  {
    RDCLOG("Root access detected");
    *flags |= AndroidFlags::RootAccess;
  }

  return;
}

extern "C" RENDERDOC_API AndroidFlags RENDERDOC_CC RENDERDOC_MakeDebuggablePackage(
    const char *hostname, const char *packageName, RENDERDOC_ProgressCallback progress)
{
  // stub for now
  return AndroidFlags::ManifestPatchFailure;
}
