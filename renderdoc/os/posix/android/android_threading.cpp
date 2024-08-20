/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2024 Baldur Karlsson
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

#include "common/common.h"

#include <errno.h>
#include <semaphore.h>
#include <time.h>
#include <unistd.h>

double Timing::GetTickFrequency()
{
  return 1000000.0;
}

uint64_t Timing::GetTick()
{
  timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return uint64_t(ts.tv_sec) * 1000000000ULL + uint32_t(ts.tv_nsec & 0xffffffff);
}

void Threading::SetCurrentThreadName(const rdcstr &name)
{
}

uint32_t Threading::NumberOfCores()
{
  long ret = sysconf(_SC_NPROCESSORS_CONF);
  if(ret <= 0)
    return 1;
  return uint32_t(ret);
}

namespace Threading
{

// works for all posix except apple, hence being here
struct PosixSemaphore : public Semaphore
{
  ~PosixSemaphore() {}

  sem_t h;
};

Semaphore *Semaphore::Create()
{
  PosixSemaphore *sem = new PosixSemaphore();
  int err = sem_init(&sem->h, 0, 0);
  // only documented errors are too large initial value (impossible for 0) or for shared semaphores
  // going wrong (we're not shared)
  RDCASSERT(err == 0, (int)errno);
  return sem;
}

void Semaphore::Destroy()
{
  PosixSemaphore *sem = (PosixSemaphore *)this;
  sem_destroy(&sem->h);
  delete sem;
}

void Semaphore::Wake(uint32_t numToWake)
{
  PosixSemaphore *sem = (PosixSemaphore *)this;
  for(uint32_t i = 0; i < numToWake; i++)
    sem_post(&sem->h);
}

void Semaphore::WaitForWake()
{
  PosixSemaphore *sem = (PosixSemaphore *)this;

  // handle extremely moronic stupid signal interruptions
  do
  {
    int ret = sem_wait(&sem->h);

    if(ret == -1)
    {
      if(errno == EINTR)
        continue;

      RDCWARN("Semaphore wait failed: %d", errno);
    }
  } while(false);
}

Semaphore::Semaphore()
{
}

Semaphore::~Semaphore()
{
}

};
