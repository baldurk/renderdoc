/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2016-2019 Baldur Karlsson
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
#include <QStandardPaths>
#include "3rdparty/flowlayout/FlowLayout.h"
#include "3rdparty/toolwindowmanager/ToolWindowManager.h"
#include "Code/QRDUtils.h"
#include "Code/qprocessinfo.h"
#include "Windows/Dialogs/EnvironmentEditor.h"
#include "Windows/Dialogs/VirtualFileDialog.h"
#include "Windows/MainWindow.h"
#include "LiveCapture.h"
#include "ui_CaptureDialog.h"

#define JSON_ID "rdocCaptureSettings"
#define JSON_VER 1

static QString GetDescription(const EnvironmentModification &env)
{
  QString ret;

  if(env.mod == EnvMod::Append)
    ret = QFormatStr("Append %1 with %2 using %3").arg(env.name).arg(env.value).arg(ToQStr(env.sep));
  else if(env.mod == EnvMod::Prepend)
    ret = QFormatStr("Prepend %1 with %2 using %3").arg(env.name).arg(env.value).arg(ToQStr(env.sep));
  else
    ret = QFormatStr("Set %1 to %2").arg(env.name).arg(env.value);

  return ret;
}

Q_DECLARE_METATYPE(CaptureSettings);

void CaptureDialog::initWarning(RDLabel *warning)
{
  if(!warning)
    return;

  if(warning->devicePixelRatio() >= 2)
    warning->setText(warning->text().replace(lit(".png"), lit("@2x.png")));

  auto calcPaletteFromStyle = [warning](QEvent *) {
    QPalette pal = warning->palette();

    QColor base = pal.color(QPalette::ToolTipBase);

    pal.setColor(QPalette::Foreground, pal.color(QPalette::ToolTipText));
    pal.setColor(QPalette::Window, base);
    pal.setColor(QPalette::Base, base.darker(120));

    warning->setPalette(pal);
  };

  calcPaletteFromStyle(NULL);

  warning->setBackgroundRole(QPalette::Window);
  warning->setForegroundRole(QPalette::Foreground);

  QObject::connect(warning, &RDLabel::mouseMoved,
                   [warning](QMouseEvent *) { warning->setBackgroundRole(QPalette::Base); });
  QObject::connect(warning, &RDLabel::leave,
                   [warning]() { warning->setBackgroundRole(QPalette::Window); });
  QObject::connect(warning, &RDLabel::styleChanged, calcPaletteFromStyle);

  warning->setAutoFillBackground(true);
  warning->setMouseTracking(true);
  warning->setVisible(false);
}

QString CaptureDialog::mostRecentFilename()
{
  return configFilePath(lit("most_recent.cap"));
}

void CaptureDialog::PopulateMostRecent()
{
  QString filename = mostRecentFilename();

  if(QFile::exists(filename))
  {
    CaptureSettings settings = LoadSettingsFromDisk(filename);

    if(!settings.executable.isEmpty())
    {
      ui->loadLastCapture->setEnabled(true);

      QString exe = settings.executable;

      // if the executable isn't a path, display the full name
      bool fullName = (exe.indexOf(QLatin1Char('/')) < 0 && exe.indexOf(QLatin1Char('\\')) < 0);

      // if it's not a windows path and only contains one '/', it's an android package so also
      // display the full name
      fullName |= (exe.indexOf(QLatin1Char('\\')) < 0 && exe.count(QLatin1Char('/')) == 1);

      if(fullName)
        ui->loadLastCapture->setText(tr("Load Last Settings - %1").arg(exe));
      else
        ui->loadLastCapture->setText(tr("Load Last Settings - %1").arg(QFileInfo(exe).fileName()));
      return;
    }
  }

  ui->loadLastCapture->setEnabled(false);
  ui->loadLastCapture->setText(tr("Load Last Settings"));
}

