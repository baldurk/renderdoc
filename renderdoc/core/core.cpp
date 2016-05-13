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


#include "core/core.h"
#include "serialise/string_utils.h"
#include "serialise/serialiser.h"
#include "replay/replay_driver.h"

#include <time.h>

#include <algorithm>

#include "data/version.h"
#include "crash_handler.h"
#include "hooks/hooks.h"

#include "stb/stb_image.h"
#include "common/dds_readwrite.h"

// not provided by tinyexr, just do by hand
bool is_exr_file(FILE *f)
{
	FileIO::fseek64(f, 0, SEEK_SET);

	const uint32_t openexr_magic = MAKE_FOURCC(0x76, 0x2f, 0x31, 0x01);

	uint32_t magic = 0;
	FileIO::fread(&magic, sizeof(magic), 1, f);

	FileIO::fseek64(f, 0, SEEK_SET);

	return magic == openexr_magic;
}

template<>
string ToStrHelper<false, RDCDriver>::Get(const RDCDriver &el)
{
	switch(el)
	{
		TOSTR_CASE_STRINGIZE(RDC_Unknown)
		TOSTR_CASE_STRINGIZE(RDC_D3D11)
		TOSTR_CASE_STRINGIZE(RDC_OpenGL)
		TOSTR_CASE_STRINGIZE(RDC_Mantle)
		TOSTR_CASE_STRINGIZE(RDC_D3D10)
		TOSTR_CASE_STRINGIZE(RDC_D3D9)
		default: break;
	}
	
	char tostrBuf[256] = {0};
	StringFormat::snprintf(tostrBuf, 255, "RDCDriver<%d>", el);

	return tostrBuf;
}

template<>
string ToStrHelper<false, RENDERDOC_InputButton>::Get(const RENDERDOC_InputButton &el)
{
	char alphanumericbuf[2] = { 'A', 0 };

	// enums map straight to ascii
	if( (el >= eRENDERDOC_Key_A && el <= eRENDERDOC_Key_Z) || (el >= eRENDERDOC_Key_0 && el <= eRENDERDOC_Key_9) )
	{
		alphanumericbuf[0] = (char)el;
		return alphanumericbuf;
	}

	switch(el)
	{
		case eRENDERDOC_Key_Divide:    return "/";
		case eRENDERDOC_Key_Multiply:  return "*";
		case eRENDERDOC_Key_Subtract:  return "-";
		case eRENDERDOC_Key_Plus:      return "+";

		case eRENDERDOC_Key_F1:        return "F1";
		case eRENDERDOC_Key_F2:        return "F2";
		case eRENDERDOC_Key_F3:        return "F3";
		case eRENDERDOC_Key_F4:        return "F4";
		case eRENDERDOC_Key_F5:        return "F5";
		case eRENDERDOC_Key_F6:        return "F6";
		case eRENDERDOC_Key_F7:        return "F7";
		case eRENDERDOC_Key_F8:        return "F8";
		case eRENDERDOC_Key_F9:        return "F9";
		case eRENDERDOC_Key_F10:       return "F10";
		case eRENDERDOC_Key_F11:       return "F11";
		case eRENDERDOC_Key_F12:       return "F12";

		case eRENDERDOC_Key_Home:      return "Home";
		case eRENDERDOC_Key_End:       return "End";
		case eRENDERDOC_Key_Insert:    return "Insert";
		case eRENDERDOC_Key_Delete:    return "Delete";
		case eRENDERDOC_Key_PageUp:    return "PageUp";
		case eRENDERDOC_Key_PageDn:    return "PageDn";

		case eRENDERDOC_Key_Backspace: return "Backspace";
		case eRENDERDOC_Key_Tab:       return "Tab";
		case eRENDERDOC_Key_PrtScrn:   return "PrtScrn";
		case eRENDERDOC_Key_Pause:     return "Pause";
		default: break;
	}
	
	char tostrBuf[256] = {0};
	StringFormat::snprintf(tostrBuf, 255, "RENDERDOC_InputButton<%d>", el);

	return tostrBuf;
}

