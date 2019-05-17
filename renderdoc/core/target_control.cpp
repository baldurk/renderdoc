/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2019 Baldur Karlsson
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

#include "android/android.h"
#include "api/replay/renderdoc_replay.h"
#include "core/core.h"
#include "jpeg-compressor/jpgd.h"
#include "os/os_specific.h"
#include "serialise/serialiser.h"

static const uint32_t TargetControlProtocolVersion = 5;

static bool IsProtocolVersionSupported(const uint32_t protocolVersion)
{
  // 2 -> 3 added a RDCDriver per capture
  if(protocolVersion == 2)
    return true;

  // 3 -> 4 added active window cycle and window count packets
  if(protocolVersion == 3)
    return true;

  // 4 -> 5 return frame number with new captures
  if(protocolVersion == 4)
    return true;

  if(protocolVersion == TargetControlProtocolVersion)
    return true;

  return false;
}

enum PacketType : uint32_t
{
  ePacket_Noop = 1,
  ePacket_Handshake,
  ePacket_Busy,
  ePacket_NewCapture,
  ePacket_APIUse,
  ePacket_TriggerCapture,
  ePacket_CopyCapture,
  ePacket_DeleteCapture,
  ePacket_QueueCapture,
  ePacket_NewChild,
  ePacket_CaptureProgress,
  ePacket_CycleActiveWindow,
  ePacket_CapturableWindowCount
};

DECLARE_REFLECTION_ENUM(PacketType);

template <>
rdcstr DoStringise(const PacketType &el)
{
  BEGIN_ENUM_STRINGISE(PacketType);
  {
    STRINGISE_ENUM_NAMED(ePacket_Noop, "No-op");
    STRINGISE_ENUM_NAMED(ePacket_Handshake, "Handshake");
    STRINGISE_ENUM_NAMED(ePacket_Busy, "Busy");
    STRINGISE_ENUM_NAMED(ePacket_NewCapture, "New Capture");
    STRINGISE_ENUM_NAMED(ePacket_APIUse, "API Use");
    STRINGISE_ENUM_NAMED(ePacket_TriggerCapture, "Trigger Capture");
    STRINGISE_ENUM_NAMED(ePacket_CopyCapture, "Copy Capture");
    STRINGISE_ENUM_NAMED(ePacket_DeleteCapture, "Delete Capture");
    STRINGISE_ENUM_NAMED(ePacket_QueueCapture, "Queue Capture");
    STRINGISE_ENUM_NAMED(ePacket_NewChild, "New Child");
    STRINGISE_ENUM_NAMED(ePacket_CaptureProgress, "Capture Progress");
    STRINGISE_ENUM_NAMED(ePacket_CycleActiveWindow, "Cycle Active Window");
    STRINGISE_ENUM_NAMED(ePacket_CapturableWindowCount, "Capturable Window Count");
  }
  END_ENUM_STRINGISE();
}

#define WRITE_DATA_SCOPE() WriteSerialiser &ser = writer;
#define READ_DATA_SCOPE() ReadSerialiser &ser = reader;

