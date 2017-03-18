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

#include "TextureSaveDialog.h"
#include <QColorDialog>
#include <QFileInfo>
#include "Code/CaptureContext.h"
#include "Code/QRDUtils.h"
#include "ui_TextureSaveDialog.h"

TextureSaveDialog::TextureSaveDialog(const FetchTexture &t, const TextureSave &s, QWidget *parent)
    : QDialog(parent), ui(new Ui::TextureSaveDialog)
{
  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
  ui->setupUi(this);

  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

  QObject::connect(&typingTimer, &QTimer::timeout, [this] { SetFiletypeFromFilename(); });

  ui->fileFormat->clear();

  QStringList strs;
  for(int i = 0; i < eFileType_Count; i++)
    strs << ToQStr((FileType)i);

  ui->fileFormat->addItems(strs);

  strs.clear();
  for(int i = 0; i < eAlphaMap_Count; i++)
    strs << ToQStr((AlphaMapping)i);

  ui->alphaMap->addItems(strs);

  tex = t;
  saveData = s;

  ui->jpegCompression->setValue(saveData.jpegQuality);

  ui->alphaMap->setCurrentIndex((int)saveData.alpha);

  ui->blackPoint->setText(Formatter::Format(saveData.comp.blackPoint));
  ui->whitePoint->setText(Formatter::Format(saveData.comp.whitePoint));

  for(uint32_t i = 0; i < tex.mips; i++)
    ui->mipSelect->addItem(QString::number(i) + " - " + QString::number(qMax(1U, tex.width >> i)) +
                           "x" + QString::number(qMax(1U, tex.height >> i)));

  ui->mipSelect->setCurrentIndex(saveData.mip >= 0 ? saveData.mip : 0);

  for(uint32_t i = 0; i < tex.msSamp; i++)
    ui->sampleSelect->addItem(QString("Sample %1").arg(i));

  ui->sampleSelect->setCurrentIndex(qMin(
      (int)tex.msSamp, (saveData.sample.sampleIndex == ~0U ? 0 : (int)saveData.sample.sampleIndex)));

  if(saveData.sample.sampleIndex == ~0U)
  {
    ui->resolveSamples->setChecked(true);
  }
  else
  {
    ui->oneSample->setChecked(true);
  }

  const char *cubeFaces[] = {"X+", "X-", "Y+", "Y-", "Z+", "Z-"};

  uint32_t numSlices = qMax(tex.arraysize, tex.depth);

  for(uint32_t i = 0; i < numSlices; i++)
  {
    if(tex.cubemap)
    {
      QString name = cubeFaces[i % 6];
      // Front 1, Back 2, 3, 4 etc for cube arrays
      if(numSlices > 6)
        name = QString("[%1] %2").arg(i / 6).arg(cubeFaces[i % 6]);
      ui->sliceSelect->addItem(name);
    }
    else
    {
      ui->sliceSelect->addItem(QString("Slice %1").arg(i));
    }
  }

  ui->sliceSelect->setCurrentIndex(saveData.slice.sliceIndex >= 0 ? saveData.slice.sliceIndex : 0);

  ui->gridWidth->setMaximum(tex.depth * tex.arraysize * tex.msSamp);

  ui->mipGroup->setVisible(tex.mips > 1);

  ui->sampleGroup->setVisible(tex.msSamp > 1);

  ui->sliceGroup->setVisible(tex.depth > 1 || tex.arraysize > 1 || tex.msSamp > 1);

  if(saveData.destType != eFileType_DDS)
  {
    ui->cubeCruciform->setEnabled(tex.cubemap && tex.arraysize == 6);

    if(!ui->oneSlice->isChecked() && !ui->cubeCruciform->isEnabled())
      ui->mapSlicesToGrid->setChecked(true);
  }

  ui->fileFormat->setCurrentIndex((int)saveData.destType);

  adjustSize();
}

TextureSaveDialog::~TextureSaveDialog()
{
  delete ui;
}

QString TextureSaveDialog::filename()
{
  return ui->filename->text();
}

void TextureSaveDialog::SetFiletypeFromFilename()
{
  QFileInfo path(ui->filename->text());
  QString ext = path.suffix().toUpper();

  for(int i = 0; i < eFileType_Count; i++)
  {
    if(ToQStr((FileType)i) == ext)
      ui->fileFormat->setCurrentIndex(i);
  }
}

