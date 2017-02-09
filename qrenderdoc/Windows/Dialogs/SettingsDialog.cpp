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
#include "Code/PersistantConfig.h"
#include "Code/QRDUtils.h"
#include "Windows/Dialogs/OrderedListEditor.h"
#include "CaptureDialog.h"
#include "ui_SettingsDialog.h"

SettingsDialog::SettingsDialog(CaptureContext &ctx, QWidget *parent)
    : QDialog(parent), ui(new Ui::SettingsDialog), m_Ctx(ctx)
{
  ui->setupUi(this);

  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

  ui->tabWidget->tabBar()->setVisible(false);

  for(int i = 0; i < ui->tabWidget->count(); i++)
    ui->pages->addItem(ui->tabWidget->tabText(i));

  m_Init = true;

  for(int i = 0; i < (int)PersistantConfig::TimeUnit::Count; i++)
  {
    ui->EventBrowser_TimeUnit->addItem(PersistantConfig::UnitPrefix((PersistantConfig::TimeUnit)i));
  }

  ui->pages->clearSelection();
  ui->pages->setItemSelected(ui->pages->item(0), true);
  ui->tabWidget->setCurrentIndex(0);

  ui->saveDirectory->setText(m_Ctx.Config.DefaultCaptureSaveDirectory);
  ui->tempDirectory->setText(m_Ctx.Config.TemporaryCaptureDirectory);

  // TODO external disassembler
  /*
  ui->ExternalDisassemblerEnabled->setChecked(m_Ctx.Config.ExternalDisassemblerEnabled);
  ui->externalDisassemblerArgs->setText(m_Ctx.Config.GetDefaultExternalDisassembler().args);
  ui->externalDisassemblePath->setText(m_Ctx.Config.GetDefaultExternalDisassembler().executable);
  */
  ui->Android_AdbExecutablePath->setText(m_Ctx.Config.Android_AdbExecutablePath);
  ui->Android_MaxConnectTimeout->setValue(m_Ctx.Config.Android_MaxConnectTimeout);

  ui->TextureViewer_ResetRange->setChecked(m_Ctx.Config.TextureViewer_ResetRange);
  ui->TextureViewer_PerTexSettings->setChecked(m_Ctx.Config.TextureViewer_PerTexSettings);
  ui->ShaderViewer_FriendlyNaming->setChecked(m_Ctx.Config.ShaderViewer_FriendlyNaming);
  ui->CheckUpdate_AllowChecks->setChecked(m_Ctx.Config.CheckUpdate_AllowChecks);
  ui->Font_PreferMonospaced->setChecked(m_Ctx.Config.Font_PreferMonospaced);

  ui->AlwaysReplayLocally->setChecked(m_Ctx.Config.AlwaysReplayLocally);

  ui->AllowGlobalHook->setChecked(m_Ctx.Config.AllowGlobalHook);

  ui->EventBrowser_TimeUnit->setCurrentIndex((int)m_Ctx.Config.EventBrowser_TimeUnit);
  ui->EventBrowser_HideEmpty->setChecked(m_Ctx.Config.EventBrowser_HideEmpty);
  ui->EventBrowser_HideAPICalls->setChecked(m_Ctx.Config.EventBrowser_HideAPICalls);
  ui->EventBrowser_ApplyColours->setChecked(m_Ctx.Config.EventBrowser_ApplyColours);
  ui->EventBrowser_ColourEventRow->setChecked(m_Ctx.Config.EventBrowser_ColourEventRow);

  // disable sub-checkbox
  ui->EventBrowser_ColourEventRow->setEnabled(ui->EventBrowser_ApplyColours->isChecked());

  ui->Formatter_MinFigures->setValue(m_Ctx.Config.Formatter_MinFigures);
  ui->Formatter_MaxFigures->setValue(m_Ctx.Config.Formatter_MaxFigures);
  ui->Formatter_NegExp->setValue(m_Ctx.Config.Formatter_NegExp);
  ui->Formatter_PosExp->setValue(m_Ctx.Config.Formatter_PosExp);

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
  m_Ctx.Config.Formatter_MinFigures = ui->Formatter_MinFigures->value();
  m_Ctx.Config.Formatter_MaxFigures = ui->Formatter_MaxFigures->value();
  m_Ctx.Config.Formatter_NegExp = ui->Formatter_NegExp->value();
  m_Ctx.Config.Formatter_PosExp = ui->Formatter_PosExp->value();

  m_Ctx.Config.SetupFormatting();

  m_Ctx.Config.Save();
}

void SettingsDialog::on_tempDirectory_textEdited(const QString &dir)
{
  if(QDir(dir).exists())
    m_Ctx.Config.TemporaryCaptureDirectory = dir;
  else
    m_Ctx.Config.TemporaryCaptureDirectory = "";

  m_Ctx.Config.Save();
}

void SettingsDialog::on_saveDirectory_textEdited(const QString &dir)
{
  if(QDir(dir).exists() || dir == "")
    m_Ctx.Config.DefaultCaptureSaveDirectory = dir;

  m_Ctx.Config.Save();
}

void SettingsDialog::on_browseSaveCaptureDirectory_clicked()
{
  QString dir = RDDialog::getExistingDirectory(this, "Choose default directory for saving captures",
                                               m_Ctx.Config.DefaultCaptureSaveDirectory);

  if(dir != "")
    m_Ctx.Config.DefaultCaptureSaveDirectory = dir;

  m_Ctx.Config.Save();
}

void SettingsDialog::on_AllowGlobalHook_toggled(bool checked)
{
  m_Ctx.Config.AllowGlobalHook = ui->AllowGlobalHook->isChecked();

  m_Ctx.Config.Save();

  if(m_Ctx.hasCaptureDialog())
    m_Ctx.captureDialog()->updateGlobalHook();
}

