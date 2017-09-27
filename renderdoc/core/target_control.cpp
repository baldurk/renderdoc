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

#include "api/replay/renderdoc_replay.h"
#include "core/android.h"
#include "core/core.h"
#include "jpeg-compressor/jpgd.h"
#include "os/os_specific.h"
#include "serialise/serialiser.h"

enum PacketType : uint32_t
{
  ePacket_Noop = 1,
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

DECLARE_REFLECTION_ENUM(PacketType);

template <>
std::string DoStringise(const PacketType &el)
{
  BEGIN_ENUM_STRINGISE(PacketType);
  {
    STRINGISE_ENUM_NAMED(ePacket_Noop, "No-op");
    STRINGISE_ENUM_NAMED(ePacket_Handshake, "Handshake");
    STRINGISE_ENUM_NAMED(ePacket_Busy, "Busy");
    STRINGISE_ENUM_NAMED(ePacket_NewCapture, "New Capture");
    STRINGISE_ENUM_NAMED(ePacket_RegisterAPI, "Register API");
    STRINGISE_ENUM_NAMED(ePacket_TriggerCapture, "Trigger Capture");
    STRINGISE_ENUM_NAMED(ePacket_CopyCapture, "Copy Capture");
    STRINGISE_ENUM_NAMED(ePacket_DeleteCapture, "Delete Capture");
    STRINGISE_ENUM_NAMED(ePacket_QueueCapture, "Queue Capture");
    STRINGISE_ENUM_NAMED(ePacket_NewChild, "New Child");
  }
  END_ENUM_STRINGISE();
}

#define WRITE_DATA_SCOPE() WriteSerialiser &ser = writer;
#define READ_DATA_SCOPE() ReadSerialiser &ser = reader;

void RenderDoc::TargetControlClientThread(Network::Socket *client)
{
  Threading::KeepModuleAlive();

  WriteSerialiser writer(new StreamWriter(client, Ownership::Nothing), Ownership::Stream);
  ReadSerialiser reader(new StreamReader(client, Ownership::Nothing), Ownership::Stream);

  writer.SetStreamingMode(true);

  std::string api = "";
  RDCDriver driver;
  RenderDoc::Inst().GetCurrentDriver(driver, api);

  std::string target = RenderDoc::Inst().GetCurrentTarget();
  uint32_t mypid = Process::GetCurrentPID();

  {
    WRITE_DATA_SCOPE();
    SCOPED_SERIALISE_CHUNK(ePacket_Handshake);
    SERIALISE_ELEMENT(target);
    SERIALISE_ELEMENT(api);
    SERIALISE_ELEMENT(mypid);
  }

  if(writer.IsErrored())
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

  std::vector<CaptureData> captures;
  std::vector<pair<uint32_t, uint32_t> > children;

  while(client)
  {
    if(RenderDoc::Inst().m_ControlClientThreadShutdown || (client && !client->Connected()))
    {
      SAFE_DELETE(client);
      break;
    }

    Threading::Sleep(ticktime);
    curtime += ticktime;

    std::string curapi;
    RenderDoc::Inst().GetCurrentDriver(driver, curapi);

    std::vector<CaptureData> caps = RenderDoc::Inst().GetCaptures();
    std::vector<pair<uint32_t, uint32_t> > childprocs = RenderDoc::Inst().GetChildProcesses();

    if(curapi != api)
    {
      api = curapi;

      WRITE_DATA_SCOPE();
      {
        SCOPED_SERIALISE_CHUNK(ePacket_RegisterAPI);
        SERIALISE_ELEMENT(api);
      }
    }
    else if(caps.size() != captures.size())
    {
      uint32_t idx = (uint32_t)captures.size();

      captures.push_back(caps[idx]);

      std::string path = FileIO::GetFullPathname(captures.back().path);

      bytebuf buf;

      ICaptureFile *file = RENDERDOC_OpenCaptureFile(captures.back().path.c_str());
      if(file->OpenStatus() == ReplayStatus::Succeeded)
      {
        buf = file->GetThumbnail(FileType::JPG, 0);
      }
      file->Shutdown();

      WRITE_DATA_SCOPE();
      {
        SCOPED_SERIALISE_CHUNK(ePacket_NewCapture);
        SERIALISE_ELEMENT(idx);
        SERIALISE_ELEMENT(captures.back().timestamp);
        SERIALISE_ELEMENT(path);
        SERIALISE_ELEMENT(buf);
      }
    }
    else if(childprocs.size() != children.size())
    {
      uint32_t idx = (uint32_t)children.size();

      children.push_back(childprocs[idx]);

      WRITE_DATA_SCOPE();
      {
        SCOPED_SERIALISE_CHUNK(ePacket_NewChild);
        SERIALISE_ELEMENT(children.back().first);
        SERIALISE_ELEMENT(children.back().second);
      }
    }

    if(curtime > pingtime)
    {
      WRITE_DATA_SCOPE();
      {
        SCOPED_SERIALISE_CHUNK(ePacket_Noop);
      }
      curtime = 0;
    }

    if(writer.IsErrored())
    {
      SAFE_DELETE(client);
      continue;
    }

    if(client->IsRecvDataWaiting())
    {
      PacketType type = (PacketType)reader.BeginChunk(0);

      if(type == ePacket_TriggerCapture)
      {
        uint32_t numFrames;

        READ_DATA_SCOPE();
        SERIALISE_ELEMENT(numFrames);

        RenderDoc::Inst().TriggerCapture(numFrames);
      }
      else if(type == ePacket_QueueCapture)
      {
        uint32_t frameNum;

        READ_DATA_SCOPE();
        SERIALISE_ELEMENT(frameNum);

        RenderDoc::Inst().QueueCapture(frameNum);
      }
      else if(type == ePacket_DeleteCapture)
      {
        uint32_t id;

        READ_DATA_SCOPE();
        SERIALISE_ELEMENT(id);

        // this means it will be deleted on shutdown
        RenderDoc::Inst().MarkCaptureRetrieved(id);
      }
      else if(type == ePacket_CopyCapture)
      {
        caps = RenderDoc::Inst().GetCaptures();

        uint32_t id;

        {
          READ_DATA_SCOPE();
          SERIALISE_ELEMENT(id);
        }

        if(id < caps.size())
        {
          WRITE_DATA_SCOPE();
          SCOPED_SERIALISE_CHUNK(ePacket_CopyCapture);
          SERIALISE_ELEMENT(id);

          std::string filename = caps[id].path;

          StreamReader fileStream(FileIO::fopen(filename.c_str(), "rb"));
          ser.SerialiseStream(filename, fileStream);

          if(fileStream.IsErrored() || ser.IsErrored())
            SAFE_DELETE(client);
          else
            RenderDoc::Inst().MarkCaptureRetrieved(id);
        }
      }

      reader.EndChunk();

      if(reader.IsErrored())
        SAFE_DELETE(client);
    }
  }

  // give up our connection
  {
    SCOPED_LOCK(RenderDoc::Inst().m_SingleClientLock);
    RenderDoc::Inst().m_SingleClientName = "";
  }

  Threading::ReleaseModuleExitThread();
}

void RenderDoc::TargetControlServerThread(Network::Socket *sock)
{
  Threading::KeepModuleAlive();

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

    std::string existingClient;
    std::string newClient;
    bool kick = false;

    // receive handshake from client and get its name
    {
      ReadSerialiser ser(new StreamReader(client, Ownership::Nothing), Ownership::Stream);

      PacketType type = (PacketType)ser.BeginChunk(0);

      if(type != ePacket_Handshake)
      {
        SAFE_DELETE(client);
        continue;
      }

      SERIALISE_ELEMENT(newClient);
      SERIALISE_ELEMENT(kick);

      ser.EndChunk();

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
      clientThread = Threading::CreateThread([client] { TargetControlClientThread(client); });
      continue;
    }
    else
    {
      // if we've been asked to kick the existing connection off
      // reject this connection and tell them who is busy
      WriteSerialiser ser(new StreamWriter(client, Ownership::Nothing), Ownership::Stream);

      ser.SetStreamingMode(true);

      std::string api = "";
      RDCDriver driver;
      RenderDoc::Inst().GetCurrentDriver(driver, api);

      std::string target = RenderDoc::Inst().GetCurrentTarget();
      {
        SCOPED_SERIALISE_CHUNK(ePacket_Busy);
        SERIALISE_ELEMENT(target);
        SERIALISE_ELEMENT(api);
        SERIALISE_ELEMENT(RenderDoc::Inst().m_SingleClientName);
      }

      // don't care about errors, we're going to close the connection either way
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
  TargetControl(Network::Socket *sock, std::string clientName, bool forceConnection)
      : m_Socket(sock),
        reader(new StreamReader(sock, Ownership::Nothing), Ownership::Stream),
        writer(new StreamWriter(sock, Ownership::Nothing), Ownership::Stream)
  {
    std::vector<byte> payload;

    writer.SetStreamingMode(true);

    m_PID = 0;

    {
      WRITE_DATA_SCOPE();

      {
        SCOPED_SERIALISE_CHUNK(ePacket_Handshake);
        SERIALISE_ELEMENT(clientName);
        SERIALISE_ELEMENT(forceConnection);
      }

      if(writer.IsErrored())
      {
        SAFE_DELETE(m_Socket);
        return;
      }
    }

    PacketType type = (PacketType)reader.BeginChunk(0);

    if(reader.IsErrored())
    {
      SAFE_DELETE(m_Socket);
      return;
    }

    if(type != ePacket_Handshake && type != ePacket_Busy)
    {
      RDCERR("Expected handshake packet, got %d", type);
      SAFE_DELETE(m_Socket);
    }

    // failed handshaking
    if(m_Socket == NULL)
      return;

    {
      READ_DATA_SCOPE();
      SERIALISE_ELEMENT(m_Target);
      SERIALISE_ELEMENT(m_API);
      SERIALISE_ELEMENT(m_PID);
    }

    reader.EndChunk();

    if(type == ePacket_Handshake)
    {
      RDCLOG("Got remote handshake: %s (%s) [%u]", m_Target.c_str(), m_API.c_str(), m_PID);
    }
    else if(type == ePacket_Busy)
    {
      RDCLOG("Got remote busy signal: %s (%s) owned by %s", m_Target.c_str(), m_API.c_str(),
             m_BusyClient.c_str());
    }
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
    WRITE_DATA_SCOPE();
    SCOPED_SERIALISE_CHUNK(ePacket_TriggerCapture);

    SERIALISE_ELEMENT(numFrames);

    if(ser.IsErrored())
      SAFE_DELETE(m_Socket);
  }

  void QueueCapture(uint32_t frameNumber)
  {
    WRITE_DATA_SCOPE();
    SCOPED_SERIALISE_CHUNK(ePacket_QueueCapture);

    SERIALISE_ELEMENT(frameNumber);

    if(ser.IsErrored())
      SAFE_DELETE(m_Socket);
  }

  void CopyCapture(uint32_t remoteID, const char *localpath)
  {
    WRITE_DATA_SCOPE();
    SCOPED_SERIALISE_CHUNK(ePacket_CopyCapture);

    SERIALISE_ELEMENT(remoteID);

    if(ser.IsErrored())
    {
      SAFE_DELETE(m_Socket);
      return;
    }

    m_CaptureCopies[remoteID] = localpath;
  }

  void DeleteCapture(uint32_t remoteID)
  {
    WRITE_DATA_SCOPE();
    SCOPED_SERIALISE_CHUNK(ePacket_DeleteCapture);

    SERIALISE_ELEMENT(remoteID);

    if(ser.IsErrored())
      SAFE_DELETE(m_Socket);
  }

  TargetControlMessage ReceiveMessage()
  {
    TargetControlMessage msg;
    if(m_Socket == NULL)
    {
      msg.Type = TargetControlMessageType::Disconnected;
      return msg;
    }

    if(!m_Socket->IsRecvDataWaiting())
    {
      if(!m_Socket->Connected())
      {
        SAFE_DELETE(m_Socket);
        msg.Type = TargetControlMessageType::Disconnected;
      }
      else
      {
        Threading::Sleep(2);
        msg.Type = TargetControlMessageType::Noop;
      }

      return msg;
    }

    PacketType type = (PacketType)reader.BeginChunk(0);

    if(reader.IsErrored())
    {
      SAFE_DELETE(m_Socket);

      msg.Type = TargetControlMessageType::Disconnected;
      return msg;
    }
    else if(type == ePacket_Noop)
    {
      msg.Type = TargetControlMessageType::Noop;
      reader.EndChunk();
      return msg;
    }
    else if(type == ePacket_Busy)
    {
      READ_DATA_SCOPE();
      SERIALISE_ELEMENT(msg.Busy.ClientName).Named("Client Name");

      SAFE_DELETE(m_Socket);

      RDCLOG("Got busy signal: '%s", msg.Busy.ClientName.c_str());
      msg.Type = TargetControlMessageType::Busy;
      return msg;
    }
    else if(type == ePacket_NewChild)
    {
      msg.Type = TargetControlMessageType::NewChild;

      READ_DATA_SCOPE();
      SERIALISE_ELEMENT(msg.NewChild.PID).Named("PID");
      SERIALISE_ELEMENT(msg.NewChild.ident).Named("Child ident");

      RDCLOG("Got a new child process: %u %u", msg.NewChild.PID, msg.NewChild.ident);

      reader.EndChunk();
      return msg;
    }
    else if(type == ePacket_NewCapture)
    {
      msg.Type = TargetControlMessageType::NewCapture;

      bytebuf thumbnail;

      {
        READ_DATA_SCOPE();
        SERIALISE_ELEMENT(msg.NewCapture.ID).Named("Capture ID");
        SERIALISE_ELEMENT(msg.NewCapture.timestamp).Named("timestamp");
        SERIALISE_ELEMENT(msg.NewCapture.path).Named("path");
        SERIALISE_ELEMENT(thumbnail);
      }

      msg.NewCapture.local = FileIO::exists(msg.NewCapture.path.c_str());

      RDCLOG("Got a new capture: %d (time %llu) %d byte thumbnail", msg.NewCapture.ID,
             msg.NewCapture.timestamp, thumbnail.count());

      int w = 0;
      int h = 0;
      int comp = 3;
      byte *thumbpixels = jpgd::decompress_jpeg_image_from_memory(
          thumbnail.data(), thumbnail.count(), &w, &h, &comp, 3);

      if(w > 0 && h > 0 && thumbpixels)
      {
        msg.NewCapture.thumbWidth = w;
        msg.NewCapture.thumbHeight = h;
        msg.NewCapture.thumbnail.assign(thumbpixels, w * h * 3);
      }
      else
      {
        msg.NewCapture.thumbWidth = 0;
        msg.NewCapture.thumbHeight = 0;
      }

      free(thumbpixels);

      reader.EndChunk();
      return msg;
    }
    else if(type == ePacket_RegisterAPI)
    {
      msg.Type = TargetControlMessageType::RegisterAPI;

      READ_DATA_SCOPE();
      SERIALISE_ELEMENT(msg.RegisterAPI.APIName).Named("API Name");

      RDCLOG("Used API: %s", msg.RegisterAPI.APIName.c_str());

      reader.EndChunk();
      return msg;
    }
    else if(type == ePacket_CopyCapture)
    {
      msg.Type = TargetControlMessageType::CaptureCopied;

      READ_DATA_SCOPE();
      SERIALISE_ELEMENT(msg.NewCapture.ID).Named("Capture ID");

      msg.NewCapture.path = m_CaptureCopies[msg.NewCapture.ID];

      StreamWriter streamWriter(FileIO::fopen(msg.NewCapture.path.c_str(), "wb"), Ownership::Stream);

      ser.SerialiseStream(msg.NewCapture.path.c_str(), streamWriter, NULL);

      if(reader.IsErrored())
      {
        SAFE_DELETE(m_Socket);

        msg.Type = TargetControlMessageType::Disconnected;
        return msg;
      }

      m_CaptureCopies.erase(msg.NewCapture.ID);

      reader.EndChunk();
      return msg;
    }
    else
    {
      RDCERR("Unexpected packed received: %d", type);
      SAFE_DELETE(m_Socket);

      msg.Type = TargetControlMessageType::Disconnected;
      return msg;
    }
  }

private:
  Network::Socket *m_Socket;
  WriteSerialiser writer;
  ReadSerialiser reader;
  std::string m_Target, m_API, m_BusyClient;
  uint32_t m_PID;

  std::map<uint32_t, std::string> m_CaptureCopies;
};

extern "C" RENDERDOC_API ITargetControl *RENDERDOC_CC RENDERDOC_CreateTargetControl(
    const char *host, uint32_t ident, const char *clientName, bool forceConnection)
{
  std::string s = "localhost";
  if(host != NULL && host[0] != '\0')
    s = host;

  bool android = false;

  if(host != NULL && Android::IsHostADB(host))
  {
    android = true;
    s = "127.0.0.1";

    // we don't need the index or device ID here, because the port is already the right one
    // forwarded to the right device.
  }

  Network::Socket *sock = Network::CreateClientSocket(s.c_str(), ident & 0xffff, 750);

  if(sock == NULL)
    return NULL;

  TargetControl *remote = new TargetControl(sock, clientName, forceConnection != 0);

  if(remote->Connected())
    return remote;

  delete remote;
  return NULL;
}
