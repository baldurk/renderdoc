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
#include <QLabel>
#include "Code/Interface/QRDInterface.h"

namespace Ui
{
class PipelineStateViewer;
}

class QXmlStreamWriter;

class QToolButton;
class QMenu;
class RDLabel;

class D3D11PipelineStateViewer;
class D3D12PipelineStateViewer;
class GLPipelineStateViewer;
class VulkanPipelineStateViewer;

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

  // ICaptureViewer
  void OnCaptureLoaded() override;
  void OnCaptureClosed() override;
  void OnSelectedEventChanged(uint32_t eventId) override {}
  void OnEventChanged(uint32_t eventId) override;

  QVariant persistData();
  void setPersistData(const QVariant &persistData);

  void SetupShaderEditButton(QToolButton *button, ResourceId pipelineId, ResourceId shaderId,
                             const ShaderReflection *shaderDetails);

  QString GenerateBufferFormatter(const ShaderResource &res, const ResourceFormat &viewFormat,
                                  uint64_t &baseByteOffset);

  QString GetVBufferFormatString(uint32_t slot);

  void setTopologyDiagram(QLabel *diagram, Topology topo);
  void setMeshViewPixmap(RDLabel *meshView);

  QXmlStreamWriter *beginHTMLExport();
  void exportHTMLTable(QXmlStreamWriter &xml, const QStringList &cols,
                       const QList<QVariantList> &rows);
  void exportHTMLTable(QXmlStreamWriter &xml, const QStringList &cols, const QVariantList &row);
  void endHTMLExport(QXmlStreamWriter *xml);

public slots:
  void shaderEdit_clicked();

private:
  Ui::PipelineStateViewer *ui;
  ICaptureContext &m_Ctx;

  QMenu *editMenus[6] = {};

  QString declareStruct(QList<QString> &declaredStructs, const QString &name,
                        const rdcarray<ShaderConstant> &members, uint32_t requiredByteStride);

  QString GenerateHLSLStub(const ShaderReflection *shaderDetails, const QString &entryFunc);
  IShaderViewer *EditShader(ResourceId id, ShaderStage shaderType, const rdcstr &entry,
                            ShaderCompileFlags compileFlags, ShaderEncoding encoding,
                            const rdcstrpairs &files);
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
