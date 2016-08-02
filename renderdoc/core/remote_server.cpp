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

#include <utility>
#include "api/replay/renderdoc_replay.h"
#include "core/core.h"
#include "os/os_specific.h"
#include "replay/replay_renderer.h"
#include "serialise/serialiser.h"
#include "serialise/string_utils.h"
#include "replay_proxy.h"
#include "socket_helpers.h"

using std::pair;

enum RemoteServerPacket
{
  eRemoteServer_Noop,
  eRemoteServer_RemoteDriverList,
  eRemoteServer_CopyCapture,
  eRemoteServer_LogOpenProgress,
  eRemoteServer_LogReady,
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

void RenderDoc::BecomeRemoteServer(const char *listenhost, uint16_t port, volatile bool32 &killReplay)
{
  Network::Socket *sock = Network::CreateServerSocket(listenhost, port, 1);

  if(sock == NULL)
    return;

  bool newlyReady = true;

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

  while(!killReplay)
  {
    if(newlyReady)
    {
      RDCLOG("Replay host ready for requests.");
      newlyReady = false;
    }

    Network::Socket *client = sock->AcceptClient(false);

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

    Serialiser ser("", Serialiser::WRITING, false);

    newlyReady = true;

    map<RDCDriver, string> drivers = RenderDoc::Inst().GetRemoteDrivers();

    uint32_t count = (uint32_t)drivers.size();
    ser.Serialise("", count);

    for(auto it = drivers.begin(); it != drivers.end(); ++it)
    {
      RDCDriver driver = it->first;
      ser.Serialise("", driver);
      ser.Serialise("", (*it).second);
    }

    if(!SendPacket(client, eRemoteServer_RemoteDriverList, ser))
    {
      RDCERR("Network error sending supported driver list");
      SAFE_DELETE(client);
      continue;
    }

    Threading::Sleep(4);

    // don't care about the result, just want to check that the socket hasn't been gracefully shut
    // down
    client->IsRecvDataWaiting();
    if(!client->Connected())
    {
      RDCLOG("Connection closed after sending remote driver list");
      SAFE_DELETE(client);
      continue;
    }

    string cap_file;
    string dummy, dummy2;
    FileIO::GetDefaultFiles("remotecopy", cap_file, dummy, dummy2);

    Serialiser *fileRecv = NULL;

    if(!RecvChunkedFile(client, eRemoteServer_CopyCapture, cap_file.c_str(), fileRecv, NULL))
    {
      FileIO::Delete(cap_file.c_str());

      RDCERR("Network error receiving file");

      SAFE_DELETE(fileRecv);
      SAFE_DELETE(client);
      continue;
    }

    RDCLOG("File received.");

    SAFE_DELETE(fileRecv);

    RDCDriver driverType = RDC_Unknown;
    string driverName = "";
    RenderDoc::Inst().FillInitParams(cap_file.c_str(), driverType, driverName, NULL);

    if(RenderDoc::Inst().HasRemoteDriver(driverType))
    {
      ProgressLoopData data;

      data.sock = client;
      data.killsignal = false;
      data.progress = 0.0f;

      RenderDoc::Inst().SetProgressPtr(&data.progress);

      Threading::ThreadHandle ticker = Threading::CreateThread(ProgressTicker, &data);

      IRemoteDriver *driver = NULL;
      auto status = RenderDoc::Inst().CreateRemoteDriver(driverType, cap_file.c_str(), &driver);

      if(status != eReplayCreate_Success || driver == NULL)
      {
        RDCERR("Failed to create remote driver for driver type %d name %s", driverType,
               driverName.c_str());
        SAFE_DELETE(client);
        continue;
      }

      driver->ReadLogInitialisation();

      RenderDoc::Inst().SetProgressPtr(NULL);

      data.killsignal = true;
      Threading::JoinThread(ticker);
      Threading::CloseThread(ticker);

      FileIO::Delete(cap_file.c_str());

      SendPacket(client, eRemoteServer_LogReady);

      ProxySerialiser *proxy = new ProxySerialiser(client, driver);

      while(client)
      {
        int packet;
        Serialiser *data = NULL;

        if(!RecvPacket(client, packet, &data))
        {
          SAFE_DELETE(data);
          SAFE_DELETE(client);
          break;
        }

        if(!proxy->Tick(packet, data) || killReplay)
        {
          SAFE_DELETE(data);
          SAFE_DELETE(client);
        }
      }

      driver->Shutdown();

      RDCLOG("Closing replay connection");

      SAFE_DELETE(proxy);
      SAFE_DELETE(client);
    }
    else
    {
      RDCERR("File needs driver for %s which isn't supported!", driverName.c_str());

      FileIO::Delete(cap_file.c_str());
    }

    SAFE_DELETE(client);
  }

  SAFE_DELETE(sock);
}

struct RemoteRenderer : public IRemoteRenderer
{
public:
  RemoteRenderer(Network::Socket *sock) : m_Socket(sock)
  {
    map<RDCDriver, string> m = RenderDoc::Inst().GetReplayDrivers();

    m_Proxies.reserve(m.size());
    for(auto it = m.begin(); it != m.end(); ++it)
      m_Proxies.push_back(*it);

    {
      RemoteServerPacket type;
      Serialiser *ser = NULL;
      GetPacket(type, &ser);

      m.clear();

      if(ser)
      {
        uint32_t count = 0;
        ser->Serialise("", count);

        for(uint32_t i = 0; i < count; i++)
        {
          RDCDriver driver = RDC_Unknown;
          string name = "";
          ser->Serialise("", driver);
          ser->Serialise("", name);

          m[driver] = name;
        }

        delete ser;
      }
    }

    m_RemoteDrivers.reserve(m.size());
    for(auto it = m.begin(); it != m.end(); ++it)
      m_RemoteDrivers.push_back(*it);
  }
  virtual ~RemoteRenderer() { SAFE_DELETE(m_Socket); }
  void Shutdown() { delete this; }
  bool Connected() { return m_Socket != NULL && m_Socket->Connected(); }
  bool LocalProxies(rdctype::array<rdctype::str> *out)
  {
    if(out == NULL)
      return false;

    create_array_uninit(*out, m_Proxies.size());

    size_t i = 0;
    for(auto it = m_Proxies.begin(); it != m_Proxies.end(); ++it, ++i)
      out->elems[i] = it->second;

    return true;
  }

