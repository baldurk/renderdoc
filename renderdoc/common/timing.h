/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2019 Baldur Karlsson
 * Copyright (c) 2014 Crytek
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

#pragma once

#include <stdarg.h>
#include <stdint.h>
#include <string>
#include "os/os_specific.h"
#include "common.h"

class PerformanceTimer
{
public:
  PerformanceTimer() : m_CounterFrequency(Timing::GetTickFrequency()) { Restart(); }
  double GetMilliseconds() const
  {
    return double(Timing::GetTick() - m_Start) / m_CounterFrequency;
  }
  double GetMicroseconds() const
  {
    return (double(Timing::GetTick() - m_Start) * 1000.0) / m_CounterFrequency;
  }

  void Restart() { m_Start = Timing::GetTick(); }
private:
  double m_CounterFrequency;
  uint64_t m_Start;
};

class FrameTimer
{
public:
  void InitTimers()
  {
    m_HighPrecisionTimer.Restart();
    m_TotalTime = m_AvgFrametime = m_MinFrametime = m_MaxFrametime = 0.0;
  }

  void UpdateTimers()
  {
    m_FrameTimes.push_back(m_HighPrecisionTimer.GetMilliseconds());
    m_TotalTime += m_FrameTimes.back();
    m_HighPrecisionTimer.Restart();

    // update every second
    if(m_TotalTime > 1000.0)
    {
      m_MinFrametime = 10000.0;
      m_MaxFrametime = 0.0;
      m_AvgFrametime = 0.0;

      m_TotalTime = 0.0;

      for(size_t i = 0; i < m_FrameTimes.size(); i++)
      {
        m_AvgFrametime += m_FrameTimes[i];
        if(m_FrameTimes[i] < m_MinFrametime)
          m_MinFrametime = m_FrameTimes[i];
        if(m_FrameTimes[i] > m_MaxFrametime)
          m_MaxFrametime = m_FrameTimes[i];
      }

      m_AvgFrametime /= double(m_FrameTimes.size());

      m_FrameTimes.clear();
    }
  }

  double GetAvgFrameTime() const { return m_AvgFrametime; }
  double GetMinFrameTime() const { return m_MinFrametime; }
  double GetMaxFrameTime() const { return m_MaxFrametime; }
private:
  PerformanceTimer m_HighPrecisionTimer;
  std::vector<double> m_FrameTimes;
  double m_TotalTime;
  double m_AvgFrametime;
  double m_MinFrametime;
  double m_MaxFrametime;
};

class ScopedTimer
{
public:
  ScopedTimer(const char *file, unsigned int line, const char *fmt, ...)
  {
    m_File = file;
    m_Line = line;

    va_list args;
    va_start(args, fmt);

    char buf[1024];
    buf[1023] = 0;
    StringFormat::vsnprintf(buf, 1023, fmt, args);

    m_Message = buf;

    va_end(args);
  }

  ~ScopedTimer()
  {
    rdclog_direct(Timing::GetUTCTime(), Process::GetCurrentPID(), LogType::Comment, RDCLOG_PROJECT,
                  m_File, m_Line, "Timer %s - %.3lf ms", m_Message.c_str(),
                  m_Timer.GetMilliseconds());
  }

private:
  const char *m_File;
  unsigned int m_Line;
  std::string m_Message;
  PerformanceTimer m_Timer;
};

#define SCOPED_TIMER(...) ScopedTimer CONCAT(timer, __LINE__)(__FILE__, __LINE__, __VA_ARGS__);
