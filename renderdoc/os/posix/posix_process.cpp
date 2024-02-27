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

#include <dlfcn.h>
#include <errno.h>
#include <limits.h>
#include <pwd.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <map>
#include "api/replay/capture_options.h"
#include "api/replay/control_types.h"
#include "common/formatting.h"
#include "common/threading.h"
#include "core/core.h"
#include "os/os_specific.h"
#include "strings/string_utils.h"

// defined in apple_helpers.mm
extern rdcstr apple_GetExecutablePathFromAppBundle(const char *appBundlePath);

// defined in foo/foo_process.cpp
char **GetCurrentEnvironment();
int GetIdentPort(pid_t childPid);

// functions to try and let the child run just far enough to get to main() but no further. This lets
// us check the ident port and resume.
void StopAtMainInChild();
bool StopChildAtMain(pid_t childPid, bool *exitWithNoExec);
void ResumeProcess(pid_t childPid, uint32_t delay = 0);

#if ENABLED(RDOC_APPLE)

#define PRELOAD_ENV_VAR "DYLD_INSERT_LIBRARIES"
#define LIB_PATH_ENV_VAR "DYLD_LIBRARY_PATH"
#define LIB_SUFFIX ".dylib"

#else

#define PRELOAD_ENV_VAR "LD_PRELOAD"
#define LIB_PATH_ENV_VAR "LD_LIBRARY_PATH"
#define LIB_SUFFIX ".so"

#endif

Threading::SpinLock zombieLock;

struct PIDNode
{
  PIDNode *next = NULL;
  pid_t pid = 0;
};

struct PIDList
{
  PIDNode *head = NULL;

  void append(PIDNode *node)
  {
    if(node == NULL)
      return;

    if(head == NULL)
    {
      head = node;
      return;
    }

    // we keep this super simple, just always iterate to the tail rather than keeping a tail
    // pointer. It's less efficient but these are short lists and accessed infrequently.
    PIDNode *tail = head;
    while(tail->next)
      tail = tail->next;

    tail->next = node;
  }

  void remove(PIDNode *node)
  {
    if(node == NULL)
      return;

    if(node == head)
    {
      // if the node we're removing is the head, just update our head pointer
      head = head->next;
      node->next = NULL;
    }
    else
    {
      // if it's not the head, then we iterate with two pointers - the previous element (starting at
      // head) and the current element (starting at head's next pointer). If cur ever points to our
      // node, we remove it from the list by updating prev's next pointer to point at cur's next
      // pointer.
      for(PIDNode *prev = head, *cur = head->next; cur;)
      {
        if(cur == node)
        {
          // remove cur from the list by modifying prev
          prev->next = cur->next;
          node->next = NULL;
          return;
        }

        prev = cur;
        cur = cur->next;
      }

      RDCERR("Couldn't find %p in list", node);
    }
  }

  PIDNode *pop_front()
  {
    PIDNode *ret = head;
    head = head->next;
    ret->next = NULL;
    return ret;
  }
};

PIDList children, freeChildren;

#if DISABLED(RDOC_ANDROID)

struct sigaction old_action;

static void ZombieWaiter(int signum, siginfo_t *handler_info, void *handler_context)
{
  // save errno
  int saved_errno = errno;

  // call the old handler
  if(old_action.sa_handler != SIG_IGN && old_action.sa_handler != SIG_DFL)
  {
    if(old_action.sa_flags & SA_SIGINFO)
      old_action.sa_sigaction(signum, handler_info, handler_context);
    else
      old_action.sa_handler(signum);
  }

  // we take the whole list here, process it and wait on all those PIDs, then restore it back at the
  // end. We only take the list itself, not the free list
  PIDList waitedChildren;
  PIDList localChildren;
  {
    SCOPED_SPINLOCK(zombieLock);
    std::swap(localChildren.head, children.head);
  }

  // wait for any children, but don't block (hang). We must only wait for our own PIDs as waiting
  // for any other handler's PIDs (like Qt's) might lose the one chance they have to wait for them
  // themselves.
  for(PIDNode *cur = localChildren.head; cur;)
  {
    // advance here immediately before potentially removing the node from the list
    PIDNode *pid = cur;
    cur = cur->next;

    // remember the child PIDs that we successfully waited on
    if(waitpid(pid->pid, NULL, WNOHANG) > 0)
    {
      // remove cur from the list
      localChildren.remove(pid);

      // add to the waitedChildren list
      waitedChildren.append(pid);
    }
  }

  // append the list back on rather than swapping again - since in between grabbing it above and
  // putting it back here there might have been a new child added.
  // Any waited children from the list are added to the free list
  {
    SCOPED_SPINLOCK(zombieLock);
    children.append(localChildren.head);
    freeChildren.append(waitedChildren.head);
  }

  // restore errno
  errno = saved_errno;
}

static void SetupZombieCollectionHandler()
{
  {
    SCOPED_SPINLOCK(zombieLock);

    static bool installed = false;
    if(installed)
      return;

    installed = true;
  }

  struct sigaction new_action = {};
  sigemptyset(&new_action.sa_mask);
  new_action.sa_flags = SA_NOCLDSTOP | SA_SIGINFO | SA_RESTART;
  new_action.sa_sigaction = &ZombieWaiter;

  sigaction(SIGCHLD, &new_action, &old_action);
}