  bool RemoteSupportedReplays(rdctype::array<rdctype::str> *out)
  {
    if(out == NULL)
      return false;

    create_array_uninit(*out, m_RemoteDrivers.size());

    size_t i = 0;
    for(auto it = m_RemoteDrivers.begin(); it != m_RemoteDrivers.end(); ++it, ++i)
      out->elems[i] = it->second;

    return true;
  }

  ReplayCreateStatus CreateProxyRenderer(uint32_t proxyid, const char *logfile, float *progress,
                                         ReplayRenderer **rend)
  {
    if(rend == NULL)
      return eReplayCreate_InternalError;

    if(proxyid >= m_Proxies.size())
    {
      RDCERR("Invalid proxy driver id %d specified for remote renderer", proxyid);
      return eReplayCreate_InternalError;
    }

    float dummy = 0.0f;
    if(progress == NULL)
      progress = &dummy;

    RDCDriver proxydrivertype = m_Proxies[proxyid].first;

    Serialiser ser("", Serialiser::WRITING, false);

    if(!SendChunkedFile(m_Socket, eRemoteServer_CopyCapture, logfile, ser, progress))
    {
      SAFE_DELETE(m_Socket);
      return eReplayCreate_NetworkIOFailed;
    }

    RDCLOG("Sent file to replay host. Loading...");

    RemoteServerPacket type = eRemoteServer_Noop;
    while(m_Socket)
    {
      Serialiser *progressSer;
      GetPacket(type, &progressSer);

      if(!m_Socket || type != eRemoteServer_LogOpenProgress)
        break;

      progressSer->Serialise("", *progress);

      RDCLOG("% 3.0f%%...", (*progress) * 100.0f);
    }

    if(!m_Socket || type != eRemoteServer_LogReady)
      return eReplayCreate_NetworkIOFailed;

    *progress = 1.0f;

    RDCLOG("Log ready on replay host");

    IReplayDriver *proxyDriver = NULL;
    auto status = RenderDoc::Inst().CreateReplayDriver(proxydrivertype, NULL, &proxyDriver);

    if(status != eReplayCreate_Success || !proxyDriver)
    {
      if(proxyDriver)
        proxyDriver->Shutdown();
      return status;
    }

    ReplayRenderer *ret = new ReplayRenderer();

    ProxySerialiser *proxy = new ProxySerialiser(m_Socket, proxyDriver);
    status = ret->SetDevice(proxy);

    if(status != eReplayCreate_Success)
    {
      SAFE_DELETE(ret);
      return status;
    }

    // ReplayRenderer takes ownership of the ProxySerialiser (as IReplayDriver)
    // and it cleans itself up in Shutdown.

    *rend = ret;

    return eReplayCreate_Success;
  }

private:
  Network::Socket *m_Socket;

  void GetPacket(RemoteServerPacket &type, Serialiser **ser)
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
  vector<pair<RDCDriver, string> > m_RemoteDrivers;
};

extern "C" RENDERDOC_API void RENDERDOC_CC RemoteRenderer_Shutdown(RemoteRenderer *remote)
{
  remote->Shutdown();
}

extern "C" RENDERDOC_API bool32 RENDERDOC_CC
RemoteRenderer_LocalProxies(RemoteRenderer *remote, rdctype::array<rdctype::str> *out)
{
  return remote->LocalProxies(out);
}

extern "C" RENDERDOC_API bool32 RENDERDOC_CC
RemoteRenderer_RemoteSupportedReplays(RemoteRenderer *remote, rdctype::array<rdctype::str> *out)
{
  return remote->RemoteSupportedReplays(out);
}

extern "C" RENDERDOC_API ReplayCreateStatus RENDERDOC_CC
RemoteRenderer_CreateProxyRenderer(RemoteRenderer *remote, uint32_t proxyid, const char *logfile,
                                   float *progress, ReplayRenderer **rend)
{
  return remote->CreateProxyRenderer(proxyid, logfile, progress, rend);
}

extern "C" RENDERDOC_API ReplayCreateStatus RENDERDOC_CC
RENDERDOC_CreateRemoteReplayConnection(const char *host, uint32_t port, RemoteRenderer **rend)
{
  if(rend == NULL)
    return eReplayCreate_InternalError;

  string s = "localhost";
  if(host != NULL && host[0] != '\0')
    s = host;

  if(port == 0)
    port = RENDERDOC_GetDefaultRemoteServerPort();

  Network::Socket *sock = NULL;

  if(s != "-")
  {
    sock = Network::CreateClientSocket(s.c_str(), (uint16_t)port, 3000);

    if(sock == NULL)
      return eReplayCreate_NetworkIOFailed;
  }

  *rend = new RemoteRenderer(sock);

  return eReplayCreate_Success;
}
