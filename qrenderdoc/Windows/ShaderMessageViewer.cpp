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

#include "ShaderMessageViewer.h"
#include <QAction>
#include <QMenu>
#include <QMouseEvent>
#include <QScrollBar>
#include "Code/QRDUtils.h"
#include "Code/Resources.h"
#include "Widgets/Extended/RDHeaderView.h"
#include "Widgets/Extended/RDLineEdit.h"
#include "toolwindowmanager/ToolWindowManager.h"
#include "ui_ShaderMessageViewer.h"

static const int debuggableRole = Qt::UserRole + 1;
static const int gotoableRole = Qt::UserRole + 2;

ButtonDelegate::ButtonDelegate(const QIcon &icon, int enableRole, QWidget *parent)
    : m_Icon(icon), m_EnableRole(enableRole), QStyledItemDelegate(parent)
{
}

void ButtonDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                           const QModelIndex &index) const
{
  // draw the background to get selection etc
  QApplication::style()->drawControl(QStyle::CE_ItemViewItem, &option, painter);

  QStyleOptionButton button;

  QSize sz = sizeHint(option, index);
  button.rect = option.rect;
  button.rect.setLeft(button.rect.center().x() - sz.width() / 2);
  button.rect.setTop(button.rect.center().y() - sz.height() / 2);
  button.rect.setSize(sz);
  button.icon = m_Icon;
  button.iconSize = sz;

  if(m_EnableRole == 0 || index.data(m_EnableRole).toBool())
    button.state = QStyle::State_Enabled;

  if(m_ClickedIndex == index)
    button.state |= QStyle::State_Sunken;

  QApplication::style()->drawControl(QStyle::CE_PushButton, &button, painter);
}

QSize ButtonDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
  QStyleOptionButton button;
  button.icon = m_Icon;
  button.state = QStyle::State_Enabled;

  return QApplication::style()->sizeFromContents(QStyle::CT_PushButton, &button,
                                                 option.decorationSize);
}

bool ButtonDelegate::editorEvent(QEvent *event, QAbstractItemModel *model,
                                 const QStyleOptionViewItem &option, const QModelIndex &index)
{
  if(event->type() == QEvent::MouseButtonPress)
  {
    if(m_EnableRole == 0 || index.data(m_EnableRole).toBool())
      m_ClickedIndex = index;
  }
  else if(event->type() == QEvent::MouseMove)
  {
    QMouseEvent *e = (QMouseEvent *)event;

    if(m_ClickedIndex != index || (e->buttons() & Qt::LeftButton) == 0)
    {
      m_ClickedIndex = QModelIndex();
    }
    else
    {
      QPoint p = e->pos();

      QSize sz = option.decorationSize;
      QRect rect = option.rect;
      rect.setLeft(rect.center().x() - sz.width() / 2);
      rect.setTop(rect.center().y() - sz.height() / 2);
      rect.setSize(sz);

      if(!rect.contains(p))
      {
        m_ClickedIndex = QModelIndex();
      }
    }
  }
  else if(event->type() == QEvent::MouseButtonRelease)
  {
    if(m_ClickedIndex == index && index != QModelIndex())
    {
      m_ClickedIndex = QModelIndex();

      QMouseEvent *e = (QMouseEvent *)event;

      QPoint p = e->pos();

      QSize sz = option.decorationSize;
      QRect rect = option.rect;
      rect.setLeft(rect.center().x() - sz.width() / 2);
      rect.setTop(rect.center().y() - sz.height() / 2);
      rect.setSize(sz);

      if(rect.contains(p))
        emit messageClicked(index);
    }
  }

  return true;
}