#else    // ANDROID

static void SetupZombieCollectionHandler()
{
}

#endif

namespace FileIO
{
void ReleaseFDAfterFork();
rdcstr FindFileInPath(const rdcstr &fileName);
};

static const rdcstr GetAbsoluteAppPathFromName(const rdcstr &appName)
{
  rdcstr appPath;

  // If the application name contains a slash character convert it to an absolute path and return it
  if(appName.contains("/"))
  {
    char realpathBuffer[PATH_MAX];
    rdcstr appDir = get_dirname(appName);
    rdcstr appBasename = get_basename(appName);
    realpath(appDir.c_str(), realpathBuffer);
    appPath = realpathBuffer;
    appPath += "/" + appBasename;
    return appPath;
  }

  // Otherwise, go search PATH for it
  return FileIO::FindFileInPath(appName);
}

static rdcarray<EnvironmentModification> &GetEnvModifications()
{
  static rdcarray<EnvironmentModification> envCallbacks;
  return envCallbacks;
}

static std::map<rdcstr, rdcstr> EnvStringToEnvMap(char *const *envstring)
{
  std::map<rdcstr, rdcstr> ret;

  char *const *e = envstring;

  while(*e)
  {
    const char *equals = strchr(*e, '=');

    if(equals == NULL)
    {
      e++;
      continue;
    }

    rdcstr name = rdcstr(*e, equals - *e);
    rdcstr value = equals + 1;

    ret[name] = value;

    e++;
  }

  return ret;
}

static rdcstr shellExpand(const rdcstr &in)
{
  rdcstr path = in.trimmed();

  // if it begins with ./ then replace with working directory
  if(path[0] == '.' && path[1] == '/')
  {
    char cwd[1024] = {};
    getcwd(cwd, 1023);
    return rdcstr(cwd) + path.substr(1);
  }

  // if it's ~/... then replace with $HOME and return
  if(path[0] == '~' && path[1] == '/')
    return Process::GetEnvVariable("HOME") + path.substr(1);

  // if it's ~user/... then use getpwname
  if(path[0] == '~')
  {
    int slash = path.find('/');

    rdcstr username;

    if(slash >= 0)
    {
      RDCASSERT(slash > 1);
      username = path.substr(1, slash - 1);
    }
    else
    {
      username = path.substr(1);
    }

    passwd *pwdata = getpwnam(username.c_str());

    if(pwdata)
    {
      if(slash >= 0)
        return rdcstr(pwdata->pw_dir) + path.substr(slash);

      return rdcstr(pwdata->pw_dir);
    }
  }

  return path;
}

using PFN_setenv = decltype(&setenv);

int direct_setenv(const char *name, const char *value, int overwrite)
{
// on linux try to bypass any hooks to ensure we don't break (looking at you bash)
#if ENABLED(RDOC_LINUX)
  static PFN_setenv dyn_setenv = NULL;
  static bool checked = false;
  if(!checked)
  {
    checked = true;
    void *libc = dlopen("libc.so.6", RTLD_NOLOAD | RTLD_GLOBAL | RTLD_NOW);
    if(libc)
      dyn_setenv = (PFN_setenv)dlsym(libc, "setenv");
  }

  if(dyn_setenv)
    return dyn_setenv(name, value, overwrite);
#endif

  return setenv(name, value, overwrite);
}

void Process::RegisterEnvironmentModification(const EnvironmentModification &modif)
{
  GetEnvModifications().push_back(modif);
}

void ApplySingleEnvMod(EnvironmentModification &m, rdcstr &value)
{
  switch(m.mod)
  {
    case EnvMod::Set: value = m.value.c_str(); break;
    case EnvMod::Append:
    {
      if(!value.empty())
      {
        if(m.sep == EnvSep::Platform || m.sep == EnvSep::Colon)
          value += ":";
        else if(m.sep == EnvSep::SemiColon)
          value += ";";
      }
      value += m.value.c_str();
      break;
    }
    case EnvMod::Prepend:
    {
      if(!value.empty())
      {
        rdcstr prep = m.value;
        if(m.sep == EnvSep::Platform || m.sep == EnvSep::Colon)
          prep += ":";
        else if(m.sep == EnvSep::SemiColon)
          prep += ";";
        value = prep + value;
      }
      else
      {
        value = m.value.c_str();
      }
      break;
    }
  }
}

void ApplyEnvironmentModifications(rdcarray<EnvironmentModification> &modifications)
{
  // turn environment string to a UTF-8 map
  char **currentEnvironment = GetCurrentEnvironment();
  std::map<rdcstr, rdcstr> currentEnv = EnvStringToEnvMap(currentEnvironment);

  for(size_t i = 0; i < modifications.size(); i++)
  {
    EnvironmentModification &m = modifications[i];

    rdcstr &value = currentEnv[m.name.c_str()];

    ApplySingleEnvMod(m, value);

    direct_setenv(m.name.c_str(), value.c_str(), true);
  }
}

// on linux we apply environment changes before launching the program, as
// there is no support for injecting/loading renderdoc into a running program
// in any way, and we also have some environment changes that we *have* to make
// for correct hooking (LD_LIBRARY_PATH/LD_PRELOAD)
//
// However we still set environment variables so that we can modify variables while
// in process (e.g. if we notice a setting and want to enable an env var as a result)
void Process::ApplyEnvironmentModification()
{
  rdcarray<EnvironmentModification> &modifications = GetEnvModifications();
  ApplyEnvironmentModifications(modifications);

  // these have been applied to the current process
  modifications.clear();
}

