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

#include <math.h>
#include "threading.h"

namespace Threading
{
// sleep/spin timings for stress-testing. Can be poked by tests but not declared publicly

// max milliseconds to sleep. 0 = no sleeps, otherwise randomly sleeps up to N-2 milliseconds
// if the random sleep is 1, then Sleep(0) is called
uint32_t randomSleepRange = 0;

// max rounds to spin. 0 = no spins, otherwise spins up to N loops
uint32_t randomSpinRange = 0;

#if ENABLED(RDOC_DEVEL)
// static to save spin result to
static float spinForce = 0.0f;

// to avoid catastrophic lock contention, but to still test locks, limit sleep/spin time while a lock is held
#define RandomSleepSpin(lockHeld)                                                \
  {                                                                              \
    uint32_t sleepMS = randomSleepRange == 0 ? 0 : (rand() % randomSleepRange);  \
    uint32_t spinRounds = randomSpinRange == 0 ? 0 : (rand() % randomSpinRange); \
    if(lockHeld)                                                                 \
    {                                                                            \
      sleepMS = RDCMIN(1U, sleepMS);                                             \
      spinRounds = RDCMIN(1000U, spinRounds);                                    \
    }                                                                            \
    if(sleepMS)                                                                  \
      Threading::Sleep(sleepMS - 1);                                             \
    float x = (float)spinRounds;                                                 \
    for(uint32_t counter = 0; counter < spinRounds; counter++)                   \
      x = sqrtf(x + 2.0f);                                                       \
    spinForce = x;                                                               \
  }

#else

#define RandomSleepSpin(lockHeld)

#endif

// Principles:
// - don't need priorities (yet)
// - jobs are basically go-wide then one big sync, no need to track lifetimes, don't need a
// continuous
//   rolling parallelisation only using this during specific points (loading, shader debugging)
// - all jobs explicitly launched from main thread, jobs cannot launch jobs
// - only simple dependencies: 1 job depends on N parents
// - don't need to be fair: as long as all jobs complete, can happen in mostly any order
// - jobs should not be too fast, 2ms would be a lower bound
// - since we expect one sync point, we don't expect perfect forward progress indefinitely with no
// syncs
//
// Safety analysis:
//
// - over-waking a semaphore a little is not a problem, the worker might spin a bit but it will
// eventually
//   go back to sleep once it can't get any work.
// - waking one semaphore is sufficient to drain the queue as one worker alone will eventually
//   complete all work just potentially without the best parallelism if other workers are sleeping
// - semaphore count limits mean we should not do one wake-per-job or it might overflow in theory
// - we wake workers in a chain. Threads mark when they go to sleep and are prioritised to wake up
//   for new jobs as we assume maximum saturation is desired. When a thread finds work in the queue
//   pending it will try to wake a sleeping sibling.
// - The main thread could in theory push work right as all threads are going to sleep but fail to
//   wake any of them if it thinks they're running. This can only happen for one job at most at a
//   time, the most recent job to be pushed, as otherwise the next job would find sleeping
//   threads and wake them. Forward progress is guaranteed by an assumed SyncAll() call. This could
//   be improved by forcing the main thread to always wake at least one thread, or perhaps to wait
//   until either it's woken a thread or the job has been grabbed from the job queue.
// - threads could be mis-identified as both sleeping or waking due to the gap between the atomic on
//   'running' and the semaphore sleep/wake, but as a result of the above double-waking a thread is
//   not a big problem as it will eventually sleep if there's no room. Thinking a thread is running
//   when it's just gone to sleep is also fine as this is equivalent to if the thread really were
//   running - we still have forward progress.

namespace JobSystem
{

struct Job
{
  // 0 = not run or running, 1 = complete
  int32_t state = 0;

  void run()
  {
    // run should not be called multiple times
    RDCASSERT(state == 0);

    callback();
    Atomic::Inc32(&state);

    // run should not be called multiple times
    RDCASSERT(state == 1);
  }

  // list of jobs that must be complete before this job can be run
  rdcarray<Job *> parents;

