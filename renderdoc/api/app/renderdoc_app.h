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
		  VerifyMapWrites(false),
		  HookIntoChildren(false),
		  RefAllResources(false),
		  SaveAllInitials(false),
		  CaptureAllCmdLists(false)
	{}

	// Whether or not to allow the application to enable vsync
	//
	// Enabled - allows the application to enable or disable vsync at will
	// Disabled - vsync is force disabled
	bool32 AllowVSync;
	
	// Whether or not to allow the application to enable fullscreen
	//
	// Enabled - allows the application to enable or disable fullscreen at will
	// Disabled - fullscreen is force disabled
	bool32 AllowFullscreen;

	// Enables in-built API debugging features and records the results into the
	// capture logfile, which is matched up with events on replay
	bool32 DebugDeviceMode;

	// Captures callstacks for every API event during capture
	bool32 CaptureCallstacks;

	// Only captures callstacks for drawcall type API events.
	// Ignored if CaptureCallstacks is disabled
	bool32 CaptureCallstacksOnlyDraws;

	// Specify a delay in seconds to wait for a debugger to attach after
	// creating or injecting into a process, before continuing to allow it to run.
	uint32_t DelayForDebugger;

	// Verify any writes to mapped buffers, to check that they don't overwrite the
	// bounds of the pointer returned.
	bool32 VerifyMapWrites;

	// Hooks any system API events that create child processes, and injects
	// renderdoc into them recursively with the same options.
	bool32 HookIntoChildren;

	// By default renderdoc only includes resources in the final logfile necessary
	// for that frame, this allows you to override that behaviour
	//
	// Enabled - all live resources at the time of capture are included in the log
	//           and available for inspection
	// Disabled - only the resources referenced by the captured frame are included
	bool32 RefAllResources;

	// By default renderdoc skips saving initial states for
	bool32 SaveAllInitials;

	// In APIs that allow for the recording of command lists to be replayed later,
	// renderdoc may choose to not capture command lists before a frame capture is
	// triggered, to reduce overheads. This means any command lists recorded once
	// and replayed many times will not be available and may cause a failure to
	// capture.
	//
	// Enabled - All command lists are captured from the start of the application
	// Disabled - Command lists are only captured if their recording begins during
	//            the period when a frame capture is in progress.
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
				>> VerifyMapWrites
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
				<< VerifyMapWrites << " "
				<< HookIntoChildren << " "
				<< RefAllResources << " "
				<< SaveAllInitials << " "
				<< CaptureAllCmdLists << " ";

		return oss.str();
	}
#endif
};

enum KeyButton
{
	eKey_0 = 0x30, // '0'
	// ...
	eKey_9 = 0x39, // '9'
	eKey_A = 0x41, // 'A'
	// ...
	eKey_Z = 0x5A, // 'Z'

	eKey_Divide,
	eKey_Multiply,
	eKey_Subtract,
	eKey_Plus,

	eKey_F1,
	eKey_F2,
	eKey_F3,
	eKey_F4,
	eKey_F5,
	eKey_F6,
	eKey_F7,
	eKey_F8,
	eKey_F9,
	eKey_F10,
	eKey_F11,
	eKey_F12,

	eKey_Home,
	eKey_End,
	eKey_Insert,
	eKey_Delete,
	eKey_PageUp,
	eKey_PageDn,

	eKey_Backspace,
	eKey_Tab,
	eKey_PrtScrn,
	eKey_Pause,

	eKey_Max,
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

// API breaking change history:
// Version 1 -> 2 - strings changed from wchar_t* to char* (UTF-8)
#define RENDERDOC_API_VERSION 2

//////////////////////////////////////////////////////////////////////////
// In-program functions
//////////////////////////////////////////////////////////////////////////

extern "C" RENDERDOC_API int RENDERDOC_CC RENDERDOC_GetAPIVersion();
typedef int (RENDERDOC_CC *pRENDERDOC_GetAPIVersion)();

extern "C" RENDERDOC_API void RENDERDOC_CC RENDERDOC_Shutdown();
typedef void (RENDERDOC_CC *pRENDERDOC_Shutdown)();

extern "C" RENDERDOC_API void RENDERDOC_CC RENDERDOC_SetLogFile(const char *logfile);
typedef void (RENDERDOC_CC *pRENDERDOC_SetLogFile)(const char *logfile);

extern "C" RENDERDOC_API const char* RENDERDOC_CC RENDERDOC_GetLogFile();
typedef const char* (RENDERDOC_CC *pRENDERDOC_GetLogFile)();

extern "C" RENDERDOC_API bool32 RENDERDOC_CC RENDERDOC_GetCapture(uint32_t idx, char *logfile, uint32_t *pathlength, uint64_t *timestamp);
typedef bool32 (RENDERDOC_CC *pRENDERDOC_GetCapture)(uint32_t idx, char *logfile, uint32_t *pathlength, uint64_t *timestamp);

extern "C" RENDERDOC_API void RENDERDOC_CC RENDERDOC_SetCaptureOptions(const CaptureOptions *opts);
typedef void (RENDERDOC_CC *pRENDERDOC_SetCaptureOptions)(const CaptureOptions *opts);

extern "C" RENDERDOC_API void RENDERDOC_CC RENDERDOC_SetActiveWindow(void *wndHandle);
typedef void (RENDERDOC_CC *pRENDERDOC_SetActiveWindow)(void *wndHandle);

extern "C" RENDERDOC_API void RENDERDOC_CC RENDERDOC_TriggerCapture();
typedef void (RENDERDOC_CC *pRENDERDOC_TriggerCapture)();

extern "C" RENDERDOC_API void RENDERDOC_CC RENDERDOC_StartFrameCapture(void *wndHandle);
typedef void (RENDERDOC_CC *pRENDERDOC_StartFrameCapture)(void *wndHandle);

extern "C" RENDERDOC_API bool32 RENDERDOC_CC RENDERDOC_EndFrameCapture(void *wndHandle);
typedef bool32 (RENDERDOC_CC *pRENDERDOC_EndFrameCapture)(void *wndHandle);

extern "C" RENDERDOC_API uint32_t RENDERDOC_CC RENDERDOC_GetOverlayBits();
typedef uint32_t (RENDERDOC_CC *pRENDERDOC_GetOverlayBits)();

extern "C" RENDERDOC_API void RENDERDOC_CC RENDERDOC_MaskOverlayBits(uint32_t And, uint32_t Or);
typedef void (RENDERDOC_CC *pRENDERDOC_MaskOverlayBits)(uint32_t And, uint32_t Or);

extern "C" RENDERDOC_API void RENDERDOC_CC RENDERDOC_SetFocusToggleKeys(KeyButton *keys, int num);
typedef void (RENDERDOC_CC *pRENDERDOC_SetFocusToggleKeys)(KeyButton *keys, int num);

extern "C" RENDERDOC_API void RENDERDOC_CC RENDERDOC_SetCaptureKeys(KeyButton *keys, int num);
typedef void (RENDERDOC_CC *pRENDERDOC_SetCaptureKeys)(KeyButton *keys, int num);

extern "C" RENDERDOC_API void RENDERDOC_CC RENDERDOC_InitRemoteAccess(uint32_t *ident);
typedef void (RENDERDOC_CC *pRENDERDOC_InitRemoteAccess)(uint32_t *ident);

extern "C" RENDERDOC_API void RENDERDOC_CC RENDERDOC_UnloadCrashHandler();
typedef void (RENDERDOC_CC *pRENDERDOC_UnloadCrashHandler)();