void TextureSaveDialog::SetFilenameFromFiletype()
{
  QFileInfo path(ui->filename->text());
  QString ext = path.suffix().toLower();

  int idx = ui->fileFormat->currentIndex();

  if(idx >= 0 && idx < eFileType_Count)
  {
    QString selectedExt = ToQStr((FileType)idx).toLower();

    if(ext != selectedExt && ext != "")
    {
      QString fn = ui->filename->text();
      fn.chop(ext.length());
      fn += selectedExt;
      ui->filename->setText(fn);
    }
  }
}

void TextureSaveDialog::on_fileFormat_currentIndexChanged(int index)
{
  saveData.destType = (FileType)ui->fileFormat->currentIndex();

  ui->jpegCompression->setEnabled(saveData.destType == eFileType_JPG);

  ui->alphaGroup->setVisible(saveData.destType != eFileType_HDR &&
                             saveData.destType != eFileType_EXR &&
                             saveData.destType != eFileType_DDS);

  bool noAlphaFormat = (saveData.destType == eFileType_BMP || saveData.destType == eFileType_JPG);

  ui->alphaMap->setEnabled(tex.format.compCount == 4 && noAlphaFormat);

  ui->alphaCol->setEnabled(saveData.alpha == eAlphaMap_BlendToColour && tex.format.compCount == 4 &&
                           noAlphaFormat);

  if(saveData.destType == eFileType_DDS)
  {
    ui->exportAllMips->setEnabled(true);
    ui->exportAllMips->setChecked(true);

    ui->exportAllSlices->setEnabled(true);
    ui->exportAllSlices->setChecked(true);

    ui->cubeCruciform->setEnabled(true);
    ui->cubeCruciform->setChecked(false);

    ui->gridWidth->setEnabled(false);

    ui->mapSlicesToGrid->setEnabled(false);
    ui->mapSlicesToGrid->setChecked(false);
  }
  else
  {
    ui->exportAllMips->setEnabled(false);
    ui->oneMip->setChecked(true);
    ui->oneSlice->setChecked(true);
  }
  SetFilenameFromFiletype();

  adjustSize();
}

void TextureSaveDialog::on_jpegCompression_valueChanged(double arg1)
{
  saveData.jpegQuality = (int)arg1;
}

void TextureSaveDialog::on_exportAllMips_toggled(bool)
{
  if(m_Recurse)
    return;

  m_Recurse = true;

  ui->oneMip->setChecked(!ui->exportAllMips->isChecked());
  ui->mipSelect->setEnabled(ui->oneMip->isChecked());

  m_Recurse = false;
}

void TextureSaveDialog::on_oneMip_toggled(bool checked)
{
  if(m_Recurse)
    return;

  m_Recurse = true;

  ui->exportAllMips->setChecked(!ui->oneMip->isChecked());
  ui->mipSelect->setEnabled(ui->oneMip->isChecked());

  if(saveData.destType != eFileType_DDS)
  {
    ui->oneMip->setChecked(true);
    ui->exportAllMips->setChecked(false);
    ui->mipSelect->setEnabled(true);
  }

  m_Recurse = false;
}

void TextureSaveDialog::on_mipSelect_currentIndexChanged(int index)
{
  saveData.mip = index;
}

void TextureSaveDialog::on_mapSampleArray_toggled(bool checked)
{
  if(m_Recurse)
    return;

  m_Recurse = true;

  if(ui->mapSampleArray->isChecked())
  {
    ui->resolveSamples->setChecked(false);
    ui->oneSample->setChecked(false);
  }
  else
  {
    ui->resolveSamples->setChecked(false);
    ui->oneSample->setChecked(true);
  }
  ui->sampleSelect->setEnabled(ui->oneSample->isChecked());

  m_Recurse = false;
}

void TextureSaveDialog::on_resolveSamples_toggled(bool checked)
{
  if(m_Recurse)
    return;

  m_Recurse = true;

  if(ui->resolveSamples->isChecked())
  {
    ui->mapSampleArray->setChecked(false);
    ui->oneSample->setChecked(false);
  }
  else
  {
    ui->mapSampleArray->setChecked(false);
    ui->oneSample->setChecked(true);
  }
  ui->sampleSelect->setEnabled(ui->oneSample->isChecked());

  m_Recurse = false;
}

void TextureSaveDialog::on_oneSample_toggled(bool checked)
{
  if(m_Recurse)
    return;

  m_Recurse = true;

  if(ui->oneSample->isChecked())
  {
    ui->mapSampleArray->setChecked(false);
    ui->resolveSamples->setChecked(false);
  }
  else
  {
    ui->mapSampleArray->setChecked(false);
    ui->resolveSamples->setChecked(true);
  }
  ui->sampleSelect->setEnabled(ui->oneSample->isChecked());

  m_Recurse = false;
}

