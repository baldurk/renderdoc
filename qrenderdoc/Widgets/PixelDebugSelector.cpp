/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2021-2026 Baldur Karlsson
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

#include "PixelDebugSelector.h"
#include "Code/QRDUtils.h"
#include "ui_PixelDebugSelector.h"

PixelDebugSelector::PixelDebugSelector(ICaptureContext &ctx, QWidget *parent)
    : QDialog(parent), ui(new Ui::PixelDebugSelector), m_Ctx(ctx)
{
  ui->setupUi(this);

  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

  ui->x->setFont(Formatter::PreferredFont());
  ui->y->setFont(Formatter::PreferredFont());
  ui->primitive->setFont(Formatter::PreferredFont());
  ui->sample->setFont(Formatter::PreferredFont());
  ui->view->setFont(Formatter::PreferredFont());
}

PixelDebugSelector::~PixelDebugSelector()
{
  delete ui;
}

void PixelDebugSelector::UpdateTarget()
{
  const ActionDescription *action = m_Ctx.CurAction();

  if(!action)
    return;

  const PipeState &pipe = m_Ctx.CurPipelineState();

  ui->view->setMaximum(pipe.MultiviewBroadcastCount() - 1);
  ui->viewLabel->setVisible(ui->view->maximum() > 1);
  ui->view->setVisible(ui->view->maximum() > 1);

  uint32_t sampleCount = pipe.GetOverrideSampleCount();

  if(sampleCount == 0)
  {
    for(const Descriptor &d : pipe.GetOutputTargets())
    {
      const TextureDescription *tex = m_Ctx.GetTexture(d.resource);
      if(tex)
        sampleCount = qMax(sampleCount, tex->msSamp);
    }
  }

  ui->sample->setMaximum(pipe.GetOverrideSampleCount() - 1);
  ui->sampleLabel->setVisible(ui->sample->maximum() > 1);
  ui->sample->setVisible(ui->sample->maximum() > 1);
}

void PixelDebugSelector::on_selectedPixel_clicked()
{
  rdcpair<int32_t, int32_t> sel = m_Ctx.GetTextureViewer()->GetPickedLocation();

  ui->x->setValue(sel.first);
  ui->y->setValue(sel.second);
}

void PixelDebugSelector::on_beginDebug_clicked()
{
  DebugPixelInputs inputs;

  bool ok = false;
  inputs.primitive = ui->primitive->text().toUInt(&ok);
  if(!ok)
    inputs.primitive = ~0U;

  inputs.sample = ui->sample->value();
  inputs.view = ui->view->value();

  emit beginDebug(ui->x->value(), ui->y->value(), inputs);
  close();
}

void PixelDebugSelector::on_cancelDebug_clicked()
{
  close();
}
