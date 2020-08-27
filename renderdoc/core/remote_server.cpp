/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2020 Baldur Karlsson
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

#include "remote_server.h"
#include <utility>
#include "android/android.h"
#include "api/replay/renderdoc_replay.h"
#include "api/replay/version.h"
#include "common/threading.h"
#include "core/core.h"
#include "core/settings.h"
#include "os/os_specific.h"
#include "replay/replay_controller.h"
#include "serialise/rdcfile.h"
#include "serialise/serialiser.h"
#include "strings/string_utils.h"
#include "replay_proxy.h"

RDOC_CONFIG(uint32_t, RemoteServer_TimeoutMS, 5000,
            "Timeout in milliseconds for remote server operations.");

RDOC_DEBUG_CONFIG(bool, RemoteServer_DebugLogging, false,
                  "Output a verbose logging file in the system's temporary folder containing the "
                  "traffic to and from the remote server.");

static const uint32_t RemoteServerProtocolVersion =
    uint32_t(RENDERDOC_VERSION_MAJOR * 1000) + RENDERDOC_VERSION_MINOR;

enum RemoteServerPacket
{
  eRemoteServer_Noop = 1,
  eRemoteServer_Handshake,
  eRemoteServer_VersionMismatch,
  eRemoteServer_Busy,

  eRemoteServer_Ping,
  eRemoteServer_RemoteDriverList,
  eRemoteServer_TakeOwnershipCapture,
  eRemoteServer_CopyCaptureToRemote,
  eRemoteServer_CopyCaptureFromRemote,
  eRemoteServer_OpenLog,
  eRemoteServer_LogOpenProgress,
  eRemoteServer_LogOpened,
  eRemoteServer_HasCallstacks,
  eRemoteServer_InitResolver,
  eRemoteServer_ResolverProgress,
  eRemoteServer_GetResolve,
  eRemoteServer_CloseLog,
  eRemoteServer_HomeDir,
  eRemoteServer_ListDir,
  eRemoteServer_ExecuteAndInject,
  eRemoteServer_ShutdownServer,
  eRemoteServer_GetDriverName,
  eRemoteServer_GetSectionCount,
  eRemoteServer_FindSectionByName,
  eRemoteServer_FindSectionByType,
  eRemoteServer_GetSectionProperties,
  eRemoteServer_GetSectionContents,
  eRemoteServer_WriteSection,
  eRemoteServer_GetAvailableGPUs,
  eRemoteServer_RemoteServerCount,
};

DECLARE_REFLECTION_ENUM(RemoteServerPacket);

RDCCOMPILE_ASSERT((int)eRemoteServer_RemoteServerCount < (int)eReplayProxy_First,
                  "Remote server and Replay Proxy packets overlap");

template <>
rdcstr DoStringise(const RemoteServerPacket &el)
{
  BEGIN_ENUM_STRINGISE(RemoteServerPacket);
  {
    STRINGISE_ENUM_NAMED(eRemoteServer_Noop, "No-op");
    STRINGISE_ENUM_NAMED(eRemoteServer_Handshake, "Handshake");
    STRINGISE_ENUM_NAMED(eRemoteServer_VersionMismatch, "VersionMismatch");
    STRINGISE_ENUM_NAMED(eRemoteServer_Busy, "Busy");

    STRINGISE_ENUM_NAMED(eRemoteServer_Ping, "Ping");
    STRINGISE_ENUM_NAMED(eRemoteServer_RemoteDriverList, "RemoteDriverList");
    STRINGISE_ENUM_NAMED(eRemoteServer_TakeOwnershipCapture, "TakeOwnershipCapture");
    STRINGISE_ENUM_NAMED(eRemoteServer_CopyCaptureToRemote, "CopyCaptureToRemote");
    STRINGISE_ENUM_NAMED(eRemoteServer_CopyCaptureFromRemote, "CopyCaptureFromRemote");
    STRINGISE_ENUM_NAMED(eRemoteServer_OpenLog, "OpenLog");
    STRINGISE_ENUM_NAMED(eRemoteServer_LogOpenProgress, "LogOpenProgress");
    STRINGISE_ENUM_NAMED(eRemoteServer_LogOpened, "LogOpened");
    STRINGISE_ENUM_NAMED(eRemoteServer_HasCallstacks, "HasCallstacks");
    STRINGISE_ENUM_NAMED(eRemoteServer_InitResolver, "InitResolver");
    STRINGISE_ENUM_NAMED(eRemoteServer_ResolverProgress, "ResolverProgress");
    STRINGISE_ENUM_NAMED(eRemoteServer_GetResolve, "GetResolve");
    STRINGISE_ENUM_NAMED(eRemoteServer_CloseLog, "CloseLog");
    STRINGISE_ENUM_NAMED(eRemoteServer_HomeDir, "HomeDir");
    STRINGISE_ENUM_NAMED(eRemoteServer_ListDir, "ListDir");
    STRINGISE_ENUM_NAMED(eRemoteServer_ExecuteAndInject, "ExecuteAndInject");
    STRINGISE_ENUM_NAMED(eRemoteServer_ShutdownServer, "ShutdownServer");
    STRINGISE_ENUM_NAMED(eRemoteServer_GetDriverName, "GetDriverName");
    STRINGISE_ENUM_NAMED(eRemoteServer_GetSectionCount, "GetSectionCount");
    STRINGISE_ENUM_NAMED(eRemoteServer_FindSectionByName, "FindSectionByName");
    STRINGISE_ENUM_NAMED(eRemoteServer_FindSectionByType, "FindSectionByType");
    STRINGISE_ENUM_NAMED(eRemoteServer_GetSectionProperties, "GetSectionProperties");
    STRINGISE_ENUM_NAMED(eRemoteServer_GetSectionContents, "GetSectionContents");
    STRINGISE_ENUM_NAMED(eRemoteServer_WriteSection, "WriteSection");
    STRINGISE_ENUM_NAMED(eRemoteServer_GetAvailableGPUs, "GetAvailableGPUs");
    STRINGISE_ENUM_NAMED(eRemoteServer_RemoteServerCount, "RemoteServerCount");
  }
  END_ENUM_STRINGISE();
}

rdcstr GetRemoteServerChunkName(uint32_t idx)
{
  if(idx < eRemoteServer_RemoteServerCount)
    return ToStr((RemoteServerPacket)idx);

  return ToStr((ReplayProxyPacket)idx);
}

#define WRITE_DATA_SCOPE() WriteSerialiser &ser = writer;
#define READ_DATA_SCOPE() ReadSerialiser &ser = reader;

struct ClientThread
{
  ClientThread()
      : socket(NULL), allowExecution(false), killThread(false), killServer(false), thread(0)
  {
  }

  Network::Socket *socket;

  bool allowExecution;
  bool killThread;
  bool killServer;

  Threading::ThreadHandle thread;
};

struct ActiveClient
{
  Threading::CriticalSection lock;
  ClientThread *active = NULL;
};

