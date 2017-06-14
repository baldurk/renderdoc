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
#include "core/core.h"
#include "os/os_specific.h"
#include "replay/replay_controller.h"
#include "serialise/serialiser.h"
#include "serialise/string_utils.h"
#include "replay_proxy.h"
#include "socket_helpers.h"

using std::pair;

template <>
string ToStrHelper<false, PathProperty>::Get(const PathProperty &el)
{
  return "<...>";
}

template <>
string ToStrHelper<false, CaptureOptions>::Get(const CaptureOptions &el)
{
  return "<...>";
}

template <>
void Serialiser::Serialise(const char *name, PathEntry &el)
{
  ScopedContext scope(this, name, "DirectoryFile", 0, true);

  Serialise("filename", el.filename);
  Serialise("flags", el.flags);
  Serialise("lastmod", el.lastmod);
  Serialise("size", el.size);
}

template <>
string ToStrHelper<false, EnvMod>::Get(const EnvMod &el)
{
  return "<...>";
}

template <>
string ToStrHelper<false, EnvSep>::Get(const EnvSep &el)
{
  return "<...>";
}

template <>
void Serialiser::Serialise(const char *name, EnvironmentModification &el)
{
  ScopedContext scope(this, name, "EnvironmentModification", 0, true);

  Serialise("mod", el.mod);
  Serialise("sep", el.sep);
  Serialise("name", el.name);
  Serialise("value", el.value);
}

static const uint32_t RemoteServerProtocolVersion = 1;

enum RemoteServerPacket
{
  eRemoteServer_Noop,
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
  eRemoteServer_CloseLog,
  eRemoteServer_HomeDir,
  eRemoteServer_ListDir,
  eRemoteServer_ExecuteAndInject,
  eRemoteServer_ShutdownServer,
  eRemoteServer_RemoteServerCount,
};

RDCCOMPILE_ASSERT((int)eRemoteServer_RemoteServerCount < (int)eReplayProxy_First,
                  "Remote server and Replay Proxy packets overlap");

struct ProgressLoopData
{
  Network::Socket *sock;
  float progress;
  bool killsignal;
};

static void ProgressTicker(void *d)
{
  ProgressLoopData *data = (ProgressLoopData *)d;

  Serialiser ser("", Serialiser::WRITING, false);

  while(!data->killsignal)
  {
    ser.Rewind();
    ser.Serialise("", data->progress);

    if(!SendPacket(data->sock, eRemoteServer_LogOpenProgress, ser))
    {
      SAFE_DELETE(data->sock);
      break;
    }
    Threading::Sleep(100);
  }
}

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

static void InactiveRemoteClientThread(void *data)
{
  ClientThread *threadData = (ClientThread *)data;

  uint32_t ip = threadData->socket->GetRemoteIP();

  // this thread just handles receiving the handshake and sending a busy signal without blocking the
  // server thread
  RemoteServerPacket type = eRemoteServer_Noop;
  Serialiser *recvser = NULL;

  if(!RecvPacket(threadData->socket, type, &recvser) || type != eRemoteServer_Handshake)
  {
    RDCWARN("Didn't receive proper handshake");
    SAFE_DELETE(threadData->socket);
    return;
  }

  uint32_t version = 0;
  recvser->Serialise("version", version);

  SAFE_DELETE(recvser);

  if(version != RemoteServerProtocolVersion)
  {
    RDCLOG("Connection using protocol %u, but we are running %u", version,
           RemoteServerProtocolVersion);
    SendPacket(threadData->socket, eRemoteServer_VersionMismatch);
  }
  else
  {
    SendPacket(threadData->socket, eRemoteServer_Busy);
  }

  SAFE_DELETE(threadData->socket);

  RDCLOG("Closed inactive connection from %u.%u.%u.%u.", Network::GetIPOctet(ip, 0),
         Network::GetIPOctet(ip, 1), Network::GetIPOctet(ip, 2), Network::GetIPOctet(ip, 3));
}

