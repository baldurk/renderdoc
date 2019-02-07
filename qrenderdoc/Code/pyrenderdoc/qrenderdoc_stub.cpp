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

rdcstr configFilePath(const rdcstr &filename)
{
  return "";
}

ICaptureContext *getCaptureContext(const QWidget *widget)
{
  return NULL;
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

ShaderToolOutput ShaderProcessingTool::DisassembleShader(QWidget *window,
                                                         const ShaderReflection *shaderDetails,
                                                         rdcstr arguments) const
{
  return {};
}

ShaderToolOutput ShaderProcessingTool::CompileShader(QWidget *window, rdcstr source,
                                                     rdcstr entryPoint, ShaderStage stage,
                                                     rdcstr arguments) const
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

int PersistantConfig::RemoteHostCount()
{
  return 0;
}

RemoteHost *PersistantConfig::GetRemoteHost(int index)
{
  return NULL;
}

void PersistantConfig::AddRemoteHost(RemoteHost host)
{
}

void PersistantConfig::AddAndroidHosts()
{
}

void PersistantConfig::SetupFormatting()
{
}

void AddRecentFile(rdcarray<rdcstr> &recentList, const rdcstr &file, int maxItems)
{
}

void PersistantConfig::SetConfigSetting(const rdcstr &name, const rdcstr &value)
{
}

rdcstr PersistantConfig::GetConfigSetting(const rdcstr &name)
{
  return "";
}

////////////////////////////////////////////////////////////////////////////////
// RemoteHost.cpp stubs
////////////////////////////////////////////////////////////////////////////////

RemoteHost::RemoteHost()
{
  serverRunning = connected = busy = versionMismatch = false;
}

void RemoteHost::CheckStatus()
{
}

ReplayStatus RemoteHost::Launch()
{
  return ReplayStatus::Succeeded;
}
