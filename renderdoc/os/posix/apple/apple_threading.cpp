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

#include <dispatch/dispatch.h>
#include <mach/mach_time.h>

double Timing::GetTickFrequency()
{
  mach_timebase_info_data_t timeInfo;
  mach_timebase_info(&timeInfo);

  uint64_t numer = timeInfo.numer;
  uint64_t denom = timeInfo.denom;
  return ((double)denom / (double)numer) * 1000000.0;
}

uint64_t Timing::GetTick()
{
  return mach_absolute_time();
}

void Threading::SetCurrentThreadName(const rdcstr &name)
{
}

uint32_t Threading::NumberOfCores()
{
  long ret = sysconf(_SC_NPROCESSORS_ONLN);
  if(ret <= 0)
    return 1;
  return uint32_t(ret);
}

namespace Threading
{

struct AppleSemaphore : public Semaphore
{
  ~AppleSemaphore() {}

  dispatch_semaphore_t h;
};

Semaphore *Semaphore::Create()
{
  AppleSemaphore *sem = new AppleSemaphore();
  sem->h = dispatch_semaphore_create(0);
  return sem;
}

void Semaphore::Destroy()
{
  AppleSemaphore *sem = (AppleSemaphore *)this;
  dispatch_release(sem->h);
  delete sem;
}

void Semaphore::Wake(uint32_t numToWake)
{
  AppleSemaphore *sem = (AppleSemaphore *)this;
  for(uint32_t i = 0; i < numToWake; i++)
    dispatch_semaphore_signal(sem->h);
}

void Semaphore::WaitForWake()
{
  AppleSemaphore *sem = (AppleSemaphore *)this;
  // no timeout, so no point checking return value as that's the only failure case
  dispatch_semaphore_wait(sem->h, DISPATCH_TIME_FOREVER);
}

Semaphore::Semaphore()
{
}

Semaphore::~Semaphore()
{
}

};
