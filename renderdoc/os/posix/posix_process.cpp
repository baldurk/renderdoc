/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2016-2019 Baldur Karlsson
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
#include "api/app/renderdoc_app.h"
#include "common/threading.h"
#include "os/os_specific.h"
#include "strings/string_utils.h"

// defined in foo/foo_process.cpp
char **GetCurrentEnvironment();
int GetIdentPort(pid_t childPid);

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
std::string FindFileInPath(const std::string &fileName);
};

static const std::string GetAbsoluteAppPathFromName(const std::string &appName)
{
  std::string appPath;

  // If the application name contains a slash character convert it to an absolute path and return it
  if(appName.find("/") != std::string::npos)
  {
    char realpathBuffer[PATH_MAX];
    std::string appDir = get_dirname(appName);
    std::string appBasename = get_basename(appName);
    realpath(appDir.c_str(), realpathBuffer);
    appPath = realpathBuffer;
    appPath += "/" + appBasename;
    return appPath;
  }

  // Otherwise, go search PATH for it
  return FileIO::FindFileInPath(appName);
}

static std::vector<EnvironmentModification> &GetEnvModifications()
{
  static std::vector<EnvironmentModification> envCallbacks;
  return envCallbacks;
}

static std::map<std::string, std::string> EnvStringToEnvMap(const char **envstring)
{
  std::map<std::string, std::string> ret;

  const char **e = envstring;

  while(*e)
  {
    const char *equals = strchr(*e, '=');

    if(equals == NULL)
    {
      e++;
      continue;
    }

    std::string name;
    std::string value;

    name.assign(*e, equals);
    value = equals + 1;

    ret[name] = value;

    e++;
  }

  return ret;
}

static std::string shellExpand(const std::string &in)
{
  std::string path = trim(in);

  // if it begins with ./ then replace with working directory
  if(path[0] == '.' && path[1] == '/')
  {
    char cwd[1024] = {};
    getcwd(cwd, 1023);
    return std::string(cwd) + path.substr(1);
  }

  // if it's ~/... then replace with $HOME and return
  if(path[0] == '~' && path[1] == '/')
    return std::string(getenv("HOME")) + path.substr(1);

  // if it's ~user/... then use getpwname
  if(path[0] == '~')
  {
    size_t slash = path.find('/');

    std::string username;

    if(slash != std::string::npos)
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
      if(slash != std::string::npos)
        return std::string(pwdata->pw_dir) + path.substr(slash);

      return std::string(pwdata->pw_dir);
    }
  }

  return path;
}