ShaderMessageViewer::ShaderMessageViewer(ICaptureContext &ctx, ShaderStageMask stages, QWidget *parent)
    : QFrame(parent), ui(new Ui::ShaderMessageViewer), m_Ctx(ctx)
{
  ui->setupUi(this);

  ui->messages->setFont(Formatter::PreferredFont());

  ui->messages->setMouseTracking(true);

  m_API = m_Ctx.APIProps().pipelineType;

  QObject::connect(ui->task, &QToolButton::toggled, [this](bool) { refreshMessages(); });
  QObject::connect(ui->mesh, &QToolButton::toggled, [this](bool) { refreshMessages(); });
  QObject::connect(ui->vertex, &QToolButton::toggled, [this](bool) { refreshMessages(); });
  QObject::connect(ui->hull, &QToolButton::toggled, [this](bool) { refreshMessages(); });
  QObject::connect(ui->domain, &QToolButton::toggled, [this](bool) { refreshMessages(); });
  QObject::connect(ui->geometry, &QToolButton::toggled, [this](bool) { refreshMessages(); });
  QObject::connect(ui->pixel, &QToolButton::toggled, [this](bool) { refreshMessages(); });
  QObject::connect(ui->filterButton, &QToolButton::clicked, [this]() { refreshMessages(); });
  QObject::connect(ui->filter, &RDLineEdit::returnPressed, [this]() { refreshMessages(); });

  QMenu *menu = new QMenu(this);

  QAction *action = new QAction(tr("Export to &Text"));
  action->setIcon(Icons::save());
  QObject::connect(action, &QAction::triggered, this, &ShaderMessageViewer::exportText);
  menu->addAction(action);

  action = new QAction(tr("Export to &CSV"));
  action->setIcon(Icons::save());
  QObject::connect(action, &QAction::triggered, this, &ShaderMessageViewer::exportCSV);
  menu->addAction(action);

  ui->exportButton->setMenu(menu);
  QObject::connect(ui->exportButton, &QToolButton::clicked, this, &ShaderMessageViewer::exportText);

  ui->task->setText(ToQStr(ShaderStage::Task, m_API));
  ui->mesh->setText(ToQStr(ShaderStage::Mesh, m_API));
  ui->vertex->setText(ToQStr(ShaderStage::Vertex, m_API));
  ui->hull->setText(ToQStr(ShaderStage::Hull, m_API));
  ui->domain->setText(ToQStr(ShaderStage::Domain, m_API));
  ui->geometry->setText(ToQStr(ShaderStage::Geometry, m_API));
  ui->pixel->setText(ToQStr(ShaderStage::Pixel, m_API));

  m_EID = m_Ctx.CurEvent();
  m_Action = m_Ctx.GetAction(m_EID);

  const PipeState &pipe = m_Ctx.CurPipelineState();

  // check if we have multiview enabled
  m_Multiview = pipe.MultiviewBroadcastCount() > 1;

  // only display sample information if one of the targets is multisampled
  m_Multisampled = false;
  rdcarray<BoundResource> outs = pipe.GetOutputTargets();
  outs.push_back(pipe.GetDepthTarget());
  outs.push_back(pipe.GetDepthResolveTarget());
  for(const BoundResource &o : outs)
  {
    if(o.resourceId == ResourceId())
      continue;

    const TextureDescription *tex = m_Ctx.GetTexture(o.resourceId);

    if(tex->msSamp > 1)
    {
      m_Multisampled = true;
      break;
    }
  }

  RDHeaderView *header = new RDHeaderView(Qt::Horizontal, this);
  ui->messages->setHeader(header);
  header->setStretchLastSection(true);
  header->setMinimumSectionSize(40);

  int sortColumn = 0;

  m_debugDelegate = new ButtonDelegate(Icons::wrench(), debuggableRole, this);

  if(m_Action && (m_Action->flags & ActionFlags::Dispatch))
  {
    ui->stageFilters->hide();

    ui->messages->setColumns({lit("Debug"), tr("Workgroup"), lit("Thread"), lit("Message")});
    sortColumn = 1;

    ui->messages->setItemDelegateForColumn(0, m_debugDelegate);

    m_OrigShaders[5] = pipe.GetShader(ShaderStage::Compute);
    m_LayoutStage = ShaderStage::Compute;
  }
  else
  {
    if(pipe.GetShader(ShaderStage::Task) != ResourceId())
    {
      ui->messages->setColumns({lit("Debug"), lit("Go to"), tr("Task group"), tr("Mesh group"),
                                lit("Thread"), lit("Message")});
      sortColumn = 4;
      m_LayoutStage = ShaderStage::Task;
    }
    else if(pipe.GetShader(ShaderStage::Mesh) != ResourceId())
    {
      ui->messages->setColumns(
          {lit("Debug"), lit("Go to"), tr("Workgroup"), lit("Thread/Location"), lit("Message")});
      sortColumn = 3;
      m_LayoutStage = ShaderStage::Mesh;
    }
    else
    {
      ui->messages->setColumns({lit("Debug"), lit("Go to"), tr("Location"), lit("Message")});
      sortColumn = 2;
      m_LayoutStage = ShaderStage::Vertex;
    }

    m_gotoDelegate = new ButtonDelegate(Icons::find(), gotoableRole, this);

    ui->messages->setItemDelegateForColumn(0, m_debugDelegate);
    ui->messages->setItemDelegateForColumn(1, m_gotoDelegate);

    QCheckBox *boxes[] = {
        ui->vertex,
        ui->hull,
        ui->domain,
        ui->geometry,
        ui->pixel,
        // compute
        NULL,
        ui->task,
        ui->mesh,
    };

    for(ShaderStage s : values<ShaderStage>())
    {
      if(s == ShaderStage::Compute)
        continue;

      uint32_t idx = (uint32_t)s;

      m_OrigShaders[idx] = pipe.GetShader(s);

      boxes[idx]->setChecked(bool(stages & MaskForStage(s)));

      // if there's no shader bound, we currently don't support adding stages at runtime so just
      // hide this box as no messages can come from the unbound stage
      if(m_OrigShaders[idx] == ResourceId())
        boxes[idx]->hide();
    }
  }

  ui->messages->setContextMenuPolicy(Qt::CustomContextMenu);
  QObject::connect(ui->messages, &RDTreeWidget::customContextMenuRequested, [this](const QPoint &pos) {
    QModelIndex idx = ui->messages->indexAt(pos);
    RDTreeWidgetItem *item = ui->messages->itemForIndex(idx);

    QMenu contextMenu(this);

    QAction copy(tr("&Copy"), this);

    contextMenu.addAction(&copy);

    copy.setIcon(Icons::copy());

    QObject::connect(&copy, &QAction::triggered,
                     [this, pos, item]() { ui->messages->copyItem(pos, item); });

    QAction debugAction(tr("&Debug"), this);
    debugAction.setIcon(Icons::wrench());
    QAction gotoAction(tr("&Go to"), this);
    gotoAction.setIcon(Icons::find());

    QObject::connect(&debugAction, &QAction::triggered,
                     [this, idx]() { m_debugDelegate->messageClicked(idx); });

    QObject::connect(&gotoAction, &QAction::triggered,
                     [this, idx]() { m_gotoDelegate->messageClicked(idx); });

    contextMenu.addAction(&debugAction);
    if(m_gotoDelegate)
      contextMenu.addAction(&gotoAction);

    RDDialog::show(&contextMenu, ui->messages->viewport()->mapToGlobal(pos));
  });

  QObject::connect(m_debugDelegate, &ButtonDelegate::messageClicked, [this](const QModelIndex &idx) {
    RDTreeWidgetItem *item = ui->messages->itemForIndex(idx);

    int msgIdx = 0;
    if(item)
      msgIdx = item->tag().toInt();
    else
      return;

    const ShaderMessage &msg = m_Messages[msgIdx];

    const ShaderReflection *refl = m_Ctx.CurPipelineState().GetShaderReflection(msg.stage);

    if(refl->debugInfo.debuggable)
    {
      bool done = false;
      ShaderDebugTrace *trace = NULL;

      m_Ctx.Replay().AsyncInvoke([&trace, &done, msg](IReplayController *r) {
        if(msg.stage == ShaderStage::Compute)
        {
          trace = r->DebugThread(msg.location.compute.workgroup, msg.location.compute.thread);
        }
        else if(msg.stage == ShaderStage::Vertex)
        {
          trace = r->DebugVertex(msg.location.vertex.vertexIndex, msg.location.vertex.instance,
                                 msg.location.vertex.vertexIndex, msg.location.vertex.view);
        }
        else if(msg.stage == ShaderStage::Pixel)
        {
          DebugPixelInputs inputs;
          inputs.sample = msg.location.pixel.sample;
          inputs.primitive = msg.location.pixel.primitive;
          inputs.view = msg.location.pixel.view;
          trace = r->DebugPixel(msg.location.pixel.x, msg.location.pixel.y, inputs);
        }

        if(trace && trace->debugger == NULL)
        {
          r->FreeTrace(trace);
          trace = NULL;
        }

        done = true;
      });

      QString debugContext;

      if(msg.stage == ShaderStage::Compute)
      {
        debugContext = lit("Group [%1,%2,%3] Thread [%4,%5,%6]")
                           .arg(msg.location.compute.workgroup[0])
                           .arg(msg.location.compute.workgroup[1])
                           .arg(msg.location.compute.workgroup[2])
                           .arg(msg.location.compute.thread[0])
                           .arg(msg.location.compute.thread[1])
                           .arg(msg.location.compute.thread[2]);
      }
      else if(msg.stage == ShaderStage::Vertex)
      {
        debugContext = tr("Vertex %1").arg(msg.location.vertex.vertexIndex);
      }
      else if(msg.stage == ShaderStage::Pixel)
      {
        debugContext = tr("Pixel %1,%2").arg(msg.location.pixel.x).arg(msg.location.pixel.y);
      }
      else if(msg.stage == ShaderStage::Task)
      {
        QString groupIdx = QFormatStr("%1").arg(msg.location.mesh.taskGroup[0]);
        if(msg.location.mesh.taskGroup[1] != ShaderMeshMessageLocation::NotUsed)
          groupIdx += QFormatStr(",%1").arg(msg.location.mesh.taskGroup[1]);
        if(msg.location.mesh.taskGroup[2] != ShaderMeshMessageLocation::NotUsed)
          groupIdx += QFormatStr(",%1").arg(msg.location.mesh.taskGroup[2]);

        QString threadIdx = QFormatStr("%1").arg(msg.location.mesh.thread[0]);
        if(msg.location.mesh.thread[1] != ShaderMeshMessageLocation::NotUsed)
          threadIdx += QFormatStr(",%1").arg(msg.location.mesh.thread[1]);
        if(msg.location.mesh.thread[2] != ShaderMeshMessageLocation::NotUsed)
          threadIdx += QFormatStr(",%1").arg(msg.location.mesh.thread[2]);

        debugContext = tr("Task Group [%1] Thread [%2]").arg(groupIdx).arg(threadIdx);
      }
      else if(msg.stage == ShaderStage::Mesh)
      {
        QString groupIdx = QFormatStr("%1").arg(msg.location.mesh.meshGroup[0]);
        if(msg.location.mesh.meshGroup[1] != ShaderMeshMessageLocation::NotUsed)
          groupIdx += QFormatStr(",%1").arg(msg.location.mesh.meshGroup[1]);
        if(msg.location.mesh.meshGroup[2] != ShaderMeshMessageLocation::NotUsed)
          groupIdx += QFormatStr(",%1").arg(msg.location.mesh.meshGroup[2]);

        QString threadIdx = QFormatStr("%1").arg(msg.location.mesh.thread[0]);
        if(msg.location.mesh.thread[1] != ShaderMeshMessageLocation::NotUsed)
          threadIdx += QFormatStr(",%1").arg(msg.location.mesh.thread[1]);
        if(msg.location.mesh.thread[2] != ShaderMeshMessageLocation::NotUsed)
          threadIdx += QFormatStr(",%1").arg(msg.location.mesh.thread[2]);

        debugContext = tr("Mesh Group [%1] Thread [%2]").arg(groupIdx).arg(threadIdx);

        if(msg.location.mesh.taskGroup[0] != ShaderMeshMessageLocation::NotUsed)
        {
          debugContext += tr(" from Task [%1").arg(msg.location.mesh.taskGroup[0]);
          if(msg.location.mesh.taskGroup[1] != ShaderMeshMessageLocation::NotUsed)
            debugContext += tr(",%1").arg(msg.location.mesh.taskGroup[1]);
          if(msg.location.mesh.taskGroup[2] != ShaderMeshMessageLocation::NotUsed)
            debugContext += tr(",%1").arg(msg.location.mesh.taskGroup[2]);
          debugContext += lit("]");
        }
      }

      // wait a short while before displaying the progress dialog (which won't show if we're already
      // done by the time we reach it)
      for(int i = 0; !done && i < 100; i++)
        QThread::msleep(5);

      ShowProgressDialog(this, tr("Debugging %1").arg(debugContext), [&done]() { return done; });

      if(!trace)
      {
        RDDialog::critical(this, tr("Debug Error"), tr("Error debugging pixel."));
        return;
      }

      const ShaderBindpointMapping &bindMapping =
          m_Ctx.CurPipelineState().GetBindpointMapping(msg.stage);
      ResourceId pipeline = msg.stage == ShaderStage::Compute
                                ? m_Ctx.CurPipelineState().GetComputePipelineObject()
                                : m_Ctx.CurPipelineState().GetGraphicsPipelineObject();

      // viewer takes ownership of the trace
      IShaderViewer *s = m_Ctx.DebugShader(&bindMapping, refl, pipeline, trace, debugContext);

      if(msg.disassemblyLine >= 0)
      {
        s->ToggleBreakpointOnDisassemblyLine(msg.disassemblyLine);
        s->RunForward();
      }

      m_Ctx.AddDockWindow(s->Widget(), DockReference::AddTo, this);
    }
    else
    {
      RDDialog::critical(
          this, tr("Shader can't be debugged"),
          tr("The shader does not support debugging: %1").arg(refl->debugInfo.debugStatus));
    }
  });

  if(m_gotoDelegate)
  {
    QObject::connect(m_gotoDelegate, &ButtonDelegate::messageClicked, [this](const QModelIndex &idx) {
      RDTreeWidgetItem *item = ui->messages->itemForIndex(idx);

      int msgIdx = 0;
      if(item)
        msgIdx = item->tag().toInt();
      else
        return;

      const ShaderMessage &msg = m_Messages[msgIdx];

      m_Ctx.SetEventID({}, m_EID, m_EID);

      if(msg.stage == ShaderStage::Vertex)
      {
        m_Ctx.ShowMeshPreview();
        m_Ctx.GetMeshPreview()->SetCurrentInstance(msg.location.vertex.instance);
        m_Ctx.GetMeshPreview()->SetCurrentView(msg.location.vertex.view);
        m_Ctx.GetMeshPreview()->ShowMeshData(MeshDataStage::VSOut);
        m_Ctx.GetMeshPreview()->ScrollToRow(msg.location.vertex.vertexIndex, MeshDataStage::VSOut);
        m_Ctx.GetMeshPreview()->ShowMeshData(MeshDataStage::VSIn);
        // TODO, not accurate for indices
        m_Ctx.GetMeshPreview()->ScrollToRow(msg.location.vertex.vertexIndex, MeshDataStage::VSIn);
      }
      else if(msg.stage == ShaderStage::Pixel)
      {
        m_Ctx.ShowTextureViewer();
        Subresource sub = m_Ctx.GetTextureViewer()->GetSelectedSubresource();
        sub.sample = msg.location.pixel.sample;
        sub.slice = msg.location.pixel.view;
        m_Ctx.GetTextureViewer()->SetSelectedSubresource(sub);

        // select an actual output. Prefer the first colour output, but if there's no colour output
        // pick depth.
        rdcarray<BoundResource> cols = m_Ctx.CurPipelineState().GetOutputTargets();
        bool hascol = false;
        for(size_t i = 0; i < cols.size(); i++)
          hascol |= cols[i].resourceId != ResourceId();

        if(hascol)
          m_Ctx.GetTextureViewer()->ViewFollowedResource(FollowType::OutputColor,
                                                         ShaderStage::Pixel, 0, 0);
        else
          m_Ctx.GetTextureViewer()->ViewFollowedResource(FollowType::OutputDepth,
                                                         ShaderStage::Pixel, 0, 0);
        m_Ctx.GetTextureViewer()->GotoLocation(msg.location.pixel.x, msg.location.pixel.y);
      }
      else if(msg.stage == ShaderStage::Geometry)
      {
        m_Ctx.ShowMeshPreview();
        m_Ctx.GetMeshPreview()->SetCurrentView(msg.location.geometry.view);
        m_Ctx.GetMeshPreview()->ShowMeshData(MeshDataStage::GSOut);
        // TODO, instances not supported
        m_Ctx.GetMeshPreview()->ScrollToRow(
            RENDERDOC_VertexOffset(m_Ctx.CurPipelineState().GetPrimitiveTopology(),
                                   msg.location.geometry.primitive),
            MeshDataStage::GSOut);
      }
      else if(msg.stage == ShaderStage::Task || msg.stage == ShaderStage::Mesh)
      {
        // TODO mesh shader jumping
      }
      else
      {
        qCritical() << "Can't go to a compute thread";
      }
    });
  }

  // deliberately copy m_OrigShaders to m_ReplacedShaders. This is impossible because we should
  // either see a ResourceId() for unedited, or a new resource for edited. This means when we first
  // get OnEventChanged() called we will definitely detect the situation as 'stale' and refresh the
  // messages.
  memcpy(m_ReplacedShaders, m_OrigShaders, sizeof(m_ReplacedShaders));

  header->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);

  ui->staleStatus->hide();

  ui->label->setText(tr("Shader messages from @%1 - %2")
                         .arg(m_EID)
                         .arg(m_Action ? m_Ctx.GetEventBrowser()->GetEventName(m_Action->eventId)
                                       : rdcstr("Unknown action")));

  setWindowTitle(tr("Shader messages at @%1").arg(m_EID));

  m_Ctx.AddCaptureViewer(this);

  OnEventChanged(m_Ctx.CurEvent());

  ui->messages->setSortComparison(
      [this](int col, Qt::SortOrder order, const RDTreeWidgetItem *a, const RDTreeWidgetItem *b) {
        if(order == Qt::DescendingOrder)
          std::swap(a, b);

        const ShaderMessage &am = m_Messages[a->tag().toInt()];
        const ShaderMessage &bm = m_Messages[b->tag().toInt()];

        if(col == 5)
        {
          // column 5 is the message when task shaders are used
          return am.message < bm.message;
        }
        else if(col == 4)
        {
          // column 4 is the message, except when task shaders are used - then it's
          // the thread index
          if((am.stage == ShaderStage::Task || am.stage == ShaderStage::Mesh) &&
             am.location.mesh.taskGroup[0] != ShaderMeshMessageLocation::NotUsed)
          {
            return am.location.mesh.thread < bm.location.mesh.thread;
          }
          else
          {
            return am.message < bm.message;
          }
        }
        else if(col == 3)
        {
          // column 3 is the mesh thread when only mesh shaders are used, or the mesh group when
          // task shaders are used. For non mesh/task it is the message
          if((am.stage == ShaderStage::Task || am.stage == ShaderStage::Mesh) &&
             am.location.mesh.taskGroup[0] != ShaderMeshMessageLocation::NotUsed)
          {
            return am.location.mesh.meshGroup < bm.location.mesh.meshGroup;
          }
          else if(am.stage == ShaderStage::Mesh)
          {
            return am.location.mesh.thread < bm.location.mesh.thread;
          }
          else
          {
            return am.message < bm.message;
          }
        }
        else if(col == 2 || m_OrigShaders[5] == ResourceId())
        {
          // sort by location either if it's selected, or if it's not dispatch in which case we
          // default to location sorting (don't try to sort by the button-only columns that have no
          // data)

          // sort by stage first
          if(am.stage != bm.stage)
            return am.stage < bm.stage;

          if(am.stage == ShaderStage::Vertex)
          {
            const ShaderVertexMessageLocation &aloc = am.location.vertex;
            const ShaderVertexMessageLocation &bloc = bm.location.vertex;

            if(aloc.view != bloc.view)
              return aloc.view < bloc.view;
            if(aloc.instance != bloc.instance)
              return aloc.instance < bloc.instance;
            return aloc.vertexIndex < bloc.vertexIndex;
          }
          else if(am.stage == ShaderStage::Pixel)
          {
            const ShaderPixelMessageLocation &aloc = am.location.pixel;
            const ShaderPixelMessageLocation &bloc = bm.location.pixel;

            if(aloc.x != bloc.x)
              return aloc.x < bloc.x;
            if(aloc.y != bloc.y)
              return aloc.y < bloc.y;
            if(aloc.primitive != bloc.primitive)
              return aloc.primitive < bloc.primitive;
            if(aloc.view != bloc.view)
              return aloc.view < bloc.view;
            return aloc.sample < bloc.sample;
          }
          else if(am.stage == ShaderStage::Compute)
          {
            // column 2 is the thread column for compute
            return am.location.compute.thread < bm.location.compute.thread;
          }
          else if(am.stage == ShaderStage::Task || am.stage == ShaderStage::Mesh)
          {
            // column 2 is the mesh group column, or the task group column, depending on if
            // task shaders were in use
            if(am.location.mesh.taskGroup[0] != ShaderMeshMessageLocation::NotUsed)
              return am.location.mesh.taskGroup < bm.location.mesh.taskGroup;
            else
              return am.location.mesh.meshGroup < bm.location.mesh.meshGroup;
          }
          else if(am.stage == ShaderStage::Geometry)
          {
            const ShaderGeometryMessageLocation &aloc = am.location.geometry;
            const ShaderGeometryMessageLocation &bloc = bm.location.geometry;

            if(aloc.view != bloc.view)
              return aloc.view < bloc.view;
            return am.location.geometry.primitive < bm.location.geometry.primitive;
          }
          else
          {
            // can't sort these, pretend they're all equal
            return false;
          }
        }
        else if(col == 1)
        {
          return am.location.compute.workgroup < bm.location.compute.workgroup;
        }

        return false;
      });

  ui->messages->sortByColumn(sortColumn, Qt::SortOrder::AscendingOrder);

  for(int i = 0; i < 4; i++)
  {
    header->setSectionResizeMode(i, QHeaderView::Interactive);
    ui->messages->resizeColumnToContents(i);
  }
}