void TextureSaveDialog::on_sampleSelect_currentIndexChanged(int index)
{
  saveData.sample.sampleIndex = (uint32_t)index;
}

void TextureSaveDialog::on_exportAllSlices_toggled(bool checked)
{
  if(m_Recurse)
    return;

  m_Recurse = true;

  ui->oneSlice->setChecked(!ui->exportAllSlices->isChecked());
  if(saveData.destType == eFileType_DDS)
  {
    ui->mapSlicesToGrid->setEnabled(false);
    ui->gridWidth->setEnabled(false);
    ui->cubeCruciform->setEnabled(false);
  }
  else
  {
    ui->mapSlicesToGrid->setEnabled(!ui->oneSlice->isChecked());
    ui->gridWidth->setEnabled(!ui->oneSlice->isChecked());

    if(!ui->oneSlice->isChecked() && !ui->cubeCruciform->isChecked())
      ui->mapSlicesToGrid->setChecked(true);

    if(tex.cubemap && tex.arraysize == 6)
      ui->cubeCruciform->setEnabled(!ui->oneSlice->isChecked());
    else
      ui->cubeCruciform->setEnabled(false);
  }
  ui->sliceSelect->setEnabled(ui->oneSlice->isChecked());

  m_Recurse = false;
}

void TextureSaveDialog::on_oneSlice_toggled(bool checked)
{
  if(m_Recurse)
    return;

  m_Recurse = true;

  ui->exportAllSlices->setChecked(!ui->oneSlice->isChecked());
  if(saveData.destType == eFileType_DDS)
  {
    ui->mapSlicesToGrid->setEnabled(false);
    ui->gridWidth->setEnabled(false);
    ui->cubeCruciform->setEnabled(false);
  }
  else
  {
    ui->mapSlicesToGrid->setEnabled(!ui->oneSlice->isChecked());
    ui->gridWidth->setEnabled(!ui->oneSlice->isChecked());

    if(!ui->oneSlice->isChecked() && !ui->cubeCruciform->isChecked())
      ui->mapSlicesToGrid->setChecked(true);

    if(tex.cubemap && tex.arraysize == 6)
      ui->cubeCruciform->setEnabled(!ui->oneSlice->isChecked());
    else
      ui->cubeCruciform->setEnabled(false);
  }
  ui->sliceSelect->setEnabled(ui->oneSlice->isChecked());

  m_Recurse = false;
}

void TextureSaveDialog::on_mapSlicesToGrid_toggled(bool checked)
{
  if(m_Recurse)
    return;

  m_Recurse = true;

  if(ui->mapSlicesToGrid->isChecked())
  {
    ui->cubeCruciform->setChecked(false);
  }
  else if(saveData.destType != eFileType_DDS)
  {
    ui->oneSlice->setChecked(true);
    ui->exportAllSlices->setChecked(false);
    ui->cubeCruciform->setEnabled(false);
    ui->mapSlicesToGrid->setEnabled(false);
    ui->gridWidth->setEnabled(false);
    ui->sliceSelect->setEnabled(true);
  }

  m_Recurse = false;

  if(saveData.destType == eFileType_DDS)
    ui->gridWidth->setEnabled(false);
  else
    ui->gridWidth->setEnabled(ui->mapSlicesToGrid->isChecked());
}

void TextureSaveDialog::on_cubeCruciform_toggled(bool checked)
{
  if(m_Recurse)
    return;

  m_Recurse = true;

  if(ui->cubeCruciform->isChecked())
  {
    ui->mapSlicesToGrid->setChecked(false);
  }
  else if(saveData.destType != eFileType_DDS)
  {
    ui->oneSlice->setChecked(true);
    ui->exportAllSlices->setChecked(false);
    ui->cubeCruciform->setEnabled(false);
    ui->mapSlicesToGrid->setEnabled(false);
    ui->gridWidth->setEnabled(false);
    ui->sliceSelect->setEnabled(true);
  }

  m_Recurse = false;
}

void TextureSaveDialog::on_sliceSelect_currentIndexChanged(int index)
{
  saveData.slice.sliceIndex = index;
}

void TextureSaveDialog::on_gridWidth_valueChanged(double arg1)
{
  saveData.slice.sliceGridWidth = (int)arg1;
}

void TextureSaveDialog::on_alphaCol_clicked()
{
  QColor col = QColorDialog::getColor(Qt::black, this, tr("Choose background colour"));

  if(col.isValid())
  {
    col = col.toRgb();

    saveData.alphaCol = FloatVector(col.redF(), col.greenF(), col.blueF(), 1.0f);
  }
}

