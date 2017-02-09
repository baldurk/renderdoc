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

#pragma once

#include <QFrame>
#include "Code/CaptureContext.h"

namespace Ui
{
class PipelineStateViewer;
}

class D3D11PipelineStateViewer;
class D3D12PipelineStateViewer;
class GLPipelineStateViewer;
class VulkanPipelineStateViewer;

class PipelineStateViewer : public QFrame, public ILogViewerForm
{
  Q_OBJECT

  Q_PROPERTY(QVariant persistData READ persistData WRITE setPersistData DESIGNABLE false SCRIPTABLE false)

public:
  explicit PipelineStateViewer(CaptureContext &ctx, QWidget *parent = 0);
  ~PipelineStateViewer();

  void OnLogfileLoaded();
  void OnLogfileClosed();
  void OnSelectedEventChanged(uint32_t eventID) {}
  void OnEventChanged(uint32_t eventID);

  QVariant persistData();
  void setPersistData(const QVariant &persistData);

private:
  Ui::PipelineStateViewer *ui;
  CaptureContext &m_Ctx;

  void setToD3D11();
  void setToD3D12();
  void setToGL();
  void setToVulkan();
  void reset();

  D3D11PipelineStateViewer *m_D3D11;
  D3D12PipelineStateViewer *m_D3D12;
  GLPipelineStateViewer *m_GL;
  VulkanPipelineStateViewer *m_Vulkan;
  ILogViewerForm *m_Current;
};
