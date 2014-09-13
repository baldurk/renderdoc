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

#include <stdint.h>

typedef uint8_t byte;
typedef uint32_t bool32;

#ifdef WIN32

#ifdef RENDERDOC_EXPORTS
#define RENDERDOC_API __declspec(dllexport)
#else
#define RENDERDOC_API __declspec(dllimport)
#endif
#define RENDERDOC_CC __cdecl

#elif defined(LINUX)

#ifdef RENDERDOC_EXPORTS
#define RENDERDOC_API __attribute__ ((visibility ("default")))
#else
#define RENDERDOC_API
#endif

#define RENDERDOC_CC

#else

#error "Unknown platform"

#endif

#include <string>
#include <sstream> // istringstream/ostringstream used to avoid other dependencies

struct CaptureOptions
{
	CaptureOptions()
		: AllowVSync(true),
		  AllowFullscreen(true),
		  DebugDeviceMode(false),
		  CaptureCallstacks(false),
		  CaptureCallstacksOnlyDraws(false),
		  DelayForDebugger(0),
		  CacheStateObjects(true),
		  HookIntoChildren(false),
		  RefAllResources(false),
		  SaveAllInitials(false),
		  CaptureAllCmdLists(false)
	{}

	bool32 AllowVSync;
	bool32 AllowFullscreen;
	bool32 DebugDeviceMode;
	bool32 CaptureCallstacks;
	bool32 CaptureCallstacksOnlyDraws;
	uint32_t DelayForDebugger;
	bool32 CacheStateObjects;
	bool32 HookIntoChildren;
	bool32 RefAllResources;
	bool32 SaveAllInitials;
	bool32 CaptureAllCmdLists;
	
#ifdef __cplusplus
	void FromString(std::string str)
	{
		std::istringstream iss(str);

		iss >> AllowFullscreen
				>> AllowVSync
				>> DebugDeviceMode
				>> CaptureCallstacks
				>> CaptureCallstacksOnlyDraws
				>> DelayForDebugger
				>> CacheStateObjects
				>> HookIntoChildren
				>> RefAllResources
				>> SaveAllInitials
				>> CaptureAllCmdLists;
	}

	std::string ToString() const
	{
		std::ostringstream oss;

		oss << AllowFullscreen << " "
				<< AllowVSync << " "
				<< DebugDeviceMode << " "
				<< CaptureCallstacks << " "
				<< CaptureCallstacksOnlyDraws << " "
				<< DelayForDebugger << " "
				<< CacheStateObjects << " "
				<< HookIntoChildren << " "
				<< RefAllResources << " "
				<< SaveAllInitials << " "
				<< CaptureAllCmdLists << " ";

		return oss.str();
	}
#endif
};

enum InAppOverlay
{
	eOverlay_Enabled = 0x1,
	eOverlay_FrameRate = 0x2,
	eOverlay_FrameNumber = 0x4,
	eOverlay_CaptureList = 0x8,

	eOverlay_Default = (eOverlay_Enabled|eOverlay_FrameRate|eOverlay_FrameNumber|eOverlay_CaptureList),
	eOverlay_All = ~0U,
	eOverlay_None = 0,
};

#define RENDERDOC_API_VERSION 1

//////////////////////////////////////////////////////////////////////////
// In-program functions
//////////////////////////////////////////////////////////////////////////

extern "C" RENDERDOC_API int RENDERDOC_CC RENDERDOC_GetAPIVersion();
typedef int (RENDERDOC_CC *pRENDERDOC_GetAPIVersion)();

extern "C" RENDERDOC_API void RENDERDOC_CC RENDERDOC_SetLogFile(const wchar_t *logfile);
typedef void (RENDERDOC_CC *pRENDERDOC_SetLogFile)(const wchar_t *logfile);

extern "C" RENDERDOC_API const wchar_t* RENDERDOC_CC RENDERDOC_GetLogFile();
typedef const wchar_t* (RENDERDOC_CC *pRENDERDOC_GetLogFile)();

extern "C" RENDERDOC_API bool RENDERDOC_CC RENDERDOC_GetCapture(uint32_t idx, wchar_t *logfile, uint32_t *pathlength, uint64_t *timestamp);
typedef bool (RENDERDOC_CC *pRENDERDOC_GetCapture)(uint32_t idx, wchar_t *logfile, uint32_t *pathlength, uint64_t *timestamp);

extern "C" RENDERDOC_API void RENDERDOC_CC RENDERDOC_SetCaptureOptions(const CaptureOptions *opts);
typedef void (RENDERDOC_CC *pRENDERDOC_SetCaptureOptions)(const CaptureOptions *opts);

extern "C" RENDERDOC_API void RENDERDOC_CC RENDERDOC_SetActiveWindow(void *wndHandle);
typedef void (RENDERDOC_CC *pRENDERDOC_SetActiveWindow)(void *wndHandle);

extern "C" RENDERDOC_API void RENDERDOC_CC RENDERDOC_TriggerCapture();
typedef void (RENDERDOC_CC *pRENDERDOC_TriggerCapture)();

extern "C" RENDERDOC_API void RENDERDOC_CC RENDERDOC_StartFrameCapture(void *wndHandle);
typedef void (RENDERDOC_CC *pRENDERDOC_StartFrameCapture)(void *wndHandle);

extern "C" RENDERDOC_API bool RENDERDOC_CC RENDERDOC_EndFrameCapture(void *wndHandle);
typedef bool (RENDERDOC_CC *pRENDERDOC_EndFrameCapture)(void *wndHandle);

extern "C" RENDERDOC_API uint32_t RENDERDOC_CC RENDERDOC_GetOverlayBits();
typedef uint32_t (RENDERDOC_CC *pRENDERDOC_GetOverlayBits)();

extern "C" RENDERDOC_API void RENDERDOC_CC RENDERDOC_MaskOverlayBits(uint32_t And, uint32_t Or);
typedef void (RENDERDOC_CC *pRENDERDOC_MaskOverlayBits)(uint32_t And, uint32_t Or);

//////////////////////////////////////////////////////////////////////////
// Injection/execution capture functions.
//////////////////////////////////////////////////////////////////////////

extern "C" RENDERDOC_API uint32_t RENDERDOC_CC RENDERDOC_ExecuteAndInject(const wchar_t *app, const wchar_t *workingDir, const wchar_t *cmdLine,
																	const wchar_t *logfile, const CaptureOptions *opts, bool waitForExit);
typedef uint32_t (RENDERDOC_CC *pRENDERDOC_ExecuteAndInject)(const wchar_t *app, const wchar_t *workingDir, const wchar_t *cmdLine,
														 const wchar_t *logfile, const CaptureOptions *opts, bool waitForExit);
     
extern "C" RENDERDOC_API uint32_t RENDERDOC_CC RENDERDOC_InjectIntoProcess(uint32_t pid, const wchar_t *logfile, const CaptureOptions *opts, bool waitForExit);
typedef uint32_t (RENDERDOC_CC *pRENDERDOC_InjectIntoProcess)(uint32_t pid, const wchar_t *logfile, const CaptureOptions *opts, bool waitForExit);
