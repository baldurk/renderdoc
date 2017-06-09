/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2017 Baldur Karlsson
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

class PythonContext;
class ScintillaEdit;
class QTextEdit;

namespace Ui
{
class PythonShell;
}

struct CaptureContextInvoker;

class PythonShell : public QFrame, public IPythonShell
{
  Q_OBJECT

public:
  explicit PythonShell(ICaptureContext &ctx, QWidget *parent = 0);

  ~PythonShell();

  // IPythonShell
  QWidget *Widget() override { return this; }
private slots:
  // automatic slots
  void on_execute_clicked();
  void on_clear_clicked();
  void on_newScript_clicked();
  void on_openScript_clicked();
  void on_saveScript_clicked();
  void on_runScript_clicked();
  void on_abortRun_clicked();

  // manual slots
  void interactive_keypress(QKeyEvent *e);
  void traceLine(const QString &file, int line);
  void exception(const QString &type, const QString &value, QList<QString> frames);
  void textOutput(bool isStdError, const QString &output);

private:
  Ui::PythonShell *ui;
  ICaptureContext &m_Ctx;
  CaptureContextInvoker *m_ThreadCtx = NULL;

  ScintillaEdit *scriptEditor;

  static const int CURRENT_MARKER = 0;

  PythonContext *interactiveContext, *scriptContext;

  QList<QString> history;
  int historyidx = -1;

  PythonContext *newContext();

  QString scriptHeader();
  void appendText(QTextEdit *output, const QString &text);
  void enableButtons(bool enable);
};
