/******************************************************************************
 * The MIT License (MIT)
 * 
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

#include "os/os_specific.h"
#include "common/threading.h"

#include "string_utils.h" 

#include <string>
using std::string;

template<> const wchar_t* pathSeparator() { return L"\\/"; }
template<> const char* pathSeparator() { return "\\/"; }

template<> const wchar_t* curdir() { return L"."; }
template<> const char* curdir() { return "."; }

std::wstring widen(std::string str)
{
	return std::wstring(str.begin(), str.end());
}

std::string narrow(std::wstring str)
{
	return std::string(str.begin(), str.end());
}

void rdcassert(const char *condition, const char *file, unsigned int line, const char *func)
{
	rdclog_int(RDCLog_Error, file, line, "Assertion failed: '%hs'", condition, file, line);
}

static __m128 zero = {0};

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
#elif defined(WIN64)
	uint64_t *a64 = (uint64_t *)a;
	uint64_t *b64 = (uint64_t *)b;

	return a64[0] != b64[0] ||
		   a64[1] != b64[1];
#else
	uint32_t *a32 = (uint32_t *)a;
	uint32_t *b32 = (uint32_t *)b;

	return a32[0] != b32[0] ||
		   a32[1] != b32[1] ||
		   a32[2] != b32[2] ||
		   a32[3] != b32[3];
#endif
}

bool FindDiffRange(void *a, void *b, size_t bufSize, size_t &diffStart, size_t &diffEnd)
{
	RDCASSERT(((unsigned long)a)%16 == 0);
	RDCASSERT(((unsigned long)b)%16 == 0);

	diffStart = bufSize+1;
	diffEnd = 0;

	size_t alignedSize = bufSize&(~0xf);
	size_t numVecs = alignedSize/16;

	size_t offs = 0;

	float *aflt = (float *)a;
	float *bflt = (float *)b;

	// init a vector to 0
	__m128 zero = {0};

	// sweep to find the start of differences
	for(size_t v=0; v < numVecs; v++)
	{
		if(Vec16NotEqual(aflt, bflt))
		{
			diffStart = offs;
			break;
		}

		aflt+=4;bflt+=4;offs+=4*sizeof(float);
	}

	// make sure we're byte-accurate, to comply with WRITE_NO_OVERWRITE
	while(diffStart < bufSize && *((byte *)a + diffStart) == *((byte *)b + diffStart)) diffStart++;

	// do we have some unaligned bytes at the end of the buffer?
	if(bufSize > alignedSize)
	{
		size_t numBytes = alignedSize-bufSize;

		// if we haven't even found a start, check in these bytes
		if(diffStart > bufSize)
		{
			offs = bufSize;

			for(size_t by=0; by < numBytes; by++)
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
		for(size_t by=0; by < numBytes; by++)
		{
			if(*((byte *)a + bufSize-1 - by) != *((byte *)b + bufSize-1 - by))
			{
				diffEnd = bufSize-by;
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
	aflt = (float *)a + offs/sizeof(float) - 4;
	bflt = (float *)b + offs/sizeof(float) - 4;

	for(size_t v=0; v < numVecs; v++)
	{
		if(Vec16NotEqual(aflt, bflt))
		{
			diffEnd = offs;
			break;
		}

		aflt-=4;bflt-=4;offs-=16;
	}
	
	// make sure we're byte-accurate, to comply with WRITE_NO_OVERWRITE
	while(diffEnd > 0 && *((byte *)a + diffEnd - 1) == *((byte *)b + diffEnd - 1)) diffEnd--;
	
	// if we found a start then we necessarily found an end
	return diffStart < bufSize;
}

static wstring &logfile()
{
	static wstring fn;
	return fn;
}

const wchar_t *rdclog_getfilename()
{
	return logfile().c_str();
}

void rdclog_filename(const wchar_t *filename)
{
	logfile() = L"";
	if(filename && filename[0])
		logfile() = filename;
}

void rdclog_delete()
{
	if(!logfile().empty())
		FileIO::UnlinkFileW(logfile().c_str());
}

void rdclog_flush()
{
}

void rdclog_int(LogType type, const char *file, unsigned int line, const char *fmt, ...)
{
	if(type <= RDCLog_First || type >= RDCLog_NumTypes)
	{
		RDCFATAL("Unexpected log type");
		return;
	}
	
	va_list args;
	va_start(args, fmt);

	const char *name = "RENDERDOC: ";

	char timestamp[64] = {0};
#if defined(INCLUDE_TIMESTAMP_IN_LOG)
	StringFormat::sntimef(timestamp, 63, "[%H:%M:%S] ");
#endif
	
	char location[64] = {0};
#if defined(INCLUDE_LOCATION_IN_LOG)
	string loc;
	loc = basename(string(file));
	StringFormat::snprintf(location, 63, "% 20s(%4d) - ", loc.c_str(), line);
#endif

	const char *typestr[RDCLog_NumTypes] = {
		"Debug  ",
		"Log    ",
		"Warning",
		"Error  ",
		"Fatal  ",
	};
	
	const size_t outBufSize = 4*1024;
	char outputBuffer[outBufSize+1];
	outputBuffer[outBufSize] = 0;

	char *output = outputBuffer;
	size_t available = outBufSize;
	
	int numWritten = StringFormat::snprintf(output, available, "%hs %hs%hs%hs - ", name, timestamp, location, typestr[type]);

	if(numWritten < 0)
	{
		va_end(args);
		return;
	}

	output += numWritten;
	available -= numWritten;

	numWritten = StringFormat::vsnprintf(output, available, fmt, args);

	va_end(args);

	if(numWritten < 0)
		return;

	output += numWritten;
	available -= numWritten;

	if(available < 2)
		return;

	*output = '\n';
	*(output+1) = 0;
	
	{
		static Threading::CriticalSection lock;

		SCOPED_LOCK(lock);

#if defined(OUTPUT_LOG_TO_DEBUG_OUT)
		OSUtility::DebugOutputA(outputBuffer);
#endif
#if defined(OUTPUT_LOG_TO_STDOUT)
		fprintf(stdout, "%hs", outputBuffer); fflush(stdout);
#endif
#if defined(OUTPUT_LOG_TO_STDERR)
		fprintf(stderr, "%hs", outputBuffer); fflush(stderr);
#endif
#if defined(OUTPUT_LOG_TO_DISK)
		if(!logfile().empty())
		{
			FILE *f = FileIO::fopen(logfile().c_str(), L"a");
			if(f)
			{
				FileIO::fwrite(outputBuffer, 1, strlen(outputBuffer), f);
				FileIO::fclose(f);
			}
		}
#endif
	}
}
