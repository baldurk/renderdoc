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

#include "SettingsDialog.h"
#include "Code/CaptureContext.h"
#include "Code/Interface/QRDInterface.h"
#include "Code/QRDUtils.h"
#include "Windows/Dialogs/OrderedListEditor.h"
#include "CaptureDialog.h"
#include "ui_SettingsDialog.h"

SettingsDialog::SettingsDialog(ICaptureContext &ctx, QWidget *parent)
    : QDialog(parent), ui(new Ui::SettingsDialog), m_Ctx(ctx)
{
  ui->setupUi(this);

  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

  ui->tabWidget->tabBar()->setVisible(false);

  for(int i = 0; i < ui->tabWidget->count(); i++)
    ui->pages->addItem(ui->tabWidget->tabText(i));

  m_Init = true;

  for(int i = 0; i < (int)TimeUnit::Count; i++)
  {
    ui->EventBrowser_TimeUnit->addItem(UnitSuffix((TimeUnit)i));
  }

  ui->pages->clearSelection();
  ui->pages->setItemSelected(ui->pages->item(0), true);
  ui->tabWidget->setCurrentIndex(0);

  ui->saveDirectory->setText(m_Ctx.Config().DefaultCaptureSaveDirectory);
  ui->tempDirectory->setText(m_Ctx.Config().TemporaryCaptureDirectory);

  if(!m_Ctx.Config().SPIRVDisassemblers.isEmpty())
  {
    ui->externalDisassemblerArgs->setText(m_Ctx.Config().SPIRVDisassemblers[0].args);
    ui->externalDisassemblePath->setText(m_Ctx.Config().SPIRVDisassemblers[0].executable);
  }
  ui->Android_AdbExecutablePath->setText(m_Ctx.Config().Android_AdbExecutablePath);
  ui->Android_MaxConnectTimeout->setValue(m_Ctx.Config().Android_MaxConnectTimeout);

  ui->TextureViewer_ResetRange->setChecked(m_Ctx.Config().TextureViewer_ResetRange);
  ui->TextureViewer_PerTexSettings->setChecked(m_Ctx.Config().TextureViewer_PerTexSettings);
  ui->ShaderViewer_FriendlyNaming->setChecked(m_Ctx.Config().ShaderViewer_FriendlyNaming);
  ui->CheckUpdate_AllowChecks->setChecked(m_Ctx.Config().CheckUpdate_AllowChecks);
  ui->Font_PreferMonospaced->setChecked(m_Ctx.Config().Font_PreferMonospaced);

  ui->AlwaysReplayLocally->setChecked(m_Ctx.Config().AlwaysReplayLocally);

  ui->AllowGlobalHook->setChecked(m_Ctx.Config().AllowGlobalHook);

  ui->EventBrowser_TimeUnit->setCurrentIndex((int)m_Ctx.Config().EventBrowser_TimeUnit);
  ui->EventBrowser_AddFake->setChecked(m_Ctx.Config().EventBrowser_AddFake);
  ui->EventBrowser_HideEmpty->setChecked(m_Ctx.Config().EventBrowser_HideEmpty);
  ui->EventBrowser_HideAPICalls->setChecked(m_Ctx.Config().EventBrowser_HideAPICalls);
  ui->EventBrowser_ApplyColors->setChecked(m_Ctx.Config().EventBrowser_ApplyColors);
  ui->EventBrowser_ColorEventRow->setChecked(m_Ctx.Config().EventBrowser_ColorEventRow);

  // disable sub-checkbox
  ui->EventBrowser_ColorEventRow->setEnabled(ui->EventBrowser_ApplyColors->isChecked());

  ui->Formatter_MinFigures->setValue(m_Ctx.Config().Formatter_MinFigures);
  ui->Formatter_MaxFigures->setValue(m_Ctx.Config().Formatter_MaxFigures);
  ui->Formatter_NegExp->setValue(m_Ctx.Config().Formatter_NegExp);
  ui->Formatter_PosExp->setValue(m_Ctx.Config().Formatter_PosExp);

  if(!RENDERDOC_CanGlobalHook())
  {
    ui->AllowGlobalHook->setEnabled(false);

    QString disabledTooltip = tr("Global hooking is not supported on this platform");
    ui->AllowGlobalHook->setToolTip(disabledTooltip);
    ui->globalHookLabel->setToolTip(disabledTooltip);
  }

  m_Init = false;

  QObject::connect(ui->Formatter_MinFigures, OverloadedSlot<int>::of(&QSpinBox::valueChanged), this,
                   &SettingsDialog::formatter_valueChanged);
  QObject::connect(ui->Formatter_MaxFigures, OverloadedSlot<int>::of(&QSpinBox::valueChanged), this,
                   &SettingsDialog::formatter_valueChanged);
  QObject::connect(ui->Formatter_NegExp, OverloadedSlot<int>::of(&QSpinBox::valueChanged), this,
                   &SettingsDialog::formatter_valueChanged);
  QObject::connect(ui->Formatter_PosExp, OverloadedSlot<int>::of(&QSpinBox::valueChanged), this,
                   &SettingsDialog::formatter_valueChanged);
}