RenderDoc *RenderDoc::m_Inst = NULL;

RenderDoc &RenderDoc::Inst()
{
	static RenderDoc realInst;
	RenderDoc::m_Inst = &realInst;
	return realInst;
}

void RenderDoc::RecreateCrashHandler()
{
	UnloadCrashHandler();

#ifdef CRASH_HANDLER_ENABLED
	m_ExHandler = new CrashHandler(m_ExHandler);
#endif
		
	if(m_ExHandler)
		m_ExHandler->RegisterMemoryRegion(this, sizeof(RenderDoc));
}

void RenderDoc::UnloadCrashHandler()
{
	if(m_ExHandler)
		m_ExHandler->UnregisterMemoryRegion(this);

	SAFE_DELETE(m_ExHandler);
}

RenderDoc::RenderDoc()
{
	m_LogFile = "";
	m_MarkerIndentLevel = 0;
	m_CurrentDriver = RDC_Unknown;

	m_CapturesActive = 0;

	m_Replay = false;

	m_Cap = false;

	m_FocusKeys.clear();
	m_FocusKeys.push_back(eRENDERDOC_Key_F11);

	m_CaptureKeys.clear();
	m_CaptureKeys.push_back(eRENDERDOC_Key_F12);
	m_CaptureKeys.push_back(eRENDERDOC_Key_PrtScrn);

	m_ProgressPtr = NULL;
	
	m_ExHandler = NULL;

	m_Overlay = eRENDERDOC_Overlay_Default;
	
	m_RemoteServerThreadShutdown = false;
	m_RemoteClientThreadShutdown = false;
}

void RenderDoc::Initialise()
{
	Callstack::Init();

	Network::Init();

	Threading::Init();

	m_RemoteIdent = 0;

	if(!IsReplayApp())
	{
		Process::ApplyEnvironmentModification();

		uint32_t port = RenderDoc_FirstCaptureNetworkPort;

		Network::Socket *sock = Network::CreateServerSocket("0.0.0.0", port&0xffff, 4);

		while(sock == NULL)
		{
			port++;
			if(port > RenderDoc_LastCaptureNetworkPort)
			{
				m_RemoteIdent = 0;
				break;
			}

			sock = Network::CreateServerSocket("0.0.0.0", port&0xffff, 4);
		}

		if(sock)
		{
			m_RemoteIdent = port;

			m_RemoteServerThreadShutdown = false;
			m_RemoteThread = Threading::CreateThread(RemoteAccessServerThread, (void *)sock);
		}
	}

	// set default capture log - useful for when hooks aren't setup
	// through the UI (and a log file isn't set manually)
	{
		string capture_filename;

		const char *base = "RenderDoc_app";
		if(IsReplayApp())
			base = "RenderDoc_replay";
		
		FileIO::GetDefaultFiles(base, capture_filename, m_LoggingFilename, m_Target);

		if(m_LogFile.empty())
			SetLogFile(capture_filename.c_str());

		string existingLog = RDCGETLOGFILE();
		FileIO::Copy(existingLog.c_str(), m_LoggingFilename.c_str(), true);
		RDCLOGFILE(m_LoggingFilename.c_str());
	}

	if(IsReplayApp())
		RDCLOG("RenderDoc v%s %s (%s) loaded in replay application",
		       RENDERDOC_VERSION_STRING, sizeof(uintptr_t) == sizeof(uint64_t) ? "x64" : "x86", GIT_COMMIT_HASH);
	else
		RDCLOG("RenderDoc v%s %s (%s) capturing application",
		       RENDERDOC_VERSION_STRING, sizeof(uintptr_t) == sizeof(uint64_t) ? "x64" : "x86", GIT_COMMIT_HASH);

	Keyboard::Init();
	
	m_ExHandler = NULL;

	{
		string curFile;
		FileIO::GetExecutableFilename(curFile);

		string f = strlower(curFile);

		// only create crash handler when we're not in renderdoccmd.exe (to prevent infinite loop as
		// the crash handler itself launches renderdoccmd.exe)
		if(f.find("renderdoccmd.exe") == string::npos)
		{
			RecreateCrashHandler();
		}
	}
}

