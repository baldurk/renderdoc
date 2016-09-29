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

#include "api/replay/renderdoc_replay.h"
#include "core/core.h"
#include "os/os_specific.h"
#include "replay/type_helpers.h"
#include "serialise/serialiser.h"
#include "socket_helpers.h"

enum PacketType
{
  ePacket_Noop,
  ePacket_Handshake,
  ePacket_Busy,
  ePacket_NewCapture,
  ePacket_RegisterAPI,
  ePacket_TriggerCapture,
  ePacket_CopyCapture,
  ePacket_DeleteCapture,
  ePacket_QueueCapture,
  ePacket_NewChild,
};

void RenderDoc::TargetControlClientThread(void *s)
{
  Threading::KeepModuleAlive();

  Network::Socket *client = (Network::Socket *)s;

  Serialiser ser("", Serialiser::WRITING, false);

  string api = "";
  RDCDriver driver;
  RenderDoc::Inst().GetCurrentDriver(driver, api);

  ser.Rewind();

  string target = RenderDoc::Inst().GetCurrentTarget();
  ser.Serialise("", target);
  ser.Serialise("", api);
  uint32_t mypid = Process::GetCurrentPID();
  ser.Serialise("", mypid);

  if(!SendPacket(client, ePacket_Handshake, ser))
  {
    SAFE_DELETE(client);

    {
      SCOPED_LOCK(RenderDoc::Inst().m_SingleClientLock);
      RenderDoc::Inst().m_SingleClientName = "";
    }

    Threading::ReleaseModuleExitThread();
    return;
  }

  const int pingtime = 1000;    // ping every 1000ms
  const int ticktime = 10;      // tick every 10ms
  int curtime = 0;

  vector<CaptureData> captures;
  vector<pair<uint32_t, uint32_t> > children;

  while(client)
  {
    if(RenderDoc::Inst().m_ControlClientThreadShutdown || (client && !client->Connected()))
    {
      SAFE_DELETE(client);
      break;
    }

    ser.Rewind();

    Threading::Sleep(ticktime);
    curtime += ticktime;

    PacketType packetType = ePacket_Noop;

    string curapi;
    RenderDoc::Inst().GetCurrentDriver(driver, curapi);

    vector<CaptureData> caps = RenderDoc::Inst().GetCaptures();
    vector<pair<uint32_t, uint32_t> > childprocs = RenderDoc::Inst().GetChildProcesses();

    if(curapi != api)
    {
      api = curapi;

      ser.Serialise("", api);

      packetType = ePacket_RegisterAPI;
    }
    else if(caps.size() != captures.size())
    {
      uint32_t idx = (uint32_t)captures.size();

      captures.push_back(caps[idx]);

      packetType = ePacket_NewCapture;

      std::string path = FileIO::GetFullPathname(captures.back().path);

      ser.Serialise("", idx);
      ser.Serialise("", captures.back().timestamp);
      ser.Serialise("", path);

      uint32_t len = 0;
      RENDERDOC_GetThumbnail(captures.back().path.c_str(), NULL, len);
      byte *thumb = new byte[len];
      RENDERDOC_GetThumbnail(captures.back().path.c_str(), thumb, len);

      size_t l = len;
      ser.Serialise("", len);
      ser.SerialiseBuffer("", thumb, l);
      delete[] thumb;
    }
    else if(childprocs.size() != children.size())
    {
      uint32_t idx = (uint32_t)children.size();

      children.push_back(childprocs[idx]);

      packetType = ePacket_NewChild;

      ser.Serialise("", children.back().first);
      ser.Serialise("", children.back().second);
    }

    if(curtime < pingtime && packetType == ePacket_Noop)
    {
      if(client->IsRecvDataWaiting())
      {
        PacketType type;
        Serialiser *recvser = NULL;

        if(!RecvPacket(client, type, &recvser))
          SAFE_DELETE(client);

        if(client == NULL)
        {
          SAFE_DELETE(recvser);
          continue;
        }
        else if(type == ePacket_TriggerCapture)
        {
          uint32_t numFrames = 0;
          recvser->Serialise("", numFrames);

          RenderDoc::Inst().TriggerCapture(numFrames);
        }
        else if(type == ePacket_QueueCapture)
        {
          uint32_t frameNum = 0;
          recvser->Serialise("", frameNum);

          RenderDoc::Inst().QueueCapture(frameNum);
        }
        else if(type == ePacket_DeleteCapture)
        {
          uint32_t id = 0;
          recvser->Serialise("", id);

          // this means it will be deleted on shutdown
          RenderDoc::Inst().MarkCaptureRetrieved(id);
        }
        else if(type == ePacket_CopyCapture)
        {
          caps = RenderDoc::Inst().GetCaptures();

          uint32_t id = 0;
          recvser->Serialise("", id);

          if(id < caps.size())
          {
            ser.Serialise("", id);

            if(!SendPacket(client, ePacket_CopyCapture, ser))
            {
              SAFE_DELETE(client);
              continue;
            }

            ser.Rewind();

            if(!SendChunkedFile(client, ePacket_CopyCapture, caps[id].path.c_str(), ser, NULL))
            {
              SAFE_DELETE(client);
              continue;
            }

            RenderDoc::Inst().MarkCaptureRetrieved(id);
          }
        }

        SAFE_DELETE(recvser);
      }

      continue;
    }

    curtime = 0;

    if(!SendPacket(client, packetType, ser))
    {
      SAFE_DELETE(client);
      continue;
    }
  }

  // give up our connection
  {
    SCOPED_LOCK(RenderDoc::Inst().m_SingleClientLock);
    RenderDoc::Inst().m_SingleClientName = "";
  }

  Threading::ReleaseModuleExitThread();
}

