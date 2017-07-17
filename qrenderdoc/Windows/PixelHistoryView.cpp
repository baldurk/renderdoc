/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2017 Baldur Karlsson
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

#include "PixelHistoryView.h"
#include <float.h>
#include <math.h>
#include <QAction>
#include <QMenu>
#include "3rdparty/toolwindowmanager/ToolWindowManager.h"
#include "ui_PixelHistoryView.h"

struct EventTag
{
  uint32_t eventID = 0;
  uint32_t primitive = ~0U;
};

Q_DECLARE_METATYPE(EventTag);

class PixelHistoryItemModel : public QAbstractItemModel
{
public:
  PixelHistoryItemModel(ICaptureContext &ctx, ResourceId tex, const TextureDisplay &display,
                        QObject *parent)
      : QAbstractItemModel(parent), m_Ctx(ctx)
  {
    m_Tex = m_Ctx.GetTexture(tex);
    m_Display = display;

    CompType compType = m_Tex->format.compType;

    if(compType == CompType::Typeless)
      compType = display.typeHint;

    m_IsUint = (compType == CompType::UInt);
    m_IsSint = (compType == CompType::SInt);
    m_IsFloat = (!m_IsUint && !m_IsSint);

    if(compType == CompType::Depth)
      m_IsDepth = true;

    if(m_Tex->format.special)
    {
      switch(m_Tex->format.specialFormat)
      {
        case SpecialFormat::D16S8:
        case SpecialFormat::D24S8:
        case SpecialFormat::D32S8:
        case SpecialFormat::S8: m_IsDepth = true; break;
        default: break;
      }
    }
  }

  void setHistory(const rdctype::array<PixelModification> &history)
  {
    m_ModList.reserve(history.count);
    for(const PixelModification &h : history)
      m_ModList.push_back(h);

    m_Loading = false;

    emit beginResetModel();

    setShowFailures(true);

    emit endResetModel();
  }

  void setShowFailures(bool show)
  {
    emit beginResetModel();

    m_History.clear();
    m_History.reserve(m_ModList.count());
    for(const PixelModification &h : m_ModList)
    {
      if(!show && !h.passed())
        continue;

      if(m_History.isEmpty() || m_History.back().back().eventID != h.eventID)
        m_History.push_back({h});
      else
        m_History.back().push_back(h);
    }

    emit endResetModel();
  }

  QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override
  {
    if(row < 0 || row >= rowCount(parent) || column < 0 || column >= columnCount())
      return QModelIndex();

    return createIndex(row, column, makeTag(row, parent));
  }

  QModelIndex parent(const QModelIndex &index) const override
  {
    if(m_Loading || isEvent(index))
      return QModelIndex();

    int eventRow = getEventRow(index);

    return createIndex(eventRow, 0, makeTag(eventRow, QModelIndex()));
  }
  int rowCount(const QModelIndex &parent = QModelIndex()) const override
  {
    if(m_Loading)
      return parent.isValid() ? 0 : 1;

    if(!parent.isValid())
      return m_History.count();

    if(isEvent(parent))
    {
      const QList<PixelModification> &mods = getMods(parent);
      const DrawcallDescription *draw = m_Ctx.GetDrawcall(mods.front().eventID);

      if(draw->flags & DrawFlags::Clear)
        return 0;

      return mods.count();
    }

    return 0;
  }
  int columnCount(const QModelIndex &parent = QModelIndex()) const override { return 5; }
  Qt::ItemFlags flags(const QModelIndex &index) const override
  {
    if(!index.isValid())
      return 0;

    return QAbstractItemModel::flags(index);
  }

  QVariant headerData(int section, Qt::Orientation orientation, int role) const override
  {
    if(orientation == Qt::Horizontal && role == Qt::DisplayRole && section == 0)
      return lit("Event");

    // sizes for the colour previews
    if(orientation == Qt::Horizontal && role == Qt::SizeHintRole && (section == 2 || section == 4))
      return QSize(18, 0);

    return QVariant();
  }

  QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override
  {
    if(index.isValid())
    {
      int col = index.column();

      // preview columns
      if(col == 2 || col == 4)
      {
        if(role == Qt::SizeHintRole)
          return QSize(16, 0);
      }

      if(m_Loading)
      {
        if(role == Qt::DisplayRole && col == 0)
          return tr("Loading...");

        return QVariant();
      }

      if(role == Qt::DisplayRole)
      {
        // main text
        if(col == 0)
        {
          if(isEvent(index))
          {
            const QList<PixelModification> &mods = getMods(index);
            const DrawcallDescription *drawcall = m_Ctx.GetDrawcall(mods.front().eventID);
            if(!drawcall)
              return QVariant();

            QString ret;
            QList<const DrawcallDescription *> drawstack;
            const DrawcallDescription *parent = m_Ctx.GetDrawcall(drawcall->parent);
            while(parent)
            {
              drawstack.push_back(parent);
              parent = m_Ctx.GetDrawcall(parent->parent);
            }

            if(!drawstack.isEmpty())
            {
              ret += lit("> ") + ToQStr(drawstack.back()->name);

              if(drawstack.count() > 3)
                ret += lit(" ...");

              ret += lit("\n");

              if(drawstack.count() > 2)
                ret += lit("> ") + ToQStr(drawstack[1]->name) + lit("\n");
              if(drawstack.count() > 1)
                ret += lit("> ") + ToQStr(drawstack[0]->name) + lit("\n");

              ret += lit("\n");
            }

            bool passed = true;
            bool uavnowrite = false;

            if(mods.front().directShaderWrite)
            {
              ret += tr("EID %1\n%2\nBound as UAV or copy - potential modification")
                         .arg(mods.front().eventID)
                         .arg(ToQStr(drawcall->name));

              if(memcmp(mods[0].preMod.col.value_u, mods[0].postMod.col.value_u,
                        sizeof(uint32_t) * 4) == 0)
              {
                ret += tr("\nNo change in tex value");
                uavnowrite = true;
              }
            }
            else
            {
              passed = false;
              for(const PixelModification &m : mods)
                passed |= m.passed();

              QString failure = passed ? QString() : failureString(mods[0]);

              ret += tr("EID %1\n%2%3\n%4 Fragments touching pixel\n")
                         .arg(mods.front().eventID)
                         .arg(ToQStr(drawcall->name))
                         .arg(failure)
                         .arg(mods.count());
            }

            return ret;
          }
          else
          {
            const PixelModification &mod = getMod(index);

            if(mod.directShaderWrite)
            {
              QString ret = tr("Potential UAV/Copy write");

              if(mod.preMod.col.value_u[0] == mod.postMod.col.value_u[0] &&
                 mod.preMod.col.value_u[1] == mod.postMod.col.value_u[1] &&
                 mod.preMod.col.value_u[2] == mod.postMod.col.value_u[2] &&
                 mod.preMod.col.value_u[3] == mod.postMod.col.value_u[3])
              {
                ret += tr("\nNo change in tex value");
              }

              return ret;
            }
            else
            {
              QString ret = tr("Primitive %1\n").arg(mod.primitiveID);

              if(mod.shaderDiscarded)
                ret += failureString(mod);

              return ret;
            }
          }
        }

        // pre mod/shader out text
        if(col == 1)
        {
          if(isEvent(index))
          {
            return tr("Tex Before\n\n") + modString(getMods(index).first().preMod);
          }
          else
          {
            const PixelModification &mod = getMod(index);
            if(mod.unboundPS)
              return tr("No Pixel\nShader\nBound");
            if(mod.directShaderWrite)
              return tr("Tex Before\n\n") + modString(mod.preMod);
            return tr("Shader Out\n\n") + modString(mod.shaderOut);
          }
        }

        // post mod text
        if(col == 3)
        {
          if(isEvent(index))
            return tr("Tex After\n\n") + modString(getMods(index).last().postMod);
          else
            return tr("Tex After\n\n") + modString(getMod(index).shaderOut);
        }
      }

      if(role == Qt::BackgroundRole && (m_IsDepth || m_IsFloat))
      {
        // pre mod color
        if(col == 2)
        {
          if(isEvent(index))
            return backgroundBrush(getMods(index).first().preMod);
          else
            return backgroundBrush(getMod(index).shaderOut);
        }
        else if(col == 4)
        {
          if(isEvent(index))
            return backgroundBrush(getMods(index).last().postMod);
          else
            return backgroundBrush(getMod(index).postMod);
        }
      }

      // text backgrounds marking pass/fail
      if(role == Qt::BackgroundRole && (col == 0 || col == 1 || col == 3))
      {
        // rest
        if(isEvent(index))
        {
          const QList<PixelModification> &mods = getMods(index);

          bool passed = false;
          for(const PixelModification &m : mods)
            passed |= m.passed();

          if(mods[0].directShaderWrite &&
             memcmp(mods[0].preMod.col.value_u, mods[0].postMod.col.value_u, sizeof(uint32_t) * 4) ==
                 0)
            return QBrush(QColor::fromRgb(235, 235, 235));

          return passed ? QBrush(QColor::fromRgb(235, 255, 235))
                        : QBrush(QColor::fromRgb(255, 235, 235));
        }
        else
        {
          if(getMod(index).shaderDiscarded)
            return QBrush(QColor::fromRgb(255, 235, 235));
        }
      }

      if(role == Qt::UserRole)
      {
        EventTag tag;

        if(isEvent(index))
        {
          tag.eventID = getMods(index).first().eventID;
        }
        else
        {
          const PixelModification &mod = getMod(index);

          tag.eventID = mod.eventID;
          if(!mod.directShaderWrite)
            tag.primitive = mod.primitiveID;
        }

        return QVariant::fromValue(tag);
      }
    }

    return QVariant();
  }

