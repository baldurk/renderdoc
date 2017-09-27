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

#include "APIInspector.h"
#include <QRegularExpression>
#include "ui_APIInspector.h"

Q_DECLARE_METATYPE(APIEvent);

APIInspector::APIInspector(ICaptureContext &ctx, QWidget *parent)
    : QFrame(parent), ui(new Ui::APIInspector), m_Ctx(ctx)
{
  ui->setupUi(this);

  ui->apiEvents->setColumns({lit("EID"), tr("Event")});

  ui->splitter->setCollapsible(1, true);
  ui->splitter->setSizes({1, 0});

  ui->callstack->setFont(Formatter::PreferredFont());
  ui->apiEvents->setFont(Formatter::PreferredFont());

  RDSplitterHandle *handle = (RDSplitterHandle *)ui->splitter->handle(1);
  handle->setTitle(tr("Callstack"));
  handle->setIndex(1);
  handle->setCollapsed(true);

  m_Ctx.AddLogViewer(this);
}

APIInspector::~APIInspector()
{
  m_Ctx.BuiltinWindowClosed(this);
  m_Ctx.RemoveLogViewer(this);
  delete ui;
}

void APIInspector::OnLogfileLoaded()
{
}

void APIInspector::OnLogfileClosed()
{
  ui->apiEvents->clear();
  ui->callstack->clear();
}

void APIInspector::OnSelectedEventChanged(uint32_t eventID)
{
  ui->apiEvents->clearSelection();

  fillAPIView();
}

void APIInspector::addCallstack(rdcarray<rdcstr> calls)
{
  ui->callstack->setUpdatesEnabled(false);
  ui->callstack->clear();

  if(calls.size() == 1 && calls[0].isEmpty())
  {
    ui->callstack->addItem(tr("Symbols not loaded. Tools -> Resolve Symbols."));
  }
  else
  {
    for(rdcstr &s : calls)
      ui->callstack->addItem(s);
  }
  ui->callstack->setUpdatesEnabled(true);
}

void APIInspector::on_apiEvents_itemSelectionChanged()
{
  RDTreeWidgetItem *node = ui->apiEvents->selectedItem();

  if(!node)
    return;

  APIEvent ev = node->tag().value<APIEvent>();

  if(!ev.callstack.isEmpty())
  {
    if(m_Ctx.Replay().GetResolver())
    {
      m_Ctx.Replay().AsyncInvoke([this, ev](IReplayController *) {
        rdcarray<rdcstr> stack = m_Ctx.Replay().GetResolver()->GetResolve(ev.callstack);

        GUIInvoke::call([this, stack]() { addCallstack(stack); });
      });
    }
    else
    {
      ui->callstack->setUpdatesEnabled(false);
      ui->callstack->clear();
      ui->callstack->addItem(tr("Callstack resolution not available."));
      ui->callstack->setUpdatesEnabled(true);
    }
  }
  else
  {
    ui->callstack->setUpdatesEnabled(false);
    ui->callstack->clear();
    ui->callstack->addItem(tr("No Callstack available."));
    ui->callstack->setUpdatesEnabled(true);
  }
}

void APIInspector::fillAPIView()
{
  ui->apiEvents->setUpdatesEnabled(false);
  ui->apiEvents->clear();

  QRegularExpression rgxopen(lit("^\\s*{"));
  QRegularExpression rgxclose(lit("^\\s*}"));

  const DrawcallDescription *draw = m_Ctx.CurSelectedDrawcall();

  if(draw != NULL && !draw->events.isEmpty())
  {
    for(const APIEvent &ev : draw->events)
    {
      QStringList lines = QString(ev.eventDesc).split(lit("\n"), QString::SkipEmptyParts);

      RDTreeWidgetItem *root = new RDTreeWidgetItem({QString::number(ev.eventID), lines[0]});

      int i = 1;

      if(i < lines.count() && lines[i].trimmed() == lit("{"))
        i++;

      QList<RDTreeWidgetItem *> nodestack;
      nodestack.push_back(root);

      for(; i < lines.count(); i++)
      {
        if(rgxopen.match(lines[i]).hasMatch())
          nodestack.push_back(nodestack.back()->child(nodestack.back()->childCount() - 1));
        else if(rgxclose.match(lines[i]).hasMatch())
          nodestack.pop_back();
        else if(!nodestack.empty())
          nodestack.back()->addChild(new RDTreeWidgetItem({QString(), lines[i].trimmed()}));
      }

      if(ev.eventID == draw->eventID)
        root->setBold(true);

      root->setTag(QVariant::fromValue(ev));

      ui->apiEvents->addTopLevelItem(root);

      ui->apiEvents->setSelectedItem(root);
    }
  }
  ui->apiEvents->setUpdatesEnabled(true);
}