  // the actual callback
  std::function<void()> callback;
};

};    // namespace JobSystem

// TODO: could be multiple queues per-priority in future...

// locked access to the queue and shutdown flag
Threading::CriticalSection queueLock;
// global flags for workers to shut down. DOES NOT automatically drain work, requires a sync first
bool shutdown = false;
// added to on on main thread and worker threads pull from and potentially reorder
rdcarray<Threading::JobSystem::Job *> jobQueue;

// list of jobs, only for lifetime management. Only accessed on main thread, for cleanup in SyncAll()
rdcarray<Threading::JobSystem::Job *> allocatedJobs;

struct JobWorker
{
  size_t idx;

  Threading::Semaphore *semaphore;
  Threading::ThreadHandle thread;

  // 1 = running, or 0 = currently sleeping
  int32_t running = 1;
};

// the thread ID of the main thread - only thread that can access the external API
uint64_t mainThread;

rdcarray<JobWorker> workers;

// wake at most one sleeping worker, either starting from 0 (and any) or starting from N (and not waking itself)
bool TryWakeFirstSleepingWorker(size_t firstIdx = ~0U)
{
  size_t exclude = firstIdx;
  if(firstIdx == ~0U)
    firstIdx = 0;

  // loop over every worker, find the next one asleep and wake it
  for(size_t i = 0; i < workers.size(); i++)
  {
    size_t idx = (firstIdx + i) % workers.size();

    if(idx == exclude)
      continue;

    // worker running state should always be 0 or 1
    int32_t running = workers[idx].running;
    RDCASSERT(running == 0 || running == 1);

    if(Atomic::CmpExch32(&workers[idx].running, 0, 0) == 0)
    {
      workers[idx].semaphore->Wake(1);
      return true;
    }
  }

  return false;
}

bool RunJobIfReady(Threading::JobSystem::Job *curJob)
{
  // default to ready to run if there are no parents
  bool dependenciesSatisfied = true;

  // check all parent jobs if they're completed. This is conservative,
  // they might have finished but not updated state or one might finish after
  // we checked it, but that's fine
  for(Threading::JobSystem::Job *p : curJob->parents)
  {
    if(Atomic::CmpExch32(&p->state, 0, 0) == 0)
      dependenciesSatisfied = false;

    // check that parent state is valid, should either be finished or not
    int32_t state = p->state;
    RDCASSERT(state == 0 || state == 1);
  }

  // if we can run the job, run it. Return whether or not it was run
  if(dependenciesSatisfied)
  {
    curJob->run();
    return true;
  }

  return false;
}

void WorkerThread(JobWorker &worker)
{
  // outer loop until shutdown
  while(true)
  {
    // job we grabbed to work on
    Threading::JobSystem::Job *curJob = NULL;
    // if there is even more work to do
    bool moreWork = false;

    RandomSleepSpin(false);

    {
      SCOPED_LOCK(queueLock);

      RandomSleepSpin(true);

      // grab a job if the queue is non-empty
      if(!jobQueue.empty())
      {
        curJob = jobQueue.back();
        jobQueue.pop_back();
      }
      // if there's even more work, note it so we can wake up a sibling worker as needed
      moreWork = !jobQueue.empty();

      RandomSleepSpin(true);

      // shut down immediately if requested, check this in the lock
      if(shutdown)
        break;
    }

    RandomSleepSpin(false);

    // if there's no more work, go to sleep
    if(!curJob)
    {
      RDCASSERT(worker.running == 1);
      Atomic::Dec32(&worker.running);

      RandomSleepSpin(false);

      // check the queue once more here to allow constant forward progress without a sync.
      // If the main thread pushed work after we last checked, but it thought we were running so
      // didn't wake us up and we got here, we can check for work and re-wake without a semaphore
      // signal that might never come.
      // If there's no work here then when the main thread adds more it will definitely see us (or
      // at least one worker) not running and wake us
      {
        SCOPED_LOCK(queueLock);
        RandomSleepSpin(true);

        if(!jobQueue.empty())
        {
          RandomSleepSpin(true);

          Atomic::Inc32(&worker.running);
          continue;
        }
      }

      RandomSleepSpin(false);

      worker.semaphore->WaitForWake();
      RDCASSERT(worker.running == 0);
      Atomic::Inc32(&worker.running);

      RandomSleepSpin(false);
    }

    // if there's more work to do, try to wake a sleeping worker too. If none are sleeping, this will do nothing
    if(moreWork)
      TryWakeFirstSleepingWorker(worker.idx);

    RandomSleepSpin(false);

    // run our job, and push it back onto the queue if it couldn't be run
    if(curJob && !RunJobIfReady(curJob))
    {
      SCOPED_LOCK(queueLock);

      RandomSleepSpin(true);
      jobQueue.insert(0, curJob);

      RandomSleepSpin(true);
    }
  }

  Atomic::Dec32(&worker.running);
}

namespace JobSystem
{
void Init(uint32_t numThreads)
{
  mainThread = Threading::GetCurrentID();

  shutdown = false;
  jobQueue.clear();

  // if numThreads is 0, auto-select a number of threads
  if(numThreads == 0)
  {
    uint32_t numCores = Threading::NumberOfCores();

    // don't get greedy with thread count
    if(numCores <= 4)
      numThreads = numCores - 1;
    else if(numCores <= 8)
      numThreads = numCores - 3;
    else if(numCores <= 16)
      numThreads = numCores - 6;
    else if(numCores <= 32)
      numThreads = numCores - 8;
    else
      numThreads = numCores / 2;
  }

  RDCLOG("Initialising job system with %u threads", numThreads);

  workers.resize(numThreads);
  for(size_t i = 0; i < numThreads; i++)
  {
    workers[i].idx = i;
    workers[i].semaphore = Threading::Semaphore::Create();
    workers[i].thread = Threading::CreateThread([i] { WorkerThread(workers[i]); });
  }
}

void Shutdown()
{
  if(mainThread == 0)
    return;

  RDCASSERTEQUAL(mainThread, Threading::GetCurrentID());

  SyncAllJobs();

  mainThread = 0;

  {
    SCOPED_LOCK(queueLock);
    shutdown = true;
  }
  for(size_t i = 0; i < workers.size(); i++)
    workers[i].semaphore->Wake(1);

  for(size_t i = 0; i < workers.size(); i++)
  {
    Threading::JoinThread(workers[i].thread);
    Threading::CloseThread(workers[i].thread);
    workers[i].semaphore->Destroy();
  }

  workers.clear();
}

void SyncAllJobs()
{
  if(workers.empty())
    return;

  RDCASSERTEQUAL(mainThread, Threading::GetCurrentID());

  while(true)
  {
    // job we grabbed to work on
    Job *curJob = NULL;

    {
      SCOPED_LOCK(queueLock);
      if(jobQueue.empty())
        break;

      curJob = jobQueue.back();
      jobQueue.pop_back();
    }

    if(!RunJobIfReady(curJob))
    {
      SCOPED_LOCK(queueLock);
      jobQueue.insert(0, curJob);
    }

    TryWakeFirstSleepingWorker();
  }

  // the queue is now empty, but workers may still be running
  bool workersRunning = false;
  do
  {
    workersRunning = false;

    // if any worker is running, we keep looping. We know a worker can't wake up again after it's
    // finished running because we force-drained the queue and nothing else will be adding work
    for(size_t i = 0; i < workers.size(); i++)
      workersRunning |= (Atomic::CmpExch32(&workers[i].running, 1, 1) == 1);

    // sleep rather than spinning
    if(workersRunning)
      Threading::Sleep(1);

  } while(workersRunning);

  // delete all jobs
  for(Job *job : allocatedJobs)
    delete job;
  allocatedJobs.clear();
}

Job *AddJob(std::function<void()> &&callback, const rdcarray<Job *> &parents)
{
  RDCASSERTEQUAL(mainThread, Threading::GetCurrentID());

  Job *ret = new Job;
  ret->callback = std::move(callback);
  ret->parents = parents;

  allocatedJobs.push_back(ret);

  {
    SCOPED_LOCK(queueLock);
    jobQueue.insert(0, ret);
  }

  TryWakeFirstSleepingWorker();

  return ret;
}

};    // namespace JobSystem

};    // namespace Threading