  const QVector<PixelModification> &modifications() { return m_ModList; }
  ResourceId texID() { return m_Tex->ID; }
private:
  ICaptureContext &m_Ctx;

  const TextureDescription *m_Tex;
  TextureDisplay m_Display;
  bool m_IsDepth = false, m_IsUint = false, m_IsSint = false, m_IsFloat = true;

  bool m_Loading = true;
  QVector<QList<PixelModification>> m_History;
  QVector<PixelModification> m_ModList;

  // mask for top bit of quintptr
  static const quintptr eventTagMask = 1ULL << (Q_PROCESSOR_WORDSIZE * 8 - 1);

  // 1 byte on 32-bit, 2 bytes on 64-bit
  static const quintptr modRowBits = Q_PROCESSOR_WORDSIZE * 2;

  // mask without top bit and however many bits we have for modification mask
  static const quintptr eventRowMask = UINTPTR_MAX >> (1 + modRowBits);

  static const quintptr modRowMask = (1 << modRowBits) - 1;

  inline bool isEvent(QModelIndex parent) const { return parent.internalId() & eventTagMask; }
  int getEventRow(QModelIndex index) const
  {
    if(isEvent(index))
      return index.row();
    else
      return (index.internalId() & ~eventTagMask) >> modRowBits;
  }

  int getModRow(QModelIndex index) const { return int(index.internalId() & modRowMask); }
  const QList<PixelModification> &getMods(QModelIndex index) const
  {
    return m_History[index.row()];
  }

  const PixelModification &getMod(QModelIndex index) const
  {
    return m_History[getEventRow(index)][getModRow(index)];
  }

  quintptr makeTag(int row, QModelIndex parent) const
  {
    if(!parent.isValid())
    {
      // event
      return eventTagMask | row;
    }
    else
    {
      // modification
      if(quintptr(row) > modRowMask)
        qCritical() << "Packing failure - more than 255 modifications in one event";

      return ((parent.internalId() & eventRowMask) << modRowBits) | (quintptr(row) & modRowMask);
    }
  }