static void CleanupStringArray(char **arr)
{
  char **arr_delete = arr;

  while(*arr)
  {
    delete[] * arr;
    arr++;
  }

  delete[] arr_delete;
}

static rdcarray<rdcstr> ParseCommandLine(const rdcstr &appName, const rdcstr &cmdLine)
{
  // argv[0] is the application name, by convention
  rdcarray<rdcstr> argv = {appName};

  const char *c = cmdLine.c_str();

  // parse command line into argv[], similar to how bash would
  if(!cmdLine.empty())
  {
    rdcstr a;
    bool haveArg = false;

    bool dquot = false, squot = false;    // are we inside ''s or ""s
    while(*c)
    {
      if(!dquot && !squot && (*c == ' ' || *c == '\t'))
      {
        if(!a.empty() || haveArg)
        {
          // if we've fetched some number of non-space characters
          argv.push_back(a);
        }

        a = "";
        haveArg = false;
      }
      // if we're not quoting at all and see a quote, enter that quote mode
      else if(!dquot && !squot && *c == '"')
      {
        dquot = true;
        haveArg = true;
      }
      else if(!dquot && !squot && *c == '\'')
      {
        squot = true;
        haveArg = true;
      }
      // exit quoting if we see the matching quote (we skip over escapes separately)
      else if(dquot && *c == '"')
      {
        dquot = false;
      }
      else if(squot && *c == '\'')
      {
        squot = false;
      }
      else if(squot)
      {
        // single quotes don't escape, just copy literally until we leave single quote mode
        a.push_back(*c);
      }
      else if(dquot)
      {
        // handle escaping
        if(*c == '\\')
        {
          c++;
          if(*c)
          {
            a.push_back(*c);
          }
          else
          {
            RDCERR("Malformed command line:\n%s", cmdLine.c_str());
            return {};
          }
        }
        else
        {
          a.push_back(*c);
        }
      }
      else
      {
        a.push_back(*c);
      }

      c++;
    }

    if(!a.empty() || haveArg)
    {
      // if we've fetched some number of non-space characters
      argv.push_back(a);
    }

    if(squot || dquot)
    {
      RDCERR("Malformed command line\n%s", cmdLine.c_str());
      return {};
    }
  }

  return argv;
}

static pid_t RunProcess(rdcstr appName, rdcstr workDir, const rdcstr &cmdLine, char **envp,
                        bool pauseAtMain, int stdoutPipe[2] = NULL, int stderrPipe[2] = NULL)
{
  if(appName.empty())
    return (pid_t)0;

  if(workDir.empty())
    workDir = get_dirname(appName);

// handle funky apple .app folders that aren't actually executables
#if ENABLED(RDOC_APPLE)
  if(appName.size() > 5 && appName.endsWith(".app"))
  {
    rdcstr realAppName = apple_GetExecutablePathFromAppBundle(appName.c_str());
    if(realAppName.empty())
    {
      RDCERR("Invalid application path '%s'", appName.c_str());
      return (pid_t)0;
    }

    if(FileIO::exists(realAppName))
    {
      RDCLOG("Running '%s' the actual executable for '%s'", realAppName.c_str(), appName.c_str());
      appName = realAppName;
    }
  }
#endif

  // do very limited expansion. wordexp(3) does too much for our needs, so we just expand ~
  // since that could be quite a common case.
  appName = shellExpand(appName);
  workDir = shellExpand(workDir);

  rdcarray<rdcstr> argvList = ParseCommandLine(appName, cmdLine);

  if(argvList.empty())
    return 0;

  char **argv = new char *[argvList.size() + 1];
  for(size_t i = 0; i < argvList.size(); i++)
    argv[i] = argvList[i].data();
  argv[argvList.size()] = NULL;

  const rdcstr appPath(GetAbsoluteAppPathFromName(appName));

  pid_t childPid = 0;

  // don't fork if we didn't find anything to execute.
  if(!appPath.empty())
  {
    // we have to handle child processes with wait otherwise they become zombies. Unfortunately Qt
    // breaks if we just ignore the signal and let the system reap them.
    SetupZombieCollectionHandler();

    childPid = fork();
    if(childPid == 0)
    {
      if(pauseAtMain)
        StopAtMainInChild();

      FileIO::ReleaseFDAfterFork();
      if(stdoutPipe)
      {
        // Redirect stdout & stderr write ends.
        dup2(stdoutPipe[1], STDOUT_FILENO);
        dup2(stderrPipe[1], STDERR_FILENO);

        // now close all pipe handles - we don't need the read ends, and the write ends have been
        // duplicated into stdout/stderr above - we don't want these handles to be inherited into
        // child processes.
        close(stdoutPipe[0]);
        close(stderrPipe[0]);
        close(stdoutPipe[1]);
        close(stderrPipe[1]);
      }

      chdir(workDir.c_str());
      execve(appPath.c_str(), argv, envp);
      fprintf(stderr, "exec failed\n");
      _exit(1);
    }
    else
    {
      if(pauseAtMain)
        StopChildAtMain(childPid, NULL);

      if(!stdoutPipe)
      {
        // remember this PID so we can wait on it later
        SCOPED_SPINLOCK(zombieLock);

        PIDNode *node = NULL;

        // take a child from the free list if available, otherwise allocate a new one
        if(freeChildren.head)
          node = freeChildren.pop_front();
        else
          node = new PIDNode();

        node->pid = childPid;

        children.append(node);
      }
    }
  }

  if(stdoutPipe)
  {
    // Close write ends, as parent will read.
    close(stdoutPipe[1]);
    close(stderrPipe[1]);
  }

  delete[] argv;
  return childPid;
}