CaptureDialog::CaptureDialog(ICaptureContext &ctx, OnCaptureMethod captureCallback,
                             OnInjectMethod injectCallback, MainWindow *main, QWidget *parent)
    : QFrame(parent), ui(new Ui::CaptureDialog), m_Ctx(ctx), m_Main(main)
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
  // use case-insensitive filtering
  proxy->setFilterCaseSensitivity(Qt::CaseInsensitive);

  ui->processList->setModel(proxy);
  ui->processList->setAlternatingRowColors(true);

  // sort by PID by default
  ui->processList->sortByColumn(1, Qt::AscendingOrder);

  // Set up warning for host layer config
  initWarning(ui->vulkanLayerWarn);
  ui->vulkanLayerWarn->setVisible(RENDERDOC_NeedVulkanLayerRegistration(NULL));
  QObject::connect(ui->vulkanLayerWarn, &RDLabel::clicked, this,
                   &CaptureDialog::vulkanLayerWarn_mouseClick);

  // Set up scanning for Android apps
  initWarning(ui->androidScan);

  // Set up warning for Android apps
  initWarning(ui->androidWarn);
  QObject::connect(ui->androidWarn, &RDLabel::clicked, this, &CaptureDialog::androidWarn_mouseClick);

  m_AndroidFlags = AndroidFlags::NoFlags;

  m_CaptureCallback = captureCallback;
  m_InjectCallback = injectCallback;

  SetSettings(CaptureSettings());

  UpdateGlobalHook();

  PopulateMostRecent();
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
    this->setWindowTitle(lit("Launch Application"));
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

  VulkanLayerRegistrationInfo info;

  RENDERDOC_NeedVulkanLayerRegistration(&info);

  const bool hasOtherJSON = bool(info.flags & VulkanLayerFlags::OtherInstallsRegistered);
  const bool thisRegistered = bool(info.flags & VulkanLayerFlags::ThisInstallRegistered);
  const bool needElevation = bool(info.flags & VulkanLayerFlags::NeedElevation);
  const bool couldElevate = bool(info.flags & VulkanLayerFlags::CouldElevate);
  const bool registerAll = bool(info.flags & VulkanLayerFlags::RegisterAll);
  const bool updateAllowed = bool(info.flags & VulkanLayerFlags::UpdateAllowed);

  if(info.flags & VulkanLayerFlags::Unfixable)
  {
    QString msg =
        tr("There is an unfixable problem with your vulkan layer configuration. Please consult the "
           "RenderDoc documentation, or package/distribution documentation on linux\n\n");

    for(const rdcstr &j : info.otherJSONs)
      msg += j + lit("\n");

    RDDialog::critical(this, tr("Unfixable vulkan layer configuration"), msg);
    return;
  }

  QString msg =
      tr("Vulkan capture happens through the API's layer mechanism. RenderDoc has detected that ");

  if(hasOtherJSON)
  {
    if(info.otherJSONs.size() > 1)
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
    for(const rdcstr &j : info.otherJSONs)
      msg += (updateAllowed ? tr("Unregister/update: %1\n") : tr("Unregister: %1\n")).arg(j);

    msg += lit("\n");
  }

  if(!thisRegistered)
  {
    if(registerAll)
    {
      for(const rdcstr &j : info.myJSONs)
        msg += (updateAllowed ? tr("Register/update: %1\n") : tr("Register: %1\n")).arg(j);
    }
    else
    {
      msg += updateAllowed ? tr("Register one of:\n") : tr("Register/update one of:\n");
      for(const rdcstr &j : info.myJSONs)
        msg += tr("  -- %1\n").arg(j);
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
          tr("Do you want to elevate permissions to install the layer at a system level?\n\n"
             "If you click 'No', the layer will be installed at a per-user level."),
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
// linux sometimes can't run GUI apps as root, so we have to run renderdoccmd. Check that it's
// installed, error if not, then invoke it.
#if defined(Q_OS_LINUX)
        QDir binDir = QFileInfo(qApp->applicationFilePath()).absoluteDir();

        QString cmd = lit("renderdoccmd");

        if(binDir.exists(cmd))
        {
          // it's next to our exe, run that
          cmd = binDir.absoluteFilePath(cmd);
        }
        else
        {
          QString inPath = QStandardPaths::findExecutable(cmd);

          if(inPath.isEmpty())
          {
            RDDialog::critical(
                this, tr("Can't locate renderdoccmd"),
                tr("On linux we must run renderdoccmd as root to register the layer, because "
                   "graphical applications like qrenderdoc may fail to launch.\n\n"
                   "renderdoccmd could not be located either next to this qrenderdoc executable or "
                   "in PATH."));
            return;
          }

          // it's in the path, we can continue
        }

        RunProcessAsAdmin(cmd, QStringList() << lit("vulkanregister") << lit("--system"), this,
                          true, [this]() { ui->vulkanLayerWarn->setVisible(false); });
#else
        RunProcessAsAdmin(qApp->applicationFilePath(),
                          QStringList() << lit("--install_vulkan_layer") << lit("root"), this,
                          false, [this]() { ui->vulkanLayerWarn->setVisible(false); });
#endif
        return;
      }
      else
      {
        QProcess *process = new QProcess;
        process->start(qApp->applicationFilePath(), QStringList() << lit("--install_vulkan_layer")
                                                                  << lit("user"));
        process->waitForFinished(300);

        // when the process exits, delete
        QObject::connect(process, OverloadedSlot<int>::of(&QProcess::finished),
                         [process](int exitCode) { process->deleteLater(); });
      }
    }

    ui->vulkanLayerWarn->setVisible(RENDERDOC_NeedVulkanLayerRegistration(NULL));
  }
}

