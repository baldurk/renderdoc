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

#include "AxisMappingDialog.h"
#include "Code/QRDUtils.h"
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

AxisMappingDialog::AxisMappingDialog(ICaptureContext &Ctx, const MeshDisplay &config, QWidget *parent)
    : QDialog(parent), m_Ctx(Ctx), m_AxisMapping(config.axisMapping), ui(new Ui::AxisMappingDialog)
{
  ui->setupUi(this);
  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

  const QStringList items = {tr("Right"), tr("Left"),        tr("Up"),
                             tr("Down"),  tr("Into Screen"), tr("Out of Screen")};

  ui->xAxisCombo->addItems(items);
  ui->yAxisCombo->addItems(items);
  ui->zAxisCombo->addItems(items);
  ui->xAxisCombo->setCurrentIndex(getIndexFromVector(m_AxisMapping.xAxis));
  ui->yAxisCombo->setCurrentIndex(getIndexFromVector(m_AxisMapping.yAxis));
  ui->zAxisCombo->setCurrentIndex(getIndexFromVector(m_AxisMapping.zAxis));

  connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &AxisMappingDialog::setNewAxisMapping);
  connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void AxisMappingDialog::setNewAxisMapping()
{
  int xIndex = ui->xAxisCombo->currentIndex();
  int yIndex = ui->yAxisCombo->currentIndex();
  int zIndex = ui->zAxisCombo->currentIndex();

  int xDirection = xIndex / 2;
  int yDirection = yIndex / 2;
  int zDirection = zIndex / 2;

  if(xDirection != yDirection && yDirection != zDirection && xDirection != zDirection)
  {
    m_AxisMapping.xAxis = getVectorFromIndex(xIndex);
    m_AxisMapping.yAxis = getVectorFromIndex(yIndex);
    m_AxisMapping.zAxis = getVectorFromIndex(zIndex);
    accept();
  }
  else
  {
    QString firstWrongAxis;
    QString secondWrongAxis;
    int duplicateDirection;
    bool allAxesDegenerate = false;

    if(xDirection == yDirection)
    {
      if(yDirection == zDirection)
      {
        allAxesDegenerate = true;
      }
      firstWrongAxis = tr("X");
      secondWrongAxis = tr("Y");
      duplicateDirection = xDirection;
    }
    else if(yDirection == zDirection)
    {
      firstWrongAxis = tr("Y");
      secondWrongAxis = tr("Z");
      duplicateDirection = yDirection;
    }
    else
    {
      firstWrongAxis = tr("X");
      secondWrongAxis = tr("Z");
      duplicateDirection = zDirection;
    }

    const QStringList directions = {tr("left/right"), tr("up/down"), tr("into screen/out of screen")};

    QString messageText;
    if(allAxesDegenerate)
    {
      messageText = tr("The selected axis mappings are degenerate "
                       "and do not cover all three directions:\n\n"
                       "All axes are mapped to the %1 direction.")
                        .arg(directions.at(duplicateDirection));
    }
    else
    {
      messageText = tr("The selected axis mappings are degenerate "
                       "and do not cover all three directions:\n\n"
                       "%1 and %2 are both mapped to the %3 direction.")
                        .arg(firstWrongAxis)
                        .arg(secondWrongAxis)
                        .arg(directions.at(duplicateDirection));
    }
    RDDialog::critical(this, tr("Error mapping axes"), messageText);
  }
}

const AxisMapping &AxisMappingDialog::getAxisMapping()
{
  return m_AxisMapping;
}

AxisMappingDialog::~AxisMappingDialog()
{
  delete ui;
}
