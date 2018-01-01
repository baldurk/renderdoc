/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2017-2018 Baldur Karlsson
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
// SPIRVDisassembler.cpp stubs
////////////////////////////////////////////////////////////////////////////////

rdcstr SPIRVDisassembler::DisassembleShader(QWidget *window, const ShaderReflection *shaderDetails) const
{
  return "";
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

void RemoteHost::Launch()
{
}

////////////////////////////////////////////////////////////////////////////////
// CommonPipelineState.cpp stubs
////////////////////////////////////////////////////////////////////////////////

rdcstr CommonPipelineState::GetResourceLayout(ResourceId id)
{
  return "";
}

rdcstr CommonPipelineState::Abbrev(ShaderStage stage)
{
  return "";
}

rdcstr CommonPipelineState::OutputAbbrev()
{
  return "";
}

const D3D11Pipe::Shader &CommonPipelineState::GetD3D11Stage(ShaderStage stage)
{
  static D3D11Pipe::Shader dummy;
  return dummy;
}

const D3D12Pipe::Shader &CommonPipelineState::GetD3D12Stage(ShaderStage stage)
{
  static D3D12Pipe::Shader dummy;
  return dummy;
}

const GLPipe::Shader &CommonPipelineState::GetGLStage(ShaderStage stage)
{
  static GLPipe::Shader dummy;
  return dummy;
}

const VKPipe::Shader &CommonPipelineState::GetVulkanStage(ShaderStage stage)
{
  static VKPipe::Shader dummy;
  return dummy;
}

rdcstr CommonPipelineState::GetShaderExtension()
{
  return "";
}

Viewport CommonPipelineState::GetViewport(int index)
{
  return Viewport();
}

Scissor CommonPipelineState::GetScissor(int index)
{
  return Scissor();
}

const ShaderBindpointMapping &CommonPipelineState::GetBindpointMapping(ShaderStage stage)
{
  static ShaderBindpointMapping dummy;
  return dummy;
}

const ShaderReflection *CommonPipelineState::GetShaderReflection(ShaderStage stage)
{
  return NULL;
}

ResourceId CommonPipelineState::GetComputePipelineObject()
{
  return ResourceId();
}

ResourceId CommonPipelineState::GetGraphicsPipelineObject()
{
  return ResourceId();
}

rdcstr CommonPipelineState::GetShaderEntryPoint(ShaderStage stage)
{
  return "";
}

ResourceId CommonPipelineState::GetShader(ShaderStage stage)
{
  return ResourceId();
}

rdcstr CommonPipelineState::GetShaderName(ShaderStage stage)
{
  return "";
}

BoundVBuffer CommonPipelineState::GetIBuffer()
{
  return BoundVBuffer();
}

bool CommonPipelineState::IsStripRestartEnabled()
{
  return false;
}

uint32_t CommonPipelineState::GetStripRestartIndex()
{
  return UINT32_MAX;
}

rdcarray<BoundVBuffer> CommonPipelineState::GetVBuffers()
{
  return rdcarray<BoundVBuffer>();
}

rdcarray<VertexInputAttribute> CommonPipelineState::GetVertexInputs()
{
  return rdcarray<VertexInputAttribute>();
}

BoundCBuffer CommonPipelineState::GetConstantBuffer(ShaderStage stage, uint32_t BufIdx,
                                                    uint32_t ArrayIdx)
{
  return BoundCBuffer();
}

rdcarray<BoundResourceArray> CommonPipelineState::GetReadOnlyResources(ShaderStage stage)
{
  return rdcarray<BoundResourceArray>();
}

rdcarray<BoundResourceArray> CommonPipelineState::GetReadWriteResources(ShaderStage stage)
{
  return rdcarray<BoundResourceArray>();
}

BoundResource CommonPipelineState::GetDepthTarget()
{
  return BoundResource();
}

rdcarray<BoundResource> CommonPipelineState::GetOutputTargets()
{
  return rdcarray<BoundResource>();
}