SettingsDialog::~SettingsDialog()
{
  delete ui;
}

void SettingsDialog::on_pages_itemSelectionChanged()
{
  QList<QListWidgetItem *> sel = ui->pages->selectedItems();

  if(sel.empty())
  {
    ui->pages->setItemSelected(ui->pages->item(ui->tabWidget->currentIndex()), true);
  }
  else
  {
    ui->tabWidget->setCurrentIndex(ui->pages->row(sel[0]));
  }
}

void SettingsDialog::on_okButton_accepted()
{
  setResult(1);
  accept();
}

// general
void SettingsDialog::formatter_valueChanged(int val)
{
  m_Ctx.Config().Formatter_MinFigures = ui->Formatter_MinFigures->value();
  m_Ctx.Config().Formatter_MaxFigures = ui->Formatter_MaxFigures->value();
  m_Ctx.Config().Formatter_NegExp = ui->Formatter_NegExp->value();
  m_Ctx.Config().Formatter_PosExp = ui->Formatter_PosExp->value();

  m_Ctx.Config().SetupFormatting();

  m_Ctx.Config().Save();
}

void SettingsDialog::on_tempDirectory_textEdited(const QString &dir)
{
  if(QDir(dir).exists())
    m_Ctx.Config().TemporaryCaptureDirectory = dir;
  else
    m_Ctx.Config().TemporaryCaptureDirectory = QString();

  m_Ctx.Config().Save();
}

void SettingsDialog::on_saveDirectory_textEdited(const QString &dir)
{
  if(QDir(dir).exists() || dir.isEmpty())
    m_Ctx.Config().DefaultCaptureSaveDirectory = dir;

  m_Ctx.Config().Save();
}

void SettingsDialog::on_browseSaveCaptureDirectory_clicked()
{
  QString dir =
      RDDialog::getExistingDirectory(this, tr("Choose default directory for saving captures"),
                                     m_Ctx.Config().DefaultCaptureSaveDirectory);

  if(!dir.isEmpty())
    m_Ctx.Config().DefaultCaptureSaveDirectory = dir;

  m_Ctx.Config().Save();
}

void SettingsDialog::on_AllowGlobalHook_toggled(bool checked)
{
  m_Ctx.Config().AllowGlobalHook = ui->AllowGlobalHook->isChecked();

  m_Ctx.Config().Save();

  if(m_Ctx.HasCaptureDialog())
    m_Ctx.GetCaptureDialog()->UpdateGlobalHook();
}

void SettingsDialog::on_CheckUpdate_AllowChecks_toggled(bool checked)
{
  m_Ctx.Config().CheckUpdate_AllowChecks = ui->CheckUpdate_AllowChecks->isChecked();

  m_Ctx.Config().Save();
}

void SettingsDialog::on_Font_PreferMonospaced_toggled(bool checked)
{
  m_Ctx.Config().Font_PreferMonospaced = ui->Font_PreferMonospaced->isChecked();

  m_Ctx.Config().SetupFormatting();

  m_Ctx.Config().Save();
}

void SettingsDialog::on_AlwaysReplayLocally_toggled(bool checked)
{
  m_Ctx.Config().AlwaysReplayLocally = ui->AlwaysReplayLocally->isChecked();

  m_Ctx.Config().Save();
}

// core
void SettingsDialog::on_chooseSearchPaths_clicked()
{
  OrderedListEditor listEd(tr("Shader debug info search paths"), tr("Search Path"),
                           BrowseMode::Folder, this);

  listEd.setItems(m_Ctx.Config()
                      .GetConfigSetting(lit("shader.debug.searchPaths"))
                      .split(QLatin1Char(';'), QString::SkipEmptyParts));

  int res = RDDialog::show(&listEd);

  if(res)
    m_Ctx.Config().SetConfigSetting(lit("shader.debug.searchPaths"),
                                    listEd.getItems().join(QLatin1Char(';')));
}

// texture viewer
void SettingsDialog::on_TextureViewer_PerTexSettings_toggled(bool checked)
{
  m_Ctx.Config().TextureViewer_PerTexSettings = ui->TextureViewer_PerTexSettings->isChecked();

  m_Ctx.Config().Save();
}

void SettingsDialog::on_TextureViewer_ResetRange_toggled(bool checked)
{
  m_Ctx.Config().TextureViewer_ResetRange = ui->TextureViewer_ResetRange->isChecked();

  m_Ctx.Config().Save();
}