static bool HandleHandshakeClient(ActiveClient &activeClient, ClientThread *threadData)
{
  uint32_t ip = threadData->socket->GetRemoteIP();

  uint32_t version = 0;

  bool activeConnectionDesired = false;
  bool activeConnectionEstablished = false;

  {
    ReadSerialiser ser(new StreamReader(threadData->socket, Ownership::Nothing), Ownership::Stream);

    // this thread just handles receiving the handshake and sending a busy signal without blocking
    // the server thread
    RemoteServerPacket type = ser.ReadChunk<RemoteServerPacket>();

    if(ser.IsErrored() || type != eRemoteServer_Handshake)
    {
      RDCWARN("Didn't receive proper handshake");
      return activeConnectionEstablished;
    }

    SERIALISE_ELEMENT(version);
    SERIALISE_ELEMENT(activeConnectionDesired);

    ser.EndChunk();
  }

  {
    WriteSerialiser ser(new StreamWriter(threadData->socket, Ownership::Nothing), Ownership::Stream);

    ser.SetStreamingMode(true);

    if(version != RemoteServerProtocolVersion)
    {
      RDCLOG("Connection using protocol %u, but we are running %u", version,
             RemoteServerProtocolVersion);

      {
        SCOPED_SERIALISE_CHUNK(eRemoteServer_VersionMismatch);
      }
    }
    else
    {
      bool busy = false;

      {
        SCOPED_LOCK(activeClient.lock);
        busy = activeClient.active != NULL;

        // if we're not busy, and the connection wants to be active, promote it.
        if(!busy && activeConnectionDesired)
        {
          RDCLOG("Promoting connection from %u.%u.%u.%u to active.", Network::GetIPOctet(ip, 0),
                 Network::GetIPOctet(ip, 1), Network::GetIPOctet(ip, 2), Network::GetIPOctet(ip, 3));
          activeConnectionEstablished = true;
          activeClient.active = threadData;
        }
      }

      // if we were busy, return that status
      if(busy)
      {
        RDCLOG("Returning busy signal for connection from %u.%u.%u.%u.", Network::GetIPOctet(ip, 0),
               Network::GetIPOctet(ip, 1), Network::GetIPOctet(ip, 2), Network::GetIPOctet(ip, 3));

        SCOPED_SERIALISE_CHUNK(eRemoteServer_Busy);
      }
      // otherwise we return a successful handshake. For active connections this begins the active
      // thread, for passive connection checks this is enough
      else
      {
        RDCLOG("Returning OK signal for connection from %u.%u.%u.%u.", Network::GetIPOctet(ip, 0),
               Network::GetIPOctet(ip, 1), Network::GetIPOctet(ip, 2), Network::GetIPOctet(ip, 3));

        SCOPED_SERIALISE_CHUNK(eRemoteServer_Handshake);
      }
    }
  }

  // return whether or not an active connection was established
  return activeConnectionEstablished;
}