RenderDoc::~RenderDoc()
{
	if(m_ExHandler)
	{
		UnloadCrashHandler();
	}

	for(auto it=m_ShutdownFunctions.begin(); it != m_ShutdownFunctions.end(); ++it)
		(*it)();

	for(size_t i=0; i < m_Captures.size(); i++)
	{
		if(m_Captures[i].retrieved)
		{
			RDCLOG("Removing remotely retrieved capture %s", m_Captures[i].path.c_str());
			FileIO::Delete(m_Captures[i].path.c_str());
		}
		else
		{
			RDCLOG("'Leaking' unretrieved capture %s", m_Captures[i].path.c_str());
		}
	}
	
	FileIO::Delete(m_LoggingFilename.c_str());

	if(m_RemoteThread)
	{
		m_RemoteServerThreadShutdown = true;
		// don't join, just close the thread, as we can't wait while in the middle of module unloading
		Threading::CloseThread(m_RemoteThread);
		m_RemoteThread = 0;
	}

	Network::Shutdown();

	Threading::Shutdown();

	FileIO::Delete(m_LoggingFilename.c_str());
}

void RenderDoc::Shutdown()
{
	if(m_ExHandler)
	{
		UnloadCrashHandler();
	}
	
	if(m_RemoteThread)
	{
		// explicitly wait for thread to shutdown, this call is not from module unloading and
		// we want to be sure everything is gone before we remove our module & hooks
		m_RemoteServerThreadShutdown = true;
		Threading::JoinThread(m_RemoteThread);
		Threading::CloseThread(m_RemoteThread);
		m_RemoteThread = 0;
	}
}

bool RenderDoc::MatchClosestWindow(void *&dev, void *&wnd)
{
	DeviceWnd dw(dev, wnd);

	// lower_bound and the DeviceWnd ordering (pointer compares, dev over wnd) means that if either
	// element in dw is NULL we can go forward from this iterator and find the first wildcardMatch
	// note that if dev is specified and wnd is NULL, this will actually point at the first
	// wildcardMatch already and we can use it immediately (since which window of multiple we
	// choose is undefined, so up to us). If dev is NULL there is no window ordering (since dev is
	// the primary sorting value) so we just iterate through the whole map. It should be small in
	// the majority of cases
	auto it = m_WindowFrameCapturers.lower_bound(dw);

	while(it != m_WindowFrameCapturers.end())
	{
		if(it->first.wildcardMatch(dw)) break;
		++it;
	}

	if(it != m_WindowFrameCapturers.end())
	{
		dev = it->first.dev;
		wnd = it->first.wnd;
		return true;
	}

	return false;
}

IFrameCapturer *RenderDoc::MatchFrameCapturer(void *dev, void *wnd)
{
	DeviceWnd dw(dev, wnd);

	// try and find the closest frame capture registered, and update
	// the values in dw to point to it precisely
	bool exactMatch = MatchClosestWindow(dw.dev, dw.wnd);

	if(!exactMatch)
	{
		// handle off-screen rendering where there are no device/window pairs in
		// m_WindowFrameCapturers, instead we use the first matching device frame capturer
		if(wnd == NULL)
		{
			auto defaultit = m_DeviceFrameCapturers.find(dev);
			if(defaultit == m_DeviceFrameCapturers.end() && !m_DeviceFrameCapturers.empty())
				defaultit = m_DeviceFrameCapturers.begin();

			if(defaultit != m_DeviceFrameCapturers.end())
				return defaultit->second;
		}

		RDCERR("Couldn't find matching frame capturer for device %p window %p", dev, wnd);
		return NULL;
	}

	auto it = m_WindowFrameCapturers.find(dw);

	if(it == m_WindowFrameCapturers.end())
	{
		RDCERR("Couldn't find frame capturer after exact match!");
		return NULL;
	}

	return it->second.FrameCapturer;
}

void RenderDoc::StartFrameCapture(void *dev, void *wnd)
{
	IFrameCapturer *frameCap = MatchFrameCapturer(dev, wnd);
	if(frameCap)
	{
		frameCap->StartFrameCapture(dev, wnd);
		m_CapturesActive++;
	}
}

