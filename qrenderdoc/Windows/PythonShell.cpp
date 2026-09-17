/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2017-2026 Baldur Karlsson
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

#include "PythonShell.h"
#include <QAbstractItemView>
#include <QCompleter>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QFileSystemWatcher>
#include <QFontDatabase>
#include <QKeyEvent>
#include <QListWidget>
#include <QMenu>
#include <QScrollBar>
#include <QStringListModel>
#include <QTimer>
#include "Code/QRDUtils.h"
#include "Code/Resources.h"
#include "Code/ScintillaSyntax.h"
#include "Code/pyrenderdoc/PythonContext.h"
#include "Widgets/Extended/RDLabel.h"
#include "Widgets/Extended/RDToolTip.h"
#include "Widgets/FindReplace.h"
#include "scintilla/include/SciLexer.h"
#include "toolwindowmanager/ToolWindowManagerArea.h"
#include "ui_PythonShell.h"

enum
{
  AllOutputFilter,
  ScriptOutputFilter,
  FirstExtensionOutputFilter,
};

EditorWrapper::EditorWrapper(PythonShell *parent) : QFrame(parent), m_PyShell(parent)
{
  m_Scintilla = new ScintillaEdit(this);

  m_Warning = new RDLabel(this);

  m_Warning->setForegroundRole(QPalette::ToolTipText);
  m_Warning->setBackgroundRole(QPalette::ToolTipBase);
  m_Warning->setAutoFillBackground(true);
  m_Warning->setMargin(6);
  m_Warning->setFrameStyle(QFrame::Box);
  m_Warning->setAlignment(Qt::AlignLeft);
  m_Warning->setIndent(1);

  m_Warning->hide();

  setLayout(new QVBoxLayout(this));

  layout()->addWidget(m_Warning);
  layout()->addWidget(m_Scintilla);
  layout()->setSpacing(0);
  layout()->setMargin(0);
  layout()->setContentsMargins(0, 0, 0, 0);

  setTitle(QString());
}

EditorWrapper::~EditorWrapper()
{
  m_PyShell->removeEditor(this);
}

void EditorWrapper::setFilename(QString filename)
{
  m_Filename = filename;
  if(!m_Filename.isEmpty())
    m_Title.clear();
  updateTitle();
}

void EditorWrapper::setTitle(QString title)
{
  if(!m_Filename.isEmpty())
    return;
  if(title.isEmpty())
    m_Title = tr("Untitled Script");
  else
    m_Title = title;
  updateTitle();
}

void EditorWrapper::setWarning(QString text)
{
  m_Warning->setText(text);
  m_Warning->setVisible(!text.isEmpty());
}

void EditorWrapper::markModified(bool modified)
{
  m_Modified = modified;
  updateTitle();
}

void EditorWrapper::updateTitle()
{
  QString title;
  if(m_Filename.isEmpty())
  {
    if(isModified())
      setWindowTitle(m_Title + lit(" *"));
    else
      setWindowTitle(m_Title);
  }
  else
  {
    if(isModified())
      setWindowTitle(QFileInfo(m_Filename).fileName() + lit(" *"));
    else
      setWindowTitle(QFileInfo(m_Filename).fileName());

    ToolWindowManager *manager = ToolWindowManager::managerOf(this);

    if(manager)
    {
      ToolWindowManagerArea *editorTabs = manager->areaOf(this);

      if(editorTabs)
      {
        int idx = editorTabs->indexOf(this);
        if(idx >= 0)
          editorTabs->setTabToolTip(idx, m_Filename);
      }
    }
  }
}

bool EditorWrapper::checkAllowClose()
{
  if(isModified())
  {
    QString filename = m_Filename;
    bool untitled = false;
    if(filename.isEmpty())
    {
      untitled = true;
      filename = m_Title;
    }
    QMessageBox::StandardButton res = RDDialog::question(this, tr("Python script is modified"),
                                                         tr("You have unsaved changes to '%1'.\n"
                                                            "Do you want to save them?")
                                                             .arg(QFileInfo(filename).fileName()),
                                                         RDDialog::YesNoCancel);

    if(res == QMessageBox::Cancel)
      return false;

    if(res == QMessageBox::No)
      return true;

    if(untitled)
      return m_PyShell->saveEditorAs(this);
    else
      return m_PyShell->saveEditor(this, filename);
  }

  return true;
}