static void ActiveRemoteClientThread(ClientThread *threadData,
                                     RENDERDOC_PreviewWindowCallback previewWindow)
{
  Threading::SetCurrentThreadName("ActiveRemoteClientThread");

  Network::Socket *&client = threadData->socket;

  client->SetTimeout(RemoteServer_TimeoutMS());

  uint32_t ip = client->GetRemoteIP();

  rdcarray<rdcstr> tempFiles;
  IRemoteDriver *remoteDriver = NULL;
  IReplayDriver *replayDriver = NULL;
  ReplayProxy *proxy = NULL;
  RDCFile *rdc = NULL;
  Callstack::StackResolver *resolver = NULL;

  FileIO::LogFileHandle *debugLog = NULL;

  WriteSerialiser writer(new StreamWriter(client, Ownership::Nothing), Ownership::Stream);
  ReadSerialiser reader(new StreamReader(client, Ownership::Nothing), Ownership::Stream);

  if(RemoteServer_DebugLogging())
  {
    reader.ConfigureStructuredExport(&GetRemoteServerChunkName, false, 0, 1.0);
    writer.ConfigureStructuredExport(&GetRemoteServerChunkName, false, 0, 1.0);

    rdcstr filename = FileIO::GetTempFolderFilename() + "/RenderDoc/RemoteServer_Server.log";

    RDCLOG("Logging remote server work to '%s'", filename.c_str());

    // truncate the log
    debugLog = FileIO::logfile_open(filename.c_str());
    FileIO::logfile_close(debugLog, filename.c_str());
    debugLog = FileIO::logfile_open(filename.c_str());

    reader.EnableDumping(debugLog);
    writer.EnableDumping(debugLog);
  }

  writer.SetStreamingMode(true);
  reader.SetStreamingMode(true);

  while(client)
  {
    if(client && !client->Connected())
      break;

    if(threadData->killThread)
      break;

    // this will block until a packet comes in.
    RemoteServerPacket type = reader.ReadChunk<RemoteServerPacket>();

    if(reader.IsErrored() || writer.IsErrored())
      break;

    if(client == NULL)
      continue;

    if(type == eRemoteServer_Ping)
    {
      reader.EndChunk();

      if(proxy)
        proxy->RefreshPreviewWindow();

      // insert a dummy line into our logcat so we can keep track of our progress
      Android::TickDeviceLogcat();

      WRITE_DATA_SCOPE();
      SCOPED_SERIALISE_CHUNK(eRemoteServer_Ping);
    }
    else if(type == eRemoteServer_RemoteDriverList)
    {
      reader.EndChunk();

      std::map<RDCDriver, rdcstr> drivers = RenderDoc::Inst().GetRemoteDrivers();
      uint32_t count = (uint32_t)drivers.size();

      WRITE_DATA_SCOPE();
      SCOPED_SERIALISE_CHUNK(eRemoteServer_RemoteDriverList);
      SERIALISE_ELEMENT(count);

      for(auto it = drivers.begin(); it != drivers.end(); ++it)
      {
        RDCDriver driverType = it->first;
        const rdcstr &driverName = it->second;

        SERIALISE_ELEMENT(driverType);
        SERIALISE_ELEMENT(driverName);
      }
    }
    else if(type == eRemoteServer_HomeDir)
    {
      reader.EndChunk();

      rdcstr home = FileIO::GetHomeFolderFilename();

      {
        WRITE_DATA_SCOPE();
        SCOPED_SERIALISE_CHUNK(eRemoteServer_HomeDir);
        SERIALISE_ELEMENT(home);
      }
    }
    else if(type == eRemoteServer_ListDir)
    {
      rdcstr path;

      {
        READ_DATA_SCOPE();
        SERIALISE_ELEMENT(path);
      }

      reader.EndChunk();

      rdcarray<PathEntry> files;
      FileIO::GetFilesInDirectory(path.c_str(), files);

      {
        WRITE_DATA_SCOPE();
        SCOPED_SERIALISE_CHUNK(eRemoteServer_ListDir);
        SERIALISE_ELEMENT(files);
      }
    }
    else if(type == eRemoteServer_CopyCaptureFromRemote)
    {
      rdcstr path;

      {
        READ_DATA_SCOPE();
        SERIALISE_ELEMENT(path);
      }

      reader.EndChunk();

      {
        WRITE_DATA_SCOPE();
        SCOPED_SERIALISE_CHUNK(eRemoteServer_CopyCaptureFromRemote);

        StreamReader fileStream(FileIO::fopen(path.c_str(), "rb"));
        ser.SerialiseStream(path, fileStream);
      }
    }
    else if(type == eRemoteServer_CopyCaptureToRemote)
    {
      rdcstr path;
      rdcstr dummy, dummy2;
      FileIO::GetDefaultFiles("remotecopy", path, dummy, dummy2);

      RDCLOG("Copying file to local path '%s'.", path.c_str());

      FileIO::CreateParentDirectory(path);

      {
        READ_DATA_SCOPE();

        StreamWriter streamWriter(FileIO::fopen(path.c_str(), "wb"), Ownership::Stream);

        ser.SerialiseStream(path.c_str(), streamWriter, NULL);
      }

      reader.EndChunk();

      if(reader.IsErrored())
      {
        FileIO::Delete(path.c_str());

        RDCERR("Network error receiving file");
        break;
      }

      RDCLOG("File received.");

      tempFiles.push_back(path);

      {
        WRITE_DATA_SCOPE();
        SCOPED_SERIALISE_CHUNK(eRemoteServer_CopyCaptureToRemote);
        SERIALISE_ELEMENT(path);
      }
    }
    else if(type == eRemoteServer_TakeOwnershipCapture)
    {
      rdcstr path;

      {
        READ_DATA_SCOPE();
        SERIALISE_ELEMENT(path);
      }

      reader.EndChunk();

      RDCLOG("Taking ownership of '%s'.", path.c_str());

      tempFiles.push_back(path);
    }
    else if(type == eRemoteServer_GetAvailableGPUs)
    {
      reader.EndChunk();

      rdcarray<GPUDevice> gpus = RenderDoc::Inst().GetAvailableGPUs();

      {
        WRITE_DATA_SCOPE();
        SCOPED_SERIALISE_CHUNK(eRemoteServer_GetAvailableGPUs);
        SERIALISE_ELEMENT(gpus);
      }
    }
    else if(type == eRemoteServer_ShutdownServer)
    {
      reader.EndChunk();

      RDCLOG("Requested to shut down.");

      threadData->killServer = true;
      threadData->killThread = true;

      {
        WRITE_DATA_SCOPE();
        SCOPED_SERIALISE_CHUNK(eRemoteServer_ShutdownServer);
      }
    }
    else if(type == eRemoteServer_OpenLog)
    {
      rdcstr path;
      ReplayOptions opts;

      {
        READ_DATA_SCOPE();
        SERIALISE_ELEMENT(path);
        SERIALISE_ELEMENT(opts);
      }

      reader.EndChunk();

      RDCASSERT(remoteDriver == NULL && proxy == NULL && rdc == NULL);
      ReplayStatus status = ReplayStatus::InternalError;

      rdc = new RDCFile();
      rdc->Open(path.c_str());

      if(rdc->ErrorCode() != ContainerError::NoError)
      {
        RDCERR("Failed to open '%s': %d", path.c_str(), rdc->ErrorCode());

        switch(rdc->ErrorCode())
        {
          case ContainerError::FileNotFound: status = ReplayStatus::FileNotFound; break;
          case ContainerError::FileIO: status = ReplayStatus::FileIOFailed; break;
          case ContainerError::Corrupt: status = ReplayStatus::FileCorrupted; break;
          case ContainerError::UnsupportedVersion:
            status = ReplayStatus::FileIncompatibleVersion;
            break;
          default: break;
        }
      }
      else
      {
        if(RenderDoc::Inst().HasRemoteDriver(rdc->GetDriver()))
        {
          bool kill = false;
          float progress = 0.0f;

          RenderDoc::Inst().SetProgressCallback<LoadProgress>([&progress](float p) { progress = p; });

          Threading::ThreadHandle ticker = Threading::CreateThread([&writer, &kill, &progress]() {
            while(!kill)
            {
              {
                WRITE_DATA_SCOPE();
                SCOPED_SERIALISE_CHUNK(eRemoteServer_LogOpenProgress);
                SERIALISE_ELEMENT(progress);
              }
              Threading::Sleep(100);
            }
          });

          // if we have a replay driver, try to create it so we can display a local preview e.g.
          if(RenderDoc::Inst().HasReplayDriver(rdc->GetDriver()))
          {
            status = RenderDoc::Inst().CreateReplayDriver(rdc, opts, &replayDriver);
            if(replayDriver)
              remoteDriver = replayDriver;
          }
          else
          {
            status = RenderDoc::Inst().CreateRemoteDriver(rdc, opts, &remoteDriver);
          }

          if(status != ReplayStatus::Succeeded || remoteDriver == NULL)
          {
            RDCERR("Failed to create remote driver for driver '%s'", rdc->GetDriverName().c_str());
          }
          else
          {
            status = remoteDriver->ReadLogInitialisation(rdc, false);

            if(status != ReplayStatus::Succeeded)
            {
              RDCERR("Failed to initialise remote driver.");

              remoteDriver->Shutdown();
              remoteDriver = NULL;
            }
          }

          RenderDoc::Inst().SetProgressCallback<LoadProgress>(RENDERDOC_ProgressCallback());

          kill = true;
          Threading::JoinThread(ticker);
          Threading::CloseThread(ticker);

          if(status == ReplayStatus::Succeeded && remoteDriver)
          {
            proxy = new ReplayProxy(reader, writer, remoteDriver, replayDriver, previewWindow);
          }
        }
        else
        {
          RDCERR("File needs driver for '%s' which isn't supported!", rdc->GetDriverName().c_str());

          status = ReplayStatus::APIUnsupported;
        }
      }

      {
        WRITE_DATA_SCOPE();
        SCOPED_SERIALISE_CHUNK(eRemoteServer_LogOpened);
        SERIALISE_ELEMENT(status);
      }
    }
    else if(type == eRemoteServer_HasCallstacks)
    {
      reader.EndChunk();

      bool HasCallstacks = rdc && rdc->SectionIndex(SectionType::ResolveDatabase) >= 0;

      {
        WRITE_DATA_SCOPE();
        SCOPED_SERIALISE_CHUNK(eRemoteServer_HasCallstacks);
        SERIALISE_ELEMENT(HasCallstacks);
      }
    }
    else if(type == eRemoteServer_InitResolver)
    {
      reader.EndChunk();

      bool success = false;

      int sectionIndex = rdc ? rdc->SectionIndex(SectionType::ResolveDatabase) : -1;

      SAFE_DELETE(resolver);
      if(sectionIndex >= 0)
      {
        StreamReader *sectionReader = rdc->ReadSection(sectionIndex);

        bytebuf buf;
        buf.resize((size_t)sectionReader->GetSize());
        success = sectionReader->Read(buf.data(), sectionReader->GetSize());

        delete sectionReader;

        if(success)
        {
          float progress = 0.0f;

          Threading::ThreadHandle ticker = Threading::CreateThread([&writer, &resolver, &progress]() {
            while(!resolver)
            {
              {
                WRITE_DATA_SCOPE();
                SCOPED_SERIALISE_CHUNK(eRemoteServer_ResolverProgress);
                SERIALISE_ELEMENT(progress);
              }
              Threading::Sleep(100);
            }
          });

          resolver = Callstack::MakeResolver(false, buf.data(), buf.size(),
                                             [&progress](float p) { progress = p; });

          Threading::JoinThread(ticker);
          Threading::CloseThread(ticker);
        }
        else
        {
          RDCERR("Failed to read resolve database.");
        }
      }

      {
        WRITE_DATA_SCOPE();
        SCOPED_SERIALISE_CHUNK(eRemoteServer_InitResolver);
        SERIALISE_ELEMENT(success);
      }
    }
    else if(type == eRemoteServer_GetResolve)
    {
      rdcarray<uint64_t> StackAddresses;

      {
        READ_DATA_SCOPE();
        SERIALISE_ELEMENT(StackAddresses);
      }

      reader.EndChunk();

      rdcarray<rdcstr> StackFrames;

      if(resolver)
      {
        StackFrames.reserve(StackAddresses.size());
        for(uint64_t frame : StackAddresses)
        {
          Callstack::AddressDetails info = resolver->GetAddr(frame);
          StackFrames.push_back(info.formattedString());
        }
      }
      else
      {
        StackFrames = {""};
      }

      {
        WRITE_DATA_SCOPE();
        SCOPED_SERIALISE_CHUNK(eRemoteServer_GetResolve);
        SERIALISE_ELEMENT(StackFrames);
      }
    }
    else if(type == eRemoteServer_GetDriverName)
    {
      reader.EndChunk();

      rdcstr driver = rdc ? rdc->GetDriverName() : "";
      {
        WRITE_DATA_SCOPE();
        SCOPED_SERIALISE_CHUNK(eRemoteServer_GetDriverName);
        SERIALISE_ELEMENT(driver);
      }
    }
    else if(type == eRemoteServer_GetSectionCount)
    {
      reader.EndChunk();

      int count = rdc ? rdc->NumSections() : 0;

      {
        WRITE_DATA_SCOPE();
        SCOPED_SERIALISE_CHUNK(eRemoteServer_GetSectionCount);
        SERIALISE_ELEMENT(count);
      }
    }
    else if(type == eRemoteServer_FindSectionByName)
    {
      rdcstr name;

      {
        READ_DATA_SCOPE();
        SERIALISE_ELEMENT(name);
      }

      reader.EndChunk();

      int index = rdc ? rdc->SectionIndex(name.c_str()) : -1;

      {
        WRITE_DATA_SCOPE();
        SCOPED_SERIALISE_CHUNK(eRemoteServer_FindSectionByName);
        SERIALISE_ELEMENT(index);
      }
    }
    else if(type == eRemoteServer_FindSectionByType)
    {
      SectionType sectionType;

      {
        READ_DATA_SCOPE();
        SERIALISE_ELEMENT(sectionType);
      }

      reader.EndChunk();

      int index = rdc ? rdc->SectionIndex(sectionType) : -1;

      {
        WRITE_DATA_SCOPE();
        SCOPED_SERIALISE_CHUNK(eRemoteServer_FindSectionByType);
        SERIALISE_ELEMENT(index);
      }
    }
    else if(type == eRemoteServer_GetSectionProperties)
    {
      int index = -1;

      {
        READ_DATA_SCOPE();
        SERIALISE_ELEMENT(index);
      }

      reader.EndChunk();

      SectionProperties props;
      if(rdc && index >= 0 && index < rdc->NumSections())
        props = rdc->GetSectionProperties(index);

      {
        WRITE_DATA_SCOPE();
        SCOPED_SERIALISE_CHUNK(eRemoteServer_GetSectionProperties);
        SERIALISE_ELEMENT(props);
      }
    }
    else if(type == eRemoteServer_GetSectionContents)
    {
      int index = -1;

      {
        READ_DATA_SCOPE();
        SERIALISE_ELEMENT(index);
      }

      reader.EndChunk();

      bytebuf contents;

      if(rdc && index >= 0 && index < rdc->NumSections())
      {
        StreamReader *sectionReader = rdc->ReadSection(index);

        contents.resize((size_t)sectionReader->GetSize());
        bool success = sectionReader->Read(contents.data(), sectionReader->GetSize());

        if(!success)
          contents.clear();

        delete sectionReader;
      }

      {
        WRITE_DATA_SCOPE();
        SCOPED_SERIALISE_CHUNK(eRemoteServer_GetSectionContents);
        SERIALISE_ELEMENT(contents);
      }
    }
    else if(type == eRemoteServer_WriteSection)
    {
      SectionProperties props;
      bytebuf contents;

      {
        READ_DATA_SCOPE();
        SERIALISE_ELEMENT(props);
        SERIALISE_ELEMENT(contents);
      }

      reader.EndChunk();

      bool success = false;

      if(rdc)
      {
        StreamWriter *sectionWriter = rdc->WriteSection(props);

        if(sectionWriter)
        {
          sectionWriter->Write(contents.data(), contents.size());
          delete sectionWriter;

          success = true;
        }
      }

      {
        WRITE_DATA_SCOPE();
        SCOPED_SERIALISE_CHUNK(eRemoteServer_WriteSection);
        SERIALISE_ELEMENT(success);
      }
    }
    else if(type == eRemoteServer_CloseLog)
    {
      reader.EndChunk();

      SAFE_DELETE(proxy);

      if(remoteDriver)
        remoteDriver->Shutdown();
      remoteDriver = NULL;
      replayDriver = NULL;

      SAFE_DELETE(rdc);
      SAFE_DELETE(resolver);
    }
    else if(type == eRemoteServer_ExecuteAndInject)
    {
      rdcstr app, workingDir, cmdLine, logfile;
      CaptureOptions opts;
      rdcarray<EnvironmentModification> env;

      {
        READ_DATA_SCOPE();
        SERIALISE_ELEMENT(app);
        SERIALISE_ELEMENT(workingDir);
        SERIALISE_ELEMENT(cmdLine);
        SERIALISE_ELEMENT(opts);
        SERIALISE_ELEMENT(env);
      }

      reader.EndChunk();

      ExecuteResult ret = {};

      if(threadData->allowExecution)
      {
        rdcpair<ReplayStatus, uint32_t> status = Process::LaunchAndInjectIntoProcess(
            app.c_str(), workingDir.c_str(), cmdLine.c_str(), env, "", opts, false);

        ret.status = status.first;
        ret.ident = status.second;
      }
      else
      {
        RDCWARN("Requested to execute program - disallowing based on configuration");
      }

      {
        WRITE_DATA_SCOPE();
        SCOPED_SERIALISE_CHUNK(eRemoteServer_ExecuteAndInject);
        SERIALISE_ELEMENT(ret);
      }
    }
    else if((int)type >= eReplayProxy_First && proxy)
    {
      bool ok = proxy->Tick(type);

      if(!ok)
        break;

      continue;
    }
  }

  FileIO::logfile_close(debugLog, NULL);

  SAFE_DELETE(proxy);

  if(remoteDriver)
    remoteDriver->Shutdown();
  remoteDriver = NULL;
  replayDriver = NULL;
  SAFE_DELETE(rdc);
  SAFE_DELETE(resolver);

  for(size_t i = 0; i < tempFiles.size(); i++)
  {
    FileIO::Delete(tempFiles[i].c_str());
  }

  RDCLOG("Closing active connection from %u.%u.%u.%u.", Network::GetIPOctet(ip, 0),
         Network::GetIPOctet(ip, 1), Network::GetIPOctet(ip, 2), Network::GetIPOctet(ip, 3));

  RDCLOG("Ready for new active connection...");

  SAFE_DELETE(client);
}