void RenderDoc::SetActiveWindow(void *dev, void *wnd)
{
	DeviceWnd dw(dev, wnd);

	auto it = m_WindowFrameCapturers.find(dw);
	if(it == m_WindowFrameCapturers.end())
	{
		RDCERR("Couldn't find frame capturer for device %p window %p", dev, wnd);
		return;
	}

	m_ActiveWindow = dw;
}

bool RenderDoc::EndFrameCapture(void *dev, void *wnd)
{
	IFrameCapturer *frameCap = MatchFrameCapturer(dev, wnd);
	if(frameCap)
	{
		m_CapturesActive--;
		return frameCap->EndFrameCapture(dev, wnd);
	}
	return false;
}

bool RenderDoc::IsRemoteAccessConnected()
{
	SCOPED_LOCK(RenderDoc::Inst().m_SingleClientLock);
	return !RenderDoc::Inst().m_SingleClientName.empty();
}

void RenderDoc::Tick()
{
	static bool prev_focus = false;
	static bool prev_cap = false;

	bool cur_focus = false;
	for(size_t i=0; i < m_FocusKeys.size(); i++) cur_focus |= Keyboard::GetKeyState(m_FocusKeys[i]);

	bool cur_cap = false;
	for(size_t i=0; i < m_CaptureKeys.size(); i++) cur_cap |= Keyboard::GetKeyState(m_CaptureKeys[i]);

	if(!prev_focus && cur_focus)
	{
		m_Cap = false;

		// can only shift focus if we have multiple windows
		if(m_WindowFrameCapturers.size() > 1)
		{
			for(auto it = m_WindowFrameCapturers.begin(); it != m_WindowFrameCapturers.end(); ++it)
			{
				if(it->first == m_ActiveWindow)
				{
					auto nextit = it; ++nextit;

					if(nextit != m_WindowFrameCapturers.end())
						m_ActiveWindow = nextit->first;
					else
						m_ActiveWindow = m_WindowFrameCapturers.begin()->first;

					break;
				}
			}
		}
	}
	if(!prev_cap && cur_cap)
	{
		TriggerCapture();
	}

	prev_focus = cur_focus;
	prev_cap = cur_cap;
}

bool RenderDoc::ShouldTriggerCapture(uint32_t frameNumber)
{
	bool ret = m_Cap;

	m_Cap = false;

	set<uint32_t> frames;
	frames.swap(m_QueuedFrameCaptures);
	for(auto it=frames.begin(); it != frames.end(); ++it)
	{
		if(*it < frameNumber)
		{
			// discard, this frame is past.
		}
		else if((*it) - 1 == frameNumber)
		{
			// we want to capture the next frame
			ret = true;
		}
		else
		{
			// not hit this yet, keep it around
			m_QueuedFrameCaptures.insert(*it);
		}
	}

	return ret;
}

Serialiser *RenderDoc::OpenWriteSerialiser(uint32_t frameNum, RDCInitParams *params, void *thpixels, size_t thlen, uint32_t thwidth, uint32_t thheight)
{
	RDCASSERT(m_CurrentDriver != RDC_Unknown);

#if defined(RELEASE)
	const bool debugSerialiser = false;
#else
	const bool debugSerialiser = true;
#endif

	m_CurrentLogFile = StringFormat::Fmt("%s_frame%u.rdc", m_LogFile.c_str(), frameNum);

	Serialiser *fileSerialiser = new Serialiser(m_CurrentLogFile.c_str(), Serialiser::WRITING, debugSerialiser);
	
	
	Serialiser *chunkSerialiser = new Serialiser(NULL, Serialiser::WRITING, debugSerialiser);

	{
		ScopedContext scope(chunkSerialiser, "Thumbnail", THUMBNAIL_DATA, false);

		bool HasThumbnail = (thpixels != NULL && thwidth > 0 && thheight > 0);
		chunkSerialiser->Serialise("HasThumbnail", HasThumbnail);

		if(HasThumbnail)
		{
			byte *buf = (byte *)thpixels;
			chunkSerialiser->Serialise("ThumbWidth", thwidth);
			chunkSerialiser->Serialise("ThumbHeight", thheight);
			chunkSerialiser->SerialiseBuffer("ThumbnailPixels", buf, thlen);
		}

		fileSerialiser->Insert(scope.Get(true));
	}

	{
		ScopedContext scope(chunkSerialiser, "Capture Create Parameters", CREATE_PARAMS, false);

		chunkSerialiser->Serialise("DriverType", m_CurrentDriver);
		chunkSerialiser->SerialiseString("DriverName", m_CurrentDriverName);
		
		{
			ScopedContext driverparams(chunkSerialiser, "Driver Specific", DRIVER_INIT_PARAMS, false);

			params->m_pSerialiser = chunkSerialiser;
			params->m_State = WRITING;
			params->Serialise();
		}

		fileSerialiser->Insert(scope.Get(true));
	}

	SAFE_DELETE(chunkSerialiser);
	
	return fileSerialiser;
}