ShaderMessageViewer::~ShaderMessageViewer()
{
  m_Ctx.RemoveCaptureViewer(this);
  delete ui;
}

bool ShaderMessageViewer::IsOutOfDate()
{
  return ui->staleStatus->isVisible();
}

void ShaderMessageViewer::OnCaptureLoaded()
{
}

void ShaderMessageViewer::OnCaptureClosed()
{
  ToolWindowManager::closeToolWindow(this);
}

void ShaderMessageViewer::OnEventChanged(uint32_t eventId)
{
  ResourceId shaders[NumShaderStages];
  bool editsChanged = false;
  QString staleReason;

  for(ShaderStage s : values<ShaderStage>())
  {
    uint32_t idx = (uint32_t)s;
    shaders[idx] = m_Ctx.GetResourceReplacement(m_OrigShaders[idx]);

    // either an edit has been applied, updated, or removed if these don't match
    if(shaders[idx] != m_ReplacedShaders[idx])
    {
      editsChanged = true;
      staleReason += QFormatStr(", %1").arg(ToQStr(s, m_API));
    }
  }

  // if the edits haven't changed, just skip
  if(!editsChanged)
    return;

  // if it's the current event we can update with the latest
  if(m_EID == eventId)
  {
    m_Messages = m_Ctx.CurPipelineState().GetShaderMessages();

    // not stale anymore
    ui->staleStatus->hide();

    // update current set of replaced shaders
    memcpy(m_ReplacedShaders, shaders, sizeof(m_ReplacedShaders));

    refreshMessages();
  }
  else
  {
    staleReason.remove(0, 2);

    // otherwise we can't - just update the stale status
    ui->staleStatus->show();
    ui->staleStatus->setText(
        tr("Messages are stale because edits to %1 shaders have changed since they were fetched.\n"
           "Select the event @%2 to refresh.")
            .arg(staleReason)
            .arg(m_EID));

    ui->messages->beginUpdate();

    for(int i = 0; i < ui->messages->topLevelItemCount(); i++)
      ui->messages->topLevelItem(i)->setItalic(true);

    ui->messages->endUpdate();
  }
}

