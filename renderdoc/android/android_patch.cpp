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

#include <sstream>
#include "3rdparty/miniz/miniz.h"
#include "api/replay/version.h"
#include "core/core.h"
#include "strings/string_utils.h"
#include "android_utils.h"

static const char keystoreName[] = "renderdoc.keystore";

namespace Android
{
bool RemoveAPKSignature(const std::string &apk)
{
  RDCLOG("Checking for existing signature");

  std::string aapt = getToolPath(ToolDir::BuildTools, "aapt", false);

  // Get the list of files in META-INF
  std::string fileList = execCommand(aapt, "list \"" + apk + "\"").strStdout;
  if(fileList.empty())
    return false;

  // Walk through the output.  If it starts with META-INF, remove it.
  uint32_t fileCount = 0;
  uint32_t matchCount = 0;
  std::istringstream contents(fileList);
  std::string line;
  std::string prefix("META-INF");
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

bool ExtractAndRemoveManifest(const std::string &apk, std::vector<byte> &manifest)
{
  // pull out the manifest with miniz
  mz_zip_archive zip;
  RDCEraseEl(zip);

  mz_bool b = mz_zip_reader_init_file(&zip, apk.c_str(), 0);

  if(b)
  {
    mz_uint numfiles = mz_zip_reader_get_num_files(&zip);

    for(mz_uint i = 0; i < numfiles; i++)
    {
      mz_zip_archive_file_stat zstat;
      mz_zip_reader_file_stat(&zip, i, &zstat);

      if(!strcmp(zstat.m_filename, "AndroidManifest.xml"))
      {
        size_t sz = 0;
        byte *buf = (byte *)mz_zip_reader_extract_to_heap(&zip, i, &sz, 0);

        RDCLOG("Got manifest of %zu bytes", sz);

        manifest.insert(manifest.begin(), buf, buf + sz);
      }
    }
  }
  else
  {
    RDCERR("Couldn't open %s", apk.c_str());
  }

  mz_zip_reader_end(&zip);

  if(manifest.empty())
    return false;

  std::string aapt = getToolPath(ToolDir::BuildTools, "aapt", false);

  RDCDEBUG("Removing AndroidManifest.xml");
  execCommand(aapt, "remove \"" + apk + "\" AndroidManifest.xml");

  std::string fileList = execCommand(aapt, "list \"" + apk + "\"").strStdout;
  std::vector<std::string> files;
  split(fileList, files, ' ');

  for(const std::string &f : files)
  {
    if(trim(f) == "AndroidManifest.xml")
    {
      RDCERR("AndroidManifest.xml found, that means removal failed!");
      return false;
    }
  }

  return true;
}

bool AddManifestToAPK(const std::string &apk, const std::string &tmpDir,
                      const std::vector<byte> &manifest)
{
  std::string aapt = getToolPath(ToolDir::BuildTools, "aapt", false);

  // write the manifest to disk
  FileIO::dump((tmpDir + "AndroidManifest.xml").c_str(), manifest.data(), manifest.size());

  // run aapt to add the manifest
  Process::ProcessResult result =
      execCommand(aapt, "add \"" + apk + "\" AndroidManifest.xml", tmpDir);

  if(result.strStdout.empty())
  {
    RDCERR("Failed to add manifest to APK. STDERR: %s", result.strStderror.c_str());
    return false;
  }

  return true;
}

bool RealignAPK(const std::string &apk, std::string &alignedAPK, const std::string &tmpDir)
{
  std::string zipalign = getToolPath(ToolDir::BuildTools, "zipalign", false);

  // Re-align the APK for performance
  RDCLOG("Realigning APK");
  std::string errOut =
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

std::string GetAndroidDebugKey()
{
  std::string keystore = getToolPath(ToolDir::None, keystoreName, false);

  // if we found the keystore, use that.
  if(FileIO::exists(keystore.c_str()))
    return keystore;

  // otherwise, see if we generated a temporary one
  std::string key = FileIO::GetTempFolderFilename() + keystoreName;

  if(FileIO::exists(key.c_str()))
    return key;

  // locate keytool and use it to generate a keystore
  std::string create;
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
bool DebugSignAPK(const std::string &apk, const std::string &workDir)
{
  RDCLOG("Signing with debug key");

  std::string aapt = getToolPath(ToolDir::BuildTools, "aapt", false);
  std::string apksigner = getToolPath(ToolDir::BuildToolsLib, "apksigner.jar", false);

  std::string debugKey = GetAndroidDebugKey();

  std::string args;
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

    std::string signerdir = get_dirname(FileIO::GetFullPathname(apksigner));

    std::string javaargs;
    javaargs += " \"-Djava.ext.dirs=" + signerdir + "\"";
    javaargs += " -jar \"" + apksigner + "\"";
    javaargs += args;

    execCommand(java, javaargs, workDir);
  }

  // Check for signature
  std::string list = execCommand(aapt, "list \"" + apk + "\"").strStdout;

  // Walk through the output.  If it starts with META-INF, we're good
  std::istringstream contents(list);
  std::string line;
  std::string prefix("META-INF");
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

bool UninstallOriginalAPK(const std::string &deviceID, const std::string &packageName,
                          const std::string &workDir)
{
  RDCLOG("Uninstalling previous version of application");

  adbExecCommand(deviceID, "uninstall " + packageName, workDir);

  // Wait until uninstall completes
  std::string uninstallResult;
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

bool ReinstallPatchedAPK(const std::string &deviceID, const std::string &apk, const std::string &abi,
                         const std::string &packageName, const std::string &workDir)
{
  RDCLOG("Reinstalling APK");

  if(abi == "null" || abi.empty())
    adbExecCommand(deviceID, "install \"" + apk + "\"", workDir);
  else
    adbExecCommand(deviceID, "install --abi " + abi + " \"" + apk + "\"", workDir);

  // Wait until re-install completes
  std::string reinstallResult;
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
  std::vector<rdcpair<ToolDir, std::string>> requirements;
  std::vector<std::string> missingTools;
  requirements.push_back({ToolDir::BuildTools, "aapt"});
  requirements.push_back({ToolDir::BuildTools, "zipalign"});
  requirements.push_back({ToolDir::BuildToolsLib, "apksigner.jar"});
  requirements.push_back({ToolDir::Java, "java"});

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

std::string DetermineInstalledABI(const std::string &deviceID, const std::string &packageName)
{
  RDCLOG("Checking installed ABI for %s", packageName.c_str());
  std::string abi;

  std::string dump = adbExecCommand(deviceID, "shell pm dump " + packageName).strStdout;
  if(dump.empty())
    RDCERR("Unable to pm dump %s", packageName.c_str());

  // Walk through the output and look for primaryCpuAbi
  std::istringstream contents(dump);
  std::string line;
  std::string prefix("primaryCpuAbi=");
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

bool PullAPK(const std::string &deviceID, const std::string &pkgPath, const std::string &apk)
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

  RDCLOG("Failed to pull APK");
  return false;
}

void CopyAPK(const std::string &deviceID, const std::string &pkgPath, const std::string &copyPath)
{
  RDCLOG("Copying APK to %s", copyPath.c_str());
  adbExecCommand(deviceID, "shell cp " + pkgPath + " " + copyPath);
}

void RemoveAPK(const std::string &deviceID, const std::string &path)
{
  RDCLOG("Removing APK from %s", path.c_str());
  adbExecCommand(deviceID, "shell rm -f " + path);
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
    return "";

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

extern "C" RENDERDOC_API void RENDERDOC_CC RENDERDOC_CheckAndroidPackage(
    const char *hostname, const char *packageAndActivity, AndroidFlags *flags)
{
  int index = 0;
  std::string deviceID;
  Android::ExtractDeviceIDAndIndex(hostname, index, deviceID);

  // Reset the flags each time we check
  *flags = AndroidFlags::NoFlags;

  if(Android::IsDebuggable(deviceID, Android::GetPackageName(packageAndActivity)))
  {
    *flags |= AndroidFlags::Debuggable;
  }
  else
  {
    RDCLOG("%s is not debuggable", packageAndActivity);
  }

  if(Android::HasRootAccess(deviceID))
  {
    RDCLOG("Root access detected");
    *flags |= AndroidFlags::RootAccess;
  }

  return;
}

extern "C" RENDERDOC_API AndroidFlags RENDERDOC_CC RENDERDOC_MakeDebuggablePackage(
    const char *hostname, const char *packageAndActivity, RENDERDOC_ProgressCallback progress)
{
  std::string package = Android::GetPackageName(packageAndActivity);

  Process::ProcessResult result = {};

  int index = 0;
  std::string deviceID;
  Android::ExtractDeviceIDAndIndex(hostname, index, deviceID);

  // make sure progress is valid so we don't have to check it everywhere
  if(!progress)
    progress = [](float) {};

  progress(0.0f);

  if(!Android::CheckPatchingRequirements())
    return AndroidFlags::MissingTools;

  progress(0.02f);

  std::string abi = Android::DetermineInstalledABI(deviceID, package);

  // Find the APK on the device
  std::string pkgPath = Android::GetPathForPackage(deviceID, package) + "base.apk";

  std::string tmpDir = FileIO::GetTempFolderFilename();
  std::string origAPK(tmpDir + package + ".orig.apk");
  std::string alignedAPK(origAPK + ".aligned.apk");
  std::vector<byte> manifest;

  // Try the following steps, bailing if anything fails
  if(!Android::PullAPK(deviceID, pkgPath, origAPK))
  {
    // Copy the APK to public storage, then try to pull again
    std::string copyPath = "/sdcard/" + package + ".copy.apk";
    Android::CopyAPK(deviceID, pkgPath, copyPath);
    bool success = Android::PullAPK(deviceID, copyPath, origAPK);
    Android::RemoveAPK(deviceID, copyPath);
    if(!success)
    {
      return AndroidFlags::ManifestPatchFailure;
    }
  }

  progress(0.4f);

  if(!Android::RemoveAPKSignature(origAPK))
    return AndroidFlags::ManifestPatchFailure;

  progress(0.425f);

  if(!Android::ExtractAndRemoveManifest(origAPK, manifest))
    return AndroidFlags::ManifestPatchFailure;

  progress(0.45f);

  if(!Android::PatchManifest(manifest))
    return AndroidFlags::ManifestPatchFailure;

  progress(0.46f);

  if(!Android::AddManifestToAPK(origAPK, tmpDir, manifest))
    return AndroidFlags::ManifestPatchFailure;

  progress(0.475f);

  if(!Android::RealignAPK(origAPK, alignedAPK, tmpDir))
    return AndroidFlags::RepackagingAPKFailure;

  progress(0.5f);

  if(!Android::DebugSignAPK(alignedAPK, tmpDir))
    return AndroidFlags::RepackagingAPKFailure;

  progress(0.525f);

  if(!Android::UninstallOriginalAPK(deviceID, package, tmpDir))
    return AndroidFlags::RepackagingAPKFailure;

  progress(0.6f);

  if(!Android::ReinstallPatchedAPK(deviceID, alignedAPK, abi, package, tmpDir))
    return AndroidFlags::RepackagingAPKFailure;

  progress(0.95f);

  if(!Android::IsDebuggable(deviceID, package))
    return AndroidFlags::ManifestPatchFailure;

  progress(1.0f);

  // All clean!
  return AndroidFlags::Debuggable;
}
