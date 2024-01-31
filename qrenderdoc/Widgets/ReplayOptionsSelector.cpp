/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2024 Baldur Karlsson
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

#include "ReplayOptionsSelector.h"
#include <QKeyEvent>
#include "Code/QRDUtils.h"
#include "ui_ReplayOptionsSelector.h"

ReplayOptionsSelector::ReplayOptionsSelector(ICaptureContext &ctx, bool actions, QWidget *parent)
    : m_Ctx(ctx), QWidget(parent), ui(new Ui::ReplayOptionsSelector)
{
  ui->setupUi(this);

  if(!actions)
  {
    ui->captureFileFrame->hide();
    ui->buttonsFrame->hide();
  }

  QObject::connect(ui->open, &QPushButton::clicked, this, &ReplayOptionsSelector::opened);
  QObject::connect(ui->cancel, &QPushButton::clicked, this, &ReplayOptionsSelector::canceled);

  // try to use the remote capture access to enumerate remote GPUs, but if it's not available open a
  // local capture access
  ICaptureFile *dummy = RENDERDOC_OpenCaptureFile();
  ICaptureAccess *capture = m_Ctx.Replay().GetCaptureAccess();

  if(!capture)
    capture = dummy;

  // fetch available GPUs
  m_GPUs = capture->GetAvailableGPUs();

  dummy->Shutdown();

  // this is always available
  ui->gpuOverride->addItem(tr("Default GPU selection"));

  for(const GPUDevice &dev : m_GPUs)
  {
    QString apis;
    for(size_t i = 0; i < dev.apis.size(); i++)
    {
      apis += ToQStr(dev.apis[i]);
      if(i + 1 < dev.apis.size())
        apis += lit(", ");
    }

    QString vendor = ToQStr(dev.vendor);
    QString name = dev.name;

    // if the name already contains the vendor, don't display it twice
    if(name.contains(vendor, Qt::CaseInsensitive))
      ui->gpuOverride->addItem(QFormatStr("%1 [%2]").arg(name).arg(apis));
    else
      ui->gpuOverride->addItem(QFormatStr("%1 %2 [%3]").arg(vendor).arg(name).arg(apis));
  }

  for(ReplayOptimisationLevel level : values<ReplayOptimisationLevel>())
    ui->replayOptimisation->addItem(ToQStr(level));

  // set default options
  {
    const ReplayOptions &opts = m_Ctx.Config().DefaultReplayOptions;

    ui->replayAPIValidation->setChecked(opts.apiValidation);
    ui->replayOptimisation->setCurrentIndex((int)opts.optimisation);

    int bestIndex = -1;

    if(opts.forceGPUVendor == GPUVendor::Unknown && opts.forceGPUDeviceID == 0 &&
       opts.forceGPUDriverName.empty())
    {
      // no forcing active
      bestIndex = -1;
    }
    else
    {
      // if the options are trying to force a GPU, pick the closest one we can find and use it
      bestIndex = 0;

      for(int i = 0; i < m_GPUs.count(); i++)
      {
        // if this is a closer vendor match than the current best, use it
        if(opts.forceGPUVendor == m_GPUs[i].vendor && opts.forceGPUVendor != m_GPUs[bestIndex].vendor)
        {
          bestIndex = i;
          continue;
        }
        else if(m_GPUs[i].vendor != opts.forceGPUVendor)
        {
          continue;
        }

        // if this is a closer device match, use it
        if(opts.forceGPUDeviceID == m_GPUs[i].deviceID &&
           opts.forceGPUDeviceID != m_GPUs[bestIndex].deviceID)
        {
          bestIndex = i;
          continue;
        }
        else if(m_GPUs[i].deviceID != opts.forceGPUDeviceID)
        {
          continue;
        }

        // if this is a closer driver match, use it
        if(opts.forceGPUDriverName == m_GPUs[i].driver &&
           opts.forceGPUDriverName != m_GPUs[bestIndex].driver)
        {
          bestIndex = i;
          continue;
        }
      }
    }

    if(bestIndex >= 0 && bestIndex < m_GPUs.count())
      ui->gpuOverride->setCurrentIndex(bestIndex + 1);
    else
      ui->gpuOverride->setCurrentIndex(0);
  }

  // add recent capture files as options in the dropdown
  for(rdcstr file : m_Ctx.Config().RecentCaptureFiles)
    ui->captureFile->insertItem(0, file);

  // default to the last opened file
  ui->captureFile->setCurrentIndex(0);
}

ReplayOptionsSelector::~ReplayOptionsSelector()
{
  delete ui;
}

QString ReplayOptionsSelector::filename()
{
  return ui->captureFile->currentText();
}

ReplayOptions ReplayOptionsSelector::options()
{
  ReplayOptions opts;

  opts.apiValidation = ui->replayAPIValidation->isChecked();
  opts.optimisation = (ReplayOptimisationLevel)ui->replayOptimisation->currentIndex();

  int gpuChoice = ui->gpuOverride->currentIndex();
  if(gpuChoice > 0 && gpuChoice - 1 < m_GPUs.count())
  {
    const GPUDevice &gpu = m_GPUs[gpuChoice - 1];
    opts.forceGPUVendor = gpu.vendor;
    opts.forceGPUDeviceID = gpu.deviceID;
    opts.forceGPUDriverName = gpu.driver;
  }

  return opts;
}

void ReplayOptionsSelector::on_saveDefaults_clicked()
{
  m_Ctx.Config().DefaultReplayOptions = options();

  m_Ctx.Config().Save();
}

void ReplayOptionsSelector::on_captureFileBrowse_clicked()
{
  QString initDir;

  QFileInfo f(ui->captureFile->currentText());
  QDir dir = f.dir();
  if(f.isAbsolute() && dir.exists())
  {
    initDir = dir.absolutePath();
  }
  else if(!m_Ctx.Config().LastCaptureFilePath.isEmpty())
  {
    initDir = m_Ctx.Config().LastCaptureFilePath;
  }

  QString filename = RDDialog::getOpenFileName(this, tr("Select capture to open"), initDir,
                                               tr("Capture Files (*.rdc);;All Files (*)"));

  if(!filename.isEmpty())
    ui->captureFile->setCurrentText(filename);
}

void ReplayOptionsSelector::keyPressEvent(QKeyEvent *e)
{
  if(e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter)
  {
    opened();
    return;
  }

  QWidget::keyPressEvent(e);
}
