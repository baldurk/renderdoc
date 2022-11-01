/*
BSD LICENSE

Copyright (c) 2019-2020 Superluminal. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

  * Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
  * Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in
    the documentation and/or other materials provided with the
    distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
#pragma once

#include <inttypes.h>

// When PERFORMANCEAPI_ENABLED is defined to 0, all calls to the PerformanceAPI (either through macro or direct function calls) will be compiled out.
#ifndef PERFORMANCEAPI_ENABLED
	#ifdef _WIN32
		#define	PERFORMANCEAPI_ENABLED 1
	#else
		#define	PERFORMANCEAPI_ENABLED 0
	#endif
#endif

#define PERFORMANCEAPI_MAJOR_VERSION 3
#define PERFORMANCEAPI_MINOR_VERSION 0
#define PERFORMANCEAPI_VERSION ((PERFORMANCEAPI_MAJOR_VERSION << 16) | PERFORMANCEAPI_MINOR_VERSION)

/**
 * This header has been designed to be fully self-contained, which makes it easy to copy this header into your own source tree as needed.
 *
 * See PerformanceAPI.h for the high level documentation on how to use the API.
 *
 * Note that this header is split into two sections: 
 * - The first section defines the static library interface. If you use these functions directly, you need to link against the PerformanceAPI static library.
 * - The second section defines the DLL interface. The DLL interface allows you to use the API without linking to a library. Instead, you can load the DLL yourself
 *   through LoadLibrary, then find the "PerformanceAPI_GetAPI" export through GetProcAddress. PerformanceAPI_GetAPI can be called to get a table of function pointers
 *   to the API. A convenience function to perform the DLL load & retrieve the API functions is provided for you in a separate header, PerformanceAPI_loader.h.
 */