PythonShell::PythonShell(ICaptureContext &ctx, QWidget *parent)
    : QFrame(parent), ui(new Ui::PythonShell), m_Ctx(ctx)
{
  ui->setupUi(this);

  QObject::connect(ui->lineInput, &RDLineEdit::keyPress, this, &PythonShell::interactive_keypress);
  QObject::connect(ui->helpSearch, &RDLineEdit::keyPress, this, &PythonShell::interactive_keypress);

  QObject::connect(ui->lineInput, &RDLineEdit::leave, [this]() { hideFunccompleteTooltip(); });
  QObject::connect(ui->helpSearch, &RDLineEdit::leave, [this]() { hideFunccompleteTooltip(); });

  // we create this up front so its state stays persistent as much as possible.
  m_FindReplace = new FindReplace(m_Scintillas, this);
  m_FindReplace->allowFindAll(false);
  m_FindReplace->setDockManager(ui->docking);

  {
    m_FindResults = new ScintillaEdit(this);

    m_FindResults->styleSetFont(STYLE_DEFAULT, Formatter::FixedFont().family().toUtf8().data());
    m_FindResults->styleSetSize(STYLE_DEFAULT, Formatter::FixedFont().pointSize());
    m_FindResults->styleSetFont(STYLE_ERROR, Formatter::FixedFont().family().toUtf8().data());
    m_FindResults->styleSetSize(STYLE_ERROR, Formatter::FixedFont().pointSize());
    m_FindResults->styleSetBack(STYLE_ERROR, IsDarkTheme() ? SCINTILLA_COLOUR(175, 70, 70)
                                                           : SCINTILLA_COLOUR(255, 150, 150));

    ConfigureSyntax(m_FindResults, SCLEX_NULL);
    m_FindResults->usePopUp(SC_POPUP_NEVER);
    m_FindResults->setWrapMode(SC_WRAP_WORD);
    m_FindResults->setReadOnly(true);
    m_FindResults->setWindowTitle(lit("Find Results"));
  }

  m_FindReplace->setFindIndicator(2);
  m_FindReplace->setFindAllResultsDisplay(m_FindResults);

  ui->lineInput->setFont(Formatter::FixedFont());
  ui->helpSearch->setFont(Formatter::FixedFont());
  ui->interactiveOutput->setFont(Formatter::FixedFont());
  ui->scriptOutput->setFont(Formatter::FixedFont());
  ui->helpText->setFont(Formatter::FixedFont());

  ui->outputGroup->setWindowTitle(tr("Output"));
  ui->helpGroup->setWindowTitle(tr("Help"));
  ui->replGroup->setWindowTitle(tr("Interactive REPL"));

  ui->lineInput->setAcceptTabCharacters(true);
  ui->helpSearch->setAcceptTabCharacters(true);

  // don't repeatedly re-parse for errors. Have a reasonable timeout
  m_SyntaxCheckTimer = new QTimer(this);
  m_SyntaxCheckTimer->setSingleShot(true);
  m_SyntaxCheckTimer->setInterval(1200);

  m_CompletionTipTimer = new QTimer(this);
  m_CompletionTipTimer->setSingleShot(false);
  // this timer will only be active while auto completing so we can be aggressive with its timer
  m_CompletionTipTimer->setInterval(10);
  QObject::connect(m_CompletionTipTimer, &QTimer::timeout, [this]() {
    EditorWrapper *editor = curEditor();

    if(!editor || !editor->scintilla()->autoCActive())
    {
      m_CompletionTipTimer->stop();
      m_CompletionTipList.clear();
      m_CurrentCompletionTip = -1;

      updateCompletionTip();
    }
    else
    {
      if(editor->scintilla()->autoCCurrent() != m_CurrentCompletionTip)
      {
        m_CurrentCompletionTip = editor->scintilla()->autoCCurrent();

        updateCompletionTip();
      }
    }
  });

  // only update the current line intermittently. We don't need to update every single time and if
  // there is a large number of traces this will rate limit it.
  m_CurLineTimer = new QTimer(this);
  m_CurLineTimer->setSingleShot(false);
  m_CurLineTimer->setInterval(10);

  QObject::connect(m_CurLineTimer, &QTimer::timeout, [this]() {
    if(m_CurLineDirty)
    {
      if(runningScriptEditor)
      {
        runningScriptEditor->markerDeleteAll(CURRENT_MARKER);
        runningScriptEditor->markerDeleteAll(CURRENT_MARKER + 1);

        runningScriptEditor->markerAdd(m_CurLine > 0 ? m_CurLine - 1 : 0, CURRENT_MARKER);
        runningScriptEditor->markerAdd(m_CurLine > 0 ? m_CurLine - 1 : 0, CURRENT_MARKER + 1);
      }

      m_CurLineDirty = false;
    }
  });

  completionContext = new PythonContext();

  QObject::connect(m_SyntaxCheckTimer, &QTimer::timeout, this, &PythonShell::doSyntaxCheck);

  QObject::connect(ui->interactiveOutput, &RDTextEdit::keyPress, [this](QKeyEvent *e) {
    // ignore keypresses that aren't typing, but for up/down redirect that to the line input to get history
    if((e->text().isEmpty() || !e->text()[0].isPrint()) && e->key() != Qt::Key_Up &&
       e->key() != Qt::Key_Down)
      return;
    ui->lineInput->setFocus(Qt::OtherFocusReason);
    QApplication::postEvent(ui->lineInput, new QKeyEvent(*e));
  });

  m_ToolTip = new RDToolTip(this);

  m_ToolTip->installEventFilter(this);

  m_ToolTip->setFont(Formatter::FixedFont());

  m_CompletionTip = new RDToolTip(this);
  m_CompletionTip->setFont(Formatter::FixedFont());

  m_InteractiveCompleter = new QCompleter(this);
  m_InteractiveCompleter->popup()->setFont(Formatter::FixedFont());
  m_InteractiveCompleter->popup()->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);
  m_InteractiveCompleter->popup()->setTextElideMode(Qt::ElideNone);
  m_InteractiveCompleter->setCompletionMode(QCompleter::UnfilteredPopupCompletion);
  m_InteractiveCompleter->setWrapAround(false);
  m_InteractiveCompletionModel = new QStringListModel(this);
  m_InteractiveCompleter->setModel(m_InteractiveCompletionModel);
  m_InteractiveCompleter->setCompletionRole(Qt::DisplayRole);

  m_InteractiveCompleter->popup()->installEventFilter(this);

  QObject::connect(m_InteractiveCompleter,
                   OverloadedSlot<const QModelIndex &>::of(&QCompleter::highlighted),
                   [this](const QModelIndex &idx) {
                     if(idx.isValid() && idx.row() < m_CompletionTipList.count())
                     {
                       if(m_CurrentCompletionTip != idx.row())
                       {
                         m_CurrentCompletionTip = idx.row();
                         updateCompletionTip();
                       }
                     }
                   });

  QObject::connect(m_InteractiveCompleter,
                   OverloadedSlot<const QModelIndex &>::of(&QCompleter::activated),
                   [this](const QModelIndex &idx) {
                     RDLineEdit *edit = qobject_cast<RDLineEdit *>(m_InteractiveCompleter->widget());
                     if(!edit)
                       return;
                     int i = idx.row();
                     if(i >= 0 && i < m_InteractiveCompletionModel->rowCount())
                     {
                       QString curText = edit->text();
                       curText.resize(curText.size() - m_InteractiveCompletionPrefix);
                       curText += m_InteractiveCompletionModel->stringList()[i];
                       edit->setText(curText);

                       edit->setCursorPosition(curText.size());
                     }
                   });

  // reset output to default
  on_clear_clicked();
  on_newScript_clicked();

  ui->saveScript->setEnabled(false);

  setupTabs();

  ui->docking->addToolWindow(m_FindReplace, ToolWindowManager::NoArea);
  ui->docking->setToolWindowProperties(m_FindReplace, ToolWindowManager::HideOnClose);

  ui->docking->addToolWindow(m_FindResults, ToolWindowManager::NoArea);
  ui->docking->setToolWindowProperties(
      m_FindResults, ToolWindowManager::HideOnClose | ToolWindowManager::DisallowFloatWindow);

  ui->docking->addToolWindow(
      ui->replGroup, ToolWindowManager::AreaReference(ToolWindowManager::BottomOf,
                                                      ui->docking->areaOf(m_Editors[0]), 0.3f));
  ui->docking->setToolWindowProperties(
      ui->replGroup, ToolWindowManager::HideCloseButton | ToolWindowManager::DisallowFloatWindow);

  ui->projectExplorer->setWindowTitle(tr("Project Explorer"));
  ui->projectExplorer->setColumns({tr("Name")});
  ui->projectExplorer->hideGridLines();

  m_Watcher = new QFileSystemWatcher({}, this);

  QObject::connect(m_Watcher, &QFileSystemWatcher::fileChanged, this, &PythonShell::openFileModified);
  QObject::connect(m_Watcher, &QFileSystemWatcher::directoryChanged, this,
                   &PythonShell::updateExtensionProjects);

  QObject::connect(PythonContext::GetExtensionContext(), &PythonContext::extensionsUpdated, [this]() {
    QList<QString> curModExts;

    for(const ExtensionMetadata &m : m_Ctx.Extensions().GetInstalledExtensions())
    {
      if(m.hasChanges)
      {
        curModExts.push_back(m.package);
      }
    }

    curModExts.sort();

    if(curModExts != m_ModifiedExtensions)
    {
      updateExtensionProjects();
      m_ModifiedExtensions = curModExts;
    }
  });

  QTimer *pyStatusTimer = new QTimer(this);
  QObject::connect(pyStatusTimer, &QTimer::timeout, [this]() {
    bool hasDebugger = PythonContext::IsDebuggerConnected();

    if(m_DebuggerAttached != hasDebugger)
    {
      updateButtonStates();
      updateNonDebugWarning();
    }

    m_DebuggerAttached = hasDebugger;
  });

  pyStatusTimer->setSingleShot(false);
  pyStatusTimer->setInterval(500);
  pyStatusTimer->start();

  ui->projectExplorer->setContextMenuPolicy(Qt::CustomContextMenu);
  QObject::connect(ui->projectExplorer, &RDTreeWidget::customContextMenuRequested, this,
                   &PythonShell::projectExplorer_contextMenu);

  ui->projectExplorer->beginUpdate();

  m_Examples = new RDTreeWidgetItem({lit("Examples")});
  m_Examples->setData(0, Qt::UserRole + 1, m_Examples->text(0));
  m_Examples->setSelectable(false);
  m_Examples->setBold(true);
  m_Examples->setIcon(0, Icons::help());

  const QPair<QString, QString> examples[] = {
      {tr("Tutorial: First Steps with Python"), lit(":/py/tutorial/first_steps.py")},
      {tr("Tutorial: UI extensions"), lit(":/py/tutorial/ui_extensions.py")},
      {tr("Show buffer with format"), lit(":/py/examples/show_buffer.py")},
      {tr("Show and save a texture"), lit(":/py/examples/show_texture.py")},
      {tr("Iterating over Actions"), lit(":/py/examples/iter_actions.py")},
      {tr("Pipeline State"), lit(":/py/examples/pipe_state.py")},
      {tr("Shader Reflection"), lit(":/py/examples/shader_refl.py")},
      {tr("Resource Usage"), lit(":/py/examples/resource_usage.py")},
      {tr("Memory bindings"), lit(":/py/examples/mem_binds.py")},
      {tr("Pixel History & Shader Debug"), lit(":/py/examples/history_debug.py")},
      {tr("Mesh Output"), lit(":/py/examples/mesh_output.py")},
      {tr("Advanced Buffers"), lit(":/py/examples/advanced_buffers.py")},
      {tr("Launching an application"), lit(":/py/examples/exe_launching.py")},
      {tr("Custom event filter"), lit(":/py/examples/event_filter.py")},
      {tr("Mini-Qt UI"), lit(":/py/examples/miniqt_ui.py")},
  };

  for(const QPair<QString, QString> &example : examples)
  {
    RDTreeWidgetItem *ex = new RDTreeWidgetItem({example.first});

    QFile file(example.second);

    file.open(QFile::ReadOnly);
    ex->setData(0, Qt::UserRole, QString::fromUtf8(file.readAll()));
    file.close();

    m_Examples->addChild(ex);
  }

  m_UIExtensions = new RDTreeWidgetItem({lit("UI Extensions")});
  m_UIExtensions->setData(0, Qt::UserRole + 1, m_UIExtensions->text(0));
  m_UIExtensions->setSelectable(false);
  m_UIExtensions->setBold(true);
  m_UIExtensions->setIcon(0, Icons::plugin());

  m_RecentFiles = new RDTreeWidgetItem({lit("Recent files")});
  m_RecentFiles->setData(0, Qt::UserRole + 1, m_UIExtensions->text(0));
  m_RecentFiles->setSelectable(false);
  m_RecentFiles->setBold(true);
  m_RecentFiles->setIcon(0, Icons::page_white_edit());

  ui->projectExplorer->addTopLevelItem(m_Examples);
  ui->projectExplorer->addTopLevelItem(m_UIExtensions);
  ui->projectExplorer->addTopLevelItem(m_RecentFiles);

  updateRecentFiles(false);
  updateExtensionProjects();

  // on first launch we could be creating the python shell before extensions are loaded while
  // initialising the UI. Refresh the extension projects after a short delay to pick up any
  // additional information on loading status.
  QTimer::singleShot(200, [this]() { updateExtensionProjects(); });

  ui->projectExplorer->endUpdate();

  ui->projectExplorer->expandItem(m_RecentFiles);

  ui->docking->addToolWindow(
      ui->projectExplorer, ToolWindowManager::AreaReference(
                               ToolWindowManager::LeftOf, ui->docking->areaOf(m_Editors[0]), 0.2f));
  ui->docking->setToolWindowProperties(
      ui->projectExplorer,
      ToolWindowManager::HideCloseButton | ToolWindowManager::DisallowFloatWindow);

  ui->docking->addToolWindow(ui->outputGroup,
                             ToolWindowManager::AreaReference(ToolWindowManager::AddTo,
                                                              ui->docking->areaOf(ui->replGroup)));
  ui->docking->setToolWindowProperties(
      ui->outputGroup, ToolWindowManager::HideCloseButton | ToolWindowManager::DisallowFloatWindow);

  ui->docking->addToolWindow(ui->helpGroup,
                             ToolWindowManager::AreaReference(ToolWindowManager::AddTo,
                                                              ui->docking->areaOf(ui->replGroup)));
  ui->docking->setToolWindowProperties(
      ui->helpGroup, ToolWindowManager::HideCloseButton | ToolWindowManager::DisallowFloatWindow);

  ToolWindowManager::raiseToolWindow(ui->replGroup);

  QVBoxLayout *layout = new QVBoxLayout(this);
  layout->setSpacing(3);
  layout->setContentsMargins(3, 3, 3, 3);
  layout->addWidget(ui->toolbar);
  layout->addWidget(ui->docking);

  enableButtons(true);

  Q_ASSERT(ui->outputContext->count() == AllOutputFilter);
  ui->outputContext->addItem(tr("All"));
  Q_ASSERT(ui->outputContext->count() == ScriptOutputFilter);
  ui->outputContext->addItem(tr("Script"));
  Q_ASSERT(ui->outputContext->count() == FirstExtensionOutputFilter);

  for(const ExtensionMetadata &e : m_Ctx.Extensions().GetInstalledExtensions())
  {
    if(m_Ctx.Extensions().IsExtensionLoaded(e.package))
    {
      ui->outputContext->addItem(tr("Extension %1").arg(e.package));
      loadedExtensions.push_back(e.package);
    }
  }

  QObject::connect(PythonContext::GetExtensionContext(), &PythonContext::textOutput, this,
                   &PythonShell::textOutput);
  QObject::connect(PythonContext::GetExtensionContext(), &PythonContext::exception, this,
                   &PythonShell::exception);
  QObject::connect(PythonContext::GetExtensionContext(), &PythonContext::extensionLoaded, this,
                   &PythonShell::extensionLoaded);

  helpContext = newContext();

  helpContext->makeHelpContext();

  QObject::connect(helpContext, &PythonContext::textOutput,
                   [this](const QString &, bool isStdError, const QString &output) {
                     appendText(ui->helpText, output);
                   });

  m_Ctx.GetMainWindow()->RegisterShortcut("CTRL+S", this,
                                          [this](QWidget *) { this->on_saveScript_clicked(); });

  updateButtonStates();

  // we defer debugging loading onto a thread so check after a delay
  QTimer::singleShot(1200, [this]() {
    if(!PythonContext::IsDebuggingEnabled())
    {
      updateButtonStates();
    }
  });
}

