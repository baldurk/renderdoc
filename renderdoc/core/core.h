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
#include <utility>
using std::string;
using std::vector;
using std::map;
using std::pair;
using std::set;

class Serialiser;
class Chunk;

#include "api/app/renderdoc_app.h"
#include "api/replay/replay_enums.h"
#include "os/os_specific.h"
#include "common/threading.h"

// not provided by tinyexr, just do by hand
bool is_exr_file(FILE *f);

struct ICrashHandler
{
	virtual ~ICrashHandler() {}

	virtual void WriteMinidump() = 0;
	virtual void WriteMinidump(void *data) = 0;

	virtual void RegisterMemoryRegion(void *mem, size_t size) = 0;
	virtual void UnregisterMemoryRegion(void *mem) = 0;
};

struct IFrameCapturer
{
	virtual void StartFrameCapture(void *dev, void *wnd) = 0;
	virtual bool EndFrameCapture(void *dev, void *wnd) = 0;
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
	RDC_Mantle = 3,
	RDC_Reserved2 = 4,
	RDC_D3D10 = 5,
	RDC_D3D9 = 6,
	RDC_Image = 7,
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

struct CaptureData
{
	CaptureData(string p, uint64_t t) : path(p), timestamp(t), retrieved(false) {}

	string path;
	uint64_t timestamp;
	bool retrieved;
};

enum LoadProgressSection
{
	DebugManagerInit,
	FileInitialRead,
	NumSections,
};

class IRemoteDriver;
class IReplayDriver;

typedef ReplayCreateStatus (*RemoteDriverProvider)(const char *logfile, IRemoteDriver **driver);
typedef ReplayCreateStatus (*ReplayDriverProvider)(const char *logfile, IReplayDriver **driver);

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
		void SetLogFile(const char *logFile);
		const char *GetLogFile() const { return m_LogFile.c_str(); }
		
		const char *GetCurrentTarget() const { return m_Target.c_str(); }

		void Initialise();
		void Shutdown();

		void SetReplayApp(bool replay) { m_Replay = replay; }
		bool IsReplayApp() const { return m_Replay; }

		void BecomeReplayHost(volatile bool32 &killReplay);

		void SetCaptureOptions(const CaptureOptions *opts);
		const CaptureOptions &GetCaptureOptions() const { return m_Options; }

		void RecreateCrashHandler();
		void UnloadCrashHandler();
		ICrashHandler *GetCrashHandler() const { return m_ExHandler; }

		Serialiser *OpenWriteSerialiser(uint32_t frameNum, RDCInitParams *params, void *thpixels, size_t thlen, uint32_t thwidth, uint32_t thheight);
		void SuccessfullyWrittenLog();

		void AddChildProcess(uint32_t pid, uint32_t ident)
		{
			SCOPED_LOCK(m_ChildLock);
			m_Children.push_back( std::make_pair(pid, ident) );
		}
		vector< pair<uint32_t, uint32_t> > GetChildProcesses()
		{
			SCOPED_LOCK(m_ChildLock);
			return m_Children;
		}

		vector<CaptureData> GetCaptures()
		{
			SCOPED_LOCK(m_CaptureLock);
			return m_Captures;
		}

		void MarkCaptureRetrieved(uint32_t idx)
		{
			SCOPED_LOCK(m_CaptureLock);
			if(idx < m_Captures.size())
			{
				m_Captures[idx].retrieved = true;
			}
		}

		ReplayCreateStatus FillInitParams(const char *logfile, RDCDriver &driverType, string &driverName, RDCInitParams *params);
		
		void RegisterReplayProvider(RDCDriver driver, const char *name, ReplayDriverProvider provider);
		void RegisterRemoteProvider(RDCDriver driver, const char *name, RemoteDriverProvider provider);

		ReplayCreateStatus CreateReplayDriver(RDCDriver driverType, const char *logfile, IReplayDriver **driver);
		ReplayCreateStatus CreateRemoteDriver(RDCDriver driverType, const char *logfile, IRemoteDriver **driver);

		map<RDCDriver, string> GetReplayDrivers();
		map<RDCDriver, string> GetRemoteDrivers();

		bool HasReplayDriver(RDCDriver driver) const;
		bool HasRemoteDriver(RDCDriver driver) const;

		void SetCurrentDriver(RDCDriver driver);
		void GetCurrentDriver(RDCDriver &driver, string &name);

		uint32_t GetRemoteAccessIdent() const { return m_RemoteIdent; }

		void Tick();

