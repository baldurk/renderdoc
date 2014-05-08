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


#include "core/core.h"
#include "common/string_utils.h"
#include "serialise/serialiser.h"
#include "replay/replay_driver.h"

#include <time.h>

#ifdef WIN32
#include "data/resource.h"
#endif

#include "crash_handler.h"

template<>
string ToStrHelper<false, RDCDriver>::Get(const RDCDriver &el)
{
	switch(el)
	{
		TOSTR_CASE_STRINGIZE(RDC_Unknown)
		TOSTR_CASE_STRINGIZE(RDC_D3D11)
		default: break;
	}
	
	char tostrBuf[256] = {0};
	StringFormat::snprintf(tostrBuf, 255, "RDCDriver<%d>", el);

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
	if(m_ExHandler)
		m_ExHandler->UnregisterMemoryRegion(this);

#ifdef CRASH_HANDLER_ENABLED
	m_ExHandler = new CrashHandler(m_ExHandler);
#endif
		
	if(m_ExHandler)
		m_ExHandler->RegisterMemoryRegion(this, sizeof(RenderDoc));
}

RenderDoc::RenderDoc()
{
	m_LogFile = L"";
	m_MarkerIndentLevel = 0;
	m_CurrentDriver = RDC_Unknown;

	m_Replay = false;

	m_Focus = false;
	m_Cap = false;

	m_ProgressPtr = NULL;
	
	m_ExHandler = NULL;
	
	m_RemoteServerThreadShutdown = false;
	m_RemoteClientThreadShutdown = false;
}

void RenderDoc::Initialise()
{
	Callstack::Init();

	Network::Init();

	m_RemoteIdent = 0;

	if(!IsReplayApp())
	{
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
		wstring capture_filename, logging_filename;

		const wchar_t *base = L"RenderDoc_app";
		if(IsReplayApp())
			base = L"RenderDoc_replay";
		
		FileIO::GetDefaultFiles(base, capture_filename, logging_filename, m_Target);

		if(m_LogFile.empty())
			SetLogFile(capture_filename.c_str());

		wstring existingLog = RDCGETLOGFILE();
		FileIO::CopyFileW(existingLog.c_str(), logging_filename.c_str(), true);
		RDCLOGFILE(logging_filename.c_str());
	}

	if(IsReplayApp())
		RDCLOG("RenderDoc v%hs (%hs) loaded in replay application", RENDERDOC_VERSION_STRING, GIT_COMMIT_HASH);
	else
		RDCLOG("RenderDoc v%hs (%hs) capturing application", RENDERDOC_VERSION_STRING, GIT_COMMIT_HASH);

	Keyboard::Init();
	
	m_ExHandler = NULL;

	{
		wstring curFile;
		FileIO::GetExecutableFilename(curFile);

		wstring f = strlower(curFile);

		// only create crash handler when we're not in renderdoccmd.exe (to prevent infinite loop as
		// the crash handler itself launches renderdoccmd.exe)
		if(f.find(L"renderdoccmd.exe") == wstring::npos)
		{
			RecreateCrashHandler();
		}
	}
}

RenderDoc::~RenderDoc()
{
	if(m_ExHandler)
	{
		m_ExHandler->UnregisterMemoryRegion(this);

		SAFE_DELETE(m_ExHandler);
	}

	for(size_t i=0; i < m_CapturePaths.size(); i++)
	{
		if(m_CaptureRetrieved[i])
		{
			RDCLOG("Removing remotely retrieved capture %ls", m_CapturePaths[i].c_str());
			FileIO::UnlinkFileW(m_CapturePaths[i].c_str());
		}
		else
		{
			RDCLOG("'Leaking' unretrieved capture %ls", m_CapturePaths[i].c_str());
		}
	}

	m_RemoteServerThreadShutdown = true;
	Threading::JoinThread(m_RemoteThread);
	Threading::CloseThread(m_RemoteThread);
	m_RemoteThread = 0;

	Network::Shutdown();

	RDCLOGDELETE();
}

void RenderDoc::Tick()
{
	static bool prev_f11 = false;
	static bool prev_f12 = false;

	bool cur_f11 = Keyboard::GetKeyState(Keyboard::eKey_F11);
	bool cur_f12 = Keyboard::GetKeyState(Keyboard::eKey_F12) || Keyboard::GetKeyState(Keyboard::eKey_PrtScrn); 

	if(!prev_f11 && cur_f11)
			FocusToggle();
	if(!prev_f12 && cur_f12)
			TriggerCapture();

	prev_f11 = cur_f11;
	prev_f12 = cur_f12;
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

	m_CurrentLogFile = StringFormat::WFmt(L"%ls_frame%u.rdc", m_LogFile.c_str(), frameNum);

	Serialiser *fileSerialiser = new Serialiser(m_CurrentLogFile.c_str(), Serialiser::WRITING, debugSerialiser);
	
	
	Serialiser *chunkSerialiser = new Serialiser(NULL, Serialiser::WRITING, debugSerialiser);

	{
		ScopedContext scope(chunkSerialiser, NULL, "Thumbnail", THUMBNAIL_DATA, false);

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
		ScopedContext scope(chunkSerialiser, NULL, "Capture Create Parameters", CREATE_PARAMS, false);

		chunkSerialiser->Serialise("DriverType", m_CurrentDriver);
		chunkSerialiser->SerialiseString("DriverName", m_CurrentDriverName);
		
		{
			ScopedContext scope(chunkSerialiser, NULL, "Driver Specific", DRIVER_INIT_PARAMS, false);

			params->m_pSerialiser = chunkSerialiser;
			params->m_State = WRITING;
			params->Serialise();
		}

		fileSerialiser->Insert(scope.Get(true));
	}

	SAFE_DELETE(chunkSerialiser);
	
	return fileSerialiser;
}

