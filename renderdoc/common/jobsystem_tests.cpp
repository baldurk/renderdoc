/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2024 Baldur Karlsson
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

#include "threading.h"

namespace Threading
{
extern uint32_t randomSleepRange;
extern uint32_t randomSpinRange;
};

#if ENABLED(ENABLE_UNIT_TESTS)

#include "catch/catch.hpp"

bool isSorted(const rdcarray<int> &arr)
{
  if(arr.size() <= 1)
    return true;

  for(size_t i = 0; i < arr.size() - 1; i++)
  {
    if(arr[i] > arr[i + 1])
      return false;
  }

  return true;
}

// helper function - we don't do this with multiple sections to avoid permuting the thread init/shutdown too much
void RunJobTests()
{
  // one job
  {
    bool flag = false;
    Threading::JobSystem::AddJob([&flag]() { flag = true; });

    Threading::JobSystem::SyncAllJobs();

    CHECK(flag);
  }

  // many jobs
  {
    static const size_t numJobs = 5000;
    static const size_t numItems = 500;
    rdcarray<int> a[numJobs];

    for(size_t j = 0; j < numJobs; j++)
    {
      a[j].reserve(numItems);
      for(size_t i = 0; i < numItems; i++)
        a[j].push_back(rand());

      Threading::JobSystem::AddJob([&a, j]() { std::sort(a[j].begin(), a[j].end()); });
    }

    Threading::JobSystem::SyncAllJobs();

    for(size_t j = 0; j < numJobs; j++)
      CHECK(isSorted(a[j]));
  }

  // one job and dependency
  {
    rdcarray<int> a;
    Threading::JobSystem::Job *first = Threading::JobSystem::AddJob([&a]() { a.push_back(1); });
    Threading::JobSystem::AddJob([&a]() { a.push_back(2); }, {first});

    Threading::JobSystem::SyncAllJobs();

    CHECK(a == rdcarray<int>({1, 2}));
  }

  // long dependency chain
  {
    rdcarray<int> a;
    rdcarray<int> b;

    rdcarray<Threading::JobSystem::Job *> parents;
    for(int i = 0; i < 100; i++)
    {
      parents = {Threading::JobSystem::AddJob([&a, i]() { a.push_back(i); }, parents)};

      b.push_back(i);
    }

    Threading::JobSystem::SyncAllJobs();

    CHECK(a == b);
  }

  // multiple dependency chains
  {
    static const size_t numChains = 50;
    rdcarray<int> a[numChains];
    rdcarray<int> b[numChains];

    rdcarray<Threading::JobSystem::Job *> parents[numChains];
    for(int i = 0; i < 100; i++)
    {
      for(size_t c = 0; c < numChains; c++)
      {
        parents[c] = {Threading::JobSystem::AddJob([&a, c, i]() { a[c].push_back(i); }, parents[c])};

        b[c].push_back(i);
      }
    }

    Threading::JobSystem::SyncAllJobs();

    for(size_t c = 0; c < numChains; c++)
      CHECK(a[c] == b[c]);
  }
}

TEST_CASE("Check job system behaviour is correct with common thread counts", "[jobs]")
{
  Threading::randomSleepRange = 10;
  Threading::randomSpinRange = 1000;
  uint32_t numThreads = GENERATE(1, 2, 14, 24);

  Threading::JobSystem::Init(numThreads);

  RunJobTests();

  Threading::JobSystem::Shutdown();

  // start up and shut down again a couple of times to ensure that works as well

  Threading::JobSystem::Init(numThreads);

  Threading::JobSystem::Shutdown();

  Threading::JobSystem::Init(numThreads);

  Threading::JobSystem::Shutdown();
}

// since lock contention can get really bad with many threads, only do this test once
TEST_CASE("Stress test job system with many threads", "[jobs][stress]")
{
  Threading::randomSleepRange = 2;
  Threading::randomSpinRange = 100;
  uint32_t numThreads = 1000;

  Threading::JobSystem::Init(numThreads);

  RunJobTests();

  Threading::JobSystem::Shutdown();

  // start up and shut down again a couple of times to ensure that works as well

  Threading::JobSystem::Init(numThreads);

  Threading::JobSystem::Shutdown();

  Threading::JobSystem::Init(numThreads);

  Threading::JobSystem::Shutdown();
}

#endif    // ENABLED(ENABLE_UNIT_TESTS)
