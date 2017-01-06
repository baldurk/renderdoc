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

Q_DECLARE_METATYPE(FetchAPIEvent);

APIInspector::APIInspector(CaptureContext *ctx, QWidget *parent)
    : QFrame(parent), ui(new Ui::APIInspector), m_Ctx(ctx)
{
  ui->setupUi(this);

  ui->apiEvents->headerItem()->setText(0, "EID");
  ui->apiEvents->headerItem()->setText(1, "Event");

  ui->splitter->setCollapsible(1, true);
  ui->splitter->setSizes({1, 0});

  m_Ctx->AddLogViewer(this);
}

APIInspector::~APIInspector()
{
  m_Ctx->windowClosed(this);
  m_Ctx->RemoveLogViewer(this);
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

void APIInspector::OnEventSelected(uint32_t eventID)
{
  ui->apiEvents->clearSelection();

  fillAPIView();
}

void APIInspector::addCallstack(rdctype::array<rdctype::str> calls)
{
  ui->callstack->setUpdatesEnabled(false);
  ui->callstack->clear();

  if(calls.count == 1 && calls[0].count == 0)
  {
    ui->callstack->addItem(tr("Symbols not loaded. Tools -> Resolve Symbols."));
  }
  else
  {
    for(rdctype::str &s : calls)
      ui->callstack->addItem(ToQStr(s));
  }
  ui->callstack->setUpdatesEnabled(true);
}

void APIInspector::on_apiEvents_itemSelectionChanged()
{
  if(ui->apiEvents->selectedItems().count() == 0)
    return;

  FetchAPIEvent ev = ui->apiEvents->selectedItems()[0]->data(0, Qt::UserRole).value<FetchAPIEvent>();

  if(ev.callstack.count > 0)
  {
    m_Ctx->Renderer()->AsyncInvoke([this, ev](IReplayRenderer *r) {
      rdctype::array<rdctype::str> trace;
      r->GetResolve(ev.callstack.elems, ev.callstack.count, &trace);

      GUIInvoke::call([this, trace]() { addCallstack(trace); });
    });
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

  QRegularExpression rgxopen("^\\s*{");
  QRegularExpression rgxclose("^\\s*}");

  const FetchDrawcall *draw = m_Ctx->CurDrawcall();

  if(draw != NULL && draw->events.count > 0)
  {
    int e = 0;
    for(const FetchAPIEvent &ev : draw->events)
    {
      QStringList lines = ToQStr(ev.eventDesc).split("\n", QString::SkipEmptyParts);

      QTreeWidgetItem *root =
          new QTreeWidgetItem(ui->apiEvents, QStringList{QString::number(ev.eventID), lines[0]});

      int i = 1;

      if(i < lines.count() && lines[i].trimmed() == "{")
        i++;

      QList<QTreeWidgetItem *> nodestack;
      nodestack.push_back(root);

      for(; i < lines.count(); i++)
      {
        if(rgxopen.match(lines[i]).hasMatch())
          nodestack.push_back(nodestack.back()->child(nodestack.back()->childCount() - 1));
        else if(rgxclose.match(lines[i]).hasMatch())
          nodestack.pop_back();
        else if(!nodestack.empty())
          new QTreeWidgetItem(nodestack.back(), QStringList{"", lines[i].trimmed()});
      }

      if(ev.eventID == draw->eventID)
      {
        QFont font = root->font(0);
        font.setBold(true);
        root->setFont(0, font);

        font = root->font(1);
        font.setBold(true);
        root->setFont(1, font);
      }

      root->setData(0, Qt::UserRole, QVariant::fromValue(ev));

      ui->apiEvents->insertTopLevelItem(e, root);
      e++;

      ui->apiEvents->clearSelection();
      ui->apiEvents->setItemSelected(root, true);
    }
  }
  ui->apiEvents->setUpdatesEnabled(true);
}