/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Baldur Karlsson
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

#include "api/replay/renderdoc_replay.h"
#include "os/os_specific.h"

namespace Network
{
class Socket;
};

enum class RDCDriver : uint32_t;

class WriteSerialiser;
class ReadSerialiser;

struct RemoteServer : public IRemoteServer
{
public:
  RemoteServer(Network::Socket *sock, const rdcstr &deviceID);

  virtual ~RemoteServer();

  virtual void ShutdownConnection();

  virtual void ShutdownServerAndConnection();

  virtual bool Connected();
  virtual bool Ping();

  virtual rdcarray<rdcstr> LocalProxies();

  virtual rdcarray<rdcstr> RemoteSupportedReplays();

  virtual rdcstr GetHomeFolder();

  virtual rdcarray<PathEntry> ListFolder(const char *path);

  virtual ExecuteResult ExecuteAndInject(const char *a, const char *w, const char *c,
                                         const rdcarray<EnvironmentModification> &env,
                                         const CaptureOptions &opts);

  virtual void CopyCaptureFromRemote(const char *remotepath, const char *localpath,
                                     RENDERDOC_ProgressCallback progress);

  virtual rdcstr CopyCaptureToRemote(const char *filename, RENDERDOC_ProgressCallback progress);

  virtual void TakeOwnershipCapture(const char *filename);

  virtual rdcpair<ReplayStatus, IReplayController *> OpenCapture(uint32_t proxyid,
                                                                 const char *filename,
                                                                 const ReplayOptions &opts,
                                                                 RENDERDOC_ProgressCallback progress);

  virtual void CloseCapture(IReplayController *rend);

  virtual rdcstr DriverName();

  virtual rdcarray<GPUDevice> GetAvailableGPUs();

  virtual int GetSectionCount();

  virtual int FindSectionByName(const char *name);

  virtual int FindSectionByType(SectionType sectionType);

  virtual SectionProperties GetSectionProperties(int index);

  virtual bytebuf GetSectionContents(int index);

  virtual bool WriteSection(const SectionProperties &props, const bytebuf &contents);

  virtual bool HasCallstacks();

  virtual bool InitResolver(RENDERDOC_ProgressCallback progress);

  virtual rdcarray<rdcstr> GetResolve(const rdcarray<uint64_t> &callstack);

protected:
  Network::Socket *m_Socket;
  WriteSerialiser *writer;
  ReadSerialiser *reader;
  FileIO::LogFileHandle *debugLog;
  rdcstr m_deviceID;

  rdcarray<rdcpair<RDCDriver, rdcstr>> m_Proxies;
};
