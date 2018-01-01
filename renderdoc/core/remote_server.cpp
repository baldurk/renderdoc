/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2017 Baldur Karlsson
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

#include <sstream>
#include <utility>
#include "api/replay/renderdoc_replay.h"
#include "core/android.h"
#include "core/core.h"
#include "os/os_specific.h"
#include "replay/replay_controller.h"
#include "serialise/rdcfile.h"
#include "serialise/serialiser.h"
#include "strings/string_utils.h"
#include "replay_proxy.h"

static const uint32_t RemoteServerProtocolVersion = 3;

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
  eRemoteServer_GetSectionCount,
  eRemoteServer_FindSectionByName,
  eRemoteServer_FindSectionByType,
  eRemoteServer_GetSectionProperties,
  eRemoteServer_GetSectionContents,
  eRemoteServer_WriteSection,
  eRemoteServer_RemoteServerCount,
};

RDCCOMPILE_ASSERT((int)eRemoteServer_RemoteServerCount < (int)eReplayProxy_First,
                  "Remote server and Replay Proxy packets overlap");

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

static void InactiveRemoteClientThread(ClientThread *threadData)
{
  uint32_t ip = threadData->socket->GetRemoteIP();

  {
    uint32_t version = 0;

    {
      ReadSerialiser ser(new StreamReader(threadData->socket, Ownership::Nothing), Ownership::Stream);

      // this thread just handles receiving the handshake and sending a busy signal without blocking
      // the server thread
      RemoteServerPacket type = ser.ReadChunk<RemoteServerPacket>();

      if(ser.IsErrored() || type != eRemoteServer_Handshake)
      {
        RDCWARN("Didn't receive proper handshake");
        SAFE_DELETE(threadData->socket);
        return;
      }

      SERIALISE_ELEMENT(version);

      ser.EndChunk();
    }

    {
      WriteSerialiser ser(new StreamWriter(threadData->socket, Ownership::Nothing),
                          Ownership::Stream);

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
        SCOPED_SERIALISE_CHUNK(eRemoteServer_Busy);
      }
    }

    SAFE_DELETE(threadData->socket);

    RDCLOG("Closed inactive connection from %u.%u.%u.%u.", Network::GetIPOctet(ip, 0),
           Network::GetIPOctet(ip, 1), Network::GetIPOctet(ip, 2), Network::GetIPOctet(ip, 3));
  }
}