PythonShell::~PythonShell()
{
  m_Ctx.BuiltinWindowClosed(this);

  m_Ctx.GetMainWindow()->UnregisterShortcut("CTRL+S", this);

  // on a clean shutdown, remove the unsaved temp file
  QString unsavedFile = interactiveContext->GetTempFilename(lit("script.py"));

  if(!unsavedFile.isEmpty() && QFile::exists(unsavedFile))
    QFile::remove(unsavedFile);

  for(EditorWrapper *edit : m_Editors)
    delete edit;

  delete m_ToolTip;

  completionContext->Finish();
  interactiveContext->Finish();
  helpContext->Finish();

  delete ui;
}

void PythonShell::doSyntaxCheck()
{
  EditorWrapper *editor = curEditor();

  if(!editor)
    return;

  ScintillaEdit *sc = editor->scintilla();

  if(sc->lexer() != SCLEX_PYTHON)
    return;

  // don't syntax check while the user still seems to be editing, e.g. with autocomplete or a
  // function tooltip active. The syntax check timer will be restarted when these go away
  if(sc->autoCActive() || m_FuncTip)
    return;

  // also don't do it while running, as it makes no sense and also could stall the UI thread if we're debugging
  if(runningScriptEditor)
    return;

  QByteArray script = sc->getText(sc->textLength() + 1);
  PyParseError parseError = completionContext->CheckPyParse(script, "script.py");

  if(parseError.lineno >= 0)
  {
    sptr_t end = sc->lineLength(parseError.lineno - 1);
    sptr_t linePos = sc->positionFromLine(parseError.lineno - 1);
    while(QChar(QLatin1Char(script[int(linePos + end - 1)])).isSpace())
      end--;
    sc->setIndicatorCurrent(0);
    sc->indicatorFillRange(linePos + parseError.offset - 1, end + 1 - parseError.offset);

    sc->annotationSetText(parseError.lineno - 1, parseError.errStr.c_str());
    sc->annotationSetVisible(ANNOTATION_BOXED);
    sc->annotationSetStyle(parseError.lineno - 1, STYLE_ERROR);
  }
}

void PythonShell::editorTab_Changed(int index)
{
  EditorWrapper *editor = curEditor();

  ui->saveScript->setEnabled(editor && editor->filename() != QString());

  updateButtonStates();

  updateNonDebugWarning();
}

void PythonShell::openFileModified(const QString &path)
{
  for(EditorWrapper *edit : m_Editors)
  {
    if(edit->filename() == path)
    {
      // delay slightly to avoid reading while the file is being written or if it was deleted before
      // being written as some editors do
      QTimer::singleShot(150, [this, edit, path]() {
        bool mod = edit->isModified();

        // re-add the path, it may have been removed if the file was deleted
        m_Watcher->addPath(path);

        if(!mod && !m_Ctx.Config().Python_PromptReloadUnchanged)
        {
          // unconditionally reload
        }
        else
        {
          QString prompt = tr("Reload from disk and overwrite your changes?");
          if(!mod)
            prompt = tr("Reload from disk?");

          QMessageBox::StandardButton response = RDDialog::question(
              this, tr("%1 has been modified on disk").arg(QFileInfo(path).fileName()),
              tr("%1 has been modified on disk. %2").arg(QFileInfo(path).fileName()).arg(prompt));

          if(response == QMessageBox::No)
            return;
        }

        QFile f(path);
        if(f.open(QIODevice::ReadOnly | QIODevice::Text))
        {
          edit->scintilla()->setText(f.readAll().data());
          edit->markModified(false);
          return;
        }
      });
    }
  }
}

void PythonShell::editorTab_Menu(const QPoint &pos)
{
  ToolWindowManagerArea *editorTabs = ui->docking->areaOf(m_Editors[0]);

  int tabIndex = editorTabs->tabBar()->tabAt(pos);

  if(tabIndex == -1)
    return;

  if(editorTabs->count() == 1)
    return;

  QAction closeTab(tr("Close tab"), this);
  QAction closeOtherTabs(tr("Close other tabs"), this);
  QAction closeRightTabs(tr("Close tabs to the right"), this);

  QMenu contextMenu(this);

  contextMenu.addAction(&closeTab);
  contextMenu.addAction(&closeOtherTabs);
  contextMenu.addAction(&closeRightTabs);

  QObject::connect(&closeTab, &QAction::triggered, [editorTabs, tabIndex]() {
    // remove the tab at this index
    delete editorTabs->widget(tabIndex);
  });

  QObject::connect(&closeRightTabs, &QAction::triggered, [editorTabs, tabIndex]() {
    for(int i = editorTabs->count() - 1; i > tabIndex; i--)
      delete editorTabs->widget(i);
  });

  QObject::connect(&closeOtherTabs, &QAction::triggered, [editorTabs, tabIndex]() {
    for(int i = editorTabs->count() - 1; i >= 0; i--)
      if(i != tabIndex)
        delete editorTabs->widget(i);
  });

  RDDialog::show(&contextMenu, QCursor::pos());
}

void PythonShell::updateExtensionProjects()
{
  RDTreeViewExpansionState expansion;
  ui->projectExplorer->saveExpansion(expansion, 0, Qt::UserRole + 1);

  ui->projectExplorer->beginUpdate();

  m_UIExtensions->clear();

  for(const ExtensionMetadata &ext : m_Ctx.Extensions().GetInstalledExtensions())
  {
    QString name = ext.name;

    if(ext.hasChanges)
      name += tr(" (Reload required)");

    if(ext.failedLoad)
      name += tr(" (Failed to load)");

    RDTreeWidgetItem *root = new RDTreeWidgetItem({name});
    root->setData(0, Qt::UserRole + 1, ext.package);

    if(ext.hasChanges)
      root->setItalic(true);

    if(m_Ctx.Config().AlwaysLoad_Extensions.contains(ext.package))
      root->setBold(true);

    addExtensionDirItems(root, QDir(ext.filePath));

    m_UIExtensions->addChild(root);
  }

  m_NewExtension = new RDTreeWidgetItem({tr("Create new...")});
  m_NewExtension->setData(0, Qt::UserRole + 1, m_NewExtension->text(0));
  m_NewExtension->setItalic(true);
  m_NewExtension->setIcon(0, Icons::plugin_add());

  m_UIExtensions->addChild(m_NewExtension);

  ui->projectExplorer->endUpdate();

  ui->projectExplorer->applyExpansion(expansion, 0, Qt::UserRole + 1);
}

void PythonShell::addExtensionDirItems(RDTreeWidgetItem *root, QDir dir)
{
  m_Watcher->addPath(dir.absolutePath());
  for(QString child : dir.entryList(QDir::AllEntries | QDir::NoDotAndDotDot))
  {
    QString path = dir.absoluteFilePath(child);
    QFileInfo fileInfo(path);

    // only show .py files, .md files (for README.md) and the extension.json
    if(fileInfo.isFile() && fileInfo.suffix().toLower() != lit("py") &&
       fileInfo.suffix().toLower() != lit("md") && child.toLower() != lit("extension.json"))
      continue;

    RDTreeWidgetItem *item = new RDTreeWidgetItem({child});
    item->setData(0, Qt::UserRole + 1, child);
    if(fileInfo.isDir())
    {
      // ignore some directories
      if(child.toLower() == lit("__pycache__") || child.toLower() == lit(".git") ||
         child.toLower() == lit(".vscode"))
        continue;

      item->setIcon(0, Icons::folder());

      addExtensionDirItems(item, QDir(path));
    }
    else
    {
      item->setData(0, Qt::UserRole, path);
    }

    root->addChild(item);
  }
}

EditorWrapper *PythonShell::curEditor()
{
  for(EditorWrapper *edit : m_Editors)
  {
    if(edit->isVisible())
      return edit;
  }

  return NULL;
}

