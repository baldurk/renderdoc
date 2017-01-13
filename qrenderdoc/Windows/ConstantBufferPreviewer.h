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
class ConstantBufferPreviewer;
}

struct FormatElement;

class ConstantBufferPreviewer : public QFrame, public ILogViewerForm
{
  Q_OBJECT

public:
  explicit ConstantBufferPreviewer(CaptureContext *ctx, const ShaderStageType stage, uint32_t slot,
                                   uint32_t idx, QWidget *parent = 0);
  ~ConstantBufferPreviewer();

  static ConstantBufferPreviewer *has(ShaderStageType stage, uint32_t slot, uint32_t idx);

  void OnLogfileLoaded();
  void OnLogfileClosed();
  void OnSelectedEventChanged(uint32_t eventID) {}
  void OnEventChanged(uint32_t eventID);

private slots:
  // automatic slots
  void on_setFormat_toggled(bool checked);
  void on_saveCSV_clicked();

  // manual slots
  void processFormat(const QString &format);

private:
  Ui::ConstantBufferPreviewer *ui;
  CaptureContext *m_Ctx = NULL;

  ResourceId m_cbuffer;
  ResourceId m_shader;
  ShaderStageType m_stage = eShaderStage_Vertex;
  uint32_t m_slot = 0;
  uint32_t m_arrayIdx = 0;

  rdctype::array<ShaderVariable> applyFormatOverride(const rdctype::array<byte> &data);

  void addVariables(QTreeWidgetItem *root, const rdctype::array<ShaderVariable> &vars);
  void setVariables(const rdctype::array<ShaderVariable> &vars);

  void updateLabels();

  static QList<ConstantBufferPreviewer *> m_Previews;

  QList<FormatElement> m_formatOverride;
};