void TextureSaveDialog::on_alphaMap_currentIndexChanged(int index)
{
  saveData.alpha = (AlphaMapping)index;

  ui->alphaCol->setEnabled(saveData.alpha == eAlphaMap_BlendToColour);
}

void TextureSaveDialog::on_blackPoint_textEdited(const QString &arg)
{
  bool ok = false;
  double d = arg.toDouble(&ok);

  if(ok)
    saveData.comp.blackPoint = d;
}

void TextureSaveDialog::on_whitePoint_textEdited(const QString &arg)
{
  bool ok = false;
  double d = arg.toDouble(&ok);

  if(ok)
    saveData.comp.whitePoint = d;
}

void TextureSaveDialog::on_browse_clicked()
{
  QString filter = "";

  for(int i = 0; i < eFileType_Count; i++)
  {
    QString ext = ToQStr((FileType)i);

    if(filter.length() > 0)
      filter += ";;";
    filter += tr("%1 Files (*.%2)").arg(ext).arg(ext.toLower());
  }

  QString *selectedFilter = NULL;

  QString filename =
      RDDialog::getSaveFileName(this, tr("Save Texture As"), "", filter, selectedFilter);

  QFileInfo checkFile(filename);
  if(filename != "")
  {
    ui->filename->setText(filename);
    SetFiletypeFromFilename();
  }
}

void TextureSaveDialog::on_filename_textEdited(const QString &arg1)
{
  typingTimer.stop();
  typingTimer.setSingleShot(true);
  typingTimer.start(500);
}

void TextureSaveDialog::on_saveCancelButtons_accepted()
{
  saveData.alpha = (AlphaMapping)ui->alphaMap->currentIndex();

  if(saveData.alpha == eAlphaMap_BlendToCheckerboard)
  {
    saveData.alphaCol = FloatVector(0.666f, 0.666f, 0.666f, 1.0f);
  }

  if(ui->exportAllMips->isChecked())
    saveData.mip = -1;
  else
    saveData.mip = (int)ui->mipSelect->currentIndex();

  if(ui->resolveSamples->isChecked())
  {
    saveData.sample.sampleIndex = ~0U;
    saveData.sample.mapToArray = false;
  }
  else if(ui->mapSampleArray->isChecked())
  {
    saveData.sample.sampleIndex = 0;
    saveData.sample.mapToArray = true;
  }
  else
  {
    saveData.sample.sampleIndex = (uint)ui->sampleSelect->currentIndex();
    saveData.sample.mapToArray = false;
  }

  if(!ui->exportAllSlices->isChecked())
  {
    saveData.slice.cubeCruciform = saveData.slice.slicesAsGrid = false;
    saveData.slice.sliceGridWidth = 1;
    saveData.slice.sliceIndex = (int)ui->sliceSelect->currentIndex();
  }
  else
  {
    saveData.slice.sliceIndex = -1;
    if(ui->cubeCruciform->isChecked())
    {
      saveData.slice.cubeCruciform = true;
      saveData.slice.slicesAsGrid = false;
      saveData.slice.sliceGridWidth = 1;
    }
    else
    {
      saveData.slice.cubeCruciform = false;
      saveData.slice.slicesAsGrid = true;
      saveData.slice.sliceGridWidth = (int)ui->gridWidth->value();
    }
  }

  saveData.destType = (FileType)ui->fileFormat->currentIndex();
  saveData.jpegQuality = (int)ui->jpegCompression->value();

  bool ok = false;
  double d = 0.0;

  d = ui->blackPoint->text().toDouble(&ok);

  if(ok)
    saveData.comp.blackPoint = d;

  d = ui->whitePoint->text().toDouble(&ok);

  if(ok)
    saveData.comp.whitePoint = d;

  QFileInfo fi(filename());

  QDir dir = fi.dir();

  bool valid = dir.makeAbsolute();

  if(!valid || !dir.exists())
  {
    RDDialog::critical(this, tr("Save Texture"),
                       tr("%1\nPath does not exist.\nCheck the path and try again.").arg(filename()));

    return;
  }

  if(fi.exists())
  {
    QMessageBox::StandardButton button =
        RDDialog::question(this, tr("Confirm Save Texture"),
                           tr("%1 already exists.\nDo you want to replace it?").arg(fi.fileName()));

    if(button != QMessageBox::Yes)
      return;
  }

  // path is valid and either doesn't exist or user confirmed replacement
  setResult(1);
  accept();
}

void TextureSaveDialog::on_saveCancelButtons_rejected()
{
  reject();
}
