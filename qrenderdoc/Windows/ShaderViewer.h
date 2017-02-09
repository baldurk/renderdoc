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
#include <QKeyEvent>
#include "Code/CaptureContext.h"

namespace Ui
{
class ShaderViewer;
}

struct ShaderDebugTrace;
struct ShaderReflection;
class ScintillaEdit;

class ShaderViewer : public QFrame, public ILogViewerForm
{
  Q_OBJECT

public:
  explicit ShaderViewer(CaptureContext &ctx, ShaderReflection *shader, ShaderStageType stage,
                        ShaderDebugTrace *trace, const QString &debugContext, QWidget *parent = 0);
  ~ShaderViewer();

  void OnLogfileLoaded();
  void OnLogfileClosed();
  void OnSelectedEventChanged(uint32_t eventID) {}
  void OnEventChanged(uint32_t eventID);

private slots:
  // manual slots
  void disassembly_keyPressed(QKeyEvent *event);
  void readonly_keyPressed(QKeyEvent *event);

private:
  Ui::ShaderViewer *ui;
  CaptureContext &m_Ctx;
  ShaderDebugTrace *m_Trace;
  ShaderReflection *m_ShaderDetails;
  ScintillaEdit *m_DisassemblyView;
  QList<ScintillaEdit *> m_Scintillas;

  static const int CURRENT_MARKER = 0;
  static const int BREAKPOINT_MARKER = 1;
  static const int FINISHED_MARKER = 2;

  ScintillaEdit *MakeEditor(const QString &name, const QString &text, bool src);
};