ReplayCreateStatus RenderDoc::FillInitParams(const char *logFile, RDCDriver &driverType, string &driverName, RDCInitParams *params)
{
	Serialiser ser(logFile, Serialiser::READING, true);

	if(ser.HasError())
	{
		FILE *f = FileIO::fopen(logFile, "rb");
		if(f)
		{
			int x = 0, y = 0, comp = 0;
			int ret = stbi_info_from_file(f, &x, &y, &comp);

			FileIO::fseek64(f, 0, SEEK_SET);

			if(is_dds_file(f))
				ret = x = y = comp = 1;
			
			if(is_exr_file(f))
				ret = x = y = comp = 1;
			
			FileIO::fclose(f);

			if(ret == 1 && x > 0 && y > 0 && comp > 0)
			{
				driverType = RDC_Image;
				driverName = "Image";
				return eReplayCreate_Success;
			}
		}

		RDCERR("Couldn't open '%s'", logFile);

		switch(ser.ErrorCode())
		{
			case Serialiser::eSerError_FileIO: return eReplayCreate_FileIOFailed;
			case Serialiser::eSerError_Corrupt: return eReplayCreate_FileCorrupted;
			case Serialiser::eSerError_UnsupportedVersion: return eReplayCreate_FileIncompatibleVersion;
			default: break;
		}

		return eReplayCreate_InternalError;
	}
	
	ser.Rewind();

	{
		int chunkType = ser.PushContext(NULL, NULL, 1, false);

		if(chunkType != THUMBNAIL_DATA)
		{
			RDCERR("Malformed logfile '%s', first chunk isn't thumbnail data", logFile);
			return eReplayCreate_FileCorrupted;
		}

		ser.SkipCurrentChunk();

		ser.PopContext(1);
	}

	{
		int chunkType = ser.PushContext(NULL, NULL, 1, false);

		if(chunkType != CREATE_PARAMS)
		{
			RDCERR("Malformed logfile '%s', second chunk isn't create params", logFile);
			return eReplayCreate_FileCorrupted;
		}

		ser.Serialise("DriverType", driverType);
		ser.SerialiseString("DriverName", driverName);

		chunkType = ser.PushContext(NULL, NULL, 1, false);

		if(chunkType != DRIVER_INIT_PARAMS)
		{
			RDCERR("Malformed logfile '%s', chunk doesn't contain driver init params", logFile);
			return eReplayCreate_FileCorrupted;
		}

		if(params)
		{
			params->m_State = READING;
			params->m_pSerialiser = &ser;
			return params->Serialise();
		}
	}

	// we can just throw away the serialiser, don't need to care about closing/popping contexts
	return eReplayCreate_Success;
}

bool RenderDoc::HasReplayDriver(RDCDriver driver) const
{
	return m_ReplayDriverProviders.find(driver) != m_ReplayDriverProviders.end();
}

bool RenderDoc::HasRemoteDriver(RDCDriver driver) const
{
	if(m_RemoteDriverProviders.find(driver) != m_RemoteDriverProviders.end())
		return true;

	return HasReplayDriver(driver);
}

