/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2023 Baldur Karlsson
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

#include <stdint.h>

// primarily here just to remove a dependency on QDateTime in the Qt UI, so we don't have to bind
// against Qt at all in the interface.
struct rdcdatetime
{
  int32_t year = 0;
  int32_t month = 0;
  int32_t day = 0;
  int32_t hour = 0;
  int32_t minute = 0;
  int32_t second = 0;
  int32_t microsecond = 0;

  rdcdatetime() = default;

  rdcdatetime(int y, int mn, int d, int h = 0, int m = 0, int s = 0, int us = 0)
      : year(y), month(mn), day(d), hour(h), minute(m), second(s), microsecond(us)
  {
  }

  bool operator==(const rdcdatetime &o) const
  {
    return year == o.year && month == o.month && day == o.day && hour == o.hour &&
           minute == o.minute && second == o.second && microsecond == o.microsecond;
  }
  bool operator!=(const rdcdatetime &o) const { return !(*this == o); }
  bool operator<(const rdcdatetime &o) const
  {
    if(year != o.year)
      return year < o.year;
    if(month != o.month)
      return month < o.month;
    if(day != o.day)
      return day < o.day;
    if(hour != o.hour)
      return hour < o.hour;
    if(minute != o.minute)
      return minute < o.minute;
    if(second != o.second)
      return second < o.second;
    if(microsecond != o.microsecond)
      return microsecond < o.microsecond;
    return false;
  }

#if defined(RENDERDOC_QT_COMPAT)
  rdcdatetime(const QDateTime &in)
  {
    year = in.date().year();
    month = in.date().month();
    day = in.date().day();
    hour = in.time().hour();
    minute = in.time().minute();
    second = in.time().second();
    microsecond = in.time().msec() * 1000;
  }
  operator QDateTime() const
  {
    return QDateTime(QDate(year, month, day), QTime(hour, minute, second, microsecond / 1000));
  }
  operator QVariant() const { return QVariant(QDateTime(*this)); }
#endif
};