void RenderDoc::TargetControlClientThread(uint32_t version, Network::Socket *client)
{
  Threading::KeepModuleAlive();

  WriteSerialiser writer(new StreamWriter(client, Ownership::Nothing), Ownership::Stream);
  ReadSerialiser reader(new StreamReader(client, Ownership::Nothing), Ownership::Stream);

  writer.SetStreamingMode(true);
  reader.SetStreamingMode(true);

  std::string target = RenderDoc::Inst().GetCurrentTarget();
  uint32_t mypid = Process::GetCurrentPID();

  {
    WRITE_DATA_SCOPE();
    SCOPED_SERIALISE_CHUNK(ePacket_Handshake);
    SERIALISE_ELEMENT(TargetControlProtocolVersion);
    SERIALISE_ELEMENT(target);
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

  float captureProgress = -1.0f;
  RenderDoc::Inst().SetProgressCallback<CaptureProgress>(
      [&captureProgress](float p) { captureProgress = p; });

  const int pingtime = 1000;       // ping every 1000ms
  const int ticktime = 10;         // tick every 10ms
  const int progresstime = 100;    // update capture progress every 100ms
  int curtime = 0;

  std::vector<CaptureData> captures;
  std::vector<rdcpair<uint32_t, uint32_t> > children;
  std::map<RDCDriver, bool> drivers;
  float prevCaptureProgress = captureProgress;
  uint32_t prevWindows = 0;

  while(client)
  {
    if(RenderDoc::Inst().m_ControlClientThreadShutdown || (client && !client->Connected()))
    {
      SAFE_DELETE(client);
      break;
    }

    Threading::Sleep(ticktime);
    curtime += ticktime;

    std::map<RDCDriver, bool> curdrivers = RenderDoc::Inst().GetActiveDrivers();

    std::vector<CaptureData> caps = RenderDoc::Inst().GetCaptures();
    std::vector<rdcpair<uint32_t, uint32_t> > childprocs = RenderDoc::Inst().GetChildProcesses();

    uint32_t curWindows = RenderDoc::Inst().GetCapturableWindowCount();

    if(curdrivers != drivers)
    {
      // find the first difference, either a new key or a key with a different value, and send it.
      RDCDriver driver = RDCDriver::Unknown;
      bool presenting = false;

      // search for new drivers
      for(auto it = curdrivers.begin(); it != curdrivers.end(); it++)
      {
        if(drivers.find(it->first) == drivers.end() || drivers[it->first] != it->second)
        {
          driver = it->first;
          presenting = it->second;
          break;
        }
      }

      RDCASSERTNOTEQUAL(driver, RDCDriver::Unknown);

      if(driver != RDCDriver::Unknown)
        drivers[driver] = presenting;

      bool supported =
          RenderDoc::Inst().HasRemoteDriver(driver) || RenderDoc::Inst().HasReplayDriver(driver);

      WRITE_DATA_SCOPE();
      {
        SCOPED_SERIALISE_CHUNK(ePacket_APIUse);
        SERIALISE_ELEMENT(driver);
        SERIALISE_ELEMENT(presenting);
        SERIALISE_ELEMENT(supported);
      }
    }
    else if(caps.size() != captures.size())
    {
      uint32_t idx = (uint32_t)captures.size();

      captures.push_back(caps[idx]);

      std::string path = FileIO::GetFullPathname(captures.back().path);

      bytebuf buf;

      ICaptureFile *file = RENDERDOC_OpenCaptureFile();
      if(file->OpenFile(captures.back().path.c_str(), "rdc", NULL) == ReplayStatus::Succeeded)
      {
        buf = file->GetThumbnail(FileType::JPG, 0).data;
      }
      file->Shutdown();

      WRITE_DATA_SCOPE();
      {
        SCOPED_SERIALISE_CHUNK(ePacket_NewCapture);
        SERIALISE_ELEMENT(idx);
        SERIALISE_ELEMENT(captures.back().timestamp);
        SERIALISE_ELEMENT(path);
        SERIALISE_ELEMENT(buf);
        if(version >= 3)
          SERIALISE_ELEMENT(captures.back().driver);
        if(version >= 5)
          SERIALISE_ELEMENT(captures.back().frameNumber);
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
    else if(prevCaptureProgress != captureProgress)
    {
      if(captureProgress == 1.0f || captureProgress == -1.0f)
        captureProgress = -1.0f;

      // send progress packets at reduced rate (not every tick), or if the progress is finished.
      // we don't need to ping while we're sending capture progress, so we re-use curtime
      if(captureProgress == -1.0f || curtime > progresstime)
      {
        curtime = 0;

        prevCaptureProgress = captureProgress;

        WRITE_DATA_SCOPE();
        {
          SCOPED_SERIALISE_CHUNK(ePacket_CaptureProgress);
          SERIALISE_ELEMENT(captureProgress);
        }
      }
    }
    else if(version >= 4 && prevWindows != curWindows)
    {
      prevWindows = curWindows;

      WRITE_DATA_SCOPE();
      {
        SCOPED_SERIALISE_CHUNK(ePacket_CapturableWindowCount);
        SERIALISE_ELEMENT(curWindows);
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

    if(client->IsRecvDataWaiting() || !reader.GetReader()->AtEnd())
    {
      PacketType type = reader.ReadChunk<PacketType>();

      if(type == ePacket_TriggerCapture)
      {
        uint32_t numFrames = 1;

        READ_DATA_SCOPE();
        SERIALISE_ELEMENT(numFrames);

        RenderDoc::Inst().TriggerCapture(numFrames);
      }
      else if(type == ePacket_QueueCapture)
      {
        uint32_t frameNum = 0;
        uint32_t numFrames = 1;

        READ_DATA_SCOPE();
        SERIALISE_ELEMENT(frameNum);
        SERIALISE_ELEMENT(numFrames);

        for(uint32_t f = 0; f < numFrames; f++)
          RenderDoc::Inst().QueueCapture(frameNum + f);
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
      else if(type == ePacket_CycleActiveWindow)
      {
        RenderDoc::Inst().CycleActiveWindow();
      }

      reader.EndChunk();

      if(reader.IsErrored())
        SAFE_DELETE(client);
    }
  }

  RenderDoc::Inst().SetProgressCallback<CaptureProgress>(RENDERDOC_ProgressCallback());

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
    Network::Socket *client = sock->AcceptClient(0);

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
    uint32_t version;
    bool kick = false;

    // receive handshake from client and get its name
    {
      ReadSerialiser ser(new StreamReader(client, Ownership::Nothing), Ownership::Stream);

      PacketType type = ser.ReadChunk<PacketType>();

      if(type != ePacket_Handshake)
      {
        SAFE_DELETE(client);
        continue;
      }

      SERIALISE_ELEMENT(version);
      SERIALISE_ELEMENT(newClient);
      SERIALISE_ELEMENT(kick);

      ser.EndChunk();

      if(newClient.empty() || !IsProtocolVersionSupported(version))
      {
        RDCLOG("Invalid/Unsupported handshake '%s' / %d", newClient.c_str(), version);
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
      clientThread =
          Threading::CreateThread([version, client] { TargetControlClientThread(version, client); });
      continue;
    }
    else
    {
      // if we've been asked to kick the existing connection off
      // reject this connection and tell them who is busy
      WriteSerialiser ser(new StreamWriter(client, Ownership::Nothing), Ownership::Stream);

      ser.SetStreamingMode(true);

      std::string target = RenderDoc::Inst().GetCurrentTarget();
      {
        SCOPED_SERIALISE_CHUNK(ePacket_Busy);
        SERIALISE_ELEMENT(TargetControlProtocolVersion);
        SERIALISE_ELEMENT(target);
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
    reader.SetStreamingMode(true);

    m_PID = 0;

    {
      WRITE_DATA_SCOPE();

      {
        SCOPED_SERIALISE_CHUNK(ePacket_Handshake);
        SERIALISE_ELEMENT(TargetControlProtocolVersion);
        SERIALISE_ELEMENT(clientName);
        SERIALISE_ELEMENT(forceConnection);
      }

      if(writer.IsErrored())
      {
        SAFE_DELETE(m_Socket);
        return;
      }
    }

    PacketType type = reader.ReadChunk<PacketType>();

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

    m_Version = 0;

    {
      READ_DATA_SCOPE();
      SERIALISE_ELEMENT(m_Version);
      SERIALISE_ELEMENT(m_Target);
      SERIALISE_ELEMENT(m_PID);
    }

    reader.EndChunk();

    if(!IsProtocolVersionSupported(m_Version))
    {
      RDCERR("Unsupported version, got %u", m_Version);
      SAFE_DELETE(m_Socket);
      return;
    }

    if(type == ePacket_Handshake)
    {
      RDCLOG("Got remote handshake: %s [%u]", m_Target.c_str(), m_PID);
    }
    else if(type == ePacket_Busy)
    {
      RDCLOG("Got remote busy signal: %s owned by %s", m_Target.c_str(), m_BusyClient.c_str());
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

  void QueueCapture(uint32_t frameNumber, uint32_t numFrames)
  {
    WRITE_DATA_SCOPE();
    SCOPED_SERIALISE_CHUNK(ePacket_QueueCapture);

    SERIALISE_ELEMENT(frameNumber);
    SERIALISE_ELEMENT(numFrames);

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

  void CycleActiveWindow()
  {
    if(m_Version < 4)
      return;

    WRITE_DATA_SCOPE();
    SCOPED_SERIALISE_CHUNK(ePacket_CycleActiveWindow);

    if(ser.IsErrored())
      SAFE_DELETE(m_Socket);
  }

  TargetControlMessage ReceiveMessage(RENDERDOC_ProgressCallback progress)
  {
    TargetControlMessage msg;
    if(m_Socket == NULL)
    {
      msg.type = TargetControlMessageType::Disconnected;
      return msg;
    }

    if(!m_Socket->IsRecvDataWaiting() && reader.GetReader()->AtEnd())
    {
      if(!m_Socket->Connected())
      {
        SAFE_DELETE(m_Socket);
        msg.type = TargetControlMessageType::Disconnected;
      }
      else
      {
        Threading::Sleep(2);
        msg.type = TargetControlMessageType::Noop;
      }

      return msg;
    }

    PacketType type = reader.ReadChunk<PacketType>();

    if(reader.IsErrored())
    {
      SAFE_DELETE(m_Socket);

      msg.type = TargetControlMessageType::Disconnected;
      return msg;
    }
    else if(type == ePacket_Noop)
    {
      msg.type = TargetControlMessageType::Noop;
      reader.EndChunk();
      return msg;
    }
    else if(type == ePacket_Busy)
    {
      READ_DATA_SCOPE();
      SERIALISE_ELEMENT(msg.busy.clientName).Named("Client Name"_lit);

      SAFE_DELETE(m_Socket);

      RDCLOG("Got busy signal: '%s", msg.busy.clientName.c_str());
      msg.type = TargetControlMessageType::Busy;
      return msg;
    }
    else if(type == ePacket_NewChild)
    {
      msg.type = TargetControlMessageType::NewChild;

      READ_DATA_SCOPE();
      SERIALISE_ELEMENT(msg.newChild.processId).Named("PID"_lit);
      SERIALISE_ELEMENT(msg.newChild.ident).Named("Child ident"_lit);

      RDCLOG("Got a new child process: %u %u", msg.newChild.processId, msg.newChild.ident);

      reader.EndChunk();
      return msg;
    }
    else if(type == ePacket_CaptureProgress)
    {
      msg.type = TargetControlMessageType::CaptureProgress;

      READ_DATA_SCOPE();
      SERIALISE_ELEMENT(msg.capProgress).Named("Capture Progress"_lit);

      reader.EndChunk();
      return msg;
    }
    else if(type == ePacket_NewCapture)
    {
      msg.type = TargetControlMessageType::NewCapture;

      bytebuf thumbnail;

      RDCDriver driver = RDCDriver::Unknown;

      {
        READ_DATA_SCOPE();
        SERIALISE_ELEMENT(msg.newCapture.captureId).Named("Capture ID"_lit);
        SERIALISE_ELEMENT(msg.newCapture.timestamp).Named("timestamp"_lit);
        SERIALISE_ELEMENT(msg.newCapture.path).Named("path"_lit);
        SERIALISE_ELEMENT(thumbnail);
        if(m_Version >= 3)
          SERIALISE_ELEMENT(driver);
        if(m_Version >= 5)
        {
          SERIALISE_ELEMENT(msg.newCapture.frameNumber);
        }
        else
        {
          msg.newCapture.frameNumber = msg.newCapture.captureId + 1;
        }
      }

      if(driver != RDCDriver::Unknown)
        msg.newCapture.api = ToStr(driver);

      msg.newCapture.local = FileIO::exists(msg.newCapture.path.c_str());

      RDCLOG("Got a new capture: %d (frame %u) (time %llu) %d byte thumbnail",
             msg.newCapture.captureId, msg.newCapture.frameNumber, msg.newCapture.timestamp,
             thumbnail.count());

      int w = 0;
      int h = 0;
      int comp = 3;
      byte *thumbpixels = jpgd::decompress_jpeg_image_from_memory(
          thumbnail.data(), thumbnail.count(), &w, &h, &comp, 3);

      if(w > 0 && h > 0 && thumbpixels)
      {
        msg.newCapture.thumbWidth = w;
        msg.newCapture.thumbHeight = h;
        msg.newCapture.thumbnail.assign(thumbpixels, w * h * 3);
      }
      else
      {
        msg.newCapture.thumbWidth = 0;
        msg.newCapture.thumbHeight = 0;
      }

      free(thumbpixels);

      reader.EndChunk();
      return msg;
    }
    else if(type == ePacket_APIUse)
    {
      msg.type = TargetControlMessageType::RegisterAPI;

      RDCDriver driver = RDCDriver::Unknown;
      bool presenting = false;
      bool supported = false;

      READ_DATA_SCOPE();
      SERIALISE_ELEMENT(driver);
      SERIALISE_ELEMENT(presenting);
      SERIALISE_ELEMENT(supported);

      msg.apiUse.name = ToStr(driver);
      msg.apiUse.presenting = presenting;
      msg.apiUse.supported = supported;

      if(presenting)
        m_API = ToStr(driver);

      RDCLOG("Used API: %s (%s & %s)", msg.apiUse.name.c_str(),
             presenting ? "Presenting" : "Not presenting",
             supported ? "supported" : "not supported");

      reader.EndChunk();
      return msg;
    }
    else if(type == ePacket_CopyCapture)
    {
      msg.type = TargetControlMessageType::CaptureCopied;

      READ_DATA_SCOPE();
      SERIALISE_ELEMENT(msg.newCapture.captureId).Named("Capture ID"_lit);

      msg.newCapture.path = m_CaptureCopies[msg.newCapture.captureId];

      StreamWriter streamWriter(FileIO::fopen(msg.newCapture.path.c_str(), "wb"), Ownership::Stream);

      ser.SerialiseStream(msg.newCapture.path.c_str(), streamWriter, progress);

      if(reader.IsErrored())
      {
        SAFE_DELETE(m_Socket);

        msg.type = TargetControlMessageType::Disconnected;
        return msg;
      }

      m_CaptureCopies.erase(msg.newCapture.captureId);

      reader.EndChunk();
      return msg;
    }
    else if(type == ePacket_CapturableWindowCount)
    {
      msg.type = TargetControlMessageType::CapturableWindowCount;
      uint32_t windows = 0;
      READ_DATA_SCOPE();
      SERIALISE_ELEMENT(windows);
      msg.capturableWindowCount = windows;
      reader.EndChunk();
      return msg;
    }
    else
    {
      RDCERR("Unexpected packed received: %d", type);
      SAFE_DELETE(m_Socket);

      msg.type = TargetControlMessageType::Disconnected;
      return msg;
    }
  }

private:
  Network::Socket *m_Socket;
  WriteSerialiser writer;
  ReadSerialiser reader;
  std::string m_Target, m_API, m_BusyClient;
  uint32_t m_Version, m_PID;

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