rdcpair<RDResult, uint32_t> Process::InjectIntoProcess(uint32_t pid,
                                                       const rdcarray<EnvironmentModification> &env,
                                                       const rdcstr &logfile,
                                                       const CaptureOptions &opts, bool waitForExit)
{
  RDCUNIMPLEMENTED("Injecting into already running processes on linux");
  return {
      RDResult(ResultCode::InjectionFailed,
               "Injecting into already running processes is not supported on non-Windows systems"),
      0};
}

uint32_t Process::LaunchProcess(const rdcstr &app, const rdcstr &workingDir, const rdcstr &cmdLine,
                                bool internal, ProcessResult *result)
{
  if(app.empty())
  {
    RDCERR("Invalid empty 'app'");
    return 0;
  }

  int stdoutPipe[2], stderrPipe[2];
  if(result)
  {
    if(pipe(stdoutPipe) == -1)
      RDCERR("Could not create stdout pipe");
    if(pipe(stderrPipe) == -1)
      RDCERR("Could not create stderr pipe");
  }

  char **currentEnvironment = GetCurrentEnvironment();
  pid_t ret = RunProcess(app, workingDir, cmdLine, currentEnvironment, false,
                         result ? stdoutPipe : NULL, result ? stderrPipe : NULL);

  if(result)
  {
    result->strStdout = "";
    result->strStderror = "";

    if(ret)
    {
      ssize_t stdoutRead, stderrRead;
      char chBuf[4096];
      do
      {
        stdoutRead = read(stdoutPipe[0], chBuf, sizeof(chBuf));
        if(stdoutRead > 0)
          result->strStdout += rdcstr(chBuf, stdoutRead);
      } while(stdoutRead > 0);

      do
      {
        stderrRead = read(stderrPipe[0], chBuf, sizeof(chBuf));
        if(stderrRead > 0)
          result->strStderror += rdcstr(chBuf, stderrRead);

      } while(stderrRead > 0);

      result->retCode = 1;
      pid_t p;
      int status;
      while((p = waitpid(ret, &status, WUNTRACED | WCONTINUED)) < 0 && errno == EINTR)
      {
        RDCLOG("Waiting on pid %u to exit", ret);
      }

      if(p < 0)
        RDCLOG("Failed to wait on pid %u, error: %d", ret, p, errno);
      else if(WIFEXITED(status))
        result->retCode = WEXITSTATUS(status);
      else
        RDCWARN("Process did not exit normally");
    }

    // Close read ends.
    close(stdoutPipe[0]);
    close(stderrPipe[0]);
  }

  return (uint32_t)ret;
}

uint32_t Process::LaunchScript(const rdcstr &script, const rdcstr &workingDir,
                               const rdcstr &argList, bool internal, ProcessResult *result)
{
  // Change parameters to invoke command interpreter
  rdcstr args = "-lc \"" + script + " " + argList + "\"";

  return LaunchProcess("bash", workingDir, args, internal, result);
}

void GetHookingEnvMods(rdcarray<EnvironmentModification> &modifications, const CaptureOptions &opts,
                       const rdcstr &capturefile)
{
  rdcstr binpath, libpath, ownlibpath;
  {
    FileIO::GetExecutableFilename(binpath);
    binpath = get_dirname(binpath);
    libpath = binpath + "/../lib";

// point to the right customiseable path
#if defined(RENDERDOC_LIB_SUFFIX)
    libpath += STRINGIZE(RENDERDOC_LIB_SUFFIX);
#endif

#if defined(RENDERDOC_LIB_SUBFOLDER)
    libpath += "/" STRINGIZE(RENDERDOC_LIB_SUBFOLDER);
#endif
  }

  FileIO::GetLibraryFilename(ownlibpath);
  ownlibpath = get_dirname(ownlibpath);

  rdcstr libfile = "lib" STRINGIZE(RDOC_BASE_NAME) LIB_SUFFIX;

// on macOS, the path must be absolute
#if ENABLED(RDOC_APPLE)
  FileIO::GetLibraryFilename(libfile);
#endif

  rdcstr optstr = opts.EncodeAsString();

  modifications.push_back(EnvironmentModification(EnvMod::Append, EnvSep::Platform,
                                                  "RENDERDOC_ORIGLIBPATH",
                                                  Process::GetEnvVariable(LIB_PATH_ENV_VAR)));
  modifications.push_back(EnvironmentModification(EnvMod::Append, EnvSep::Platform,
                                                  "RENDERDOC_ORIGPRELOAD",
                                                  Process::GetEnvVariable(PRELOAD_ENV_VAR)));
  modifications.push_back(
      EnvironmentModification(EnvMod::Append, EnvSep::Platform, LIB_PATH_ENV_VAR, binpath));
  modifications.push_back(
      EnvironmentModification(EnvMod::Append, EnvSep::Platform, LIB_PATH_ENV_VAR, libpath));
  modifications.push_back(
      EnvironmentModification(EnvMod::Append, EnvSep::Platform, LIB_PATH_ENV_VAR, ownlibpath));
  modifications.push_back(
      EnvironmentModification(EnvMod::Append, EnvSep::Platform, PRELOAD_ENV_VAR, libfile));
  modifications.push_back(
      EnvironmentModification(EnvMod::Set, EnvSep::NoSep, "RENDERDOC_CAPFILE", capturefile));
  modifications.push_back(
      EnvironmentModification(EnvMod::Set, EnvSep::NoSep, "RENDERDOC_CAPOPTS", optstr));
  modifications.push_back(EnvironmentModification(EnvMod::Set, EnvSep::NoSep,
                                                  "RENDERDOC_DEBUG_LOG_FILE", RDCGETLOGFILE()));
}

