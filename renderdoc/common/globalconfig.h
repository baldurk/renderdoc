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

/////////////////////////////////////////////////
// Build/machine configuration
#if defined(__LP64__) || defined(_WIN64) || defined(__x86_64__) || defined(_M_X64) || \
    defined(__ia64) || defined(_M_IA64) || defined(__aarch64__) || defined(__powerpc64__)
#define RDC64BIT 1
#endif

/////////////////////////////////////////////////
// Global constants
enum
{
  RenderDoc_FirstTargetControlPort = 38920,
  RenderDoc_LastTargetControlPort = RenderDoc_FirstTargetControlPort + 7,
  RenderDoc_RemoteServerPort = 39920,
  RenderDoc_AndroidPortOffset = 50,
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

/////////////////////////////////////////////////
// Logging configuration

// error logs trigger a breakpoint
#define DEBUGBREAK_ON_ERROR_LOG

// whether to include timestamp on log lines
#define INCLUDE_TIMESTAMP_IN_LOG

// whether to include file and line on log lines
#define INCLUDE_LOCATION_IN_LOG

#if !defined(RENDERDOC_PLATFORM_WIN32)
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