static void ActiveRemoteClientThread(void *data)
{
  ClientThread *threadData = (ClientThread *)data;

  Network::Socket *&client = threadData->socket;

  uint32_t ip = client->GetRemoteIP();

  RemoteServerPacket type = eRemoteServer_Noop;
  Serialiser *handshakeSer = NULL;

  if(!RecvPacket(threadData->socket, type, &handshakeSer) || type != eRemoteServer_Handshake)
  {
    RDCWARN("Didn't receive proper handshake");
    SAFE_DELETE(client);
    return;
  }

  uint32_t version = 0;
  handshakeSer->Serialise("version", version);

  SAFE_DELETE(handshakeSer);

  if(version != RemoteServerProtocolVersion)
  {
    RDCLOG("Connection using protocol %u, but we are running %u", version,
           RemoteServerProtocolVersion);
    SendPacket(threadData->socket, eRemoteServer_VersionMismatch);
    SAFE_DELETE(client);
    return;
  }
  else
  {
    // handshake and continue
    SendPacket(threadData->socket, eRemoteServer_Handshake);
  }

  vector<string> tempFiles;
  IRemoteDriver *driver = NULL;
  ReplayProxy *proxy = NULL;

  Serialiser sendSer("", Serialiser::WRITING, false);

  while(client)
  {
    if(client && !client->Connected())
      break;

    if(threadData->killThread)
      break;

    RemoteServerPacket sendType = eRemoteServer_Noop;
    sendSer.Rewind();

    Threading::Sleep(4);

    if(client->IsRecvDataWaiting())
    {
      type = eRemoteServer_Noop;
      Serialiser *recvser = NULL;

      if(!RecvPacket(client, type, &recvser))
        break;

      if(client == NULL)
      {
        SAFE_DELETE(recvser);
        continue;
      }
      else if(type == eRemoteServer_Ping)
      {
        sendType = eRemoteServer_Ping;
      }
      else if(type == eRemoteServer_RemoteDriverList)
      {
        map<RDCDriver, string> drivers = RenderDoc::Inst().GetRemoteDrivers();

        sendType = eRemoteServer_RemoteDriverList;

        uint32_t count = (uint32_t)drivers.size();
        sendSer.Serialise("", count);

        for(auto it = drivers.begin(); it != drivers.end(); ++it)
        {
          RDCDriver driverType = it->first;
          sendSer.Serialise("", driverType);
          sendSer.Serialise("", (*it).second);
        }
      }
      else if(type == eRemoteServer_HomeDir)
      {
        sendType = eRemoteServer_HomeDir;

        string home = FileIO::GetHomeFolderFilename();
        sendSer.Serialise("", home);
      }
      else if(type == eRemoteServer_ListDir)
      {
        string path;
        recvser->Serialise("path", path);

        sendType = eRemoteServer_ListDir;

        std::vector<PathEntry> files = FileIO::GetFilesInDirectory(path.c_str());

        sendSer.Serialise("", files);
      }
      else if(type == eRemoteServer_CopyCaptureFromRemote)
      {
        string path;
        recvser->Serialise("path", path);

        if(!SendChunkedFile(client, eRemoteServer_CopyCaptureFromRemote, path.c_str(), sendSer, NULL))
        {
          RDCERR("Network error sending file");
          SAFE_DELETE(recvser);
          break;
        }

        sendSer.Rewind();
      }
      else if(type == eRemoteServer_CopyCaptureToRemote)
      {
        string cap_file;
        string dummy, dummy2;
        FileIO::GetDefaultFiles("remotecopy", cap_file, dummy, dummy2);

        Serialiser *fileRecv = NULL;

        RDCLOG("Copying file to local path '%s'.", cap_file.c_str());

        if(!RecvChunkedFile(client, type, cap_file.c_str(), fileRecv, NULL))
        {
          FileIO::Delete(cap_file.c_str());

          RDCERR("Network error receiving file");

          SAFE_DELETE(fileRecv);
          SAFE_DELETE(recvser);
          break;
        }

        RDCLOG("File received.");

        tempFiles.push_back(cap_file);

        SAFE_DELETE(fileRecv);

        sendType = eRemoteServer_CopyCaptureToRemote;
        sendSer.Serialise("path", cap_file);
      }
      else if(type == eRemoteServer_TakeOwnershipCapture)
      {
        string cap_file;
        recvser->Serialise("filename", cap_file);

        RDCLOG("Taking ownership of '%s'.", cap_file.c_str());

        tempFiles.push_back(cap_file);
      }
      else if(type == eRemoteServer_ShutdownServer)
      {
        RDCLOG("Requested to shut down.");

        threadData->killServer = true;
        threadData->killThread = true;

        sendType = eRemoteServer_ShutdownServer;
      }
      else if(type == eRemoteServer_OpenLog)
      {
        string cap_file;
        recvser->Serialise("filename", cap_file);

        RDCASSERT(driver == NULL && proxy == NULL);

        RDCDriver driverType = RDC_Unknown;
        string driverName = "";
        uint64_t fileMachineIdent = 0;
        ReplayStatus status = RenderDoc::Inst().FillInitParams(cap_file.c_str(), driverType,
                                                               driverName, fileMachineIdent, NULL);

        if(status != ReplayStatus::Succeeded)
        {
          RDCERR("Failed to open %s", cap_file.c_str());
        }
        else if(RenderDoc::Inst().HasRemoteDriver(driverType))
        {
          ProgressLoopData progressData;

          progressData.sock = client;
          progressData.killsignal = false;
          progressData.progress = 0.0f;

          RenderDoc::Inst().SetProgressPtr(&progressData.progress);

          Threading::ThreadHandle ticker = Threading::CreateThread(ProgressTicker, &progressData);

          status = RenderDoc::Inst().CreateRemoteDriver(driverType, cap_file.c_str(), &driver);

          if(status != ReplayStatus::Succeeded || driver == NULL)
          {
            RDCERR("Failed to create remote driver for driver type %d name %s", driverType,
                   driverName.c_str());
          }
          else
          {
            driver->ReadLogInitialisation();

            RenderDoc::Inst().SetProgressPtr(NULL);

            progressData.killsignal = true;
            Threading::JoinThread(ticker);
            Threading::CloseThread(ticker);

            proxy = new ReplayProxy(client, driver);
          }
        }
        else
        {
          RDCERR("File needs driver for %s which isn't supported!", driverName.c_str());

          status = ReplayStatus::APIUnsupported;
        }

        sendType = eRemoteServer_LogOpened;
        sendSer.Serialise("status", status);
      }
      else if(type == eRemoteServer_CloseLog)
      {
        if(driver)
          driver->Shutdown();
        driver = NULL;

        SAFE_DELETE(proxy);
      }
      else if(type == eRemoteServer_ExecuteAndInject)
      {
        string app, workingDir, cmdLine, logfile;
        CaptureOptions opts;
        recvser->Serialise("app", app);
        recvser->Serialise("workingDir", workingDir);
        recvser->Serialise("cmdLine", cmdLine);
        recvser->Serialise("opts", opts);

        rdctype::array<EnvironmentModification> env;
        recvser->Serialise("env", env);

        uint32_t ident = uint32_t(ReplayStatus::NetworkIOFailed);

        if(threadData->allowExecution)
        {
          ident = Process::LaunchAndInjectIntoProcess(app.c_str(), workingDir.c_str(),
                                                      cmdLine.c_str(), env, "", opts, false);
        }
        else
        {
          RDCWARN("Requested to execute program - disallowing based on configuration");
        }

        sendType = eRemoteServer_ExecuteAndInject;
        sendSer.Serialise("ident", ident);
      }
      else if((int)type >= eReplayProxy_First && proxy)
      {
        bool ok = proxy->Tick(type, recvser);

        SAFE_DELETE(recvser);

        if(!ok)
          break;

        continue;
      }

      SAFE_DELETE(recvser);

      if(sendType != eRemoteServer_Noop && !SendPacket(client, sendType, sendSer))
      {
        RDCERR("Network error sending supported driver list");
        break;
      }

      continue;
    }
  }

  if(driver)
    driver->Shutdown();
  SAFE_DELETE(proxy);

  for(size_t i = 0; i < tempFiles.size(); i++)
  {
    FileIO::Delete(tempFiles[i].c_str());
  }

  RDCLOG("Closing active connection from %u.%u.%u.%u.", Network::GetIPOctet(ip, 0),
         Network::GetIPOctet(ip, 1), Network::GetIPOctet(ip, 2), Network::GetIPOctet(ip, 3));

  RDCLOG("Ready for new active connection...");

  SAFE_DELETE(client);
}