EditorWrapper *PythonShell::makeEditor(rdcstr filename, rdcstr text)
{
  EditorWrapper *editor = new EditorWrapper(this);
  editor->setObjectName(lit("scriptEditor"));

  ScintillaEdit *sc = editor->scintilla();

  editor->setFilename(filename);

  sc->indicSetFore(0, 0x0000ff);

  m_FindReplace->configureFindIndicator(sc);

  sc->styleSetFont(STYLE_DEFAULT, Formatter::FixedFont().family().toUtf8().data());
  sc->styleSetSize(STYLE_DEFAULT, Formatter::FixedFont().pointSize());
  sc->styleSetFont(STYLE_ERROR, Formatter::FixedFont().family().toUtf8().data());
  sc->styleSetSize(STYLE_ERROR, Formatter::FixedFont().pointSize());
  sc->styleSetBack(STYLE_ERROR,
                   IsDarkTheme() ? SCINTILLA_COLOUR(175, 70, 70) : SCINTILLA_COLOUR(255, 150, 150));

  sc->setMarginLeft(4.0);
  sc->setMarginWidthN(0, 32.0);
  sc->setMarginWidthN(1, 0.0);
  sc->setMarginWidthN(2, 16.0);

  sc->markerSetBack(CURRENT_MARKER, SCINTILLA_COLOUR(240, 128, 128));
  sc->markerSetBack(CURRENT_MARKER + 1, SCINTILLA_COLOUR(240, 128, 128));
  sc->markerDefine(CURRENT_MARKER, SC_MARK_SHORTARROW);
  sc->markerDefine(CURRENT_MARKER + 1, SC_MARK_BACKGROUND);

  sc->usePopUp(SC_POPUP_NEVER);

  sc->setContextMenuPolicy(Qt::CustomContextMenu);
  QObject::connect(sc, &ScintillaEdit::customContextMenuRequested, this,
                   &PythonShell::editor_contextMenu);

  QString suffix;

  if(!filename.isEmpty())
    suffix = QFileInfo(filename).suffix().toLower();
  if(suffix == lit("md"))
  {
    ConfigureSyntax(sc, SCLEX_NULL);
    sc->setWrapMode(SC_WRAP_WORD);
  }
  else if(suffix.toLower() == lit("json"))
  {
    ConfigureSyntax(sc, SCLEX_JSON);
    sc->setWrapMode(SC_WRAP_WORD);
  }
  else
  {
    ConfigureSyntax(sc, SCLEX_PYTHON);
  }

  sc->setTabWidth(4);
  sc->setUseTabs(false);

  sc->setScrollWidth(1);
  sc->setScrollWidthTracking(true);

  sc->colourise(0, -1);

  sc->autoCSetMaxHeight(10);
  sc->autoCSetCancelAtStart(false);

  sc->setMouseDwellTime(400);

  sc->installEventFilter(this);

  // start syntax checking if we exit autocomplete
  QObject::connect(sc, &ScintillaEdit::autoCompleteCancelled,
                   [this]() { m_SyntaxCheckTimer->start(); });
  QObject::connect(sc, &ScintillaEdit::autoCompleteSelection,
                   [this]() { m_SyntaxCheckTimer->start(); });

  QObject::connect(
      sc, &ScintillaEdit::modified,
      [this, editor, sc](int type, int, int, int, const QByteArray &text, int, int, int) {
        if(type & (SC_MOD_INSERTTEXT | SC_MOD_DELETETEXT | SC_MOD_BEFOREINSERT | SC_MOD_BEFOREDELETE))
        {
          m_FindReplace->clearFindState();

          editor->markModified(true);

          sc->markerDeleteAll(CURRENT_MARKER);
          sc->markerDeleteAll(CURRENT_MARKER + 1);

          // always remove errors immediately
          sc->setIndicatorCurrent(0);
          sc->indicatorClearRange(0, sc->textLength());
          sc->annotationClearAll();

          m_SyntaxCheckTimer->start();
        }

        if(type & (SC_MOD_INSERTTEXT | SC_MOD_DELETETEXT))
        {
          if(!sc->autoCActive() || text.contains('\r') || text.contains('\n'))
          {
            completionContext->reflectSource(QString::fromUtf8(sc->getText(sc->textLength() + 1)));
          }
          else if(sc->autoCActive())
          {
            // delay updating the autocomplete so the current cursor position is updated
            GUIInvoke::defer(this, [this, sc]() { doAutocomplete(sc); });
          }
        }
      });

  QObject::connect(sc, &ScintillaEdit::dwellStart, [this, sc](int x, int y) {
    if(sc->autoCActive())
      return;

    if(m_ToolTip->isVisible() && m_FuncTip)
      return;

    if(!sc->geometry().contains(sc->mapFromGlobal(QCursor::pos())))
      return;

    QWidget *widgetInCursor = QApplication::widgetAt(QCursor::pos());
    if(widgetInCursor && sc != widgetInCursor && sc != widgetInCursor->parentWidget())
      return;

    sptr_t pos = sc->positionFromPointClose(x, y);

    if(pos == -1)
      return;

    sptr_t line = sc->lineFromPosition(pos);
    sptr_t col = pos - sc->positionFromLine(line);

    QString tooltip = completionContext->tooltipForLoc(line + 1, col);

    if(!tooltip.isEmpty())
    {
      hideFunccompleteTooltip();

      m_ToolTip->configureTip(this, tooltip);
      m_ToolTip->showTipAtPos(QCursor::pos() + QPoint(5, 5));
    }
  });

  QObject::connect(sc, &ScintillaEdit::dwellEnd, [this, sc](int, int) {
    if(sc->autoCActive())
      return;

    if(!m_FuncTip)
      m_ToolTip->hideTip();
  });

  QObject::connect(sc, &ScintillaEdit::charAdded, [this, sc](int ch) { doAutocomplete(sc); });

  QObject::connect(sc, &ScintillaEdit::buttonPressed,
                   [this](QMouseEvent *ev) { hideFunccompleteTooltip(); });

  QObject::connect(sc, &ScintillaEdit::keyPressed, [this, sc](QKeyEvent *ev) {
    if(ev->key() == Qt::Key_Space && (ev->modifiers() & Qt::ControlModifier))
      doAutocomplete(sc);

    if(m_ToolTip->isVisible() && m_FuncTip)
    {
      if(sc->lineFromPosition(sc->currentPos()) == m_FuncTipLine)
      {
        doFunccomplete(sc);
        return;
      }

      hideFunccompleteTooltip();
    }

    if(ev->key() == Qt::Key_F1)
    {
      sptr_t pos = sc->currentPos();

      if(pos >= 0)
      {
        sptr_t line = sc->lineFromPosition(pos);
        sptr_t col = pos - sc->positionFromLine(line);

        QString typeName = completionContext->typenameForLoc(line + 1, col);

        if(!typeName.isEmpty())
          selectedHelp(typeName);
      }
    }

    m_FindReplace->handleEditorKeypress(sc, ev);
  });

  if(m_Editors.empty())
  {
    ui->docking->addToolWindow(editor, ToolWindowManager::EmptySpace);
  }
  else
  {
    ui->docking->addToolWindow(editor,
                               ToolWindowManager::AreaReference(ToolWindowManager::AddTo,
                                                                ui->docking->areaOf(m_Editors[0])));
  }

  m_Editors.push_back(editor);
  m_Scintillas.push_back(sc);

  updateEditorCloseButton();

  sc->setText(text.c_str());
  sc->emptyUndoBuffer();
  editor->markModified(false);

  return editor;
}

void PythonShell::updateEditorCloseButton()
{
  for(EditorWrapper *edit : m_Editors)
  {
    ToolWindowManager::ToolWindowProperty props =
        ToolWindowManager::DisallowUserDocking | ToolWindowManager::AlwaysDisplayFullTabs;

    // disallow closing last scintilla
    if(m_Editors.size() == 1)
      props = props | ToolWindowManager::HideCloseButton;

    ui->docking->setToolWindowProperties(edit, props);
  }
}

void PythonShell::updateNonDebugWarning()
{
  m_DebuggerAttached = PythonContext::IsDebuggerConnected();

  for(EditorWrapper *edit : m_Editors)
  {
    if(edit->filename().isEmpty())
    {
      if(m_DebuggerAttached)
        edit->setWarning(
            tr("External debugger will not work for unsaved files. "
               "Save script to disk to allow debugging."));
      else
        edit->setWarning(QString());
    }
    else
    {
      edit->setWarning(QString());
    }
  }

  updateButtonStates();
}

void PythonShell::updateButtonStates()
{
  enableButtons(ui->newScript->isEnabled());
}

void PythonShell::addRecentFile(rdcstr filename)
{
  // don't add recent files from UI extensions
  for(const ExtensionMetadata &ext : m_Ctx.Extensions().GetInstalledExtensions())
  {
    if(QString(QFileInfo(filename).absolutePath())
           .toLower()
           .startsWith(QDir(ext.filePath).absolutePath().toLower()))
    {
      return;
    }
  }

  m_Ctx.Config().Python_RecentFiles.removeIf([filename](const rdcstr &o) { return o == filename; });

  if(m_Ctx.Config().Python_RecentFiles.size() == 10)
    m_Ctx.Config().Python_RecentFiles.pop_back();

  m_Ctx.Config().Python_RecentFiles.insert(0, filename);

  m_Ctx.Config().Save();

  updateRecentFiles(true);
}

void PythonShell::updateRecentFiles(bool added)
{
  ui->projectExplorer->beginUpdate();

  bool expanded = ui->projectExplorer->isItemExpanded(m_RecentFiles);

  // expand if we're adding the first recent file
  if(m_Ctx.Config().Python_RecentFiles.size() == 1 && added)
    expanded = true;

  m_RecentFiles->clear();

  QString unsavedFile = interactiveContext->GetTempFilename(lit("script.py"));

  if(!unsavedFile.isEmpty() && QFile::exists(unsavedFile) && !m_IgnoreRecovered)
  {
    RDTreeWidgetItem *recent = new RDTreeWidgetItem({tr("Recovered Script")});
    recent->setItalic(true);
    recent->setData(0, Qt::UserRole, QString(unsavedFile));
    m_RecentFiles->addChild(recent);
  }
  else
  {
    // we didn't have a recovered script, remember that and don't display the file in future
    // refreshes appears in future due to temp script running. When the window is closed the script
    // will be removed, and if we crash and restart then the script will be found
    m_IgnoreRecovered = true;
  }

  for(rdcstr file : m_Ctx.Config().Python_RecentFiles)
  {
    RDTreeWidgetItem *recent = new RDTreeWidgetItem({QFileInfo(file).fileName()});
    recent->setData(0, Qt::UserRole, QString(file));
    m_RecentFiles->addChild(recent);
  }

  ui->projectExplorer->endUpdate();

  if(expanded)
    ui->projectExplorer->expandItem(m_RecentFiles);
  else
    ui->projectExplorer->collapseItem(m_RecentFiles);
}

bool PythonShell::eventFilter(QObject *watched, QEvent *event)
{
  if(qobject_cast<ScintillaEdit *>(watched))
  {
    if(event->type() == QEvent::Leave)
    {
      if(!m_FuncTip)
      {
        m_ToolTip->hideTip();
      }
      else if(m_FuncTip)
      {
        if(!m_ToolTip->geometry().contains(QCursor::pos()))
        {
          hideFunccompleteTooltip();
        }
      }
    }
    else if(event->type() == QEvent::KeyPress && ((QKeyEvent *)event)->key() == Qt::Key_Escape)
    {
      hideFunccompleteTooltip();
    }
  }

  if(m_InteractiveCompleter && watched == m_InteractiveCompleter->popup() &&
     (event->type() == QEvent::Hide || event->type() == QEvent::FocusOut))
  {
    m_CurrentCompletionTip = -1;
    updateCompletionTip();
  }

  if(m_FuncTip && watched == m_FuncTipWidget && event->type() == QEvent::FocusOut)
  {
    hideFunccompleteTooltip();
  }

  return QObject::eventFilter(watched, event);
}

