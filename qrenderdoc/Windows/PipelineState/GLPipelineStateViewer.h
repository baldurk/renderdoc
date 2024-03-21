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
#include "Code/Interface/QRDInterface.h"

namespace Ui
{
class GLPipelineStateViewer;
}

class QXmlStreamWriter;

class RDLabel;
class RDTreeWidget;
class RDTreeWidgetItem;
class PipelineStateViewer;

enum class GLReadWriteType
{
  Atomic,
  SSBO,
  Image,
};

class GLPipelineStateViewer : public QFrame, public ICaptureViewer
{
  Q_OBJECT

public:
  explicit GLPipelineStateViewer(ICaptureContext &ctx, PipelineStateViewer &common,
                                 QWidget *parent = 0);
  ~GLPipelineStateViewer();

  // ICaptureViewer
  void OnCaptureLoaded() override;
  void OnCaptureClosed() override;
  void OnSelectedEventChanged(uint32_t eventId) override {}
  void OnEventChanged(uint32_t eventId) override;

  void SelectPipelineStage(PipelineStage stage);
  ResourceId GetResource(RDTreeWidgetItem *item);

private slots:
  // automatic slots
  void on_showUnused_toggled(bool checked);
  void on_showEmpty_toggled(bool checked);
  void on_exportHTML_clicked();
  void on_meshView_clicked();
  void on_viAttrs_itemActivated(RDTreeWidgetItem *item, int column);
  void on_viBuffers_itemActivated(RDTreeWidgetItem *item, int column);
  void on_viAttrs_mouseMove(QMouseEvent *event);
  void on_viBuffers_mouseMove(QMouseEvent *event);
  void on_pipeFlow_stageSelected(int index);

  // manual slots
  void shaderView_clicked();
  void shaderSave_clicked();
  void resource_itemActivated(RDTreeWidgetItem *item, int column);
  void ubo_itemActivated(RDTreeWidgetItem *item, int column);
  void vertex_leave(QEvent *e);

private:
  Ui::GLPipelineStateViewer *ui;
  ICaptureContext &m_Ctx;
  PipelineStateViewer &m_Common;

  QString MakeGenericValueString(uint32_t compCount, CompType compType,
                                 const GLPipe::VertexAttribute &val);
  GLReadWriteType GetGLReadWriteType(ShaderResource res);

  void setShaderState(const GLPipe::Shader &stage, RDLabel *shader, RDTreeWidget *sub);

  void addUBORow(const Descriptor &descriptor, uint32_t reg, uint32_t index,
                 const ConstantBlock *shaderBind, bool usedSlot, RDTreeWidget *ubos);
  void addImageSamplerRow(const Descriptor &descriptor, const SamplerDescriptor &samplerDescriptor,
                          uint32_t reg, const ShaderResource *shaderTex,
                          const ShaderSampler *shaderSamp, bool usedSlot,
                          const GLPipe::TextureCompleteness *texCompleteness,
                          RDTreeWidgetItem *textures, RDTreeWidgetItem *samplers);
  void addReadWriteRow(const Descriptor &descriptor, uint32_t reg, uint32_t index,
                       const ShaderResource *shaderBind, bool usedSlot,
                       const GLPipe::TextureCompleteness *texCompleteness,
                       RDTreeWidgetItem *readwrites);

  void clearShaderState(RDLabel *shader, RDTreeWidget *tex, RDTreeWidget *samp, RDTreeWidget *ubo,
                        RDTreeWidget *sub, RDTreeWidget *rw);
  void setState();
  void clearState();

  bool isInactiveRow(RDTreeWidgetItem *node);
  void setInactiveRow(RDTreeWidgetItem *node);
  void setEmptyRow(RDTreeWidgetItem *node);
  void highlightIABind(int slot);

  QString formatMembers(int indent, const QString &nameprefix, const rdcarray<ShaderConstant> &vars);
  const GLPipe::Shader *stageForSender(QWidget *widget);

  bool showNode(bool usedSlot, bool filledSlot);

  void setViewDetails(RDTreeWidgetItem *node, TextureDescription *tex, uint32_t firstMip,
                      uint32_t numMips, uint32_t firstSlice, uint32_t numSlices,
                      const GLPipe::TextureCompleteness *texCompleteness = NULL);

  void exportHTML(QXmlStreamWriter &xml, const GLPipe::VertexInput &vtx);
  void exportHTML(QXmlStreamWriter &xml, const GLPipe::Shader &sh);
  void exportHTML(QXmlStreamWriter &xml, const GLPipe::Feedback &xfb);
  void exportHTML(QXmlStreamWriter &xml, const GLPipe::Rasterizer &rs);
  void exportHTML(QXmlStreamWriter &xml, const GLPipe::FrameBuffer &fb);

  rdcarray<DescriptorLogicalLocation> m_Locations;
  rdcarray<Descriptor> m_Descriptors;
  rdcarray<SamplerDescriptor> m_SamplerDescriptors;

  // keep track of the VB nodes (we want to be able to highlight them easily on hover)
  QList<RDTreeWidgetItem *> m_VBNodes;
  // list of empty VB nodes that shouldn't be highlighted on hover
  QList<RDTreeWidgetItem *> m_EmptyNodes;
};