void RenderDoc::BecomeRemoteServer(const char *listenhost, uint16_t port,
                                   std::function<bool()> killReplay,
                                   RENDERDOC_PreviewWindowCallback previewWindow)
{
  Network::Socket *sock = Network::CreateServerSocket(listenhost, port, 1);

  if(sock == NULL)
    return;

  rdcarray<rdcpair<uint32_t, uint32_t> > listenRanges;
  bool allowExecution = true;

  FILE *f = FileIO::fopen(FileIO::GetAppFolderFilename("remoteserver.conf").c_str(), "r");

  rdcstr configFile;

  if(f)
  {
    FileIO::fseek64(f, 0, SEEK_END);
    configFile.resize((size_t)FileIO::ftell64(f));
    FileIO::fseek64(f, 0, SEEK_SET);

    FileIO::fread(configFile.data(), 1, configFile.size(), f);

    FileIO::fclose(f);
  }

  rdcarray<rdcstr> lines;
  split(configFile, lines, '\n');

  for(rdcstr &line : lines)
  {
    line.trim();

    if(line == "")
      continue;

    // skip comments
    if(line[0] == '#')
      continue;

    if(line.substr(0, sizeof("whitelist") - 1) == "whitelist")
    {
      uint32_t ip = 0, mask = 0;

      // CIDR notation
      bool found = Network::ParseIPRangeCIDR(line.c_str() + sizeof("whitelist"), ip, mask);

      if(found)
      {
        listenRanges.push_back(make_rdcpair(ip, mask));
        continue;
      }
      else
      {
        RDCLOG("Couldn't parse IP range from: %s", line.c_str() + sizeof("whitelist"));
      }

      continue;
    }
    else if(line.substr(0, sizeof("noexec") - 1) == "noexec")
    {
      allowExecution = false;

      continue;
    }

    RDCLOG("Malformed line '%s'. See documentation for file format.", line.c_str());
  }

  if(listenRanges.empty())
  {
    RDCLOG("No whitelist IP ranges configured - using default private IP ranges.");
    RDCLOG(
        "Create a config file remoteserver.conf in ~/.renderdoc or %%APPDATA%%/renderdoc to "
        "narrow "
        "this down or accept connections from more ranges.");

    listenRanges.push_back(make_rdcpair(Network::MakeIP(10, 0, 0, 0), 0xff000000));
    listenRanges.push_back(make_rdcpair(Network::MakeIP(172, 16, 0, 0), 0xfff00000));
    listenRanges.push_back(make_rdcpair(Network::MakeIP(192, 168, 0, 0), 0xffff0000));
  }

  RDCLOG("Allowing connections from:");

  for(size_t i = 0; i < listenRanges.size(); i++)
  {
    uint32_t ip = listenRanges[i].first;
    uint32_t mask = listenRanges[i].second;

    RDCLOG("%u.%u.%u.%u / %u.%u.%u.%u", Network::GetIPOctet(ip, 0), Network::GetIPOctet(ip, 1),
           Network::GetIPOctet(ip, 2), Network::GetIPOctet(ip, 3), Network::GetIPOctet(mask, 0),
           Network::GetIPOctet(mask, 1), Network::GetIPOctet(mask, 2), Network::GetIPOctet(mask, 3));
  }

  if(allowExecution)
    RDCLOG("Allowing execution commands");
  else
    RDCLOG("Blocking execution commands");

  RDCLOG("Replay host ready for requests...");

  ActiveClient activeClientData;

  rdcarray<ClientThread *> clients;

  while(!killReplay())
  {
    Network::Socket *client = sock->AcceptClient(0);

    {
      SCOPED_LOCK(activeClientData.lock);
      if(activeClientData.active && activeClientData.active->killServer)
        break;
    }

    // reap any dead client threads
    for(size_t i = 0; i < clients.size(); i++)
    {
      if(clients[i]->socket == NULL)
      {
        {
          SCOPED_LOCK(activeClientData.lock);
          if(activeClientData.active == clients[i])
            activeClientData.active = NULL;
        }

        Threading::JoinThread(clients[i]->thread);
        Threading::CloseThread(clients[i]->thread);
        delete clients[i];
        clients.erase(i);
        break;
      }
    }

    if(client == NULL)
    {
      if(!sock->Connected())
      {
        RDCERR("Error in accept - shutting down server");

        SAFE_DELETE(sock);
        return;
      }

      Threading::Sleep(5);

      continue;
    }

    uint32_t ip = client->GetRemoteIP();

    RDCLOG("Connection received from %u.%u.%u.%u.", Network::GetIPOctet(ip, 0),
           Network::GetIPOctet(ip, 1), Network::GetIPOctet(ip, 2), Network::GetIPOctet(ip, 3));

    bool valid = false;

    // always allow connections from localhost
    valid = Network::MatchIPMask(ip, Network::MakeIP(127, 0, 0, 1), ~0U);

    for(size_t i = 0; i < listenRanges.size(); i++)
    {
      if(Network::MatchIPMask(ip, listenRanges[i].first, listenRanges[i].second))
      {
        valid = true;
        break;
      }
    }

    if(!valid)
    {
      RDCLOG("Doesn't match any listen range, closing connection.");
      SAFE_DELETE(client);
      continue;
    }

    RDCLOG("Processing connection");

    ClientThread *clientThread = new ClientThread();
    clientThread->socket = client;
    clientThread->allowExecution = allowExecution;
    clientThread->thread =
        Threading::CreateThread([&activeClientData, clientThread, previewWindow]() {
          if(HandleHandshakeClient(activeClientData, clientThread))
          {
            ActiveRemoteClientThread(clientThread, previewWindow);
          }
          else
          {
            SAFE_DELETE(clientThread->socket);
          }
        });

    clients.push_back(clientThread);
  }

  {
    SCOPED_LOCK(activeClientData.lock);
    if(activeClientData.active)
      activeClientData.active->killThread = true;
    activeClientData.active = NULL;
  }

  // shut down client threads
  for(size_t i = 0; i < clients.size(); i++)
  {
    Threading::JoinThread(clients[i]->thread);
    Threading::CloseThread(clients[i]->thread);
    delete clients[i];
  }

  SAFE_DELETE(sock);
}

