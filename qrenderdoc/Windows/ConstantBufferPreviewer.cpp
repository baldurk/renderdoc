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

#include "ConstantBufferPreviewer.h"
#include <QFontDatabase>
#include "3rdparty/toolwindowmanager/ToolWindowManager.h"
#include "ui_ConstantBufferPreviewer.h"

QList<ConstantBufferPreviewer *> ConstantBufferPreviewer::m_Previews;

ConstantBufferPreviewer::ConstantBufferPreviewer(ICaptureContext &ctx, const ShaderStage stage,
                                                 uint32_t slot, uint32_t idx, QWidget *parent)
    : QFrame(parent), ui(new Ui::ConstantBufferPreviewer), m_Ctx(ctx)
{
  ui->setupUi(this);

  m_stage = stage;
  m_slot = slot;
  m_arrayIdx = idx;

  QObject::connect(ui->formatSpecifier, &BufferFormatSpecifier::processFormat, this,
                   &ConstantBufferPreviewer::processFormat);

  ui->formatSpecifier->showHelp(false);

  ui->splitter->setCollapsible(1, true);
  ui->splitter->setSizes({1, 0});
  ui->splitter->handle(1)->setEnabled(false);

  {
    ui->variables->setColumns({tr("Name"), tr("Value"), tr("Type")});
    ui->variables->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    ui->variables->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    ui->variables->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
  }

  ui->variables->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));

  m_Previews.push_back(this);
  m_Ctx.AddLogViewer(this);
}

ConstantBufferPreviewer::~ConstantBufferPreviewer()
{
  m_Ctx.RemoveLogViewer(this);
  m_Previews.removeOne(this);
  delete ui;
}

ConstantBufferPreviewer *ConstantBufferPreviewer::has(ShaderStage stage, uint32_t slot, uint32_t idx)
{
  for(ConstantBufferPreviewer *c : m_Previews)
  {
    if(c->m_stage == stage && c->m_slot == slot && c->m_arrayIdx == idx)
      return c;
  }

  return NULL;
}

ConstantBufferPreviewer *ConstantBufferPreviewer::getOne()
{
  for(ConstantBufferPreviewer *c : m_Previews)
  {
    if(ToolWindowManager::managerOf(c))
      return c;
  }

  return NULL;
}

void ConstantBufferPreviewer::OnLogfileLoaded()
{
  OnLogfileClosed();
}

void ConstantBufferPreviewer::OnLogfileClosed()
{
  ui->variables->clear();

  ui->saveCSV->setEnabled(false);

  ToolWindowManager::closeToolWindow(this);
}

void ConstantBufferPreviewer::OnEventChanged(uint32_t eventID)
{
  BoundCBuffer cb = m_Ctx.CurPipelineState().GetConstantBuffer(m_stage, m_slot, m_arrayIdx);
  m_cbuffer = cb.Buffer;
  uint64_t offs = cb.ByteOffset;
  uint64_t size = cb.ByteSize;

  m_shader = m_Ctx.CurPipelineState().GetShader(m_stage);
  QString entryPoint = m_Ctx.CurPipelineState().GetShaderEntryPoint(m_stage);
  const ShaderReflection *reflection = m_Ctx.CurPipelineState().GetShaderReflection(m_stage);

  updateLabels();

  if(reflection == NULL || reflection->ConstantBlocks.count <= (int)m_slot)
  {
    setVariables({});
    return;
  }

  if(!m_formatOverride.empty())
  {
    m_Ctx.Replay().AsyncInvoke([this, offs, size](IReplayController *r) {
      rdctype::array<byte> data = r->GetBufferData(m_cbuffer, offs, size);
      rdctype::array<ShaderVariable> vars = applyFormatOverride(data);
      GUIInvoke::call([this, vars] { setVariables(vars); });
    });
  }
  else
  {
    m_Ctx.Replay().AsyncInvoke([this, entryPoint, offs](IReplayController *r) {
      rdctype::array<ShaderVariable> vars = r->GetCBufferVariableContents(
          m_shader, entryPoint.toUtf8().data(), m_slot, m_cbuffer, offs);
      GUIInvoke::call([this, vars] { setVariables(vars); });
    });
  }
}

void ConstantBufferPreviewer::on_setFormat_toggled(bool checked)
{
  if(!checked)
  {
    ui->splitter->setCollapsible(1, true);
    ui->splitter->setSizes({1, 0});
    ui->splitter->handle(1)->setEnabled(false);

    processFormat(QString());
    return;
  }

  ui->splitter->setCollapsible(1, false);
  ui->splitter->setSizes({1, 1});
  ui->splitter->handle(1)->setEnabled(true);
}

void ConstantBufferPreviewer::on_saveCSV_clicked()
{
  QString filename = RDDialog::getSaveFileName(this, tr("Export buffer data as CSV"), QString(),
                                               tr("CSV Files (*.csv)"));

  if(!filename.isEmpty())
  {
    QDir dirinfo = QFileInfo(filename).dir();
    if(dirinfo.exists())
    {
      QFile f(filename, this);
      if(f.open(QIODevice::WriteOnly | QIODevice::Truncate))
      {
        QTextStream ts(&f);

        ts << tr("Name,Value,Type\n");

        for(int i = 0; i < ui->variables->topLevelItemCount(); i++)
          exportCSV(ts, QString(), ui->variables->topLevelItem(i));

        return;
      }

      RDDialog::critical(
          this, tr("Error exporting buffer data"),
          tr("Couldn't open path %1 for write.\n%2").arg(filename).arg(f.errorString()));
    }
    else
    {
      RDDialog::critical(this, tr("Invalid directory"),
                         tr("Cannot find target directory to save to"));
    }
  }
}