void RenderDoc::BecomeRemoteServer(const char *listenhost, uint16_t port, volatile bool32 &killReplay)
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
        "Create a config file remoteserver.conf in ~/.renderdoc or %%APPDATA%%/renderdoc to narrow "
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

      activeClientData->thread = Threading::CreateThread(ActiveRemoteClientThread, activeClientData);

      RDCLOG("Making active connection");
    }
    else
    {
      ClientThread *inactive = new ClientThread();
      inactive->socket = client;
      inactive->allowExecution = false;

      inactive->thread = Threading::CreateThread(InactiveRemoteClientThread, inactive);

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
  RemoteServer(Network::Socket *sock, const char *hostname) : m_Socket(sock), m_hostname(hostname)
  {
    map<RDCDriver, string> m = RenderDoc::Inst().GetReplayDrivers();

    m_Proxies.reserve(m.size());
    for(auto it = m.begin(); it != m.end(); ++it)
      m_Proxies.push_back(*it);
  }
  const string &hostname() const { return m_hostname; }
  virtual ~RemoteServer() { SAFE_DELETE(m_Socket); }
  void ShutdownConnection() { delete this; }
  void ShutdownServerAndConnection()
  {
    Serialiser sendData("", Serialiser::WRITING, false);
    Send(eRemoteServer_ShutdownServer, sendData);

    RemoteServerPacket type = eRemoteServer_Noop;
    vector<byte> payload;
    RecvPacket(m_Socket, type, payload);
    delete this;
  }
  bool Connected() { return m_Socket != NULL && m_Socket->Connected(); }
  bool Ping()
  {
    if(!Connected())
      return false;

    Serialiser sendData("", Serialiser::WRITING, false);
    Send(eRemoteServer_Ping, sendData);

    RemoteServerPacket type = eRemoteServer_Noop;
    Serialiser *ser = NULL;
    Get(type, &ser);

    SAFE_DELETE(ser);

    return type == eRemoteServer_Ping;
  }

  rdctype::array<rdctype::str> LocalProxies()
  {
    rdctype::array<rdctype::str> out;

    create_array_uninit(out, m_Proxies.size());

    size_t i = 0;
    for(auto it = m_Proxies.begin(); it != m_Proxies.end(); ++it, ++i)
      out[i] = it->second;

    return out;
  }

  rdctype::array<rdctype::str> RemoteSupportedReplays()
  {
    rdctype::array<rdctype::str> out;

    {
      Serialiser sendData("", Serialiser::WRITING, false);
      Send(eRemoteServer_RemoteDriverList, sendData);

      RemoteServerPacket type = eRemoteServer_RemoteDriverList;

      Serialiser *ser = NULL;
      Get(type, &ser);

      if(ser)
      {
        uint32_t count = 0;
        ser->Serialise("", count);

        create_array_uninit(out, count);

        for(uint32_t i = 0; i < count; i++)
        {
          RDCDriver driver = RDC_Unknown;
          string name = "";
          ser->Serialise("", driver);
          ser->Serialise("", name);

          out[i] = name;
        }

        delete ser;
      }
    }

    return out;
  }

  rdctype::str GetHomeFolder()
  {
    if(Android::IsHostADB(m_hostname.c_str()))
      return "/";

    rdctype::str ret;

    Serialiser sendData("", Serialiser::WRITING, false);
    Send(eRemoteServer_HomeDir, sendData);

    RemoteServerPacket type = eRemoteServer_HomeDir;

    Serialiser *ser = NULL;
    Get(type, &ser);

    if(ser)
    {
      string home;
      ser->Serialise("", home);

      ret = home;

      delete ser;
    }

    return ret;
  }

  rdctype::array<PathEntry> ListFolder(const char *path)
  {
    rdctype::array<PathEntry> ret;

    if(Android::IsHostADB(m_hostname.c_str()))
    {
      int index = 0;
      std::string deviceID;
      Android::extractDeviceIDAndIndex(m_hostname, index, deviceID);

      string adbStdout = Android::adbExecCommand(deviceID, "shell pm list packages -3");
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
          packages.push_back(package);
        }
      }

      create_array_uninit(ret, packages.size());
      for(size_t i = 0; i < packages.size(); i++)
        ret[i] = packages[i];

      return ret;
    }

    string folderPath = path;

    Serialiser sendData("", Serialiser::WRITING, false);
    sendData.Serialise("", folderPath);
    Send(eRemoteServer_ListDir, sendData);

    RemoteServerPacket type = eRemoteServer_ListDir;

    Serialiser *ser = NULL;
    Get(type, &ser);

    if(ser)
    {
      std::vector<PathEntry> paths;

      ser->Serialise("", paths);

      ret = paths;

      delete ser;
    }
    else
    {
      create_array_uninit(ret, 1);
      ret.elems[0].filename = path;
      ret.elems[0].flags = PathProperty::ErrorUnknown;
    }

    return ret;
  }

  uint32_t ExecuteAndInject(const char *app, const char *workingDir, const char *cmdLine,
                            const rdctype::array<EnvironmentModification> &env,
                            const CaptureOptions &opts)
  {
    const char *host = hostname().c_str();
    if(Android::IsHostADB(host))
      return Android::StartAndroidPackageForCapture(host, app);

    string appstr = app && app[0] ? app : "";
    string workstr = workingDir && workingDir[0] ? workingDir : "";
    string cmdstr = cmdLine && cmdLine[0] ? cmdLine : "";

    Serialiser sendData("", Serialiser::WRITING, false);
    sendData.Serialise("app", appstr);
    sendData.Serialise("workingDir", workstr);
    sendData.Serialise("cmdLine", cmdstr);
    sendData.Serialise("opts", (CaptureOptions &)opts);
    sendData.Serialise("env", (rdctype::array<EnvironmentModification> &)env);

    Send(eRemoteServer_ExecuteAndInject, sendData);

    RemoteServerPacket type = eRemoteServer_ExecuteAndInject;

    Serialiser *ser = NULL;
    Get(type, &ser);

    uint32_t ident = 0;

    if(ser)
      ser->Serialise("ident", ident);

    SAFE_DELETE(ser);

    return ident;
  }

  void CopyCaptureFromRemote(const char *remotepath, const char *localpath, float *progress)
  {
    string path = remotepath;
    Serialiser sendData("", Serialiser::WRITING, false);
    sendData.Serialise("path", path);
    Send(eRemoteServer_CopyCaptureFromRemote, sendData);

    float dummy = 0.0f;
    if(progress == NULL)
      progress = &dummy;

    Serialiser *fileRecv = NULL;

    if(!RecvChunkedFile(m_Socket, eRemoteServer_CopyCaptureFromRemote, localpath, fileRecv, progress))
    {
      SAFE_DELETE(fileRecv);
      RDCERR("Network error receiving file");
      return;
    }
    SAFE_DELETE(fileRecv);
  }

  rdctype::str CopyCaptureToRemote(const char *filename, float *progress)
  {
    Serialiser sendData("", Serialiser::WRITING, false);
    Send(eRemoteServer_CopyCaptureToRemote, sendData);

    float dummy = 0.0f;
    if(progress == NULL)
      progress = &dummy;

    sendData.Rewind();

    if(!SendChunkedFile(m_Socket, eRemoteServer_CopyCaptureToRemote, filename, sendData, progress))
    {
      SAFE_DELETE(m_Socket);
      return "";
    }

    RemoteServerPacket type = eRemoteServer_Noop;
    Serialiser *ser = NULL;
    Get(type, &ser);

    if(type == eRemoteServer_CopyCaptureToRemote && ser)
    {
      string remotepath;
      ser->Serialise("path", remotepath);
      return remotepath;
    }

    return "";
  }

  void TakeOwnershipCapture(const char *filename)
  {
    string logfile = filename;

    Serialiser sendData("", Serialiser::WRITING, false);
    sendData.Serialise("logfile", logfile);
    Send(eRemoteServer_TakeOwnershipCapture, sendData);
  }

  rdctype::pair<ReplayStatus, IReplayController *> OpenCapture(uint32_t proxyid,
                                                               const char *filename, float *progress)
  {
    rdctype::pair<ReplayStatus, IReplayController *> ret;
    ret.first = ReplayStatus::InternalError;
    ret.second = NULL;

    string logfile = filename;

    if(proxyid != ~0U && proxyid >= m_Proxies.size())
    {
      RDCERR("Invalid proxy driver id %d specified for remote renderer", proxyid);
      ret.first = ReplayStatus::InternalError;
      return ret;
    }

    float dummy = 0.0f;
    if(progress == NULL)
      progress = &dummy;

    // if the proxy id is ~0U, then we just don't care so let RenderDoc pick the most
    // appropriate supported proxy for the current platform.
    RDCDriver proxydrivertype = proxyid == ~0U ? RDC_Unknown : m_Proxies[proxyid].first;

    Serialiser sendData("", Serialiser::WRITING, false);
    sendData.Serialise("logfile", logfile);
    Send(eRemoteServer_OpenLog, sendData);

    Serialiser *progressSer = NULL;
    RemoteServerPacket type = eRemoteServer_Noop;
    while(m_Socket)
    {
      Get(type, &progressSer);

      if(!m_Socket || progressSer == NULL || type != eRemoteServer_LogOpenProgress)
        break;

      progressSer->Serialise("", *progress);

      RDCLOG("% 3.0f%%...", (*progress) * 100.0f);

      SAFE_DELETE(progressSer);
    }

    if(!m_Socket || progressSer == NULL || type != eRemoteServer_LogOpened)
    {
      ret.first = ReplayStatus::NetworkIOFailed;
      return ret;
    }

    ReplayStatus status = ReplayStatus::Succeeded;
    progressSer->Serialise("status", status);

    SAFE_DELETE(progressSer);

    *progress = 1.0f;

    if(status != ReplayStatus::Succeeded)
    {
      ret.first = status;
      return ret;
    }

    RDCLOG("Log ready on replay host");

    IReplayDriver *proxyDriver = NULL;
    status = RenderDoc::Inst().CreateReplayDriver(proxydrivertype, NULL, &proxyDriver);

    if(status != ReplayStatus::Succeeded || !proxyDriver)
    {
      if(proxyDriver)
        proxyDriver->Shutdown();
      ret.first = status;
      return ret;
    }

    ReplayController *rend = new ReplayController();

    ReplayProxy *proxy = new ReplayProxy(m_Socket, proxyDriver);
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
    Serialiser sendData("", Serialiser::WRITING, false);
    Send(eRemoteServer_CloseLog, sendData);

    rend->Shutdown();
  }