void PreForkConfigureHooks()
{
  rdcarray<EnvironmentModification> modifications;

  GetHookingEnvMods(modifications, RenderDoc::Inst().GetCaptureOptions(),
                    RenderDoc::Inst().GetCaptureFileTemplate());

  ApplyEnvironmentModifications(modifications);
}

void GetUnhookedEnvp(char *const *envp, rdcstr &envpStr, rdcarray<char *> &modifiedEnv)
{
  std::map<rdcstr, rdcstr> envmap = EnvStringToEnvMap(envp);

  // this is a nasty hack. We set this env var when we inject into a child, but because we don't
  // know when vulkan may be initialised we need to leave it on indefinitely. If we're not
  // injecting into children we need to unset this variable so it doesn't get inherited.
  envmap.erase(RENDERDOC_VULKAN_LAYER_VAR);

  envpStr.clear();

  // flatten the map to a string
  for(auto it = envmap.begin(); it != envmap.end(); it++)
  {
    envpStr += it->first;
    envpStr += "=";
    envpStr += it->second;
    envpStr.push_back('\0');
  }
  envpStr.push_back('\0');

  // create the array desired
  char *c = envpStr.data();
  while(*c)
  {
    modifiedEnv.push_back(c);
    c += strlen(c) + 1;
  }
  modifiedEnv.push_back(NULL);
}

void GetHookedEnvp(char *const *envp, rdcstr &envpStr, rdcarray<char *> &modifiedEnv)
{
  rdcarray<EnvironmentModification> modifications;

  GetHookingEnvMods(modifications, RenderDoc::Inst().GetCaptureOptions(),
                    RenderDoc::Inst().GetCaptureFileTemplate());

  std::map<rdcstr, rdcstr> envmap = EnvStringToEnvMap(envp);

  for(EnvironmentModification &mod : modifications)
  {
    // update the values for original values we're storing, since they were gotten by querying the
    // *current* environment not envp here.
    if(mod.name == "RENDERDOC_ORIGLIBPATH")
      mod.value = envmap[LIB_PATH_ENV_VAR];
    else if(mod.name == "RENDERDOC_ORIGPRELOAD")
      mod.value = envmap[PRELOAD_ENV_VAR];

    // modify the map in-place
    ApplySingleEnvMod(mod, envmap[mod.name.c_str()]);
  }

  envpStr.clear();

  // flatten the map to a string
  for(auto it = envmap.begin(); it != envmap.end(); it++)
  {
    envpStr += it->first;
    envpStr += "=";
    envpStr += it->second;
    envpStr.push_back('\0');
  }
  envpStr.push_back('\0');

  // create the array desired
  char *c = envpStr.data();
  while(*c)
  {
    modifiedEnv.push_back(c);
    c += strlen(c) + 1;
  }
  modifiedEnv.push_back(NULL);
}

void ResetHookingEnvVars()
{
  direct_setenv(LIB_PATH_ENV_VAR, Process::GetEnvVariable("RENDERDOC_ORIGLIBPATH").c_str(), true);
  direct_setenv(PRELOAD_ENV_VAR, Process::GetEnvVariable("RENDERDOC_ORIGPRELOAD").c_str(), true);
  direct_setenv("RENDERDOC_ORIGLIBPATH", "", true);
  direct_setenv("RENDERDOC_ORIGPRELOAD", "", true);
}

