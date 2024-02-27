/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2024 Baldur Karlsson
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

#include "common.h"
#include <stdarg.h>
#include <string.h>
#include "common/threading.h"
#include "os/os_specific.h"
#include "strings/string_utils.h"

int utf8printv(char *buf, size_t bufsize, const char *fmt, va_list args);
int utf8printf(char *str, size_t bufSize, const char *fmt, ...);

void rdcassert(const char *msg, const char *file, unsigned int line, const char *func)
{
  rdclog_direct(FILL_AUTO_VALUE, FILL_AUTO_VALUE, LogType::Error, RDCLOG_PROJECT, file, line,
                "Assertion failed: %s", msg);
}

#if 0
static __m128 zero = {0};
#endif

// assumes a and b both point to 16-byte aligned 16-byte chunks of memory.
// Returns if they're equal or different
bool Vec16NotEqual(void *a, void *b)
{
// disabled SSE version as it's acting dodgy
#if 0
	__m128 avec = _mm_load_ps(aflt);
	__m128 bvec = _mm_load_ps(bflt);

	__m128 diff = _mm_xor_ps(avec, bvec);

	__m128 eq = _mm_cmpeq_ps(diff, zero);
	int mask = _mm_movemask_ps(eq);
	int signMask = _mm_movemask_ps(diff);

	// first check ensures that diff is floatequal to zero (ie. avec bitwise equal to bvec).
	// HOWEVER -0 is floatequal to 0, so we ensure no sign bits are set on diff
	if((mask^0xf) || signMask != 0)
	{
		return true;
	}
	
	return false;
#elif ENABLED(RDOC_X64)
  uint64_t *a64 = (uint64_t *)a;
  uint64_t *b64 = (uint64_t *)b;

  return a64[0] != b64[0] || a64[1] != b64[1];
#else
  uint32_t *a32 = (uint32_t *)a;
  uint32_t *b32 = (uint32_t *)b;

  return a32[0] != b32[0] || a32[1] != b32[1] || a32[2] != b32[2] || a32[3] != b32[3];
#endif
}

bool FindDiffRange(void *a, void *b, size_t bufSize, size_t &diffStart, size_t &diffEnd)
{
  RDCASSERT(uintptr_t(a) % 16 == 0);
  RDCASSERT(uintptr_t(b) % 16 == 0);

  diffStart = bufSize + 1;
  diffEnd = 0;

  size_t alignedSize = bufSize & (~0xf);
  size_t numVecs = alignedSize / 16;

  size_t offs = 0;

  float *aflt = (float *)a;
  float *bflt = (float *)b;

  // sweep to find the start of differences
  for(size_t v = 0; v < numVecs; v++)
  {
    if(Vec16NotEqual(aflt, bflt))
    {
      diffStart = offs;
      break;
    }

    aflt += 4;
    bflt += 4;
    offs += 4 * sizeof(float);
  }

  // make sure we're byte-accurate, to comply with WRITE_NO_OVERWRITE
  while(diffStart < bufSize && *((byte *)a + diffStart) == *((byte *)b + diffStart))
    diffStart++;

  // do we have some unaligned bytes at the end of the buffer?
  if(bufSize > alignedSize)
  {
    size_t numBytes = bufSize - alignedSize;

    // if we haven't even found a start, check in these bytes
    if(diffStart > bufSize)
    {
      offs = alignedSize;

      for(size_t by = 0; by < numBytes; by++)
      {
        if(*((byte *)a + alignedSize + by) != *((byte *)b + alignedSize + by))
        {
          diffStart = offs;
          break;
        }

        offs++;
      }
    }

    // sweep from the last byte to find the end
    for(size_t by = 0; by < numBytes; by++)
    {
      if(*((byte *)a + bufSize - 1 - by) != *((byte *)b + bufSize - 1 - by))
      {
        diffEnd = bufSize - by;
        break;
      }
    }
  }

  // if we haven't found a start, or we've found a start AND and end,
  // then we're done.
  if(diffStart > bufSize || diffEnd > 0)
    return diffStart < bufSize;

  offs = alignedSize;

  // sweep from the last __m128
  aflt = (float *)a + offs / sizeof(float) - 4;
  bflt = (float *)b + offs / sizeof(float) - 4;

  for(size_t v = 0; v < numVecs; v++)
  {
    if(Vec16NotEqual(aflt, bflt))
    {
      diffEnd = offs;
      break;
    }

    aflt -= 4;
    bflt -= 4;
    offs -= 16;
  }

  // make sure we're byte-accurate, to comply with WRITE_NO_OVERWRITE
  while(diffEnd > 0 && *((byte *)a + diffEnd - 1) == *((byte *)b + diffEnd - 1))
    diffEnd--;

  // if we found a start then we necessarily found an end
  return diffStart < bufSize;
}