private:
  Network::Socket *m_Socket;
  string m_hostname;

  void Send(RemoteServerPacket type, const Serialiser &ser) { SendPacket(m_Socket, type, ser); }
  void Get(RemoteServerPacket &type, Serialiser **ser)
  {
    vector<byte> payload;

    if(!RecvPacket(m_Socket, type, payload))
    {
      SAFE_DELETE(m_Socket);
      if(ser)
        *ser = NULL;
      return;
    }

    if(ser)
      *ser = new Serialiser(payload.size(), &payload[0], false);
  }

  vector<pair<RDCDriver, string> > m_Proxies;
};

extern "C" RENDERDOC_API void RENDERDOC_CC RemoteServer_ShutdownConnection(IRemoteServer *remote)
{
  remote->ShutdownConnection();
}

extern "C" RENDERDOC_API void RENDERDOC_CC RemoteServer_ShutdownServerAndConnection(IRemoteServer *remote)
{
  remote->ShutdownServerAndConnection();
}

extern "C" RENDERDOC_API bool32 RENDERDOC_CC RemoteServer_Ping(IRemoteServer *remote)
{
  return remote->Ping();
}

extern "C" RENDERDOC_API void RENDERDOC_CC RemoteServer_LocalProxies(IRemoteServer *remote,
                                                                     rdctype::array<rdctype::str> *out)
{
  *out = remote->LocalProxies();
}

