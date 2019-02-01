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

#include <QFrame>

namespace Ui
{
class FindReplace;
}

class QComboBox;

class FindReplace : public QFrame
{
  Q_OBJECT

public:
  explicit FindReplace(QWidget *parent = 0);
  ~FindReplace();

  enum SearchContext
  {
    File,
    AllFiles,
  };

  enum SearchDirection
  {
    Up,
    Down,
  };

  bool replaceMode();

  SearchContext context();
  SearchDirection direction();
  bool matchCase();
  bool matchWord();
  bool regexp();

  QString findText();
  QString replaceText();

public slots:
  void allowUserModeChange(bool allow);
  void setReplaceMode(bool replacing);
  void setDirection(SearchDirection dir);
  void takeFocus();

signals:
  void performFind();
  void performFindAll();
  void performReplace();
  void performReplaceAll();

private slots:
  // automatic slots
  void on_find_clicked();
  void on_findAll_clicked();
  void on_replace_clicked();
  void on_replaceAll_clicked();
  void on_findMode_clicked();
  void on_replaceMode_clicked();

private:
  void keyPressEvent(QKeyEvent *event) override;

  Ui::FindReplace *ui;

  void addHistory(QComboBox *combo);
};
