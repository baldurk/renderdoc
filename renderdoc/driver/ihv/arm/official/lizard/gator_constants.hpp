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

#ifndef GATOR_CONSTANTS_H
#define GATOR_CONSTANTS_H

namespace lizard
{
enum
{
  FRAME_UNKNOWN = 0,
  FRAME_SUMMARY = 1,
  FRAME_BACKTRACE = 2,
  FRAME_NAME = 3,
  FRAME_COUNTER = 4,
  FRAME_BLOCK_COUNTER = 5,
  FRAME_ANNOTATE = 6,
  FRAME_SCHED_TRACE = 7,
  FRAME_IDLE = 9,
  FRAME_EXTERNAL = 10,
  FRAME_PERF_ATTRS = 11,
  FRAME_PROC = 11,
  FRAME_PERF = 12,
  FRAME_ACTIVITY_TRACE = 13,
};

enum
{
  RESPONSE_XML = 1,
  RESPONSE_APC_DATA = 3,
  RESPONSE_ACK = 4,
  RESPONSE_NAK = 5,
  RESPONSE_ERROR = 0xFF
};

enum
{
  COMMAND_REQUEST_XML = 0,
  COMMAND_DELIVER_XML = 1,
  COMMAND_APC_START = 2,
  COMMAND_APC_STOP = 3,
  COMMAND_DISCONNECT = 4,
  COMMAND_PING = 5
};

enum
{
  CLASS_UNKNOWN,
  CLASS_ABSOLUTE,
  CLASS_ACTIVITY,
  CLASS_DELTA,
  CLASS_INCIDENT
};

enum
{
  DISPLAY_UNKNOWN,
  DISPLAY_ACCUMULATE,
  DISPLAY_AVERAGE,
  DISPLAY_MAXIMUM,
  DISPLAY_MINIMUM,
  DISPLAY_HERTZ
};

} /* namespace lizard */

#endif    // GATOR_CONSTANTS_H