extern "C" RENDERDOC_API ReplayStatus RENDERDOC_CC
RENDERDOC_CreateRemoteServerConnection(const char *URL, IRemoteServer **rend)
{
  rdcstr host = "localhost";
  if(URL != NULL && URL[0] != '\0')
    host = URL;

  rdcstr deviceID = host;

  IDeviceProtocolHandler *protocol = RenderDoc::Inst().GetDeviceProtocol(deviceID);

  uint16_t port = RenderDoc_RemoteServerPort;

  if(protocol)
  {
    deviceID = protocol->GetDeviceID(deviceID);
    host = protocol->RemapHostname(deviceID);
    if(host.empty())
      return ReplayStatus::NetworkIOFailed;

    port = protocol->RemapPort(deviceID, port);
  }

  Network::Socket *sock = Network::CreateClientSocket(host.c_str(), port, 750);

  if(sock == NULL)
    return ReplayStatus::NetworkIOFailed;

  uint32_t version = RemoteServerProtocolVersion;

  sock->SetTimeout(RemoteServer_TimeoutMS());

  bool activeConnection = (rend != NULL);

  {
    WriteSerialiser ser(new StreamWriter(sock, Ownership::Nothing), Ownership::Stream);

    ser.SetStreamingMode(true);

    SCOPED_SERIALISE_CHUNK(eRemoteServer_Handshake);
    SERIALISE_ELEMENT(version);
    SERIALISE_ELEMENT(activeConnection);
  }

  if(!sock->Connected())
    return ReplayStatus::NetworkIOFailed;

  {
    ReadSerialiser ser(new StreamReader(sock, Ownership::Nothing), Ownership::Stream);

    RemoteServerPacket type = ser.ReadChunk<RemoteServerPacket>();

    ser.EndChunk();

    if(type == eRemoteServer_Busy)
    {
      SAFE_DELETE(sock);
      return ReplayStatus::NetworkRemoteBusy;
    }

    if(type == eRemoteServer_VersionMismatch)
    {
      SAFE_DELETE(sock);
      return ReplayStatus::NetworkVersionMismatch;
    }

    if(ser.IsErrored() || type != eRemoteServer_Handshake)
    {
      RDCWARN("Didn't get proper handshake");
      SAFE_DELETE(sock);
      return ReplayStatus::NetworkIOFailed;
    }
  }

  if(rend == NULL)
    return ReplayStatus::Succeeded;

  if(protocol)
    *rend = protocol->CreateRemoteServer(sock, deviceID);
  else
    *rend = new RemoteServer(sock, deviceID);

  return ReplayStatus::Succeeded;
}

