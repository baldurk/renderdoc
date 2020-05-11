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

#include <dirent.h>
#include <dlfcn.h>    // for dladdr
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include "api/app/renderdoc_app.h"
#include "api/replay/data_types.h"
#include "common/formatting.h"
#include "common/threading.h"
#include "os/os_specific.h"
#include "strings/string_utils.h"

// gives us an address to identify this so with
static int soLocator = 0;

namespace FileIO
{
// in posix/.../..._stringio.cpp
rdcstr GetTempRootPath();

rdcstr GetHomeFolderFilename()
{
  passwd *pw = getpwuid(getuid());
  const char *homedir = pw->pw_dir;

  return homedir;
}

rdcstr GetTempFolderFilename()
{
  return GetTempRootPath() + "/";
}

void CreateParentDirectory(const rdcstr &filename)
{
  rdcstr fn = get_dirname(filename);

  // want trailing slash so that we create all directories
  fn.push_back('/');

  int offs = fn.find('/', 1);

  while(offs >= 0)
  {
    // create directory path from 0 to offs by NULLing the
    // / at offs, mkdir, then set it back
    fn[offs] = 0;
    mkdir(fn.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    fn[offs] = '/';

    offs = fn.find('/', offs + 1);
  }
}

bool IsRelativePath(const rdcstr &path)
{
  if(path.empty())
    return false;

  return path.front() != '/';
}

rdcstr GetFullPathname(const rdcstr &filename)
{
  char path[PATH_MAX + 1] = {0};
  realpath(filename.c_str(), path);

  return rdcstr(path);
}

rdcstr FindFileInPath(const rdcstr &fileName)
{
  rdcstr filePath;

  // Search the PATH directory list for the application (like shell which) to get the absolute path
  // Return "" if no exectuable found in the PATH list
  char *pathEnvVar = getenv("PATH");
  if(!pathEnvVar)
    return filePath;

  // Make a copy of our PATH so strtok can insert NULL without actually changing env
  char *localPath = new char[strlen(pathEnvVar) + 1];
  strcpy(localPath, pathEnvVar);

  const char *pathSeparator = ":";
  const char *path = strtok(localPath, pathSeparator);
  while(path)
  {
    rdcstr testPath(path);
    testPath += "/" + fileName;
    if(!access(testPath.c_str(), X_OK))
    {
      filePath = testPath;
      break;
    }
    path = strtok(NULL, pathSeparator);
  }

  delete[] localPath;
  return filePath;
}

rdcstr GetReplayAppFilename()
{
  // look up the shared object's path via dladdr
  Dl_info info;
  dladdr((void *)&soLocator, &info);
  rdcstr path = info.dli_fname ? info.dli_fname : "";
  path = get_dirname(path);
  rdcstr replay = path + "/qrenderdoc";

  FILE *f = FileIO::fopen(replay.c_str(), "r");
  if(f)
  {
    FileIO::fclose(f);
    return replay;
  }

  // if it's not in the same directory, try in a sibling /bin
  //
  // start from our path
  replay = path + "/";

// if there's a custom lib subfolder, go up one (e.g. /usr/lib/renderdoc/librenderdoc.so)
#if defined(RENDERDOC_LIB_SUBFOLDER)
  replay += "../";
#endif

  // leave the lib/ folder, and go into bin/
  replay += "../bin/qrenderdoc";

  f = FileIO::fopen(replay.c_str(), "r");
  if(f)
  {
    FileIO::fclose(f);
    return replay;
  }

  // random guesses!
  const char *guess[] = {"/opt/renderdoc/qrenderdoc", "/opt/renderdoc/bin/qrenderdoc",
                         "/usr/local/bin/qrenderdoc", "/usr/bin/qrenderdoc"};

  for(size_t i = 0; i < ARRAY_COUNT(guess); i++)
  {
    f = FileIO::fopen(guess[i], "r");
    if(f)
    {
      FileIO::fclose(f);
      return guess[i];
    }
  }

  // out of ideas, just return the filename and hope it's in PATH
  return "qrenderdoc";
}

void GetDefaultFiles(const char *logBaseName, rdcstr &capture_filename, rdcstr &logging_filename,
                     rdcstr &target)
{
  rdcstr path;
  GetExecutableFilename(path);

  const char *mod = strrchr(path.c_str(), '/');
  if(mod != NULL)
    mod++;
  else if(path.length())
    mod = path.c_str();    // Keep Android package name i.e. org.company.app
  else
    mod = "unknown";

  target = rdcstr(mod);

  time_t t = time(NULL);
  tm now = *localtime(&t);

  char temp_folder[2048] = {0};

  strcpy(temp_folder, GetTempRootPath().c_str());

  char *temp_override = getenv("RENDERDOC_TEMP");
  if(temp_override && temp_override[0] == '/')
  {
    strncpy(temp_folder, temp_override, sizeof(temp_folder) - 1);
    size_t len = strlen(temp_folder);
    while(temp_folder[len - 1] == '/')
      temp_folder[--len] = 0;
  }

  capture_filename =
      StringFormat::Fmt("%s/RenderDoc/%s_%04d.%02d.%02d_%02d.%02d.rdc", temp_folder, mod,
                        1900 + now.tm_year, now.tm_mon + 1, now.tm_mday, now.tm_hour, now.tm_min);

  // set by UI when launching programs so all logging goes to the same file
  char *logfile_override = getenv("RENDERDOC_DEBUG_LOG_FILE");
  if(logfile_override)
    logging_filename = rdcstr(logfile_override);
  else
    logging_filename = StringFormat::Fmt(
        "%s/RenderDoc/%s_%04d.%02d.%02d_%02d.%02d.%02d.log", temp_folder, logBaseName,
        1900 + now.tm_year, now.tm_mon + 1, now.tm_mday, now.tm_hour, now.tm_min, now.tm_sec);
}

uint64_t GetModifiedTimestamp(const rdcstr &filename)
{
  struct ::stat st;
  int res = stat(filename.c_str(), &st);

  if(res == 0)
  {
    return (uint64_t)st.st_mtime;
  }

  return 0;
}

uint64_t GetFileSize(const rdcstr &filename)
{
  struct ::stat st;
  int res = stat(filename.c_str(), &st);

  if(res == 0)
  {
    return (uint64_t)st.st_size;
  }

  return 0;
}

bool Copy(const char *from, const char *to, bool allowOverwrite)
{
  if(from[0] == 0 || to[0] == 0)
    return false;

  FILE *ff = ::fopen(from, "r");

  if(!ff)
  {
    RDCERR("Can't open source file for copy '%s'", from);
    return false;
  }

  FILE *tf = ::fopen(to, "r");

  if(tf && !allowOverwrite)
  {
    RDCERR("Destination file for non-overwriting copy '%s' already exists", from);
    ::fclose(ff);
    ::fclose(tf);
    return false;
  }

  if(tf)
    ::fclose(tf);

  tf = ::fopen(to, "w");

  if(!tf)
  {
    ::fclose(ff);
    RDCERR("Can't open destination file for copy '%s'", to);
    return false;
  }

  char buffer[BUFSIZ];

  while(!::feof(ff))
  {
    size_t nread = ::fread(buffer, 1, BUFSIZ, ff);
    ::fwrite(buffer, 1, nread, tf);
  }

  ::fclose(ff);
  ::fclose(tf);

  return true;
}

bool Move(const char *from, const char *to, bool allowOverwrite)
{
  if(exists(to))
  {
    if(!allowOverwrite)
      return false;
  }

  return ::rename(from, to) == 0;
}

void Delete(const char *path)
{
  unlink(path);
}

void GetFilesInDirectory(const char *path, rdcarray<PathEntry> &ret)
{
  ret.clear();

  DIR *d = opendir(path);

  if(d == NULL)
  {
    PathProperty flags = PathProperty::ErrorUnknown;

    if(errno == ENOENT)
      flags = PathProperty::ErrorInvalidPath;
    else if(errno == EACCES)
      flags = PathProperty::ErrorAccessDenied;

    ret.push_back(PathEntry(path, flags));
    return;
  }

  dirent *ent = NULL;

  for(;;)
  {
    ent = readdir(d);

    if(!ent)
      break;

    // skip "." and ".."
    if(!strcmp(ent->d_name, ".") || !strcmp(ent->d_name, ".."))
      continue;

    rdcstr fullpath = path;
    fullpath += '/';
    fullpath += ent->d_name;

    struct ::stat st;
    int res = stat(fullpath.c_str(), &st);

    // invalid/bad file - skip it
    if(res != 0)
      continue;

    PathProperty flags = PathProperty::NoFlags;

    // make directory/executable mutually exclusive for clarity's sake
    if(S_ISDIR(st.st_mode))
      flags |= PathProperty::Directory;
    else if(st.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH))
      flags |= PathProperty::Executable;

    if(ent->d_name[0] == '.')
      flags |= PathProperty::Hidden;

    PathEntry f(ent->d_name, flags);

    f.lastmod = (uint32_t)st.st_mtime;
    f.size = (uint64_t)st.st_size;

    ret.push_back(f);
  }

  // don't care if we hit an error or enumerated all files, just finish

  closedir(d);
}

FILE *fopen(const char *filename, const char *mode)
{
  return ::fopen(filename, mode);
}

rdcstr ErrorString()
{
  int err = errno;

  char buf[256] = {0};

  strerror_r(err, buf, 256);

  return buf;
}

size_t fread(void *buf, size_t elementSize, size_t count, FILE *f)
{
  return ::fread(buf, elementSize, count, f);
}
size_t fwrite(const void *buf, size_t elementSize, size_t count, FILE *f)
{
  return ::fwrite(buf, elementSize, count, f);
}

uint64_t ftell64(FILE *f)
{
  return (uint64_t)::ftell(f);
}
void fseek64(FILE *f, uint64_t offset, int origin)
{
  ::fseek(f, (long)offset, origin);
}

bool feof(FILE *f)
{
  return ::feof(f) != 0;
}

void ftruncateat(FILE *f, uint64_t length)
{
  ::fflush(f);
  int fd = ::fileno(f);
  ::ftruncate(fd, (off_t)length);
}

bool fflush(FILE *f)
{
  return ::fflush(f) == 0;
}

int fclose(FILE *f)
{
  return ::fclose(f);
}

bool exists(const char *filename)
{
  struct ::stat st;
  int res = stat(filename, &st);

  return (res == 0);
}

rdcarray<int> logfiles;

// this is used in posix_process.cpp, so that we can close the handle any time that we fork()
void ReleaseFDAfterFork()
{
  // we do NOT release the shared lock here, since the file descriptor is shared so we'd be
  // releasing the parent process's lock. Just close our file descriptor
  for(int log : logfiles)
    close(log);
}

rdcstr logfile_readall(const char *filename)
{
  FILE *f = FileIO::fopen(filename, "r");

  rdcstr ret;

  if(f == NULL)
    return ret;

  FileIO::fseek64(f, 0, SEEK_END);
  uint64_t size = FileIO::ftell64(f);
  FileIO::fseek64(f, 0, SEEK_SET);

  ret.resize((size_t)size);

  FileIO::fread(&ret[0], 1, ret.size(), f);

  FileIO::fclose(f);

  return ret;
}

LogFileHandle *logfile_open(const char *filename)
{
  int fd = open(filename, O_APPEND | O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

  if(fd < 0)
  {
    RDCWARN("Couldn't open logfile '%s': %d", filename, (int)errno);
    return NULL;
  }

  logfiles.push_back(fd);

  // acquire a shared lock. Every process acquires a shared lock to the common logfile. Each time a
  // process shuts down and wants to close the logfile, it releases its shared lock and tries to
  // acquire an exclusive lock, to see if it can delete the file. See logfile_close.
  int err = flock(fd, LOCK_SH | LOCK_NB);

  if(err < 0)
    RDCWARN("Couldn't acquire shared lock to '%s': %d", filename, (int)errno);

  return (LogFileHandle *)(uintptr_t)fd;
}

void logfile_append(LogFileHandle *logHandle, const char *msg, size_t length)
{
  if(logHandle)
  {
    int fd = int(uintptr_t(logHandle) & 0xffffffff);

    write(fd, msg, (unsigned int)length);
  }
}

void logfile_close(LogFileHandle *logHandle, const char *deleteFilename)
{
  if(logHandle)
  {
    int fd = int(uintptr_t(logHandle) & 0xffffffff);

    // release our shared lock
    int err = flock(fd, LOCK_UN | LOCK_NB);

    if(err == 0 && deleteFilename)
    {
      // now try to acquire an exclusive lock. If this succeeds, no other processes are using the
      // file (since no other shared locks exist), so we can delete it. If it fails, some other
      // shared lock still exists so we can just close our fd and exit.
      // NOTE: there is a race here between acquiring the exclusive lock and unlinking, but we
      // aren't interested in this kind of race - we're interested in whether an application is
      // still running when the UI closes, or vice versa, or similar cases.
      err = flock(fd, LOCK_EX | LOCK_NB);

      if(err == 0)
      {
        // we got the exclusive lock. Now release it, close fd, and unlink the file
        err = flock(fd, LOCK_UN | LOCK_NB);

        // can't really error handle here apart from retrying
        if(err != 0)
          RDCWARN("Couldn't release exclusive lock to '%s': %d", deleteFilename, (int)errno);

        close(fd);

        unlink(deleteFilename);

        // return immediately so we don't close again below.
        return;
      }
    }
    else
    {
      RDCWARN("Couldn't release shared lock to '%s': %d", deleteFilename, (int)errno);
      // nothing to do, we won't try again, just exit. The log might lie around, but that's
      // relatively harmless.
    }

    logfiles.removeOne(fd);

    close(fd);
  }
}
};

namespace StringFormat
{
rdcstr sntimef(time_t utcTime, const char *format)
{
  tm *tmv = localtime(&utcTime);

  // conservatively assume that most formatters will replace like-for-like (e.g. %H with 12) and
  // a few will increase (%Y to 2019) but generally the string will stay the same size.
  size_t len = strlen(format) + 16;

  size_t ret = 0;
  char *buf = NULL;

  // loop until we have successfully formatted
  while(ret == 0)
  {
    // delete any previous buffer
    delete[] buf;

    // alloate new one of the new size
    buf = new char[len + 1];
    buf[len] = 0;

    // try formatting
    ret = strftime(buf, len, format, tmv);

    // double the length for next time, if this failed
    len *= 2;
  }

  rdcstr str = buf;

  // delete successful buffer
  delete[] buf;

  return str;
}
};
