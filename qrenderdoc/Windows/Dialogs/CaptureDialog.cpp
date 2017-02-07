/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2016-2017 Baldur Karlsson
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

#include "CaptureDialog.h"
#include <QSortFilterProxyModel>
#include <QStandardItemModel>
#include "Code/QRDUtils.h"
#include "Code/qprocessinfo.h"
#include "FlowLayout.h"
#include "LiveCapture.h"
#include "ToolWindowManager.h"
#include "ui_CaptureDialog.h"

#define JSON_ID "rdocCaptureSettings"
#define JSON_VER 1

Q_DECLARE_METATYPE(CaptureSettings);

CaptureSettings::CaptureSettings()
{
  Inject = false;
  AutoStart = false;
  RENDERDOC_GetDefaultCaptureOptions(&Options);
}

QVariantMap CaptureSettings::toJSON() const
{
  QVariantMap ret;

  ret["AutoStart"] = AutoStart;

  ret["Executable"] = Executable;
  ret["WorkingDir"] = WorkingDir;
  ret["CmdLine"] = CmdLine;

  QVariantList env;
  for(int i = 0; i < Environment.size(); i++)
    env.push_back(Environment[i].toJSON());
  ret["Environment"] = env;

  QVariantMap opts;
  opts["AllowVSync"] = Options.AllowVSync;
  opts["AllowFullscreen"] = Options.AllowFullscreen;
  opts["APIValidation"] = Options.APIValidation;
  opts["CaptureCallstacks"] = Options.CaptureCallstacks;
  opts["CaptureCallstacksOnlyDraws"] = Options.CaptureCallstacksOnlyDraws;
  opts["DelayForDebugger"] = Options.DelayForDebugger;
  opts["VerifyMapWrites"] = Options.VerifyMapWrites;
  opts["HookIntoChildren"] = Options.HookIntoChildren;
  opts["RefAllResources"] = Options.RefAllResources;
  opts["SaveAllInitials"] = Options.SaveAllInitials;
  opts["CaptureAllCmdLists"] = Options.CaptureAllCmdLists;
  opts["DebugOutputMute"] = Options.DebugOutputMute;
  ret["Options"] = opts;

  return ret;
}

void CaptureSettings::fromJSON(const QVariantMap &data)
{
  AutoStart = data["AutoStart"].toBool();

  Executable = data["Executable"].toString();
  WorkingDir = data["WorkingDir"].toString();
  CmdLine = data["CmdLine"].toString();

  QVariantList env = data["Environment"].toList();
  for(int i = 0; i < env.size(); i++)
  {
    EnvironmentModification e;
    e.fromJSON(env[i].value<QVariantMap>());
    Environment.push_back(e);
  }

  QVariantMap opts = data["Options"].toMap();

  Options.AllowVSync = opts["AllowVSync"].toBool();
  Options.AllowFullscreen = opts["AllowFullscreen"].toBool();
  Options.APIValidation = opts["APIValidation"].toBool();
  Options.CaptureCallstacks = opts["CaptureCallstacks"].toBool();
  Options.CaptureCallstacksOnlyDraws = opts["CaptureCallstacksOnlyDraws"].toBool();
  Options.DelayForDebugger = opts["DelayForDebugger"].toUInt();
  Options.VerifyMapWrites = opts["VerifyMapWrites"].toBool();
  Options.HookIntoChildren = opts["HookIntoChildren"].toBool();
  Options.RefAllResources = opts["RefAllResources"].toBool();
  Options.SaveAllInitials = opts["SaveAllInitials"].toBool();
  Options.CaptureAllCmdLists = opts["CaptureAllCmdLists"].toBool();
  Options.DebugOutputMute = opts["DebugOutputMute"].toBool();
}

