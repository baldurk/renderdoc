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


#pragma once

/////////////////////////////////////////////////
// Global constants
enum
{
	RenderDoc_FirstCaptureNetworkPort = 38920,
	RenderDoc_LastCaptureNetworkPort = RenderDoc_FirstCaptureNetworkPort + 7,
	RenderDoc_ReplayNetworkPort = 39920,
};

/////////////////////////////////////////////////
// Debugging features configuration

// remove all logging code
//#define STRIP_LOG

// remove all compile time asserts. Normally done even in release
// but this would speed up compilation
//#define STRIP_COMPILE_ASSERTS

// force asserts regardless of debug/release mode
#define FORCE_ASSERTS 

// force debugbreaks regardless of debug/release mode
//#define FORCE_DEBUGBREAK

// synchronously (ie. flushed to disk) write out the text-mode only serialised
// data of ALL serialising. Helps in debugging crashes to know exactly which
// calls have been made
//#define DEBUG_TEXT_SERIALISER

/////////////////////////////////////////////////
// Logging configuration

// error logs trigger a breakpoint
#define DEBUGBREAK_ON_ERROR_LOG

// whether to include timestamp on log lines
#define INCLUDE_TIMESTAMP_IN_LOG

// whether to include file and line on log lines
#define INCLUDE_LOCATION_IN_LOG

#if !defined(WIN32)
// logs go to stdout/stderr
#define OUTPUT_LOG_TO_STDOUT
//#define OUTPUT_LOG_TO_STDERR
#endif

// logs go to debug output (visual studio output window)
#define OUTPUT_LOG_TO_DEBUG_OUT

// logs go to disk
#define OUTPUT_LOG_TO_DISK

// normally only in a debug build do we
// include debug logs. This prints them all the time
//#define FORCE_DEBUG_LOGS
// this strips them completely
//#define STRIP_DEBUG_LOGS

/////////////////////////////////////////////////
// optional features

#if defined(NVIDIA_PERFKIT_DIR)
#define ENABLE_NVIDIA_PERFKIT
#endif

#if defined(AMD_PERFAPI_DIR)
#define ENABLE_AMD_PERFAPI
#endif