  QBrush backgroundBrush(const ModificationValue &val) const
  {
    float rangesize = (m_Display.rangemax - m_Display.rangemin);

    float r = val.col.value_f[0];
    float g = val.col.value_f[1];
    float b = val.col.value_f[2];

    if(!m_Display.Red)
      r = 0.0f;
    if(!m_Display.Green)
      g = 0.0f;
    if(!m_Display.Blue)
      b = 0.0f;

    if(m_Display.Red && !m_Display.Green && !m_Display.Blue && !m_Display.Alpha)
      g = b = r;
    if(!m_Display.Red && m_Display.Green && !m_Display.Blue && !m_Display.Alpha)
      r = b = g;
    if(!m_Display.Red && !m_Display.Green && m_Display.Blue && !m_Display.Alpha)
      g = r = b;
    if(!m_Display.Red && !m_Display.Green && !m_Display.Blue && m_Display.Alpha)
      g = b = r = val.col.value_f[3];

    r = qBound(0.0f, (r - m_Display.rangemin) / rangesize, 1.0f);
    g = qBound(0.0f, (g - m_Display.rangemin) / rangesize, 1.0f);
    b = qBound(0.0f, (b - m_Display.rangemin) / rangesize, 1.0f);

    if(m_IsDepth)
      r = g = b = qBound(0.0f, (val.depth - m_Display.rangemin) / rangesize, 1.0f);

    {
      r = (float)powf(r, 1.0f / 2.2f);
      g = (float)powf(g, 1.0f / 2.2f);
      b = (float)powf(b, 1.0f / 2.2f);
    }

    return QBrush(QColor::fromRgb((int)(255.0f * r), (int)(255.0f * g), (int)(255.0f * b)));
  }

  QString modString(const ModificationValue &val) const
  {
    QString s;

    int numComps = (int)(m_Tex->format.compCount);

    static const QString colourLetterPrefix[] = {lit("R: "), lit("G: "), lit("B: "), lit("A: ")};

    if(!m_IsDepth)
    {
      if(m_IsUint)
      {
        for(int i = 0; i < numComps; i++)
          s += colourLetterPrefix[i] + Formatter::Format(val.col.value_u[i]) + lit("\n");
      }
      else if(m_IsSint)
      {
        for(int i = 0; i < numComps; i++)
          s += colourLetterPrefix[i] + Formatter::Format(val.col.value_i[i]) + lit("\n");
      }
      else
      {
        for(int i = 0; i < numComps; i++)
          s += colourLetterPrefix[i] + Formatter::Format(val.col.value_f[i]) + lit("\n");
      }
    }

    if(val.depth >= 0.0f)
      s += lit("\nD: ") + Formatter::Format(val.depth);
    else if(val.depth < -1.5f)
      s += lit("\nD: ?");
    else
      s += lit("\nD: -");

    if(val.stencil >= 0)
      s += lit("\nS: 0x") + Formatter::Format(uint8_t(val.stencil & 0xff), true);
    else if(val.stencil == -2)
      s += lit("\nS: ?");
    else
      s += lit("\nS: -");

    return s;
  }

  QString failureString(const PixelModification &mod) const
  {
    QString s;

    if(mod.sampleMasked)
      s += tr("\nMasked by SampleMask");
    if(mod.backfaceCulled)
      s += tr("\nBackface culled");
    if(mod.depthClipped)
      s += tr("\nDepth Clipped");
    if(mod.scissorClipped)
      s += tr("\nScissor Clipped");
    if(mod.shaderDiscarded)
      s += tr("\nShader executed a discard");
    if(mod.depthTestFailed)
      s += tr("\nDepth test failed");
    if(mod.stencilTestFailed)
      s += tr("\nStencil test failed");

    return s;
  }
};