CaptureDialog::CaptureDialog(CaptureContext *ctx, OnCaptureMethod captureCallback,
                             OnInjectMethod injectCallback, QWidget *parent)
    : QFrame(parent), ui(new Ui::CaptureDialog), m_Ctx(ctx)
{
  ui->setupUi(this);

  // setup FlowLayout for options group
  {
    QLayout *oldLayout = ui->optionsGroup->layout();

    QObjectList options = ui->optionsGroup->children();
    options.removeOne((QObject *)oldLayout);

    delete oldLayout;

    FlowLayout *optionsFlow = new FlowLayout(ui->optionsGroup, -1, 3, 3);

    optionsFlow->setFixedGrid(true);

    for(QObject *o : options)
      optionsFlow->addWidget(qobject_cast<QWidget *>(o));

    ui->optionsGroup->setLayout(optionsFlow);
  }

  ui->envVar->setEnabled(false);

  m_ProcessModel = new QStandardItemModel(0, 3, this);

  m_ProcessModel->setHeaderData(0, Qt::Horizontal, tr("Name"));
  m_ProcessModel->setHeaderData(1, Qt::Horizontal, tr("PID"));
  m_ProcessModel->setHeaderData(2, Qt::Horizontal, tr("Window Title"));

  QSortFilterProxyModel *proxy = new QSortFilterProxyModel(this);

  proxy->setSourceModel(m_ProcessModel);
  // filter on all columns
  proxy->setFilterKeyColumn(-1);
  // allow updating the underlying model
  proxy->setDynamicSortFilter(true);

  ui->processList->setModel(proxy);
  ui->processList->setAlternatingRowColors(true);

  // sort by PID by default
  ui->processList->sortByColumn(1, Qt::AscendingOrder);

  // TODO Vulkan Layer
  ui->vulkanLayerWarn->setVisible(false);

  m_CaptureCallback = captureCallback;
  m_InjectCallback = injectCallback;

  setSettings(CaptureSettings());

  updateGlobalHook();
}

CaptureDialog::~CaptureDialog()
{
  m_Ctx->windowClosed(this);

  if(ui->toggleGlobal->isChecked())
  {
    ui->toggleGlobal->setChecked(false);

    updateGlobalHook();
  }

  delete ui;
}

void CaptureDialog::setInjectMode(bool inject)
{
  m_Inject = inject;

  if(inject)
  {
    ui->injectGroup->setVisible(true);
    ui->exeGroup->setVisible(false);
    ui->topVerticalSpacer->spacerItem()->changeSize(0, 0, QSizePolicy::Minimum, QSizePolicy::Minimum);
    ui->verticalLayout->invalidate();

    ui->globalGroup->setVisible(false);

    fillProcessList();

    ui->launch->setText("Inject");
    this->setWindowTitle("Inject into Process");
  }
  else
  {
    ui->injectGroup->setVisible(false);
    ui->exeGroup->setVisible(true);
    ui->topVerticalSpacer->spacerItem()->changeSize(0, 0, QSizePolicy::Minimum,
                                                    QSizePolicy::Expanding);
    ui->verticalLayout->invalidate();

    ui->globalGroup->setVisible(m_Ctx->Config.AllowGlobalHook);

    ui->launch->setText("Launch");
    this->setWindowTitle("Capture Executable");
  }
}

void CaptureDialog::on_CaptureCallstacks_toggled(bool checked)
{
  if(ui->CaptureCallstacks->isChecked())
  {
    ui->CaptureCallstacksOnlyDraws->setEnabled(true);
  }
  else
  {
    ui->CaptureCallstacksOnlyDraws->setChecked(false);
    ui->CaptureCallstacksOnlyDraws->setEnabled(false);
  }
}

void CaptureDialog::on_processFilter_textChanged(const QString &filter)
{
  QSortFilterProxyModel *model = (QSortFilterProxyModel *)ui->processList->model();

  if(model == NULL)
    return;

  model->setFilterFixedString(filter);
}

