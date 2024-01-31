/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2024 Baldur Karlsson
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

#include <QAtomicInt>
#include <QMutex>
#include <QMutexLocker>
#include <QProcess>
#include <QThread>
#include "Code/QRDUtils.h"
#include "QRDInterface.h"

struct RemoteHostData
{
  QAtomicInt refcount;
  QMutex mutex;

  void ref() { refcount.ref(); }
  void deref()
  {
    if(!refcount.deref())
      delete this;
  }

  RemoteHostData() : refcount(1) {}
  rdcstr m_friendlyName, m_runCommand, m_lastCapturePath, m_versionError;
  bool m_serverRunning = false, m_connected = false, m_busy = false, m_versionMismatch = false;
};

RemoteHost::RemoteHost(const QVariant &var)
{
  m_data = new RemoteHostData();

  QVariantMap map = var.toMap();
  if(map.contains(lit("hostname")))
    m_hostname = map[lit("hostname")].toString();
  if(map.contains(lit("friendlyName")))
    m_data->m_friendlyName = map[lit("friendlyName")].toString();
  if(map.contains(lit("runCommand")))
    m_data->m_runCommand = map[lit("runCommand")].toString();
  if(map.contains(lit("lastCapturePath")))
    m_data->m_lastCapturePath = map[lit("lastCapturePath")].toString();

  m_protocol = RENDERDOC_GetDeviceProtocolController(m_hostname);
}

RemoteHost::RemoteHost()
{
  m_data = new RemoteHostData();
}

RemoteHost::RemoteHost(const rdcstr &host)
{
  // create a new host
  m_hostname = host;
  m_data = new RemoteHostData();

  m_protocol = RENDERDOC_GetDeviceProtocolController(m_hostname);
}

RemoteHost::RemoteHost(const RemoteHost &o)
{
  *this = o;
}

RemoteHost &RemoteHost::operator=(const RemoteHost &o)
{
  m_hostname = o.m_hostname;
  m_protocol = o.m_protocol;

  // deref old data
  if(m_data)
    m_data->deref();
  // point to new data
  m_data = o.m_data;
  // get a ref on it
  m_data->ref();
  return *this;
}

RemoteHost::~RemoteHost()
{
  m_data->deref();
}

RemoteHost::operator QVariant() const
{
  QVariantMap map;
  m_data->mutex.lock();
  map[lit("hostname")] = m_hostname;
  map[lit("friendlyName")] = m_data->m_friendlyName;
  map[lit("runCommand")] = m_data->m_runCommand;
  map[lit("lastCapturePath")] = m_data->m_lastCapturePath;
  m_data->mutex.unlock();
  return map;
}

void RemoteHost::CheckStatus()
{
  // special case - this is the local context
  if(m_hostname == "localhost")
  {
    QMutexLocker autolock(&m_data->mutex);
    m_data->m_serverRunning = m_data->m_versionMismatch = m_data->m_busy = false;
    m_data->m_versionError.clear();
    return;
  }

  UpdateStatus(RENDERDOC_CheckRemoteServerConnection(m_hostname));
}

ResultDetails RemoteHost::Connect(IRemoteServer **server)
{
  QMutexLocker autolock(&m_data->mutex);
  return RENDERDOC_CreateRemoteServerConnection(m_hostname, server);
}

void RemoteHost::SetConnected(bool connected)
{
  QMutexLocker autolock(&m_data->mutex);
  m_data->m_connected = connected;
}

void RemoteHost::SetShutdown()
{
  QMutexLocker autolock(&m_data->mutex);
  m_data->m_connected = false;
  m_data->m_serverRunning = false;
  m_data->m_busy = false;
}

void RemoteHost::UpdateStatus(ResultDetails result)
{
  {
    QMutexLocker autolock(&m_data->mutex);

    if(result.code == ResultCode::Succeeded)
    {
      m_data->m_serverRunning = true;
      m_data->m_versionMismatch = m_data->m_busy = false;
      m_data->m_versionError.clear();
    }
    else if(result.code == ResultCode::NetworkRemoteBusy)
    {
      m_data->m_serverRunning = true;
      m_data->m_busy = true;
      m_data->m_versionMismatch = false;
      m_data->m_versionError.clear();
    }
    else if(result.code == ResultCode::NetworkVersionMismatch)
    {
      m_data->m_serverRunning = true;
      m_data->m_busy = true;
      m_data->m_versionMismatch = true;
      m_data->m_versionError = result.Message();
    }
    else
    {
      m_data->m_serverRunning = false;
      m_data->m_versionMismatch = m_data->m_busy = false;
      m_data->m_versionError.clear();
    }
  }

  // since we can only have one active client at once on a remote server, we need
  // to avoid DDOS'ing by doing multiple CheckStatus() one after the other so fast
  // that the active client can't be properly shut down. Sleeping here for a short
  // time gives that breathing room.
  // Not the most elegant solution, but it is simple

  QThread::msleep(15);
}

ResultDetails RemoteHost::Launch()
{
  if(m_protocol)
    // this is blocking
    return m_protocol->StartRemoteServer(m_hostname);

  rdcstr run = RunCommand();

  RDProcess process;
  process.start(run);
  process.waitForFinished(2000);
  process.detach();

  return {ResultCode::Succeeded};
}

bool RemoteHost::IsServerRunning() const
{
  QMutexLocker autolock(&m_data->mutex);
  return m_data->m_serverRunning;
}

bool RemoteHost::IsConnected() const
{
  QMutexLocker autolock(&m_data->mutex);
  return m_data->m_connected;
}

bool RemoteHost::IsBusy() const
{
  QMutexLocker autolock(&m_data->mutex);
  return m_data->m_busy;
}

bool RemoteHost::IsVersionMismatch() const
{
  QMutexLocker autolock(&m_data->mutex);
  return m_data->m_versionMismatch;
}

rdcstr RemoteHost::VersionMismatchError() const
{
  QMutexLocker autolock(&m_data->mutex);
  if(m_data->m_versionError.empty())
    return "Version Mismatch";
  return m_data->m_versionError;
}

rdcstr RemoteHost::FriendlyName() const
{
  QMutexLocker autolock(&m_data->mutex);
  return m_data->m_friendlyName;
}

void RemoteHost::SetFriendlyName(const rdcstr &name)
{
  QMutexLocker autolock(&m_data->mutex);
  m_data->m_friendlyName = name;
}

rdcstr RemoteHost::RunCommand() const
{
  QMutexLocker autolock(&m_data->mutex);
  return m_data->m_runCommand;
}

void RemoteHost::SetRunCommand(const rdcstr &cmd)
{
  QMutexLocker autolock(&m_data->mutex);
  m_data->m_runCommand = cmd;
}

rdcstr RemoteHost::LastCapturePath() const
{
  QMutexLocker autolock(&m_data->mutex);
  return m_data->m_lastCapturePath;
}

void RemoteHost::SetLastCapturePath(const rdcstr &path)
{
  QMutexLocker autolock(&m_data->mutex);
  m_data->m_lastCapturePath = path;
}
