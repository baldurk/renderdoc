/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2017 Baldur Karlsson
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

#include "RemoteHost.h"
#include <QProcess>
#include <QThread>
#include "Code/QRDUtils.h"
#include "renderdoc_replay.h"

RemoteHost::RemoteHost()
{
  ServerRunning = Connected = Busy = VersionMismatch = false;
}

RemoteHost::RemoteHost(const QVariant &var)
{
  QVariantMap map = var.toMap();
  if(map.contains("Hostname"))
    Hostname = map["Hostname"].toString();
  if(map.contains("RunCommand"))
    RunCommand = map["RunCommand"].toString();

  ServerRunning = Connected = Busy = VersionMismatch = false;
}

RemoteHost::operator QVariant() const
{
  QVariantMap map;
  map["Hostname"] = Hostname;
  map["RunCommand"] = RunCommand;
  return map;
}

void RemoteHost::CheckStatus()
{
  // special case - this is the local context
  if(Hostname == "localhost")
  {
    ServerRunning = false;
    VersionMismatch = Busy = false;
    return;
  }

  IRemoteServer *rend = NULL;
  ReplayCreateStatus status =
      RENDERDOC_CreateRemoteServerConnection(Hostname.toUtf8().data(), 0, &rend);

  if(status == eReplayCreate_Success)
  {
    ServerRunning = true;
    VersionMismatch = Busy = false;
  }
  else if(status == eReplayCreate_NetworkRemoteBusy)
  {
    ServerRunning = true;
    Busy = true;
    VersionMismatch = false;
  }
  else if(status == eReplayCreate_NetworkVersionMismatch)
  {
    ServerRunning = true;
    Busy = true;
    VersionMismatch = true;
  }
  else
  {
    ServerRunning = false;
    VersionMismatch = Busy = false;
  }

  if(rend)
    rend->ShutdownConnection();

  // since we can only have one active client at once on a remote server, we need
  // to avoid DDOS'ing by doing multiple CheckStatus() one after the other so fast
  // that the active client can't be properly shut down. Sleeping here for a short
  // time gives that breathing room.
  // Not the most elegant solution, but it is simple

  QThread::msleep(15);
}

void RemoteHost::Launch()
{
  RDProcess process;
  process.start(RunCommand);
  process.waitForFinished(2000);
  process.detach();
}