void CaptureDialog::on_exePath_textChanged(const QString &exe)
{
  QFileInfo f(exe);
  QDir dir = f.dir();
  bool valid = dir.makeAbsolute();

  if(valid && f.isAbsolute())
  {
    QString path = QDir::toNativeSeparators(dir.absolutePath());

    // match the path separators from the path
    if(exe.count(QChar('/')) > exe.count(QChar('\\')))
      path = path.replace('\\', '/');
    else
      path = path.replace('/', '\\');

    ui->workDirPath->setPlaceholderText(path);
  }
  else if(exe == "")
  {
    ui->workDirPath->setPlaceholderText("");
  }

  updateGlobalHook();
}

void CaptureDialog::on_vulkanLayerWarn_clicked()
{
  // TODO Vulkan Layer
}

void CaptureDialog::on_processRefesh_clicked()
{
  fillProcessList();
}

void CaptureDialog::on_exePathBrowse_clicked()
{
  QString initDir = "";
  QString file = "";

  QFileInfo f(ui->exePath->text());
  QDir dir = f.dir();
  if(ui->exePath->text() != "" && f.isAbsolute() && dir.exists())
  {
    initDir = dir.absolutePath();
  }
  else if(m_Ctx->Config.LastCapturePath != "")
  {
    initDir = m_Ctx->Config.LastCapturePath;
    if(m_Ctx->Config.LastCaptureExe != "")
      file = m_Ctx->Config.LastCaptureExe;
  }

  QAbstractProxyModel *proxy = NULL;
  QFileDialog::Options options;

  if(m_Ctx->Renderer()->remote())
  {
    // proxy = new RemoteFileProxy(m_Ctx->Renderer());
    options = QFileDialog::DontUseNativeDialog;
  }

  QString filename =
      RDDialog::getExecutableFileName(this, tr("Choose executable"), initDir, options, proxy);

  if(filename != "")
    setExecutableFilename(filename);
}

void CaptureDialog::on_workDirBrowse_clicked()
{
  QString initDir = "";

  if(QDir(ui->workDirPath->text()).exists())
  {
    initDir = ui->workDirPath->text();
  }
  else
  {
    QDir dir = QFileInfo(ui->exePath->text()).dir();
    if(dir.exists())
      initDir = dir.absolutePath();
    else if(m_Ctx->Config.LastCapturePath != "")
      initDir = m_Ctx->Config.LastCapturePath;
  }

  QAbstractProxyModel *proxy = NULL;
  QFileDialog::Options options = QFileDialog::ShowDirsOnly;

  if(m_Ctx->Renderer()->remote())
  {
    // proxy = new RemoteFileProxy(m_Ctx->Renderer());
    options |= QFileDialog::DontUseNativeDialog;
  }

  QString dir =
      RDDialog::getExistingDirectory(this, "Choose working directory", initDir, options, proxy);

  if(dir != "")
    ui->workDirPath->setText(dir);
}

void CaptureDialog::on_envVarEdit_clicked()
{
  // TODO Env Editor
}

void CaptureDialog::on_toggleGlobal_clicked()
{
  if(ui->toggleGlobal->isEnabled())
  {
    ui->toggleGlobal->setChecked(!ui->toggleGlobal->isChecked());

    updateGlobalHook();
  }
}

void CaptureDialog::on_saveSettings_clicked()
{
  QString filename = RDDialog::getSaveFileName(this, tr("Save Settings As"), QString(),
                                               "Capture settings (*.cap)");

  if(filename != "")
  {
    QDir dirinfo = QFileInfo(filename).dir();
    if(dirinfo.exists())
    {
      saveSettings(filename);
      PersistantConfig::AddRecentFile(m_Ctx->Config.RecentCaptureSettings, filename, 10);
    }
  }
}

void CaptureDialog::on_loadSettings_clicked()
{
  QString filename =
      RDDialog::getOpenFileName(this, tr("Open Settings"), QString(), "Capture settings (*.cap)");

  if(filename != "" && QFileInfo::exists(filename))
  {
    loadSettings(filename);
    PersistantConfig::AddRecentFile(m_Ctx->Config.RecentCaptureSettings, filename, 10);
  }
}