QVariant PythonShell::persistData()
{
  QVariantMap state = ui->docking->saveState();

  RDTreeViewExpansionState expansion;
  ui->projectExplorer->saveExpansion(expansion, 0);

  QVariantList expansionList;
  for(uint x : expansion)
    expansionList.append(x);
  state[lit("projectExplorerExpansion")] = expansionList;

  return state;
}

void PythonShell::setPersistData(const QVariant &persistData)
{
  QVariantMap state = persistData.toMap();

  ui->docking->restoreState(state);

  ui->projectExplorer->collapseAll();

  RDTreeViewExpansionState expansion;
  for(QVariant x : state[lit("projectExplorerExpansion")].toList())
    expansion.insert(x.toUInt());
  ui->projectExplorer->applyExpansion(expansion, 0);

  if(ui->docking->areaOf(m_Editors[0]) == NULL)
    ui->docking->addToolWindow(m_Editors[0], ToolWindowManager::AreaReference(
                                                 ToolWindowManager::RightOf,
                                                 ui->docking->areaOf(ui->projectExplorer), 0.8f));

  setupTabs();
}

void PythonShell::setupTabs()
{
  ToolWindowManagerArea *editorTabs = ui->docking->areaOf(m_Editors[0]);

  QObject::connect(editorTabs, &QTabWidget::currentChanged, this, &PythonShell::editorTab_Changed);

  editorTabs->tabBar()->setContextMenuPolicy(Qt::CustomContextMenu);

  QObject::connect(editorTabs->tabBar(), &QTabBar::customContextMenuRequested, this,
                   &PythonShell::editorTab_Menu);
}

PythonContext *PythonShell::GetScriptContext()
{
  return scriptContext;
}

bool PythonShell::CheckUnsavedChanges()
{
  return checkAllowClose();
}

bool PythonShell::LoadScriptFromFilename(rdcstr filename)
{
  if(!filename.isEmpty())
  {
    QFile f(filename);
    if(f.open(QIODevice::ReadOnly | QIODevice::Text))
    {
      makeEditor(filename, f.readAll().data());

      ui->saveScript->setEnabled(true);

      QString unsavedFile = interactiveContext->GetTempFilename(lit("script.py"));
      if(QString(filename) != unsavedFile)
        addRecentFile(filename);

      m_Watcher->addPath(filename);

      return true;
    }
  }

  return false;
}

void PythonShell::CreateNewScriptEditor(rdcstr name, rdcstr text)
{
  EditorWrapper *ed = makeEditor("", text);

  QFileInfo info(name);
  if(info.isAbsolute() && (info.exists() || info.absoluteDir().exists()))
    ed->setFilename(name);
  else
    ed->setTitle(name);

  ui->saveScript->setEnabled(false);
  updateNonDebugWarning();
}

rdcstr PythonShell::GetScriptText()
{
  EditorWrapper *editor = curEditor();

  if(!editor)
    return rdcstr();

  ScintillaEdit *sc = editor->scintilla();

  return sc->getText(sc->textLength() + 1).data();
}

void PythonShell::SetExtensionOutputFilter(const rdcstr &extensionName)
{
  if(extensionName.empty())
    return;

  int idx = loadedExtensions.indexOf(extensionName);
  if(idx < 0)
    return;

  ui->outputContext->setCurrentIndex(FirstExtensionOutputFilter + idx);
}

void PythonShell::SetScriptOutputFilter()
{
  ui->outputContext->setCurrentIndex(ScriptOutputFilter);
}

void PythonShell::RemoveOutputFilter()
{
  ui->outputContext->setCurrentIndex(AllOutputFilter);
}

void PythonShell::ShowOutput()
{
  ToolWindowManager::raiseToolWindow(ui->outputGroup);
}

void PythonShell::ShowREPL()
{
  ToolWindowManager::raiseToolWindow(ui->replGroup);
}

void PythonShell::ShowHelp()
{
  ToolWindowManager::raiseToolWindow(ui->helpGroup);
}

void PythonShell::RunScript()
{
  EditorWrapper *editor = curEditor();

  if(!editor)
    return;

  if(editor->isModified() && !editor->filename().isEmpty())
    saveEditor(editor, editor->filename());

  PythonContext *context = newContext();

  ANALYTIC_SET(UIFeatures.PythonInterop, true);

  ShowOutput();

  scriptOutputLines.removeIf([](const ScriptOutputLine &l) { return l.extension.isEmpty(); });

  updateScriptOutput(true);

  QString script = GetScriptText();

  enableButtons(false);

  // save any changes
  if(editor->isModified() && !editor->filename().isEmpty())
  {
    saveEditor(editor, editor->filename());
  }
  else if(editor->filename().isEmpty())
  {
    // write the script as a temporary file, so if we crash it will be recoverable.
    // this only applies if editor is an unsaved file
    QString unsavedFile = context->GetTempFilename(lit("script.py"));
    QFile f(unsavedFile);
    f.open(QFile::Truncate | QFile::WriteOnly);
    f.write(script.toUtf8().data());
    f.close();
  }

  m_CurLineTimer->start();

  LambdaThread *thread = new LambdaThread([this, script, context, editor]() {
    PythonContext::AddDebuggableThread();

    scriptContext = context;
    runningScriptEditor = editor->scintilla();
    context->executeString(editor->filename(), script);
    scriptContext = NULL;

    GUIInvoke::call(this, [this, context]() {
      m_CurLineTimer->stop();

      context->Finish();
      runningScriptEditor = NULL;
      enableButtons(true);

      QString unsavedFile = context->GetTempFilename(lit("script.py"));
      QFile::remove(unsavedFile);
    });

    PythonContext::RemoveDebuggableThread();
  });

  thread->setName(lit("Python script"));
  thread->selfDelete(true);
  thread->start();
}

void PythonShell::AttachDebugger(const rdcstr &contextLocation)
{
  QString path = contextLocation;
  for(const ExtensionMetadata &e : m_Ctx.Extensions().GetInstalledExtensions())
  {
    if(e.package == contextLocation)
      path = e.filePath;
  }

  if(!QDir(path).exists())
    return;

  PythonContext::LaunchDebugger(this, m_Ctx.Config(), path);

  updateButtonStates();
  updateNonDebugWarning();
}

void PythonShell::on_findReplace_clicked()
{
  m_FindReplace->raiseOrShow();
}

void PythonShell::on_execute_clicked()
{
  QString command = ui->lineInput->text();

  ANALYTIC_SET(UIFeatures.PythonInterop, true);

  appendText(ui->interactiveOutput, command + lit("\n"));

  if(!command.trimmed().isEmpty())
    history.push_front(command);
  historyidx = -1;

  ui->lineInput->clear();

  // assume a trailing colon means there will be continuation. Store the command and add a continue
  // prompt. If we're already continuing, then wait until we get a blank line before executing.
  if(command.trimmed().right(1) == lit(":") || (!m_storedLines.isEmpty() && !command.isEmpty()))
  {
    appendText(ui->interactiveOutput, lit(".. "));
    m_storedLines += command + lit("\n");
    return;
  }

  // concatenate any previous lines if we are doing a multi-line command.
  command = m_storedLines + command;
  m_storedLines = QString();

  if(command.trimmed().length() > 0)
  {
    interactiveContext->executeString(command);
    interactiveContext->reflectSource(QString());
  }

  appendText(ui->interactiveOutput, lit(">> "));
}

void PythonShell::on_clear_clicked()
{
  QString minidocHeader = scriptHeader();

  minidocHeader += lit("\n\n>> ");

  ui->interactiveOutput->setText(minidocHeader);

  if(interactiveContext)
    interactiveContext->Finish();

  interactiveContext = newContext();
  interactiveContext->reflectSource(QString());
}

void PythonShell::on_newScript_clicked()
{
  QString minidocHeader = scriptHeader();

  minidocHeader.replace(QLatin1Char('\n'), lit("\n# "));

  minidocHeader = QFormatStr("# %1\n\n").arg(minidocHeader);

  CreateNewScriptEditor("", minidocHeader);
}

void PythonShell::on_openScript_clicked()
{
  QString filename = RDDialog::getOpenFileName(this, tr("Open Python Script"), QString(),
                                               tr("Python scripts (*.py)"));

  if(!LoadScriptFromFilename(filename))
  {
    RDDialog::critical(this, tr("Error loading script"), tr("Couldn't open path %1.").arg(filename));
  }
}

void PythonShell::on_saveScript_clicked()
{
  EditorWrapper *editor = curEditor();

  if(!editor)
    return;

  QString filename = editor->filename();

  if(!QFileInfo(filename).isAbsolute())
    return on_saveAsScript_clicked();

  if(saveEditor(editor, filename))
    editor->markModified(false);
}

void PythonShell::on_saveAsScript_clicked()
{
  EditorWrapper *editor = curEditor();

  if(!editor)
    return;

  if(saveEditorAs(editor))
    editor->markModified(false);
}

void PythonShell::on_runScript_clicked()
{
  RunScript();
}

void PythonShell::on_debugAttach_clicked()
{
  EditorWrapper *editor = curEditor();

  if(!editor)
    return;

  QString filename = editor->filename();

  if(editor->isUIExtension())
  {
    filename = QFileInfo(filename).absolutePath();

    for(const ExtensionMetadata &e : m_Ctx.Extensions().GetInstalledExtensions())
    {
      if(filename.startsWith(QDir(e.filePath).absolutePath()))
      {
        AttachDebugger(e.package);
        return;
      }
    }
  }
  else if(!filename.isEmpty())
  {
    AttachDebugger(QFileInfo(filename).absoluteDir().absolutePath());
  }
}

void PythonShell::on_abortRun_clicked()
{
  if(scriptContext)
    scriptContext->abort();
}

void PythonShell::traceLine(const QString &file, int line)
{
  if(!runningScriptEditor)
    return;

  // we only update the current line on a fixed timer to avoid DoS'ing ourselves with too many rapid
  // updates. Both happen on the UI thread so we just need a simple flag
  m_CurLine = line;
  m_CurLineDirty = true;
}