void Process::RegisterEnvironmentModification(EnvironmentModification modif)
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
  std::map<std::string, std::string> currentEnv =
      EnvStringToEnvMap((const char **)currentEnvironment);
  std::vector<EnvironmentModification> &modifications = GetEnvModifications();

  for(size_t i = 0; i < modifications.size(); i++)
  {
    EnvironmentModification &m = modifications[i];

    std::string value = currentEnv[m.name.c_str()];

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
          std::string prep = m.value;
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

static void CleanupStringArray(char **arr, char **invalid)
{
  if(arr != invalid)
  {
    char **arr_delete = arr;

    while(*arr)
    {
      delete[] * arr;
      arr++;
    }

    delete[] arr_delete;
  }
}

static pid_t RunProcess(const char *app, const char *workingDir, const char *cmdLine, char **envp,
                        int stdoutPipe[2] = NULL, int stderrPipe[2] = NULL)
{
  if(!app)
    return (pid_t)0;

  std::string appName = app;
  std::string workDir = (workingDir && workingDir[0]) ? workingDir : get_dirname(appName);

// handle funky apple .app folders that aren't actually executables
#if ENABLED(RDOC_APPLE)
  if(appName.size() > 5 && appName.rfind(".app") == appName.size() - 4)
  {
    std::string realAppName = appName + "/Contents/MacOS/" + get_basename(appName);
    realAppName.erase(realAppName.size() - 4);

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

  // it is safe to use app directly as execve never modifies argv
  char *emptyargv[] = {(char *)appName.c_str(), NULL};
  char **argv = emptyargv;

  const char *c = cmdLine;

  // parse command line into argv[], similar to how bash would
  if(cmdLine)
  {
    int argc = 1;

    // get a rough upper bound on the number of arguments
    while(*c)
    {
      if(*c == ' ' || *c == '\t')
        argc++;
      c++;
    }

    argv = new char *[argc + 2];
    memset(argv, 0, (argc + 2) * sizeof(char *));

    c = cmdLine;

    std::string a;

    argc = 0;    // current argument we're fetching

    // argv[0] is the application name, by convention
    size_t len = appName.length() + 1;
    argv[argc] = new char[len];
    strcpy(argv[argc], appName.c_str());

    argc++;

    bool dquot = false, squot = false;    // are we inside ''s or ""s
    while(*c)
    {
      if(!dquot && !squot && (*c == ' ' || *c == '\t'))
      {
        if(!a.empty())
        {
          // if we've fetched some number of non-space characters
          argv[argc] = new char[a.length() + 1];
          memcpy(argv[argc], a.c_str(), a.length() + 1);
          argc++;
        }

        a = "";
      }
      else if(!dquot && *c == '"')
      {
        dquot = true;
      }
      else if(!squot && *c == '\'')
      {
        squot = true;
      }
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
            CleanupStringArray(argv, emptyargv);
            RDCERR("Malformed command line:\n%s", cmdLine);
            return 0;
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

    if(!a.empty())
    {
      // if we've fetched some number of non-space characters
      argv[argc] = new char[a.length() + 1];
      memcpy(argv[argc], a.c_str(), a.length() + 1);
      argc++;
    }

    if(squot || dquot)
    {
      CleanupStringArray(argv, emptyargv);
      RDCERR("Malformed command line\n%s", cmdLine);
      return 0;
    }
  }

  const std::string appPath(GetAbsoluteAppPathFromName(appName));

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

  if(stdoutPipe)
  {
    // Close write ends, as parent will read.
    close(stdoutPipe[1]);
    close(stderrPipe[1]);
  }

  CleanupStringArray(argv, emptyargv);
  return childPid;
}

ExecuteResult Process::InjectIntoProcess(uint32_t pid, const rdcarray<EnvironmentModification> &env,
                                         const char *logfile, const CaptureOptions &opts,
                                         bool waitForExit)
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
  uint32_t ret = (uint32_t)RunProcess(app, workingDir, cmdLine, currentEnvironment,
                                      result ? stdoutPipe : NULL, result ? stderrPipe : NULL);

  if(ret && result)
  {
    result->strStdout = "";
    result->strStderror = "";

    ssize_t stdoutRead, stderrRead;
    char chBuf[4096];
    do
    {
      stdoutRead = read(stdoutPipe[0], chBuf, sizeof(chBuf));
      if(stdoutRead > 0)
        result->strStdout += std::string(chBuf, stdoutRead);
    } while(stdoutRead > 0);

    do
    {
      stderrRead = read(stderrPipe[0], chBuf, sizeof(chBuf));
      if(stderrRead > 0)
        result->strStderror += std::string(chBuf, stderrRead);

    } while(stderrRead > 0);

    // Close read ends.
    close(stdoutPipe[0]);
    close(stderrPipe[0]);
  }

  return ret;
}

uint32_t Process::LaunchScript(const char *script, const char *workingDir, const char *argList,
                               bool internal, ProcessResult *result)
{
  // Change parameters to invoke command interpreter
  std::string args = "-lc \"" + std::string(script) + " " + std::string(argList) + "\"";

  return LaunchProcess("bash", workingDir, args.c_str(), internal, result);
}

ExecuteResult Process::LaunchAndInjectIntoProcess(const char *app, const char *workingDir,
                                                  const char *cmdLine,
                                                  const rdcarray<EnvironmentModification> &envList,
                                                  const char *capturefile,
                                                  const CaptureOptions &opts, bool waitForExit)
{
  if(app == NULL || app[0] == 0)
  {
    RDCERR("Invalid empty 'app'");
    return {ReplayStatus::InternalError, 0};
  }

  // turn environment string to a UTF-8 map
  char **currentEnvironment = GetCurrentEnvironment();
  std::map<std::string, std::string> env = EnvStringToEnvMap((const char **)currentEnvironment);
  std::vector<EnvironmentModification> modifications = GetEnvModifications();

  for(const EnvironmentModification &e : envList)
    modifications.push_back(e);

  if(capturefile == NULL)
    capturefile = "";

  std::string binpath, libpath, ownlibpath;
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

  std::string libfile = "librenderdoc" LIB_SUFFIX;

// on macOS, the path must be absolute
#if ENABLED(RDOC_APPLE)
  libfile = libpath + "/" + libfile;
#endif

  std::string optstr = opts.EncodeAsString();

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

    std::string &value = env[m.name.c_str()];

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
    std::string envline = it->first + "=" + it->second;
    envp[i] = new char[envline.size() + 1];
    memcpy(envp[i], envline.c_str(), envline.size() + 1);
    i++;
  }

  pid_t childPid = RunProcess(app, workingDir, cmdLine, envp);

  int ret = 0;

  if(childPid != (pid_t)0)
  {
    // wait for child to have opened its socket
    usleep(1000);

    ret = GetIdentPort(childPid);

    if(waitForExit)
    {
      int dummy = 0;
      waitpid(childPid, &dummy, 0);
    }
  }

  CleanupStringArray(envp, NULL);
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

#include "3rdparty/catch/catch.hpp"

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
