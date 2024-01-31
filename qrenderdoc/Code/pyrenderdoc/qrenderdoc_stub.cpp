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

#include <Python.h>

#define SWIG_GENERATED
#include "Code/Interface/QRDInterface.h"

// we only support the qrenderdoc module for docs generation, so it doesn't matter that these stub
// functions aren't valid

////////////////////////////////////////////////////////////////////////////////
// QRDInterface.cpp stubs
////////////////////////////////////////////////////////////////////////////////

CaptureSettings::CaptureSettings()
{
  inject = false;
  autoStart = false;
  queuedFrameCap = 0;
  numQueuedFrames = 0;
  RENDERDOC_GetDefaultCaptureOptions(&options);
}

rdcstr ConfigFilePath(const rdcstr &filename)
{
  return "";
}

////////////////////////////////////////////////////////////////////////////////
// PythonContext.cpp stubs
////////////////////////////////////////////////////////////////////////////////

class QWidget;

extern "C" QWidget *QWidgetFromPy(PyObject *widget)
{
  return NULL;
}

extern "C" PyObject *QWidgetToPy(QWidget *widget)
{
  Py_IncRef(Py_None);
  return Py_None;
}

////////////////////////////////////////////////////////////////////////////////
// ShaderProcessingTool.cpp stubs
////////////////////////////////////////////////////////////////////////////////

rdcstr ShaderProcessingTool::DefaultArguments() const
{
  return "";
}

rdcstr ShaderProcessingTool::IOArguments() const
{
  return "";
}

ShaderToolOutput ShaderProcessingTool::DisassembleShader(QWidget *window,
                                                         const ShaderReflection *shaderDetails,
                                                         rdcstr arguments) const
{
  return {};
}

ShaderToolOutput ShaderProcessingTool::CompileShader(QWidget *window, rdcstr source,
                                                     rdcstr entryPoint, ShaderStage stage,
                                                     rdcstr spirvVer, rdcstr arguments) const
{
  return {};
}

////////////////////////////////////////////////////////////////////////////////
// PersistantConfig.cpp stubs
////////////////////////////////////////////////////////////////////////////////

rdcstr BugReport::URL() const
{
  return "";
}

bool PersistantConfig::SetStyle()
{
  return false;
}

PersistantConfig::PersistantConfig()
{
}

PersistantConfig::~PersistantConfig()
{
}

bool PersistantConfig::Load(const rdcstr &filename)
{
  return false;
}

bool PersistantConfig::Save()
{
  return false;
}

void PersistantConfig::Close()
{
}

rdcarray<RemoteHost> PersistantConfig::GetRemoteHosts()
{
  return {};
}

RemoteHost PersistantConfig::GetRemoteHost(const rdcstr &)
{
  return RemoteHost();
}

void PersistantConfig::AddRemoteHost(RemoteHost host)
{
}

void PersistantConfig::RemoveRemoteHost(RemoteHost host)
{
}

void PersistantConfig::UpdateEnumeratedProtocolDevices()
{
}

void PersistantConfig::SetupFormatting()
{
}

void AddRecentFile(rdcarray<rdcstr> &recentList, const rdcstr &file)
{
}

void RemoveRecentFile(rdcarray<rdcstr> &recentList, const rdcstr &file)
{
}

////////////////////////////////////////////////////////////////////////////////
// RemoteHost.cpp stubs
////////////////////////////////////////////////////////////////////////////////

RemoteHost::RemoteHost()
{
}

RemoteHost::RemoteHost(const rdcstr &host)
{
}

RemoteHost::RemoteHost(const RemoteHost &o)
{
}

RemoteHost &RemoteHost::operator=(const RemoteHost &o)
{
  return *this;
}

RemoteHost::~RemoteHost()
{
}

void RemoteHost::CheckStatus()
{
}

ResultDetails RemoteHost::Connect(IRemoteServer **server)
{
  return {ResultCode::Succeeded};
}

ResultDetails RemoteHost::Launch()
{
  return {ResultCode::Succeeded};
}

bool RemoteHost::IsServerRunning() const
{
  return false;
}

bool RemoteHost::IsConnected() const
{
  return false;
}

bool RemoteHost::IsBusy() const
{
  return false;
}

bool RemoteHost::IsVersionMismatch() const
{
  return false;
}

rdcstr RemoteHost::VersionMismatchError() const
{
  return rdcstr();
}

rdcstr RemoteHost::FriendlyName() const
{
  return rdcstr();
}

void RemoteHost::SetFriendlyName(const rdcstr &name)
{
}

rdcstr RemoteHost::RunCommand() const
{
  return rdcstr();
}

void RemoteHost::SetRunCommand(const rdcstr &cmd)
{
}

rdcstr RemoteHost::LastCapturePath() const
{
  return rdcstr();
}

void RemoteHost::SetLastCapturePath(const rdcstr &path)
{
}

void RemoteHost::SetConnected(bool connected)
{
}

void RemoteHost::SetShutdown()
{
}