void RenderDoc::RegisterReplayProvider(RDCDriver driver, const char *name, ReplayDriverProvider provider)
{
	if(HasReplayDriver(driver))
		RDCERR("Re-registering provider for %s (was %s)", name, m_DriverNames[driver].c_str());
	if(HasRemoteDriver(driver))
		RDCWARN("Registering local provider %s for existing remote provider %s", name, m_DriverNames[driver].c_str());
		
	m_DriverNames[driver] = name;
	m_ReplayDriverProviders[driver] = provider;
}

void RenderDoc::RegisterRemoteProvider(RDCDriver driver, const char *name, RemoteDriverProvider provider)
{
	if(HasRemoteDriver(driver))
		RDCERR("Re-registering provider for %s (was %s)", name, m_DriverNames[driver].c_str());
	if(HasReplayDriver(driver))
		RDCWARN("Registering remote provider %s for existing local provider %s", name, m_DriverNames[driver].c_str());
		
	m_DriverNames[driver] = name;
	m_RemoteDriverProviders[driver] = provider;
}

ReplayCreateStatus RenderDoc::CreateReplayDriver(RDCDriver driverType, const char *logfile, IReplayDriver **driver)
{
	if(driver == NULL) return eReplayCreate_InternalError;
	
	// allows passing RDC_Unknown as 'I don't care, give me a proxy driver of any type'
	// only valid if logfile is NULL and it will be used as a proxy, not to process a log
	if(driverType == RDC_Unknown && logfile == NULL && !m_ReplayDriverProviders.empty())
		return m_ReplayDriverProviders.begin()->second(logfile, driver);

	if(m_ReplayDriverProviders.find(driverType) != m_ReplayDriverProviders.end())
		return m_ReplayDriverProviders[driverType](logfile, driver);

	RDCERR("Unsupported replay driver requested: %d", driverType);
	return eReplayCreate_APIUnsupported;
}

ReplayCreateStatus RenderDoc::CreateRemoteDriver(RDCDriver driverType, const char *logfile, IRemoteDriver **driver)
{
	if(driver == NULL) return eReplayCreate_InternalError;

	if(m_RemoteDriverProviders.find(driverType) != m_RemoteDriverProviders.end())
		return m_RemoteDriverProviders[driverType](logfile, driver);

	// replay drivers are remote drivers, fall back and try them
	if(m_ReplayDriverProviders.find(driverType) != m_ReplayDriverProviders.end())
	{
		IReplayDriver *dr = NULL;
		auto status = m_ReplayDriverProviders[driverType](logfile, &dr);

		if(status == eReplayCreate_Success)
			*driver = (IRemoteDriver *)dr;
		else
			RDCASSERT(dr == NULL);

		return status;
	}

	RDCERR("Unsupported replay driver requested: %d", driverType);
	return eReplayCreate_APIUnsupported;
}

void RenderDoc::SetCurrentDriver(RDCDriver driver)
{
	if(!HasReplayDriver(driver) && !HasRemoteDriver(driver))
	{
		RDCFATAL("Trying to register unsupported driver!");
	}
	m_CurrentDriver = driver;
	m_CurrentDriverName = m_DriverNames[driver];
}

void RenderDoc::GetCurrentDriver(RDCDriver &driver, string &name)
{
	driver = m_CurrentDriver;
	name = m_CurrentDriverName;
}

map<RDCDriver, string> RenderDoc::GetReplayDrivers()
{
	map<RDCDriver, string> ret;
	for(auto it=m_ReplayDriverProviders.begin(); it != m_ReplayDriverProviders.end(); ++it)
		ret[it->first] = m_DriverNames[it->first];
	return ret;
}

map<RDCDriver, string> RenderDoc::GetRemoteDrivers()
{
	map<RDCDriver, string> ret;

	for(auto it=m_RemoteDriverProviders.begin(); it != m_RemoteDriverProviders.end(); ++it)
		ret[it->first] = m_DriverNames[it->first];

	// replay drivers are remote drivers.
	for(auto it=m_ReplayDriverProviders.begin(); it != m_ReplayDriverProviders.end(); ++it)
		ret[it->first] = m_DriverNames[it->first];

	return ret;
}

