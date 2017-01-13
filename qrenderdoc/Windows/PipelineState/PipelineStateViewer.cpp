/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2016-2017 Baldur Karlsson
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

#include "PipelineStateViewer.h"
#include "D3D11PipelineStateViewer.h"
#include "D3D12PipelineStateViewer.h"
#include "GLPipelineStateViewer.h"
#include "VulkanPipelineStateViewer.h"
#include "ui_PipelineStateViewer.h"

PipelineStateViewer::PipelineStateViewer(CaptureContext *ctx, QWidget *parent)
    : QFrame(parent), ui(new Ui::PipelineStateViewer), m_Ctx(ctx)
{
  ui->setupUi(this);

  m_D3D11 = NULL;
  m_D3D12 = NULL;
  m_GL = NULL;
  m_Vulkan = NULL;

  m_Current = NULL;

  m_Ctx->AddLogViewer(this);

  setToD3D11();
}

PipelineStateViewer::~PipelineStateViewer()
{
  reset();

  m_Ctx->windowClosed(this);
  m_Ctx->RemoveLogViewer(this);

  delete ui;
}

void PipelineStateViewer::OnLogfileLoaded()
{
  if(m_Ctx->APIProps().pipelineType == eGraphicsAPI_D3D11)
    setToD3D11();
  else if(m_Ctx->APIProps().pipelineType == eGraphicsAPI_D3D12)
    setToD3D12();
  else if(m_Ctx->APIProps().pipelineType == eGraphicsAPI_OpenGL)
    setToGL();
  else if(m_Ctx->APIProps().pipelineType == eGraphicsAPI_Vulkan)
    setToVulkan();

  if(m_Current)
    m_Current->OnLogfileLoaded();
}

void PipelineStateViewer::OnLogfileClosed()
{
  if(m_Current)
    m_Current->OnLogfileClosed();
}

void PipelineStateViewer::OnEventChanged(uint32_t eventID)
{
  if(m_Current)
    m_Current->OnEventChanged(eventID);
}

QVariant PipelineStateViewer::persistData()
{
  QVariantMap state;

  if(m_Current == m_D3D11)
    state["type"] = "D3D11";
  else if(m_Current == m_D3D12)
    state["type"] = "D3D12";
  else if(m_Current == m_GL)
    state["type"] = "GL";
  else if(m_Current == m_Vulkan)
    state["type"] = "Vulkan";
  else
    state["type"] = "";

  return state;
}

void PipelineStateViewer::setPersistData(const QVariant &persistData)
{
  QString str = persistData.toMap()["type"].toString();

  if(str == "D3D11")
    setToD3D11();
  else if(str == "D3D12")
    setToD3D12();
  else if(str == "GL")
    setToGL();
  else if(str == "Vulkan")
    setToVulkan();
}

void PipelineStateViewer::reset()
{
  delete m_D3D11;
  delete m_D3D12;
  delete m_GL;
  delete m_Vulkan;

  m_D3D11 = NULL;
  m_D3D12 = NULL;
  m_GL = NULL;
  m_Vulkan = NULL;

  m_Current = NULL;
}

void PipelineStateViewer::setToD3D11()
{
  if(m_D3D11)
    return;

  reset();

  m_D3D11 = new D3D11PipelineStateViewer(m_Ctx, this);
  ui->layout->addWidget(m_D3D11);
  m_Current = m_D3D11;
  m_Ctx->CurPipelineState.DefaultType = eGraphicsAPI_D3D11;
}

void PipelineStateViewer::setToD3D12()
{
  if(m_D3D12)
    return;

  reset();

  m_D3D12 = new D3D12PipelineStateViewer(m_Ctx, this);
  ui->layout->addWidget(m_D3D12);
  m_Current = m_D3D12;
  m_Ctx->CurPipelineState.DefaultType = eGraphicsAPI_D3D12;
}

void PipelineStateViewer::setToGL()
{
  if(m_GL)
    return;

  reset();

  m_GL = new GLPipelineStateViewer(m_Ctx, this);
  ui->layout->addWidget(m_GL);
  m_Current = m_GL;
  m_Ctx->CurPipelineState.DefaultType = eGraphicsAPI_OpenGL;
}

void PipelineStateViewer::setToVulkan()
{
  if(m_Vulkan)
    return;

  reset();

  m_Vulkan = new VulkanPipelineStateViewer(m_Ctx, this);
  ui->layout->addWidget(m_Vulkan);
  m_Current = m_Vulkan;
  m_Ctx->CurPipelineState.DefaultType = eGraphicsAPI_Vulkan;
}
