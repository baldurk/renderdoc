/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2016 Baldur Karlsson
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

using std::string;

class PerformanceTimer
{
public:
  PerformanceTimer() : m_CounterFrequency(Timing::GetTickFrequency()) { Restart(); }
  double GetMilliseconds() const
  {
    return double(Timing::GetTick() - m_Start) / m_CounterFrequency;
  }

  void Restart() { m_Start = Timing::GetTick(); }
private:
  double m_CounterFrequency;
  uint64_t m_Start;
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
    rdclog_int(RDCLog_Comment, RDCLOG_PROJECT, m_File, m_Line, "Timer %s - %.3lf ms",
               m_Message.c_str(), m_Timer.GetMilliseconds());
  }

private:
  const char *m_File;
  unsigned int m_Line;
  string m_Message;
  PerformanceTimer m_Timer;
};

#define SCOPED_TIMER(...) ScopedTimer CONCAT(timer, __LINE__)(__FILE__, __LINE__, __VA_ARGS__);