extern "C" RENDERDOC_API ReplayStatus RENDERDOC_CC RENDERDOC_CheckRemoteServerConnection(const char *URL)
{
  return RENDERDOC_CreateRemoteServerConnection(URL, NULL);
}

#undef WRITE_DATA_SCOPE
#undef READ_DATA_SCOPE
#define WRITE_DATA_SCOPE() WriteSerialiser &ser = *writer;
#define READ_DATA_SCOPE() ReadSerialiser &ser = *reader;

RemoteServer::RemoteServer(Network::Socket *sock, const rdcstr &deviceID)
    : m_Socket(sock), m_deviceID(deviceID)
{
  reader = new ReadSerialiser(new StreamReader(sock, Ownership::Nothing), Ownership::Stream);
  writer = new WriteSerialiser(new StreamWriter(sock, Ownership::Nothing), Ownership::Stream);

  if(RemoteServer_DebugLogging())
  {
    reader->ConfigureStructuredExport(&GetRemoteServerChunkName, false, 0, 1.0);
    writer->ConfigureStructuredExport(&GetRemoteServerChunkName, false, 0, 1.0);

    rdcstr filename = FileIO::GetTempFolderFilename() + "/RenderDoc/RemoteServer_Client.log";

    RDCLOG("Logging remote server work to '%s'", filename.c_str());

    // truncate the log
    debugLog = FileIO::logfile_open(filename.c_str());
    FileIO::logfile_close(debugLog, filename.c_str());
    debugLog = FileIO::logfile_open(filename.c_str());

    reader->EnableDumping(debugLog);
    writer->EnableDumping(debugLog);
  }
  else
  {
    debugLog = NULL;
  }

  writer->SetStreamingMode(true);
  reader->SetStreamingMode(true);

  std::map<RDCDriver, rdcstr> m = RenderDoc::Inst().GetReplayDrivers();

  m_Proxies.reserve(m.size());
  for(auto it = m.begin(); it != m.end(); ++it)
    m_Proxies.push_back({it->first, it->second});
}

RemoteServer::~RemoteServer()
{
  FileIO::logfile_close(debugLog, NULL);
  SAFE_DELETE(writer);
  SAFE_DELETE(reader);
  SAFE_DELETE(m_Socket);
}

void RemoteServer::ShutdownConnection()
{
  delete this;
}

void RemoteServer::ShutdownServerAndConnection()
{
  {
    WRITE_DATA_SCOPE();
    SCOPED_SERIALISE_CHUNK(eRemoteServer_ShutdownServer);
  }

  {
    READ_DATA_SCOPE();
    RemoteServerPacket type = ser.ReadChunk<RemoteServerPacket>();
    ser.EndChunk();

    RDCASSERT(type == eRemoteServer_ShutdownServer);
  }

  delete this;
}

bool RemoteServer::Connected()
{
  return m_Socket != NULL && m_Socket->Connected();
}

bool RemoteServer::Ping()
{
  if(!Connected())
    return false;

  {
    WRITE_DATA_SCOPE();
    SCOPED_SERIALISE_CHUNK(eRemoteServer_Ping);
  }

  RemoteServerPacket type;

  {
    READ_DATA_SCOPE();
    type = ser.ReadChunk<RemoteServerPacket>();
    ser.EndChunk();
  }

  return type == eRemoteServer_Ping;
}

rdcarray<rdcstr> RemoteServer::LocalProxies()
{
  rdcarray<rdcstr> out;

  m_Proxies.reserve(m_Proxies.size());

  size_t i = 0;
  for(auto it = m_Proxies.begin(); it != m_Proxies.end(); ++it, ++i)
    out.push_back(it->second);

  return out;
}

rdcarray<rdcstr> RemoteServer::RemoteSupportedReplays()
{
  rdcarray<rdcstr> out;

  {
    WRITE_DATA_SCOPE();
    SCOPED_SERIALISE_CHUNK(eRemoteServer_RemoteDriverList);
  }

  {
    READ_DATA_SCOPE();

    RemoteServerPacket type = ser.ReadChunk<RemoteServerPacket>();

    if(type == eRemoteServer_RemoteDriverList)
    {
      uint32_t count = 0;
      SERIALISE_ELEMENT(count);

      out.reserve(count);

      for(uint32_t i = 0; i < count; i++)
      {
        RDCDriver driverType = RDCDriver::Unknown;
        rdcstr driverName = "";

        SERIALISE_ELEMENT(driverType);
        SERIALISE_ELEMENT(driverName);

        out.push_back(driverName);
      }
    }
    else
    {
      RDCERR("Unexpected response to remote driver list request");
    }

    ser.EndChunk();
  }

  return out;
}

rdcstr RemoteServer::GetHomeFolder()
{
  {
    WRITE_DATA_SCOPE();
    SCOPED_SERIALISE_CHUNK(eRemoteServer_HomeDir);
  }

  rdcstr home;

  {
    READ_DATA_SCOPE();

    RemoteServerPacket type = ser.ReadChunk<RemoteServerPacket>();

    if(type == eRemoteServer_HomeDir)
    {
      SERIALISE_ELEMENT(home);
    }
    else
    {
      RDCERR("Unexpected response to home folder request");
    }

    ser.EndChunk();
  }

  return home;
}

rdcarray<PathEntry> RemoteServer::ListFolder(const char *path)
{
  {
    WRITE_DATA_SCOPE();
    SCOPED_SERIALISE_CHUNK(eRemoteServer_ListDir);
    SERIALISE_ELEMENT(path);
  }

  rdcarray<PathEntry> files;

  {
    READ_DATA_SCOPE();

    RemoteServerPacket type = ser.ReadChunk<RemoteServerPacket>();

    if(type == eRemoteServer_ListDir)
    {
      SERIALISE_ELEMENT(files);
    }
    else
    {
      RDCERR("Unexpected response to list directory request");
      files.resize(1);
      files[0].filename = path;
      files[0].flags = PathProperty::ErrorUnknown;
    }

    ser.EndChunk();
  }

  return files;
}