		void AddFrameCapturer(void *dev, void *wnd, IFrameCapturer *cap);
		void RemoveFrameCapturer(void *dev, void *wnd);

		// add window-less frame capturers for use via users capturing
		// manually through the renderdoc API with NULL device/window handles
		void AddDefaultFrameCapturer(IFrameCapturer *cap);
		void RemoveDefaultFrameCapturer(IFrameCapturer *cap);
		
		void StartFrameCapture(void *dev, void *wnd);
		void SetActiveWindow(void *dev, void *wnd);
		bool EndFrameCapture(void *dev, void *wnd);

		bool IsActiveWindow(void *dev, void *wnd) { return dev == m_ActiveWindow.dev && wnd == m_ActiveWindow.wnd; }

		void TriggerCapture() { m_Cap = true; }

		uint32_t GetOverlayBits() { return m_Overlay; }
		void MaskOverlayBits(uint32_t And, uint32_t Or) { m_Overlay = (m_Overlay & And) | Or; }

		void QueueCapture(uint32_t frameNumber) { m_QueuedFrameCaptures.insert(frameNumber); }

		void SetFocusKeys(KeyButton *keys, int num)
		{
			m_FocusKeys.resize(num);
			for(int i=0; i < num && keys; i++)
				m_FocusKeys[i] = keys[i];
		}
		void SetCaptureKeys(KeyButton *keys, int num)
		{
			m_CaptureKeys.resize(num);
			for(int i=0; i < num && keys; i++)
				m_CaptureKeys[i] = keys[i];
		}

		const vector<KeyButton> &GetFocusKeys() { return m_FocusKeys; }
		const vector<KeyButton> &GetCaptureKeys() { return m_CaptureKeys; }

		bool ShouldTriggerCapture(uint32_t frameNumber);
	private:
		RenderDoc();
		~RenderDoc();

		static RenderDoc *m_Inst;

		bool m_Replay;

		bool m_Cap;

		vector<KeyButton> m_FocusKeys;
		vector<KeyButton> m_CaptureKeys;

		string m_LoggingFilename;

		string m_Target;
		string m_LogFile;
		string m_CurrentLogFile;
		CaptureOptions m_Options;
		uint32_t m_Overlay;

		set<uint32_t> m_QueuedFrameCaptures;

		uint32_t m_RemoteIdent;
		Threading::ThreadHandle m_RemoteThread;

		int32_t m_MarkerIndentLevel;
		RDCDriver m_CurrentDriver;
		string m_CurrentDriverName;

		float *m_ProgressPtr;

		Threading::CriticalSection m_CaptureLock;
		vector<CaptureData> m_Captures;
		
		Threading::CriticalSection m_ChildLock;
		vector< pair<uint32_t, uint32_t> > m_Children;

		map<RDCDriver, string> m_DriverNames;
		map<RDCDriver, ReplayDriverProvider> m_ReplayDriverProviders;
		map<RDCDriver, RemoteDriverProvider> m_RemoteDriverProviders;

		struct FrameCap
		{
			FrameCap() : FrameCapturer(NULL), RefCount(1) {}
			IFrameCapturer *FrameCapturer;
			int RefCount;
		};

		struct DeviceWnd
		{
			DeviceWnd() : dev(NULL), wnd(NULL) {}
			DeviceWnd(void *d, void *w) : dev(d), wnd(w) {}
			void *dev;
			void *wnd;

			bool operator ==(const DeviceWnd &o) const
			{
				return dev == o.dev && wnd == o.wnd;
			}
			bool operator <(const DeviceWnd &o) const
			{
				if(dev != o.dev) return dev < o.dev;
				return wnd < o.wnd;
			}
		};

		map<DeviceWnd, FrameCap> m_WindowFrameCapturers;
		DeviceWnd m_ActiveWindow;
		set<IFrameCapturer *> m_DefaultFrameCapturers;

		volatile bool m_RemoteServerThreadShutdown;
		volatile bool m_RemoteClientThreadShutdown;
		Threading::CriticalSection m_SingleClientLock;
		string m_SingleClientName;

		static void RemoteAccessServerThread(void *s);
		static void RemoteAccessClientThread(void *s);

		ICrashHandler *m_ExHandler;
};

struct DriverRegistration
{
	DriverRegistration(RDCDriver driver, const char *name, ReplayDriverProvider provider) { RenderDoc::Inst().RegisterReplayProvider(driver, name, provider); }
	DriverRegistration(RDCDriver driver, const char *name, RemoteDriverProvider provider) { RenderDoc::Inst().RegisterRemoteProvider(driver, name, provider); }
};