void SettingsDialog::on_CheckUpdate_AllowChecks_toggled(bool checked)
{
  m_Ctx.Config.CheckUpdate_AllowChecks = ui->CheckUpdate_AllowChecks->isChecked();

  m_Ctx.Config.Save();
}

void SettingsDialog::on_Font_PreferMonospaced_toggled(bool checked)
{
  m_Ctx.Config.Font_PreferMonospaced = ui->Font_PreferMonospaced->isChecked();

  m_Ctx.Config.SetupFormatting();

  m_Ctx.Config.Save();
}

void SettingsDialog::on_AlwaysReplayLocally_toggled(bool checked)
{
  m_Ctx.Config.AlwaysReplayLocally = ui->AlwaysReplayLocally->isChecked();

  m_Ctx.Config.Save();
}

// core
void SettingsDialog::on_chooseSearchPaths_clicked()
{
  OrderedListEditor listEd(tr("Shader debug info search paths"), tr("Search Path"),
                           BrowseMode::Folder, this);

  listEd.setItems(m_Ctx.Config.GetConfigSetting("shader.debug.searchPaths")
                      .split(QChar(';'), QString::SkipEmptyParts));

  int res = RDDialog::show(&listEd);

  if(res)
    m_Ctx.Config.SetConfigSetting("shader.debug.searchPaths", listEd.getItems().join(QChar(';')));
}

// texture viewer
void SettingsDialog::on_TextureViewer_PerTexSettings_toggled(bool checked)
{
  m_Ctx.Config.TextureViewer_PerTexSettings = ui->TextureViewer_PerTexSettings->isChecked();

  m_Ctx.Config.Save();
}

void SettingsDialog::on_TextureViewer_ResetRange_toggled(bool checked)
{
  m_Ctx.Config.TextureViewer_ResetRange = ui->TextureViewer_ResetRange->isChecked();

  m_Ctx.Config.Save();
}

// shader viewer
void SettingsDialog::on_ShaderViewer_FriendlyNaming_toggled(bool checked)
{
  m_Ctx.Config.ShaderViewer_FriendlyNaming = ui->ShaderViewer_FriendlyNaming->isChecked();

  m_Ctx.Config.Save();
}

void SettingsDialog::on_ExternalDisassemblerEnabled_toggled(bool checked)
{
  // TODO external disassembler
  // m_Ctx.Config.ExternalDisassemblerEnabled = ui->ExternalDisassemblerEnabled->isChecked();

  m_Ctx.Config.Save();
}

void SettingsDialog::on_browseExtDisasemble_clicked()
{
  // TODO external disassembler
}

void SettingsDialog::on_externalDisassemblePath_textEdited(const QString &disasm)
{
  // TODO external disassembler

  m_Ctx.Config.Save();
}

void SettingsDialog::on_externalDisassemblerArgs_textEdited(const QString &args)
{
  // TODO external disassembler

  m_Ctx.Config.Save();
}

// event browser
void SettingsDialog::on_EventBrowser_TimeUnit_currentIndexChanged(int index)
{
  if(m_Init)
    return;

  m_Ctx.Config.EventBrowser_TimeUnit =
      (PersistantConfig::TimeUnit)ui->EventBrowser_TimeUnit->currentIndex();

  m_Ctx.Config.Save();
}

void SettingsDialog::on_EventBrowser_HideEmpty_toggled(bool checked)
{
  m_Ctx.Config.EventBrowser_HideEmpty = ui->EventBrowser_HideEmpty->isChecked();

  m_Ctx.Config.Save();
}

void SettingsDialog::on_EventBrowser_HideAPICalls_toggled(bool checked)
{
  m_Ctx.Config.EventBrowser_HideAPICalls = ui->EventBrowser_HideAPICalls->isChecked();

  m_Ctx.Config.Save();
}

void SettingsDialog::on_EventBrowser_ApplyColours_toggled(bool checked)
{
  m_Ctx.Config.EventBrowser_ApplyColours = ui->EventBrowser_ApplyColours->isChecked();

  m_Ctx.Config.Save();
}

void SettingsDialog::on_EventBrowser_ColourEventRow_toggled(bool checked)
{
  m_Ctx.Config.EventBrowser_ColourEventRow = ui->EventBrowser_ColourEventRow->isChecked();

  m_Ctx.Config.Save();
}

// android
void SettingsDialog::on_browseTempCaptureDirectory_clicked()
{
  QString dir = RDDialog::getExistingDirectory(this, "Choose directory for temporary captures",
                                               m_Ctx.Config.TemporaryCaptureDirectory);

  if(dir != "")
    m_Ctx.Config.TemporaryCaptureDirectory = dir;

  m_Ctx.Config.Save();
}

void SettingsDialog::on_browseAdbPath_clicked()
{
  QString adb = RDDialog::getExecutableFileName(
      this, "Locate adb executable",
      QFileInfo(m_Ctx.Config.Android_AdbExecutablePath).absoluteDir().path());

  if(adb != "")
    m_Ctx.Config.Android_AdbExecutablePath = adb;

  m_Ctx.Config.Save();
}

void SettingsDialog::on_Android_MaxConnectTimeout_valueChanged(double timeout)
{
  m_Ctx.Config.Android_MaxConnectTimeout = ui->Android_MaxConnectTimeout->value();

  m_Ctx.Config.Save();
}

void SettingsDialog::on_Android_AdbExecutablePath_textEdited(const QString &adb)
{
  if(QFileInfo::exists(adb))
    m_Ctx.Config.Android_AdbExecutablePath = adb;

  m_Ctx.Config.Save();
}