rdcpair<RDResult, uint32_t> Process::LaunchAndInjectIntoProcess(
    const rdcstr &app, const rdcstr &workingDir, const rdcstr &cmdLine,
    const rdcarray<EnvironmentModification> &envList, const rdcstr &capturefile,
    const CaptureOptions &opts, bool waitForExit)
{
  if(app.empty())
  {
    RDResult result;
    SET_ERROR_RESULT(result, ResultCode::InvalidParameter, "Invalid empty path to launch.");
    return {result, 0};
  }

  // turn environment string to a UTF-8 map
  char **currentEnvironment = GetCurrentEnvironment();
  std::map<rdcstr, rdcstr> env = EnvStringToEnvMap(currentEnvironment);
  rdcarray<EnvironmentModification> modifications = GetEnvModifications();

  for(const EnvironmentModification &e : envList)
    modifications.push_back(e);

  GetHookingEnvMods(modifications, opts, capturefile);

  for(size_t i = 0; i < modifications.size(); i++)
  {
    EnvironmentModification &m = modifications[i];

    rdcstr &value = env[m.name.c_str()];

    switch(m.mod)
    {
      case EnvMod::Set: value = m.value.c_str(); break;
      case EnvMod::Append:
      {
        if(!value.empty())
        {
          if(m.sep == EnvSep::Platform || m.sep == EnvSep::Colon)
            value += ":";
          else if(m.sep == EnvSep::SemiColon)
            value += ";";
        }
        value += m.value.c_str();
        break;
      }
      case EnvMod::Prepend:
      {
        if(!value.empty())
        {
          if(m.sep == EnvSep::Platform || m.sep == EnvSep::Colon)
            value += ":";
          else if(m.sep == EnvSep::SemiColon)
            value += ";";
        }
        else
        {
          value = m.value.c_str();
        }
        break;
      }
    }
  }

  char **envp = new char *[env.size() + 1];
  envp[env.size()] = NULL;

  int i = 0;
  for(auto it = env.begin(); it != env.end(); it++)
  {
    rdcstr envline = it->first + "=" + it->second;
    envp[i] = new char[envline.size() + 1];
    memcpy(envp[i], envline.c_str(), envline.size() + 1);
    i++;
  }

  RDCLOG("Running process %s for injection", app.c_str());

  pid_t childPid = RunProcess(app, workingDir, cmdLine, envp, true);

  int ret = 0;

  if(childPid != (pid_t)0)
  {
    // ideally we stopped at main so we can check the port immediately. Otherwise this will do an
    // exponential wait to get it as soon as possible
    ret = GetIdentPort(childPid);

    ResumeProcess(childPid, opts.delayForDebugger);

    if(waitForExit)
    {
      int dummy = 0;
      waitpid(childPid, &dummy, 0);
    }
  }

  CleanupStringArray(envp);
  RDResult result;
  if(ret == 0)
  {
    SET_ERROR_RESULT(result, ResultCode::InjectionFailed,
                     "Couldn't connect to target program. Check that it didn't crash or exit "
                     "during early initialisation, e.g. due to an incorrectly configured working "
                     "directory.");
  }
  return {result, (uint32_t)ret};
}

RDResult Process::StartGlobalHook(const rdcstr &pathmatch, const rdcstr &logfile,
                                  const CaptureOptions &opts)
{
  RDCUNIMPLEMENTED("Global hooking of all processes on linux");
  return RDResult(ResultCode::InvalidParameter,
                  "Global hooking is not supported on non-Windows systems");
}

bool Process::CanGlobalHook()
{
  return false;
}

bool Process::IsGlobalHookActive()
{
  return false;
}

void Process::StopGlobalHook()
{
}

bool Process::IsModuleLoaded(const rdcstr &module)
{
  return dlopen(module.c_str(), RTLD_NOW | RTLD_NOLOAD) != NULL;
}

void *Process::LoadModule(const rdcstr &module)
{
  return dlopen(module.c_str(), RTLD_NOW);
}

void *Process::GetFunctionAddress(void *module, const rdcstr &function)
{
  if(module == NULL)
    return NULL;

  return dlsym(module, function.c_str());
}

uint32_t Process::GetCurrentPID()
{
  return (uint32_t)getpid();
}

void Process::Shutdown()
{
  // delete all items in the freeChildren list
  for(PIDNode *cur = freeChildren.head; cur;)
  {
    PIDNode *del = cur;
    cur = cur->next;
    delete del;
  }
}

#if ENABLED(ENABLE_UNIT_TESTS)

#include "catch/catch.hpp"