extern "C" RENDERDOC_API void RENDERDOC_CC RemoteServer_GetHomeFolder(IRemoteServer *remote,
                                                                      rdctype::str *home)
{
  rdctype::str path = remote->GetHomeFolder();
  if(home)
    *home = path;
}

extern "C" RENDERDOC_API void RENDERDOC_CC RemoteServer_ListFolder(IRemoteServer *remote,
                                                                   const char *path,
                                                                   rdctype::array<PathEntry> *dirlist)
{
  rdctype::array<PathEntry> files = remote->ListFolder(path);
  if(dirlist)
    *dirlist = files;
}

extern "C" RENDERDOC_API void RENDERDOC_CC
RemoteServer_RemoteSupportedReplays(IRemoteServer *remote, rdctype::array<rdctype::str> *out)
{
  *out = remote->RemoteSupportedReplays();
}

extern "C" RENDERDOC_API uint32_t RENDERDOC_CC RemoteServer_ExecuteAndInject(
    IRemoteServer *remote, const char *app, const char *workingDir, const char *cmdLine,
    const rdctype::array<EnvironmentModification> &env, const CaptureOptions &opts)
{
  return remote->ExecuteAndInject(app, workingDir, cmdLine, env, opts);
}

extern "C" RENDERDOC_API void RENDERDOC_CC RemoteServer_TakeOwnershipCapture(IRemoteServer *remote,
                                                                             const char *filename)
{
  remote->TakeOwnershipCapture(filename);
}