void CaptureDialog::on_launch_clicked()
{
  triggerCapture();
}

void CaptureDialog::on_close_clicked()
{
  ToolWindowManager::closeToolWindow(this);
}

void CaptureDialog::setSettings(CaptureSettings settings)
{
  setInjectMode(settings.Inject);

  ui->exePath->setText(settings.Executable);
  ui->workDirPath->setText(settings.WorkingDir);
  ui->cmdline->setText(settings.CmdLine);

  setEnvironmentModifications(settings.Environment);

  ui->AllowFullscreen->setChecked(settings.Options.AllowFullscreen);
  ui->AllowVSync->setChecked(settings.Options.AllowVSync);
  ui->HookIntoChildren->setChecked(settings.Options.HookIntoChildren);
  ui->CaptureCallstacks->setChecked(settings.Options.CaptureCallstacks);
  ui->CaptureCallstacksOnlyDraws->setChecked(settings.Options.CaptureCallstacksOnlyDraws);
  ui->APIValidation->setChecked(settings.Options.APIValidation);
  ui->RefAllResources->setChecked(settings.Options.RefAllResources);
  ui->SaveAllInitials->setChecked(settings.Options.SaveAllInitials);
  ui->DelayForDebugger->setValue(settings.Options.DelayForDebugger);
  ui->VerifyMapWrites->setChecked(settings.Options.VerifyMapWrites);
  ui->AutoStart->setChecked(settings.AutoStart);

  if(settings.AutoStart)
  {
    triggerCapture();
  }
}

CaptureSettings CaptureDialog::settings()
{
  CaptureSettings ret;

  ret.Inject = injectMode();

  ret.AutoStart = ui->AutoStart->isChecked();

  ret.Executable = ui->exePath->text();
  ret.WorkingDir = ui->workDirPath->text();
  ret.CmdLine = ui->cmdline->text();

  ret.Environment = m_EnvModifications;

  ret.Options.AllowFullscreen = ui->AllowFullscreen->isChecked();
  ret.Options.AllowVSync = ui->AllowVSync->isChecked();
  ret.Options.HookIntoChildren = ui->HookIntoChildren->isChecked();
  ret.Options.CaptureCallstacks = ui->CaptureCallstacks->isChecked();
  ret.Options.CaptureCallstacksOnlyDraws = ui->CaptureCallstacksOnlyDraws->isChecked();
  ret.Options.APIValidation = ui->APIValidation->isChecked();
  ret.Options.RefAllResources = ui->RefAllResources->isChecked();
  ret.Options.SaveAllInitials = ui->SaveAllInitials->isChecked();
  ret.Options.CaptureAllCmdLists = ui->CaptureAllCmdLists->isChecked();
  ret.Options.DelayForDebugger = (uint32_t)ui->DelayForDebugger->value();
  ret.Options.VerifyMapWrites = ui->VerifyMapWrites->isChecked();

  return ret;
}

