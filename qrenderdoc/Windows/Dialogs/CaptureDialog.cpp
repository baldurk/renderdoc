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
#include <QMouseEvent>
#include <QSortFilterProxyModel>
#include <QStandardItemModel>
#include "3rdparty/flowlayout/FlowLayout.h"
#include "3rdparty/toolwindowmanager/ToolWindowManager.h"
#include "Code/QRDUtils.h"
#include "Code/qprocessinfo.h"
#include "Windows/Dialogs/EnvironmentEditor.h"
#include "Windows/Dialogs/VirtualFileDialog.h"
#include "LiveCapture.h"
#include "ui_CaptureDialog.h"

#define JSON_ID "rdocCaptureSettings"
#define JSON_VER 1

static QString GetDescription(const EnvironmentModification &env)
{
  QString ret;

  if(env.mod == EnvMod::Append)
    ret = QFormatStr("Append %1 with %2 using %3")
              .arg(ToQStr(env.name))
              .arg(ToQStr(env.value))
              .arg(ToQStr(env.sep));
  else if(env.mod == EnvMod::Prepend)
    ret = QFormatStr("Prepend %1 with %2 using %3")
              .arg(ToQStr(env.name))
              .arg(ToQStr(env.value))
              .arg(ToQStr(env.sep));
  else
    ret = QFormatStr("Set %1 to %2").arg(ToQStr(env.name)).arg(ToQStr(env.value));

  return ret;
}

Q_DECLARE_METATYPE(CaptureSettings);

CaptureDialog::CaptureDialog(ICaptureContext &ctx, OnCaptureMethod captureCallback,
                             OnInjectMethod injectCallback, QWidget *parent)
    : QFrame(parent), ui(new Ui::CaptureDialog), m_Ctx(ctx)
{
  ui->setupUi(this);

  ui->exePath->setFont(Formatter::PreferredFont());
  ui->workDirPath->setFont(Formatter::PreferredFont());
  ui->cmdline->setFont(Formatter::PreferredFont());
  ui->processList->setFont(Formatter::PreferredFont());

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

  ui->vulkanLayerWarn->setVisible(RENDERDOC_NeedVulkanLayerRegistration(NULL, NULL, NULL));

  QObject::connect(ui->vulkanLayerWarn, &RDLabel::clicked, this,
                   &CaptureDialog::vulkanLayerWarn_mouseClick);

  QPalette pal = ui->vulkanLayerWarn->palette();

  QColor base = pal.color(QPalette::ToolTipBase);

  pal.setColor(QPalette::Foreground, pal.color(QPalette::ToolTipText));

  pal.setColor(QPalette::Window, base);
  pal.setColor(QPalette::Base, base.darker(120));

  ui->vulkanLayerWarn->setBackgroundRole(QPalette::Window);

  QObject::connect(ui->vulkanLayerWarn, &RDLabel::mouseMoved, [this](QMouseEvent *) {
    ui->vulkanLayerWarn->setBackgroundRole(QPalette::Base);
  });
  QObject::connect(ui->vulkanLayerWarn, &RDLabel::leave,
                   [this]() { ui->vulkanLayerWarn->setBackgroundRole(QPalette::Window); });

  ui->vulkanLayerWarn->setPalette(pal);
  ui->vulkanLayerWarn->setAutoFillBackground(true);

  ui->vulkanLayerWarn->setMouseTracking(true);

  m_CaptureCallback = captureCallback;
  m_InjectCallback = injectCallback;

  SetSettings(CaptureSettings());

  UpdateGlobalHook();
}

CaptureDialog::~CaptureDialog()
{
  m_Ctx.BuiltinWindowClosed(this);

  if(ui->toggleGlobal->isChecked())
  {
    ui->toggleGlobal->setChecked(false);

    UpdateGlobalHook();
  }

  delete ui;
}

