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
  ui->vulkanLayerWarn->setVisible(RENDERDOC_NeedVulkanLayerRegistration(NULL, NULL, NULL));
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

  VulkanLayerFlags flags = VulkanLayerFlags::NoFlags;
  rdcarray<rdcstr> myJSONs;
  rdcarray<rdcstr> otherJSONs;

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

    for(const rdcstr &j : otherJSONs)
      msg += j + lit("\n");

    RDDialog::critical(this, tr("Unfixable vulkan layer configuration"), msg);
    return;
  }

  QString msg =
      tr("Vulkan capture happens through the API's layer mechanism. RenderDoc has detected that ");

  if(hasOtherJSON)
  {
    if(otherJSONs.size() > 1)
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
    for(const rdcstr &j : otherJSONs)
      msg += (updateAllowed ? tr("Unregister/update: %1\n") : tr("Unregister: %1\n")).arg(j);

    msg += lit("\n");
  }

  if(!thisRegistered)
  {
    if(registerAll)
    {
      for(const rdcstr &j : myJSONs)
        msg += (updateAllowed ? tr("Register/update: %1\n") : tr("Register: %1\n")).arg(j);
    }
    else
    {
      msg += updateAllowed ? tr("Register one of:\n") : tr("Register/update one of:\n");
      for(const rdcstr &j : myJSONs)
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

void CaptureDialog::CheckAndroidSetup(QString &filename)
{
  ui->androidScan->setVisible(true);
  ui->androidWarn->setVisible(false);

  LambdaThread *scan = new LambdaThread([this, filename]() {

    QByteArray hostnameBytes = m_Ctx.Replay().CurrentRemote()->Hostname.toUtf8();
    RENDERDOC_CheckAndroidPackage(hostnameBytes.data(), filename.toUtf8().data(), &m_AndroidFlags);

    const bool missingLibrary = bool(m_AndroidFlags & AndroidFlags::MissingLibrary);

    if(missingLibrary)
    {
      // Check failed - set the warning visible
      GUIInvoke::call([this]() {
        ui->androidScan->setVisible(false);
        ui->androidWarn->setVisible(true);
      });
    }
    else
    {
      // Check passed - no warnings needed
      GUIInvoke::call([this]() {
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

  QString caption = tr("Missing RenderDoc requirements");

  QString msg = tr("In order to debug on Android, the following problems must be fixed:<br><br>");

  bool missingLibrary = bool(m_AndroidFlags & AndroidFlags::MissingLibrary);
  bool rootAccess = bool(m_AndroidFlags & AndroidFlags::RootAccess);

  if(missingLibrary)
  {
    msg +=
        tr("<b>Missing library</b><br>"
           "The RenderDoc library must be present in the "
           "installed application.<br><br>");
  }

  // Track whether we tried to push layer directly, to influence text
  bool triedPush = false;

  // Track whether to continue with push suggestion dialogue, in case user clicked Cancel
  bool suggestPatch = true;

  if(rootAccess)
  {
    // Check whether user has requested automatic pushing
    bool autoPushConfig = m_Ctx.Config().Android_AutoPushLayerToApp;

    // Separately, track whether the persistent checkBox is selected
    bool autoPushCheckBox = autoPushConfig;

    QMessageBox::StandardButton prompt = QMessageBox::No;

    // Only display initial prompt if user has not chosen to push automatically
    if(!autoPushConfig)
    {
      QString rootmsg = msg;
      rootmsg +=
          tr("Your device appears to have <b>root access</b>. If you are only targeting Vulkan, "
             "RenderDoc can try to push the layer directly to your application.<br><br>"
             "Would you like RenderDoc to push the layer?<br>");

      QString checkMsg(tr("Automatically push the layer on rooted devices"));
      QCheckBox *cb = new QCheckBox(checkMsg, this);
      cb->setChecked(autoPushCheckBox);
      prompt = RDDialog::questionChecked(this, caption, rootmsg, cb, autoPushCheckBox,
                                         RDDialog::YesNoCancel);
    }

    if(autoPushConfig || prompt == QMessageBox::Yes)
    {
      bool pushSucceeded = false;
      triedPush = true;

      // Only update the autoPush setting if Yes was clicked
      if(autoPushCheckBox != m_Ctx.Config().Android_AutoPushLayerToApp)
      {
        m_Ctx.Config().Android_AutoPushLayerToApp = autoPushCheckBox;
        m_Ctx.Config().Save();
      }

      // Call into layer push routine, then continue
      LambdaThread *push = new LambdaThread([this, exe, &pushSucceeded]() {
        QByteArray hostnameBytes = m_Ctx.Replay().CurrentRemote()->Hostname.toUtf8();
        if(RENDERDOC_PushLayerToInstalledAndroidApp(hostnameBytes.data(), exe.toUtf8().data()))
        {
          // Sucess!
          pushSucceeded = true;

          RDDialog::information(
              this, tr("Push succeeded!"),
              tr("The push attempt succeeded and<br>%1 now contains the RenderDoc layer").arg(exe));
        }
      });

      push->start();
      if(push->isRunning())
      {
        ShowProgressDialog(this, tr("Pushing layer to %1, please wait...").arg(exe),
                           [push]() { return !push->isRunning(); });
      }
      push->deleteLater();

      if(pushSucceeded)
      {
        // We should be good from here, no futher prompts
        suggestPatch = false;
        ui->androidWarn->setVisible(false);
      }
    }
    else if(prompt == QMessageBox::Cancel)
    {
      // Cancel skips any other fix prompts
      suggestPatch = false;
    }
  }

  if(suggestPatch)
  {
    if(triedPush)
      msg.insert(0, tr("The push attempt failed, so other methods must be used to fix the missing "
                       "layer.<br><br>"));

    msg +=
        tr("To fix this, you should repackage the APK following guidelines on the "
           "<a href='http://github.com/baldurk/renderdoc/wiki/Android-Support'>"
           "RenderDoc Wiki</a><br><br>"
           "If you are only targeting Vulkan, RenderDoc can try to <b>add the layer for you</b>, "
           "which requires pulling the APK, patching it, uninstalling the original, and "
           "installing the modified version with a debug key. "
           "This works for many debuggable applications, but not all, especially those that "
           "check their integrity before launching.<br><br>"
           "Your system will need several tools installed and available to RenderDoc. "
           "Any missing tools will be noted in the log. Follow the steps "
           "<a href='http://github.com/baldurk/renderdoc/wiki/Android-Support'>here"
           "</a> to get them.<br><br>"
           "Would you like RenderDoc to try patching your APK?");

    QMessageBox::StandardButton prompt =
        RDDialog::question(this, caption, msg, RDDialog::YesNoCancel);

    if(prompt == QMessageBox::Yes)
    {
      float progress = 0.0f;
      bool patchSucceeded = false;

      // call into APK pull, patch, install routine, then continue
      LambdaThread *patch = new LambdaThread([this, exe, &patchSucceeded, &progress]() {
        QByteArray hostnameBytes = m_Ctx.Replay().CurrentRemote()->Hostname.toUtf8();
        if(RENDERDOC_AddLayerToAndroidPackage(hostnameBytes.data(), exe.toUtf8().data(), &progress))
        {
          // Sucess!
          patchSucceeded = true;

          RDDialog::information(
              this, tr("Patch succeeded!"),
              tr("The patch process succeeded and<br>%1 now contains the RenderDoc layer").arg(exe));
        }
        else
        {
          RDDialog::critical(this, tr("Failed to patch APK"),
                             tr("Something has gone wrong and APK patching failed "
                                "for:<br>%1<br>Check diagnostic log in Help "
                                "menu for more details.")
                                 .arg(exe));
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
  {
    SetExecutableFilename(filename);

    if(m_Ctx.Replay().CurrentRemote() && m_Ctx.Replay().CurrentRemote()->IsHostADB())
    {
      CheckAndroidSetup(filename);
    }
  }
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

    QString capturefile = m_Ctx.TempCaptureFilename(QFileInfo(exe).baseName());

    bool success = RENDERDOC_StartGlobalHook(exe.toUtf8().data(), capturefile.toUtf8().data(),
                                             Settings().Options);

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

  // force flush this state
  on_CaptureCallstacks_toggled(ui->CaptureCallstacks->isChecked());

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
