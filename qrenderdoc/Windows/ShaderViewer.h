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

// from Scintilla
typedef intptr_t sptr_t;

class ShaderViewer : public QFrame, public ILogViewerForm
{
  Q_OBJECT

public:
  typedef std::function<void(CaptureContext *ctx, ShaderViewer *, const QStringMap &)> SaveMethod;
  typedef std::function<void(CaptureContext *ctx)> CloseMethod;

  static ShaderViewer *editShader(CaptureContext &ctx, bool customShader, const QString &entryPoint,
                                  const QStringMap &files, SaveMethod saveCallback,
                                  CloseMethod closeCallback, QWidget *parent)
  {
    ShaderViewer *ret = new ShaderViewer(ctx, parent);
    ret->m_SaveCallback = saveCallback;
    ret->m_CloseCallback = closeCallback;
    ret->editShader(customShader, entryPoint, files);
    return ret;
  }

  static ShaderViewer *debugShader(CaptureContext &ctx, const ShaderBindpointMapping *bind,
                                   const ShaderReflection *shader, ShaderStageType stage,
                                   ShaderDebugTrace *trace, const QString &debugContext,
                                   QWidget *parent)
  {
    ShaderViewer *ret = new ShaderViewer(ctx, parent);
    ret->debugShader(bind, shader, stage, trace, debugContext);
    return ret;
  }

  static ShaderViewer *viewShader(CaptureContext &ctx, const ShaderBindpointMapping *bind,
                                  const ShaderReflection *shader, ShaderStageType stage,
                                  QWidget *parent)
  {
    return ShaderViewer::debugShader(ctx, bind, shader, stage, NULL, "", parent);
  }

  ~ShaderViewer();

  void OnLogfileLoaded();
  void OnLogfileClosed();
  void OnSelectedEventChanged(uint32_t eventID) {}
  void OnEventChanged(uint32_t eventID);

  int currentStep();
  void setCurrentStep(int step);

  void toggleBreakpoint(int instruction = -1);

  void showErrors(const QString &errors);

private slots:
  // automatic slots
  void on_findReplace_clicked();
  void on_save_clicked();
  void on_intView_clicked();
  void on_floatView_clicked();

  // manual slots
  void readonly_keyPressed(QKeyEvent *event);
  void editable_keyPressed(QKeyEvent *event);
  void disassembly_buttonReleased(QMouseEvent *event);

public slots:
  bool stepBack();
  bool stepNext();
  void runToCursor();
  void runToSample();
  void runToNanOrInf();
  void runBack();
  void run();

private:
  explicit ShaderViewer(CaptureContext &ctx, QWidget *parent = 0);
  void editShader(bool customShader, const QString &entryPoint, const QStringMap &files);
  void debugShader(const ShaderBindpointMapping *bind, const ShaderReflection *shader,
                   ShaderStageType stage, ShaderDebugTrace *trace, const QString &debugContext);

  Ui::ShaderViewer *ui;
  CaptureContext &m_Ctx;
  const ShaderBindpointMapping *m_Mapping = NULL;
  const ShaderReflection *m_ShaderDetails = NULL;
  ShaderStageType m_Stage;
  ScintillaEdit *m_DisassemblyView = NULL;
  ScintillaEdit *m_Errors = NULL;
  QList<ScintillaEdit *> m_Scintillas;

  SaveMethod m_SaveCallback;
  CloseMethod m_CloseCallback;

  ShaderDebugTrace *m_Trace = NULL;
  int m_CurrentStep;
  QList<int> m_Breakpoints;

  static const int CURRENT_MARKER = 0;
  static const int BREAKPOINT_MARKER = 2;
  static const int FINISHED_MARKER = 4;

  void addFileList();

  ScintillaEdit *MakeEditor(const QString &name, const QString &text, bool src);
  ScintillaEdit *AddFileScintilla(const QString &name, const QString &text);

  int instructionForLine(sptr_t line);

  void updateDebugging();
  void runTo(int runToInstruction, bool forward, ShaderDebugStateFlags condition = eShaderDbg_None);

  QString stringRep(const ShaderVariable &var, bool useType);
  QTreeWidgetItem *makeResourceRegister(const BindpointMap &bind, uint32_t idx,
                                        const BoundResource &ro, const ShaderResource &resources);
};
