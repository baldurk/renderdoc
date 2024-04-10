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

#include <QDir>
#include <QFrame>
#include "Code/Interface/QRDInterface.h"

namespace Ui
{
class VulkanPipelineStateViewer;
}

class QXmlStreamWriter;

class ComputeDebugSelector;
class RDLabel;
class RDTreeWidget;
class RDTreeWidgetItem;
class PipelineStateViewer;

struct CombinedSamplerData
{
  CombinedSamplerData() : node(NULL) {}
  QList<RDTreeWidgetItem *> images;
  RDTreeWidgetItem *node;
};

class VulkanPipelineStateViewer : public QFrame, public ICaptureViewer
{
  Q_OBJECT

public:
  explicit VulkanPipelineStateViewer(ICaptureContext &ctx, PipelineStateViewer &common,
                                     QWidget *parent = 0);
  ~VulkanPipelineStateViewer();

  void OnCaptureLoaded();
  void OnCaptureClosed();
  void OnSelectedEventChanged(uint32_t eventId) {}
  void OnEventChanged(uint32_t eventId);

  void SelectPipelineStage(PipelineStage stage);
  ResourceId GetResource(RDTreeWidgetItem *item);

private slots:
  // automatic slots
  void on_showUnused_toggled(bool checked);
  void on_showEmpty_toggled(bool checked);

  void on_meshView_clicked();
  void on_msMeshButton_clicked();
  void on_viAttrs_itemActivated(RDTreeWidgetItem *item, int column);
  void on_viBuffers_itemActivated(RDTreeWidgetItem *item, int column);
  void on_viAttrs_mouseMove(QMouseEvent *event);
  void on_viBuffers_mouseMove(QMouseEvent *event);
  void on_pipeFlow_stageSelected(int index);

  // manual slots
  void shaderView_clicked();
  void shaderSave_clicked();
  void shaderMessages_clicked();
  void predicateBufferView_clicked();
  void resource_itemActivated(RDTreeWidgetItem *item, int column);
  void resource_hoverItemChanged(RDTreeWidgetItem *hover);
  void ubo_itemActivated(RDTreeWidgetItem *item, int column);
  void descSet_itemActivated(RDTreeWidgetItem *item, int column);
  void vertex_leave(QEvent *e);

  void on_computeDebugSelector_clicked();
  void computeDebugSelector_beginDebug(const rdcfixedarray<uint32_t, 3> &group,
                                       const rdcfixedarray<uint32_t, 3> &thread);

  void exportHTML_clicked();
  void exportFOZ_clicked();

private:
  Ui::VulkanPipelineStateViewer *ui;
  ICaptureContext &m_Ctx;
  PipelineStateViewer &m_Common;
  ComputeDebugSelector *m_ComputeDebugSelector;

  void setOldMeshPipeFlow();
  void setNewMeshPipeFlow();

  QVariantList makeSampler(const QString &slotname, const SamplerDescriptor &descriptor);
  void addResourceRow(const ShaderResource *shaderRes, const ShaderSampler *shaderSamp,
                      const UsedDescriptor &used, uint32_t dynamicOffset, RDTreeWidget *resources,
                      QMap<ResourceId, RDTreeWidgetItem *> &samplers);
  void addConstantBlockRow(const ConstantBlock *cblock, const UsedDescriptor &used,
                           uint32_t dynamicOffset, RDTreeWidget *ubos);

  void setShaderState(const VKPipe::Pipeline &pipe, const VKPipe::Shader &stage, RDLabel *shader,
                      RDLabel *pipeLayout, RDTreeWidget *descSets);
  void clearShaderState(RDLabel *shader, RDLabel *pipeLayout, RDTreeWidget *resources,
                        RDTreeWidget *cbuffers, RDTreeWidget *descSets);
  void setState();
  void clearState();

  void setInactiveRow(RDTreeWidgetItem *node);
  void setEmptyRow(RDTreeWidgetItem *node);
  void highlightIABind(int slot);

  bool IsPushSet(ShaderStage stage, ResourceId id);

  uint32_t getMinOffset(const rdcarray<ShaderConstant> &variables);

  QString formatByteRange(const BufferDescription *buf, const Descriptor &descriptor,
                          uint32_t dynamicOffset);
  QString formatMembers(int indent, const QString &nameprefix, const rdcarray<ShaderConstant> &vars);
  const VKPipe::Shader *stageForSender(QWidget *widget);

  bool setViewDetails(RDTreeWidgetItem *node, const Descriptor &descriptor, TextureDescription *tex,
                      const QString &hiddenCombinedSampler, bool includeSampleLocations = false,
                      bool includeOffsets = false);

  bool setViewDetails(RDTreeWidgetItem *node, const Descriptor &descriptor, BufferDescription *buf);

  bool showNode(bool usedSlot, bool filledSlot);

  bool m_ShowUnused = false;
  bool m_ShowEmpty = false;

  void exportHTML(QXmlStreamWriter &xml, const VKPipe::VertexInput &vi);
  void exportHTML(QXmlStreamWriter &xml, const VKPipe::InputAssembly &ia);
  void exportHTML(QXmlStreamWriter &xml, const VKPipe::Shader &sh);
  void exportHTML(QXmlStreamWriter &xml, const VKPipe::TransformFeedback &rs);
  void exportHTML(QXmlStreamWriter &xml, const VKPipe::Rasterizer &rs);
  void exportHTML(QXmlStreamWriter &xml, const VKPipe::ColorBlendState &cb);
  void exportHTML(QXmlStreamWriter &xml, const VKPipe::DepthStencil &ds);
  void exportHTML(QXmlStreamWriter &xml, const VKPipe::CurrentPass &pass);
  void exportHTML(QXmlStreamWriter &xml, const VKPipe::ConditionalRendering &cr);

  const ShaderResource *exportDescriptorHTML(const UsedDescriptor &used, const ShaderReflection *refl,
                                             const Descriptor &descriptor, uint32_t dynamicOffset,
                                             QList<QVariantList> &rows);

  QString GetFossilizeHash(ResourceId id);
  QString GetFossilizeFilename(QDir d, uint32_t tag, ResourceId id);
  QVariant ConvertSDObjectToFossilizeJSON(const SDObject *obj, QMap<QByteArray, QByteArray> renames);
  void AddFossilizeNexts(QVariantMap &info, const SDObject *baseStruct);
  QByteArray ReconstructSpecializationData(const VKPipe::Shader &sh, const SDObject *mapEntries);
  QString GetBufferForFossilize(const SDObject *obj);
  void EncodeFossilizeVarint(const bytebuf &spirv, bytebuf &varint);
  void WriteFossilizeJSON(QIODevice &f, QVariantMap &contents);

  void exportFOZ(QString dir, ResourceId pso);

  QMenu *m_ExportMenu = NULL;

  QAction *m_ExportHTML = NULL;
  QAction *m_ExportFOZ = NULL;

  // keep track of the VB nodes (we want to be able to highlight them easily on hover)
  QList<RDTreeWidgetItem *> m_VBNodes;
  QList<RDTreeWidgetItem *> m_BindNodes;
  // list of empty VB nodes that shouldn't be highlighted on hover
  QList<RDTreeWidgetItem *> m_EmptyNodes;

  // from an combined image to its sampler (since we de-duplicate)
  QMap<RDTreeWidgetItem *, RDTreeWidgetItem *> m_CombinedImageSamplers;

  QMap<QPair<ResourceId, uint32_t>, DescriptorLogicalLocation> m_Locations;

  bool m_MeshPipe = false;
};