ReplayCreateStatus RenderDoc::FillInitParams(const wchar_t *logFile, RDCDriver &driverType, wstring &driverName, RDCInitParams *params)
{
	Serialiser ser(logFile, Serialiser::READING, true);

	if(ser.HasError())
	{
		RDCERR("Couldn't open '%ls'", logFile);

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
		int chunkType = ser.PushContext(NULL, 1, false);

		if(chunkType != THUMBNAIL_DATA)
		{
			RDCERR("Malformed logfile '%ls', first chunk isn't thumbnail data", logFile);
			return eReplayCreate_FileCorrupted;
		}

		ser.SkipCurrentChunk();

		ser.PopContext(NULL, 1);
	}

	{
		int chunkType = ser.PushContext(NULL, 1, false);

		if(chunkType != CREATE_PARAMS)
		{
			RDCERR("Malformed logfile '%ls', second chunk isn't create params", logFile);
			return eReplayCreate_FileCorrupted;
		}

		ser.Serialise("DriverType", driverType);
		ser.SerialiseString("DriverName", driverName);

		chunkType = ser.PushContext(NULL, 1, false);

		if(chunkType != DRIVER_INIT_PARAMS)
		{
			RDCERR("Malformed logfile '%ls', chunk doesn't contain driver init params", logFile);
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

void RenderDoc::RegisterReplayProvider(RDCDriver driver, const wchar_t *name, ReplayDriverProvider provider)
{
	if(HasReplayDriver(driver))
		RDCERR("Re-registering provider for %ls (was %ls)", name, m_DriverNames[driver].c_str());
	if(HasRemoteDriver(driver))
		RDCWARN("Registering local provider %ls for existing remote provider %ls", name, m_DriverNames[driver].c_str());
		
	m_DriverNames[driver] = name;
	m_ReplayDriverProviders[driver] = provider;
}

void RenderDoc::RegisterRemoteProvider(RDCDriver driver, const wchar_t *name, RemoteDriverProvider provider)
{
	if(HasRemoteDriver(driver))
		RDCERR("Re-registering provider for %ls (was %ls)", name, m_DriverNames[driver].c_str());
	if(HasReplayDriver(driver))
		RDCWARN("Registering remote provider %ls for existing local provider %ls", name, m_DriverNames[driver].c_str());
		
	m_DriverNames[driver] = name;
	m_RemoteDriverProviders[driver] = provider;
}

ReplayCreateStatus RenderDoc::CreateReplayDriver(RDCDriver driverType, const wchar_t *logfile, IReplayDriver **driver)
{
	if(driver == NULL) return eReplayCreate_InternalError;

	if(m_ReplayDriverProviders.find(driverType) != m_ReplayDriverProviders.end())
		return m_ReplayDriverProviders[driverType](logfile, driver);

	RDCERR("Unsupported replay driver requested: %d", driverType);
	return eReplayCreate_APIUnsupported;
}

ReplayCreateStatus RenderDoc::CreateRemoteDriver(RDCDriver driverType, const wchar_t *logfile, IRemoteDriver **driver)
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

void RenderDoc::GetCurrentDriver(RDCDriver &driver, wstring &name)
{
	driver = m_CurrentDriver;
	name = m_CurrentDriverName;
}

map<RDCDriver, wstring> RenderDoc::GetReplayDrivers()
{
	map<RDCDriver, wstring> ret;
	for(auto it=m_ReplayDriverProviders.begin(); it != m_ReplayDriverProviders.end(); ++it)
		ret[it->first] = m_DriverNames[it->first];
	return ret;
}

map<RDCDriver, wstring> RenderDoc::GetRemoteDrivers()
{
	map<RDCDriver, wstring> ret;

	for(auto it=m_RemoteDriverProviders.begin(); it != m_RemoteDriverProviders.end(); ++it)
		ret[it->first] = m_DriverNames[it->first];

	// replay drivers are remote drivers.
	for(auto it=m_ReplayDriverProviders.begin(); it != m_ReplayDriverProviders.end(); ++it)
		ret[it->first] = m_DriverNames[it->first];

	return ret;
}

void RenderDoc::SetCaptureOptions(const CaptureOptions *opts)
{
	m_Options = *opts;
}

void RenderDoc::SetLogFile(const wchar_t *logFile)
{
	m_LogFile = logFile;

	if(m_LogFile.substr(m_LogFile.length()-4) == L".rdc")
		m_LogFile = m_LogFile.substr(0, m_LogFile.length()-4);
}

void RenderDoc::SetProgress(LoadProgressSection section, float delta)
{
	if(m_ProgressPtr == NULL)
		return;

	float weights[NumSections];

	// must sum to 1.0
	weights[DebugManagerInit] = 0.4f;
	weights[FileInitialRead] = 0.6f;

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
	RDCLOG("Written to disk: %ls", m_CurrentLogFile.c_str());	

	{
		SCOPED_LOCK(m_CaptureLock);
		m_CapturePaths.push_back(m_CurrentLogFile);
		m_CaptureRetrieved.push_back(false);
	}
}
