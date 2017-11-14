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
#include "ui_APIInspector.h"

Q_DECLARE_METATYPE(APIEvent);

APIInspector::APIInspector(ICaptureContext &ctx, QWidget *parent)
    : QFrame(parent), ui(new Ui::APIInspector), m_Ctx(ctx)
{
  ui->setupUi(this);

  ui->apiEvents->setColumns({lit("EID"), tr("Event")});
  ui->apiEvents->header()->resizeSection(0, 200);

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

  const SDFile &file = m_Ctx.GetStructuredFile();
  const DrawcallDescription *draw = m_Ctx.CurSelectedDrawcall();

  if(draw != NULL && !draw->events.isEmpty())
  {
    for(const APIEvent &ev : draw->events)
    {
      RDTreeWidgetItem *root = new RDTreeWidgetItem({QString::number(ev.eventID), QString()});

      if(ev.chunkIndex < file.chunks.size())
      {
        SDChunk *chunk = file.chunks[ev.chunkIndex];

        root->setText(1, chunk->name);

        addObjects(root, chunk->data.children, false);
      }
      else
      {
        root->setText(1, tr("Invalid chunk index %1").arg(ev.chunkIndex));
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

void APIInspector::addObjects(RDTreeWidgetItem *parent, const StructuredObjectList &objs,
                              bool parentIsArray)
{
  for(const SDObject *obj : objs)
  {
    if(obj->type.flags & SDTypeFlags::Hidden)
      continue;

    QString param;

    if(parentIsArray)
      param = QFormatStr("[%1]").arg(parent->childCount());
    else
      param = obj->name;

    RDTreeWidgetItem *item = new RDTreeWidgetItem({param, QString()});

    param = QString();

    // we don't identify via the type name as many types could be serialised as a ResourceId -
    // e.g. ID3D11Resource* or ID3D11Buffer* which would be the actual typename. We want to preserve
    // that for the best raw structured data representation instead of flattening those out to just
    // "ResourceId", and we also don't want to store two types ('fake' and 'real'), so instead we
    // check the custom string.
    if((obj->type.flags & SDTypeFlags::HasCustomString) &&
       !strncmp(obj->data.str.c_str(), "ResourceId", 10))
    {
      ResourceId id;
      static_assert(sizeof(id) == sizeof(obj->data.basic.u), "ResourceId is no longer uint64_t!");
      memcpy(&id, &obj->data.basic.u, sizeof(id));
      // for resource IDs, try to locate the resource.
      TextureDescription *tex = m_Ctx.GetTexture(id);
      BufferDescription *buf = m_Ctx.GetBuffer(id);

      if(tex)
      {
        param += tex->name;
      }
      else if(buf)
      {
        param += buf->name;
      }
      else
      {
        param += lit("%1 %2").arg(obj->type.name).arg(obj->data.basic.u);
      }
    }
    else if(obj->type.flags & SDTypeFlags::NullString)
    {
      param += lit("NULL");
    }
    else if(obj->type.flags & SDTypeFlags::HasCustomString)
    {
      param += obj->data.str;
    }
    else
    {
      switch(obj->type.basetype)
      {
        case SDBasic::Chunk:
        case SDBasic::Struct:
          param += QFormatStr("%1()").arg(obj->type.name);
          addObjects(item, obj->data.children, false);
          break;
        case SDBasic::Array:
          param += QFormatStr("%1[]").arg(obj->type.name);
          addObjects(item, obj->data.children, true);
          break;
        case SDBasic::Null: param += lit("NULL"); break;
        case SDBasic::Buffer: param += lit("(%1 byte buffer)").arg(obj->type.byteSize); break;
        case SDBasic::String: param += obj->data.str; break;
        case SDBasic::Enum:
        case SDBasic::UnsignedInteger: param += Formatter::Format(obj->data.basic.u); break;
        case SDBasic::SignedInteger: param += Formatter::Format(obj->data.basic.i); break;
        case SDBasic::Float: param += Formatter::Format(obj->data.basic.d); break;
        case SDBasic::Boolean: param += (obj->data.basic.b ? tr("True") : tr("False")); break;
        case SDBasic::Character: param += QLatin1Char(obj->data.basic.c); break;
      }
    }

    item->setText(1, param);

    parent->addChild(item);
  }
}