ExecuteResult RemoteServer::ExecuteAndInject(const char *a, const char *w, const char *c,
                                             const rdcarray<EnvironmentModification> &env,
                                             const CaptureOptions &opts)
{
  rdcstr app = a && a[0] ? a : "";
  rdcstr workingDir = w && w[0] ? w : "";
  rdcstr cmdline = c && c[0] ? c : "";

  {
    WRITE_DATA_SCOPE();
    SCOPED_SERIALISE_CHUNK(eRemoteServer_ExecuteAndInject);
    SERIALISE_ELEMENT(app);
    SERIALISE_ELEMENT(workingDir);
    SERIALISE_ELEMENT(cmdline);
    SERIALISE_ELEMENT(opts);
    SERIALISE_ELEMENT(env);
  }

  ExecuteResult ret = {};

  {
    READ_DATA_SCOPE();
    RemoteServerPacket type = ser.ReadChunk<RemoteServerPacket>();

    if(type == eRemoteServer_ExecuteAndInject)
    {
      SERIALISE_ELEMENT(ret);
    }
    else
    {
      RDCERR("Unexpected response to execute and inject request");
    }

    ser.EndChunk();
  }

  return ret;
}

void RemoteServer::CopyCaptureFromRemote(const char *remotepath, const char *localpath,
                                         RENDERDOC_ProgressCallback progress)
{
  rdcstr path = remotepath;

  {
    WRITE_DATA_SCOPE();
    SCOPED_SERIALISE_CHUNK(eRemoteServer_CopyCaptureFromRemote);
    SERIALISE_ELEMENT(path);
  }

  {
    READ_DATA_SCOPE();
    RemoteServerPacket type = ser.ReadChunk<RemoteServerPacket>();

    if(type == eRemoteServer_CopyCaptureFromRemote)
    {
      StreamWriter streamWriter(FileIO::fopen(localpath, "wb"), Ownership::Stream);

      ser.SerialiseStream(localpath, streamWriter, progress);

      if(ser.IsErrored())
      {
        RDCERR("Network error receiving file");
        return;
      }
    }
    else
    {
      RDCERR("Unexpected response to capture copy request");
    }

    ser.EndChunk();
  }
}

rdcstr RemoteServer::CopyCaptureToRemote(const char *filename, RENDERDOC_ProgressCallback progress)
{
  FILE *fileHandle = FileIO::fopen(filename, "rb");

  if(!fileHandle)
  {
    RDCERR("Can't open file '%s'", filename);
    return "";
  }

  {
    WRITE_DATA_SCOPE();
    SCOPED_SERIALISE_CHUNK(eRemoteServer_CopyCaptureToRemote);

    // this will take ownership of and close the file
    StreamReader fileStream(fileHandle);
    ser.SerialiseStream(filename, fileStream, progress);
  }

  rdcstr path;

  {
    READ_DATA_SCOPE();
    RemoteServerPacket type = ser.ReadChunk<RemoteServerPacket>();

    if(type == eRemoteServer_CopyCaptureToRemote)
    {
      SERIALISE_ELEMENT(path);
    }
    else
    {
      RDCERR("Unexpected response to capture copy request");
    }

    ser.EndChunk();
  }

  return path;
}

void RemoteServer::TakeOwnershipCapture(const char *filename)
{
  rdcstr path = filename;

  {
    WRITE_DATA_SCOPE();
    SCOPED_SERIALISE_CHUNK(eRemoteServer_TakeOwnershipCapture);
    SERIALISE_ELEMENT(path);
  }
}

rdcpair<ReplayStatus, IReplayController *> RemoteServer::OpenCapture(
    uint32_t proxyid, const char *filename, const ReplayOptions &opts,
    RENDERDOC_ProgressCallback progress)
{
  rdcpair<ReplayStatus, IReplayController *> ret;
  ret.first = ReplayStatus::InternalError;
  ret.second = NULL;

  if(proxyid != ~0U && proxyid >= m_Proxies.size())
  {
    RDCERR("Invalid proxy driver id %d specified for remote renderer", proxyid);
    return ret;
  }

  RDCLOG("Opening capture remotely");

  LogReplayOptions(opts);

  // if the proxy id is ~0U, then we just don't care so let RenderDoc pick the most
  // appropriate supported proxy for the current platform.
  RDCDriver proxydrivertype = proxyid == ~0U ? RDCDriver::Unknown : m_Proxies[proxyid].first;

  {
    WRITE_DATA_SCOPE();
    SCOPED_SERIALISE_CHUNK(eRemoteServer_OpenLog);
    SERIALISE_ELEMENT(filename);
    SERIALISE_ELEMENT(opts);
  }

  RemoteServerPacket type = eRemoteServer_Noop;
  while(!reader->IsErrored())
  {
    READ_DATA_SCOPE();
    type = ser.ReadChunk<RemoteServerPacket>();

    if(reader->IsErrored() || type != eRemoteServer_LogOpenProgress)
      break;

    float progressValue = 0.0f;

    SERIALISE_ELEMENT(progressValue);

    ser.EndChunk();

    if(progress)
      progress(progressValue);
  }

  RDCLOG("Capture open complete");

  if(reader->IsErrored() || type != eRemoteServer_LogOpened)
  {
    RDCERR("Error opening capture");
    ret.first = ReplayStatus::NetworkIOFailed;
    return ret;
  }

  ReplayStatus status = ReplayStatus::Succeeded;
  {
    READ_DATA_SCOPE();
    SERIALISE_ELEMENT(status);
    ser.EndChunk();
  }

  if(progress)
    progress(1.0f);

  if(status != ReplayStatus::Succeeded)
  {
    RDCERR("Capture open failed: %s", ToStr(status).c_str());
    ret.first = status;
    return ret;
  }

  RDCLOG("Capture ready on replay host");

  IReplayDriver *proxyDriver = NULL;
  status = RenderDoc::Inst().CreateProxyReplayDriver(proxydrivertype, &proxyDriver);

  if(status != ReplayStatus::Succeeded || !proxyDriver)
  {
    RDCERR("Creating proxy driver failed: %s", ToStr(status).c_str());
    if(proxyDriver)
      proxyDriver->Shutdown();
    ret.first = status;
    return ret;
  }

  ReplayController *rend = new ReplayController();

  ReplayProxy *proxy = new ReplayProxy(*reader, *writer, proxyDriver);
  status = rend->SetDevice(proxy);

  if(status != ReplayStatus::Succeeded)
  {
    SAFE_DELETE(rend);
    ret.first = status;
    return ret;
  }

  // ReplayController takes ownership of the ProxySerialiser (as IReplayDriver)
  // and it cleans itself up in Shutdown.

  RDCLOG("Remote capture open complete & proxy ready");

  ret.first = ReplayStatus::Succeeded;
  ret.second = rend;
  return ret;
}

void RemoteServer::CloseCapture(IReplayController *rend)
{
  {
    WRITE_DATA_SCOPE();
    SCOPED_SERIALISE_CHUNK(eRemoteServer_CloseLog);
  }

  rend->Shutdown();
}

rdcstr RemoteServer::DriverName()
{
  if(!Connected())
    return "";

  {
    WRITE_DATA_SCOPE();
    SCOPED_SERIALISE_CHUNK(eRemoteServer_GetDriverName);
  }

  rdcstr driverName = "";

  {
    READ_DATA_SCOPE();
    RemoteServerPacket type = ser.ReadChunk<RemoteServerPacket>();

    if(type == eRemoteServer_GetDriverName)
    {
      SERIALISE_ELEMENT(driverName);
    }
    else
    {
      RDCERR("Unexpected response to GetDriverName");
    }

    ser.EndChunk();
  }

  return driverName;
}