#ifdef __cplusplus
extern "C" {
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Static library interface - if you use these functions, you need to link against the PerformanceAPI library
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * Helper struct that is used to prevent calls to EndEvent from being optimized to jmp instructions as part of tail call optimization.
 * You don't ever need to do anything with this as user of the API.
 */
typedef struct
{
	int64_t SuppressTailCall[3];
} PerformanceAPI_SuppressTailCallOptimization;

#if PERFORMANCEAPI_ENABLED
	/**
	 * Helper function to create an uint32_t color from 3 RGB values. The R, G and B values must be in range [0, 255].
	 * The resulting color can be passed to the BeginEvent function.
	 */
	#define PERFORMANCEAPI_MAKE_COLOR(R, G, B) ((((uint32_t)(R)) << 24) | (((uint32_t)(G)) << 16) | (((uint32_t)(B)) << 8) | (uint32_t)0xFF)

	/**
	 * Use this define if you don't care about the color of an event and just want to use the default
	 */
	#define PERFORMANCEAPI_DEFAULT_COLOR 0xFFFFFFFF

	/**
	 * Set the name of the current thread to the specified thread name. 
	 *
	 * @param inThreadName The thread name as an UTF8 encoded string.
	 */
	void PerformanceAPI_SetCurrentThreadName(const char* inThreadName);

	/**
	 * Set the name of the current thread to the specified thread name. 
	 *
	 * @param inThreadName The thread name as an UTF8 encoded string.
	 * @param inThreadNameLength The length of the thread name, in characters, excluding the null terminator.
	 */
	void PerformanceAPI_SetCurrentThreadName_N(const char* inThreadName, uint16_t inThreadNameLength);

	/**
	 * Begin an instrumentation event with the specified ID and runtime data
	 *
	 * @param inID    The ID of this scope as an UTF8 encoded string. The ID for a specific scope must be the same over the lifetime of the program (see docs at the top of this file)
	 * @param inData  [optional] The data for this scope as an UTF8 encoded string. The data can vary for each invocation of this scope and is intended to hold information that is only available at runtime. See docs at the top of this file.
	 *							 Set to null if not available.
	 * @param inColor [optional] The color for this scope. The color for a specific scope is coupled to the ID and must be the same over the lifetime of the program
	 *							 Set to PERFORMANCEAPI_DEFAULT_COLOR to use default coloring.
	 *
	 */
	void PerformanceAPI_BeginEvent(const char* inID, const char* inData, uint32_t inColor);

	/**
	 * Begin an instrumentation event with the specified ID and runtime data, both with an explicit length.
	 
	 * It works the same as the regular BeginEvent function (see docs above). The difference is that it allows you to specify the length of both the ID and the data,
	 * which is useful for languages that do not have null-terminated strings.
	 *
	 * Note: both lengths should be specified in the number of characters, not bytes, excluding the null terminator.
	 */
	void PerformanceAPI_BeginEvent_N(const char* inID, uint16_t inIDLength, const char* inData, uint16_t inDataLength, uint32_t inColor);

	/**
	 * Begin an instrumentation event with the specified ID and runtime data
	 *
	 * @param inID    The ID of this scope as an UTF16 encoded string. The ID for a specific scope must be the same over the lifetime of the program (see docs at the top of this file)
	 * @param inData  [optional] The data for this scope as an UTF16 encoded string. The data can vary for each invocation of this scope and is intended to hold information that is only available at runtime. See docs at the top of this file.
	 						     Set to null if not available.
	 * @param inColor [optional] The color for this scope. The color for a specific scope is coupled to the ID and must be the same over the lifetime of the program
	 *							 Set to PERFORMANCEAPI_DEFAULT_COLOR to use default coloring.
	 */
	void PerformanceAPI_BeginEvent_Wide(const wchar_t* inID, const wchar_t* inData, uint32_t inColor);

	/**
	 * Begin an instrumentation event with the specified ID and runtime data, both with an explicit length.
	 
	 * It works the same as the regular BeginEvent_Wide function (see docs above). The difference is that it allows you to specify the length of both the ID and the data,
	 * which is useful for languages that do not have null-terminated strings.
	 *
	 * Note: both lengths should be specified in the number of characters, not bytes, excluding the null terminator.
	 */
	void PerformanceAPI_BeginEvent_Wide_N(const wchar_t* inID, uint16_t inIDLength, const wchar_t* inData, uint16_t inDataLength, uint32_t inColor);

	/**
	 * End an instrumentation event. Must be matched with a call to BeginEvent within the same function
	 * Note: the return value can be ignored. It is only there to prevent calls to the function from being optimized to jmp instructions as part of tail call optimization.
	 */
	PerformanceAPI_SuppressTailCallOptimization PerformanceAPI_EndEvent();

	/**
	 * Call this function when a fiber starts running
	 *
	 * @param inFiberID    The currently running fiber
	 */
	void PerformanceAPI_RegisterFiber(uint64_t inFiberID);

	/**
	 * Call this function before a fiber ends
	 *
	 * @param inFiberID    The currently running fiber
	 */
	void PerformanceAPI_UnregisterFiber(uint64_t inFiberID);

	/**
	 * The call to the Windows SwitchFiber function should be surrounded by BeginFiberSwitch and EndFiberSwitch calls. For example:
	 * 
	 *		PerformanceAPI_BeginFiberSwitch(currentFiber, otherFiber);
	 *		SwitchToFiber(otherFiber);
	 *		PerformanceAPI_EndFiberSwitch(currentFiber);
	 *
	 * @param inCurrentFiberID    The currently running fiber
	 * @param inNewFiberID		  The fiber we're switching to
	 */
	void PerformanceAPI_BeginFiberSwitch(uint64_t inCurrentFiberID, uint64_t inNewFiberID);

	/**
	 * The call to the Windows SwitchFiber function should be surrounded by BeginFiberSwitch and EndFiberSwitch calls
	 * 	
	 *		PerformanceAPI_BeginFiberSwitch(currentFiber, otherFiber);
	 *		SwitchToFiber(otherFiber);
	 *		PerformanceAPI_EndFiberSwitch(currentFiber);
	 *
	 * @param inFiberID    The fiber that was running before the call to SwitchFiber (so, the same as inCurrentFiberID in the BeginFiberSwitch call)
	 */
	void PerformanceAPI_EndFiberSwitch(uint64_t inFiberID);
#else
	#define PERFORMANCEAPI_MAKE_COLOR(R, G, B) 0xFFFFFFFF
	#define PERFORMANCEAPI_DEFAULT_COLOR 0xFFFFFFFF

	inline void PerformanceAPI_SetCurrentThreadName(const char* inThreadName) {}
	inline void PerformanceAPI_SetCurrentThreadName_N(const char* inThreadName, uint16_t inThreadNameLength) {}
	inline void PerformanceAPI_BeginEvent(const char* inID, const char* inData, uint32_t inColor) {}
	inline void PerformanceAPI_BeginEvent_N(const char* inID, uint16_t inIDLength, const char* inData, uint16_t inDataLength, uint32_t inColor) {}
	inline void PerformanceAPI_BeginEvent_Wide(const wchar_t* inID, const wchar_t* inData, uint32_t inColor) {}
	inline void PerformanceAPI_BeginEvent_Wide_N(const wchar_t* inID, uint16_t inIDLength, const wchar_t* inData, uint16_t inDataLength, uint32_t inColor) {}
	inline void PerformanceAPI_EndEvent() {}

	inline void PerformanceAPI_RegisterFiber(uint64_t inFiberID) {}
	inline void PerformanceAPI_UnregisterFiber(uint64_t inFiberID) {}
	inline void PerformanceAPI_BeginFiberSwitch(uint64_t inCurrentFiberID, uint64_t inNewFiberID) {}
	inline void PerformanceAPI_EndFiberSwitch(uint64_t inFiberID) {}
#endif

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// DLL interface - These functions can be used without linking by loading PerformanceAPI.dll and using GetProcAddress to find the PerformanceAPI_GetAPI function.
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef void (PerformanceAPI_SetCurrentThreadName_Func)(const char* inThreadName);
typedef void (PerformanceAPI_SetCurrentThreadName_N_Func)(const char* inThreadName, uint16_t inThreadNameLength);
typedef void (PerformanceAPI_BeginEvent_Func)(const char* inID, const char* inData, uint32_t inColor);
typedef void (PerformanceAPI_BeginEvent_N_Func)(const char* inID, uint16_t inIDLength, const char* inData, uint16_t inDataLength, uint32_t inColor);
typedef void (PerformanceAPI_BeginEvent_Wide_Func)(const wchar_t* inID, const wchar_t* inData, uint32_t inColor);
typedef void (PerformanceAPI_BeginEvent_Wide_N_Func)(const wchar_t* inID, uint16_t inIDLength, const wchar_t* inData, uint16_t inDataLength, uint32_t inColor);
typedef PerformanceAPI_SuppressTailCallOptimization (PerformanceAPI_EndEvent_Func)();

typedef void (PerformanceAPI_RegisterFiber_Func)(uint64_t inFiberID);
typedef void (PerformanceAPI_UnregisterFiber_Func)(uint64_t inFiberID);
typedef void (PerformanceAPI_BeginFiberSwitch_Func)(uint64_t inCurrentFiberID, uint64_t inNewFiberID);
typedef void (PerformanceAPI_EndFiberSwitch_Func)(uint64_t inFiberID);

typedef struct
{
	PerformanceAPI_SetCurrentThreadName_Func*	SetCurrentThreadName;
	PerformanceAPI_SetCurrentThreadName_N_Func*	SetCurrentThreadNameN;
	PerformanceAPI_BeginEvent_Func*				BeginEvent;
	PerformanceAPI_BeginEvent_N_Func*			BeginEventN;
	PerformanceAPI_BeginEvent_Wide_Func*		BeginEventWide;
	PerformanceAPI_BeginEvent_Wide_N_Func*		BeginEventWideN;
	PerformanceAPI_EndEvent_Func*				EndEvent;

	PerformanceAPI_RegisterFiber_Func*			RegisterFiber;
	PerformanceAPI_UnregisterFiber_Func*		UnregisterFiber;
	PerformanceAPI_BeginFiberSwitch_Func*		BeginFiberSwitch;
	PerformanceAPI_EndFiberSwitch_Func*			EndFiberSwitch;

} PerformanceAPI_Functions;

/**
 * Entry point for the PerformanceAPI when used through a DLL. You can get the actual function from the DLL through
 * GetProcAddress and then cast it to this function pointer. The name of the function exported from the DLL is "PerformanceAPI_GetAPI".
 *
 * A convenience function to find & call this function from the PerformanceAPI dll is provided in a separate header, PerformanceAPI_loader.h (PerformanceAPI_LoadFrom)
 * 
 * @param inVersion The version of the header that's used to request the function table. Always specify PERFORMANCEAPI_VERSION for this argument (defined at the top of this file). 
 *					Note: the version of the header and DLL must match exactly; if it doesn't an error will be returned.
 * @param outFunctions Pointer to a PerformanceAPI_Functions struct that will be filled with the correct function pointers to use the API
 *
 * @return 0 if there was an error (version mismatch), 1 on success
 */
typedef int (*PerformanceAPI_GetAPI_Func)(int inVersion, PerformanceAPI_Functions* outFunctions);

#ifdef __cplusplus
} // extern "C"
#endif