void RenderDoc::TargetControlServerThread(void *s)
{
  Threading::KeepModuleAlive();

  Network::Socket *sock = (Network::Socket *)s;

  RenderDoc::Inst().m_SingleClientName = "";

  Threading::ThreadHandle clientThread = 0;

  RenderDoc::Inst().m_ControlClientThreadShutdown = false;

  while(!RenderDoc::Inst().m_TargetControlThreadShutdown)
  {
    Network::Socket *client = sock->AcceptClient(false);

    if(client == NULL)
    {
      if(!sock->Connected())
      {
        RDCERR("Error in accept - shutting down server");

        SAFE_DELETE(sock);
        Threading::ReleaseModuleExitThread();
        return;
      }

      Threading::Sleep(5);

      continue;
    }

    string existingClient;
    string newClient;
    bool kick = false;

    // receive handshake from client and get its name
    {
      PacketType type;
      Serialiser *ser = NULL;
      if(!RecvPacket(client, type, &ser))
      {
        SAFE_DELETE(ser);
        SAFE_DELETE(client);
        continue;
      }

      if(type != ePacket_Handshake)
      {
        SAFE_DELETE(ser);
        SAFE_DELETE(client);
        continue;
      }

      ser->SerialiseString("", newClient);
      ser->Serialise("", kick);

      SAFE_DELETE(ser);

      if(newClient.empty())
      {
        SAFE_DELETE(client);
        continue;
      }
    }

    // see if we have a client
    {
      SCOPED_LOCK(RenderDoc::Inst().m_SingleClientLock);
      existingClient = RenderDoc::Inst().m_SingleClientName;
    }

    if(!existingClient.empty() && kick)
    {
      // forcibly close communication thread which will kill the connection
      RenderDoc::Inst().m_ControlClientThreadShutdown = true;
      Threading::JoinThread(clientThread);
      Threading::CloseThread(clientThread);
      clientThread = 0;
      RenderDoc::Inst().m_ControlClientThreadShutdown = false;
      existingClient = "";
    }

    if(existingClient.empty())
    {
      SCOPED_LOCK(RenderDoc::Inst().m_SingleClientLock);
      RenderDoc::Inst().m_SingleClientName = newClient;
    }

    // if we've claimed client status, spawn a thread to communicate
    if(existingClient.empty() || kick)
    {
      clientThread = Threading::CreateThread(TargetControlClientThread, client);
      continue;
    }
    else
    {
      // if we've been asked to kick the existing connection off
      // reject this connection and tell them who is busy
      Serialiser ser("", Serialiser::WRITING, false);

      string api = "";
      RDCDriver driver;
      RenderDoc::Inst().GetCurrentDriver(driver, api);

      string target = RenderDoc::Inst().GetCurrentTarget();
      ser.Serialise("", target);
      ser.Serialise("", api);

      ser.SerialiseString("", RenderDoc::Inst().m_SingleClientName);

      // don't care about errors, we're going to close the connection either way
      SendPacket(client, ePacket_Busy, ser);

      SAFE_DELETE(client);
    }
  }

  RenderDoc::Inst().m_ControlClientThreadShutdown = true;
  // don't join, just close the thread, as we can't wait while in the middle of module unloading
  Threading::CloseThread(clientThread);
  clientThread = 0;

  SAFE_DELETE(sock);

  Threading::ReleaseModuleExitThread();
}