rdcarray<GPUDevice> RemoteServer::GetAvailableGPUs()
{
  if(!Connected())
    return {};

  {
    WRITE_DATA_SCOPE();
    SCOPED_SERIALISE_CHUNK(eRemoteServer_GetAvailableGPUs);
  }

  rdcarray<GPUDevice> gpus;

  {
    READ_DATA_SCOPE();
    RemoteServerPacket type = ser.ReadChunk<RemoteServerPacket>();

    if(type == eRemoteServer_GetAvailableGPUs)
    {
      SERIALISE_ELEMENT(gpus);
    }
    else
    {
      RDCERR("Unexpected response to GetAvailableGPUs");
    }

    ser.EndChunk();
  }

  return gpus;
}

int RemoteServer::GetSectionCount()
{
  if(!Connected())
    return 0;

  {
    WRITE_DATA_SCOPE();
    SCOPED_SERIALISE_CHUNK(eRemoteServer_GetSectionCount);
  }

  int count = 0;

  {
    READ_DATA_SCOPE();
    RemoteServerPacket type = ser.ReadChunk<RemoteServerPacket>();

    if(type == eRemoteServer_GetSectionCount)
    {
      SERIALISE_ELEMENT(count);
    }
    else
    {
      RDCERR("Unexpected response to GetSectionCount");
    }

    ser.EndChunk();
  }

  return count;
}

int RemoteServer::FindSectionByName(const char *name)
{
  if(!Connected())
    return -1;

  {
    WRITE_DATA_SCOPE();
    SCOPED_SERIALISE_CHUNK(eRemoteServer_FindSectionByName);
    SERIALISE_ELEMENT(name);
  }

  int index = -1;

  {
    READ_DATA_SCOPE();
    RemoteServerPacket type = ser.ReadChunk<RemoteServerPacket>();

    if(type == eRemoteServer_FindSectionByName)
    {
      SERIALISE_ELEMENT(index);
    }
    else
    {
      RDCERR("Unexpected response to FindSectionByName");
    }

    ser.EndChunk();
  }

  return index;
}

int RemoteServer::FindSectionByType(SectionType sectionType)
{
  if(!Connected())
    return -1;

  {
    WRITE_DATA_SCOPE();
    SCOPED_SERIALISE_CHUNK(eRemoteServer_FindSectionByType);
    SERIALISE_ELEMENT(sectionType);
  }

  int index = -1;

  {
    READ_DATA_SCOPE();
    RemoteServerPacket type = ser.ReadChunk<RemoteServerPacket>();

    if(type == eRemoteServer_FindSectionByType)
    {
      SERIALISE_ELEMENT(index);
    }
    else
    {
      RDCERR("Unexpected response to FindSectionByType");
    }

    ser.EndChunk();
  }

  return index;
}

SectionProperties RemoteServer::GetSectionProperties(int index)
{
  if(!Connected())
    return SectionProperties();

  {
    WRITE_DATA_SCOPE();
    SCOPED_SERIALISE_CHUNK(eRemoteServer_GetSectionProperties);
    SERIALISE_ELEMENT(index);
  }

  SectionProperties props;

  {
    READ_DATA_SCOPE();
    RemoteServerPacket type = ser.ReadChunk<RemoteServerPacket>();

    if(type == eRemoteServer_GetSectionProperties)
    {
      SERIALISE_ELEMENT(props);
    }
    else
    {
      RDCERR("Unexpected response to GetSectionProperties");
    }

    ser.EndChunk();
  }

  return props;
}

bytebuf RemoteServer::GetSectionContents(int index)
{
  if(!Connected())
    return bytebuf();

  {
    WRITE_DATA_SCOPE();
    SCOPED_SERIALISE_CHUNK(eRemoteServer_GetSectionContents);
    SERIALISE_ELEMENT(index);
  }

  bytebuf contents;

  {
    READ_DATA_SCOPE();
    RemoteServerPacket type = ser.ReadChunk<RemoteServerPacket>();

    if(type == eRemoteServer_GetSectionContents)
    {
      SERIALISE_ELEMENT(contents);
    }
    else
    {
      RDCERR("Unexpected response to GetSectionContents");
    }

    ser.EndChunk();
  }

  return contents;
}

bool RemoteServer::WriteSection(const SectionProperties &props, const bytebuf &contents)
{
  if(!Connected())
    return false;

  {
    WRITE_DATA_SCOPE();
    SCOPED_SERIALISE_CHUNK(eRemoteServer_WriteSection);
    SERIALISE_ELEMENT(props);
    SERIALISE_ELEMENT(contents);
  }

  bool success = false;

  {
    READ_DATA_SCOPE();
    RemoteServerPacket type = ser.ReadChunk<RemoteServerPacket>();

    if(type == eRemoteServer_WriteSection)
    {
      SERIALISE_ELEMENT(success);
    }
    else
    {
      RDCERR("Unexpected response to has write section request");
    }

    ser.EndChunk();
  }

  return success;
}

bool RemoteServer::HasCallstacks()
{
  if(!Connected())
    return false;

  {
    WRITE_DATA_SCOPE();
    SCOPED_SERIALISE_CHUNK(eRemoteServer_HasCallstacks);
  }

  bool hasCallstacks = false;

  {
    READ_DATA_SCOPE();
    RemoteServerPacket type = ser.ReadChunk<RemoteServerPacket>();

    if(type == eRemoteServer_HasCallstacks)
    {
      SERIALISE_ELEMENT(hasCallstacks);
    }
    else
    {
      RDCERR("Unexpected response to has callstacks request");
    }

    ser.EndChunk();
  }

  return hasCallstacks;
}

bool RemoteServer::InitResolver(bool interactive, RENDERDOC_ProgressCallback progress)
{
  {
    WRITE_DATA_SCOPE();
    SCOPED_SERIALISE_CHUNK(eRemoteServer_InitResolver);
  }

  RemoteServerPacket type = eRemoteServer_Noop;
  while(!reader->IsErrored())
  {
    READ_DATA_SCOPE();
    type = ser.ReadChunk<RemoteServerPacket>();

    if(reader->IsErrored() || type != eRemoteServer_ResolverProgress)
      break;

    float progressValue = 0.0f;

    SERIALISE_ELEMENT(progressValue);

    ser.EndChunk();

    if(progress)
      progress(progressValue);

    RDCLOG("% 3.0f%%...", progressValue * 100.0f);
  }

  if(reader->IsErrored() || type != eRemoteServer_InitResolver)
  {
    return false;
  }

  bool success = false;
  {
    READ_DATA_SCOPE();
    SERIALISE_ELEMENT(success);
    ser.EndChunk();
  }

  if(progress)
    progress(1.0f);

  return success;
}

rdcarray<rdcstr> RemoteServer::GetResolve(const rdcarray<uint64_t> &callstack)
{
  if(!Connected())
    return {""};

  {
    WRITE_DATA_SCOPE();
    SCOPED_SERIALISE_CHUNK(eRemoteServer_GetResolve);
    SERIALISE_ELEMENT(callstack);
  }

  rdcarray<rdcstr> StackFrames;

  {
    READ_DATA_SCOPE();
    RemoteServerPacket type = ser.ReadChunk<RemoteServerPacket>();

    if(type == eRemoteServer_GetResolve)
    {
      SERIALISE_ELEMENT(StackFrames);
    }
    else
    {
      RDCERR("Unexpected response to resolve request");
    }

    ser.EndChunk();
  }

  return StackFrames;
}
