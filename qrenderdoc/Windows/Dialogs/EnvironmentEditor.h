/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2017-2019 Baldur Karlsson
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

#include <QDialog>
#include "Code/ReplayManager.h"

namespace Ui
{
class EnvironmentEditor;
}

class RDTreeWidgetItem;
class QCompleter;

class EnvironmentEditor : public QDialog
{
  Q_OBJECT

public:
  explicit EnvironmentEditor(QWidget *parent = 0);
  ~EnvironmentEditor();

  void addModification(EnvironmentModification mod, bool silent);
  QList<EnvironmentModification> modifications();

public slots:
  // automatic slots
  void on_name_textChanged(const QString &text);
  void on_addUpdate_clicked();
  void on_deleteButton_clicked();
  void on_variables_keyPress(QKeyEvent *event);
  void on_variables_currentItemChanged(RDTreeWidgetItem *current, RDTreeWidgetItem *previous);
  void on_buttonBox_accepted();

private:
  Ui::EnvironmentEditor *ui;
  QCompleter *m_Completer;

  int existingIndex();
};
