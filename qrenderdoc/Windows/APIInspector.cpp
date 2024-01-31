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

#include "APIInspector.h"
#include "ui_APIInspector.h"

APIInspector::APIInspector(ICaptureContext &ctx, QWidget *parent)
    : QFrame(parent), ui(new Ui::APIInspector), m_Ctx(ctx)
{
  ui->setupUi(this);

  ui->apiEvents->setColumns({lit("EID"), tr("Event")});
  ui->apiEvents->header()->resizeSection(0, 150);

  ui->splitter->setCollapsible(1, true);
  ui->splitter->setSizes({1, 0});

  ui->callstack->setFont(Formatter::PreferredFont());
  ui->apiEvents->setFont(Formatter::PreferredFont());

  RDSplitterHandle *handle = (RDSplitterHandle *)ui->splitter->handle(1);
  handle->setTitle(tr("Callstack"));
  handle->setIndex(1);
  handle->setCollapsed(true);

  m_Ctx.AddCaptureViewer(this);
}

APIInspector::~APIInspector()
{
  m_Ctx.BuiltinWindowClosed(this);
  m_Ctx.RemoveCaptureViewer(this);
  delete ui;
}

void APIInspector::RevealParameter(SDObject *param)
{
  if(!param)
    return;

  rdcarray<SDObject *> hierarchy;
  while(param)
  {
    hierarchy.push_back(param);
    param = param->GetParent();
  }

  if(hierarchy.back()->type.basetype == SDBasic::Chunk)
  {
    SDChunk *chunk = (SDChunk *)hierarchy.back();
    hierarchy.pop_back();

    int rootIdx = m_Chunks.indexOf(chunk);

    SDObject *current = chunk;

    if(rootIdx >= 0)
    {
      RDTreeWidgetItem *parent = ui->apiEvents->topLevelItem(rootIdx);

      while(parent)
      {
        ui->apiEvents->expandItem(parent);

        SDObject *next = hierarchy.back();
        hierarchy.pop_back();

        RDTreeWidgetItem *item = NULL;

        for(size_t i = 0; i < current->NumChildren(); i++)
        {
          if(current->GetChild(i) == next)
          {
            current = next;
            item = parent->child((int)i);
            break;
          }
        }

        parent = item;

        if(hierarchy.empty())
          break;
      }

      ui->apiEvents->setSelectedItem(parent);
      ui->apiEvents->scrollToItem(parent);
    }
  }
}

void APIInspector::OnCaptureLoaded()
{
  OnSelectedEventChanged(m_Ctx.CurSelectedEvent());
}

void APIInspector::OnCaptureClosed()
{
  m_Chunks.clear();
  ui->apiEvents->clear();
  ui->callstack->clear();
  ui->apiEvents->clearInternalExpansions();
  m_EventID = 0;
}

void APIInspector::OnSelectedEventChanged(uint32_t eventId)
{
  ui->apiEvents->saveExpansion(ui->apiEvents->getInternalExpansion(m_EventID), 0);

  ui->apiEvents->clearSelection();

  fillAPIView();

  m_EventID = eventId;
  ui->apiEvents->applyExpansion(ui->apiEvents->getInternalExpansion(m_EventID), 0);
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

  SDChunk *chunk = NULL;
  // search up the tree to find the next parent with a chunk tag
  while(node)
  {
    chunk = (SDChunk *)node->tag().value<quintptr>();

    // if we found one, break
    if(chunk)
      break;

    // move to the parent
    node = node->parent();
  }

  if(chunk && !chunk->metadata.callstack.isEmpty())
  {
    if(m_Ctx.Replay().GetCaptureAccess())
    {
      m_Ctx.Replay().AsyncInvoke([this, chunk](IReplayController *) {
        rdcarray<rdcstr> stack =
            m_Ctx.Replay().GetCaptureAccess()->GetResolve(chunk->metadata.callstack);

        GUIInvoke::call(this, [this, stack]() { addCallstack(stack); });
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

  m_Chunks.clear();

  const ActionDescription *action = m_Ctx.CurSelectedAction();

  if(action != NULL && !action->events.isEmpty())
  {
    if(action->IsFakeMarker())
    {
      RDTreeWidgetItem *root = new RDTreeWidgetItem({lit("---"), QString(action->customName)});
      root->setBold(true);
      ui->apiEvents->addTopLevelItem(root);
      ui->apiEvents->setSelectedItem(root);
    }
    else
    {
      for(const APIEvent &ev : action->events)
      {
        addEvent(ev, ev.eventId == action->eventId);
      }
    }
  }
  else
  {
    APIEvent ev = m_Ctx.GetEventBrowser()->GetAPIEventForEID(m_Ctx.CurSelectedEvent());

    if(ev.eventId > 0)
      addEvent(ev, true);
  }

  ui->apiEvents->setUpdatesEnabled(true);
}

void APIInspector::addEvent(const APIEvent &ev, bool primary)
{
  if(ev.chunkIndex == APIEvent::NoChunk)
    return;

  const SDFile &file = m_Ctx.GetStructuredFile();

  RDTreeWidgetItem *root = new RDTreeWidgetItem({QString::number(ev.eventId), QString()});

  SDChunk *chunk = NULL;

  if(ev.chunkIndex < file.chunks.size())
  {
    chunk = file.chunks[ev.chunkIndex];

    m_Chunks.push_back(chunk);

    root->setText(1, SDObject2Variant(chunk, true));

    addStructuredChildren(root, *chunk);
  }
  else
  {
    root->setText(1, tr("Invalid chunk index %1").arg(ev.chunkIndex));
  }

  if(primary)
    root->setBold(true);

  root->setTag(QVariant::fromValue((quintptr)(void *)chunk));

  ui->apiEvents->addTopLevelItem(root);

  ui->apiEvents->setSelectedItem(root);
}