uint32_t CalcNumMips(int w, int h, int d)
{
  int mipLevels = 1;

  while(w > 1 || h > 1 || d > 1)
  {
    w = RDCMAX(1, w >> 1);
    h = RDCMAX(1, h >> 1);
    d = RDCMAX(1, d >> 1);
    mipLevels++;
  }

  return mipLevels;
}

byte *AllocAlignedBuffer(uint64_t size, uint64_t alignment)
{
  byte *rawAlloc = NULL;

#if defined(__EXCEPTIONS) || defined(_CPPUNWIND)
  try
#endif
  {
    rawAlloc = new byte[(size_t)size + sizeof(byte *) + (size_t)alignment];
  }
#if defined(__EXCEPTIONS) || defined(_CPPUNWIND)
  catch(std::bad_alloc &)
  {
    rawAlloc = NULL;
  }
#endif

  if(rawAlloc == NULL)
    RDCFATAL("Allocation for %llu bytes failed", size);

  RDCASSERT(rawAlloc);

  byte *alignedAlloc = (byte *)AlignUp(uint64_t(rawAlloc + sizeof(byte *)), alignment);

  byte **realPointer = (byte **)alignedAlloc;
  realPointer[-1] = rawAlloc;

  return alignedAlloc;
}

void FreeAlignedBuffer(byte *buf)
{
  if(buf == NULL)
    return;

  byte **realPointer = (byte **)buf;
  byte *rawAlloc = realPointer[-1];

  delete[] rawAlloc;
}

uint32_t Log2Floor(uint32_t value)
{
  if(!value)
    return ~0U;
  return 31 - Bits::CountLeadingZeroes(value);
}

#if ENABLED(RDOC_X64)
uint64_t Log2Floor(uint64_t value)
{
  if(!value)
    return ~0ULL;
  return 63 - Bits::CountLeadingZeroes(value);
}
#endif

uint32_t Log2Ceil(uint32_t value)
{
  if(!value)
    return ~0U;
  return 32 - Bits::CountLeadingZeroes(value - 1);
}

#if ENABLED(RDOC_X64)
uint64_t Log2Ceil(uint64_t value)
{
  if(!value)
    return ~0ULL;
  return 64 - Bits::CountLeadingZeroes(value - 1);
}
#endif

// deliberately leak so it doesn't get destroyed before our static RenderDoc destructor needs it
static rdcstr *logfile = new rdcstr;
static FileIO::LogFileHandle *logfileHandle = NULL;

const char *rdclog_getfilename()
{
  return logfile->c_str();
}

void rdclog_filename(const char *filename)
{
  rdcstr previous = *logfile;

  *logfile = "";
  if(filename && filename[0])
    *logfile = filename;

  FileIO::logfile_close(logfileHandle, rdcstr());

  logfileHandle = NULL;

  if(!logfile->empty())
  {
    logfileHandle = FileIO::logfile_open(*logfile);

    if(logfileHandle && !previous.empty())
    {
      rdcstr previousContents;
      FileIO::ReadAll(previous, previousContents);

      if(!previousContents.empty())
        FileIO::logfile_append(logfileHandle, previousContents.c_str(), previousContents.length());

      FileIO::Delete(previous);
    }
  }
}

static bool log_output_enabled = false;

void rdclog_enableoutput()
{
  log_output_enabled = true;
}

void rdclog_closelog()
{
  log_output_enabled = false;
  FileIO::logfile_close(logfileHandle, *logfile);
}

void rdclog_flush()
{
}

void rdclogprint_int(LogType type, const char *fullMsg, const char *msg)
{
  static Threading::CriticalSection *lock = new Threading::CriticalSection();

  SCOPED_LOCK(*lock);

#if ENABLED(OUTPUT_LOG_TO_DEBUG_OUT)
  OSUtility::WriteOutput(OSUtility::Output_DebugMon, fullMsg);
#endif
#if ENABLED(OUTPUT_LOG_TO_STDOUT)
  // don't output debug messages to stdout/stderr
  if(type != LogType::Debug && log_output_enabled)
    OSUtility::WriteOutput(OSUtility::Output_StdOut, msg);
#endif
#if ENABLED(OUTPUT_LOG_TO_STDERR)
  // don't output debug messages to stdout/stderr
  if(type != LogType::Debug && log_output_enabled)
    OSUtility::WriteOutput(OSUtility::Output_StdErr, msg);
  else
#endif
  {
    // always output fatal errors to stderr no matter what, even if not normally enabled, to catch
    // errors during startup
    if(type == LogType::Fatal)
      OSUtility::WriteOutput(OSUtility::Output_StdErr, msg);
  }
#if ENABLED(OUTPUT_LOG_TO_DISK)
  if(logfileHandle)
  {
    // strlen used as byte length - str is UTF-8 so this is NOT number of characters
    FileIO::logfile_append(logfileHandle, fullMsg, strlen(fullMsg));
  }
#endif
}

