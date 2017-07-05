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

namespace Ui
{
class ShaderViewer;
}

class RDTreeWidgetItem;
struct ShaderDebugTrace;
struct ShaderReflection;
class ScintillaEdit;
class FindReplace;
class QTableWidgetItem;
class QKeyEvent;
class QMouseEvent;
class QComboBox;

// from Scintilla
typedef intptr_t sptr_t;

enum class VariableCategory
{
  Unknown,
  Inputs,
  Constants,
  IndexTemporaries,
  Temporaries,
  Outputs,
};

class ShaderViewer : public QFrame, public IShaderViewer, public ILogViewer
{
  Q_OBJECT

public:
  static IShaderViewer *EditShader(ICaptureContext &ctx, bool customShader, const QString &entryPoint,
                                   const QStringMap &files, IShaderViewer::SaveCallback saveCallback,
                                   IShaderViewer::CloseCallback closeCallback, QWidget *parent)
  {
    ShaderViewer *ret = new ShaderViewer(ctx, parent);
    ret->m_SaveCallback = saveCallback;
    ret->m_CloseCallback = closeCallback;
    ret->editShader(customShader, entryPoint, files);
    return ret;
  }

  static IShaderViewer *DebugShader(ICaptureContext &ctx, const ShaderBindpointMapping *bind,
                                    const ShaderReflection *shader, ShaderStage stage,
                                    ShaderDebugTrace *trace, const QString &debugContext,
                                    QWidget *parent)
  {
    ShaderViewer *ret = new ShaderViewer(ctx, parent);
    ret->debugShader(bind, shader, stage, trace, debugContext);
    return ret;
  }

  static IShaderViewer *ViewShader(ICaptureContext &ctx, const ShaderBindpointMapping *bind,
                                   const ShaderReflection *shader, ShaderStage stage, QWidget *parent)
  {
    return DebugShader(ctx, bind, shader, stage, NULL, QString(), parent);
  }

  ~ShaderViewer();

  // IShaderViewer
  virtual QWidget *Widget() override { return this; }
  virtual int CurrentStep() override;
  virtual void SetCurrentStep(int step) override;

  virtual void ToggleBreakpoint(int instruction = -1) override;

  virtual void ShowErrors(const QString &errors) override;

  // ILogViewerForm
  void OnLogfileLoaded() override;
  void OnLogfileClosed() override;
  void OnSelectedEventChanged(uint32_t eventID) override {}
  void OnEventChanged(uint32_t eventID) override;

private slots:
  // automatic slots
  void on_findReplace_clicked();
  void on_save_clicked();
  void on_intView_clicked();
  void on_floatView_clicked();

  void on_watch_itemChanged(QTableWidgetItem *item);

  // manual slots
  void readonly_keyPressed(QKeyEvent *event);
  void editable_keyPressed(QKeyEvent *event);
  void disassembly_contextMenu(const QPoint &pos);
  void disassembly_buttonReleased(QMouseEvent *event);
  void disassemble_typeChanged(int index);
  void watch_keyPress(QKeyEvent *event);
  void performFind();
  void performFindAll();
  void performReplace();
  void performReplaceAll();

  void snippet_textureDimensions();
  void snippet_selectedMip();
  void snippet_selectedSlice();
  void snippet_selectedSample();
  void snippet_selectedType();
  void snippet_samplers();
  void snippet_resources();

  void disasm_tooltipShow(int x, int y);
  void disasm_tooltipHide(int x, int y);

public slots:
  bool stepBack();
  bool stepNext();
  void runToCursor();
  void runToSample();
  void runToNanOrInf();
  void runBack();
  void run();

private:
  explicit ShaderViewer(ICaptureContext &ctx, QWidget *parent = 0);
  void editShader(bool customShader, const QString &entryPoint, const QStringMap &files);
  void debugShader(const ShaderBindpointMapping *bind, const ShaderReflection *shader,
                   ShaderStage stage, ShaderDebugTrace *trace, const QString &debugContext);

  bool eventFilter(QObject *watched, QEvent *event) override;

  const rdctype::array<ShaderVariable> *GetVariableList(VariableCategory varCat, int arrayIdx);
  void getRegisterFromWord(const QString &text, VariableCategory &varCat, int &varIdx, int &arrayIdx);

  void showVariableTooltip(VariableCategory varCat, int varIdx, int arrayIdx);
  void updateVariableTooltip();
  void hideVariableTooltip();

  VariableCategory m_TooltipVarCat = VariableCategory::Temporaries;
  int m_TooltipVarIdx = -1;
  int m_TooltipArrayIdx = -1;
  QPoint m_TooltipPos;

  Ui::ShaderViewer *ui;
  ICaptureContext &m_Ctx;
  const ShaderBindpointMapping *m_Mapping = NULL;
  const ShaderReflection *m_ShaderDetails = NULL;
  ShaderStage m_Stage;
  ScintillaEdit *m_DisassemblyView = NULL;
  QWidget *m_DisassemblyFrame = NULL;
  QComboBox *m_DisassemblyType = NULL;
  ScintillaEdit *m_Errors = NULL;
  ScintillaEdit *m_FindResults = NULL;
  QList<ScintillaEdit *> m_Scintillas;

  FindReplace *m_FindReplace;

  struct FindState
  {
    // hash identifies when the search has changed
    QString hash;

    // the range identified when the search first occurred (for incremental find/replace)
    sptr_t start = 0;
    sptr_t end = 0;

    // the current offset where to search from next time, relative to above range
    sptr_t offset = 0;

    // the last result
    QPair<int, int> prevResult;
  } m_FindState;

  SaveCallback m_SaveCallback;
  CloseCallback m_CloseCallback;

  ShaderDebugTrace *m_Trace = NULL;
  int m_CurrentStep;
  QList<int> m_Breakpoints;

  static const int CURRENT_MARKER = 0;
  static const int BREAKPOINT_MARKER = 2;
  static const int FINISHED_MARKER = 4;

  static const int INDICATOR_FINDRESULT = 0;
  static const int INDICATOR_REGHIGHLIGHT = 1;

  void addFileList();

  ScintillaEdit *MakeEditor(const QString &name, const QString &text, int lang);
  ScintillaEdit *AddFileScintilla(const QString &name, const QString &text);

  ScintillaEdit *currentScintilla();
  ScintillaEdit *nextScintilla(ScintillaEdit *cur);

  int snippetPos();
  void insertVulkanUBO();

  int instructionForLine(sptr_t line);

  void updateDebugging();

  void ensureLineScrolled(ScintillaEdit *s, int i);

  void find(bool down);

  void runTo(int runToInstruction, bool forward, ShaderEvents condition = ShaderEvents::NoEvent);

  QString stringRep(const ShaderVariable &var, bool useType);
  RDTreeWidgetItem *makeResourceRegister(const BindpointMap &bind, uint32_t idx,
                                         const BoundResource &ro, const ShaderResource &resources);
};