// shader viewer
void SettingsDialog::on_ShaderViewer_FriendlyNaming_toggled(bool checked)
{
  m_Ctx.Config().ShaderViewer_FriendlyNaming = ui->ShaderViewer_FriendlyNaming->isChecked();

  m_Ctx.Config().Save();
}

void SettingsDialog::on_browseExtDisasemble_clicked()
{
  QString filePath = RDDialog::getExecutableFileName(this, tr("Locate SPIR-V disassembler"));

  if(!filePath.isEmpty())
  {
    ui->externalDisassemblePath->setText(filePath);
    on_externalDisassemblePath_textEdited(filePath);
  }
}

void SettingsDialog::on_externalDisassemblePath_textEdited(const QString &path)
{
  if(m_Ctx.Config().SPIRVDisassemblers.isEmpty())
  {
    m_Ctx.Config().SPIRVDisassemblers.push_back(SPIRVDisassembler());
    m_Ctx.Config().SPIRVDisassemblers.back().name = lit("Unknown");
  }

  m_Ctx.Config().SPIRVDisassemblers.back().executable = path;

  m_Ctx.Config().Save();
}

void SettingsDialog::on_externalDisassemblerArgs_textEdited(const QString &args)
{
  if(m_Ctx.Config().SPIRVDisassemblers.isEmpty())
  {
    m_Ctx.Config().SPIRVDisassemblers.push_back(SPIRVDisassembler());
    m_Ctx.Config().SPIRVDisassemblers.back().name = lit("Unknown");
  }

  m_Ctx.Config().SPIRVDisassemblers.back().args = args;

  m_Ctx.Config().Save();
}

// event browser
void SettingsDialog::on_EventBrowser_TimeUnit_currentIndexChanged(int index)
{
  if(m_Init)
    return;

  m_Ctx.Config().EventBrowser_TimeUnit = (TimeUnit)ui->EventBrowser_TimeUnit->currentIndex();

  if(m_Ctx.HasEventBrowser())
    m_Ctx.GetEventBrowser()->UpdateDurationColumn();

  m_Ctx.Config().Save();
}

void SettingsDialog::on_EventBrowser_AddFake_toggled(bool checked)
{
  m_Ctx.Config().EventBrowser_AddFake = ui->EventBrowser_AddFake->isChecked();

  m_Ctx.Config().Save();
}

void SettingsDialog::on_EventBrowser_HideEmpty_toggled(bool checked)
{
  m_Ctx.Config().EventBrowser_HideEmpty = ui->EventBrowser_HideEmpty->isChecked();

  m_Ctx.Config().Save();
}

void SettingsDialog::on_EventBrowser_HideAPICalls_toggled(bool checked)
{
  m_Ctx.Config().EventBrowser_HideAPICalls = ui->EventBrowser_HideAPICalls->isChecked();

  m_Ctx.Config().Save();
}

void SettingsDialog::on_EventBrowser_ApplyColors_toggled(bool checked)
{
  m_Ctx.Config().EventBrowser_ApplyColors = ui->EventBrowser_ApplyColors->isChecked();

  m_Ctx.Config().Save();
}

void SettingsDialog::on_EventBrowser_ColorEventRow_toggled(bool checked)
{
  m_Ctx.Config().EventBrowser_ColorEventRow = ui->EventBrowser_ColorEventRow->isChecked();

  m_Ctx.Config().Save();
}

// android
void SettingsDialog::on_browseTempCaptureDirectory_clicked()
{
  QString dir = RDDialog::getExistingDirectory(this, tr("Choose directory for temporary captures"),
                                               m_Ctx.Config().TemporaryCaptureDirectory);

  if(!dir.isEmpty())
    m_Ctx.Config().TemporaryCaptureDirectory = dir;

  m_Ctx.Config().Save();
}

void SettingsDialog::on_browseAdbPath_clicked()
{
  QString adb = RDDialog::getExecutableFileName(
      this, tr("Locate adb executable"),
      QFileInfo(m_Ctx.Config().Android_AdbExecutablePath).absoluteDir().path());

  if(!adb.isEmpty())
  {
    ui->Android_AdbExecutablePath->setText(adb);
    m_Ctx.Config().Android_AdbExecutablePath = adb;
  }

  m_Ctx.Config().Save();
}

void SettingsDialog::on_Android_MaxConnectTimeout_valueChanged(double timeout)
{
  m_Ctx.Config().Android_MaxConnectTimeout = ui->Android_MaxConnectTimeout->value();

  m_Ctx.Config().Save();
}

void SettingsDialog::on_Android_AdbExecutablePath_textEdited(const QString &adb)
{
  if(QFileInfo::exists(adb) || adb.isEmpty())
    m_Ctx.Config().Android_AdbExecutablePath = adb;

  m_Ctx.Config().Save();
}
