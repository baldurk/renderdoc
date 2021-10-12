/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2021 Baldur Karlsson
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

#include "AxisMappingDialog.h"
#include <QMessageBox>
#include "ui_AxisMappingDialog.h"

int AxisMappingDialog::getIndexFromVector(const FloatVector &v)
{
  float x = v.x;
  float y = v.y;
  float z = v.z;
  int index;
  float nonZeroMapping;

  if(x != 0.0f)
  {
    index = 0;
    nonZeroMapping = x;
  }
  else if(y != 0.0f)
  {
    index = 2;
    nonZeroMapping = y;
  }
  else
  {
    index = 4;
    nonZeroMapping = z;
  }

  if(nonZeroMapping == -1.0f)
  {
    index += 1;
  }
  return index;
}

FloatVector AxisMappingDialog::getVectorFromIndex(int index)
{
  FloatVector v = FloatVector();
  if(index == 0)
  {
    v.x = 1.0f;
  }
  else if(index == 1)
  {
    v.x = -1.0f;
  }
  else if(index == 2)
  {
    v.y = 1.0f;
  }
  else if(index == 3)
  {
    v.y = -1.0f;
  }
  else if(index == 4)
  {
    v.z = 1.0f;
  }
  else
  {
    v.z = -1.0f;
  }
  return v;
}

AxisMappingDialog::AxisMappingDialog(ICaptureContext &Ctx, const MeshDisplay &m_config,
                                     QWidget *parent)
    : QDialog(parent),
      m_Ctx(Ctx),
      xAxisMapping(m_config.xAxisMapping),
      yAxisMapping(m_config.yAxisMapping),
      zAxisMapping(m_config.zAxisMapping),
      ui(new Ui::AxisMappingDialog)
{
  ui->setupUi(this);
  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

  const QStringList items{tr("Right"), tr("Left"),        tr("Up"),
                          tr("Down"),  tr("Into Screen"), tr("Out of Screen")};

  ui->xAxisCombo->addItems(items);
  ui->yAxisCombo->addItems(items);
  ui->zAxisCombo->addItems(items);
  ui->xAxisCombo->setCurrentIndex(getIndexFromVector(xAxisMapping));
  ui->yAxisCombo->setCurrentIndex(getIndexFromVector(yAxisMapping));
  ui->zAxisCombo->setCurrentIndex(getIndexFromVector(zAxisMapping));

  connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &AxisMappingDialog::setNewAxisMappings);
  connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void AxisMappingDialog::setNewAxisMappings()
{
  int xIndex = ui->xAxisCombo->currentIndex();
  int yIndex = ui->yAxisCombo->currentIndex();
  int zIndex = ui->zAxisCombo->currentIndex();

  if(xIndex / 2 != yIndex / 2 && yIndex / 2 != zIndex / 2 && xIndex / 2 != zIndex / 2)
  {
    xAxisMapping = getVectorFromIndex(xIndex);
    yAxisMapping = getVectorFromIndex(yIndex);
    zAxisMapping = getVectorFromIndex(zIndex);
    accept();
  }
  else
  {
    QMessageBox messageBox;
    messageBox.critical(0, tr("Error"), tr("Your axis mappings are not compatible."));
    messageBox.setFixedSize(700, 150);
  }
  // insert error message logic here
}

FloatVector AxisMappingDialog::getXAxisMapping()
{
  return xAxisMapping;
}

FloatVector AxisMappingDialog::getYAxisMapping()
{
  return yAxisMapping;
}

FloatVector AxisMappingDialog::getZAxisMapping()
{
  return zAxisMapping;
}

AxisMappingDialog::~AxisMappingDialog()
{
  delete ui;
}