void ShaderMessageViewer::exportText()
{
  exportData(false);
}

void ShaderMessageViewer::exportCSV()
{
  exportData(true);
}

void ShaderMessageViewer::exportData(bool csv)
{
  QString filter;
  QString title;
  if(csv)
  {
    filter = tr("CSV Files (*.csv)");
    title = tr("Export buffer to CSV");
  }
  else
  {
    filter = tr("Text Files (*.txt)");
    title = tr("Export buffer to text");
  }

  QString filename =
      RDDialog::getSaveFileName(this, title, QString(), tr("%1;;All files (*)").arg(filter));

  if(filename.isEmpty())
    return;

  QFile *f = new QFile(filename);

  QIODevice::OpenMode flags = QIODevice::WriteOnly | QFile::Truncate | QIODevice::Text;

  if(!f->open(flags))
  {
    delete f;
    RDDialog::critical(this, tr("Error exporting file"),
                       tr("Couldn't open file '%1' for writing").arg(filename));
    return;
  }

  LambdaThread *exportThread = new LambdaThread([this, csv, f]() {
    QTextStream s(f);

    bool compute = (m_OrigShaders[5] != ResourceId());

    if(csv)
    {
      if(compute)
        s << tr("Workgroup,Thread,Message\n");
      else
        s << tr("Location,Message\n");
    }

    const int start = compute ? 1 : 2;
    const int end = 3;

    int locationWidth = 0;
    for(int i = 0; i < ui->messages->topLevelItemCount(); i++)
    {
      RDTreeWidgetItem *node = ui->messages->topLevelItem(i);

      locationWidth = qMax(locationWidth, node->text(start).length());
      if(compute)
        locationWidth = qMax(locationWidth, node->text(start + 1).length());
    }

    for(int i = 0; i < ui->messages->topLevelItemCount(); i++)
    {
      RDTreeWidgetItem *node = ui->messages->topLevelItem(i);

      if(csv)
      {
        int col = start;
        for(; col <= end - 1; col++)
          s << "\"" << node->text(col) << "\",";
        s << "\"" << node->text(col).replace(QLatin1Char('"'), lit("\"\"")) << "\"\n";
      }
      else
      {
        int col = start;
        for(; col <= end - 1; col++)
          s << QFormatStr("%1").arg(node->text(col), -locationWidth) << "\t";
        s << node->text(col) << "\n";
      }
    }

    f->close();

    delete f;
  });
  exportThread->start();

  // wait a short while before displaying the progress dialog (which won't show if we're already
  // done by the time we reach it)
  for(int i = 0; exportThread->isRunning() && i < 100; i++)
    QThread::msleep(5);

  ShowProgressDialog(this, tr("Exporting messages"),
                     [exportThread]() { return !exportThread->isRunning(); });

  exportThread->deleteLater();
}