TEST_CASE("Test command line parsing", "[osspecific]")
{
  rdcarray<rdcstr> args;

  SECTION("empty command line")
  {
    args = ParseCommandLine("app", "");

    REQUIRE(args.size() == 1);
    CHECK(args[0] == "app");

    args = ParseCommandLine("app", "   ");

    REQUIRE(args.size() == 1);
    CHECK(args[0] == "app");

    args = ParseCommandLine("app", "  \t  \t ");

    REQUIRE(args.size() == 1);
    CHECK(args[0] == "app");
  }

  SECTION("whitespace command line")
  {
    args = ParseCommandLine("app", "'   '");

    REQUIRE(args.size() == 2);
    CHECK(args[0] == "app");
    CHECK(args[1] == "   ");

    args = ParseCommandLine("app", "   '   '");

    REQUIRE(args.size() == 2);
    CHECK(args[0] == "app");
    CHECK(args[1] == "   ");

    args = ParseCommandLine("app", "   '   '   ");

    REQUIRE(args.size() == 2);
    CHECK(args[0] == "app");
    CHECK(args[1] == "   ");

    args = ParseCommandLine("app", "   \"   \"   ");

    REQUIRE(args.size() == 2);
    CHECK(args[0] == "app");
    CHECK(args[1] == "   ");
  }

  SECTION("a single parameter")
  {
    args = ParseCommandLine("app", "--foo");

    REQUIRE(args.size() == 2);
    CHECK(args[0] == "app");
    CHECK(args[1] == "--foo");

    args = ParseCommandLine("app", "--bar");

    REQUIRE(args.size() == 2);
    CHECK(args[0] == "app");
    CHECK(args[1] == "--bar");

    args = ParseCommandLine("app", "/a/path/to/somewhere");

    REQUIRE(args.size() == 2);
    CHECK(args[0] == "app");
    CHECK(args[1] == "/a/path/to/somewhere");
  }

  SECTION("multiple parameters")
  {
    args = ParseCommandLine("app", "--foo --bar   ");

    REQUIRE(args.size() == 3);
    CHECK(args[0] == "app");
    CHECK(args[1] == "--foo");
    CHECK(args[2] == "--bar");

    args = ParseCommandLine("app", "  --qux    \t   --asdf");

    REQUIRE(args.size() == 3);
    CHECK(args[0] == "app");
    CHECK(args[1] == "--qux");
    CHECK(args[2] == "--asdf");

    args = ParseCommandLine("app", "--path /a/path/to/somewhere    --many --param a   b c     d ");

    REQUIRE(args.size() == 9);
    CHECK(args[0] == "app");
    CHECK(args[1] == "--path");
    CHECK(args[2] == "/a/path/to/somewhere");
    CHECK(args[3] == "--many");
    CHECK(args[4] == "--param");
    CHECK(args[5] == "a");
    CHECK(args[6] == "b");
    CHECK(args[7] == "c");
    CHECK(args[8] == "d");
  }

  SECTION("parameters with single quotes")
  {
    args = ParseCommandLine("app", "'single quoted single parameter'");

    REQUIRE(args.size() == 2);
    CHECK(args[0] == "app");
    CHECK(args[1] == "single quoted single parameter");

    args = ParseCommandLine("app", "      'single quoted single parameter'  ");

    REQUIRE(args.size() == 2);
    CHECK(args[0] == "app");
    CHECK(args[1] == "single quoted single parameter");

    args = ParseCommandLine("app", "      'single quoted \t\tsingle parameter'  ");

    REQUIRE(args.size() == 2);
    CHECK(args[0] == "app");
    CHECK(args[1] == "single quoted \t\tsingle parameter");

    args = ParseCommandLine("app", "   --thing='single quoted single parameter'  ");

    REQUIRE(args.size() == 2);
    CHECK(args[0] == "app");
    CHECK(args[1] == "--thing=single quoted single parameter");

    args = ParseCommandLine("app", " 'quoted string with \"double quotes inside\" it' ");

    REQUIRE(args.size() == 2);
    CHECK(args[0] == "app");
    CHECK(args[1] == "quoted string with \"double quotes inside\" it");

    args =
        ParseCommandLine("app", " --multiple --params 'single quoted parameter'  --with --quotes ");

    REQUIRE(args.size() == 6);
    CHECK(args[0] == "app");
    CHECK(args[1] == "--multiple");
    CHECK(args[2] == "--params");
    CHECK(args[3] == "single quoted parameter");
    CHECK(args[4] == "--with");
    CHECK(args[5] == "--quotes");

    args = ParseCommandLine("app", "--explicit '' --empty");

    REQUIRE(args.size() == 4);
    CHECK(args[0] == "app");
    CHECK(args[1] == "--explicit");
    CHECK(args[2] == "");
    CHECK(args[3] == "--empty");

    args = ParseCommandLine("app", "--explicit '  ' --spaces");

    REQUIRE(args.size() == 4);
    CHECK(args[0] == "app");
    CHECK(args[1] == "--explicit");
    CHECK(args[2] == "  ");
    CHECK(args[3] == "--spaces");

    args = ParseCommandLine("app", "--explicit ''");

    REQUIRE(args.size() == 3);
    CHECK(args[0] == "app");
    CHECK(args[1] == "--explicit");
    CHECK(args[2] == "");

    args = ParseCommandLine("app", "--explicit '  '");

    REQUIRE(args.size() == 3);
    CHECK(args[0] == "app");
    CHECK(args[1] == "--explicit");
    CHECK(args[2] == "  ");
  }

  SECTION("parameters with double quotes")
  {
    args = ParseCommandLine("app", "\"double quoted single parameter\"");

    REQUIRE(args.size() == 2);
    CHECK(args[0] == "app");
    CHECK(args[1] == "double quoted single parameter");

    args = ParseCommandLine("app", "      \"double quoted single parameter\"  ");

    REQUIRE(args.size() == 2);
    CHECK(args[0] == "app");
    CHECK(args[1] == "double quoted single parameter");

    args = ParseCommandLine("app", "      \"double quoted \t\tsingle parameter\"  ");

    REQUIRE(args.size() == 2);
    CHECK(args[0] == "app");
    CHECK(args[1] == "double quoted \t\tsingle parameter");

    args = ParseCommandLine("app", "   --thing=\"double quoted single parameter\"  ");

    REQUIRE(args.size() == 2);
    CHECK(args[0] == "app");
    CHECK(args[1] == "--thing=double quoted single parameter");

    args = ParseCommandLine("app", " \"quoted string with \\\"double quotes inside\\\" it\" ");

    REQUIRE(args.size() == 2);
    CHECK(args[0] == "app");
    CHECK(args[1] == "quoted string with \"double quotes inside\" it");

    args = ParseCommandLine("app", " \"string's contents has a quoted quote\" ");

    REQUIRE(args.size() == 2);
    CHECK(args[0] == "app");
    CHECK(args[1] == "string's contents has a quoted quote");

    args =
        ParseCommandLine("app", " --multiple --params 'double quoted parameter'  --with --quotes ");

    REQUIRE(args.size() == 6);
    CHECK(args[0] == "app");
    CHECK(args[1] == "--multiple");
    CHECK(args[2] == "--params");
    CHECK(args[3] == "double quoted parameter");
    CHECK(args[4] == "--with");
    CHECK(args[5] == "--quotes");

    args = ParseCommandLine("app", "--explicit \"\" --empty");

    REQUIRE(args.size() == 4);
    CHECK(args[0] == "app");
    CHECK(args[1] == "--explicit");
    CHECK(args[2] == "");
    CHECK(args[3] == "--empty");

    args = ParseCommandLine("app", "--explicit \"  \" --spaces");

    REQUIRE(args.size() == 4);
    CHECK(args[0] == "app");
    CHECK(args[1] == "--explicit");
    CHECK(args[2] == "  ");
    CHECK(args[3] == "--spaces");

    args = ParseCommandLine("app", "--explicit \"\"");

    REQUIRE(args.size() == 3);
    CHECK(args[0] == "app");
    CHECK(args[1] == "--explicit");
    CHECK(args[2] == "");

    args = ParseCommandLine("app", "--explicit \"  \"");

    REQUIRE(args.size() == 3);
    CHECK(args[0] == "app");
    CHECK(args[1] == "--explicit");
    CHECK(args[2] == "  ");
  }

  SECTION("concatenated quotes")
  {
    args = ParseCommandLine("app", "'foo''bar''blah'");

    REQUIRE(args.size() == 2);
    CHECK(args[0] == "app");
    CHECK(args[1] == "foobarblah");

    args = ParseCommandLine("app", "\"foo\"\"bar\"\"blah\"");

    REQUIRE(args.size() == 2);
    CHECK(args[0] == "app");
    CHECK(args[1] == "foobarblah");

    args = ParseCommandLine("app", "\"foo\"'bar'\"blah\"");

    REQUIRE(args.size() == 2);
    CHECK(args[0] == "app");
    CHECK(args[1] == "foobarblah");

    args = ParseCommandLine("app", "\"foo\"'bar'\"blah\"");

    REQUIRE(args.size() == 2);
    CHECK(args[0] == "app");
    CHECK(args[1] == "foobarblah");

    args = ParseCommandLine("app", "foo'bar'blah");

    REQUIRE(args.size() == 2);
    CHECK(args[0] == "app");
    CHECK(args[1] == "foobarblah");

    args = ParseCommandLine("app", "foo\"bar\"blah");

    REQUIRE(args.size() == 2);
    CHECK(args[0] == "app");
    CHECK(args[1] == "foobarblah");

    args = ParseCommandLine("app", "\"string with spaces\"' and other string'");

    REQUIRE(args.size() == 2);
    CHECK(args[0] == "app");
    CHECK(args[1] == "string with spaces and other string");
  }
}