static void ActiveRemoteClientThread(ClientThread *threadData)
{
  Network::Socket *&client = threadData->socket;

  uint32_t ip = client->GetRemoteIP();

  uint32_t version = 0;

  {
    ReadSerialiser ser(new StreamReader(client, Ownership::Nothing), Ownership::Stream);

    // this thread just handles receiving the handshake and sending a busy signal without blocking
    // the server thread
    RemoteServerPacket type = ser.ReadChunk<RemoteServerPacket>();

    if(ser.IsErrored() || type != eRemoteServer_Handshake)
    {
      RDCWARN("Didn't receive proper handshake");
      SAFE_DELETE(client);
      return;
    }

    SERIALISE_ELEMENT(version);

    ser.EndChunk();
  }

  {
    WriteSerialiser ser(new StreamWriter(client, Ownership::Nothing), Ownership::Stream);

    ser.SetStreamingMode(true);

    if(version != RemoteServerProtocolVersion)
    {
      RDCLOG("Connection using protocol %u, but we are running %u", version,
             RemoteServerProtocolVersion);

      {
        SCOPED_SERIALISE_CHUNK(eRemoteServer_VersionMismatch);
      }
      SAFE_DELETE(client);
      return;
    }
    else
    {
      // handshake and continue
      SCOPED_SERIALISE_CHUNK(eRemoteServer_Handshake);
    }
  }

  std::vector<std::string> tempFiles;
  IRemoteDriver *driver = NULL;
  ReplayProxy *proxy = NULL;
  RDCFile *rdc = NULL;
  Callstack::StackResolver *resolver = NULL;

  WriteSerialiser writer(new StreamWriter(client, Ownership::Nothing), Ownership::Stream);
  ReadSerialiser reader(new StreamReader(client, Ownership::Nothing), Ownership::Stream);

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

      WRITE_DATA_SCOPE();
      SCOPED_SERIALISE_CHUNK(eRemoteServer_Ping);
    }
    else if(type == eRemoteServer_RemoteDriverList)
    {
      reader.EndChunk();

      std::map<RDCDriver, std::string> drivers = RenderDoc::Inst().GetRemoteDrivers();
      uint32_t count = (uint32_t)drivers.size();

      WRITE_DATA_SCOPE();
      SCOPED_SERIALISE_CHUNK(eRemoteServer_RemoteDriverList);
      SERIALISE_ELEMENT(count);

      for(auto it = drivers.begin(); it != drivers.end(); ++it)
      {
        RDCDriver driverType = it->first;
        const std::string &driverName = it->second;

        SERIALISE_ELEMENT(driverType);
        SERIALISE_ELEMENT(driverName);
      }
    }
    else if(type == eRemoteServer_HomeDir)
    {
      reader.EndChunk();

      std::string home = FileIO::GetHomeFolderFilename();

      {
        WRITE_DATA_SCOPE();
        SCOPED_SERIALISE_CHUNK(eRemoteServer_HomeDir);
        SERIALISE_ELEMENT(home);
      }
    }
    else if(type == eRemoteServer_ListDir)
    {
      std::string path;

      {
        READ_DATA_SCOPE();
        SERIALISE_ELEMENT(path);
      }

      reader.EndChunk();

      std::vector<PathEntry> files = FileIO::GetFilesInDirectory(path.c_str());

      {
        WRITE_DATA_SCOPE();
        SCOPED_SERIALISE_CHUNK(eRemoteServer_ListDir);
        SERIALISE_ELEMENT(files);
      }
    }
    else if(type == eRemoteServer_CopyCaptureFromRemote)
    {
      std::string path;

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
      std::string path;
      std::string dummy, dummy2;
      FileIO::GetDefaultFiles("remotecopy", path, dummy, dummy2);

      RDCLOG("Copying file to local path '%s'.", path.c_str());

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
      std::string path;

      {
        READ_DATA_SCOPE();
        SERIALISE_ELEMENT(path);
      }

      reader.EndChunk();

      RDCLOG("Taking ownership of '%s'.", path.c_str());

      tempFiles.push_back(path);
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
      std::string path;

      {
        READ_DATA_SCOPE();
        SERIALISE_ELEMENT(path);
      }

      reader.EndChunk();

      RDCASSERT(driver == NULL && proxy == NULL && rdc == NULL);
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

          RenderDoc::Inst().SetProgressPointer<LoadProgress>(&progress);

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

          status = RenderDoc::Inst().CreateRemoteDriver(rdc, &driver);

          if(status != ReplayStatus::Succeeded || driver == NULL)
          {
            RDCERR("Failed to create remote driver for driver '%s'", rdc->GetDriverName().c_str());
          }
          else
          {
            status = driver->ReadLogInitialisation(rdc, false);

            if(status != ReplayStatus::Succeeded)
            {
              RDCERR("Failed to initialise remote driver.");

              driver->Shutdown();
              driver = NULL;
            }
            else
            {
              RenderDoc::Inst().SetProgressPointer<LoadProgress>(NULL);

              kill = true;
              Threading::JoinThread(ticker);
              Threading::CloseThread(ticker);

              proxy = new ReplayProxy(reader, writer, driver);
            }
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

        std::vector<byte> buf;
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

          resolver = Callstack::MakeResolver(buf.data(), buf.size(), &progress);

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
      std::string name;

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

      if(rdc)
      {
        StreamWriter *sectionWriter = rdc->WriteSection(props);

        if(sectionWriter)
        {
          sectionWriter->Write(contents.data(), contents.size());
          delete sectionWriter;
        }
      }
    }
    else if(type == eRemoteServer_CloseLog)
    {
      reader.EndChunk();

      if(driver)
        driver->Shutdown();
      driver = NULL;

      SAFE_DELETE(proxy);
      SAFE_DELETE(rdc);
      SAFE_DELETE(resolver);
    }
    else if(type == eRemoteServer_ExecuteAndInject)
    {
      std::string app, workingDir, cmdLine, logfile;
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

      uint32_t ident = 0;

      if(threadData->allowExecution)
      {
        ident = Process::LaunchAndInjectIntoProcess(app.c_str(), workingDir.c_str(),
                                                    cmdLine.c_str(), env, "", opts, false);
      }
      else
      {
        RDCWARN("Requested to execute program - disallowing based on configuration");
      }

      {
        WRITE_DATA_SCOPE();
        SCOPED_SERIALISE_CHUNK(eRemoteServer_ExecuteAndInject);
        SERIALISE_ELEMENT(ident);
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

  if(driver)
    driver->Shutdown();
  SAFE_DELETE(proxy);
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

void RenderDoc::BecomeRemoteServer(const char *listenhost, uint16_t port, volatile bool &killReplay)
{
  Network::Socket *sock = Network::CreateServerSocket(listenhost, port, 1);

  if(sock == NULL)
    return;

  std::vector<std::pair<uint32_t, uint32_t> > listenRanges;
  bool allowExecution = true;

  FILE *f = FileIO::fopen(FileIO::GetAppFolderFilename("remoteserver.conf").c_str(), "r");

  while(f && !FileIO::feof(f))
  {
    string line = trim(FileIO::getline(f));

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
        listenRanges.push_back(std::make_pair(ip, mask));
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

  if(f)
    FileIO::fclose(f);

  if(listenRanges.empty())
  {
    RDCLOG("No whitelist IP ranges configured - using default private IP ranges.");
    RDCLOG(
        "Create a config file remoteserver.conf in ~/.renderdoc or %%APPDATA%%/renderdoc to "
        "narrow "
        "this down or accept connections from more ranges.");

    listenRanges.push_back(std::make_pair(Network::MakeIP(10, 0, 0, 0), 0xff000000));
    listenRanges.push_back(std::make_pair(Network::MakeIP(172, 16, 0, 0), 0xfff00000));
    listenRanges.push_back(std::make_pair(Network::MakeIP(192, 168, 0, 0), 0xffff0000));
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

  ClientThread *activeClientData = NULL;

  std::vector<ClientThread *> inactives;

  while(!killReplay)
  {
    Network::Socket *client = sock->AcceptClient(false);

    if(activeClientData && activeClientData->killServer)
      break;

    // reap any dead inactive threads
    for(size_t i = 0; i < inactives.size(); i++)
    {
      if(inactives[i]->socket == NULL)
      {
        Threading::JoinThread(inactives[i]->thread);
        Threading::CloseThread(inactives[i]->thread);
        delete inactives[i];
        inactives.erase(inactives.begin() + i);
        break;
      }
    }

    // reap our active connection possibly
    if(activeClientData && activeClientData->socket == NULL)
    {
      Threading::JoinThread(activeClientData->thread);
      Threading::CloseThread(activeClientData->thread);

      delete activeClientData;
      activeClientData = NULL;
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

    if(activeClientData == NULL)
    {
      activeClientData = new ClientThread();
      activeClientData->socket = client;
      activeClientData->allowExecution = allowExecution;

      activeClientData->thread = Threading::CreateThread(
          [activeClientData]() { ActiveRemoteClientThread(activeClientData); });

      RDCLOG("Making active connection");
    }
    else
    {
      ClientThread *inactive = new ClientThread();
      inactive->socket = client;
      inactive->allowExecution = false;

      inactive->thread =
          Threading::CreateThread([inactive]() { InactiveRemoteClientThread(inactive); });

      inactives.push_back(inactive);

      RDCLOG("Refusing inactive connection");
    }
  }

  if(activeClientData && activeClientData->socket != NULL)
  {
    activeClientData->killThread = true;

    Threading::JoinThread(activeClientData->thread);
    Threading::CloseThread(activeClientData->thread);

    delete activeClientData;
  }

  // shut down client threads
  for(size_t i = 0; i < inactives.size(); i++)
  {
    Threading::JoinThread(inactives[i]->thread);
    Threading::CloseThread(inactives[i]->thread);
    delete inactives[i];
  }

  SAFE_DELETE(sock);
}

struct RemoteServer : public IRemoteServer
{
public:
  RemoteServer(Network::Socket *sock, const char *hostname)
      : m_Socket(sock),
        m_hostname(hostname),
        reader(new StreamReader(sock, Ownership::Nothing), Ownership::Stream),
        writer(new StreamWriter(sock, Ownership::Nothing), Ownership::Stream)
  {
    writer.SetStreamingMode(true);
    reader.SetStreamingMode(true);

    std::map<RDCDriver, std::string> m = RenderDoc::Inst().GetReplayDrivers();

    m_Proxies.reserve(m.size());
    for(auto it = m.begin(); it != m.end(); ++it)
      m_Proxies.push_back(*it);
  }
  const std::string &hostname() const { return m_hostname; }
  virtual ~RemoteServer() { SAFE_DELETE(m_Socket); }
  void ShutdownConnection() { delete this; }
  void ShutdownServerAndConnection()
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
  bool Connected() { return m_Socket != NULL && m_Socket->Connected(); }
  bool Ping()
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

  rdcarray<rdcstr> LocalProxies()
  {
    rdcarray<rdcstr> out;

    m_Proxies.reserve(m_Proxies.size());

    size_t i = 0;
    for(auto it = m_Proxies.begin(); it != m_Proxies.end(); ++it, ++i)
      out.push_back(it->second);

    return out;
  }

  rdcarray<rdcstr> RemoteSupportedReplays()
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
          std::string driverName = "";

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

  rdcstr GetHomeFolder()
  {
    if(Android::IsHostADB(m_hostname.c_str()))
      return "";

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

  rdcarray<PathEntry> ListFolder(const char *path)
  {
    if(Android::IsHostADB(m_hostname.c_str()))
    {
      int index = 0;
      std::string deviceID;
      Android::extractDeviceIDAndIndex(m_hostname, index, deviceID);

      string adbStdout = Android::adbExecCommand(deviceID, "shell pm list packages -3").strStdout;
      using namespace std;
      istringstream stdoutStream(adbStdout);
      string line;
      vector<PathEntry> packages;
      while(getline(stdoutStream, line))
      {
        vector<string> tokens;
        split(line, tokens, ':');
        if(tokens.size() == 2 && tokens[0] == "package")
        {
          PathEntry package;
          package.filename = trim(tokens[1]);
          package.size = 0;
          package.lastmod = 0;
          package.flags = PathProperty::Executable;

          // hide our own internal packages
          if(strstr(package.filename.c_str(), "org.renderdoc."))
            continue;

          packages.push_back(package);
        }
      }

      return packages;
    }

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

  uint32_t ExecuteAndInject(const char *a, const char *w, const char *c,
                            const rdcarray<EnvironmentModification> &env, const CaptureOptions &opts)
  {
    const char *host = hostname().c_str();
    if(Android::IsHostADB(host))
      return Android::StartAndroidPackageForCapture(host, a);

    std::string app = a && a[0] ? a : "";
    std::string workingDir = w && w[0] ? w : "";
    std::string cmdline = c && c[0] ? c : "";

    {
      WRITE_DATA_SCOPE();
      SCOPED_SERIALISE_CHUNK(eRemoteServer_ExecuteAndInject);
      SERIALISE_ELEMENT(app);
      SERIALISE_ELEMENT(workingDir);
      SERIALISE_ELEMENT(cmdline);
      SERIALISE_ELEMENT(opts);
      SERIALISE_ELEMENT(env);
    }

    uint32_t ident = 0;

    {
      READ_DATA_SCOPE();
      RemoteServerPacket type = ser.ReadChunk<RemoteServerPacket>();

      if(type == eRemoteServer_ExecuteAndInject)
      {
        SERIALISE_ELEMENT(ident);
      }
      else
      {
        RDCERR("Unexpected response to execute and inject request");
      }

      ser.EndChunk();
    }

    return ident;
  }

  void CopyCaptureFromRemote(const char *remotepath, const char *localpath, float *progress)
  {
    std::string path = remotepath;

    {
      WRITE_DATA_SCOPE();
      SCOPED_SERIALISE_CHUNK(eRemoteServer_CopyCaptureFromRemote);
      SERIALISE_ELEMENT(path);
    }

    float dummy = 0.0f;
    if(progress == NULL)
      progress = &dummy;

    {
      READ_DATA_SCOPE();
      RemoteServerPacket type = ser.ReadChunk<RemoteServerPacket>();

      if(type == eRemoteServer_CopyCaptureFromRemote)
      {
        StreamWriter streamWriter(FileIO::fopen(localpath, "wb"), Ownership::Stream);

        ser.SerialiseStream(localpath, streamWriter, NULL);

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

  rdcstr CopyCaptureToRemote(const char *filename, float *progress)
  {
    {
      WRITE_DATA_SCOPE();
      SCOPED_SERIALISE_CHUNK(eRemoteServer_CopyCaptureToRemote);

      StreamReader fileStream(FileIO::fopen(filename, "rb"));
      ser.SerialiseStream(filename, fileStream);
    }

    std::string path;

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

  void TakeOwnershipCapture(const char *filename)
  {
    std::string path = filename;

    {
      WRITE_DATA_SCOPE();
      SCOPED_SERIALISE_CHUNK(eRemoteServer_TakeOwnershipCapture);
      SERIALISE_ELEMENT(path);
    }
  }

  rdcpair<ReplayStatus, IReplayController *> OpenCapture(uint32_t proxyid, const char *filename,
                                                         float *progressPtr)
  {
    rdcpair<ReplayStatus, IReplayController *> ret;
    ret.first = ReplayStatus::InternalError;
    ret.second = NULL;

    if(proxyid != ~0U && proxyid >= m_Proxies.size())
    {
      RDCERR("Invalid proxy driver id %d specified for remote renderer", proxyid);
      ret.first = ReplayStatus::InternalError;
      return ret;
    }

    float dummy = 0.0f;
    if(progressPtr == NULL)
      progressPtr = &dummy;

    float &progress = *progressPtr;

    // if the proxy id is ~0U, then we just don't care so let RenderDoc pick the most
    // appropriate supported proxy for the current platform.
    RDCDriver proxydrivertype = proxyid == ~0U ? RDCDriver::Unknown : m_Proxies[proxyid].first;

    {
      WRITE_DATA_SCOPE();
      SCOPED_SERIALISE_CHUNK(eRemoteServer_OpenLog);
      SERIALISE_ELEMENT(filename);
    }

    RemoteServerPacket type = eRemoteServer_Noop;
    while(!reader.IsErrored())
    {
      READ_DATA_SCOPE();
      type = ser.ReadChunk<RemoteServerPacket>();

      if(reader.IsErrored() || type != eRemoteServer_LogOpenProgress)
        break;

      SERIALISE_ELEMENT(progress);

      ser.EndChunk();

      RDCLOG("% 3.0f%%...", progress * 100.0f);
    }

    if(reader.IsErrored() || type != eRemoteServer_LogOpened)
    {
      ret.first = ReplayStatus::NetworkIOFailed;
      return ret;
    }

    ReplayStatus status = ReplayStatus::Succeeded;
    {
      READ_DATA_SCOPE();
      SERIALISE_ELEMENT(status);
      ser.EndChunk();
    }

    progress = 1.0f;

    if(status != ReplayStatus::Succeeded)
    {
      ret.first = status;
      return ret;
    }

    RDCLOG("Log ready on replay host");

    IReplayDriver *proxyDriver = NULL;
    status = RenderDoc::Inst().CreateProxyReplayDriver(proxydrivertype, &proxyDriver);

    if(status != ReplayStatus::Succeeded || !proxyDriver)
    {
      if(proxyDriver)
        proxyDriver->Shutdown();
      ret.first = status;
      return ret;
    }

    ReplayController *rend = new ReplayController();

    ReplayProxy *proxy = new ReplayProxy(reader, writer, proxyDriver);
    status = rend->SetDevice(proxy);

    if(status != ReplayStatus::Succeeded)
    {
      SAFE_DELETE(rend);
      ret.first = status;
      return ret;
    }

    // ReplayController takes ownership of the ProxySerialiser (as IReplayDriver)
    // and it cleans itself up in Shutdown.

    ret.first = ReplayStatus::Succeeded;
    ret.second = rend;
    return ret;
  }

  void CloseCapture(IReplayController *rend)
  {
    {
      WRITE_DATA_SCOPE();
      SCOPED_SERIALISE_CHUNK(eRemoteServer_CloseLog);
    }

    rend->Shutdown();
  }

  int GetSectionCount()
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

  int FindSectionByName(const char *name)
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

  int FindSectionByType(SectionType sectionType)
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

  SectionProperties GetSectionProperties(int index)
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

  bytebuf GetSectionContents(int index)
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

  void WriteSection(const SectionProperties &props, const bytebuf &contents)
  {
    if(!Connected())
      return;

    {
      WRITE_DATA_SCOPE();
      SCOPED_SERIALISE_CHUNK(eRemoteServer_WriteSection);
      SERIALISE_ELEMENT(props);
      SERIALISE_ELEMENT(contents);
    }
  }

  bool HasCallstacks()
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

  bool InitResolver(float *progressPtr)
  {
    float dummy = 0.0f;
    if(progressPtr == NULL)
      progressPtr = &dummy;

    float &progress = *progressPtr;

    {
      WRITE_DATA_SCOPE();
      SCOPED_SERIALISE_CHUNK(eRemoteServer_InitResolver);
    }

    RemoteServerPacket type = eRemoteServer_Noop;
    while(!reader.IsErrored())
    {
      READ_DATA_SCOPE();
      type = ser.ReadChunk<RemoteServerPacket>();

      if(reader.IsErrored() || type != eRemoteServer_ResolverProgress)
        break;

      SERIALISE_ELEMENT(progress);

      ser.EndChunk();

      RDCLOG("% 3.0f%%...", progress * 100.0f);
    }

    if(reader.IsErrored() || type != eRemoteServer_InitResolver)
    {
      return false;
    }

    bool success = false;
    {
      READ_DATA_SCOPE();
      SERIALISE_ELEMENT(success);
      ser.EndChunk();
    }

    progress = 1.0f;

    return success;
  }

  rdcarray<rdcstr> GetResolve(const rdcarray<uint64_t> &callstack)
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

private:
  Network::Socket *m_Socket;
  WriteSerialiser writer;
  ReadSerialiser reader;
  std::string m_hostname;

  std::vector<std::pair<RDCDriver, std::string> > m_Proxies;
};

extern "C" RENDERDOC_API ReplayStatus RENDERDOC_CC
RENDERDOC_CreateRemoteServerConnection(const char *host, uint32_t port, IRemoteServer **rend)
{
  if(rend == NULL)
    return ReplayStatus::InternalError;

  string s = "localhost";
  if(host != NULL && host[0] != '\0')
    s = host;

  if(port == 0)
    port = RENDERDOC_GetDefaultRemoteServerPort();

  if(host != NULL && Android::IsHostADB(host))
  {
    s = "127.0.0.1";

    int index = 0;
    std::string deviceID;
    Android::extractDeviceIDAndIndex(host, index, deviceID);

    // each subsequent device gets a new range of ports. The deviceID isn't needed since we
    // already
    // forwarded the ports to the right devices.
    if(port == RENDERDOC_GetDefaultRemoteServerPort())
      port += RenderDoc_AndroidPortOffset * (index + 1);
  }

  Network::Socket *sock = Network::CreateClientSocket(s.c_str(), (uint16_t)port, 750);

  if(sock == NULL)
    return ReplayStatus::NetworkIOFailed;

  uint32_t version = RemoteServerProtocolVersion;

  {
    WriteSerialiser ser(new StreamWriter(sock, Ownership::Nothing), Ownership::Stream);

    ser.SetStreamingMode(true);

    SCOPED_SERIALISE_CHUNK(eRemoteServer_Handshake);
    SERIALISE_ELEMENT(version);
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

  *rend = new RemoteServer(sock, host);

  return ReplayStatus::Succeeded;
}
