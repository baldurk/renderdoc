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
class GLPipelineStateViewer;
}

class RDTreeWidget;
class QTreeWidgetItem;

class GLPipelineStateViewer : public QFrame, public ILogViewerForm
{
  Q_OBJECT

public:
  explicit GLPipelineStateViewer(CaptureContext &ctx, QWidget *parent = 0);
  ~GLPipelineStateViewer();

  void OnLogfileLoaded();
  void OnLogfileClosed();
  void OnSelectedEventChanged(uint32_t eventID) {}
  void OnEventChanged(uint32_t eventID);

private slots:
  // automatic slots
  void on_showDisabled_toggled(bool checked);
  void on_showEmpty_toggled(bool checked);
  void on_exportHTML_clicked();
  void on_meshView_clicked();
  void on_viAttrs_itemActivated(QTreeWidgetItem *item, int column);
  void on_viBuffers_itemActivated(QTreeWidgetItem *item, int column);
  void on_viAttrs_mouseMove(QMouseEvent *event);
  void on_viBuffers_mouseMove(QMouseEvent *event);

  // manual slots
  void shaderView_clicked();
  void shaderEdit_clicked();
  void shaderSave_clicked();
  void resource_itemActivated(QTreeWidgetItem *item, int column);
  void ubo_itemActivated(QTreeWidgetItem *item, int column);
  void vertex_leave(QEvent *e);

private:
  Ui::GLPipelineStateViewer *ui;
  CaptureContext &m_Ctx;

  enum class GLReadWriteType
  {
    Atomic,
    SSBO,
    Image,
  };

  QString MakeGenericValueString(uint32_t compCount, FormatComponentType compType,
                                 const GLPipelineState::VertexInput::VertexAttribute &val);
  GLReadWriteType GetGLReadWriteType(ShaderResource res);

  void setShaderState(const GLPipelineState::ShaderStage &stage, QLabel *shader, RDTreeWidget *tex,
                      RDTreeWidget *samp, RDTreeWidget *ubo, RDTreeWidget *sub, RDTreeWidget *rw);
  void clearShaderState(QLabel *shader, RDTreeWidget *tex, RDTreeWidget *samp, RDTreeWidget *ubo,
                        RDTreeWidget *sub, RDTreeWidget *rw);
  void setState();
  void clearState();

  void setInactiveRow(QTreeWidgetItem *node);
  void setEmptyRow(QTreeWidgetItem *node);
  void highlightIABind(int slot);

  QString formatMembers(int indent, const QString &nameprefix,
                        const rdctype::array<ShaderConstant> &vars);
  const GLPipelineState::ShaderStage *stageForSender(QWidget *widget);

  bool showNode(bool usedSlot, bool filledSlot);

  // keep track of the VB nodes (we want to be able to highlight them easily on hover)
  QList<QTreeWidgetItem *> m_VBNodes;
};
