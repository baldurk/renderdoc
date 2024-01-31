/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2020-2024 Baldur Karlsson
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

#include <QMetaObject>
#include "Interface/QRDInterface.h"

class QDialog;

class MiniQtHelper : public IMiniQtHelper
{
public:
  MiniQtHelper(ICaptureContext &ctx);
  virtual ~MiniQtHelper();

  void InvokeOntoUIThread(std::function<void()> callback) override;

  QWidget *CreateToplevelWidget(const rdcstr &windowTitle, WidgetCallback closed) override;
  void CloseToplevelWidget(QWidget *widget) override;

  // widget hierarchy

  void SetWidgetName(QWidget *widget, const rdcstr &name) override;
  rdcstr GetWidgetName(QWidget *widget) override;
  rdcstr GetWidgetType(QWidget *widget) override;
  QWidget *FindChildByName(QWidget *parent, const rdcstr &name) override;
  QWidget *GetParent(QWidget *widget) override;
  int32_t GetNumChildren(QWidget *widget) override;
  QWidget *GetChild(QWidget *parent, int32_t index) override;
  void DestroyWidget(QWidget *widget) override;

  // dialogs

  bool ShowWidgetAsDialog(QWidget *widget) override;
  void CloseCurrentDialog(bool success) override;

  // layout functions

  QWidget *CreateHorizontalContainer() override;
  QWidget *CreateVerticalContainer() override;
  QWidget *CreateGridContainer() override;
  QWidget *CreateSpacer(bool horizontal) override;

  void ClearContainedWidgets(QWidget *parent) override;
  void AddGridWidget(QWidget *parent, int32_t row, int32_t column, QWidget *child, int32_t rowSpan,
                     int32_t columnSpan) override;
  void AddWidget(QWidget *parent, QWidget *child) override;
  void InsertWidget(QWidget *parent, int32_t index, QWidget *child) override;

  // widget manipulation

  void SetWidgetText(QWidget *widget, const rdcstr &text) override;
  rdcstr GetWidgetText(QWidget *widget) override;

  void SetWidgetFont(QWidget *widget, const rdcstr &font, int32_t fontSize, bool bold,
                     bool italic) override;

  void SetWidgetEnabled(QWidget *widget, bool enabled) override;
  bool IsWidgetEnabled(QWidget *widget) override;

  void SetWidgetVisible(QWidget *widget, bool visible) override;
  bool IsWidgetVisible(QWidget *widget) override;

  // specific widgets

  QWidget *CreateGroupBox(bool collapsible) override;

  QWidget *CreateButton(WidgetCallback pressed) override;

  QWidget *CreateLabel() override;
  void SetLabelImage(QWidget *widget, const bytebuf &data, int32_t width, int32_t height,
                     bool alpha) override;

  QWidget *CreateOutputRenderingWidget() override;

  WindowingData GetWidgetWindowingData(QWidget *widget) override;
  void SetWidgetReplayOutput(QWidget *widget, IReplayOutput *output) override;
  void SetWidgetBackgroundColor(QWidget *widget, float red, float green, float blue) override;

  QWidget *CreateCheckbox(WidgetCallback changed) override;
  QWidget *CreateRadiobox(WidgetCallback changed) override;

  void SetWidgetChecked(QWidget *checkableWidget, bool checked) override;
  bool IsWidgetChecked(QWidget *checkableWidget) override;

  QWidget *CreateSpinbox(int32_t decimalPlaces, double step) override;

  void SetSpinboxBounds(QWidget *spinbox, double minVal, double maxVal) override;
  void SetSpinboxValue(QWidget *spinbox, double value) override;
  double GetSpinboxValue(QWidget *spinbox) override;

  QWidget *CreateTextBox(bool singleLine, WidgetCallback changed) override;

  QWidget *CreateComboBox(bool editable, WidgetCallback changed) override;

  void SetComboOptions(QWidget *combo, const rdcarray<rdcstr> &options) override;
  size_t GetComboCount(QWidget *combo) override;
  void SelectComboOption(QWidget *combo, const rdcstr &option) override;

  QWidget *CreateProgressBar(bool horizontal) override;

  void ResetProgressBar(QWidget *pbar) override;
  void SetProgressBarValue(QWidget *pbar, int32_t value) override;
  void UpdateProgressBarValue(QWidget *pbar, int32_t delta) override;
  int32_t GetProgressBarValue(QWidget *pbar) override;
  void SetProgressBarRange(QWidget *pbar, int32_t minimum, int32_t maximum) override;
  int32_t GetProgressBarMinimum(QWidget *pbar) override;
  int32_t GetProgressBarMaximum(QWidget *pbar) override;

private:
  ICaptureContext &m_Ctx;

  QDialog *m_CurrentDialog = NULL;
  rdcarray<rdcpair<QWidget *, QMetaObject::Connection>> m_Connections;

  void AddWidgetCallback(QWidget *widget, QMetaObject::Connection connection);
};