void ConstantBufferPreviewer::exportCSV(QTextStream &ts, const QString &prefix, RDTreeWidgetItem *item)
{
  if(item->childCount() == 0)
  {
    ts << QFormatStr("%1,\"%2\",%3\n").arg(item->text(0)).arg(item->text(1)).arg(item->text(2));
  }
  else
  {
    ts << QFormatStr("%1,,%2\n").arg(item->text(0)).arg(item->text(2));
    for(int i = 0; i < item->childCount(); i++)
      exportCSV(ts, item->text(0) + lit("."), item->child(i));
  }
}

void ConstantBufferPreviewer::processFormat(const QString &format)
{
  if(format.isEmpty())
  {
    m_formatOverride.clear();
    ui->formatSpecifier->setErrors(QString());
  }
  else
  {
    QString errors;

    m_formatOverride = FormatElement::ParseFormatString(format, 0, false, errors);
    ui->formatSpecifier->setErrors(errors);
  }

  OnEventChanged(m_Ctx.CurEvent());
}

void ConstantBufferPreviewer::addVariables(RDTreeWidgetItem *root,
                                           const rdctype::array<ShaderVariable> &vars)
{
  for(const ShaderVariable &v : vars)
  {
    RDTreeWidgetItem *n = new RDTreeWidgetItem({ToQStr(v.name), VarString(v), TypeString(v)});

    root->addChild(n);

    if(v.rows > 1)
    {
      for(uint32_t i = 0; i < v.rows; i++)
        n->addChild(new RDTreeWidgetItem({QFormatStr("%1.row%2").arg(ToQStr(v.name)).arg(i),
                                          RowString(v, i), RowTypeString(v)}));
    }

    if(v.members.count > 0)
      addVariables(n, v.members);
  }
}

bool ConstantBufferPreviewer::updateVariables(RDTreeWidgetItem *root,
                                              const rdctype::array<ShaderVariable> &prevVars,
                                              const rdctype::array<ShaderVariable> &newVars)
{
  // mismatched child count? can't update
  if(prevVars.count != newVars.count)
    return false;

  for(int i = 0; i < prevVars.count; i++)
  {
    const ShaderVariable &a = prevVars[i];
    const ShaderVariable &b = newVars[i];

    // different names? can't update
    if(strcmp(a.name.c_str(), b.name.c_str()))
      return false;

    // different size or type? can't update
    if(a.rows != b.rows || a.columns != b.columns || a.displayAsHex != b.displayAsHex ||
       a.isStruct != b.isStruct || a.type != b.type)
      return false;

    // update this node's value column
    RDTreeWidgetItem *node = root->child(i);

    node->setText(1, VarString(b));

    if(a.rows > 1)
    {
      for(uint32_t r = 0; r < a.rows; r++)
        node->child(r)->setText(1, RowString(b, r));
    }

    if(a.members.count > 0)
    {
      // recurse to update child members. This handles a and b having different number of variables
      bool updated = updateVariables(node, a.members, b.members);

      if(!updated)
        return false;
    }
  }

  // got this far without bailing? we updated!
  return true;
}

void ConstantBufferPreviewer::setVariables(const rdctype::array<ShaderVariable> &vars)
{
  ui->variables->setUpdatesEnabled(false);

  // try to update the variables in-place by only changing their values, if the set of variables
  // matches *exactly* to what we had before.
  //
  // This keeps things like expanded structs and matrices when moving between drawcalls
  bool updated = updateVariables(ui->variables->invisibleRootItem(), m_Vars, vars);

  // update the variables either way
  m_Vars = vars;

  if(updated)
  {
    ui->variables->setUpdatesEnabled(true);
    return;
  }

  ui->variables->clear();

  ui->saveCSV->setEnabled(false);

  if(vars.count > 0)
  {
    addVariables(ui->variables->invisibleRootItem(), vars);
    ui->saveCSV->setEnabled(true);
  }

  ui->variables->setUpdatesEnabled(true);
}

void ConstantBufferPreviewer::updateLabels()
{
  QString bufName;

  bool needName = true;

  BufferDescription *buf = m_Ctx.GetBuffer(m_cbuffer);
  if(buf)
  {
    bufName = ToQStr(buf->name);
    if(buf->customName)
      needName = false;
  }

  const ShaderReflection *reflection = m_Ctx.CurPipelineState().GetShaderReflection(m_stage);

  if(reflection != NULL)
  {
    if(needName && (int)m_slot < reflection->ConstantBlocks.count &&
       reflection->ConstantBlocks[m_slot].name.count > 0)
      bufName = QFormatStr("<%1>").arg(ToQStr(reflection->ConstantBlocks[m_slot].name));
  }

  ui->nameLabel->setText(bufName);

  GraphicsAPI pipeType = m_Ctx.APIProps().pipelineType;

  QString title = QFormatStr("%1 %2 %3")
                      .arg(ToQStr(m_stage, pipeType))
                      .arg(IsD3D(pipeType) ? lit("CB") : lit("UBO"))
                      .arg(m_slot);

  if(m_Ctx.CurPipelineState().SupportsResourceArrays())
    title += QFormatStr(" [%1]").arg(m_arrayIdx);

  ui->slotLabel->setText(title);
  setWindowTitle(title);
}

rdctype::array<ShaderVariable> ConstantBufferPreviewer::applyFormatOverride(
    const rdctype::array<byte> &bytes)
{
  QVector<ShaderVariable> variables;

  variables.resize(m_formatOverride.length());

  for(int i = 0; i < m_formatOverride.length(); i++)
  {
    const byte *b = bytes.begin() + m_formatOverride[i].offset;
    variables[i] = m_formatOverride[i].GetShaderVar(b, bytes.end());
  }

  rdctype::array<ShaderVariable> ret;
  ret = variables.toStdVector();
  return ret;
}