void ShaderMessageViewer::refreshMessages()
{
  ShaderStageMask mask = ShaderStageMask::Compute;

  if(!m_Action || !(m_Action->flags & ActionFlags::Dispatch))
  {
    mask = ShaderStageMask::Unknown;

    if(ui->task->isChecked())
      mask |= ShaderStageMask::Task;
    if(ui->mesh->isChecked())
      mask |= ShaderStageMask::Mesh;
    if(ui->vertex->isChecked())
      mask |= ShaderStageMask::Vertex;
    if(ui->hull->isChecked())
      mask |= ShaderStageMask::Hull;
    if(ui->domain->isChecked())
      mask |= ShaderStageMask::Domain;
    if(ui->geometry->isChecked())
      mask |= ShaderStageMask::Geometry;
    if(ui->pixel->isChecked())
      mask |= ShaderStageMask::Pixel;
  }

  int vs = ui->messages->verticalScrollBar()->value();
  int curMsg = -1;
  {
    RDTreeWidgetItem *item = ui->messages->currentItem();
    if(item)
      curMsg = item->tag().toInt();
  }
  RDTreeWidgetItem *newCurrentItem = NULL;
  ui->messages->beginUpdate();
  ui->messages->clear();

  QString filter = ui->filter->text().trimmed();

  const ShaderReflection *vsrefl = m_Ctx.CurPipelineState().GetShaderReflection(ShaderStage::Vertex);
  const ShaderReflection *psrefl = m_Ctx.CurPipelineState().GetShaderReflection(ShaderStage::Pixel);
  const ShaderReflection *csrefl = m_Ctx.CurPipelineState().GetShaderReflection(ShaderStage::Compute);
  const ShaderReflection *tsrefl = m_Ctx.CurPipelineState().GetShaderReflection(ShaderStage::Task);
  const ShaderReflection *msrefl = m_Ctx.CurPipelineState().GetShaderReflection(ShaderStage::Mesh);

  for(int i = 0; i < m_Messages.count(); i++)
  {
    const ShaderMessage &msg = m_Messages[i];

    // filter by stages
    if(!(MaskForStage(msg.stage) & mask))
      continue;

    QString text(msg.message);

    const ShaderReflection *refl = NULL;

    QString location;
    if(msg.stage == ShaderStage::Vertex)
    {
      refl = vsrefl;

      // only show the view if the draw has multiview enabled
      if(m_Multiview)
      {
        location += lit("View %1, ").arg(msg.location.vertex.view);
      }

      // only show the instance if the draw is actually instanced
      if(m_Action && (m_Action->flags & ActionFlags::Instanced) && m_Action->numInstances > 1)
      {
        location += lit("Inst %1, ").arg(msg.location.vertex.instance);
      }

      if(m_Action && (m_Action->flags & ActionFlags::Indexed))
      {
        location += lit("Idx %1").arg(msg.location.vertex.vertexIndex);
      }
      else
      {
        location += lit("Vert %1").arg(msg.location.vertex.vertexIndex);
      }
    }
    else if(msg.stage == ShaderStage::Pixel)
    {
      refl = psrefl;

      location = QFormatStr("%1 %2,%3")
                     .arg(IsD3D(m_API) ? lit("Pixel") : lit("Frag"))
                     .arg(msg.location.pixel.x)
                     .arg(msg.location.pixel.y);

      if(msg.location.pixel.primitive == ~0U)
        location += lit(", Prim ?");
      else
        location += lit(", Prim %1").arg(msg.location.pixel.primitive);

      // only show the view if the draw has multiview enabled
      if(m_Multiview)
      {
        location += lit(", View %1").arg(msg.location.pixel.view);
      }

      if(m_Multisampled && msg.location.pixel.sample != ~0U)
      {
        location += lit(", Samp %1").arg(msg.location.pixel.sample);
      }
    }
    else if(msg.stage == ShaderStage::Compute)
    {
      refl = csrefl;
    }
    else if(msg.stage == ShaderStage::Geometry)
    {
      location = lit("Geometry Prim %1").arg(msg.location.geometry.primitive);

      // only show the view if the draw has multiview enabled
      if(m_Multiview)
      {
        location += lit(", View %1").arg(msg.location.geometry.view);
      }
    }
    else if(msg.stage == ShaderStage::Task)
    {
      refl = tsrefl;
    }
    else if(msg.stage == ShaderStage::Mesh)
    {
      refl = msrefl;
    }
    else
    {
      // no location info for other stages
      location = tr("Unknown %1").arg(ToQStr(msg.stage, m_Ctx.APIProps().pipelineType));
    }

    // filter by text on location and messag
    if(!filter.isEmpty() && !text.contains(filter, Qt::CaseInsensitive) &&
       !location.contains(filter, Qt::CaseInsensitive))
      continue;

    RDTreeWidgetItem *node = NULL;

    if(msg.stage == ShaderStage::Compute)
    {
      node = new RDTreeWidgetItem({
          QString(),
          QFormatStr("%1, %2, %3")
              .arg(msg.location.compute.workgroup[0])
              .arg(msg.location.compute.workgroup[1])
              .arg(msg.location.compute.workgroup[2]),
          QFormatStr("%1, %2, %3")
              .arg(msg.location.compute.thread[0])
              .arg(msg.location.compute.thread[1])
              .arg(msg.location.compute.thread[2]),
          text,
      });

      node->setData(0, debuggableRole, refl && refl->debugInfo.debuggable);
    }
    else if(msg.stage == ShaderStage::Task)
    {
      QString groupIdx = QFormatStr("%1").arg(msg.location.mesh.taskGroup[0]);
      if(msg.location.mesh.taskGroup[1] != ShaderMeshMessageLocation::NotUsed)
        groupIdx += QFormatStr(",%1").arg(msg.location.mesh.taskGroup[1]);
      if(msg.location.mesh.taskGroup[2] != ShaderMeshMessageLocation::NotUsed)
        groupIdx += QFormatStr(",%1").arg(msg.location.mesh.taskGroup[2]);

      QString threadIdx = QFormatStr("%1").arg(msg.location.mesh.thread[0]);
      if(msg.location.mesh.thread[1] != ShaderMeshMessageLocation::NotUsed)
        threadIdx += QFormatStr(",%1").arg(msg.location.mesh.thread[1]);
      if(msg.location.mesh.thread[2] != ShaderMeshMessageLocation::NotUsed)
        threadIdx += QFormatStr(",%1").arg(msg.location.mesh.thread[2]);

      node = new RDTreeWidgetItem({
          QString(),
          QString(),
          groupIdx,
          lit("-"),
          threadIdx,
          text,
      });

      node->setData(0, debuggableRole, refl && refl->debugInfo.debuggable);
      node->setData(1, gotoableRole, true);
    }
    else if(msg.stage == ShaderStage::Mesh)
    {
      QString taskIdx;
      if(msg.location.mesh.taskGroup[0] != ShaderMeshMessageLocation::NotUsed)
        taskIdx = QFormatStr("%1").arg(msg.location.mesh.taskGroup[0]);
      if(msg.location.mesh.taskGroup[1] != ShaderMeshMessageLocation::NotUsed)
        taskIdx += QFormatStr(",%1").arg(msg.location.mesh.taskGroup[1]);
      if(msg.location.mesh.taskGroup[2] != ShaderMeshMessageLocation::NotUsed)
        taskIdx += QFormatStr(",%1").arg(msg.location.mesh.taskGroup[2]);

      QString groupIdx = QFormatStr("%1").arg(msg.location.mesh.meshGroup[0]);
      if(msg.location.mesh.meshGroup[1] != ShaderMeshMessageLocation::NotUsed)
        groupIdx += QFormatStr(",%1").arg(msg.location.mesh.meshGroup[1]);
      if(msg.location.mesh.meshGroup[2] != ShaderMeshMessageLocation::NotUsed)
        groupIdx += QFormatStr(",%1").arg(msg.location.mesh.meshGroup[2]);

      QString threadIdx = QFormatStr("%1").arg(msg.location.mesh.thread[0]);
      if(msg.location.mesh.thread[1] != ShaderMeshMessageLocation::NotUsed)
        threadIdx += QFormatStr(",%1").arg(msg.location.mesh.thread[1]);
      if(msg.location.mesh.thread[2] != ShaderMeshMessageLocation::NotUsed)
        threadIdx += QFormatStr(",%1").arg(msg.location.mesh.thread[2]);

      if(m_LayoutStage == ShaderStage::Task)
        node = new RDTreeWidgetItem({
            QString(),
            QString(),
            taskIdx,
            groupIdx,
            threadIdx,
            text,
        });
      else
        node = new RDTreeWidgetItem({
            QString(),
            QString(),
            groupIdx,
            threadIdx,
            text,
        });

      node->setData(0, debuggableRole, refl && refl->debugInfo.debuggable);
      node->setData(1, gotoableRole, true);
    }
    else
    {
      if(m_LayoutStage == ShaderStage::Task)
        node = new RDTreeWidgetItem({QString(), QString(), QString(), QString(), location, text});
      else if(m_LayoutStage == ShaderStage::Mesh)
        node = new RDTreeWidgetItem({QString(), QString(), QString(), location, text});
      else
        node = new RDTreeWidgetItem({QString(), QString(), location, text});

      node->setData(0, debuggableRole, refl && refl->debugInfo.debuggable);
      node->setData(1, gotoableRole,
                    msg.stage == ShaderStage::Vertex || msg.stage == ShaderStage::Pixel ||
                        msg.stage == ShaderStage::Geometry);
    }

    if(node)
    {
      if(i == curMsg)
        newCurrentItem = node;

      node->setItalic(ui->staleStatus->isVisible());
      node->setTag(i);
      ui->messages->addTopLevelItem(node);
    }
  }

  ui->messages->clearSelection();
  ui->messages->endUpdate();
  ui->messages->verticalScrollBar()->setValue(vs);

  if(newCurrentItem)
  {
    ui->messages->setCurrentItem(newCurrentItem);
    ui->messages->scrollToItem(newCurrentItem);
  }
}