void PythonShell::exception(const QString &extension, const QString &type, const QString &value,
                            int finalLine, QList<QString> frames)
{
  if(finalLine >= 0)
    traceLine(QString(), finalLine);

  QString exString;

  if(!frames.isEmpty())
  {
    exString += tr("Traceback (most recent call last):\n");
    for(const QString &f : frames)
      exString += QFormatStr("  %1\n").arg(f);
  }
  exString += QFormatStr("%1: %2\n").arg(type).arg(value);

  if(QObject::sender() == (QObject *)interactiveContext)
  {
    appendText(ui->interactiveOutput, exString);
    return;
  }

  exString.insert(0, QLatin1Char('\n'));

  scriptOutputLines.push_back({extension, exString});

  updateScriptOutput(false);
}

void PythonShell::textOutput(const QString &extension, bool isStdError, const QString &output)
{
  if(QObject::sender() == (QObject *)interactiveContext)
  {
    appendText(ui->interactiveOutput, output);
    return;
  }

  scriptOutputLines.push_back({extension, output});
  updateScriptOutput(false);
}

void PythonShell::on_outputContext_currentIndexChanged(int idx)
{
  updateScriptOutput(true);
}

void PythonShell::on_projectExplorer_itemActivated(RDTreeWidgetItem *item, int column)
{
  // ignore these, just let them collapse/expand
  if(item == m_Examples || item == m_UIExtensions || item == m_RecentFiles)
    return;

  if(item == m_NewExtension)
  {
    createExtension_clicked();
  }
  else if(item->parent() == m_Examples)
  {
    QString title = tr("Example: ") + item->text(0);
    QString text = item->data(0, Qt::UserRole).toString();

    for(EditorWrapper *edit : m_Editors)
    {
      if(edit->filename() == title || edit->title() == title)
      {
        ToolWindowManager::raiseToolWindow(edit);
        return;
      }
    }

    CreateNewScriptEditor("", text);

    m_Editors.back()->setTitle(title);
  }
  else
  {
    // recent file or UI extension, the user role contains the path
    QString filename = item->data(0, Qt::UserRole).toString();

    // directories in UI extensions have no filename to activate
    if(filename.isEmpty())
      return;

    bool isExt = false;
    RDTreeWidgetItem *parent = item;
    while(parent)
    {
      if(parent == m_UIExtensions)
        isExt = true;
      parent = parent->parent();
    }

    for(EditorWrapper *edit : m_Editors)
    {
      if(edit->filename() == filename)
      {
        ToolWindowManager::raiseToolWindow(edit);
        return;
      }
    }

    if(!QFileInfo(filename).exists())
    {
      QMessageBox::StandardButton response = RDDialog::question(
          this, tr("'%1' does not exist").arg(QFileInfo(filename).fileName()),
          tr("File not found at path:\n%1\n\nRemove from recent files list?").arg(filename));

      if(response == QMessageBox::Yes)
      {
        rdcstr f = filename;
        m_Ctx.Config().Python_RecentFiles.removeIf([f](const rdcstr &o) { return o == f; });

        m_Ctx.Config().Save();

        GUIInvoke::defer(this, [this]() { updateRecentFiles(false); });
      }

      return;
    }

    LoadScriptFromFilename(filename);

    if(isExt)
      m_Editors.back()->setUIExtension(true);

    editorTab_Changed(-1);

    QString unsavedFile = interactiveContext->GetTempFilename(lit("script.py"));
    if(filename == unsavedFile)
    {
      QFile::remove(unsavedFile);
      updateRecentFiles(false);
    }
  }
}

bool PythonShell::checkAllowClose()
{
  for(EditorWrapper *edit : m_Editors)
  {
    if(!edit->checkAllowClose())
      return false;
  }
  return true;
}

void PythonShell::updateScriptOutput(bool fullRefresh)
{
  if(fullRefresh)
  {
    ui->scriptOutput->clear();
    lastDisplayedLine = 0;
  }

  for(size_t i = lastDisplayedLine; i < scriptOutputLines.size(); i++)
  {
    bool display = false;

    if(ui->outputContext->currentIndex() == AllOutputFilter)
      display = true;

    // Script
    else if(ui->outputContext->currentIndex() == ScriptOutputFilter)
      display = scriptOutputLines[i].extension.isEmpty();

    else if(loadedExtensions.indexOf(scriptOutputLines[i].extension) + FirstExtensionOutputFilter ==
            ui->outputContext->currentIndex())
      display = true;

    if(display)
    {
      appendText(ui->scriptOutput, scriptOutputLines[i].text);
    }
  }

  lastDisplayedLine = scriptOutputLines.size();
}

bool PythonShell::saveEditorAs(EditorWrapper *editor)
{
  QString filename = RDDialog::getSaveFileName(this, tr("Save Python Script"), QString(),
                                               tr("Python scripts (*.py)"));
  if(filename.isEmpty())
    return false;
  return saveEditor(editor, filename);
}

