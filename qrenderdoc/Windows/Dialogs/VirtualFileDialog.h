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
#include <QItemSelection>

namespace Ui
{
class VirtualFileDialog;
}

struct ICaptureContext;
class RemoteFileModel;
class RemoteFileProxy;

class VirtualFileDialog : public QDialog
{
  Q_OBJECT

public:
  explicit VirtualFileDialog(ICaptureContext &ctx, QString initialDirectory, QWidget *parent = 0);
  ~VirtualFileDialog();

  QString chosenPath() { return m_ChosenPath; }
  void setDirBrowse();

private slots:
  // automatic slots
  void on_dirList_clicked(const QModelIndex &index);
  void on_fileList_doubleClicked(const QModelIndex &index);
  void on_fileList_clicked(const QModelIndex &index);
  void on_fileList_keyPress(QKeyEvent *e);
  void on_showHidden_toggled(bool checked);
  void on_filename_keyPress(QKeyEvent *e);
  void on_filter_currentIndexChanged(int index);
  void on_location_keyPress(QKeyEvent *e);
  void on_buttonBox_accepted();
  void on_back_clicked();
  void on_forward_clicked();
  void on_upFolder_clicked();

  // manual slots
  void dirList_selectionChanged(const QItemSelection &selected, const QItemSelection &deselected);
  void fileList_selectionChanged(const QItemSelection &selected, const QItemSelection &deselected);

private:
  Ui::VirtualFileDialog *ui;

  QString m_ChosenPath;

  RemoteFileModel *m_Model;
  RemoteFileProxy *m_DirProxy;
  RemoteFileProxy *m_FileProxy;

  QList<QModelIndex> m_History;
  int m_HistoryIndex = 0;

  void keyPressEvent(QKeyEvent *e) override;
  void accept() override;

  QModelIndex currentDir();
  void changeCurrentDir(const QModelIndex &index, bool recordHistory = true);

  void fileNotFound(const QString &path);
  void accessDenied(const QString &path);
};