void CaptureDialog::SetInjectMode(bool inject)
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

    ui->launch->setText(lit("Inject"));
    this->setWindowTitle(lit("Inject into Process"));
  }
  else
  {
    ui->injectGroup->setVisible(false);
    ui->exeGroup->setVisible(true);
    ui->topVerticalSpacer->spacerItem()->changeSize(0, 0, QSizePolicy::Minimum,
                                                    QSizePolicy::Expanding);
    ui->verticalLayout->invalidate();

    ui->globalGroup->setVisible(m_Ctx.Config().AllowGlobalHook);

    ui->launch->setText(lit("Launch"));
    this->setWindowTitle(lit("Capture Executable"));
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

void CaptureDialog::on_exePath_textChanged(const QString &text)
{
  QString exe = text;

  // This is likely due to someone pasting a full path copied using copy path. Removing the quotes
  // is safe in any case
  if(exe.startsWith(QLatin1Char('"')) && exe.endsWith(QLatin1Char('"')) && exe.count() > 2)
  {
    exe = exe.mid(1, exe.count() - 2);
    ui->exePath->setText(exe);
    return;
  }

  QFileInfo f(exe);
  QDir dir = f.dir();
  bool valid = dir.makeAbsolute();

  if(valid && f.isAbsolute())
  {
    QString path = dir.absolutePath();

    if(!m_Ctx.Replay().CurrentRemote())
      path = QDir::toNativeSeparators(path);

    // match the path separators from the path
    if(exe.count(QLatin1Char('/')) > exe.count(QLatin1Char('\\')))
      path = path.replace(QLatin1Char('\\'), QLatin1Char('/'));
    else
      path = path.replace(QLatin1Char('/'), QLatin1Char('\\'));

    ui->workDirPath->setPlaceholderText(path);
  }
  else if(exe.isEmpty())
  {
    ui->workDirPath->setPlaceholderText(QString());
  }

  UpdateGlobalHook();
}

void CaptureDialog::vulkanLayerWarn_mouseClick()
{
  QString caption = tr("Configure Vulkan layer settings in registry?");

  VulkanLayerFlags flags = VulkanLayerFlags::NoFlags;
  rdctype::array<rdctype::str> myJSONs;
  rdctype::array<rdctype::str> otherJSONs;

  RENDERDOC_NeedVulkanLayerRegistration(&flags, &myJSONs, &otherJSONs);

  const bool hasOtherJSON = bool(flags & VulkanLayerFlags::OtherInstallsRegistered);
  const bool thisRegistered = bool(flags & VulkanLayerFlags::ThisInstallRegistered);
  const bool needElevation = bool(flags & VulkanLayerFlags::NeedElevation);
  const bool couldElevate = bool(flags & VulkanLayerFlags::CouldElevate);
  const bool registerAll = bool(flags & VulkanLayerFlags::RegisterAll);
  const bool updateAllowed = bool(flags & VulkanLayerFlags::UpdateAllowed);

  if(flags & VulkanLayerFlags::Unfixable)
  {
    QString msg =
        tr("There is an unfixable problem with your vulkan layer configuration. Please consult the "
           "RenderDoc documentation, or package/distribution documentation on linux\n\n");

    for(const rdctype::str &j : otherJSONs)
      msg += ToQStr(j) + lit("\n");

    RDDialog::critical(this, tr("Unfixable vulkan layer configuration"), msg);
    return;
  }

  QString msg =
      tr("Vulkan capture happens through the API's layer mechanism. RenderDoc has detected that ");

  if(hasOtherJSON)
  {
    if(otherJSONs.count > 1)
      msg +=
          tr("there are other RenderDoc builds registered already. They must be disabled so that "
             "capture can happen without nasty clashes.");
    else
      msg +=
          tr("there is another RenderDoc build registered already. It must be disabled so that "
             "capture can happen without nasty clashes.");

    if(!thisRegistered)
      msg += tr(" Also ");
  }

  if(!thisRegistered)
  {
    msg +=
        tr("the layer for this installation is not yet registered. This could be due to an "
           "upgrade from a version that didn't support Vulkan, or if this version is just a loose "
           "unzip/dev build.");
  }

  msg += tr("\n\nWould you like to proceed with the following changes?\n\n");

  if(hasOtherJSON)
  {
    for(const rdctype::str &j : otherJSONs)
      msg += (updateAllowed ? tr("Unregister/update: %1\n") : tr("Unregister: %1\n")).arg(ToQStr(j));

    msg += lit("\n");
  }

  if(!thisRegistered)
  {
    if(registerAll)
    {
      for(const rdctype::str &j : myJSONs)
        msg += (updateAllowed ? tr("Register/update: %1\n") : tr("Register: %1\n")).arg(ToQStr(j));
    }
    else
    {
      msg += updateAllowed ? tr("Register one of:\n") : tr("Register/update one of:\n");
      for(const rdctype::str &j : myJSONs)
        msg += tr("  -- %1\n").arg(ToQStr(j));
    }

    msg += lit("\n");
  }

  msg += tr("This is a one-off change, it won't be needed again unless the installation moves.");

  QMessageBox::StandardButton install = RDDialog::question(this, caption, msg, RDDialog::YesNoCancel);

  if(install == QMessageBox::Yes)
  {
    bool run = false;
    bool admin = false;

    // if we need to elevate, just try it.
    if(needElevation)
    {
      admin = run = true;
    }
    // if we could elevate, ask the user
    else if(couldElevate)
    {
      QMessageBox::StandardButton elevate = RDDialog::question(
          this, tr("System layer install"),
          tr("Do you want to elevate permissions to install the layer at a system level?"),
          RDDialog::YesNoCancel);

      if(elevate == QMessageBox::Yes)
        admin = true;
      else if(elevate == QMessageBox::No)
        admin = false;

      run = (elevate != QMessageBox::Cancel);
    }
    // otherwise run non-elevated
    else
    {
      run = true;
    }

    if(run)
    {
      if(admin)
      {
        RunProcessAsAdmin(qApp->applicationFilePath(),
                          QStringList() << lit("--install_vulkan_layer") << lit("root"),
                          [this]() { ui->vulkanLayerWarn->setVisible(false); });
        return;
      }
      else
      {
        QProcess process;
        process.start(qApp->applicationFilePath(), QStringList() << lit("--install_vulkan_layer")
                                                                 << lit("user"));
        process.waitForFinished(300);
      }
    }

    ui->vulkanLayerWarn->setVisible(RENDERDOC_NeedVulkanLayerRegistration(NULL, NULL, NULL));
  }
}

void CaptureDialog::on_processRefesh_clicked()
{
  fillProcessList();
}

bool CaptureDialog::checkAllowClose()
{
  if(RENDERDOC_IsGlobalHookActive())
  {
    RDDialog::critical(this, tr("Global hook active"),
                       tr("Cannot close this window while global hook is active."));
    return false;
  }

  return true;
}

void CaptureDialog::on_exePathBrowse_clicked()
{
  QString initDir;
  QString file;

  QFileInfo f(ui->exePath->text());
  QDir dir = f.dir();
  if(f.isAbsolute() && dir.exists())
  {
    initDir = dir.absolutePath();
  }
  else if(!m_Ctx.Config().LastCapturePath.isEmpty())
  {
    initDir = m_Ctx.Config().LastCapturePath;
    if(!m_Ctx.Config().LastCaptureExe.isEmpty())
      file = m_Ctx.Config().LastCaptureExe;
  }

  QString filename;

  if(m_Ctx.Replay().CurrentRemote())
  {
    VirtualFileDialog vfd(m_Ctx, this);
    RDDialog::show(&vfd);
    filename = vfd.chosenPath();
  }
  else
  {
    filename = RDDialog::getExecutableFileName(this, tr("Choose executable"), initDir);
  }

  if(!filename.isEmpty())
    SetExecutableFilename(filename);
}

void CaptureDialog::on_workDirBrowse_clicked()
{
  QString initDir;

  if(QDir(ui->workDirPath->text()).exists())
  {
    initDir = ui->workDirPath->text();
  }
  else
  {
    QDir dir = QFileInfo(ui->exePath->text()).dir();
    if(dir.exists())
      initDir = dir.absolutePath();
    else if(!m_Ctx.Config().LastCapturePath.isEmpty())
      initDir = m_Ctx.Config().LastCapturePath;
  }

  QString dir;

  if(m_Ctx.Replay().CurrentRemote())
  {
    VirtualFileDialog vfd(m_Ctx, this);
    vfd.setDirBrowse();
    RDDialog::show(&vfd);
    dir = vfd.chosenPath();
  }
  else
  {
    dir = RDDialog::getExistingDirectory(this, tr("Choose working directory"), initDir);
  }

  if(!dir.isEmpty())
    ui->workDirPath->setText(dir);
}

void CaptureDialog::on_envVarEdit_clicked()
{
  EnvironmentEditor envEditor(this);

  for(const EnvironmentModification &mod : m_EnvModifications)
    envEditor.addModification(mod, true);

  int res = RDDialog::show(&envEditor);

  if(res)
    SetEnvironmentModifications(envEditor.modifications());
}

void CaptureDialog::on_toggleGlobal_clicked()
{
  if(!ui->toggleGlobal->isEnabled())
    return;

  ui->toggleGlobal->setEnabled(false);

  QList<QWidget *> enableDisableWidgets = {ui->exePath,       ui->exePathBrowse, ui->workDirPath,
                                           ui->workDirBrowse, ui->cmdline,       ui->launch,
                                           ui->saveSettings,  ui->loadSettings};

  for(QWidget *o : ui->optionsGroup->findChildren<QWidget *>(QString(), Qt::FindDirectChildrenOnly))
    if(o)
      enableDisableWidgets << o;

  for(QWidget *o : ui->actionGroup->findChildren<QWidget *>(QString(), Qt::FindDirectChildrenOnly))
    if(o)
      enableDisableWidgets << o;

  if(ui->toggleGlobal->isChecked())
  {
    if(!IsRunningAsAdmin())
    {
      QMessageBox::StandardButton res = RDDialog::question(
          this, tr("Restart as admin?"),
          tr("RenderDoc needs to restart with administrator privileges. Restart?"),
          RDDialog::YesNoCancel);

      if(res == QMessageBox::Yes)
      {
        QString capfile = QDir::temp().absoluteFilePath(lit("global.cap"));

        bool wasChecked = ui->AutoStart->isChecked();
        ui->AutoStart->setChecked(false);

        SaveSettings(capfile);

        ui->AutoStart->setChecked(wasChecked);

        // save the config here explicitly
        m_Ctx.Config().Save();

        bool success = RunProcessAsAdmin(qApp->applicationFilePath(), QStringList() << capfile);

        if(success)
        {
          // close the config so that when we're shutting down we don't conflict with new process
          // loading
          m_Ctx.Config().Close();
          m_Ctx.GetMainWindow()->Widget()->close();
          return;
        }

        // don't restart if it failed for some reason (e.g. user clicked no to the elevation prompt)
        ui->toggleGlobal->setChecked(false);
        ui->toggleGlobal->setEnabled(true);
        return;
      }
      else
      {
        ui->toggleGlobal->setChecked(false);
        ui->toggleGlobal->setEnabled(true);
        return;
      }
    }

    setEnabledMultiple(enableDisableWidgets, false);

    ui->toggleGlobal->setText(tr("Disable Global Hook"));

    if(RENDERDOC_IsGlobalHookActive())
      RENDERDOC_StopGlobalHook();

    QString exe = ui->exePath->text();

    QString logfile = m_Ctx.TempLogFilename(QFileInfo(exe).baseName());

    bool success =
        RENDERDOC_StartGlobalHook(exe.toUtf8().data(), logfile.toUtf8().data(), Settings().Options);

    if(!success)
    {
      // tidy up and exit
      RDDialog::critical(
          this, tr("Couldn't start global hook"),
          tr("Aborting. Couldn't start global hook. Check diagnostic log in help menu for more "
             "information"));

      setEnabledMultiple(enableDisableWidgets, true);

      // won't recurse because it's not enabled yet
      ui->toggleGlobal->setChecked(false);
      ui->toggleGlobal->setText(tr("Enable Global Hook"));
      ui->toggleGlobal->setEnabled(true);
      return;
    }
  }
  else
  {
    // not checked
    if(RENDERDOC_IsGlobalHookActive())
      RENDERDOC_StopGlobalHook();

    setEnabledMultiple(enableDisableWidgets, true);

    ui->toggleGlobal->setText(tr("Enable Global Hook"));
  }

  ui->toggleGlobal->setEnabled(true);

  UpdateGlobalHook();
}

void CaptureDialog::on_saveSettings_clicked()
{
  QString filename = RDDialog::getSaveFileName(this, tr("Save Settings As"), QString(),
                                               tr("Capture settings (*.cap)"));

  if(!filename.isEmpty())
  {
    QDir dirinfo = QFileInfo(filename).dir();
    if(dirinfo.exists())
    {
      SaveSettings(filename);
      AddRecentFile(m_Ctx.Config().RecentCaptureSettings, filename, 10);
    }
  }
}

void CaptureDialog::on_loadSettings_clicked()
{
  QString filename = RDDialog::getOpenFileName(this, tr("Open Settings"), QString(),
                                               tr("Capture settings (*.cap)"));

  if(!filename.isEmpty() && QFileInfo::exists(filename))
  {
    LoadSettings(filename);
    AddRecentFile(m_Ctx.Config().RecentCaptureSettings, filename, 10);
  }
}

void CaptureDialog::on_launch_clicked()
{
  TriggerCapture();
}

void CaptureDialog::on_close_clicked()
{
  ToolWindowManager::closeToolWindow(this);
}

void CaptureDialog::SetSettings(CaptureSettings settings)
{
  SetInjectMode(settings.Inject);

  ui->exePath->setText(settings.Executable);
  ui->workDirPath->setText(settings.WorkingDir);
  ui->cmdline->setText(settings.CmdLine);

  SetEnvironmentModifications(settings.Environment);

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
    TriggerCapture();
  }
}

