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
//#include "api/app/renderdoc_app.h"
#include "api/replay/capture_options.h"
#include "api/replay/control_types.h"
#include "common/threading.h"
#include "os/os_specific.h"
#include "strings/string_utils.h"

// defined in foo/foo_process.cpp
char **GetCurrentEnvironment();
int GetIdentPort(pid_t childPid);

// functions to try and let the child run just far enough to get to main() but no further. This lets
// us check the ident port and resume.
void StopAtMainInChild();
bool StopChildAtMain(pid_t childPid);
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

static std::map<rdcstr, rdcstr> EnvStringToEnvMap(const char **envstring)
{
  std::map<rdcstr, rdcstr> ret;

  const char **e = envstring;

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
    return rdcstr(getenv("HOME")) + path.substr(1);

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

void Process::RegisterEnvironmentModification(const EnvironmentModification &modif)
{
  GetEnvModifications().push_back(modif);
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
  // turn environment string to a UTF-8 map
  char **currentEnvironment = GetCurrentEnvironment();
  std::map<rdcstr, rdcstr> currentEnv = EnvStringToEnvMap((const char **)currentEnvironment);
  rdcarray<EnvironmentModification> &modifications = GetEnvModifications();

  for(size_t i = 0; i < modifications.size(); i++)
  {
    EnvironmentModification &m = modifications[i];

    rdcstr value = currentEnv[m.name.c_str()];

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

    setenv(m.name.c_str(), value.c_str(), true);
  }

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

static rdcarray<rdcstr> ParseCommandLine(const rdcstr &appName, const char *cmdLine)
{
  // argv[0] is the application name, by convention
  rdcarray<rdcstr> argv = {appName};

  const char *c = cmdLine;

  // parse command line into argv[], similar to how bash would
  if(cmdLine)
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
            RDCERR("Malformed command line:\n%s", cmdLine);
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
      RDCERR("Malformed command line\n%s", cmdLine);
      return {};
    }
  }

  return argv;
}

static pid_t RunProcess(const char *app, const char *workingDir, const char *cmdLine, char **envp,
                        bool pauseAtMain, int stdoutPipe[2] = NULL, int stderrPipe[2] = NULL)
{
  if(!app)
    return (pid_t)0;

  rdcstr appName = app;
  rdcstr workDir = (workingDir && workingDir[0]) ? workingDir : get_dirname(appName);

// handle funky apple .app folders that aren't actually executables
#if ENABLED(RDOC_APPLE)
  if(appName.size() > 5 && appName.endsWith(".app"))
  {
    rdcstr realAppName = appName + "/Contents/MacOS/" + get_basename(appName);
    realAppName.erase(realAppName.size() - 4, ~0U);

    if(FileIO::exists(realAppName.c_str()))
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

      if(pauseAtMain)
        StopChildAtMain(childPid);
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

rdcpair<ReplayStatus, uint32_t> Process::InjectIntoProcess(
    uint32_t pid, const rdcarray<EnvironmentModification> &env, const char *logfile,
    const CaptureOptions &opts, bool waitForExit)
{
  RDCUNIMPLEMENTED("Injecting into already running processes on linux");
  return {ReplayStatus::InjectionFailed, 0};
}

uint32_t Process::LaunchProcess(const char *app, const char *workingDir, const char *cmdLine,
                                bool internal, ProcessResult *result)
{
  if(app == NULL || app[0] == 0)
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

uint32_t Process::LaunchScript(const char *script, const char *workingDir, const char *argList,
                               bool internal, ProcessResult *result)
{
  // Change parameters to invoke command interpreter
  rdcstr args = "-lc \"" + rdcstr(script) + " " + rdcstr(argList) + "\"";

  return LaunchProcess("bash", workingDir, args.c_str(), internal, result);
}

rdcpair<ReplayStatus, uint32_t> Process::LaunchAndInjectIntoProcess(
    const char *app, const char *workingDir, const char *cmdLine,
    const rdcarray<EnvironmentModification> &envList, const char *capturefile,
    const CaptureOptions &opts, bool waitForExit)
{
  if(app == NULL || app[0] == 0)
  {
    RDCERR("Invalid empty 'app'");
    return {ReplayStatus::InternalError, 0};
  }

  // turn environment string to a UTF-8 map
  char **currentEnvironment = GetCurrentEnvironment();
  std::map<rdcstr, rdcstr> env = EnvStringToEnvMap((const char **)currentEnvironment);
  rdcarray<EnvironmentModification> modifications = GetEnvModifications();

  for(const EnvironmentModification &e : envList)
    modifications.push_back(e);

  if(capturefile == NULL)
    capturefile = "";

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

  rdcstr libfile = "librenderdoc" LIB_SUFFIX;

// on macOS, the path must be absolute
#if ENABLED(RDOC_APPLE)
  libfile = libpath + "/" + libfile;
#endif

  rdcstr optstr = opts.EncodeAsString();

  modifications.push_back(
      EnvironmentModification(EnvMod::Append, EnvSep::Platform, LIB_PATH_ENV_VAR, binpath.c_str()));
  modifications.push_back(
      EnvironmentModification(EnvMod::Append, EnvSep::Platform, LIB_PATH_ENV_VAR, libpath.c_str()));
  modifications.push_back(EnvironmentModification(EnvMod::Append, EnvSep::Platform,
                                                  LIB_PATH_ENV_VAR, ownlibpath.c_str()));
  modifications.push_back(
      EnvironmentModification(EnvMod::Append, EnvSep::Platform, PRELOAD_ENV_VAR, libfile.c_str()));
  modifications.push_back(
      EnvironmentModification(EnvMod::Set, EnvSep::NoSep, "RENDERDOC_CAPFILE", capturefile));
  modifications.push_back(
      EnvironmentModification(EnvMod::Set, EnvSep::NoSep, "RENDERDOC_CAPOPTS", optstr.c_str()));
  modifications.push_back(EnvironmentModification(EnvMod::Set, EnvSep::NoSep,
                                                  "RENDERDOC_DEBUG_LOG_FILE", RDCGETLOGFILE()));

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

  RDCLOG("Running process %s for injection", app);

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
  return {ret == 0 ? ReplayStatus::InjectionFailed : ReplayStatus::Succeeded, (uint32_t)ret};
}

bool Process::StartGlobalHook(const char *pathmatch, const char *logfile, const CaptureOptions &opts)
{
  RDCUNIMPLEMENTED("Global hooking of all processes on linux");
  return false;
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

bool Process::IsModuleLoaded(const char *module)
{
  return dlopen(module, RTLD_NOW | RTLD_NOLOAD) != NULL;
}

void *Process::LoadModule(const char *module)
{
  return dlopen(module, RTLD_NOW);
}

void *Process::GetFunctionAddress(void *module, const char *function)
{
  if(module == NULL)
    return NULL;

  return dlsym(module, function);
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

  SECTION("NULL command line")
  {
    args = ParseCommandLine("app", NULL);

    REQUIRE(args.size() == 1);
    CHECK(args[0] == "app");
  }

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