struct TargetControl : public ITargetControl
{
public:
  TargetControl(Network::Socket *sock, string clientName, bool forceConnection, bool localhost)
      : m_Socket(sock), m_Local(localhost)
  {
    PacketType type;
    vector<byte> payload;

    m_PID = 0;

    {
      Serialiser ser("", Serialiser::WRITING, false);

      ser.SerialiseString("", clientName);
      ser.Serialise("", forceConnection);

      if(!SendPacket(m_Socket, ePacket_Handshake, ser))
      {
        SAFE_DELETE(m_Socket);
        return;
      }
    }

    Serialiser *ser = NULL;
    GetPacket(type, ser);

    // failed handshaking
    if(m_Socket == NULL || ser == NULL)
      return;

    RDCASSERT(type == ePacket_Handshake || type == ePacket_Busy);

    if(type == ePacket_Handshake)
    {
      ser->Serialise("", m_Target);
      ser->Serialise("", m_API);
      ser->Serialise("", m_PID);

      RDCLOG("Got remote handshake: %s (%s) [%u]", m_Target.c_str(), m_API.c_str(), m_PID);
    }
    else if(type == ePacket_Busy)
    {
      ser->Serialise("", m_Target);
      ser->Serialise("", m_API);
      ser->Serialise("", m_BusyClient);

      RDCLOG("Got remote busy signal: %s (%s) owned by %s", m_Target.c_str(), m_API.c_str(),
             m_BusyClient.c_str());
    }

    SAFE_DELETE(ser);
  }

  virtual ~TargetControl() {}
  bool Connected() { return m_Socket != NULL && m_Socket->Connected(); }
  void Shutdown()
  {
    SAFE_DELETE(m_Socket);
    delete this;
  }

  const char *GetTarget() { return m_Target.c_str(); }
  const char *GetAPI() { return m_API.c_str(); }
  uint32_t GetPID() { return m_PID; }
  const char *GetBusyClient() { return m_BusyClient.c_str(); }
  void TriggerCapture(uint32_t numFrames)
  {
    Serialiser ser("", Serialiser::WRITING, false);

    ser.Serialise("", numFrames);

    if(!SendPacket(m_Socket, ePacket_TriggerCapture, ser))
    {
      SAFE_DELETE(m_Socket);
      return;
    }
  }

  void QueueCapture(uint32_t frameNumber)
  {
    Serialiser ser("", Serialiser::WRITING, false);

    ser.Serialise("", frameNumber);

    if(!SendPacket(m_Socket, ePacket_QueueCapture, ser))
    {
      SAFE_DELETE(m_Socket);
      return;
    }
  }

