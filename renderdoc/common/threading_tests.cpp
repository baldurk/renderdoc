/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2017-2019 Baldur Karlsson
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

#include "common/threading.h"
#include "os/os_specific.h"

#if ENABLED(ENABLE_UNIT_TESTS)

#include "3rdparty/catch/catch.hpp"

static int value = 0;

TEST_CASE("Test spin lock", "[threading]")
{
  int finalValue = 0;

  std::vector<Threading::ThreadHandle> threads;
  std::vector<int> threadcount;

  threads.resize(8);
  threadcount.resize(8);

  Threading::SpinLock lock;

  for(int i = 0; i < 8; i++)
  {
    threadcount[i] = (rand() & 0xff) << 4;

    finalValue += threadcount[i];
  }

  for(int i = 0; i < 8; i++)
  {
    threads[i] = Threading::CreateThread([&lock, &threadcount, i]() {
      for(int c = 0; c < threadcount[i]; c++)
      {
        lock.Lock();
        value++;
        lock.Unlock();
      }
    });
  }

  for(Threading::ThreadHandle t : threads)
  {
    Threading::JoinThread(t);
    Threading::CloseThread(t);
  }

  CHECK(finalValue == value);
}

#endif    // ENABLED(ENABLE_UNIT_TESTS)