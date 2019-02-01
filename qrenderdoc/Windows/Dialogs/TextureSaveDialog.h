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

#pragma once

#include <QDialog>
#include <QTimer>
#include "Code/Interface/QRDInterface.h"

namespace Ui
{
class TextureSaveDialog;
}

class TextureSaveDialog : public QDialog
{
  Q_OBJECT

public:
  explicit TextureSaveDialog(const TextureDescription &t, bool enableOverlaySelection,
                             const TextureSave &s, QWidget *parent = 0);
  ~TextureSaveDialog();

  QString filename();

  TextureSave config() { return saveData; }
  bool saveOverlayInstead() { return m_saveOverlayInsteadOfSelectedTexture; }
private slots:
  void on_mainTex_clicked();
  void on_overlayTex_clicked();
  void on_fileFormat_currentIndexChanged(int index);
  void on_jpegCompression_valueChanged(double arg1);
  void on_exportAllMips_toggled(bool checked);
  void on_oneMip_toggled(bool checked);
  void on_mipSelect_currentIndexChanged(int index);
  void on_mapSampleArray_toggled(bool checked);
  void on_resolveSamples_toggled(bool checked);
  void on_oneSample_toggled(bool checked);
  void on_sampleSelect_currentIndexChanged(int index);
  void on_exportAllSlices_toggled(bool checked);
  void on_oneSlice_toggled(bool checked);
  void on_mapSlicesToGrid_toggled(bool checked);
  void on_cubeCruciform_toggled(bool checked);
  void on_sliceSelect_currentIndexChanged(int index);
  void on_gridWidth_valueChanged(double arg1);
  void on_alphaCol_clicked();
  void on_alphaMap_currentIndexChanged(int index);
  void on_blackPoint_textEdited(const QString &arg1);
  void on_whitePoint_textEdited(const QString &arg1);
  void on_browse_clicked();
  void on_filename_textEdited(const QString &arg1);

  void on_saveCancelButtons_accepted();

  void on_saveCancelButtons_rejected();

private:
  Ui::TextureSaveDialog *ui;

  void SetOptionsVisible(bool visible);

  void SetFilenameFromFiletype();
  void SetFiletypeFromFilename();

  FileType selectedFileType();

  QTimer typingTimer;

  TextureDescription tex;
  TextureSave saveData;

  bool m_saveOverlayInsteadOfSelectedTexture = false;

  bool m_Recurse = false;
};