void CaptureDialog::saveSettings(QString filename)
{
  QFile f(filename);
  if(f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
  {
    QVariantMap values;
    values["settings"] = settings().toJSON();
    SaveToJSON(values, f, JSON_ID, JSON_VER);
  }
  else
  {
    RDDialog::critical(this, "Error saving config",
                       tr("Couldn't open path %1 for write.").arg(filename));
  }
}

void CaptureDialog::fillProcessList()
{
  m_ProcessModel->removeRows(0, m_ProcessModel->rowCount());

  QProcessList processes = QProcessInfo::enumerate();

  // no way of listing processes in Qt, fill with dummy data
  m_ProcessModel->insertRows(0, processes.size());

  int n = 0;
  for(const QProcessInfo &process : processes)
  {
    m_ProcessModel->setData(m_ProcessModel->index(n, 0), process.name());
    m_ProcessModel->setData(m_ProcessModel->index(n, 1), process.pid());
    m_ProcessModel->setData(m_ProcessModel->index(n, 2), process.windowTitle());
    n++;
  }
}

void CaptureDialog::setExecutableFilename(QString filename)
{
  filename = QDir::toNativeSeparators(QFileInfo(filename).absoluteFilePath());
  ui->exePath->setText(filename);

  m_Ctx->Config.LastCapturePath = QFileInfo(filename).absolutePath();
  m_Ctx->Config.LastCaptureExe = QFileInfo(filename).completeBaseName();
}

void CaptureDialog::loadSettings(QString filename)
{
  QFile f(filename);
  if(f.open(QIODevice::ReadOnly | QIODevice::Text))
  {
    QVariantMap values;

    bool success = LoadFromJSON(values, f, JSON_ID, JSON_VER);

    if(success)
    {
      CaptureSettings settings;
      settings.fromJSON(values["settings"].value<QVariantMap>());
      setSettings(settings);
    }
    else
    {
      RDDialog::critical(this, "Error loading config",
                         tr("Couldn't interpret settings in %1.").arg(filename));
    }
  }
  else
  {
    RDDialog::critical(this, "Error loading config", tr("Couldn't open path %1.").arg(filename));
  }
}

void CaptureDialog::updateGlobalHook()
{
  ui->globalGroup->setVisible(!injectMode() && m_Ctx->Config.AllowGlobalHook);

  if(ui->exePath->text().length() >= 4)
  {
    ui->toggleGlobal->setEnabled(true);
    QString text = tr("Global hooking is risky!\nBe sure you know what you're doing.");

    if(ui->toggleGlobal->isChecked())
      text += tr("\nEmergency restore @ %TEMP%\\RenderDoc_RestoreGlobalHook.reg");

    ui->globalLabel->setText(text);
  }
  else
  {
    ui->toggleGlobal->setEnabled(false);
    ui->globalLabel->setText(tr("Global hooking requires an executable path, or filename"));
  }
}

void CaptureDialog::setEnvironmentModifications(const QList<EnvironmentModification> &modifications)
{
  m_EnvModifications = modifications;

  QString envModText = "";

  for(const EnvironmentModification &mod : modifications)
  {
    if(envModText != "")
      envModText += ", ";

    envModText += mod.GetDescription();
  }

  ui->envVar->setText(envModText);
}

void CaptureDialog::triggerCapture()
{
  if(injectMode())
  {
    QModelIndexList sel = ui->processList->selectionModel()->selectedRows();
    if(sel.size() == 1)
    {
      QModelIndex item = sel[0];

      QSortFilterProxyModel *model = (QSortFilterProxyModel *)ui->processList->model();

      item = model->mapToSource(item);

      QString name = m_ProcessModel->data(m_ProcessModel->index(item.row(), 0)).toString();
      uint32_t PID = m_ProcessModel->data(m_ProcessModel->index(item.row(), 1)).toUInt();

      LiveCapture *live = m_InjectCallback(PID, settings().Environment, name, settings().Options);

      if(ui->queueFrameCap->isChecked() && live != NULL)
        live->QueueCapture((int)ui->queuedFrame->value());
    }
  }
  else
  {
    QString exe = ui->exePath->text();

    // for non-remote captures, check the executable locally
    if(!m_Ctx->Renderer()->remote())
    {
      if(!QFileInfo::exists(exe))
      {
        RDDialog::critical(this, tr("Invalid executable"),
                           tr("Invalid application executable: %1").arg(exe));
        return;
      }
    }

    QString workingDir = "";

    // for non-remote captures, check the directory locally
    if(m_Ctx->Renderer()->remote())
    {
      workingDir = ui->workDirPath->text();
    }
    else
    {
      if(QDir(ui->workDirPath->text()).exists())
        workingDir = ui->workDirPath->text();
    }

    QString cmdLine = ui->cmdline->text();

    LiveCapture *live =
        m_CaptureCallback(exe, workingDir, cmdLine, settings().Environment, settings().Options);

    if(ui->queueFrameCap->isChecked() && live != NULL)
      live->QueueCapture((int)ui->queuedFrame->value());
  }
}
