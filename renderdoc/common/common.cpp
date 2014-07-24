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

//	for(int i=0; i < 256; i++)
//	{
//		uint8_t comp = i&0xff;
//		float srgbF = float(comp)/255.0f;
//		
//		if(srgbF <= 0.04045f)
//		  SRGB8_lookuptable[comp] = srgbF/12.92f;
//		else
//		  SRGB8_lookuptable[comp] = powf((0.055f + srgbF) / 1.055f, 2.4f);
//	}

float SRGB8_lookuptable[256] = {
	0.000000f, 0.000304f, 0.000607f, 0.000911f, 0.001214f, 0.001518f, 0.001821f, 0.002125f,
	0.002428f, 0.002732f, 0.003035f, 0.003347f, 0.003677f, 0.004025f, 0.004391f, 0.004777f,
	0.005182f, 0.005605f, 0.006049f, 0.006512f, 0.006995f, 0.007499f, 0.008023f, 0.008568f,
	0.009134f, 0.009721f, 0.010330f, 0.010960f, 0.011612f, 0.012286f, 0.012983f, 0.013702f,
	0.014444f, 0.015209f, 0.015996f, 0.016807f, 0.017642f, 0.018500f, 0.019382f, 0.020289f,
	0.021219f, 0.022174f, 0.023153f, 0.024158f, 0.025187f, 0.026241f, 0.027321f, 0.028426f,
	0.029557f, 0.030713f, 0.031896f, 0.033105f, 0.034340f, 0.035601f, 0.036889f, 0.038204f,
	0.039546f, 0.040915f, 0.042311f, 0.043735f, 0.045186f, 0.046665f, 0.048172f, 0.049707f,
	0.051269f, 0.052861f, 0.054480f, 0.056128f, 0.057805f, 0.059511f, 0.061246f, 0.063010f,
	0.064803f, 0.066626f, 0.068478f, 0.070360f, 0.072272f, 0.074214f, 0.076185f, 0.078187f,
	0.080220f, 0.082283f, 0.084376f, 0.086500f, 0.088656f, 0.090842f, 0.093059f, 0.095307f,
	0.097587f, 0.099899f, 0.102242f, 0.104616f, 0.107023f, 0.109462f, 0.111932f, 0.114435f,
	0.116971f, 0.119538f, 0.122139f, 0.124772f, 0.127438f, 0.130136f, 0.132868f, 0.135633f,
	0.138432f, 0.141263f, 0.144128f, 0.147027f, 0.149960f, 0.152926f, 0.155926f, 0.158961f,
	0.162029f, 0.165132f, 0.168269f, 0.171441f, 0.174647f, 0.177888f, 0.181164f, 0.184475f,
	0.187821f, 0.191202f, 0.194618f, 0.198069f, 0.201556f, 0.205079f, 0.208637f, 0.212231f,
	0.215861f, 0.219526f, 0.223228f, 0.226966f, 0.230740f, 0.234551f, 0.238398f, 0.242281f,
	0.246201f, 0.250158f, 0.254152f, 0.258183f, 0.262251f, 0.266356f, 0.270498f, 0.274677f,
	0.278894f, 0.283149f, 0.287441f, 0.291771f, 0.296138f, 0.300544f, 0.304987f, 0.309469f,
	0.313989f, 0.318547f, 0.323143f, 0.327778f, 0.332452f, 0.337164f, 0.341914f, 0.346704f,
	0.351533f, 0.356400f, 0.361307f, 0.366253f, 0.371238f, 0.376262f, 0.381326f, 0.386430f,
	0.391573f, 0.396755f, 0.401978f, 0.407240f, 0.412543f, 0.417885f, 0.423268f, 0.428691f,
	0.434154f, 0.439657f, 0.445201f, 0.450786f, 0.456411f, 0.462077f, 0.467784f, 0.473532f,
	0.479320f, 0.485150f, 0.491021f, 0.496933f, 0.502887f, 0.508881f, 0.514918f, 0.520996f,
	0.527115f, 0.533276f, 0.539480f, 0.545725f, 0.552011f, 0.558340f, 0.564712f, 0.571125f,
	0.577581f, 0.584078f, 0.590619f, 0.597202f, 0.603827f, 0.610496f, 0.617207f, 0.623960f,
	0.630757f, 0.637597f, 0.644480f, 0.651406f, 0.658375f, 0.665387f, 0.672443f, 0.679543f,
	0.686685f, 0.693872f, 0.701102f, 0.708376f, 0.715694f, 0.723055f, 0.730461f, 0.737911f,
	0.745404f, 0.752942f, 0.760525f, 0.768151f, 0.775822f, 0.783538f, 0.791298f, 0.799103f,
	0.806952f, 0.814847f, 0.822786f, 0.830770f, 0.838799f, 0.846873f, 0.854993f, 0.863157f,
	0.871367f, 0.879622f, 0.887923f, 0.896269f, 0.904661f, 0.913099f, 0.921582f, 0.930111f,
	0.938686f, 0.947307f, 0.955974f, 0.964686f, 0.973445f, 0.982251f, 0.991102f, 1.000000f,
};

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