void RenderDoc::SetCaptureOptions(const CaptureOptions &opts)
{
	m_Options = opts;

	LibraryHooks::GetInstance().OptionsUpdated();
}

void RenderDoc::SetLogFile(const char *logFile)
{
	if (logFile == NULL || logFile[0] == '\0')
		return;

	m_LogFile = logFile;

	if(m_LogFile.length() > 4 && m_LogFile.substr(m_LogFile.length()-4) == ".rdc")
		m_LogFile = m_LogFile.substr(0, m_LogFile.length()-4);

	FileIO::CreateParentDirectory(m_LogFile);
}

void RenderDoc::SetProgress(LoadProgressSection section, float delta)
{
	if(m_ProgressPtr == NULL || section < 0 || section >= NumSections)
		return;

	float weights[NumSections];

	// must sum to 1.0
	weights[DebugManagerInit] = 0.1f;
	weights[FileInitialRead] = 0.75f;
	weights[FrameEventsRead] = 0.15f;

	float progress = 0.0f;
	for(int i=0; i < section; i++)
	{
		progress += weights[i];
	}

	progress += weights[section]*delta;

	*m_ProgressPtr = progress;
}

void RenderDoc::SuccessfullyWrittenLog()
{
	RDCLOG("Written to disk: %s", m_CurrentLogFile.c_str());	

	CaptureData cap(m_CurrentLogFile, Timing::GetUnixTimestamp());
	{
		SCOPED_LOCK(m_CaptureLock);
		m_Captures.push_back(cap);
	}
}

void RenderDoc::AddDeviceFrameCapturer(void *dev, IFrameCapturer *cap)
{
	if(dev == NULL || cap == NULL)
	{
		RDCERR("Invalid FrameCapturer combination: %#p / %#p", dev, cap);
		return;
	}

	m_DeviceFrameCapturers[dev] = cap;
}

void RenderDoc::RemoveDeviceFrameCapturer(void *dev)
{
	if(dev == NULL)
	{
		RDCERR("Invalid device pointer: %#p / %#p", dev);
		return;
	}

	m_DeviceFrameCapturers.erase(dev);
}

void RenderDoc::AddFrameCapturer(void *dev, void *wnd, IFrameCapturer *cap)
{
	if(dev == NULL || wnd == NULL || cap == NULL)
	{
		RDCERR("Invalid FrameCapturer combination: %#p / %#p", wnd, cap);
		return;
	}

	DeviceWnd dw(dev, wnd);
	
	auto it = m_WindowFrameCapturers.find(dw);
	if(it != m_WindowFrameCapturers.end())
	{
		if(it->second.FrameCapturer != cap)
			RDCERR("New different FrameCapturer being registered for known device/window pair!");

		it->second.RefCount++;
	}
	else
	{
		m_WindowFrameCapturers[dw].FrameCapturer = cap;
	}

	// the first one we see becomes the default
	if(m_ActiveWindow == DeviceWnd())
		m_ActiveWindow = dw;
}

void RenderDoc::RemoveFrameCapturer(void *dev, void *wnd)
{
	DeviceWnd dw(dev, wnd);
	
	auto it = m_WindowFrameCapturers.find(dw);
	if(it != m_WindowFrameCapturers.end())
	{
		it->second.RefCount--;

		if(it->second.RefCount <= 0)
		{
			if(m_ActiveWindow == dw)
			{
				if(m_WindowFrameCapturers.size() == 1)
				{
					m_ActiveWindow = DeviceWnd();
				}
				else
				{
					auto newactive = m_WindowFrameCapturers.begin();
					// active window could be the first in our list, move
					// to second (we know from above there are at least 2)
					if(m_ActiveWindow == newactive->first)
						newactive++;
					m_ActiveWindow = newactive->first;
				}
			}

			m_WindowFrameCapturers.erase(it);
		}
	}
	else
	{
		RDCERR("Removing FrameCapturer for unknown window!");
	}
}
