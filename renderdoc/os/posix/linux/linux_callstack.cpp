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

#include <execinfo.h>
#include <stdio.h>
#include <string.h>
#include <map>
#include <vector>
#include "os/os_specific.h"

void *renderdocBase = NULL;
void *renderdocEnd = NULL;

class LinuxCallstack : public Callstack::Stackwalk
{
public:
  LinuxCallstack()
  {
    RDCEraseEl(addrs);
    numLevels = 0;
    Collect();
  }
  LinuxCallstack(uint64_t *calls, size_t num) { Set(calls, num); }
  ~LinuxCallstack() {}
  void Set(uint64_t *calls, size_t num)
  {
    numLevels = num;
    for(int i = 0; i < numLevels; i++)
      addrs[i] = calls[i];
  }

  size_t NumLevels() const { return size_t(numLevels); }
  const uint64_t *GetAddrs() const { return addrs; }
private:
  LinuxCallstack(const Callstack::Stackwalk &other);

  void Collect()
  {
    void *addrs_ptr[ARRAY_COUNT(addrs)];

    numLevels = backtrace(addrs_ptr, ARRAY_COUNT(addrs));

    int offs = 0;
    // if we want to trim levels of the stack, we can do that here
    // by incrementing offs and decrementing numLevels
    while(numLevels > 0 && addrs_ptr[offs] >= renderdocBase && addrs_ptr[offs] < renderdocEnd)
    {
      offs++;
      numLevels--;
    }

    for(int i = 0; i < numLevels; i++)
      addrs[i] = (uint64_t)addrs_ptr[i + offs];
  }

  uint64_t addrs[128];
  int numLevels;
};

namespace Callstack
{
void Init()
{
  // look for our own line
  FILE *f = FileIO::fopen("/proc/self/maps", "r");

  if(f)
  {
    while(!feof(f))
    {
      char line[512] = {0};
      if(fgets(line, 511, f))
      {
        if(strstr(line, "librenderdoc") && strstr(line, "r-xp"))
        {
          sscanf(line, "%p-%p", &renderdocBase, &renderdocEnd);
          break;
        }
      }
    }

    FileIO::fclose(f);
  }
}

Stackwalk *Collect()
{
  return new LinuxCallstack();
}

Stackwalk *Create()
{
  return new LinuxCallstack(NULL, 0);
}

bool GetLoadedModules(byte *buf, size_t &size)
{
  // we just dump the whole file rather than pre-parsing, that way we can improve
  // parsing without needing to recapture
  FILE *f = FileIO::fopen("/proc/self/maps", "r");

  size = 0;

  if(buf)
    memcpy(buf, "LNUXCALL", 8);

  size += 8;

  byte dummy[512];

  while(!feof(f))
  {
    byte *readbuf = buf ? buf + size : dummy;
    size += FileIO::fread(readbuf, 1, 512, f);
  }

  FileIO::fclose(f);

  return true;
}

struct LookupModule
{
  uint64_t base;
  uint64_t end;
  uint64_t offset;
  char path[2048];
};

class LinuxResolver : public Callstack::StackResolver
{
public:
  LinuxResolver(std::vector<LookupModule> modules) { m_Modules = modules; }
  Callstack::AddressDetails GetAddr(uint64_t addr)
  {
    EnsureCached(addr);

    return m_Cache[addr];
  }

private:
  void EnsureCached(uint64_t addr)
  {
    auto it = m_Cache.insert(
        std::pair<uint64_t, Callstack::AddressDetails>(addr, Callstack::AddressDetails()));
    if(!it.second)
      return;

    Callstack::AddressDetails &ret = it.first->second;

    ret.filename = "Unknown";
    ret.line = 0;
    ret.function = StringFormat::Fmt("0x%08llx", addr);

    for(size_t i = 0; i < m_Modules.size(); i++)
    {
      if(addr >= m_Modules[i].base && addr < m_Modules[i].end)
      {
        uint64_t relative = addr - m_Modules[i].base + m_Modules[i].offset;
        std::string cmd =
            StringFormat::Fmt("addr2line -fCe \"%s\" 0x%llx", m_Modules[i].path, relative);

        FILE *f = ::popen(cmd.c_str(), "r");

        char result[2048] = {0};
        fread(result, 1, 2047, f);

        fclose(f);

        char *line2 = strchr(result, '\n');
        if(line2)
        {
          *line2 = 0;
          line2++;
        }

        ret.function = result;

        if(line2)
        {
          char *linenum = line2 + strlen(line2) - 1;
          while(linenum > line2 && *linenum != ':')
            linenum--;

          ret.line = 0;

          if(*linenum == ':')
          {
            *linenum = 0;
            linenum++;

            while(*linenum >= '0' && *linenum <= '9')
            {
              ret.line *= 10;
              ret.line += (uint32_t(*linenum) - uint32_t('0'));
              linenum++;
            }
          }

          ret.filename = line2;
        }

        break;
      }
    }
  }

  std::vector<LookupModule> m_Modules;
  std::map<uint64_t, Callstack::AddressDetails> m_Cache;
};

StackResolver *MakeResolver(byte *moduleDB, size_t DBSize, RENDERDOC_ProgressCallback progress)
{
  // we look in the original locations for the files, we don't prompt if we can't
  // find the file, or the file doesn't have symbols (and we don't validate that
  // the file is the right version). A good option for doing this would be
  // http://github.com/mlabbe/nativefiledialog

  if(DBSize < 8 || memcmp(moduleDB, "LNUXCALL", 8))
  {
    RDCWARN("Can't load callstack resolve for this log. Possibly from another platform?");
    return NULL;
  }

  char *start = (char *)(moduleDB + 8);
  char *search = start;
  char *dbend = (char *)(moduleDB + DBSize);

  std::vector<LookupModule> modules;

  while(search && search < dbend)
  {
    if(progress)
      progress(float(search - start) / float(DBSize));

    // find .text segments
    {
      long unsigned int base = 0, end = 0, offset = 0;

      int inode = 0;
      int offs = 0;
      //                        base-end   perms offset devid   inode offs
      int num = sscanf(search, "%lx-%lx  r-xp  %lx    %*x:%*x %d    %n", &base, &end, &offset,
                       &inode, &offs);

      // we don't care about inode actually, we ust use it to verify that
      // we read all 4 params (and so perms == r-xp)
      if(num == 4 && offs > 0)
      {
        LookupModule mod = {0};

        mod.base = (uint64_t)base;
        mod.end = (uint64_t)end;
        mod.offset = (uint64_t)offset;

        search += offs;
        while(search < dbend && (*search == ' ' || *search == '\t'))
          search++;

        if(search < dbend && *search != '[' && *search != 0 && *search != '\n')
        {
          size_t n = ARRAY_COUNT(mod.path) - 1;
          mod.path[n] = 0;
          for(size_t i = 0; i < n; i++)
          {
            if(search[i] == 0 || search[i] == '\n')
            {
              mod.path[i] = 0;
              break;
            }
            if(search + i >= dbend)
            {
              mod.path[i] = 0;
              break;
            }
            mod.path[i] = search[i];
          }

          modules.push_back(mod);
        }
      }
    }

    if(progress)
      progress(RDCMIN(1.0f, float(search - start) / float(DBSize)));

    if(search >= dbend)
      break;

    search = strchr(search, '\n');
    if(search)
      search++;
  }

  return new LinuxResolver(modules);
}
};