  void CopyCapture(uint32_t remoteID, const char *localpath)
  {
    Serialiser ser("", Serialiser::WRITING, false);

    ser.Serialise("", remoteID);

    if(!SendPacket(m_Socket, ePacket_CopyCapture, ser))
    {
      SAFE_DELETE(m_Socket);
      return;
    }

    m_CaptureCopies[remoteID] = localpath;
  }

  void DeleteCapture(uint32_t remoteID)
  {
    Serialiser ser("", Serialiser::WRITING, false);

    ser.Serialise("", remoteID);

    if(!SendPacket(m_Socket, ePacket_DeleteCapture, ser))
    {
      SAFE_DELETE(m_Socket);
      return;
    }
  }

  void ReceiveMessage(TargetControlMessage *msg)
  {
    if(m_Socket == NULL)
    {
      msg->Type = eTargetControlMsg_Disconnected;
      return;
    }

    if(!m_Socket->IsRecvDataWaiting())
    {
      if(!m_Socket->Connected())
      {
        SAFE_DELETE(m_Socket);
        msg->Type = eTargetControlMsg_Disconnected;
      }
      else
      {
        Threading::Sleep(2);
        msg->Type = eTargetControlMsg_Noop;
      }

      return;
    }

    PacketType type;
    Serialiser *ser = NULL;

    GetPacket(type, ser);

    if(m_Socket == NULL)
    {
      SAFE_DELETE(ser);

      msg->Type = eTargetControlMsg_Disconnected;
      return;
    }
    else
    {
      if(type == ePacket_Noop)
      {
        SAFE_DELETE(ser);

        msg->Type = eTargetControlMsg_Noop;
        return;
      }
      else if(type == ePacket_Busy)
      {
        string existingClient;
        ser->Serialise("", existingClient);

        SAFE_DELETE(ser);

        SAFE_DELETE(m_Socket);

        RDCLOG("Got busy signal: '%s", existingClient.c_str());
        msg->Type = eTargetControlMsg_Busy;
        msg->Busy.ClientName = existingClient;
        return;
      }
      else if(type == ePacket_CopyCapture)
      {
        msg->Type = eTargetControlMsg_CaptureCopied;

        ser->Serialise("", msg->NewCapture.ID);

        SAFE_DELETE(ser);

        msg->NewCapture.path = m_CaptureCopies[msg->NewCapture.ID];

        if(!RecvChunkedFile(m_Socket, ePacket_CopyCapture, msg->NewCapture.path.elems, ser, NULL))
        {
          SAFE_DELETE(ser);
          SAFE_DELETE(m_Socket);

          msg->Type = eTargetControlMsg_Disconnected;
          return;
        }

        m_CaptureCopies.erase(msg->NewCapture.ID);

        SAFE_DELETE(ser);

        return;
      }
      else if(type == ePacket_NewChild)
      {
        msg->Type = eTargetControlMsg_NewChild;

        ser->Serialise("", msg->NewChild.PID);
        ser->Serialise("", msg->NewChild.ident);

        RDCLOG("Got a new child process: %u %u", msg->NewChild.PID, msg->NewChild.ident);

        SAFE_DELETE(ser);

        return;
      }
      else if(type == ePacket_NewCapture)
      {
        msg->Type = eTargetControlMsg_NewCapture;

        ser->Serialise("", msg->NewCapture.ID);
        ser->Serialise("", msg->NewCapture.timestamp);

        string path;
        ser->Serialise("", path);
        msg->NewCapture.path = path;
        msg->NewCapture.local = m_Local;

        uint32_t thumblen = 0;
        ser->Serialise("", thumblen);

        create_array_uninit(msg->NewCapture.thumbnail, thumblen);

        size_t l = 0;
        byte *buf = &msg->NewCapture.thumbnail[0];
        ser->SerialiseBuffer("", buf, l);

        RDCLOG("Got a new capture: %d (time %llu) %d byte thumbnail", msg->NewCapture.ID,
               msg->NewCapture.timestamp, thumblen);

        SAFE_DELETE(ser);

        return;
      }
      else if(type == ePacket_RegisterAPI)
      {
        msg->Type = eTargetControlMsg_RegisterAPI;

        ser->Serialise("", m_API);
        msg->RegisterAPI.APIName = m_API;

        RDCLOG("Used API: %s", m_API.c_str());

        SAFE_DELETE(ser);

        return;
      }
    }

    SAFE_DELETE(ser);

    msg->Type = eTargetControlMsg_Noop;
  }

private:
  Network::Socket *m_Socket;
  bool m_Local;
  string m_Target, m_API, m_BusyClient;
  uint32_t m_PID;

