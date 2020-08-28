/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2020 Baldur Karlsson
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

#include "api/replay/version.h"
#include "core/core.h"
#include "miniz/miniz.h"
#include "replay/replay_driver.h"
#include "strings/string_utils.h"
#include "android_utils.h"

static const char keystoreName[] = "renderdoc.keystore";

namespace Android
{
bool RemoveAPKSignature(const rdcstr &apk)
{
  RDCLOG("Checking for existing signature");

  rdcstr aapt = getToolPath(ToolDir::BuildTools, "aapt", false);

  // Get the list of files in META-INF
  rdcstr fileList = execCommand(aapt, "list \"" + apk + "\"").strStdout;
  if(fileList.empty())
    return false;

  // Walk through the output.  If it starts with META-INF, remove it.
  uint32_t fileCount = 0;
  uint32_t matchCount = 0;

  rdcarray<rdcstr> lines;
  split(fileList, lines, '\n');

  for(rdcstr &line : lines)
  {
    line.trim();
    fileCount++;
    if(line.beginsWith("META-INF"))
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
  split(fileList, lines, '\n');
  for(rdcstr &line : lines)
  {
    line.trim();
    if(line.beginsWith("META-INF"))
    {
      RDCERR("Match found, that means removal failed! %s", line.c_str());
      return false;
    }
  }
  return true;
}

bool ExtractAndRemoveManifest(const rdcstr &apk, bytebuf &manifest)
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

        manifest = bytebuf(buf, sz);
        free(buf);
        break;
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

  rdcstr aapt = getToolPath(ToolDir::BuildTools, "aapt", false);

  RDCDEBUG("Removing AndroidManifest.xml");
  execCommand(aapt, "remove \"" + apk + "\" AndroidManifest.xml");

  rdcstr fileList = execCommand(aapt, "list \"" + apk + "\"").strStdout;
  rdcarray<rdcstr> files;
  split(fileList, files, ' ');

  for(rdcstr &f : files)
  {
    f.trim();
    if(f == "AndroidManifest.xml")
    {
      RDCERR("AndroidManifest.xml found, that means removal failed!");
      return false;
    }
  }

  return true;
}

bool AddManifestToAPK(const rdcstr &apk, const rdcstr &tmpDir, const bytebuf &manifest)
{
  rdcstr aapt = getToolPath(ToolDir::BuildTools, "aapt", false);

  // write the manifest to disk
  FileIO::WriteAll(tmpDir + "AndroidManifest.xml", manifest);

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

bool RealignAPK(const rdcstr &apk, rdcstr &alignedAPK, const rdcstr &tmpDir)
{
  rdcstr zipalign = getToolPath(ToolDir::BuildTools, "zipalign", false);

  // Re-align the APK for performance
  RDCLOG("Realigning APK");
  rdcstr errOut =
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

rdcstr GetAndroidDebugKey()
{
  rdcstr keystore = getToolPath(ToolDir::None, keystoreName, false);

  // if we found the keystore, use that.
  if(FileIO::exists(keystore.c_str()))
    return keystore;

  // otherwise, generate a temporary one
  rdcstr key = FileIO::GetTempFolderFilename() + keystoreName;

  FileIO::Delete(key.c_str());

  // locate keytool and use it to generate a keystore
  rdcstr create;
  create += " -genkey";
  create += " -keystore \"" + key + "\"";
  create += " -storepass android";
  create += " -alias rdocandroidkey";
  create += " -keypass android";
  create += " -keyalg RSA";
  create += " -keysize 2048";
  create += " -validity 10000";
  create += " -dname \"CN=, OU=, O=, L=, S=, C=\"";

  rdcstr keytool = getToolPath(ToolDir::Java, "keytool", false);

  Process::ProcessResult createResult = execCommand(keytool, create);

  Process::ProcessResult verifyResult;

  // if the keystore was created, check that the key we expect to be in it is there
  if(FileIO::exists(key.c_str()))
  {
    rdcstr verify;
    verify += " -list";
    verify += " -keystore \"" + key + "\"";
    verify += " -storepass android";

    verifyResult = execCommand(keytool, verify);

    if(verifyResult.strStdout.contains("rdocandroidkey"))
      return key;
  }

  RDCERR("Failed to create debug key: %s\n%s\n%s\n%s", createResult.strStdout.c_str(),
         createResult.strStderror.c_str(), verifyResult.strStdout.c_str(),
         verifyResult.strStderror.c_str());
  return "";
}

bool DebugSignAPK(const rdcstr &apk, const rdcstr &workDir)
{
  RDCLOG("Signing with debug key");

  rdcstr aapt = getToolPath(ToolDir::BuildTools, "aapt", false);
  rdcstr apksigner = getToolPath(ToolDir::BuildToolsLib, "apksigner.jar", false);

  rdcstr debugKey = GetAndroidDebugKey();

  if(debugKey.empty())
    return false;

  rdcstr args;
  args += " sign ";
  args += " --ks \"" + debugKey + "\" ";
  args += " --ks-pass pass:android ";
  args += " --key-pass pass:android ";
  args += " --ks-key-alias rdocandroidkey ";
  args += "\"" + apk + "\"";

  if(!apksigner.contains(".jar"))
  {
    // if we found the non-jar version, then the jar wasn't located and we found the wrapper script
    // in PATH. Execute it as a script
    execScript(apksigner, args, workDir);
  }
  else
  {
    // otherwise, find and invoke java on the .jar
    rdcstr java = getToolPath(ToolDir::Java, "java", false);

    rdcstr signerdir = get_dirname(FileIO::GetFullPathname(apksigner));

    rdcstr javaargs;
    javaargs += " \"-Djava.ext.dirs=" + signerdir + "\"";
    javaargs += " -jar \"" + apksigner + "\"";
    javaargs += args;

    execCommand(java, javaargs, workDir);
  }

  // Check for signature
  rdcstr list = execCommand(aapt, "list \"" + apk + "\"").strStdout;

  list.insert(0, '\n');

  if(list.find("\nMETA-INF") >= 0)
  {
    RDCLOG("Signature found, continuing...");
    return true;
  }

  RDCERR("re-sign of APK failed!");
  return false;
}

bool UninstallOriginalAPK(const rdcstr &deviceID, const rdcstr &packageName, const rdcstr &workDir)
{
  RDCLOG("Uninstalling previous version of application");

  adbExecCommand(deviceID, "uninstall " + packageName, workDir);

  // Wait until uninstall completes
  rdcstr uninstallResult;
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

bool ReinstallPatchedAPK(const rdcstr &deviceID, const rdcstr &apk, const rdcstr &abi,
                         const rdcstr &packageName, const rdcstr &workDir)
{
  RDCLOG("Reinstalling APK");

  if(abi == "null" || abi.empty())
    adbExecCommand(deviceID, "install \"" + apk + "\"", workDir);
  else
    adbExecCommand(deviceID, "install --abi " + abi + " \"" + apk + "\"", workDir);

  // Wait until re-install completes
  rdcstr reinstallResult;
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
  rdcarray<rdcpair<ToolDir, rdcstr>> requirements;
  rdcarray<rdcstr> missingTools;
  requirements.push_back({ToolDir::BuildTools, "aapt"});
  requirements.push_back({ToolDir::BuildTools, "zipalign"});
  requirements.push_back({ToolDir::BuildToolsLib, "apksigner.jar"});
  requirements.push_back({ToolDir::Java, "java"});

  for(uint32_t i = 0; i < requirements.size(); i++)
  {
    rdcstr tool = getToolPath(requirements[i].first, requirements[i].second, true);

    // if we located the tool, we're fine.
    if(toolExists(tool))
      continue;

    // didn't find it.
    missingTools.push_back(requirements[i].second);
  }

  // keytool is special - we look for a debug key first
  {
    rdcstr key = getToolPath(ToolDir::None, keystoreName, true);

    if(key.empty())
    {
      // if we don't have the debug key, check that we can find keytool. First in our normal search
      rdcstr tool = getToolPath(ToolDir::Java, "keytool", true);

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

bool PullAPK(const rdcstr &deviceID, const rdcstr &pkgPath, const rdcstr &apk)
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

void CopyAPK(const rdcstr &deviceID, const rdcstr &pkgPath, const rdcstr &copyPath)
{
  RDCLOG("Copying APK to %s", copyPath.c_str());
  adbExecCommand(deviceID, "shell cp " + pkgPath + " " + copyPath);
}

void RemoveAPK(const rdcstr &deviceID, const rdcstr &path)
{
  RDCLOG("Removing APK from %s", path.c_str());
  adbExecCommand(deviceID, "shell rm -f " + path);
}

bool HasRootAccess(const rdcstr &deviceID)
{
  RDCLOG("Checking for root access on %s", deviceID.c_str());

  // Try switching adb to root and check a few indicators for success
  // Nothing will fall over if we get a false positive here, it just enables
  // additional methods of getting things set up.

  Process::ProcessResult result = adbExecCommand(deviceID, "root");

  rdcstr whoami = adbExecCommand(deviceID, "shell whoami").strStdout.trimmed();
  if(whoami == "root")
    return true;

  rdcstr checksu =
      adbExecCommand(deviceID, "shell test -e /system/xbin/su && echo found").strStdout.trimmed();
  if(checksu == "found")
    return true;

  return false;
}

rdcstr GetFirstMatchingLine(const rdcstr &haystack, const rdcstr &needle)
{
  int needleOffset = haystack.find(needle);

  if(needleOffset == -1)
    return rdcstr();

  int nextLine = haystack.find('\n', needleOffset + 1);

  return haystack.substr(needleOffset, nextLine == -1 ? ~0U : size_t(nextLine - needleOffset));
}

bool IsDebuggable(const rdcstr &deviceID, const rdcstr &packageName)
{
  RDCLOG("Checking that APK is debuggable");

  rdcstr info = adbExecCommand(deviceID, "shell dumpsys package " + packageName).strStdout;

  rdcstr pkgFlags = GetFirstMatchingLine(info, "pkgFlags=[");

  if(pkgFlags == "")
  {
    RDCERR("Couldn't get pkgFlags from adb");
    return false;
  }

  return pkgFlags.contains("DEBUGGABLE");
}
};

extern "C" RENDERDOC_API void RENDERDOC_CC
RENDERDOC_CheckAndroidPackage(const char *URL, const char *packageAndActivity, AndroidFlags *flags)
{
  IDeviceProtocolHandler *adb = RenderDoc::Inst().GetDeviceProtocol("adb");

  rdcstr deviceID = adb->GetDeviceID(URL);

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
    const char *URL, const char *packageAndActivity, RENDERDOC_ProgressCallback progress)
{
  rdcstr package = Android::GetPackageName(packageAndActivity);

  Process::ProcessResult result = {};

  IDeviceProtocolHandler *adb = RenderDoc::Inst().GetDeviceProtocol("adb");

  rdcstr deviceID = adb->GetDeviceID(URL);

  // make sure progress is valid so we don't have to check it everywhere
  if(!progress)
    progress = [](float) {};

  progress(0.0f);

  if(!Android::CheckPatchingRequirements())
    return AndroidFlags::MissingTools;

  progress(0.02f);

  rdcstr abi = Android::DetermineInstalledABI(deviceID, package);

  // Find the APK on the device
  rdcstr pkgPath = Android::GetPathForPackage(deviceID, package) + "base.apk";

  rdcstr tmpDir = FileIO::GetTempFolderFilename();
  rdcstr origAPK(tmpDir + package + ".orig.apk");
  rdcstr alignedAPK(origAPK + ".aligned.apk");
  bytebuf manifest;

  // Try the following steps, bailing if anything fails
  if(!Android::PullAPK(deviceID, pkgPath, origAPK))
  {
    // Copy the APK to public storage, then try to pull again
    rdcstr copyPath = "/sdcard/" + package + ".copy.apk";
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
