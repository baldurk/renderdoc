/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2020 Baldur Karlsson
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

  QWidget *CreateToplevelWidget(const rdcstr &windowTitle) override;

  // widget hierarchy

  void SetWidgetName(QWidget *widget, const rdcstr &name) override;
  rdcstr GetWidgetName(QWidget *widget) override;
  rdcstr GetWidgetType(QWidget *widget) override;
  QWidget *FindChildByName(QWidget *parent, const rdcstr &name) override;
  QWidget *GetParent(QWidget *widget) override;
  int GetNumChildren(QWidget *widget) override;
  QWidget *GetChild(QWidget *parent, int index) override;

  // dialogs

  bool ShowWidgetAsDialog(QWidget *widget) override;
  void CloseCurrentDialog(bool success) override;

  // layout functions

  QWidget *CreateHorizontalContainer() override;
  QWidget *CreateVerticalContainer() override;
  QWidget *CreateGridContainer() override;

  void ClearContainedWidgets(QWidget *parent) override;
  void AddGridWidget(QWidget *parent, int row, int column, QWidget *child, int rowSpan,
                     int columnSpan) override;
  void AddWidget(QWidget *parent, QWidget *child) override;
  void InsertWidget(QWidget *parent, int index, QWidget *child) override;

  // widget manipulation

  void SetWidgetText(QWidget *widget, const rdcstr &text) override;
  rdcstr GetWidgetText(QWidget *widget) override;

  void SetWidgetFont(QWidget *widget, const rdcstr &font, int fontSize, bool bold,
                     bool italic) override;

  void SetWidgetEnabled(QWidget *widget, bool enabled) override;
  bool IsWidgetEnabled(QWidget *widget) override;

  // specific widgets

  QWidget *CreateGroupBox(bool collapsible) override;

  QWidget *CreateButton(WidgetCallback pressed) override;

  QWidget *CreateLabel() override;

  QWidget *CreateOutputRenderingWidget() override;

  WindowingData GetWidgetWindowingData(QWidget *widget) override;
  void SetWidgetReplayOutput(QWidget *widget, IReplayOutput *output) override;
  void SetWidgetBackgroundColor(QWidget *widget, float red, float green, float blue) override;

  QWidget *CreateCheckbox(WidgetCallback changed) override;
  QWidget *CreateRadiobox(WidgetCallback changed) override;

  void SetWidgetChecked(QWidget *checkableWidget, bool checked) override;
  bool IsWidgetChecked(QWidget *checkableWidget) override;

  QWidget *CreateSpinbox(int decimalPlaces, double step) override;

  void SetSpinboxBounds(QWidget *spinbox, double minVal, double maxVal) override;
  void SetSpinboxValue(QWidget *spinbox, double value) override;
  double GetSpinboxValue(QWidget *spinbox) override;

  QWidget *CreateTextBox(bool singleLine, WidgetCallback changed) override;

  QWidget *CreateComboBox(bool editable, WidgetCallback changed) override;

  void SetComboOptions(QWidget *combo, const rdcarray<rdcstr> &options) override;

private:
  ICaptureContext &m_Ctx;

  QDialog *m_CurrentDialog = NULL;
  rdcarray<rdcpair<QWidget *, QMetaObject::Connection>> m_Connections;

  void AddWidgetCallback(QWidget *widget, QMetaObject::Connection connection);
};
