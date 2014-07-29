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

#include <string>
#include <vector>
#include <map>
#include <set>
using std::wstring;
using std::vector;
using std::map;
using std::set;

class Serialiser;
class Chunk;

#include "replay/capture_options.h"
#include "replay/replay_enums.h"
#include "os/os_specific.h"
#include "common/threading.h"

struct ICrashHandler
{
	virtual ~ICrashHandler() {}

	virtual void WriteMinidump() = 0;
	virtual void WriteMinidump(void *data) = 0;

	virtual void RegisterMemoryRegion(void *mem, size_t size) = 0;
	virtual void UnregisterMemoryRegion(void *mem) = 0;
};

enum LogState
{
	READING = 0,
	EXECUTING,
	WRITING,
	WRITING_IDLE,
	WRITING_CAPFRAME,
};

enum SystemChunks
{
	// 0 is reserved as a 'null' chunk that is only for debug
	CREATE_PARAMS = 1,
	THUMBNAIL_DATA,
	DRIVER_INIT_PARAMS,
	INITIAL_CONTENTS,

	FIRST_CHUNK_ID,
};

enum RDCDriver
{
	RDC_Unknown = 0,
	RDC_D3D11 = 1,
	RDC_OpenGL = 2,
	RDC_Reserved1 = 3,
	RDC_Reserved2 = 4,
	RDC_D3D10 = 5,
	RDC_D3D9 = 6,
	RDC_Custom = 100000,
	RDC_Custom0 = RDC_Custom,
	RDC_Custom1,
	RDC_Custom2,
	RDC_Custom3,
	RDC_Custom4,
	RDC_Custom5,
	RDC_Custom6,
	RDC_Custom7,
	RDC_Custom8,
	RDC_Custom9,
};

namespace DXBC { class DXBCFile; }
namespace Callstack { class StackResolver; }

enum ReplayLogType
{
	eReplay_Full,
	eReplay_WithoutDraw,
	eReplay_OnlyDraw,
};

struct RDCInitParams
{
	RDCInitParams() { m_State = WRITING; m_pSerialiser = m_pDebugSerialiser = NULL; }
	virtual ~RDCInitParams() {}
	virtual ReplayCreateStatus Serialise() = 0;

	LogState m_State;
	Serialiser *m_pSerialiser;
	Serialiser *m_pDebugSerialiser;
};

enum LoadProgressSection
{
	DebugManagerInit,
	FileInitialRead,
	NumSections,
};

class IRemoteDriver;
class IReplayDriver;

typedef ReplayCreateStatus (*RemoteDriverProvider)(const wchar_t *logfile, IRemoteDriver **driver);
typedef ReplayCreateStatus (*ReplayDriverProvider)(const wchar_t *logfile, IReplayDriver **driver);

// this class mediates everything and owns any 'global' resources such as the crash handler.
//
// It acts as a central hub that registers any driver providers and can be asked to create one
// for a given logfile or type.
class RenderDoc
{
	public:
		static RenderDoc &Inst();

		void SetProgressPtr(float *progress) { m_ProgressPtr = progress; }
		void SetProgress(LoadProgressSection section, float delta);
		
		// set from outside of the device creation interface
		void SetLogFile(const wchar_t *logFile);
		const wchar_t *GetLogFile() const { return m_LogFile.c_str(); }
		
		const wchar_t *GetCurrentTarget() const { return m_Target.c_str(); }

		void Initialise();

		void SetReplayApp(bool replay) { m_Replay = replay; }
		bool IsReplayApp() const { return m_Replay; }

		void BecomeReplayHost(volatile bool &killReplay);

		void SetCaptureOptions(const CaptureOptions *opts);
		const CaptureOptions &GetCaptureOptions() const { return m_Options; }

		void RecreateCrashHandler();
		ICrashHandler *GetCrashHandler() const { return m_ExHandler; }