PixelHistoryView::PixelHistoryView(ICaptureContext &ctx, ResourceId id, QPoint point,
                                   const TextureDisplay &display, QWidget *parent)
    : QFrame(parent), ui(new Ui::PixelHistoryView), m_Ctx(ctx)
{
  ui->setupUi(this);

  ui->events->setFont(Formatter::PreferredFont());

  m_Pixel = point;
  m_Display = display;

  TextureDescription *tex = m_Ctx.GetTexture(id);

  QString title =
      tr("Pixel History on %1 for (%2, %3)").arg(ToQStr(tex->name)).arg(point.x()).arg(point.y());
  if(tex->msSamp > 1)
    title += tr(" @ Sample %1").arg(display.sampleIdx);
  setWindowTitle(title);

  QString channelStr;
  if(display.Red)
    channelStr += lit("R");
  if(display.Green)
    channelStr += lit("G");
  if(display.Blue)
    channelStr += lit("B");

  if(channelStr.length() > 1)
    channelStr += tr(" channels");
  else
    channelStr += tr(" channel");

  if(!display.Red && !display.Green && !display.Blue && display.Alpha)
    channelStr = lit("Alpha");

  QString text;
  text = tr("Preview colours displayed in visible range %1 - %2 with %3 visible.\n\n")
             .arg(Formatter::Format(display.rangemin))
             .arg(Formatter::Format(display.rangemax))
             .arg(channelStr);
  text +=
      tr("Double click to jump to an event.\n"
         "Right click to debug an event, or hide failed events.");

  ui->label->setText(text);

  ui->eventsHidden->setVisible(false);

  m_Model = new PixelHistoryItemModel(ctx, id, display, this);
  ui->events->setModel(m_Model);

  ui->events->hideBranches();

  ui->events->header()->setSectionResizeMode(0, QHeaderView::Stretch);
  ui->events->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
  ui->events->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
  ui->events->header()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
  ui->events->header()->setSectionResizeMode(4, QHeaderView::ResizeToContents);

  m_Ctx.AddLogViewer(this);
}

PixelHistoryView::~PixelHistoryView()
{
  disableTimelineHighlight();

  ui->events->setModel(NULL);
  m_Ctx.RemoveLogViewer(this);
  delete ui;
}

void PixelHistoryView::enableTimelineHighlight()
{
  if(m_Ctx.HasTimelineBar())
    m_Ctx.GetTimelineBar()->HighlightHistory(m_Model->texID(), m_Model->modifications().toList());
}

void PixelHistoryView::disableTimelineHighlight()
{
  if(m_Ctx.HasTimelineBar())
    m_Ctx.GetTimelineBar()->HighlightHistory(ResourceId(), {});
}

void PixelHistoryView::enterEvent(QEvent *event)
{
  enableTimelineHighlight();
}

void PixelHistoryView::leaveEvent(QEvent *event)
{
  disableTimelineHighlight();
}

void PixelHistoryView::OnLogfileLoaded()
{
}

void PixelHistoryView::OnLogfileClosed()
{
  ToolWindowManager::closeToolWindow(this);
}

void PixelHistoryView::SetHistory(const rdctype::array<PixelModification> &history)
{
  m_Model->setHistory(history);

  enableTimelineHighlight();
}

