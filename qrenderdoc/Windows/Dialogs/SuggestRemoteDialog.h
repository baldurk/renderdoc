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

namespace Ui
{
class SuggestRemoteDialog;
}

class QMenu;

class SuggestRemoteDialog : public QDialog
{
  Q_OBJECT

public:
  explicit SuggestRemoteDialog(const QString &driver, const QString &machineIdent,
                               QWidget *parent = 0);
  ~SuggestRemoteDialog();

  enum SuggestRemoteResult
  {
    Cancel,
    Local,
    Remote
  };

  QMenu *remotesMenu() { return m_Remotes; }
  void remotesAdded();

  bool alwaysReplayLocally();
  SuggestRemoteResult choice() { return m_Choice; }
private slots:
  // automatic slots
  void on_alwaysLocal_toggled(bool checked);
  void on_local_clicked();
  void on_cancel_clicked();

  // manual slots
  void remoteItem_clicked(QAction *action);

private:
  Ui::SuggestRemoteDialog *ui;
  QMenu *m_Remotes;

  QString m_WarningStart;
  SuggestRemoteResult m_Choice = Cancel;
};