		Serialiser *OpenWriteSerialiser(uint32_t frameNum, RDCInitParams *params, void *thpixels, size_t thlen, uint32_t thwidth, uint32_t thheight);
		void SuccessfullyWrittenLog();

		vector<wstring> GetCaptures()
		{
			SCOPED_LOCK(m_CaptureLock);
			return m_CapturePaths;
		}

		void MarkCaptureRetrieved(uint32_t idx)
		{
			SCOPED_LOCK(m_CaptureLock);
			if(idx < m_CapturePaths.size())
			{
				m_CaptureRetrieved[idx] = true;
			}
		}

		ReplayCreateStatus FillInitParams(const wchar_t *logfile, RDCDriver &driverType, wstring &driverName, RDCInitParams *params);
		
		void RegisterReplayProvider(RDCDriver driver, const wchar_t *name, ReplayDriverProvider provider);
		void RegisterRemoteProvider(RDCDriver driver, const wchar_t *name, RemoteDriverProvider provider);

		ReplayCreateStatus CreateReplayDriver(RDCDriver driverType, const wchar_t *logfile, IReplayDriver **driver);
		ReplayCreateStatus CreateRemoteDriver(RDCDriver driverType, const wchar_t *logfile, IRemoteDriver **driver);

		map<RDCDriver, wstring> GetReplayDrivers();
		map<RDCDriver, wstring> GetRemoteDrivers();

		bool HasReplayDriver(RDCDriver driver) const;
		bool HasRemoteDriver(RDCDriver driver) const;

		void SetCurrentDriver(RDCDriver driver);
		void GetCurrentDriver(RDCDriver &driver, wstring &name);

		uint32_t GetRemoteAccessIdent() const { return m_RemoteIdent; }

		void Tick();

		void FocusToggle() { m_Focus = true; m_Cap = false; }
		void TriggerCapture() { m_Cap = true; }

		void QueueCapture(uint32_t frameNumber) { m_QueuedFrameCaptures.insert(frameNumber); }

		bool ShouldFocusToggle() { bool ret = m_Focus; m_Focus = false; return ret; }
		bool ShouldTriggerCapture(uint32_t frameNumber);
	private:
		RenderDoc();
		~RenderDoc();

		static RenderDoc *m_Inst;

		bool m_Replay;

		bool m_Focus;
		bool m_Cap;

		wstring m_Target;
		wstring m_LogFile;
		wstring m_CurrentLogFile;
		CaptureOptions m_Options;

		set<uint32_t> m_QueuedFrameCaptures;

		uint32_t m_RemoteIdent;
		Threading::ThreadHandle m_RemoteThread;

		int32_t m_MarkerIndentLevel;
		RDCDriver m_CurrentDriver;
		wstring m_CurrentDriverName;

		float *m_ProgressPtr;

		Threading::CriticalSection m_CaptureLock;
		vector<wstring> m_CapturePaths;
		vector<bool> m_CaptureRetrieved;

		map<RDCDriver, wstring> m_DriverNames;
		map<RDCDriver, ReplayDriverProvider> m_ReplayDriverProviders;
		map<RDCDriver, RemoteDriverProvider> m_RemoteDriverProviders;

		volatile bool m_RemoteServerThreadShutdown;
		volatile bool m_RemoteClientThreadShutdown;
		Threading::CriticalSection m_SingleClientLock;
		wstring m_SingleClientName;

		static void RemoteAccessServerThread(void *s);
		static void RemoteAccessClientThread(void *s);

		ICrashHandler *m_ExHandler;
};

struct DriverRegistration
{
	DriverRegistration(RDCDriver driver, const wchar_t *name, ReplayDriverProvider provider) { RenderDoc::Inst().RegisterReplayProvider(driver, name, provider); }
	DriverRegistration(RDCDriver driver, const wchar_t *name, RemoteDriverProvider provider) { RenderDoc::Inst().RegisterRemoteProvider(driver, name, provider); }
};