extern "C" RENDERDOC_API void RENDERDOC_CC RemoteServer_CopyCaptureToRemote(IRemoteServer *remote,
                                                                            const char *filename,
                                                                            float *progress,
                                                                            rdctype::str *remotepath)
{
  rdctype::str path = remote->CopyCaptureToRemote(filename, progress);
  if(remotepath)
    *remotepath = path;
}

extern "C" RENDERDOC_API void RENDERDOC_CC RemoteServer_CopyCaptureFromRemote(IRemoteServer *remote,
                                                                              const char *remotepath,
                                                                              const char *localpath,
                                                                              float *progress)
{
  remote->CopyCaptureFromRemote(remotepath, localpath, progress);
}

extern "C" RENDERDOC_API ReplayStatus RENDERDOC_CC RemoteServer_OpenCapture(IRemoteServer *remote,
                                                                            uint32_t proxyid,
                                                                            const char *logfile,
                                                                            float *progress,
                                                                            IReplayController **rend)
{
  auto ret = remote->OpenCapture(proxyid, logfile, progress);
  if(rend)
    *rend = ret.second;
  return ret.first;
}

extern "C" RENDERDOC_API void RENDERDOC_CC RemoteServer_CloseCapture(IRemoteServer *remote,
                                                                     IReplayController *rend)
{
  return remote->CloseCapture(rend);
}

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

    // each subsequent device gets a new range of ports. The deviceID isn't needed since we already
    // forwarded the ports to the right devices.
    if(port == RENDERDOC_GetDefaultRemoteServerPort())
      port += RenderDoc_AndroidPortOffset * (index + 1);
  }

  Network::Socket *sock = NULL;

  if(s != "-")
  {
    sock = Network::CreateClientSocket(s.c_str(), (uint16_t)port, 750);

    if(sock == NULL)
      return ReplayStatus::NetworkIOFailed;
  }

  Serialiser sendData("", Serialiser::WRITING, false);
  uint32_t version = RemoteServerProtocolVersion;
  sendData.Serialise("version", version);
  SendPacket(sock, eRemoteServer_Handshake, sendData);

  RemoteServerPacket type = (RemoteServerPacket)RecvPacket(sock);

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

  if(type != eRemoteServer_Handshake)
  {
    RDCWARN("Didn't get proper handshake");
    SAFE_DELETE(sock);
    return ReplayStatus::NetworkIOFailed;
  }

  *rend = new RemoteServer(sock, host);

  return ReplayStatus::Succeeded;
}
