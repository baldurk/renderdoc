/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2020 Samsung Electronics (UK) Limited
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

#include "lizard.hpp"

#include <unistd.h>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>

#include "gator_api.hpp"
#include "hwcpipe_communication.hpp"
#include "lizard_communication.hpp"
#include "lizard_counter.hpp"

namespace lizard
{
enum
{
  MAX_HOSTNAME_SIZE = 64,
};

Lizard::Lizard(void)
    : m_idCounter(0),
      m_availableCounters(),
      m_enabledCounters(),
      m_gatorApi(NULL),
      m_comm(NULL),
      m_HwcPipeApi(NULL),
      m_HwcPipe_comm(NULL)
{
}

Lizard::~Lizard()
{
  if(m_gatorApi)
  {
    delete m_gatorApi;
  }
  if(m_HwcPipeApi)
  {
    delete m_HwcPipeApi;
  }
}

bool Lizard::configure(const char *hostname, uint32_t port)
{
  m_availableCounters.clear();

  m_configuredGatord = configureGatord(hostname, port);
  m_configuredHwcPipe = configureHwcPipe();
  if(m_configuredGatord || m_configuredHwcPipe)
  {
    m_availableCounters.shrink_to_fit();
    m_enabledCounters.clear();
    m_enabledCounters.resize(m_availableCounters.size() + 1);    // +1 as 0 ID is invalid
    return true;
  }
  return false;
}

bool Lizard::configureGatord(const char *hostname, uint32_t port)
{
  m_gatorApi = new lizard::GatorApi(strndup(hostname, MAX_HOSTNAME_SIZE), port, m_availableCounters,
                                    m_dataStore);

  if(!m_gatorApi->createConnection())
  {
    return false;
  }

  if(!m_gatorApi->sendVersion())
  {
    m_gatorApi->destroyConnection();
    return false;
  }

  bool success = m_gatorApi->init(m_idCounter);

  m_gatorApi->sendDisconnect();
  m_gatorApi->destroyConnection();

  return success;
}

uint32_t Lizard::availableCountersCount(void) const
{
  return m_availableCounters.size();
}

void Lizard::enableCounters(const LizardCounterId *counterIds, uint32_t arraySize)
{
  for(uint32_t idx = 0; idx < arraySize; idx++)
  {
    m_enabledCounters[counterIds[idx]] = true;
  }
}

void Lizard::disableCounters(const LizardCounterId *counterIds, uint32_t arraySize)
{
  for(uint32_t idx = 0; idx < arraySize; idx++)
  {
    m_enabledCounters[counterIds[idx]] = false;
  }
}

bool Lizard::startCapture(void)
{
  m_dataStore.clear();

  bool started = false;

  if(m_configuredGatord)
  {
    started = startGatord() || started;
  }

  if(m_configuredHwcPipe)
  {
    started = startHwcPipe() || started;
  }

  return started;
}

bool Lizard::startGatord()
{
  if(m_comm != NULL)
  {
    return false;
  }

  std::vector<LizardCounter> enabledGatorCounters;

  for(size_t i = 0; i < m_availableCounters.size(); i++)
  {
    if(m_enabledCounters[m_availableCounters[i].id()] &&
       m_availableCounters[i].sourceType() == LizardCounter::SOURCE_GATORD)
    {
      enabledGatorCounters.push_back(m_availableCounters[i]);
    }
  }

  if(!m_gatorApi->resendConfiguration(enabledGatorCounters))
  {
    return false;
  }

  if(!m_gatorApi->startSession())
  {
    return false;
  }

  m_comm = new CommunicationThread(*m_gatorApi);
  m_comm->start();

  // Give gatord a bit of time to start up
  usleep(1);

  return true;
}

bool Lizard::startHwcPipe()
{
  if(m_HwcPipe_comm != NULL)
  {
    return false;
  }

  std::vector<LizardCounter> enabledHwcPipeCounters;

  for(size_t i = 0; i < m_availableCounters.size(); i++)
  {
    if(m_enabledCounters[m_availableCounters[i].id()] &&
       (m_availableCounters[i].sourceType() == LizardCounter::SOURCE_HWCPIPE_CPU ||
        m_availableCounters[i].sourceType() == LizardCounter::SOURCE_HWCPIPE_GPU))
    {
      enabledHwcPipeCounters.push_back(m_availableCounters[i]);
    }
  }

  if(enabledHwcPipeCounters.empty())
  {
    return false;
  }

  m_HwcPipeApi->enableCounters(enabledHwcPipeCounters);

  m_HwcPipe_comm = new HwcPipeThread(*m_HwcPipeApi);
  m_HwcPipe_comm->start();

  return true;
}

void Lizard::endCapture(void)
{
  stopGatord();
  stopHwcPipe();
}

void Lizard::stopGatord(void)
{
  if(m_comm != NULL)
  {
    usleep(1);
    m_comm->stop();

    delete m_comm;
    m_comm = NULL;
  }
}

void Lizard::stopHwcPipe(void)
{
  if(m_HwcPipe_comm != NULL)
  {
    m_HwcPipe_comm->stop();

    delete m_HwcPipe_comm;
    m_HwcPipe_comm = NULL;
  }
}

LizardCounterData *Lizard::readCounter(const LizardCounterId counterId) const
{
  if(counterId < 1 || counterId > m_availableCounters.size())
  {
    return NULL;
  }

  const std::vector<Value> &values = m_dataStore.getValues(counterId);

  switch(getCounterInfo(counterId)->sourceType())
  {
    case LizardCounter::SOURCE_GATORD:
    {
      std::vector<int64_t> vec(values.size());
      for(size_t i = 0; i < values.size(); i++)
      {
        vec[i] = values[i].as_int;
      }
      return new LizardCounterData(counterId, (int64_t *)&vec[0], vec.size());
    }
    case LizardCounter::SOURCE_HWCPIPE_CPU:
    case LizardCounter::SOURCE_HWCPIPE_GPU:
    {
      std::vector<double> vec(values.size());
      for(size_t i = 0; i < values.size(); i++)
      {
        vec[i] = values[i].as_double;
      }
      return new LizardCounterData(counterId, vec.data(), vec.size());
    }
  }

  return NULL;
}

size_t Lizard::readCounterInt(const LizardCounterId counterId, int64_t *values) const
{
  if(counterId < 1 || counterId > m_availableCounters.size())
  {
    return 0;
  }

  const std::vector<Value> &vals = m_dataStore.getValues(counterId);
  std::vector<int64_t> vec(vals.size());

  for(size_t i = 0; i < vals.size(); i++)
  {
    vec[i] = vals[i].as_int;
  }
  if(values)
  {
    memcpy(values, vec.data(), vec.size() * sizeof(int64_t));
  }

  return vec.size();
}

size_t Lizard::readCounterDouble(const LizardCounterId counterId, double *values) const
{
  if(counterId < 1 || counterId > m_availableCounters.size())
  {
    return 0;
  }

  const std::vector<Value> &vals = m_dataStore.getValues(counterId);
  std::vector<double> vec(vals.size());

  for(size_t i = 0; i < vals.size(); i++)
  {
    vec[i] = vals[i].as_double;
  }
  if(values)
  {
    memcpy(values, vec.data(), vec.size() * sizeof(double));
  }

  return vec.size();
}

const LizardCounter *Lizard::getCounterInfo(const LizardCounterId counterId) const
{
  if(counterId < 1 || counterId > m_availableCounters.size())
  {
    return NULL;
  }
  return &m_availableCounters[counterId - 1];
}

bool Lizard::configureHwcPipe()
{
  m_HwcPipeApi = new HwcPipeApi(m_availableCounters, m_dataStore);
  return m_HwcPipeApi->init(m_idCounter);
}

} /* namespace lizard */