  map<uint32_t, string> m_CaptureCopies;

  void GetPacket(PacketType &type, Serialiser *&ser)
  {
    if(!RecvPacket(m_Socket, type, &ser))
      SAFE_DELETE(m_Socket);
  }
};

extern "C" RENDERDOC_API void RENDERDOC_CC TargetControl_Shutdown(TargetControl *control)
{
  control->Shutdown();
}

extern "C" RENDERDOC_API const char *RENDERDOC_CC TargetControl_GetTarget(TargetControl *control)
{
  return control->GetTarget();
}
extern "C" RENDERDOC_API const char *RENDERDOC_CC TargetControl_GetAPI(TargetControl *control)
{
  return control->GetAPI();
}
extern "C" RENDERDOC_API uint32_t RENDERDOC_CC TargetControl_GetPID(TargetControl *control)
{
  return control->GetPID();
}
extern "C" RENDERDOC_API const char *RENDERDOC_CC TargetControl_GetBusyClient(TargetControl *control)
{
  return control->GetBusyClient();
}

extern "C" RENDERDOC_API void RENDERDOC_CC TargetControl_TriggerCapture(TargetControl *control,
                                                                        uint32_t numFrames)
{
  control->TriggerCapture(numFrames);
}
extern "C" RENDERDOC_API void RENDERDOC_CC TargetControl_QueueCapture(TargetControl *control,
                                                                      uint32_t frameNumber)
{
  control->QueueCapture(frameNumber);
}
extern "C" RENDERDOC_API void RENDERDOC_CC TargetControl_CopyCapture(TargetControl *control,
                                                                     uint32_t remoteID,
                                                                     const char *localpath)
{
  control->CopyCapture(remoteID, localpath);
}

extern "C" RENDERDOC_API void RENDERDOC_CC TargetControl_DeleteCapture(TargetControl *control,
                                                                       uint32_t remoteID)
{
  control->DeleteCapture(remoteID);
}

extern "C" RENDERDOC_API void RENDERDOC_CC TargetControl_ReceiveMessage(TargetControl *control,
                                                                        TargetControlMessage *msg)
{
  control->ReceiveMessage(msg);
}

extern "C" RENDERDOC_API TargetControl *RENDERDOC_CC RENDERDOC_CreateTargetControl(
    const char *host, uint32_t ident, const char *clientName, bool32 forceConnection)
{
  string s = "localhost";
  if(host != NULL && host[0] != '\0')
    s = host;

  bool android = false;

  if(host != NULL && !strncmp(host, "adb:", 4))
  {
    android = true;
    s = "127.0.0.1";

    // could parse out an (optional) device name from host+4 here.
  }

  Network::Socket *sock = Network::CreateClientSocket(s.c_str(), ident & 0xffff, 750);

  if(sock == NULL)
    return NULL;

  bool localhost = !android && (Network::GetIPOctet(sock->GetRemoteIP(), 0) == 127);

  TargetControl *remote = new TargetControl(sock, clientName, forceConnection != 0, localhost);

  if(remote->Connected())
    return remote;

  delete remote;
  return NULL;
}