bool PythonShell::saveEditor(EditorWrapper *editor, QString filename)
{
  QString oldFilename = editor->filename();
  if(!filename.isEmpty())
  {
    QDir dirinfo = QFileInfo(filename).dir();
    if(dirinfo.exists())
    {
      QFile f(filename);
      if(f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
      {
        // remove the path from watching first so we don't get a notification from the write itself
        m_Watcher->removePath(oldFilename);

        ScintillaEdit *sc = editor->scintilla();

        QString text = QString::fromUtf8(sc->getText(sc->textLength() + 1));
        text.remove(QLatin1Char('\r'));
        f.write(text.toUtf8());

        addRecentFile(filename);

        // delay a short while before starting to watch this file. This is highly unlikely to miss
        // any real writes (which would have to happen externally after we save), but prevents us
        // from identifying our own writes as an external modification.
        QTimer::singleShot(200, [this, filename]() { m_Watcher->addPath(filename); });

        editor->setFilename(filename);
        updateNonDebugWarning();
        return true;
      }
      else
      {
        RDDialog::critical(
            this, tr("Error saving script"),
            tr("Couldn't open path %1 for write.\n%2").arg(filename).arg(f.errorString()));
      }
    }
    else
    {
      RDDialog::critical(this, tr("Invalid directory"),
                         tr("Cannot find target directory to save to:\n%1").arg(filename));
    }
  }
  return false;
}

void PythonShell::removeEditor(EditorWrapper *editor)
{
  hideFunccompleteTooltip();
  if(!editor->filename().isEmpty())
    m_Watcher->removePath(editor->filename());
  m_Editors.removeOne(editor);
  m_Scintillas.removeOne(editor->scintilla());
  updateEditorCloseButton();
}

void PythonShell::extensionLoaded(const QString &extension)
{
  if(!loadedExtensions.contains(extension))
  {
    ui->outputContext->addItem(tr("Extension %1").arg(extension));
    loadedExtensions.push_back(extension);
  }
}

void PythonShell::editor_contextMenu(const QPoint &pos)
{
  ScintillaEdit *editor = qobject_cast<ScintillaEdit *>(QObject::sender());

  if(!editor)
    return;

  hideFunccompleteTooltip();

  m_ContextMenuVisible = true;

  QMenu contextMenu(this);

  QString typeName;

  sptr_t scintillaPos = editor->positionFromPoint(pos.x(), pos.y());
  if(scintillaPos >= 0)
  {
    sptr_t line = editor->lineFromPosition(scintillaPos);
    sptr_t col = scintillaPos - editor->positionFromLine(line);

    typeName = completionContext->typenameForLoc(line + 1, col);
  }

  bool valid = !typeName.isEmpty();

  QAction help(valid ? tr("Help for '%1'").arg(typeName) : tr("Help"), this);

  QObject::connect(&help, &QAction::triggered, [this, typeName] { selectedHelp(typeName); });

  help.setEnabled(valid);

  contextMenu.addAction(&help);
  contextMenu.addSeparator();

  QAction undo(tr("Undo"), this);
  QAction redo(tr("Redo"), this);

  QObject::connect(&undo, &QAction::triggered, [editor] { editor->undo(); });
  QObject::connect(&redo, &QAction::triggered, [editor] { editor->redo(); });

  undo.setEnabled(editor->canUndo());
  redo.setEnabled(editor->canRedo());

  contextMenu.addAction(&undo);
  contextMenu.addAction(&redo);
  contextMenu.addSeparator();

  QAction cutText(tr("Cut"), this);
  QAction copyText(tr("Copy"), this);
  QAction pasteText(tr("Paste"), this);
  QAction deleteText(tr("Delete"), this);

  QObject::connect(&cutText, &QAction::triggered, [editor] { editor->cut(); });

  QObject::connect(&copyText, &QAction::triggered, [editor] {
    editor->copyRange(editor->selectionStart(), editor->selectionEnd());
  });

  QObject::connect(&pasteText, &QAction::triggered, [editor] { editor->paste(); });

  QObject::connect(&deleteText, &QAction::triggered, [editor] {
    editor->deleteRange(editor->selectionStart(), editor->selectionEnd());
  });

  contextMenu.addAction(&cutText);
  contextMenu.addAction(&copyText);
  contextMenu.addAction(&pasteText);
  contextMenu.addAction(&deleteText);
  contextMenu.addSeparator();

  if(editor->selectionEmpty())
  {
    cutText.setEnabled(false);
    copyText.setEnabled(false);
    deleteText.setEnabled(false);
  }

  pasteText.setEnabled(editor->canPaste());

  QAction selectAll(tr("Select All"), this);
  QObject::connect(&selectAll, &QAction::triggered, [editor] { editor->selectAll(); });
  contextMenu.addAction(&selectAll);

  RDDialog::show(&contextMenu, editor->viewport()->mapToGlobal(pos));

  m_ContextMenuVisible = false;
}

void PythonShell::projectExplorer_contextMenu(const QPoint &pos)
{
  m_ContextMenuVisible = true;

  RDTreeWidgetItem *item = ui->projectExplorer->itemAt(pos);

  QMenu contextMenu(this);

  QAction expandAll(tr("&Expand All"), this);
  expandAll.setIcon(Icons::arrow_out());

  QAction collapseAll(tr("&Collapse All"), this);
  collapseAll.setIcon(Icons::arrow_in());

  expandAll.setEnabled(item && item->childCount() > 0);
  collapseAll.setEnabled(expandAll.isEnabled());

  QAction reloadExtension(tr("&Reload Extension"), this);
  reloadExtension.setIcon(Icons::update());

  QAction explorerOpen(tr("&Open in File Explorer"), this);
  explorerOpen.setIcon(Icons::folder());

  QAction openEditor(tr("&Edit file"), this);
  openEditor.setIcon(Icons::page_white_edit());

  QAction viewOutput(tr("&View output"), this);
  viewOutput.setIcon(Icons::filter());

  QAction createExtension(tr("Create &New Extension"), this);
  createExtension.setIcon(Icons::plugin_add());

  QAction refreshExtensionList(tr("&Refresh Extension List"), this);
  refreshExtensionList.setIcon(Icons::update());

  QObject::connect(&expandAll, &QAction::triggered,
                   [this, item]() { ui->projectExplorer->expandAllItems(item); });

  QObject::connect(&collapseAll, &QAction::triggered,
                   [this, item]() { ui->projectExplorer->collapseAllItems(item); });

  contextMenu.addAction(&expandAll);
  contextMenu.addAction(&collapseAll);

  if(item && !(item == m_UIExtensions || item == m_Examples || item == m_RecentFiles))
  {
    QString diskLocation = item->data(0, Qt::UserRole).toString();
    contextMenu.addSeparator();

    // if this is the root node of a UI extension, add options to filter output/reload
    if(item->parent() == m_UIExtensions)
    {
      rdcstr itemPath = item->data(0, Qt::UserRole + 1).toString();

      contextMenu.insertAction(contextMenu.actions()[0], &reloadExtension);
      contextMenu.insertSeparator(contextMenu.actions()[1]);

      contextMenu.addAction(&explorerOpen);
      contextMenu.addAction(&viewOutput);

      rdcarray<ExtensionMetadata> exts = m_Ctx.Extensions().GetInstalledExtensions();
      for(const ExtensionMetadata &m : exts)
      {
        if(m.package == rdcstr(itemPath))
        {
          reloadExtension.setEnabled(m.hasChanges || m.failedLoad);
          diskLocation = QFileInfo(m.filePath).absoluteFilePath();
          break;
        }
      }

      explorerOpen.setEnabled(!diskLocation.isEmpty());
      viewOutput.setEnabled(m_Ctx.Extensions().IsExtensionLoaded(itemPath));

      QObject::connect(&reloadExtension, &QAction::triggered,
                       [this, itemPath]() { m_Ctx.Extensions().LoadExtension(itemPath); });

      if(!m_Ctx.Config().AlwaysLoad_Extensions.contains(itemPath))
      {
        reloadExtension.setEnabled(true);
        reloadExtension.setText(tr("Enable extension"));
        reloadExtension.setIcon(Icons::add());

        QObject::connect(&reloadExtension, &QAction::triggered, [this, itemPath]() {
          m_Ctx.Config().AlwaysLoad_Extensions.push_back(itemPath);
          m_Ctx.Config().Save();
        });
      }

      QObject::connect(&explorerOpen, &QAction::triggered,
                       [diskLocation]() { QDesktopServices::openUrl(diskLocation); });

      QObject::connect(&viewOutput, &QAction::triggered, [this, itemPath]() {
        ShowOutput();
        SetExtensionOutputFilter(itemPath);
      });
    }
    else if(item->parent() != m_Examples && !diskLocation.isEmpty())
    {
      // real files - either recent or in UI extensions - have options to open as editors or in
      // explorer, but we ditch the collapse/expand
      contextMenu.clear();

      // get the containing directory, not the file
      diskLocation = QFileInfo(diskLocation).absoluteDir().absolutePath();

      contextMenu.addAction(&openEditor);
      contextMenu.addAction(&explorerOpen);

      QObject::connect(&openEditor, &QAction::triggered,
                       [this, item]() { on_projectExplorer_itemActivated(item, 0); });

      QObject::connect(&explorerOpen, &QAction::triggered,
                       [diskLocation]() { QDesktopServices::openUrl(diskLocation); });
    }
  }
  else if(item == m_UIExtensions)
  {
    contextMenu.addSeparator();

    contextMenu.addAction(&createExtension);
    contextMenu.addAction(&refreshExtensionList);

    QObject::connect(&createExtension, &QAction::triggered, [this]() { createExtension_clicked(); });
    QObject::connect(&refreshExtensionList, &QAction::triggered,
                     [this]() { updateExtensionProjects(); });
  }

  RDDialog::show(&contextMenu, ui->projectExplorer->viewport()->mapToGlobal(pos));

  m_ContextMenuVisible = false;
}

void PythonShell::createExtension_clicked()
{
  QDialog dialog;
  RDLabel label;
  RDLineEdit extensionName;
  QDialogButtonBox buttons;

  dialog.setWindowTitle(tr("Create new UI extension"));
  dialog.setWindowFlags(dialog.windowFlags() & ~Qt::WindowContextHelpButtonHint);

  label.setText(
      tr("Create a new UI extension, with some example code.\n"
         "\n"
         "This will create the directory structure for the specified package name, with a default\n"
         "extension metadata json and some simple example code to give you a starting point."));

  extensionName.setPlaceholderText(tr("myname.example"));

  buttons.setOrientation(Qt::Horizontal);
  buttons.setStandardButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
  buttons.setCenterButtons(true);

  QObject::connect(&buttons, &QDialogButtonBox::accepted, [this, &dialog, &extensionName]() {
    QString extName = extensionName.text().trimmed();

    if(extName.isEmpty())
    {
      RDDialog::critical(&dialog, tr("Invalid extension name"),
                         tr("Must specify a name for the new extension."));
      return;
    }

    if(extName.startsWith(lit("renderdoc.")))
    {
      RDDialog::critical(&dialog, tr("Invalid extension name"),
                         tr("Extension name conflicts with builtin module 'renderdoc'."));
      return;
    }

    if(extName.contains(QLatin1Char(' ')) || extName.contains(QLatin1Char('\t')))
    {
      RDDialog::critical(
          &dialog, tr("Invalid extension name"),
          tr("Extension names should be valid python package names, note including whitespace."));
      return;
    }

    for(const ExtensionMetadata &e : m_Ctx.Extensions().GetInstalledExtensions())
    {
      if(QString(e.package) == extName)
      {
        RDDialog::critical(&dialog, tr("Extension name in use"),
                           tr("The extension name '%1' already exists.").arg(e.package));
        return;
      }
    }

    QStringList locations = PythonContext::GetApplicationExtensionsPaths();

    if(!locations.empty())
    {
      QDir dir(locations[0]);

      QStringList paths = extName.split(QLatin1Char('.'));

      bool nonexist = false;

      while(!paths.empty())
      {
        QString dirname = paths[0];
        paths.pop_front();

        if(!dir.cd(dirname))
        {
          nonexist = true;
          break;
        }

        qInfo() << dir.absolutePath();
      }

      if(!nonexist && dir.exists() && !dir.isEmpty())
      {
        RDDialog::critical(&dialog, tr("Directory already exists"),
                           tr("Extension directory already exists:\n%1").arg(dir.absolutePath()));
        return;
      }
    }

    dialog.accept();
  });
  QObject::connect(&buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

  QVBoxLayout *layout = new QVBoxLayout(&dialog);
  layout->addWidget(&label);
  layout->addWidget(&extensionName);
  layout->addWidget(&buttons);

  if(!RDDialog::show(&dialog))
    return;

  if(dialog.result() == QDialog::Accepted)
  {
    QStringList locations = PythonContext::GetApplicationExtensionsPaths();
    QDir dir(locations[0]);

    QString extName = extensionName.text().trimmed();
    QStringList paths = extName.split(QLatin1Char('.'));

    while(!paths.empty())
    {
      QString dirname = paths[0];
      paths.pop_front();

      dir.mkdir(dirname);

      if(!dir.cd(dirname))
      {
        RDDialog::critical(&dialog, tr("Couldn't create directory"),
                           tr("Failed to create %1 in %2").arg(dirname).arg(dir.absolutePath()));
        return;
      }
    }

    paths = extName.split(QLatin1Char('.'));

    QString metadata = lit(R"({
	"extension_api": 1,
	"name": "%3",
	"version": "1.0",
	"minimum_renderdoc": "%1.%2",
	"description": "Template extension %4",
	"author": "My Name <my.email@example.com>",
	"url": "https://github.com/example/example"
}
)")
                           .arg(RENDERDOC_VERSION_MAJOR)
                           .arg(RENDERDOC_VERSION_MINOR)
                           .arg(paths.back())
                           .arg(extName);

    {
      QFile ext(dir.absoluteFilePath(lit("extension.json")));
      if(ext.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
      {
        ext.write(metadata.toUtf8());
      }

      QFile init(dir.absoluteFilePath(lit("__init__.py")));
      if(init.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
      {
        init.write(R"(
# Blank RenderDoc UI extension

import renderdoc as rd
import qrenderdoc as qrd

def register(version: str, pyrenderdoc: qrd.CaptureContext):
    print(f"New UI extension loaded in RenderDoc {version}")

def unregister():
    print(f"New UI extension being unloaded")
)");
      }
    }

    updateExtensionProjects();

    LoadScriptFromFilename(dir.absoluteFilePath(lit("__init__.py")));

    m_Editors.back()->setUIExtension(true);

    editorTab_Changed(-1);
  }
}

void PythonShell::selectedHelp(QString word)
{
  ui->helpSearch->setText(word);

  refreshCurrentHelp();
}

void PythonShell::refreshCurrentHelp()
{
  ToolWindowManager::raiseToolWindow(ui->helpGroup);

  ui->helpText->clear();

  helpContext->executeString(lit(R"(
try:
  import keyword
  if keyword.iskeyword("%1"):
    help("%1")
  else:
    help(%1)
except ImportError:
  help(%1)
)")
                                 .arg(ui->helpSearch->text()));

  ui->helpText->verticalScrollBar()->setValue(0);
}

void PythonShell::interactive_keypress(QKeyEvent *event)
{
  RDLineEdit *edit = qobject_cast<RDLineEdit *>(QObject::sender());
  if(!edit)
    return;

  bool triggerCompletion = false;

  if(m_InteractiveCompleter->popup()->isVisible())
  {
    switch(event->key())
    {
      // manually trigger a completion with tab
      case Qt::Key_Tab:
        // allow prefixed tabs to be inserted
        if(edit->text().trimmed().isEmpty())
        {
          edit->insert(lit("\t"));
        }
        else
        {
          m_InteractiveCompleter->setWidget(edit);
          m_InteractiveCompleter->activated(
              m_InteractiveCompleter->popup()->selectionModel()->currentIndex());
          m_InteractiveCompleter->popup()->hide();
        }
        return;
      // if a completion is in progress ignore any events the completer will process
      case Qt::Key_Return:
      case Qt::Key_Enter: return;
      // allow key scrolling
      case Qt::Key_Up:
      case Qt::Key_Down:
      case Qt::Key_PageUp:
      case Qt::Key_PageDown: break;
      // all other keys close the popup
      default: triggerCompletion = true;
    }
  }
  else
  {
    if(event->text() != QString() && event->text()[0].isPrint() && event->key() != Qt::Key_Return &&
       event->key() != Qt::Key_Enter)
      triggerCompletion = true;

    if(event->key() == Qt::Key_Escape && m_FuncTip && m_ToolTip->isVisible())
      hideFunccompleteTooltip();
  }

  if(triggerCompletion)
  {
    QString base = edit->text();

    QStringList completions;
    int oldCount = m_CompletionTipList.count();
    m_CompletionTipList.clear();

    if(base.trimmed() != QString())
    {
      PythonContext *ctx = interactiveContext;
      if(edit == ui->helpSearch)
        ctx = helpContext;
      m_CompletionTipList = ctx->completionOptions(0, base, m_InteractiveCompletionPrefix);

      for(const QPair<QString, QString> &item : m_CompletionTipList)
        completions << item.first;
    }

    if(oldCount != m_CompletionTipList.count())
      m_CurrentCompletionTip = -1;

    if(completions.isEmpty())
    {
      if(event->key() == Qt::Key_Tab)
        edit->insert(lit("\t"));
      m_InteractiveCompleter->popup()->hide();

      QString prompt = interactiveContext->tryFunctionCompletion(0, base);

      if(!prompt.isEmpty())
      {
        m_ToolTip->configureTip(this, prompt);

        QPoint p = edit->fontMetrics().boundingRect(base).bottomRight();
        p.setY(edit->geometry().height());
        p = edit->mapToGlobal(p);
        if(!m_ToolTip->isVisible())
          m_ToolTip->showTipAtPos(p);
        m_FuncTip = true;
        m_FuncTipWidget = edit;
      }
      else
      {
        hideFunccompleteTooltip();
      }

      return;
    }

    hideFunccompleteTooltip();

    m_InteractiveCompletionModel->setStringList(completions);

    QRect r = edit->rect();
    QFontMetrics fm = edit->fontMetrics();

#if(QT_VERSION < QT_VERSION_CHECK(5, 11, 0))
#define horizontalAdvance width
#endif

    int longestWidth = 0;
    for(QString &c : completions)
    {
      longestWidth = qMax(longestWidth, fm.horizontalAdvance(c));
    }

    base.resize(base.size() - m_InteractiveCompletionPrefix);

    r.setLeft(r.left() + fm.horizontalAdvance(base));
    r.setWidth(longestWidth + edit->style()->pixelMetric(QStyle::PM_ScrollBarExtent) +
               edit->style()->pixelMetric(QStyle::PM_ButtonMargin));

    m_InteractiveCompleter->setWidget(edit);
    m_InteractiveCompleter->complete(r);
    m_InteractiveCompleter->popup()->selectionModel()->setCurrentIndex(
        m_InteractiveCompletionModel->index(0), QItemSelectionModel::ClearAndSelect);

    return;
  }

  if(event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)
  {
    if(edit == ui->lineInput)
      on_execute_clicked();
    else if(edit == ui->helpSearch)
      refreshCurrentHelp();
  }

  // only line input has history
  if(edit == ui->lineInput)
  {
    bool moved = false;

    if(event->key() == Qt::Key_Down && historyidx > -1)
    {
      historyidx--;

      moved = true;
    }

    QString workingtext;

    if(event->key() == Qt::Key_Up && historyidx + 1 < history.count())
    {
      if(historyidx == -1)
        workingtext = ui->lineInput->text();

      historyidx++;

      moved = true;
    }

    if(moved)
    {
      if(historyidx == -1)
        ui->lineInput->setText(workingtext);
      else
        ui->lineInput->setText(history[historyidx]);

      ui->lineInput->deselect();
    }
  }
}

QString PythonShell::scriptHeader()
{
  return tr(R"(RenderDoc Python console, powered by python %1.
The 'pyrenderdoc' object is the current CaptureContext instance.
The 'renderdoc' and 'qrenderdoc' modules are available.
Documentation is available: https://renderdoc.org/docs/python_api/index.html)")
      .arg(PythonContext::versionString());
}

void PythonShell::appendText(QTextEdit *output, const QString &text)
{
  output->moveCursor(QTextCursor::End);
  output->insertPlainText(text);

  // scroll to the bottom
  QScrollBar *vscroll = output->verticalScrollBar();
  vscroll->setValue(vscroll->maximum());
}

void PythonShell::enableButtons(bool enable)
{
  ui->newScript->setEnabled(enable);
  ui->openScript->setEnabled(enable);
  ui->saveScript->setEnabled(enable);
  ui->runScript->setEnabled(enable);
  ui->abortRun->setEnabled(!enable);
  ui->debugAttach->setEnabled(enable && !m_DebuggerAttached && PythonContext::IsDebuggingEnabled());
  ui->debugAttach->setToolTip(QString());

  EditorWrapper *editor = curEditor();

  ui->runScript->setToolTip(QString());

  if(enable && m_DebuggerAttached)
  {
    ui->debugAttach->setToolTip(tr("Debugger is already attached"));
  }
  else if(enable && !PythonContext::IsDebuggingEnabled())
  {
    ui->debugAttach->setToolTip(
        tr("Debugging not supported - check documentation for setup instructions"));
  }

  if(editor)
  {
    if(editor->isUIExtension())
    {
      ui->runScript->setEnabled(false);
      ui->runScript->setToolTip(tr("UI Extension files can't be run"));
    }
  }

  if(editor == NULL || editor->filename().isEmpty())
  {
    ui->debugAttach->setEnabled(false);
    ui->debugAttach->setToolTip(tr("Debugger requires a script saved to disk"));
  }
}

void PythonShell::doAutocomplete(ScintillaEdit *editor)
{
  sptr_t pos = editor->currentPos();
  sptr_t line = editor->lineFromPosition(pos);
  sptr_t lineStart = editor->positionFromLine(line);
  QByteArray lineText = editor->getLine(line);
  lineText.resize(pos - lineStart);

  int oldCount = m_CompletionTipList.count();

  int prefix_len = 0;
  m_CompletionTipList =
      completionContext->completionOptions(line, QString::fromUtf8(lineText), prefix_len);

  if(m_CompletionTipList.empty())
  {
    doFunccomplete(editor);
    return;
  }

  QString completion_merged;
  for(const QPair<QString, QString> &item : m_CompletionTipList)
  {
    completion_merged += item.first;
    completion_merged += QLatin1Char(' ');
  }
  completion_merged.remove(completion_merged.count() - 1, 1);

  if(oldCount != m_CompletionTipList.count())
    m_CurrentCompletionTip = -1;

  hideFunccompleteTooltip();
  editor->autoCShow(prefix_len, completion_merged.toUtf8().data());
  // scintilla doesn't give us a callback/event when an item is highlighted, so we query in a timer
  m_CompletionTipTimer->start();
}

void PythonShell::doFunccomplete(ScintillaEdit *editor)
{
  sptr_t pos = editor->currentPos();
  sptr_t line = editor->lineFromPosition(pos);
  sptr_t lineStart = editor->positionFromLine(line);
  QByteArray lineText = editor->getLine(line);
  lineText.resize(pos - lineStart);

  QString prompt = completionContext->tryFunctionCompletion(line, QString::fromUtf8(lineText));

  if(!prompt.isEmpty())
  {
    m_ToolTip->configureTip(this, prompt);

    sptr_t tooltipPos = editor->positionFromLine(line + 1);

    QPoint p(editor->pointXFromPosition(tooltipPos),
             editor->pointYFromPosition(lineStart + lineText.size()) + editor->textHeight(line));
    p = editor->mapToGlobal(p);
    if(!m_ToolTip->isVisible())
      m_ToolTip->showTipAtPos(p);
    m_FuncTip = true;
    m_FuncTipWidget = editor;
    m_FuncTipLine = line;
  }
  else
  {
    hideFunccompleteTooltip();
  }
}

void PythonShell::hideFunccompleteTooltip()
{
  m_ToolTip->hideTip();
  m_FuncTip = false;
  m_FuncTipWidget = NULL;
  // start the syntax check timer in case this naturally disappeared
  m_SyntaxCheckTimer->start();
}

void PythonShell::updateCompletionTip()
{
  if(m_CompletionTipList.empty() || m_CurrentCompletionTip < 0 ||
     m_CurrentCompletionTip >= m_CompletionTipList.count())
  {
    m_CurrentCompletionTip = -1;
    m_CompletionTip->hideTip();
    return;
  }

  m_CompletionTip->configureTip(this, m_CompletionTipList[m_CurrentCompletionTip].second);
  {
    QPoint pos;
    if(m_InteractiveCompleter->popup()->isVisible())
    {
      pos = m_InteractiveCompleter->popup()->mapToGlobal(
          m_InteractiveCompleter->popup()->rect().topRight());
    }
    else
    {
      EditorWrapper *editor = curEditor();

      if(editor)
      {
        pos = QPoint(editor->scintilla()->autoCRectRight(), editor->scintilla()->autoCRectTop());
      }
    }

    if(pos != QPoint())
      m_CompletionTip->showTipAtPos(pos);
  }
}

PythonContext *PythonShell::newContext()
{
  PythonContext *ret = new PythonContext();

  QObject::connect(ret, &PythonContext::traceLine, this, &PythonShell::traceLine);
  QObject::connect(ret, &PythonContext::exception, this, &PythonShell::exception);
  QObject::connect(ret, &PythonContext::textOutput, this, &PythonShell::textOutput);

  return ret;
}
