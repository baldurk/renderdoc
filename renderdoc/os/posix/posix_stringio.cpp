/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2016 Baldur Karlsson
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

#include <dlfcn.h>    // for dladdr
#include <errno.h>
#include <pwd.h>
#include <stdio.h>
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
  char path[512] = {0};
  readlink(filename.c_str(), path, 511);

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

  // out of ideas
  return "";
}

void GetDefaultFiles(const char *logBaseName, string &capture_filename, string &logging_filename,
                     string &target)
{
  string path;
  GetExecutableFilename(path);

  const char *mod = strrchr(path.c_str(), '/');
  if(mod == NULL)
    mod = "unknown";
  else
    mod++;

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

  snprintf(temp_filename, sizeof(temp_filename) - 1, "%s/%s_%04d.%02d.%02d_%02d.%02d.rdc",
           GetTempRootPath(), mod, 1900 + now.tm_year, now.tm_mon + 1, now.tm_mday, now.tm_hour,
           now.tm_min);

  capture_filename = string(temp_filename);

  snprintf(temp_filename, sizeof(temp_filename) - 1, "%s/%s_%04d.%02d.%02d_%02d.%02d.%02d.log",
           GetTempRootPath(), logBaseName, 1900 + now.tm_year, now.tm_mon + 1, now.tm_mday,
           now.tm_hour, now.tm_min, now.tm_sec);

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
