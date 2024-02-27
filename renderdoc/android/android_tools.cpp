/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2024 Baldur Karlsson
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

#include "common/formatting.h"
#include "core/core.h"
#include "core/settings.h"
#include "strings/string_utils.h"
#include "android_utils.h"

RDOC_CONFIG(rdcstr, Android_SDKDirPath, "",
            "The location of the root of the Android SDK. This path "
            "should contain folders such as build-tools and platform-tools.");

RDOC_CONFIG(rdcstr, Android_JDKDirPath, "",
            "The location of the root of the Java JDK. This path "
            "should contain folders such as bin and lib.");

namespace Android
{
static bool adbKillServer = false;
bool toolExists(const rdcstr &path)
{
  if(path.empty())
    return false;
  return FileIO::exists(path) || FileIO::exists(path + ".exe");
}
rdcstr getToolInSDK(ToolDir subdir, const rdcstr &jdkroot, const rdcstr &sdkroot,
                    const rdcstr &toolname)
{
  rdcstr toolpath;

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

        rdcarray<PathEntry> paths;
        FileIO::GetFilesInDirectory(toolpath, paths);

        if(paths.empty())
          break;

        uint32_t bestversion = 0;
        rdcstr bestpath;

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
  rdcstr sdk, jdk;
  std::map<rdcstr, rdcstr> paths;
};

static ToolPathCache &getCache()
{
  static ToolPathCache *cache = new ToolPathCache;
  return *cache;
}

rdcstr getToolPath(ToolDir subdir, const rdcstr &toolname, bool checkExist)
{
  // search path for tools:
  // 1. First look relative to the configured paths, these come from the user manually setting them
  //    so they always have priority.
  // 2. Next we try to auto-locate it.
  //    - First check if the tool is in the path, assuming the user configured it to their system.
  //    - Otherwise check environment variables or default locations
  // 3. Finally if those paths don't exist or the tool isn't found, we search relative to our
  //    executable looking for an android/ subfolder, and look for the tool in there.
  //
  // The main reason we check our bundled folder last is because adb requires a *precise* match in
  // its client-server setup, so if we run our bundled adb that might be newer than the user's, they
  // will then get fighting back and forth when trying to run their own.

  rdcstr sdk = Android_SDKDirPath();
  rdcstr jdk = Android_JDKDirPath();

  ToolPathCache &cache = getCache();

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

  rdcstr &toolpath = cache.paths[toolname];

  // first try according to the configured paths
  toolpath = getToolInSDK(subdir, jdk, sdk, toolname);

  if(toolExists(toolpath))
    return toolpath;

  // need to try to auto-guess the tool's location

  // first try in PATH
  if(subdir != ToolDir::None)
  {
    toolpath = FileIO::FindFileInPath(toolname);

    if(toolExists(toolpath))
      return toolpath;

    // if the tool name contains a .jar then try stripping that and look for the non-.jar version in
    // the PATH.
    if(toolname.contains(".jar"))
    {
      toolpath = strip_extension(toolname);
      toolpath = FileIO::FindFileInPath(toolpath);

      if(toolExists(toolpath))
        return toolpath;
    }
  }

  // now try to find it based on heuristics/environment variables
  jdk = Process::GetEnvVariable("JAVA_HOME");
  sdk = Process::GetEnvVariable("ANDROID_HOME");

  if(sdk.empty() || !FileIO::exists(sdk))
  {
    sdk = Process::GetEnvVariable("ANDROID_SDK_ROOT");
  }

  if(sdk.empty() || !FileIO::exists(sdk))
  {
    sdk = Process::GetEnvVariable("ANDROID_SDK");
  }

  if(sdk.empty() || !FileIO::exists(sdk))
  {
    sdk = Process::GetEnvVariable("ANDROID_SDK_HOME");
  }

#if ENABLED(RDOC_APPLE)
  // on macOS it's common not to have the environment variable globally available, so try the home
  // Library folder first, then the global folder
  if(sdk.empty() || !FileIO::exists(sdk))
  {
    rdcstr librarySDK = FileIO::GetHomeFolderFilename() + "/Library/Android/sdk";
    sdk = FileIO::exists(librarySDK) ? librarySDK : "";
  }

  if(sdk.empty() || !FileIO::exists(sdk))
  {
    rdcstr librarySDK = "/Library/Android/sdk";
    sdk = FileIO::exists(librarySDK) ? librarySDK : "";
  }
#endif

  // maybe in future we can try to search in common install locations.

  toolpath = getToolInSDK(subdir, jdk, sdk, toolname);

  if(toolExists(toolpath))
    return toolpath;

  // finally try to locate it in our own distributed android subfolder
  {
    rdcstr libpath;
    FileIO::GetLibraryFilename(libpath);
    rdcstr libdir = get_dirname(FileIO::GetFullPathname(libpath));

    toolpath = libdir + "/plugins/android/" + toolname;
    if(toolExists(toolpath))
    {
      if(toolname == "adb")
      {
        // if we're using our own adb, we should kill the server upon shutdown
        adbKillServer = true;
      }

      return toolpath;
    }
  }

  toolpath = "";

  // if we're checking for existence, we have failed so return empty string.
  if(checkExist)
    return toolpath;

  // otherwise we at least return the tool name so that there's something to try and run
  return toolname;
}
Process::ProcessResult execScript(const rdcstr &script, const rdcstr &args, const rdcstr &workDir,
                                  bool silent)
{
  if(!silent)
    RDCLOG("SCRIPT: %s", script.c_str());

  Process::ProcessResult result;
  Process::LaunchScript(script, workDir, args, true, &result);
  return result;
}
Process::ProcessResult execCommand(const rdcstr &exe, const rdcstr &args, const rdcstr &workDir,
                                   bool silent)
{
  if(!silent)
    RDCLOG("COMMAND: %s '%s'", exe.c_str(), args.c_str());

  Process::ProcessResult result;
  Process::LaunchProcess(exe, workDir, args, true, &result);
  return result;
}
Process::ProcessResult adbExecCommand(const rdcstr &device, const rdcstr &args,
                                      const rdcstr &workDir, bool silent)
{
  rdcstr adb = getToolPath(ToolDir::PlatformTools, "adb", false);
  Process::ProcessResult result;
  rdcstr deviceArgs;
  if(device.empty())
    deviceArgs = args;
  else
    deviceArgs = StringFormat::Fmt("-s %s %s", device.c_str(), args.c_str());
  return execCommand(adb, deviceArgs, workDir, silent);
}
void initAdb()
{
  // we don't use adbExecCommand because we need to be sure we don't wait for it to exit
  rdcstr adb = getToolPath(ToolDir::PlatformTools, "adb", false);
  rdcstr workdir = ".";
  if(adb.contains('/') || adb.contains('\\'))
    workdir = get_dirname(adb);

  RDCLOG("Initialising adb using '%s'", adb.c_str());

  if(adb.empty() || (!FileIO::exists(adb) && !FileIO::exists(adb + ".exe")))
  {
    if(FileIO::FindFileInPath(adb) == "")
      RDCWARN(
          "Couldn't locate adb. Ensure adb is in PATH, ANDROID_SDK or ANDROID_HOME is set, or you "
          "configure your SDK location");
  }

  Process::ProcessResult res = {};
  Process::LaunchProcess(adb, workdir, "start-server", true, &res);

  if(res.strStdout.find("daemon") >= 0 || res.strStderror.find("daemon") >= 0)
  {
    RDCLOG("Started adb server");
  }
}
void shutdownAdb()
{
  if(adbKillServer)
    adbExecCommand("", "kill-server", ".", false);
}
};
