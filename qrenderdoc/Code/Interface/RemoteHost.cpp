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

#include <QProcess>
#include <QThread>
#include "Code/QRDUtils.h"
#include "QRDInterface.h"
#include "renderdoc_replay.h"

RemoteHost::RemoteHost()
{
  ServerRunning = Connected = Busy = VersionMismatch = false;
}

RemoteHost::RemoteHost(const QVariant &var)
{
  QVariantMap map = var.toMap();
  if(map.contains(lit("Hostname")))
    Hostname = map[lit("Hostname")].toString();
  if(map.contains(lit("FriendlyName")))
    FriendlyName = map[lit("FriendlyName")].toString();
  if(map.contains(lit("RunCommand")))
    RunCommand = map[lit("RunCommand")].toString();

  ServerRunning = Connected = Busy = VersionMismatch = false;
}

RemoteHost::operator QVariant() const
{
  QVariantMap map;
  map[lit("Hostname")] = Hostname;
  map[lit("FriendlyName")] = FriendlyName;
  map[lit("RunCommand")] = RunCommand;
  return map;
}

void RemoteHost::CheckStatus()
{
  // special case - this is the local context
  if(Hostname == lit("localhost"))
  {
    ServerRunning = false;
    VersionMismatch = Busy = false;
    return;
  }

  IRemoteServer *rend = NULL;
  ReplayStatus status = RENDERDOC_CreateRemoteServerConnection(Hostname.toUtf8().data(), 0, &rend);

  if(status == ReplayStatus::Succeeded)
  {
    ServerRunning = true;
    VersionMismatch = Busy = false;
  }
  else if(status == ReplayStatus::NetworkRemoteBusy)
  {
    ServerRunning = true;
    Busy = true;
    VersionMismatch = false;
  }
  else if(status == ReplayStatus::NetworkVersionMismatch)
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
  int WAIT_TIME = 2000;

  if(IsHostADB())
  {
    RENDERDOC_StartAndroidRemoteServer(Hostname.toUtf8().data());
    QThread::msleep(WAIT_TIME);
    return;
  }

  RDProcess process;
  process.start(RunCommand);
  process.waitForFinished(WAIT_TIME);
  process.detach();
}
