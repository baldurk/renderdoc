/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2021 Baldur Karlsson
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
#include <QStringList>
#include <QTextEdit>

class QCompleter;
class QStringListModel;

class RDTextEdit : public QTextEdit
{
private:
  Q_OBJECT

  bool m_singleLine = false;
  QCompleter *m_Completer = NULL;
  QStringListModel *m_CompletionModel = NULL;
  QString m_WordCharacters;

public:
  explicit RDTextEdit(QWidget *parent = 0);
  ~RDTextEdit();

  void setSingleLine();
  void setHoverTrack();
  void enableCompletion();

  QCompleter *completer() { return m_Completer; }
  void setCompletionWordCharacters(QString chars);
  void setCompletionStrings(QStringList list);
  bool completionInProgress();
  void triggerCompletion();

signals:
  void enter();
  void leave();
  void hoverEnter();
  void hoverLeave();
  void mouseMoved(QMouseEvent *event);
  void keyPress(QKeyEvent *e);
  void completionBegin(QString prefix);
  void completionEnd();

public slots:

protected:
  void focusInEvent(QFocusEvent *e);
  void focusOutEvent(QFocusEvent *e);
  void keyPressEvent(QKeyEvent *e);
  void mouseMoveEvent(QMouseEvent *event);
  bool event(QEvent *e);
  bool eventFilter(QObject *watched, QEvent *event);
};