CaptureSettings CaptureDialog::Settings()
{
  CaptureSettings ret;

  ret.Inject = IsInjectMode();

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

void CaptureDialog::SaveSettings(QString filename)
{
  QFile f(filename);
  if(f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
  {
    QVariantMap values;
    values[lit("settings")] = (QVariant)Settings();
    SaveToJSON(values, f, JSON_ID, JSON_VER);
  }
  else
  {
    RDDialog::critical(this, tr("Error saving config"),
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

void CaptureDialog::SetExecutableFilename(const QString &filename)
{
  QString fn = filename;

  if(!m_Ctx.Replay().CurrentRemote())
    fn = QDir::toNativeSeparators(QFileInfo(fn).absoluteFilePath());

  ui->exePath->setText(fn);

  if(!m_Ctx.Replay().CurrentRemote())
  {
    m_Ctx.Config().LastCapturePath = QFileInfo(fn).absolutePath();
    m_Ctx.Config().LastCaptureExe = QFileInfo(fn).completeBaseName();
  }
}

void CaptureDialog::SetWorkingDirectory(const QString &dir)
{
  ui->workDirPath->setText(dir);
}

void CaptureDialog::SetCommandLine(const QString &cmd)
{
  ui->cmdline->setText(cmd);
}

void CaptureDialog::LoadSettings(QString filename)
{
  QFile f(filename);
  if(f.open(QIODevice::ReadOnly | QIODevice::Text))
  {
    QVariantMap values;

    bool success = LoadFromJSON(values, f, JSON_ID, JSON_VER);

    if(success)
    {
      CaptureSettings settings(values[lit("settings")]);
      SetSettings(settings);
    }
    else
    {
      RDDialog::critical(this, tr("Error loading config"),
                         tr("Couldn't interpret settings in %1.").arg(filename));
    }
  }
  else
  {
    RDDialog::critical(this, tr("Error loading config"), tr("Couldn't open path %1.").arg(filename));
  }
}

void CaptureDialog::UpdateGlobalHook()
{
  ui->globalGroup->setVisible(!IsInjectMode() && m_Ctx.Config().AllowGlobalHook &&
                              RENDERDOC_CanGlobalHook());

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

void CaptureDialog::SetEnvironmentModifications(const QList<EnvironmentModification> &modifications)
{
  m_EnvModifications = modifications;

  QString envModText;

  for(const EnvironmentModification &mod : modifications)
  {
    if(!envModText.isEmpty())
      envModText += lit(", ");

    envModText += GetDescription(mod);
  }

  ui->envVar->setText(envModText);
}

void CaptureDialog::TriggerCapture()
{
  if(IsInjectMode())
  {
    QModelIndexList sel = ui->processList->selectionModel()->selectedRows();
    if(sel.size() == 1)
    {
      QModelIndex item = sel[0];

      QSortFilterProxyModel *model = (QSortFilterProxyModel *)ui->processList->model();

      item = model->mapToSource(item);

      QString name = m_ProcessModel->data(m_ProcessModel->index(item.row(), 0)).toString();
      uint32_t PID = m_ProcessModel->data(m_ProcessModel->index(item.row(), 1)).toUInt();

      m_InjectCallback(PID, Settings().Environment, name, Settings().Options,
                       [this](LiveCapture *live) {
                         if(ui->queueFrameCap->isChecked())
                           live->QueueCapture((int)ui->queuedFrame->value());
                       });
    }
  }
  else
  {
    QString exe = ui->exePath->text();

    // for non-remote captures, check the executable locally
    if(!m_Ctx.Replay().CurrentRemote())
    {
      if(!QFileInfo::exists(exe))
      {
        RDDialog::critical(this, tr("Invalid executable"),
                           tr("Invalid application executable: %1").arg(exe));
        return;
      }
    }

    QString workingDir;

    // for non-remote captures, check the directory locally
    if(m_Ctx.Replay().CurrentRemote())
    {
      workingDir = ui->workDirPath->text();
    }
    else
    {
      if(QDir(ui->workDirPath->text()).exists())
        workingDir = ui->workDirPath->text();
    }

    QString cmdLine = ui->cmdline->text();

    m_CaptureCallback(exe, workingDir, cmdLine, Settings().Environment, Settings().Options,
                      [this](LiveCapture *live) {
                        if(ui->queueFrameCap->isChecked())
                          live->QueueCapture((int)ui->queuedFrame->value());
                      });
  }
}
