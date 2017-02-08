/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2016-2017 Baldur Karlsson
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
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include "api/app/renderdoc_app.h"
#include "common/threading.h"
#include "os/os_specific.h"
#include "serialise/string_utils.h"

using std::string;

// gives us an address to identify this so with
static int soLocator = 0;

namespace FileIO
{
// in posix/.../..._stringio.cpp
const char *GetTempRootPath();

string GetHomeFolderFilename()
{
  passwd *pw = getpwuid(getuid());
  const char *homedir = pw->pw_dir;

  return homedir;
}

void CreateParentDirectory(const string &filename)
{
  string fn = dirname(filename);

  // want trailing slash so that we create all directories
  fn.push_back('/');

  if(fn[0] != '/')
    return;

  size_t offs = fn.find('/', 1);

  while(offs != string::npos)
  {
    // create directory path from 0 to offs by NULLing the
    // / at offs, mkdir, then set it back
    fn[offs] = 0;
    mkdir(fn.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    fn[offs] = '/';

    offs = fn.find_first_of('/', offs + 1);
  }
}

string GetFullPathname(const string &filename)
{
  char path[PATH_MAX + 1] = {0};
  realpath(filename.c_str(), path);

  return string(path);
}

string GetReplayAppFilename()
{
  // look up the shared object's path via dladdr
  Dl_info info;
  dladdr((void *)&soLocator, &info);
  string path = info.dli_fname ? info.dli_fname : "";
  path = dirname(path);
  string replay = path + "/qrenderdoc";

  FILE *f = FileIO::fopen(replay.c_str(), "r");
  if(f)
  {
    FileIO::fclose(f);
    return replay;
  }

  // if it's not in the same directory, try in a sibling /bin
  // e.g. /foo/bar/lib/librenderdoc.so -> /foo/bar/bin/qrenderdoc
  replay = path + "/../bin/qrenderdoc";

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

void GetDefaultFiles(const char *logBaseName, string &capture_filename, string &logging_filename,
                     string &target)
{
  string path;
  GetExecutableFilename(path);

  const char *mod = strrchr(path.c_str(), '/');
  if(mod != NULL)
    mod++;
  else if(path.length())
    mod = path.c_str();    // Keep Android package name i.e. org.company.app
  else
    mod = "unknown";

  target = string(mod);

  time_t t = time(NULL);
  tm now = *localtime(&t);

  char temp_folder[2048] = {0};

  strcpy(temp_folder, GetTempRootPath());

  char *temp_override = getenv("RENDERDOC_TEMP");
  if(temp_override && temp_override[0] == '/')
  {
    strncpy(temp_folder, temp_override, sizeof(temp_folder) - 1);
    size_t len = strlen(temp_folder);
    while(temp_folder[len - 1] == '/')
      temp_folder[--len] = 0;
  }

  char temp_filename[2048] = {0};

  snprintf(temp_filename, sizeof(temp_filename) - 1, "%s/RenderDoc/%s_%04d.%02d.%02d_%02d.%02d.rdc",
           temp_folder, mod, 1900 + now.tm_year, now.tm_mon + 1, now.tm_mday, now.tm_hour,
           now.tm_min);

  capture_filename = string(temp_filename);

  snprintf(temp_filename, sizeof(temp_filename) - 1,
           "%s/RenderDoc/%s_%04d.%02d.%02d_%02d.%02d.%02d.log", temp_folder, logBaseName,
           1900 + now.tm_year, now.tm_mon + 1, now.tm_mday, now.tm_hour, now.tm_min, now.tm_sec);

  // set by UI when launching programs so all logging goes to the same file
  char *logfile_override = getenv("RENDERDOC_DEBUG_LOG_FILE");
  if(logfile_override)
    logging_filename = string(logfile_override);
  else
    logging_filename = string(temp_filename);
}

uint64_t GetModifiedTimestamp(const string &filename)
{
  struct ::stat st;
  int res = stat(filename.c_str(), &st);

  if(res == 0)
  {
    return (uint64_t)st.st_mtime;
  }

  return 0;
}

void Copy(const char *from, const char *to, bool allowOverwrite)
{
  if(from[0] == 0 || to[0] == 0)
    return;

  FILE *ff = ::fopen(from, "r");

  if(!ff)
  {
    RDCERR("Can't open source file for copy '%s'", from);
    return;
  }

  FILE *tf = ::fopen(to, "r");

  if(tf && !allowOverwrite)
  {
    RDCERR("Destination file for non-overwriting copy '%s' already exists", from);
    ::fclose(ff);
    ::fclose(tf);
    return;
  }

  if(tf)
    ::fclose(tf);

  tf = ::fopen(to, "w");

  if(!tf)
  {
    ::fclose(ff);
    RDCERR("Can't open destination file for copy '%s'", to);
  }

  char buffer[BUFSIZ];

  while(!::feof(ff))
  {
    size_t nread = ::fread(buffer, 1, BUFSIZ, ff);
    ::fwrite(buffer, 1, nread, tf);
  }

  ::fclose(ff);
  ::fclose(tf);
}

void Delete(const char *path)
{
  unlink(path);
}

vector<FoundFile> GetFilesInDirectory(const char *path)
{
  vector<FoundFile> ret;

  DIR *d = opendir(path);

  if(d == NULL)
  {
    uint32_t flags = eFileProp_ErrorUnknown;

    if(errno == ENOENT)
      flags = eFileProp_ErrorInvalidPath;
    else if(errno == EACCES)
      flags = eFileProp_ErrorAccessDenied;

    ret.push_back(FoundFile(path, flags));
    return ret;
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

    string fullpath = path;
    fullpath += '/';
    fullpath += ent->d_name;

    struct ::stat st;
    int res = stat(fullpath.c_str(), &st);

    // invalid/bad file - skip it
    if(res != 0)
      continue;

    uint32_t flags = 0;

    // make directory/executable mutually exclusive for clarity's sake
    if(S_ISDIR(st.st_mode))
      flags |= eFileProp_Directory;
    else if(st.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH))
      flags |= eFileProp_Executable;

    if(ent->d_name[0] == '.')
      flags |= eFileProp_Hidden;

    FoundFile f(ent->d_name, flags);

    f.lastmod = (uint32_t)st.st_mtime;
    f.size = (uint64_t)st.st_size;

    ret.push_back(f);
  }

  // don't care if we hit an error or enumerated all files, just finish

  closedir(d);

  return ret;
}

FILE *fopen(const char *filename, const char *mode)
{
  return ::fopen(filename, mode);
}

string getline(FILE *f)
{
  string ret;

  while(!FileIO::feof(f))
  {
    char c = (char)::fgetc(f);

    if(FileIO::feof(f))
      break;

    if(c != 0 && c != '\n')
      ret.push_back(c);
    else
      break;
  }

  return ret;
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

int fclose(FILE *f)
{
  return ::fclose(f);
}

void *logfile_open(const char *filename)
{
  int fd = open(filename, O_APPEND | O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
  return (void *)(intptr_t)fd;
}

void logfile_append(void *handle, const char *msg, size_t length)
{
  if(handle)
  {
    int fd = ((intptr_t)handle & 0xffffffff);
    write(fd, msg, (unsigned int)length);
  }
}

void logfile_close(void *handle)
{
  if(handle)
  {
    int fd = ((intptr_t)handle & 0xffffffff);
    close(fd);
  }
}
};

namespace StringFormat
{
void sntimef(char *str, size_t bufSize, const char *format)
{
  time_t tim;
  time(&tim);

  tm *tmv = localtime(&tim);

  strftime(str, bufSize, format, tmv);
}

string Fmt(const char *format, ...)
{
  va_list args;
  va_start(args, format);

  va_list args2;
  va_copy(args2, args);

  int size = StringFormat::vsnprintf(NULL, 0, format, args2);

  char *buf = new char[size + 1];
  StringFormat::vsnprintf(buf, size + 1, format, args);
  buf[size] = 0;

  va_end(args);
  va_end(args2);

  string ret = buf;

  delete[] buf;

  return ret;
}
};
