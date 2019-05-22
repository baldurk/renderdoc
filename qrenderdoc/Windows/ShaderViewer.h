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
#include "Code/Interface/QRDInterface.h"

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
  ByString,
};

class ShaderViewer : public QFrame, public IShaderViewer, public ICaptureViewer
{
  Q_OBJECT

public:
  static IShaderViewer *EditShader(ICaptureContext &ctx, bool customShader, ShaderStage stage,
                                   const QString &entryPoint, const rdcstrpairs &files,
                                   ShaderEncoding shaderEncoding, ShaderCompileFlags flags,
                                   IShaderViewer::SaveCallback saveCallback,
                                   IShaderViewer::CloseCallback closeCallback, QWidget *parent)
  {
    ShaderViewer *ret = new ShaderViewer(ctx, parent);
    ret->m_SaveCallback = saveCallback;
    ret->m_CloseCallback = closeCallback;
    ret->editShader(customShader, stage, entryPoint, files, shaderEncoding, flags);
    return ret;
  }

  static IShaderViewer *DebugShader(ICaptureContext &ctx, const ShaderBindpointMapping *bind,
                                    const ShaderReflection *shader, ResourceId pipeline,
                                    ShaderDebugTrace *trace, const QString &debugContext,
                                    QWidget *parent)
  {
    ShaderViewer *ret = new ShaderViewer(ctx, parent);
    ret->debugShader(bind, shader, pipeline, trace, debugContext);
    return ret;
  }

  static IShaderViewer *ViewShader(ICaptureContext &ctx, const ShaderReflection *shader,
                                   ResourceId pipeline, QWidget *parent)
  {
    return DebugShader(ctx, NULL, shader, pipeline, NULL, QString(), parent);
  }

  ~ShaderViewer();

  // IShaderViewer
  virtual QWidget *Widget() override { return this; }
  virtual int CurrentStep() override;
  virtual void SetCurrentStep(int step) override;

  virtual void ToggleBreakpoint(int instruction = -1) override;

  virtual void ShowErrors(const rdcstr &errors) override;

  virtual void AddWatch(const rdcstr &variable) override;

  // ICaptureViewer
  void OnCaptureLoaded() override;
  void OnCaptureClosed() override;
  void OnSelectedEventChanged(uint32_t eventId) override {}
  void OnEventChanged(uint32_t eventId) override;

private slots:
  // automatic slots
  void on_findReplace_clicked();
  void on_refresh_clicked();
  void on_intView_clicked();
  void on_floatView_clicked();
  void on_debugToggle_clicked();

  void on_watch_itemChanged(QTableWidgetItem *item);

  // manual slots
  void readonly_keyPressed(QKeyEvent *event);
  void editable_keyPressed(QKeyEvent *event);
  void debug_contextMenu(const QPoint &pos);
  void variables_contextMenu(const QPoint &pos);
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
  void editShader(bool customShader, ShaderStage stage, const QString &entryPoint,
                  const rdcstrpairs &files, ShaderEncoding shaderEncoding, ShaderCompileFlags flags);
  void debugShader(const ShaderBindpointMapping *bind, const ShaderReflection *shader,
                   ResourceId pipeline, ShaderDebugTrace *trace, const QString &debugContext);
  bool eventFilter(QObject *watched, QEvent *event) override;

  void PopulateCompileTools();
  void PopulateCompileToolParameters();
  bool ProcessIncludeDirectives(QString &source, const rdcstrpairs &files);

  const rdcarray<ShaderVariable> *GetVariableList(VariableCategory varCat, int arrayIdx);
  void getRegisterFromWord(const QString &text, VariableCategory &varCat, int &varIdx, int &arrayIdx);

  void updateWindowTitle();
  void gotoSourceDebugging();
  void gotoDisassemblyDebugging();

  void insertSnippet(const QString &text);

  void showVariableTooltip(VariableCategory varCat, int varIdx, int arrayIdx);
  void showVariableTooltip(QString name);
  void updateVariableTooltip();
  void hideVariableTooltip();

  bool isSourceDebugging();

  ShaderEncoding currentEncoding();

  VariableCategory m_TooltipVarCat = VariableCategory::Temporaries;
  QString m_TooltipName;
  int m_TooltipVarIdx = -1;
  int m_TooltipArrayIdx = -1;
  QPoint m_TooltipPos;

  Ui::ShaderViewer *ui;
  ICaptureContext &m_Ctx;
  const ShaderBindpointMapping *m_Mapping = NULL;
  const ShaderReflection *m_ShaderDetails = NULL;
  bool m_CustomShader = false;
  ShaderCompileFlags m_Flags;
  QList<ShaderEncoding> m_Encodings;
  ShaderStage m_Stage;
  QString m_DebugContext;
  ResourceId m_Pipeline;
  ScintillaEdit *m_DisassemblyView = NULL;
  QFrame *m_DisassemblyToolbar = NULL;
  QWidget *m_DisassemblyFrame = NULL;
  QComboBox *m_DisassemblyType = NULL;
  ScintillaEdit *m_Errors = NULL;
  ScintillaEdit *m_FindResults = NULL;
  QList<ScintillaEdit *> m_Scintillas;

  // a map per file, from line number to instruction index
  QVector<QMap<int32_t, size_t>> m_Line2Inst;

  ScintillaEdit *m_CurInstructionScintilla = NULL;
  QList<ScintillaEdit *> m_FileScintillas;

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

  static const int CURRENT_INDICATOR = 20;
  static const int FINISHED_INDICATOR = 21;

  static const int INDICATOR_FINDRESULT = 0;
  static const int INDICATOR_REGHIGHLIGHT = 1;

  QString targetName(const ShaderProcessingTool &disasm);

  void addFileList();

  ScintillaEdit *MakeEditor(const QString &name, const QString &text, int lang);
  ScintillaEdit *AddFileScintilla(const QString &name, const QString &text, ShaderEncoding encoding);

  ScintillaEdit *currentScintilla();
  ScintillaEdit *nextScintilla(ScintillaEdit *cur);

  int snippetPos();
  QString vulkanUBO();

  int instructionForLine(sptr_t line);

  void updateDebugging();

  const ShaderVariable *GetRegisterVariable(const RegisterRange &r);

  void ensureLineScrolled(ScintillaEdit *s, int i);

  void find(bool down);

  void runTo(int runToInstruction, bool forward, ShaderEvents condition = ShaderEvents::NoEvent);

  QString stringRep(const ShaderVariable &var, bool useType);
  RDTreeWidgetItem *makeResourceRegister(const Bindpoint &bind, uint32_t idx,
                                         const BoundResource &ro, const ShaderResource &resources);
  void combineStructures(RDTreeWidgetItem *root);
  RDTreeWidgetItem *findLocal(RDTreeWidgetItem *root, QString name);
};