void CaptureDialog::CheckAndroidSetup(QString &filename)
{
  ui->androidScan->setVisible(true);
  ui->androidWarn->setVisible(false);

  LambdaThread *scan = new LambdaThread([this, filename]() {

    rdcstr host = m_Ctx.Replay().CurrentRemote()->hostname;
    RENDERDOC_CheckAndroidPackage(host.c_str(), filename.toUtf8().data(), &m_AndroidFlags);

    const bool debuggable = bool(m_AndroidFlags & AndroidFlags::Debuggable);
    const bool hasroot = bool(m_AndroidFlags & AndroidFlags::RootAccess);

    if(!debuggable && !hasroot)
    {
      // Check failed - set the warning visible
      GUIInvoke::call(this, [this]() {
        ui->androidScan->setVisible(false);
        ui->androidWarn->setVisible(true);
      });
    }
    else
    {
      // Check passed, either app is debuggable or we have root - no warnings needed
      GUIInvoke::call(this, [this]() {
        ui->androidScan->setVisible(false);
        ui->androidWarn->setVisible(false);
      });
    }
  });

  scan->start();
  scan->deleteLater();
}

void CaptureDialog::androidWarn_mouseClick()
{
  QString exe = ui->exePath->text();

  const RemoteHost *remote = m_Ctx.Replay().CurrentRemote();

  if(!remote)
  {
    RDDialog::critical(this, tr("Android server disconnected"),
                       tr("You've been disconnected from the android server.\n\n"
                          "Please reconnect before attempting to fix package problems."));
    return;
  }

  rdcstr host = remote->hostname;

  QString caption = tr("Application is not debuggable");

  QString msg = tr(R"(In order to debug on Android, the package must be <b>debuggable</b>.
<br><br>
On UE4 you must disable <em>for distribution</em>, on Unity enable <em>development mode</em>.
<br><br>
RenderDoc can try to add the flag for you, which will involve completely reinstalling your package
as well as re-signing it with a debug key. This method is prone to error and is
<b>not recommended</b>. It is instead advised to configure your app to be debuggable at build time.
<br><br>
Would you like RenderDoc to try patching your package?
)");

  QMessageBox::StandardButton prompt = RDDialog::question(this, caption, msg, RDDialog::YesNoCancel);

  if(prompt == QMessageBox::Yes)
  {
    float progress = 0.0f;
    bool patchSucceeded = false;

    // call into APK pull, patch, install routine, then continue
    LambdaThread *patch = new LambdaThread([this, host, exe, &patchSucceeded, &progress]() {
      AndroidFlags result = RENDERDOC_MakeDebuggablePackage(host.c_str(), exe.toUtf8().data(),
                                                            [&progress](float p) { progress = p; });

      if(result & AndroidFlags::Debuggable)
      {
        // Sucess!
        patchSucceeded = true;

        RDDialog::information(this, tr("Patch succeeded!"),
                              tr("The patch process succeeded and the package is ready to debug"));
      }
      else
      {
        QString failmsg = tr("Something has gone wrong and the patching process failed.<br><br>");

        if(result == AndroidFlags::MissingTools)
        {
          failmsg +=
              tr("Tools required for the process were not found. Try configuring the path to your "
                 "android SDK or java JDK in the settings dialog.");
        }
        else if(result == AndroidFlags::ManifestPatchFailure)
        {
          failmsg +=
              tr("The package manifest could not be patched. This is not solveable, you will have "
                 "to rebuild the package with the debuggable flag.");
        }
        else if(result == AndroidFlags::RepackagingAPKFailure)
        {
          failmsg += tr("The package was patched but could not be repackaged and installed.");
        }

        RDDialog::critical(this, tr("Failed to patch package"), failmsg);
      }
    });

    patch->start();
    // wait a few ms before popping up a progress bar
    patch->wait(500);
    if(patch->isRunning())
    {
      ShowProgressDialog(this, tr("Patching %1, please wait...").arg(exe),
                         [patch]() { return !patch->isRunning(); },
                         [&progress]() { return progress; });
    }
    patch->deleteLater();

    if(patchSucceeded)
      ui->androidWarn->setVisible(false);
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
  QString initExe;

  QFileInfo f(ui->exePath->text());
  QDir dir = f.dir();
  if(f.isAbsolute() && dir.exists())
  {
    initDir = dir.absolutePath();
    initExe = f.fileName();
  }
  else if(m_Ctx.Replay().CurrentRemote())
  {
    initDir = m_Ctx.Replay().CurrentRemote()->lastCapturePath;
  }
  else if(!m_Ctx.Config().LastCapturePath.isEmpty())
  {
    initDir = m_Ctx.Config().LastCapturePath;
    if(!m_Ctx.Config().LastCaptureExe.isEmpty())
      initExe = m_Ctx.Config().LastCaptureExe;
  }

  QString filename;

  if(m_Ctx.Replay().CurrentRemote())
  {
    VirtualFileDialog vfd(m_Ctx, initDir, this);
    RDDialog::show(&vfd);
    filename = vfd.chosenPath();
  }
  else
  {
    filename = RDDialog::getExecutableFileName(this, tr("Choose executable"), initDir, initExe);
  }

  if(!filename.isEmpty())
  {
    SetExecutableFilename(filename);

    if(m_Ctx.Replay().CurrentRemote() && m_Ctx.Replay().CurrentRemote()->IsADB())
    {
      CheckAndroidSetup(filename);
    }
  }
}

void CaptureDialog::on_workDirBrowse_clicked()
{
  QString initDir = ui->workDirPath->text();

  if(initDir.isEmpty())
  {
    if(m_Ctx.Replay().CurrentRemote())
    {
      initDir = m_Ctx.Replay().CurrentRemote()->lastCapturePath;
    }
    else if(!QDir(initDir).exists())
    {
      QDir dir = QFileInfo(ui->exePath->text()).dir();
      if(dir.exists())
        initDir = dir.absolutePath();
      else if(!m_Ctx.Config().LastCapturePath.isEmpty())
        initDir = m_Ctx.Config().LastCapturePath;
      else
        initDir = QString();
    }
  }

  QString dir;

  if(m_Ctx.Replay().CurrentRemote())
  {
    VirtualFileDialog vfd(m_Ctx, initDir, this);
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

    QString capturefile = m_Ctx.TempCaptureFilename(QFileInfo(exe).baseName());

    bool success = RENDERDOC_StartGlobalHook(exe.toUtf8().data(), capturefile.toUtf8().data(),
                                             Settings().options);

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
      m_Main->PopulateRecentCaptureSettings();
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

void CaptureDialog::on_loadLastCapture_clicked()
{
  LoadSettings(mostRecentFilename());
}

void CaptureDialog::on_launch_clicked()
{
  TriggerCapture();
}

void CaptureDialog::on_processList_activated(const QModelIndex &index)
{
  TriggerCapture();
}

void CaptureDialog::SetSettings(CaptureSettings settings)
{
  SetInjectMode(settings.inject);

  ui->exePath->setText(settings.executable);
  ui->workDirPath->setText(settings.workingDir);
  ui->cmdline->setText(settings.commandLine);

  SetEnvironmentModifications(settings.environment);

  ui->AllowFullscreen->setChecked(settings.options.allowFullscreen);
  ui->AllowVSync->setChecked(settings.options.allowVSync);
  ui->HookIntoChildren->setChecked(settings.options.hookIntoChildren);
  ui->CaptureCallstacks->setChecked(settings.options.captureCallstacks);
  ui->CaptureCallstacksOnlyDraws->setChecked(settings.options.captureCallstacksOnlyDraws);
  ui->APIValidation->setChecked(settings.options.apiValidation);
  ui->RefAllResources->setChecked(settings.options.refAllResources);
  ui->CaptureAllCmdLists->setChecked(settings.options.captureAllCmdLists);
  ui->DelayForDebugger->setValue(settings.options.delayForDebugger);
  ui->VerifyBufferAccess->setChecked(settings.options.verifyBufferAccess);
  ui->AutoStart->setChecked(settings.autoStart);

  // force flush this state
  on_CaptureCallstacks_toggled(ui->CaptureCallstacks->isChecked());

  if(settings.numQueuedFrames > 0)
  {
    ui->queuedFrame->setValue(settings.queuedFrameCap);
    ui->numFrames->setValue(settings.numQueuedFrames);
    ui->queueFrameCap->setChecked(true);
  }
  else
  {
    ui->queuedFrame->setValue(0);
    ui->numFrames->setValue(0);
    ui->queueFrameCap->setChecked(false);
  }

  if(settings.autoStart)
  {
    TriggerCapture();
  }
}

CaptureSettings CaptureDialog::Settings()
{
  CaptureSettings ret;

  ret.inject = IsInjectMode();

  ret.autoStart = ui->AutoStart->isChecked();

  ret.executable = ui->exePath->text();
  ret.workingDir = ui->workDirPath->text();
  ret.commandLine = ui->cmdline->text();

  ret.environment = m_EnvModifications;

  ret.options.allowFullscreen = ui->AllowFullscreen->isChecked();
  ret.options.allowVSync = ui->AllowVSync->isChecked();
  ret.options.hookIntoChildren = ui->HookIntoChildren->isChecked();
  ret.options.captureCallstacks = ui->CaptureCallstacks->isChecked();
  ret.options.captureCallstacksOnlyDraws = ui->CaptureCallstacksOnlyDraws->isChecked();
  ret.options.apiValidation = ui->APIValidation->isChecked();
  ret.options.refAllResources = ui->RefAllResources->isChecked();
  ret.options.captureAllCmdLists = ui->CaptureAllCmdLists->isChecked();
  ret.options.delayForDebugger = (uint32_t)ui->DelayForDebugger->value();
  ret.options.verifyBufferAccess = ui->VerifyBufferAccess->isChecked();

  if(ui->queueFrameCap->isChecked())
  {
    ret.queuedFrameCap = (uint32_t)ui->queuedFrame->value();
    ret.numQueuedFrames = (uint32_t)ui->numFrames->value();
  }

  return ret;
}

void CaptureDialog::SaveSettings(const rdcstr &filename)
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

  for(int i = 0; i < processes.count(); i++)
  {
    if(processes[i].pid() == qApp->applicationPid())
    {
      processes.removeAt(i);
      break;
    }
  }

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

void CaptureDialog::SetExecutableFilename(const rdcstr &filename)
{
  QString fn = filename;

  if(!m_Ctx.Replay().CurrentRemote())
    fn = QDir::toNativeSeparators(QFileInfo(fn).absoluteFilePath());

  ui->exePath->setText(fn);

  if(m_Ctx.Replay().CurrentRemote())
  {
    // remove the filename itself before setting the last capture path. Try /, then \ as path
    // separator. If no separators are found, there is no path to set.
    int idx = fn.lastIndexOf(QLatin1Char('/'));
    if(idx >= 0)
    {
      fn = fn.mid(0, idx);
    }
    else
    {
      idx = fn.lastIndexOf(QLatin1Char('\\'));

      if(idx >= 0)
        fn = fn.mid(0, idx);
      else
        fn = QString();
    }

    m_Ctx.Replay().CurrentRemote()->lastCapturePath = fn;
  }
  else
  {
    m_Ctx.Config().LastCapturePath = QFileInfo(fn).absolutePath();
    m_Ctx.Config().LastCaptureExe = QFileInfo(fn).fileName();
  }

  m_Ctx.Config().Save();
}

void CaptureDialog::SetWorkingDirectory(const rdcstr &dir)
{
  ui->workDirPath->setText(dir);
}

void CaptureDialog::SetCommandLine(const rdcstr &cmd)
{
  ui->cmdline->setText(cmd);
}

void CaptureDialog::LoadSettings(const rdcstr &filename)
{
  SetSettings(LoadSettingsFromDisk(filename));
}

CaptureSettings CaptureDialog::LoadSettingsFromDisk(const rdcstr &filename)
{
  QFile f(filename);
  if(f.open(QIODevice::ReadOnly | QIODevice::Text))
  {
    QVariantMap values;

    bool success = LoadFromJSON(values, f, JSON_ID, JSON_VER);

    if(success)
    {
      return CaptureSettings(values[lit("settings")]);
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

  return CaptureSettings();
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

void CaptureDialog::UpdateRemoteHost()
{
  const RemoteHost *host = m_Ctx.Replay().CurrentRemote();

  if(host && host->IsADB())
    ui->cmdLineLabel->setText(tr("Intent Arguments"));
  else
    ui->cmdLineLabel->setText(tr("Command-line Arguments"));
}

void CaptureDialog::SetEnvironmentModifications(const rdcarray<EnvironmentModification> &modifications)
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

      m_InjectCallback(
          PID, Settings().environment, name, Settings().options, [this](LiveCapture *live) {
            if(ui->queueFrameCap->isChecked())
              live->QueueCapture((int)ui->queuedFrame->value(), (int)ui->numFrames->value());
          });
    }
    else
    {
      RDDialog::critical(this, tr("No process selected"),
                         tr("No process is selected to inject from the list above."));
    }
  }
  else
  {
    QString exe = ui->exePath->text().trimmed();

    if(exe.isEmpty())
    {
      RDDialog::critical(this, tr("No executable selected"),
                         tr("No program selected to launch, click browse next to 'Executable Path' "
                            "above to select the program to launch."));
      return;
    }

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

    SaveSettings(mostRecentFilename());

    PopulateMostRecent();

    m_CaptureCallback(exe, workingDir, cmdLine, Settings().environment, Settings().options,
                      [this](LiveCapture *live) {
                        if(ui->queueFrameCap->isChecked())
                          live->QueueCapture((int)ui->queuedFrame->value(),
                                             (int)ui->numFrames->value());
                      });
  }
}