TEST_CASE("Test PID Node list handling", "[osspecific]")
{
  PIDNode *a = new PIDNode;
  a->pid = (pid_t)500;

  PIDList list1;

  list1.append(a);

  CHECK(list1.head == a);

  PIDNode *b = new PIDNode;
  b->pid = (pid_t)501;

  list1.append(b);

  CHECK(list1.head == a);
  CHECK(list1.head->next == b);

  PIDNode *c = new PIDNode;
  c->pid = (pid_t)502;

  list1.append(c);

  CHECK(list1.head == a);
  CHECK(list1.head->next == b);
  CHECK(list1.head->next->next == c);

  PIDNode *popped = list1.pop_front();

  CHECK(popped == a);

  CHECK(list1.head == b);
  CHECK(list1.head->next == c);

  list1.append(popped);

  CHECK(list1.head == b);
  CHECK(list1.head->next == c);
  CHECK(list1.head->next->next == a);

  list1.remove(c);

  CHECK(list1.head == b);
  CHECK(list1.head->next == a);

  list1.append(c);

  CHECK(list1.head == b);
  CHECK(list1.head->next == a);
  CHECK(list1.head->next->next == c);

  list1.remove(c);

  CHECK(list1.head == b);
  CHECK(list1.head->next == a);

  list1.append(c);

  CHECK(list1.head == b);
  CHECK(list1.head->next == a);
  CHECK(list1.head->next->next == c);

  list1.remove(b);

  CHECK(list1.head == a);
  CHECK(list1.head->next == c);

  list1.append(b);

  CHECK(list1.head == a);
  CHECK(list1.head->next == c);
  CHECK(list1.head->next->next == b);

  PIDNode *d = new PIDNode;
  d->pid = (pid_t)900;
  PIDNode *e = new PIDNode;
  b->pid = (pid_t)901;
  PIDNode *f = new PIDNode;
  c->pid = (pid_t)902;

  PIDList list2;

  list2.append(d);
  list2.append(e);
  list2.append(f);

  list1.append(list2.head);

  CHECK(list1.head == a);
  CHECK(list1.head->next == c);
  CHECK(list1.head->next->next == b);
  CHECK(list1.head->next->next->next == d);
  CHECK(list1.head->next->next->next->next == e);
  CHECK(list1.head->next->next->next->next->next == f);

  delete a;
  delete b;
  delete c;
  delete d;
  delete e;
  delete f;
};

#endif    // ENABLED(ENABLE_UNIT_TESTS)
