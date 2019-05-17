/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2016-2019 Baldur Karlsson
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
#include "Code/Interface/QRDInterface.h"

namespace Ui
{
class D3D11PipelineStateViewer;
}

class QXmlStreamWriter;
class RDLabel;
class RDTreeWidget;
class RDTreeWidgetItem;
struct D3D11ViewTag;
class PipelineStateViewer;

class D3D11PipelineStateViewer : public QFrame, public ICaptureViewer
{
  Q_OBJECT

public:
  explicit D3D11PipelineStateViewer(ICaptureContext &ctx, PipelineStateViewer &common,
                                    QWidget *parent = 0);
  ~D3D11PipelineStateViewer();

  // ICaptureViewer
  void OnCaptureLoaded() override;
  void OnCaptureClosed() override;
  void OnSelectedEventChanged(uint32_t eventId) override {}
  void OnEventChanged(uint32_t eventId) override;

private slots:
  // automatic slots
  void on_showUnused_toggled(bool checked);
  void on_showEmpty_toggled(bool checked);
  void on_exportHTML_clicked();
  void on_meshView_clicked();
  void on_iaLayouts_itemActivated(RDTreeWidgetItem *item, int column);
  void on_iaBuffers_itemActivated(RDTreeWidgetItem *item, int column);
  void on_iaLayouts_mouseMove(QMouseEvent *event);
  void on_iaBuffers_mouseMove(QMouseEvent *event);
  void on_pipeFlow_stageSelected(int index);

  // manual slots
  void shaderView_clicked();

  void shaderSave_clicked();
  void resource_itemActivated(RDTreeWidgetItem *item, int column);
  void cbuffer_itemActivated(RDTreeWidgetItem *item, int column);
  void vertex_leave(QEvent *e);

  void on_debugThread_clicked();

private:
  Ui::D3D11PipelineStateViewer *ui;
  ICaptureContext &m_Ctx;
  PipelineStateViewer &m_Common;

  void setShaderState(const D3D11Pipe::Shader &stage, RDLabel *shader, RDTreeWidget *tex,
                      RDTreeWidget *samp, RDTreeWidget *cbuffer, RDTreeWidget *classes);

  void addResourceRow(const D3D11ViewTag &view, const ShaderResource *shaderInput,
                      const Bindpoint *map, RDTreeWidget *resources);

  void clearShaderState(RDLabel *shader, RDTreeWidget *tex, RDTreeWidget *samp,
                        RDTreeWidget *cbuffer, RDTreeWidget *classes);
  void setState();
  void clearState();

  QVariantList exportViewHTML(const D3D11Pipe::View &view, int i, ShaderReflection *refl,
                              const QString &extraParams);
  void exportHTML(QXmlStreamWriter &xml, const D3D11Pipe::InputAssembly &ia);
  void exportHTML(QXmlStreamWriter &xml, const D3D11Pipe::Shader &sh);
  void exportHTML(QXmlStreamWriter &xml, const D3D11Pipe::StreamOut &so);
  void exportHTML(QXmlStreamWriter &xml, const D3D11Pipe::Rasterizer &rs);
  void exportHTML(QXmlStreamWriter &xml, const D3D11Pipe::OutputMerger &om);

  void setInactiveRow(RDTreeWidgetItem *node);
  void setEmptyRow(RDTreeWidgetItem *node);
  void highlightIABind(int slot);

  const D3D11Pipe::Shader *stageForSender(QWidget *widget);

  bool HasImportantViewParams(const D3D11Pipe::View &view, TextureDescription *tex);
  bool HasImportantViewParams(const D3D11Pipe::View &view, BufferDescription *buf);

  void setViewDetails(RDTreeWidgetItem *node, const D3D11ViewTag &view, TextureDescription *tex);
  void setViewDetails(RDTreeWidgetItem *node, const D3D11ViewTag &view, BufferDescription *buf);

  bool showNode(bool usedSlot, bool filledSlot);

  // keep track of the VB nodes (we want to be able to highlight them easily on hover)
  QList<RDTreeWidgetItem *> m_VBNodes;
  // list of empty VB nodes that shouldn't be highlighted on hover
  QList<RDTreeWidgetItem *> m_EmptyNodes;
};
