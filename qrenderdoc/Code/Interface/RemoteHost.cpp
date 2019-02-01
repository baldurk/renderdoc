/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2017-2019 Baldur Karlsson
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

RemoteHost::RemoteHost()
{
  serverRunning = connected = busy = versionMismatch = false;
}

RemoteHost::RemoteHost(const QVariant &var)
{
  QVariantMap map = var.toMap();
  if(map.contains(lit("hostname")))
    hostname = map[lit("hostname")].toString();
  if(map.contains(lit("friendlyName")))
    friendlyName = map[lit("friendlyName")].toString();
  if(map.contains(lit("runCommand")))
    runCommand = map[lit("runCommand")].toString();
  if(map.contains(lit("lastCapturePath")))
    lastCapturePath = map[lit("lastCapturePath")].toString();

  serverRunning = connected = busy = versionMismatch = false;
}

RemoteHost::operator QVariant() const
{
  QVariantMap map;
  map[lit("hostname")] = hostname;
  map[lit("friendlyName")] = friendlyName;
  map[lit("runCommand")] = runCommand;
  map[lit("lastCapturePath")] = lastCapturePath;
  return map;
}

void RemoteHost::CheckStatus()
{
  // special case - this is the local context
  if(hostname == "localhost")
  {
    serverRunning = false;
    versionMismatch = busy = false;
    return;
  }

  IRemoteServer *rend = NULL;
  ReplayStatus status = RENDERDOC_CreateRemoteServerConnection(hostname.c_str(), 0, &rend);

  if(status == ReplayStatus::Succeeded)
  {
    serverRunning = true;
    versionMismatch = busy = false;
  }
  else if(status == ReplayStatus::NetworkRemoteBusy)
  {
    serverRunning = true;
    busy = true;
    versionMismatch = false;
  }
  else if(status == ReplayStatus::NetworkVersionMismatch)
  {
    serverRunning = true;
    busy = true;
    versionMismatch = true;
  }
  else
  {
    serverRunning = false;
    versionMismatch = busy = false;
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

ReplayStatus RemoteHost::Launch()
{
  ReplayStatus status = ReplayStatus::Succeeded;

  int WAIT_TIME = 2000;

  if(IsADB())
  {
    status = RENDERDOC_StartAndroidRemoteServer(hostname.c_str());
    QThread::msleep(WAIT_TIME);
    return status;
  }

  RDProcess process;
  process.start(runCommand);
  process.waitForFinished(WAIT_TIME);
  process.detach();

  return status;
}
