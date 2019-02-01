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

#include "os/os_specific.h"

class AndroidCallstack : public Callstack::Stackwalk
{
public:
  AndroidCallstack()
  {
    RDCEraseEl(addrs);
    numLevels = 0;
  }
  AndroidCallstack(uint64_t *calls, size_t num) { Set(calls, num); }
  ~AndroidCallstack() {}
  void Set(uint64_t *calls, size_t num)
  {
    numLevels = num;
    for(int i = 0; i < numLevels; i++)
      addrs[i] = calls[i];
  }

  size_t NumLevels() const { return 0; }
  const uint64_t *GetAddrs() const { return addrs; }
private:
  AndroidCallstack(const Callstack::Stackwalk &other);

  uint64_t addrs[128];
  int numLevels;
};

namespace Callstack
{
void Init()
{
}

Stackwalk *Collect()
{
  return new AndroidCallstack();
}

Stackwalk *Create()
{
  return new AndroidCallstack(NULL, 0);
}

bool GetLoadedModules(byte *buf, size_t &size)
{
  if(buf)
    memcpy(buf, "ANRDCALL", 8);

  size += 8;

  return true;
}

StackResolver *MakeResolver(byte *moduleDB, size_t DBSize, RENDERDOC_ProgressCallback progress)
{
  RDCERR("Callstack resolving not supported on Android.");
  return NULL;
}
};
