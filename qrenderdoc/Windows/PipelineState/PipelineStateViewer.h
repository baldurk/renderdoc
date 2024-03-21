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

#pragma once

#include <QFrame>
#include <QLabel>
#include "Code/Interface/QRDInterface.h"
#include "Widgets/Extended/RDTreeView.h"

namespace Ui
{
class PipelineStateViewer;
}

class QXmlStreamWriter;

class QToolButton;
class QMenu;
class RDLabel;
class RDTreeWidgetItem;
class RDTreeWidget;
class CustomPaintWidget;

class D3D11PipelineStateViewer;
class D3D12PipelineStateViewer;
class GLPipelineStateViewer;
class VulkanPipelineStateViewer;

class PipelineStateViewer;

struct ScopedTreeUpdater
{
  ScopedTreeUpdater(RDTreeWidget *widget);
  ~ScopedTreeUpdater();

  RDTreeWidget *m_Widget;
  int vs;
};

class RDPreviewTooltip : public QFrame, public ITreeViewTipDisplay
{
private:
  Q_OBJECT

  PipelineStateViewer *pipe = NULL;
  QLabel *title = NULL;
  QLabel *label = NULL;
  ICaptureContext &m_Ctx;

public:
  explicit RDPreviewTooltip(PipelineStateViewer *parent, CustomPaintWidget *thumbnail,
                            ICaptureContext &ctx);

  void hideTip();
  QSize configureTip(QWidget *widget, QModelIndex idx, QString text);
  void showTip(QPoint pos);
  bool forceTip(QWidget *widget, QModelIndex idx);

protected:
  void paintEvent(QPaintEvent *);
  void resizeEvent(QResizeEvent *);
};

class PipelineStateViewer : public QFrame, public IPipelineStateViewer, public ICaptureViewer
{
  Q_OBJECT

  Q_PROPERTY(QVariant persistData READ persistData WRITE setPersistData DESIGNABLE false SCRIPTABLE false)

public:
  explicit PipelineStateViewer(ICaptureContext &ctx, QWidget *parent = 0);
  ~PipelineStateViewer();

  // IPipelineStateViewer
  QWidget *Widget() override { return this; }
  bool SaveShaderFile(const ShaderReflection *shader) override;
  void SelectPipelineStage(PipelineStage stage) override;

  // ICaptureViewer
  void OnCaptureLoaded() override;
  void OnCaptureClosed() override;
  void OnSelectedEventChanged(uint32_t eventId) override {}
  void OnEventChanged(uint32_t eventId) override;

  QVariant persistData();
  void setPersistData(const QVariant &persistData);

  void SetStencilLabelValue(QLabel *label, uint8_t value);
  void SetStencilTreeItemValue(RDTreeWidgetItem *item, int column, uint8_t value);

  void SetupShaderEditButton(QToolButton *button, ResourceId pipelineId, ResourceId shaderId,
                             const ShaderReflection *shaderDetails);

  void SetupResourceView(RDTreeWidget *view);

  QString GetVBufferFormatString(uint32_t slot);

  QColor GetViewDetailsColor();

  void setTopologyDiagram(QLabel *diagram, Topology topo);
  void setMeshViewPixmap(RDLabel *meshView);

  ResourceId updateThumbnail(QWidget *widget, QModelIndex idx);
  bool hasThumbnail(QWidget *widget, QModelIndex idx);

  QXmlStreamWriter *beginHTMLExport();
  void exportHTMLTable(QXmlStreamWriter &xml, const QStringList &cols,
                       const QList<QVariantList> &rows);
  void exportHTMLTable(QXmlStreamWriter &xml, const QStringList &cols, const QVariantList &row);
  void endHTMLExport(QXmlStreamWriter *xml);

public slots:
  void shaderEdit_clicked();

private:
  void showEvent(QShowEvent *event) override;

  Ui::PipelineStateViewer *ui;
  ICaptureContext &m_Ctx;

  QMenu *editMenus[NumShaderStages] = {};

  RDPreviewTooltip *m_Tooltip = NULL;

  TextureDisplay m_TexDisplay;
  IReplayOutput *m_Output = NULL;

  void RT_UpdateAndDisplay(IReplayController *r);

  void AddResourceUsageEntry(QMenu &menu, uint32_t start, uint32_t end, ResourceUsage usage);
  void ShowResourceContextMenu(RDTreeWidget *widget, const QPoint &pos, ResourceId id,
                               const rdcarray<EventUsage> &usage);

  QString GenerateHLSLStub(const ShaderReflection *shaderDetails, const QString &entryFunc);
  IShaderViewer *EditShader(ResourceId id, ShaderStage shaderType, const rdcstr &entry,
                            ShaderCompileFlags compileFlags, KnownShaderTool knownTool,
                            ShaderEncoding shaderEncoding, const rdcstrpairs &files);
  IShaderViewer *EditOriginalShaderSource(ResourceId id, const ShaderReflection *shaderDetails);
  IShaderViewer *EditDecompiledSource(const ShaderProcessingTool &tool, ResourceId id,
                                      const ShaderReflection *shaderDetails);

  void MakeShaderVariablesHLSL(bool cbufferContents, const rdcarray<ShaderConstant> &vars,
                               QString &struct_contents, QString &struct_defs);

  QPixmap m_TopoPixmaps[(int)Topology::PatchList + 1];

  void setToD3D11();
  void setToD3D12();
  void setToGL();
  void setToVulkan();
  void reset();

  QString GetCurrentAPI();

  D3D11PipelineStateViewer *m_D3D11;
  D3D12PipelineStateViewer *m_D3D12;
  GLPipelineStateViewer *m_GL;
  VulkanPipelineStateViewer *m_Vulkan;
  ICaptureViewer *m_Current;
};