const int rdclog_outBufSize = 4 * 1024;
static char rdclog_outputBuffer[rdclog_outBufSize + 3];

static void write_newline(char *output)
{
#if ENABLED(RDOC_WIN32)
  *(output++) = '\r';
#endif
  *(output++) = '\n';
  *output = 0;
}

void rdclog_direct(time_t utcTime, uint32_t pid, LogType type, const char *project,
                   const char *file, unsigned int line, const char *fmt, ...)
{
  if(utcTime == FILL_AUTO_VALUE)
    utcTime = Timing::GetUTCTime();

  static uint32_t curpid = Process::GetCurrentPID();

  if(pid == FILL_AUTO_VALUE)
    pid = curpid;

  va_list args;
  va_start(args, fmt);

  // this copy is just for in case we need to print again if the buffer is oversized
  va_list args2;
  va_copy(args2, args);

  rdcstr timestamp;
#if ENABLED(INCLUDE_TIMESTAMP_IN_LOG)
  timestamp = StringFormat::sntimef(utcTime, "[%H:%M:%S] ");
#endif

  char location[64] = {0};
#if ENABLED(INCLUDE_LOCATION_IN_LOG)
  rdcstr loc;
  loc = get_basename(file);
  utf8printf(location, 63, "% 20s(%4d) - ", loc.c_str(), line);
#endif

  const char *typestr[(uint32_t)LogType::Count] = {
      "Debug  ", "Log    ", "Warning", "Error  ", "Fatal  ",
  };

  static Threading::CriticalSection *lock = new Threading::CriticalSection();

  SCOPED_LOCK(*lock);

  rdclog_outputBuffer[rdclog_outBufSize] = rdclog_outputBuffer[0] = 0;

  char *output = rdclog_outputBuffer;
  size_t available = rdclog_outBufSize;

  char *base = output;

  int numWritten = utf8printf(output, available, "% 4s %06u: %s%s%s - ", project, pid,
                              timestamp.c_str(), location, typestr[(uint32_t)type]);

  if(numWritten < 0)
  {
    va_end(args);
    va_end(args2);
    return;
  }

  output += numWritten;
  available -= numWritten;

  // -3 is for the " - " after the type.
  const char *noPrefixOutput = (output - 3 - (sizeof(typestr[(uint32_t)type]) - 1));
  const char *prefixEnd = output;

  int totalWritten = numWritten;

  numWritten = utf8printv(output, available, fmt, args);

  totalWritten += numWritten;

  va_end(args);

  if(numWritten < 0)
  {
    va_end(args2);
    return;
  }

  output += numWritten;

  // we overran the static buffer. This is a 4k buffer so we won't be hitting this case often - just
  // do the simple thing of allocating a temporary, print again, and re-assigning.
  char *oversizedBuffer = NULL;
  if(totalWritten > rdclog_outBufSize)
  {
    available = totalWritten + 3;
    oversizedBuffer = output = new char[available + 3];
    base = output;

    numWritten =
        utf8printf(output, available, "% 4s %06u: %s%s%s - ", project, Process::GetCurrentPID(),
                   timestamp.c_str(), location, typestr[(uint32_t)type]);

    output += numWritten;
    available -= numWritten;

    prefixEnd = output;

    noPrefixOutput = (output - 3 - (sizeof(typestr[(uint32_t)type]) - 1));

    numWritten = utf8printv(output, available, fmt, args2);

    output += numWritten;

    va_end(args2);
  }

  // normalise newlines
  {
    char *nl = base;
    while(*nl)
    {
      if(*nl == '\r')
        *nl = '\n';
      nl++;
    }
  }

  // likely path - string contains no newlines
  char *nl = strchr(base, '\n');
  if(nl == NULL)
  {
    // append newline
    write_newline(output);

    rdclogprint_int(type, base, noPrefixOutput);
  }
  else
  {
    char backup[2];

    rdcstr prefixText(base, size_t(prefixEnd - base));

    bool first = true;

    // otherwise, print the string in sections to ensure newlines are in native format
    while(nl)
    {
      // backup the two characters after the \n, to allow for DOS newlines ('\r' '\n' '\0')
      backup[0] = nl[1];
      backup[1] = nl[2];

      write_newline(nl);

      if(first)
        rdclogprint_int(type, base, noPrefixOutput);
      else
        rdclogprint_int(type, (prefixText + base).c_str(), noPrefixOutput);

      // restore the characters
      nl[1] = backup[0];
      nl[2] = backup[1];

      base = nl + 1;
      noPrefixOutput = nl + 1;

      first = false;

      nl = strchr(base, '\n');
    }

    // append final newline and write the last line
    write_newline(output);

    if(first)
      rdclogprint_int(type, base, noPrefixOutput);
    else
      rdclogprint_int(type, (prefixText + base).c_str(), noPrefixOutput);
  }

  SAFE_DELETE_ARRAY(oversizedBuffer);
}
