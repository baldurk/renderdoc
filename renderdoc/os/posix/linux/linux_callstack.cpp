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

bool GetLoadedModules(char *&buf, size_t &size)
{
  // we just dump the whole file rather than pre-parsing, that way we can improve
  // parsing without needing to recapture
  FILE *f = FileIO::fopen("/proc/self/maps", "r");

  if(buf)
  {
    buf[0] = 'L';
    buf[1] = 'N';
    buf[2] = 'U';
    buf[3] = 'X';
    buf[4] = 'C';
    buf[5] = 'A';
    buf[6] = 'L';
    buf[7] = 'L';
  }

  size += 8;

  char dummy[512];

  while(!feof(f))
  {
    char *readbuf = buf ? buf + size : dummy;
    size += FileIO::fread(readbuf, 1, 512, f);
  }

  FileIO::fclose(f);

  return true;
}

struct LookupModule
{
  uint64_t base;
  uint64_t end;
  char path[2048];
};

class LinuxResolver : public Callstack::StackResolver
{
public:
  LinuxResolver(vector<LookupModule> modules) { m_Modules = modules; }
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
        uint64_t relative = addr - m_Modules[i].base;
        string cmd =
            StringFormat::Fmt("addr2line -j.text -fCe \"%s\" 0x%llx", m_Modules[i].path, relative);

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
          char *last = line2 + strlen(line2) - 1;
          uint32_t mul = 1;
          ret.line = 0;
          while(*last >= '0' && *last <= '9')
          {
            ret.line += mul * (uint32_t(*last) - uint32_t('0'));
            *last = 0;
            last--;
            mul *= 10;
          }
          if(*last == ':')
            *last = 0;

          ret.filename = line2;
        }

        break;
      }
    }
  }

  std::vector<LookupModule> m_Modules;
  std::map<uint64_t, Callstack::AddressDetails> m_Cache;
};

StackResolver *MakeResolver(char *moduleDB, size_t DBSize, string pdbSearchPaths,
                            volatile bool *killSignal)
{
  // we look in the original locations for the files, we don't prompt if we can't
  // find the file, or the file doesn't have symbols (and we don't validate that
  // the file is the right version). A good option for doing this would be
  // http://github.com/mlabbe/nativefiledialog

  bool valid = true;
  if(memcmp(moduleDB, "LNUXCALL", 8))
  {
    RDCWARN("Can't load callstack resolve for this log. Possibly from another platform?");
    valid = false;
  }

  char *search = moduleDB + 8;

  vector<LookupModule> modules;

  while(valid && search && size_t(search - moduleDB) < DBSize)
  {
    if(killSignal && *killSignal)
      break;

    // find .text segments
    {
      long unsigned int base = 0, end = 0;

      int inode = 0;
      int offs = 0;
      //                        base-end   perms offset devid   inode offs
      int num = sscanf(search, "%lx-%lx  r-xp  %*x    %*x:%*x %d    %n", &base, &end, &inode, &offs);

      // we don't care about inode actually, we ust use it to verify that
      // we read all 3 params (and so perms == r-xp)
      if(num == 3 && offs > 0)
      {
        LookupModule mod = {0};

        mod.base = (uint64_t)base;
        mod.end = (uint64_t)end;

        search += offs;
        while(size_t(search - moduleDB) < DBSize && (*search == ' ' || *search == '\t'))
          search++;

        if(size_t(search - moduleDB) < DBSize && *search != '[' && *search != 0 && *search != '\n')
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
            if(search + i >= moduleDB + DBSize)
            {
              mod.path[i] = 0;
              break;
            }
            mod.path[i] = search[i];
          }

          // addr2line wants offsets relative to text section, have to find offset of .text section
          // in this
          // mmapped section

          int textoffs = 0;
          string cmd = StringFormat::Fmt("readelf -WS \"%s\"", mod.path);

          FILE *f = ::popen(cmd.c_str(), "r");

          char result[4096] = {0};
          fread(result, 1, 4095, f);

          fclose(f);

          // find .text line
          char *find = strstr(result, ".text");

          if(find)
          {
            find += 5;

            while(isalpha(*find) || isspace(*find))
              find++;

            // virtual address is listed first, we want the offset
            sscanf(find, "%*x %x", &textoffs);

            mod.base += textoffs;

            modules.push_back(mod);
          }
        }
      }
    }

    if(search >= (char *)(moduleDB + DBSize))
      break;

    search = strchr(search, '\n');
    if(search)
      search++;
  }

  return new LinuxResolver(modules);
}
};