void PixelHistoryView::startDebug(EventTag tag)
{
  m_Ctx.SetEventID({this}, tag.eventID, tag.eventID);

  ShaderDebugTrace *trace = NULL;

  m_Ctx.Replay().BlockInvoke([this, &trace](IReplayController *r) {
    trace = r->DebugPixel((uint32_t)m_Pixel.x(), (uint32_t)m_Pixel.y(), m_Display.sampleIdx, ~0U);
  });

  if(trace->states.count == 0)
  {
    RDDialog::critical(this, tr("Debug Error"), tr("Error debugging pixel."));
    m_Ctx.Replay().AsyncInvoke([trace](IReplayController *r) { r->FreeTrace(trace); });
    return;
  }

  GUIInvoke::call([this, trace]() {
    QString debugContext = QFormatStr("Pixel %1,%2").arg(m_Pixel.x()).arg(m_Pixel.y());

    const ShaderReflection *shaderDetails =
        m_Ctx.CurPipelineState().GetShaderReflection(ShaderStage::Pixel);
    const ShaderBindpointMapping &bindMapping =
        m_Ctx.CurPipelineState().GetBindpointMapping(ShaderStage::Pixel);

    // viewer takes ownership of the trace
    IShaderViewer *s =
        m_Ctx.DebugShader(&bindMapping, shaderDetails, ShaderStage::Pixel, trace, debugContext);

    m_Ctx.AddDockWindow(s->Widget(), DockReference::MainToolArea, NULL);
  });
}

void PixelHistoryView::jumpToPrimitive(EventTag tag)
{
  m_Ctx.SetEventID({this}, tag.eventID, tag.eventID);
  m_Ctx.ShowMeshPreview();

  IBufferViewer *viewer = m_Ctx.GetMeshPreview();

  const DrawcallDescription *draw = m_Ctx.CurDrawcall();

  if(draw)
  {
    uint32_t vertIdx = RENDERDOC_VertexOffset(draw->topology, tag.primitive);

    if(vertIdx != ~0U)
      viewer->ScrollToRow(vertIdx);
  }
}

void PixelHistoryView::on_events_customContextMenuRequested(const QPoint &pos)
{
  QModelIndex index = ui->events->indexAt(pos);

  QMenu contextMenu(this);

  QAction hideFailed(tr("&Show failed events"), this);
  hideFailed.setCheckable(true);
  hideFailed.setChecked(m_ShowFailures);

  contextMenu.addAction(&hideFailed);

  QObject::connect(&hideFailed, &QAction::toggled, [this](bool checked) {
    m_Model->setShowFailures(m_ShowFailures = checked);
    ui->eventsHidden->setVisible(!m_ShowFailures);
  });

  if(!index.isValid())
  {
    RDDialog::show(&contextMenu, ui->events->viewport()->mapToGlobal(pos));
    return;
  }

  EventTag tag = m_Model->data(index, Qt::UserRole).value<EventTag>();
  if(tag.eventID == 0)
  {
    RDDialog::show(&contextMenu, ui->events->viewport()->mapToGlobal(pos));
    return;
  }

  QAction jumpAction(tr("&Go to primitive %1 at Event %2").arg(tag.primitive).arg(tag.eventID), this);

  QString debugText;

  if(tag.primitive == ~0U)
  {
    debugText =
        tr("&Debug Pixel (%1, %2) at Event %3").arg(m_Pixel.x()).arg(m_Pixel.y()).arg(tag.eventID);
  }
  else
  {
    debugText = tr("&Debug Pixel (%1, %2) primitive %3 at Event %4")
                    .arg(m_Pixel.x())
                    .arg(m_Pixel.y())
                    .arg(tag.eventID)
                    .arg(tag.primitive);

    contextMenu.addAction(&jumpAction);
  }

  QAction debugAction(debugText, this);

  contextMenu.addAction(&debugAction);

  QObject::connect(&jumpAction, &QAction::triggered, [this, tag]() { jumpToPrimitive(tag); });
  QObject::connect(&debugAction, &QAction::triggered, [this, tag]() { startDebug(tag); });

  RDDialog::show(&contextMenu, ui->events->viewport()->mapToGlobal(pos));
}

void PixelHistoryView::on_events_doubleClicked(const QModelIndex &index)
{
  EventTag tag = m_Model->data(index, Qt::UserRole).value<EventTag>();
  if(tag.eventID > 0)
    m_Ctx.SetEventID({this}, tag.eventID, tag.eventID);
}

// TODO TimelineBar
